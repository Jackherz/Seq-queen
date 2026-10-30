#pragma once

// -----------------------------------------------------------------------------
//  FxBus: the six punch-in effects as a standalone stereo insert.
//
//  This is what the "Seq Queen FX" audio-effect plugin runs. It is the same
//  FxChain the instrument uses, minus the sequencer: triggers arrive from the
//  outside (a held pad, a MIDI note from a clip in the host, or automation),
//  so the host's own sequencer decides when each punch happens.
// -----------------------------------------------------------------------------

#include "FxChain.h"
#include <atomic>

namespace opxy
{

class FxBus
{
public:
    FxBus();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Host tempo/position for the next process() call (tempo-synced FX). */
    void setTransport (const TransportInfo& newTransport) noexcept { transport = newTransport; }

    /** Processes a stereo block in place. Any block size is accepted. */
    void process (float* left, float* right, int numSamples);

    FxSettings& getSettings (int fxIndex) noexcept;
    const FxSettings& getSettings (int fxIndex) const noexcept;

    /** Records the velocity of the trigger that just punched an FX (1 = full). */
    void setPunchVelocity (int fxIndex, float velocity) noexcept;

    /** Wet amount the FX is using right now (0 when not engaged). For the UI. */
    float getEffectiveAmount (int fxIndex) const noexcept;
    bool isEngaged (int fxIndex) const noexcept;

private:
    void applySettings();

    FxChain chain;
    FxSettings settings[kMaxFx];
    std::atomic<float> effectiveAmount[kMaxFx];
    std::atomic<bool> engaged[kMaxFx];
    int maxBlockSize = kMaxBlockSize;
    double sampleRate = 44100.0;
    TransportInfo transport;
};

} // namespace opxy
