#include "DelayThrowFx.h"

#include <algorithm>
#include <cmath>

namespace opxy
{

void DelayThrowFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);

    line.prepare (sampleRate, 2.0f, 2);

    inGain.prepare (sampleRate);
    inGain.setTimeSeconds (0.008f);
    inGain.setTarget (0.0f);
    inGain.snapToTarget();

    outGain.prepare (sampleRate);
    outGain.setTimeSeconds (0.02f);
    outGain.setTarget (0.0f);
    outGain.snapToTarget();

    reset();
}

void DelayThrowFx::reset()
{
    PunchInFx::reset();
    line.reset();
    inGain.setTarget (0.0f);
    inGain.snapToTarget();
    outGain.setTarget (0.0f);
    outGain.snapToTarget();
    smoothedDelay = delaySeconds * static_cast<float> (sampleRate);
    dampStateL = dampStateR = 0.0f;
}

void DelayThrowFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        delaySeconds = 0.03f + value * 0.87f;   // 30 ms .. 900 ms
    else if (index == 1)
        feedback = 0.9f * value;                // 0 .. 0.9
}

float DelayThrowFx::getPunchParam (int index) const
{
    if (index == 0)
        return (delaySeconds - 0.03f) / 0.87f;
    if (index == 1)
        return feedback / 0.9f;
    return 0.0f;
}

bool DelayThrowFx::hasTail() const
{
    return outGain.getCurrent() > 0.001f || inGain.getCurrent() > 0.001f;
}

void DelayThrowFx::process (float* left, float* right, int numSamples)
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    numSamples = std::min (numSamples, maxBlockSize);

    captureDry (left, right, numSamples);

    inGain.setTarget (engaged ? 1.0f : 0.0f);

    // Fast fade in on press, slower "wash out" on release.
    outGain.setTimeSeconds (engaged ? 0.02f : std::max (0.35f, delaySeconds * 2.5f));
    outGain.setTarget (engaged ? 1.0f : 0.0f);

    const float targetDelay = delaySeconds * static_cast<float> (sampleRate);
    const float damp = 0.35f; // fixed, darkens the repeats

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = inGain.next();
        const float out = outGain.next();

        smoothedDelay += (targetDelay - smoothedDelay) * 0.0008f;

        const float delayedL = line.read (0, smoothedDelay);
        const float delayedR = line.read (1, smoothedDelay);

        // Damped feedback: the tail gets darker as it repeats.
        dampStateL += (delayedL - dampStateL) * damp;
        dampStateR += (delayedR - dampStateR) * damp;

        const float writeL = left[i] * in + dampStateL * feedback;
        const float writeR = right[i] * in + dampStateR * feedback;

        line.write (0, writeL);
        line.write (1, writeR);
        line.advance();

        left[i]  = dryLeft (i)  + delayedL * out * 0.7f;
        right[i] = dryRight (i) + delayedR * out * 0.7f;
    }

    if (! engaged && outGain.getCurrent() < 0.001f && inGain.getCurrent() < 0.001f)
        line.reset();
}

} // namespace opxy
