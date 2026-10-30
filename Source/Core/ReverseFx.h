#pragma once

// Reverse: plays the audio that just happened backwards. On engage it takes the
// last `length` of audio (a 16th up to a bar, in host beats) and plays it in
// reverse; each time a segment finishes it grabs the newest one, so holding it
// keeps reversing the previous beat. Slice edges fade so the loop doesn't crack.
//
// Macros: param 0 = segment length (1/16, 1/8, 1/4, 1/2, 1 bar),
//         param 1 = playback speed (0.5x .. 2x).

#include "PunchInFx.h"
#include "DelayLine.h"

namespace opxy
{

class ReverseFx : public PunchInFx
{
public:
    const char* getName() const override { return "Reverse"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    void listen (const float* left, const float* right, int numSamples) override;
    void setTransport (const TransportInfo& transport) override;

    /** Segment length in beats for a macro value (shared with the UI). */
    static double lengthBeats (float value) noexcept;

protected:
    void onEngage (bool nowEngaged) override;
    void processWet (float* left, float* right, int numSamples) override;

private:
    void record (float left, float right) noexcept;
    void capture() noexcept;

    DelayLine history;
    double writeAbs = 0.0;
    double anchor = 0.0;          // absolute position the reversed segment starts from
    double readOffset = 0.0;      // how far back from the anchor we are
    double segmentLength = 0.0;
    double fadeSamples = 1.0;
    bool   playing = false;
    double bpm = 120.0;

    float lengthParam = 0.5f;
    float speedParam = 1.0f / 3.0f;   // 1x
};

} // namespace opxy
