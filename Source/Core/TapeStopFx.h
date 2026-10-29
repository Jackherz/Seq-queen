#pragma once

// Tape stop ("vinyl brake"): the playback rate ramps to zero, so pitch falls
// away and the audio freezes; on release the rate climbs back and the read head
// catches up to real time, which gives the classic pitch-up "tape start".
//
// Implementation: a stereo delay line whose read head advances at `rate`
// samples per output sample instead of 1. At rate 0 the head is frozen, so the
// same sample repeats and the sound stops dead.
//
// Macros: param 0 = stop time (s), param 1 = start time (s).

#include "PunchInFx.h"
#include "DelayLine.h"

namespace opxy
{

class TapeStopFx : public PunchInFx
{
public:
    const char* getName() const override { return "Tape Stop"; }

    void prepare (double newSampleRate, int newMaxBlockSize) override;
    void reset() override;

    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

protected:
    void onEngage (bool nowEngaged) override;
    void processWet (float* left, float* right, int numSamples) override;

private:
    void computeDelayGap() noexcept;

    DelayLine line;
    double readAbs = 0.0;    // absolute read position (in samples)
    double writeAbs = 0.0;   // absolute write position (in samples)

    float rate = 1.0f;
    float targetRate = 1.0f;
    float rateStep = 0.0f;
    float stopTime = 0.35f;
    float startTime = 0.25f;
    float gainCompensation = 1.0f;
};

} // namespace opxy
