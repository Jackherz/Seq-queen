#pragma once

// Reverb burst: send-style reverb wash. Held, it dumps the track into a small
// Schroeder reverb; released, the wet signal is allowed to decay naturally
// instead of being cut, which is the whole point of the gesture.
//
// Reverb is 4 damped comb filters + 2 allpasses per channel (Freeverb tunings),
// which is cheap, stable and plenty for a momentary effect.
//
// Macros: param 0 = decay/size, param 1 = damping.

#include "PunchInFx.h"
#include "DelayLine.h"
#include <vector>

namespace opxy
{

class ReverbBurstFx : public PunchInFx
{
public:
    const char* getName() const override { return "Reverb"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    void process (float* left, float* right, int numSamples) override;

protected:
    void processWet (float* left, float* right, int numSamples) override;
    bool hasTail() const override;

private:
    struct Comb
    {
        DelayLine line;
        float state = 0.0f;
        float process (float input, int delaySamples, float feedback, float damping) noexcept;
    };

    struct Allpass
    {
        DelayLine line;
        float process (float input, int delaySamples, float gain = 0.5f) noexcept;
    };

    static constexpr int kCombs = 4;
    Comb combs[2][kCombs];
    Allpass allpasses[2][2];
    int combDelays[kCombs] = {};
    int allpassDelays[2] = {};

    float decay = 0.8f;     // 0..1 -> feedback
    float damping = 0.4f;   // 0..1 -> comb low-pass amount
    Ramp outGain;
};

} // namespace opxy
