#include "FilterSweepFx.h"

#include <cmath>

namespace opxy
{

void FilterSweepFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    filterL.prepare (sampleRate);
    filterR.prepare (sampleRate);
    reset();
}

void FilterSweepFx::reset()
{
    PunchInFx::reset();
    filterL.reset();
    filterR.reset();
    currentCutoff = baseCutoff;
    currentResonance = 0.1f;
}

void FilterSweepFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        depth = value;
    else if (index == 1)
        resonanceBoost = value;
}

float FilterSweepFx::getPunchParam (int index) const
{
    if (index == 0)
        return depth;
    if (index == 1)
        return resonanceBoost;
    return 0.0f;
}

void FilterSweepFx::processWet (float* left, float* right, int numSamples)
{
    // Target cutoff drops by up to ~5 octaves as the sweep engages.
    const float engagedCutoff = clamp (baseCutoff * std::pow (0.03f, depth), 40.0f, 18000.0f);
    const float targetCutoff  = engaged ? engagedCutoff : baseCutoff;
    const float targetRes     = engaged ? clamp (0.15f + resonanceBoost * 0.8f, 0.0f, 0.95f) : 0.1f;

    // ~25 ms glide: fast enough to feel immediate, slow enough to stay click free.
    const float coeff = onePoleCoeff (sampleRate, 0.025f);

    for (int i = 0; i < numSamples; ++i)
    {
        currentCutoff    += (targetCutoff - currentCutoff) * coeff;
        currentResonance += (targetRes - currentResonance) * coeff;

        filterL.setParams (currentCutoff, currentResonance);
        filterR.setParams (currentCutoff, currentResonance);

        left[i]  = filterL.process (left[i], Svf::Mode::LowPass);
        right[i] = filterR.process (right[i], Svf::Mode::LowPass);
    }
}

} // namespace opxy
