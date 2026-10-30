#include "HighPassFx.h"

#include <cmath>

namespace opxy
{

float HighPassFx::sweepHz (float value) noexcept
{
    return kRestCutoff * std::pow (2.0f, 2.0f + clamp (value, 0.0f, 1.0f) * 6.5f);   // 80 Hz .. ~7 kHz
}

void HighPassFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    filterL.prepare (sampleRate);
    filterR.prepare (sampleRate);
    reset();
}

void HighPassFx::reset()
{
    PunchInFx::reset();
    filterL.reset();
    filterR.reset();
    currentCutoff = kRestCutoff;
    currentResonance = 0.1f;
}

void HighPassFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        depth = value;
    else if (index == 1)
        resonanceBoost = value;
}

float HighPassFx::getPunchParam (int index) const
{
    return index == 0 ? depth : (index == 1 ? resonanceBoost : 0.0f);
}

void HighPassFx::processWet (float* left, float* right, int numSamples)
{
    const float targetCutoff = engaged ? sweepHz (depth) : kRestCutoff;
    const float targetRes    = engaged ? clamp (0.1f + resonanceBoost * 0.8f, 0.0f, 0.9f) : 0.1f;

    // Slower than the low-pass sweep: a build-up should rise, not snap.
    const float coeff = onePoleCoeff (sampleRate, 0.12f);

    for (int i = 0; i < numSamples; ++i)
    {
        currentCutoff    += (targetCutoff - currentCutoff) * coeff;
        currentResonance += (targetRes - currentResonance) * coeff;

        filterL.setParams (currentCutoff, currentResonance);
        filterR.setParams (currentCutoff, currentResonance);

        left[i]  = filterL.process (left[i], Svf::Mode::HighPass);
        right[i] = filterR.process (right[i], Svf::Mode::HighPass);
    }
}

} // namespace opxy
