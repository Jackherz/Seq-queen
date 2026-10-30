#include "ReverbBurstFx.h"

#include <algorithm>
#include <cmath>

namespace opxy
{

float ReverbBurstFx::Comb::process (float input, int delaySamples, float feedback, float damping) noexcept
{
    const float delayed = line.read (0, static_cast<float> (delaySamples));

    // One-pole low-pass in the feedback path = darker tail as it decays.
    state += (delayed - state) * (1.0f - damping);

    line.write (0, input + state * feedback);
    line.advance();

    return delayed;
}

float ReverbBurstFx::Allpass::process (float input, int delaySamples, float gain) noexcept
{
    const float delayed = line.read (0, static_cast<float> (delaySamples));
    const float output  = delayed - input;
    line.write (0, input + delayed * gain);
    line.advance();
    return output;
}

void ReverbBurstFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);

    // Freeverb-ish tunings (ms), scaled to the sample rate.
    static constexpr float leftMs[kCombs]  = { 29.7f, 37.1f, 41.1f, 43.7f };
    static constexpr float rightMs[kCombs] = { 30.9f, 35.3f, 39.7f, 45.7f };
    static constexpr float apMs[2]         = { 5.0f, 1.7f };

    const auto toSamples = [this] (float ms)
    {
        return static_cast<int> (std::max (8.0f, static_cast<float> (sampleRate) * ms * 0.001f));
    };

    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < kCombs; ++i)
        {
            const float ms = (c == 0 ? leftMs[i] : rightMs[i]);
            combs[c][i].line.prepare (sampleRate, ms * 0.001f + 0.01f, 1);
            if (c == 0)
                combDelays[i] = toSamples (ms);
        }

        for (int i = 0; i < 2; ++i)
        {
            allpasses[c][i].line.prepare (sampleRate, apMs[i] * 0.001f + 0.01f, 1);
            if (c == 0)
                allpassDelays[i] = toSamples (apMs[i]);
        }
    }

    outGain.prepare (sampleRate);
    outGain.setTimeSeconds (0.02f);
    outGain.setTarget (0.0f);
    outGain.snapToTarget();

    reset();
}

void ReverbBurstFx::reset()
{
    PunchInFx::reset();
    for (int c = 0; c < 2; ++c)
    {
        for (auto& comb : combs[c])
        {
            comb.line.reset();
            comb.state = 0.0f;
        }
        for (auto& ap : allpasses[c])
            ap.line.reset();
    }
    outGain.setTarget (0.0f);
    outGain.snapToTarget();
}

void ReverbBurstFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        decay = value;
    else if (index == 1)
        damping = value;
}

float ReverbBurstFx::getPunchParam (int index) const
{
    if (index == 0)
        return decay;
    if (index == 1)
        return damping;
    return 0.0f;
}

bool ReverbBurstFx::hasTail() const
{
    return outGain.getCurrent() > 0.001f;
}

void ReverbBurstFx::processWet (float* left, float* right, int numSamples)
{
    // Longer decay needs a longer feedback; clamped well below 1 for stability.
    const float feedback = clamp (0.7f + decay * 0.28f, 0.0f, 0.98f);
    const float damp = clamp (damping, 0.0f, 0.95f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = (left[i] + right[i]) * 0.5f;

        for (int c = 0; c < 2; ++c)
        {
            float acc = 0.0f;
            for (int k = 0; k < kCombs; ++k)
            {
                // Slightly detuned right channel avoids a mono-ish tail.
                const int delay = combDelays[k] + (c == 1 ? static_cast<int> (combDelays[k] * 0.031f) : 0);
                acc += combs[c][k].process (in * 0.25f, delay, feedback, damp);
            }

            for (int a = 0; a < 2; ++a)
                acc = allpasses[c][a].process (acc, allpassDelays[a]);

            // Normalise the comb bank so a momentary burst can't slam the master.
            acc *= 0.28f;

            if (c == 0)
                left[i] = acc;
            else
                right[i] = acc;
        }
    }
}

void ReverbBurstFx::process (float* left, float* right, int numSamples)
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    numSamples = std::min (numSamples, maxBlockSize);

    captureDry (left, right, numSamples);
    processWet (left, right, numSamples);

    // Send style: dry stays intact, the wet wash rides on top and is allowed to
    // decay over ~1.2 s after release rather than being crossfaded out.
    outGain.setTimeSeconds (engaged ? 0.02f : 1.2f);
    outGain.setTarget (engaged ? 1.0f : 0.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGain.next();
        left[i]  = sanitise (dryLeft (i)  + left[i]  * g * 0.9f);
        right[i] = sanitise (dryRight (i) + right[i] * g * 0.9f);
    }
}

} // namespace opxy
