#include "PatternState.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace opxy
{
namespace
{

std::string formatFloat (float value)
{
    // Compact but round-trip safe enough for musical values.
    char buffer[32];
    std::snprintf (buffer, sizeof (buffer), "%.5g", static_cast<double> (value));
    return std::string (buffer);
}

PlockParam plockParamFromId (const std::string& id, bool& found)
{
    found = true;
    for (int i = 0; i < kNumPlockParams; ++i)
    {
        const auto p = static_cast<PlockParam> (i);
        if (id == plockParamId (p))
            return p;
    }
    found = false;
    return PlockParam::Pitch;
}

std::vector<std::string> split (const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text)
    {
        if (c == separator)
        {
            parts.push_back (current);
            current.clear();
        }
        else
        {
            current.push_back (c);
        }
    }
    parts.push_back (current);
    return parts;
}

} // namespace

std::string serialiseSteps (const Pattern& pattern)
{
    std::ostringstream out;
    out << "opxy-steps v2\n";

    for (int t = 0; t < kMaxTracks; ++t)
    {
        const Track& track = pattern.tracks[t];

        for (int s = 0; s < kMaxSteps; ++s)
        {
            const Step& step = track.steps[s];

            const bool active = step.active.load (std::memory_order_relaxed);
            const int ratchet = step.ratchet.load (std::memory_order_relaxed);
            const float nudge = step.nudge.load (std::memory_order_relaxed);
            const bool hasPlocks = step.hasAnyPLock();

            if (! active && ! hasPlocks && ratchet <= 1 && nudge == 0.0f)
                continue;

            out << "t" << t << " s" << s
                << " a=" << (active ? 1 : 0)
                << " n=" << formatFloat (step.note.load (std::memory_order_relaxed))
                << " v=" << formatFloat (step.velocity.load (std::memory_order_relaxed))
                << " g=" << formatFloat (step.gate.load (std::memory_order_relaxed))
                << " r=" << ratchet
                << " d=" << formatFloat (nudge);

            if (hasPlocks)
            {
                out << " p=";
                bool first = true;
                for (int i = 0; i < kNumPlockParams; ++i)
                {
                    const auto param = static_cast<PlockParam> (i);
                    if (! step.hasPLock (param))
                        continue;
                    if (! first)
                        out << ",";
                    first = false;
                    out << plockParamId (param) << ":"
                        << formatFloat (step.getPLock (param, 0.0f));
                }
            }

            out << "\n";
        }
    }

    // --- sequenced punch-FX lanes -------------------------------------------
    for (int f = 0; f < kMaxFx; ++f)
    {
        const bool enabled = pattern.isFxLaneEnabled (f);
        bool hasSteps = false;
        for (int s = 0; s < kMaxSteps; ++s)
            if (pattern.getFxLaneStep (f, s) > 0.0f)
                hasSteps = true;

        if (! enabled && ! hasSteps)
            continue;

        out << "x" << f
            << " on=" << (enabled ? 1 : 0)
            << " len=" << pattern.getLaneLength() << "\n";

        for (int s = 0; s < kMaxSteps; ++s)
        {
            const float value = pattern.getFxLaneStep (f, s);
            if (value <= 0.0f)
                continue;
            out << "x" << f << " s" << s << " v=" << formatFloat (value) << "\n";
        }
    }

    return out.str();
}

