#pragma once

// Bitcrush / lo-fi: momentary bit-depth and sample-rate reduction, plus a touch
// of drive so it still cuts through when engaged.
//
// Macros: param 0 = bit depth reduction, param 1 = sample-rate reduction.

#include "PunchInFx.h"

namespace opxy
{

class BitcrushFx : public PunchInFx
{
public:
    const char* getName() const override { return "Bitcrush"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

protected:
    void processWet (float* left, float* right, int numSamples) override;

private:
    float bitsReduction = 0.7f;    // 0..1 -> 16 down to 3 bits
    float rateReduction = 0.5f;    // 0..1 -> 1x down to ~16x decimation
    float heldL = 0.0f, heldR = 0.0f;
    int   counter = 0;
};

} // namespace opxy
