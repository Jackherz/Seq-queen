#pragma once

// Filter sweep: momentary resonant low-pass. While held the cutoff glides down
// and the resonance comes up; on release it glides back. Glide is a one-pole so
// it can't overshoot or click.
//
// Macros: param 0 = sweep depth, param 1 = resonance boost.

#include "PunchInFx.h"
#include "Svf.h"

namespace opxy
{

class FilterSweepFx : public PunchInFx
{
public:
    const char* getName() const override { return "Filter"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    /** Base cutoff, so the sweep can be relative to the track's own filter. */
    void setBaseCutoff (float hz) noexcept { baseCutoff = clamp (hz, 40.0f, 18000.0f); }

protected:
    void processWet (float* left, float* right, int numSamples) override;

private:
    Svf filterL, filterR;
    float baseCutoff = 9000.0f;
    float depth = 0.75f;
    float resonanceBoost = 0.6f;
    float currentCutoff = 9000.0f;
    float currentResonance = 0.1f;
};

} // namespace opxy