bool deserialiseSteps (const std::string& text, Pattern& pattern)
{
    for (auto& track : pattern.tracks)
        for (auto& step : track.steps)
            step.reset();

    for (int f = 0; f < kMaxFx; ++f)
    {
        pattern.clearFxLane (f);
        pattern.setFxLaneEnabled (f, false);
    }
    pattern.fxLaneSteps.store (16);

    std::istringstream stream (text);
    std::string line;

    if (! std::getline (stream, line))
        return false;

    // Header is "opxy-steps v1"; accept a missing header for robustness but
    // reject anything that does not look like our format.
    if (line.rfind ("opxy-steps", 0) != 0)
        return false;

    // v1 = steps only, v2 adds the punch-FX lanes. Anything newer is still
    // accepted as long as the token format matches; unknown lines are ignored.
    const bool hasFxLanes = line.find ("v1") == std::string::npos;

    while (std::getline (stream, line))
    {
        if (line.empty())
            continue;

        int trackIndex = -1;
        int stepIndex = -1;
        bool active = false;
        float note = 60.0f, velocity = 0.8f, gate = 0.5f, nudge = 0.0f;
        int ratchet = 1;
        std::string plockBlob;

        const auto tokenList = split (line, ' ');

        // --- punch-FX lane lines ("x<fx> ...") ------------------------------
        if (! tokenList.empty() && tokenList[0].size() > 1 && tokenList[0][0] == 'x'
            && tokenList[0][1] != '=')
        {
            const int fxIndex = std::atoi (tokenList[0].c_str() + 1);
            if (! hasFxLanes || fxIndex < 0 || fxIndex >= kMaxFx)
                continue;

            int laneStep = -1;
            float laneValue = -1.0f;
            bool laneOn = false, hasOn = false;
            int laneLen = 0;

            for (std::size_t i = 1; i < tokenList.size(); ++i)
            {
                const auto& token = tokenList[i];
                if (token.size() < 2)
                    continue;

                if (token[0] == 's' && token[1] != '=')
                    laneStep = std::atoi (token.c_str() + 1);
                else if (token[0] == 'v' && token.size() > 2)
                    laneValue = static_cast<float> (std::atof (token.c_str() + 2));
                else if (token.rfind ("on=", 0) == 0)
                {
                    laneOn = std::atoi (token.c_str() + 3) != 0;
                    hasOn = true;
                }
                else if (token.rfind ("len=", 0) == 0)
                    laneLen = std::atoi (token.c_str() + 4);
            }

            if (hasOn)
                pattern.setFxLaneEnabled (fxIndex, laneOn);
            if (laneLen > 0)
                pattern.fxLaneSteps.store (clamp (laneLen, 1, kMaxSteps));
            if (laneStep >= 0 && laneValue >= 0.0f)
                pattern.setFxLaneStep (fxIndex, laneStep, laneValue);

            continue;
        }

        for (const auto& token : tokenList)
        {
            if (token.size() < 2)
                continue;

            const char key = token[0];
            const std::string value = token.substr (2); // skip "x="

            switch (key)
            {
                case 't':
                    if (token.size() > 1 && token[1] != '=')
                        trackIndex = std::atoi (token.c_str() + 1);
                    break;
                case 's':
                    if (token.size() > 1 && token[1] != '=')
                        stepIndex = std::atoi (token.c_str() + 1);
                    break;
                case 'a':
                    active = std::atoi (value.c_str()) != 0;
                    break;
                case 'n':
                    note = static_cast<float> (std::atof (value.c_str()));
                    break;
                case 'v':
                    velocity = static_cast<float> (std::atof (value.c_str()));
                    break;
                case 'g':
                    gate = static_cast<float> (std::atof (value.c_str()));
                    break;
                case 'r':
                    ratchet = std::atoi (value.c_str());
                    break;
                case 'd':
                    nudge = static_cast<float> (std::atof (value.c_str()));
                    break;
                case 'p':
                    plockBlob = value;
                    break;
                default:
                    break;
            }
        }

        if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kMaxSteps)
            continue;

        Step& step = pattern.tracks[trackIndex].steps[stepIndex];
        step.active.store (active);
        step.note.store (clamp (note, 0.0f, 127.0f));
        step.velocity.store (clamp (velocity, 0.0f, 1.0f));
        step.gate.store (clamp (gate, 0.01f, 8.0f));
        step.ratchet.store (clamp (ratchet, 1, kMaxRatchet));
        step.nudge.store (clamp (nudge, -0.5f, 0.5f));

        if (! plockBlob.empty())
        {
            for (const auto& entry : split (plockBlob, ','))
            {
                const auto colon = entry.find (':');
                if (colon == std::string::npos)
                    continue;

                bool found = false;
                const auto param = plockParamFromId (entry.substr (0, colon), found);
                if (! found)
                    continue;

                step.setPLock (param, static_cast<float> (std::atof (entry.substr (colon + 1).c_str())));
            }
        }
    }

    return true;
}

} // namespace opxy
