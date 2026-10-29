#pragma once

// -----------------------------------------------------------------------------
//  Sequencer: sample-accurate 16th-note clock for the 8 tracks.
//
//  * Host-synced when the playhead reports a ppq position, otherwise free runs.
//  * Per-track lengths give polyrhythms (track 3 at 7 steps against track 1 at 16).
//  * Swing, micro-timing (nudge) and ratchet sub-hits are all scheduled through a
//    small fixed-size pending queue, so a hit that lands after the current block
//    still fires at the right sample instead of being quantised to a block edge.
//  * Never allocates: events are written into a caller-supplied array.
// -----------------------------------------------------------------------------

#include "Pattern.h"
#include "Types.h"
#include <atomic>
#include <cstdint>

namespace opxy
{

class Sequencer
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Advances the clock and writes note events triggered inside this block.
        Returns the number of events written to `outEvents`. */
    int process (const Pattern& pattern,
                 const TransportInfo& transport,
                 int numSamples,
                 StepEvent* outEvents,
                 int maxEvents) noexcept;

    /** Tempo used when the host provides no transport (standalone / no playhead). */
    void setFreeRunTempo (double bpm) noexcept { freeRunTempo = clamp (bpm, 20.0, 300.0); }

    /** 0 = straight, 1 = maximum swing (odd 16ths pushed 50% later). */
    void setSwing (float amount01) noexcept { swing = clamp (amount01, 0.0f, 1.0f); }

    float getSwing() const noexcept { return swing; }

    /** Current 16th-step index per track, or -1 when the clock is not running. */
    int getCurrentStep (int track) const noexcept;

    bool isRunning() const noexcept { return running; }
    double getSamplesPerStep() const noexcept { return samplesPerStep; }
    double getStepPosition() const noexcept { return stepPos; }

    /** Total number of 16th-note boundaries passed since starting (global). */
    std::int64_t getGlobalStepCount() const noexcept { return lastBoundary; }

private:
    struct PendingHit
    {
        std::int64_t time = 0;        // absolute sample time in the sequencer's own clock
        int   track = 0;
        int   step = 0;
        int   note = 60;
        float velocity = 0.8f;
        float gateSteps = 0.5f;
        float pan = 0.0f;
        int   engine = 0;
        float cutoff = 3000.0f;
        float resonance = 0.2f;
        float decay = 0.3f;
        float level = 0.7f;
        bool  noise = false;
        bool  primary = false;        // primary hits drive the playhead + MIDI out
    };

    void scheduleHit (const PendingHit& hit) noexcept;
    void flushPending() noexcept;

    double sampleRate = 44100.0;
    double samplesPerStep = 6000.0;
    double stepPos = 0.0;
    double freeRunTempo = 120.0;
    float  swing = 0.0f;

    bool running = false;
    bool wasPlaying = false;
    std::int64_t lastBoundary = -1;
    std::int64_t sampleCounter = 0;

    std::atomic<int> currentStep[kMaxTracks];

    PendingHit pending[kMaxSubHits + 32];
    int pendingCount = 0;

    Prng rng { 0x51ED270Bu };
};

} // namespace opxy
