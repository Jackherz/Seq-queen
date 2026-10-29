#pragma once

// -----------------------------------------------------------------------------
//  Pattern: the mutable musical state of the 8-track sequencer.
//
//  Threading model (deliberate, and documented in docs/ARCHITECTURE.md):
//  every field that the UI can edit and the audio thread can read is a
//  std::atomic. That makes single-field edits lock free and click free, which
//  is exactly the failure mode the original brief flagged (unprotected p-lock
//  writes from the UI thread). Multi-field edits are still only "eventually
//  consistent", which is fine musically: a step flip lands one block later.
//
//  Anything that needs several fields to change atomically (e.g. loading a
//  whole pattern) is done from the message thread while the engine is stopped,
//  or through Pattern serialisation which is read by the audio thread as text
//  only in the non-real-time demo tool.
// -----------------------------------------------------------------------------

#include "Types.h"
#include "ScalarUtils.h"
#include "Prng.h"
#include <atomic>

namespace opxy
{

/** One step of one track. */
struct Step
{
    std::atomic<bool>  active    { false };
    std::atomic<float> note      { 60.0f };  // MIDI note, fractional allowed (micro-tuning)
    std::atomic<float> velocity  { 0.8f };
    std::atomic<float> gate      { 0.5f };   // length in 16th steps
    std::atomic<int>   ratchet   { 1 };      // 1..kMaxRatchet sub-hits
    std::atomic<float> nudge     { 0.0f };   // micro-timing, fraction of a step (+/-0.5)

    std::atomic<std::uint32_t> plockMask { 0u };
    std::atomic<float> plocks[kNumPlockParams];

    Step()
    {
        for (auto& p : plocks)
            p.store (0.0f, std::memory_order_relaxed);
    }

    Step (const Step&) = delete;
    Step& operator= (const Step&) = delete;

    void setPLock (PlockParam param, float value) noexcept
    {
        const int index = static_cast<int> (param);
        if (index < 0 || index >= kNumPlockParams)
            return;
        plocks[index].store (value, std::memory_order_relaxed);
        plockMask.fetch_or (1u << index, std::memory_order_release);
    }

    void clearPLock (PlockParam param) noexcept
    {
        const int index = static_cast<int> (param);
        if (index < 0 || index >= kNumPlockParams)
            return;
        plockMask.fetch_and (~(1u << index), std::memory_order_release);
    }

    void clearAllPLocks() noexcept
    {
        plockMask.store (0u, std::memory_order_release);
    }

    bool hasPLock (PlockParam param) const noexcept
    {
        return (plockMask.load (std::memory_order_acquire) & (1u << static_cast<int> (param))) != 0u;
    }

    /** Returns the locked value, or `defaultValue` when the step has no lock. */
    float getPLock (PlockParam param, float defaultValue) const noexcept
    {
        const int index = static_cast<int> (param);
        if (index < 0 || index >= kNumPlockParams)
            return defaultValue;
        if (! hasPLock (param))
            return defaultValue;
        return plocks[index].load (std::memory_order_relaxed);
    }

    bool hasAnyPLock() const noexcept
    {
        return plockMask.load (std::memory_order_relaxed) != 0u;
    }

    void reset()
    {
        active.store (false);
        note.store (60.0f);
        velocity.store (0.8f);
        gate.store (0.5f);
        ratchet.store (1);
        nudge.store (0.0f);
        clearAllPLocks();
    }
};

/** One track: its steps, its voice settings, and its generative "brains". */
struct Track
{
    std::atomic<int>   length { 16 };   // 1..kMaxSteps -> polyrhythms
    std::atomic<int>   engine { static_cast<int> (EngineType::Saw) };
    std::atomic<bool>  muted  { false };

    // Voice settings (the defaults that p-locks override per step)
    std::atomic<float> level     { 0.7f };
    std::atomic<float> pan       { 0.0f };
    std::atomic<float> cutoff    { 3200.0f };
    std::atomic<float> resonance { 0.15f };
    std::atomic<float> decay     { 0.35f };
    std::atomic<bool>  noiseOn   { false }; // noise layer mixed into the voice (hats/snares)

    // Brains
    std::atomic<float> probability   { 1.0f };
    std::atomic<bool>  euclidOn      { false };
    std::atomic<int>   euclidHits    { 4 };
    std::atomic<int>   euclidOffset  { 0 };
    std::atomic<int>   ratchet       { 1 };     // track-wide ratchet multiplier
    std::atomic<float> randomPitch   { 0.0f };  // semitone range for the randomise brain
    std::atomic<float> randomVel     { 0.0f };  // 0..1 amount of velocity randomisation
    std::atomic<int>   scaleType     { static_cast<int> (ScaleType::Off) };
    std::atomic<int>   scaleRoot     { 0 };

    Step steps[kMaxSteps];

    Track() = default;
    Track (const Track&) = delete;
    Track& operator= (const Track&) = delete;

    int getClampedLength() const noexcept
    {
        return clamp (length.load (std::memory_order_relaxed), 1, kMaxSteps);
    }

    /** Effective MIDI note for a step, after the randomise + scale brains. */
    int effectiveNote (int stepIndex, Prng& rng) const noexcept;

    /** Effective velocity after the velocity randomiser. */
    float effectiveVelocity (int stepIndex, Prng& rng) const noexcept;

    void reset()
    {
        for (auto& s : steps)
            s.reset();
    }
};

/** The whole 8-track pattern. Non-copyable because the steps hold atomics;
    save/load goes through the serialiser in PatternState.h instead. */
struct Pattern
{
    Track tracks[kMaxTracks];

    Pattern() = default;
    Pattern (const Pattern&) = delete;
    Pattern& operator= (const Pattern&) = delete;

    /** Silences every step on every track but keeps the voice settings. */
    void clearSteps()
    {
        for (auto& track : tracks)
            for (auto& step : track.steps)
                step.active.store (false);
    }
};

/** Quantises a note to a scale. `scaleType` 0 (Off) returns the note unchanged. */
int quantiseToScale (int note, int root, int scaleType) noexcept;

/** True if `step` of this track fires, taking Euclidean + probability into account. */
bool trackStepFires (const Track& track, int stepIndex, Prng& rng) noexcept;

/** The Euclidean mask for a given track configuration (bit i = step i hits). */
std::uint64_t euclideanMask (int numSteps, int hits, int offset) noexcept;

} // namespace opxy
