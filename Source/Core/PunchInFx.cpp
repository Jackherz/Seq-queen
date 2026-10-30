#include "PunchInFx.h"

namespace opxy
{

void PunchInFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlockSize = clamp (newMaxBlockSize, 16, kMaxBlockSize);

    dryL.assign (static_cast<std::size_t> (maxBlockSize), 0.0f);
    dryR.assign (static_cast<std::size_t> (maxBlockSize), 0.0f);

    wetRamp.prepare (sampleRate);
    wetRamp.setTimeSeconds (0.03f);
    wetRamp.setTarget (0.0f);
    wetRamp.snapToTarget();

    reset();
}

void PunchInFx::reset()
{
    wetRamp.setTarget (0.0f);
    wetRamp.snapToTarget();
    engaged = false;
    for (auto& s : dryL) s = 0.0f;
    for (auto& s : dryR) s = 0.0f;
}

void PunchInFx::setRampSeconds (float seconds) noexcept
{
    wetRamp.setTimeSeconds (seconds);
    wetRamp.setTarget (wetRamp.getTarget()); // re-arm at the new rate
}

void PunchInFx::setEngaged (bool shouldBeEngaged)
{
    if (shouldBeEngaged == engaged)
        return;

    engaged = shouldBeEngaged;
    wetRamp.setTarget (engaged ? amount : 0.0f);
    onEngage (engaged);
}

void PunchInFx::process (float* left, float* right, int numSamples)
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    numSamples = std::min (numSamples, maxBlockSize);

    captureDry (left, right, numSamples);
    processWet (left, right, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = wetRamp.next();
        left[i]  = dryL[static_cast<std::size_t> (i)]  * (1.0f - mix) + left[i]  * mix;
        right[i] = dryR[static_cast<std::size_t> (i)] * (1.0f - mix) + right[i] * mix;
        left[i]  = sanitise (left[i]);
        right[i] = sanitise (right[i]);
    }
}

void PunchInFx::captureDry (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        dryL[static_cast<std::size_t> (i)] = left[i];
        dryR[static_cast<std::size_t> (i)] = right[i];
    }
}

} // namespace opxy
