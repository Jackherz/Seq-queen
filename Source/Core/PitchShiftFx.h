#pragma once

// Pitch shift: moves the audio up or down without changing its speed, for the
// chipmunk / slowed-down punch. Classic two-tap delay-line shifter: two read
// heads sweep through a short window half a cycle apart, each faded in and out
// with a sin^2 window so the pair always sums to unity.
//
// Macros: param 0 = interval (-12 .. +12 semitones, in whole steps),
//         param 1 = window (20 .. 120 ms; shorter = tighter, longer = smoother).

#include "PunchInFx.h"
#include "DelayLine.h"

namespace opxy
{

class PitchShiftFx : public PunchInFx
{
public:
    const char* getName() const override { return "Pitch"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    void listen (const float* left, const float* right, int numSamples) override;

    /** Semitones for a macro value, and window seconds (shared with the UI). */
    static int semitones (float value) noexcept;
    static float windowSeconds (float value) noexcept;

protected:
    void processWet (float* left, float* right, int numSamples) override;

private:
    DelayLine line;
    double phase = 0.0;

    float intervalParam = 0.0f;   // -12 st
    float windowParam = 0.4f;
};

} // namespace opxy
