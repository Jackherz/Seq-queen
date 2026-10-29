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
    out << "opxy-steps v1\n";

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

    return out.str();
}

bool deserialiseSteps (const std::string& text, Pattern& pattern)
{
    for (auto& track : pattern.tracks)
        for (auto& step : track.steps)
            step.reset();

    std::istringstream stream (text);
    std::string line;

    if (! std::getline (stream, line))
        return false;

    // Header is "opxy-steps v1"; accept a missing header for robustness but
    // reject anything that does not look like our format.
    if (line.rfind ("opxy-steps", 0) != 0)
        return false;

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

        for (const auto& token : split (line, ' '))
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
