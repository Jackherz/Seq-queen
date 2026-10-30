#pragma once

// Delay throw: a send-style effect. While held, the incoming audio is pushed
// into a feedback delay and blended on top of the dry signal. On release the
// input is gated off but the delay keeps ringing, so you get the OP-XY style
// "throw it into the delay and let it wash out" gesture.
//
// Because it is send-style it overrides process() instead of using the base
// class dry/wet crossfade.
//
// Macros: param 0 = delay time, param 1 = feedback.

#include "PunchInFx.h"
#include "DelayLine.h"

namespace opxy
{

class DelayThrowFx : public PunchInFx
{
public:
    const char* getName() const override { return "Delay Throw"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    void process (float* left, float* right, int numSamples) override;

protected:
    void processWet (float* left, float* right, int numSamples) override { (void) left; (void) right; (void) numSamples; }
    bool hasTail() const override;

private:
    DelayLine line;
    float delaySeconds = 0.28f;
    float feedback = 0.45f;
    float smoothedDelay = 0.28f;
    float dampStateL = 0.0f, dampStateR = 0.0f;

    Ramp inGain, outGain;
};

} // namespace opxy
