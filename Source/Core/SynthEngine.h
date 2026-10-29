#pragma once

// -----------------------------------------------------------------------------
//  SynthEngine: the sound source for the 8 tracks.
//
//  The OP-XY has swappable "engines" per track; here each track picks one of a
//  handful of simple subtractive flavours (sine/tri/saw/square/noise) plus an
//  optional noise layer. That is enough to build a kick, a bass, a hat and a
//  pluck and to hear every sequencer and punch-in FX feature.
//
//  Voices are allocated from one shared pool so a busy drum track can't starve
//  the others of memory, and every voice remembers which track it belongs to so
//  per-track punch-in FX can be applied after rendering.
// -----------------------------------------------------------------------------

#include "Types.h"
#include "Envelope.h"
#include "Oscillator.h"
#include "Prng.h"
#include "Svf.h"
#include <vector>

namespace opxy
{

struct Voice
{
    bool  active = false;
    int   track = 0;
    int   voiceIndex = 0;
    float age = 0.0f;

    EngineType engine = EngineType::Saw;
    Oscillator osc;
    Svf  filterL, filterR;
    Adsr env;
    DcBlocker dcL, dcR;
    Prng rng;

    float noiseAmount = 0.0f;
    float panL = 0.7071f, panR = 0.7071f;
    float level = 0.7f;
    float velocityGain = 1.0f;

    float gateSamples = 1000.0f;
    float elapsed = 0.0f;
    bool  gateReleased = false;
    float releaseSeconds = 0.08f;
};

class SynthEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Starts a voice from a sequencer event. `samplesPerStep` converts the
        event's gate length (in 16th steps) into samples. */
    void startNote (const StepEvent& event, double samplesPerStep);

    /** Renders `count` samples (starting at `startSample`) of every voice that
        belongs to `track`, adding into the two buffers. Realtime safe. */
    void renderTrack (int track, float* left, float* right, int startSample, int count);

    int getActiveVoiceCount() const;
    bool anyVoiceActive() const;

private:
    Voice& findFreeVoice();

    double sampleRate = 44100.0;
    std::vector<Voice> voices;
    unsigned int voiceCounter = 0;
};

} // namespace opxy
