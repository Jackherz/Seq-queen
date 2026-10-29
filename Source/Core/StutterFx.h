#pragma once

// Stutter / repeat: freezes the last slice of audio and retriggers it. While
// held, a slice of `loopLength` seconds is looped at a playback rate, giving the
// OP-XY "beat repeat" feel. Release crossfades back to the live signal (the base
// class ramp does that), so the transition stays click free.
//
// Macros: param 0 = slice length, param 1 = playback rate (0.5 .. 2.0).

#include "PunchInFx.h"
#include "DelayLine.h"

namespace opxy
{

class StutterFx : public PunchInFx
{
public:
    const char* getName() const override { return "Stutter"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

protected:
    void onEngage (bool nowEngaged) override;
    void processWet (float* left, float* right, int numSamples) override;

private:
    DelayLine history;            // rolling history of the live signal
    double writeAbs = 0.0;
    double loopStart = 0.0;
    double loopLength = 0.0;
    double playPos = 0.0;
    double playbackRate = 1.0;
    bool   looping = false;
    double loopElapsed = 0.0;      // samples into the current repetition
    double fadeSamples = 0.0;      // short declick window at loop edges

    float sliceSeconds = 0.125f;
    float rateParam = 0.5f;       // 0..1 -> 0.5x .. 2x
};

} // namespace opxy
