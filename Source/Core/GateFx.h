#pragma once

// Gate / pump: chops the audio in time with the host. Shape 0 is a hard trance
// gate (on for the first half of each division, off for the second); shape 1 is
// a sidechain-style pump (ducked on the beat, swelling back up). In between
// blends the two. The gain is smoothed over ~1.5 ms so the chops never click.
//
// Locks to the host's beat position when the transport runs, and free-runs at
// the host tempo otherwise.
//
// Macros: param 0 = rate (1/4, 1/8, 1/16, 1/32), param 1 = shape (gate .. pump).

#include "PunchInFx.h"

namespace opxy
{

class GateFx : public PunchInFx
{
public:
    const char* getName() const override { return "Gate"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

    void setTransport (const TransportInfo& transport) override;

    /** Division length in beats for a macro value (shared with the UI). */
    static double divisionBeats (float value) noexcept;

protected:
    void processWet (float* left, float* right, int numSamples) override;

private:
    double bpm = 120.0;
    bool   locked = false;        // following host ppq this block
    double blockPpq = 0.0;
    double freeBeats = 0.0;       // beat position when there is no host position
    float  gain = 1.0f;

    float rateParam = 2.0f / 3.0f;   // 1/16
    float shapeParam = 0.3f;
};

} // namespace opxy
