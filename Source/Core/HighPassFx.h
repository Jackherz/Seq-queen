#pragma once

// High-pass sweep: the build-up filter. While held the cutoff glides up from
// below the audible range, draining the bass out; on release it glides back.
// The mirror image of FilterSweepFx.
//
// Macros: param 0 = sweep (how high the cutoff goes), param 1 = resonance.

#include "PunchInFx.h"
#include "Svf.h"

namespace opxy
{

class HighPassFx : public PunchInFx
{
public:
    const char* getName() const override { return "High-Pass"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    /** Highest cutoff the sweep reaches for a macro value (shared with the UI). */
    static float sweepHz (float value) noexcept;

protected:
    void processWet (float* left, float* right, int numSamples) override;

private:
    static constexpr float kRestCutoff = 20.0f;

    Svf filterL, filterR;
    float depth = 0.75f;
    float resonanceBoost = 0.4f;
    float currentCutoff = kRestCutoff;
    float currentResonance = 0.1f;
};

} // namespace opxy
