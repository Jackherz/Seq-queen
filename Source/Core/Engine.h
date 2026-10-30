#pragma once

// -----------------------------------------------------------------------------
//  Engine: the whole instrument, JUCE-free.
//
//      sequencer --> synth voices --> per-track buffers --> focus-track FX
//                                                      \-> sum -> master FX -> out
//
//  Signal flow per block:
//    1. Sequencer clock advances; note events are produced with sample offsets.
//    2. Voices are started at those offsets (sub-block rendering keeps timing).
//    3. Each track renders into its own stereo buffer so punch-in FX can target
//       one track without touching the rest.
//    4. The focus track's buffer is processed by the "focus" FX chain.
//    5. Tracks are summed (with smoothed level/mute) through the master chain.
// -----------------------------------------------------------------------------

#include "Types.h"
#include "Pattern.h"
#include "Sequencer.h"
#include "SynthEngine.h"
#include "FxChain.h"
#include "ScalarUtils.h"
#include <atomic>
#include <vector>

namespace opxy
{

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Renders `numSamples` frames. `outputChannels` may be mono or stereo.
        Any block size is accepted; large blocks are chunked internally. */
    void process (float* const* outputChannels,
                  int numChannels,
                  int numSamples,
                  const TransportInfo& transport);

    Pattern& getPattern() noexcept { return pattern; }
    const Pattern& getPattern() const noexcept { return pattern; }

    FxSettings& getFxSettings (int fxIndex) noexcept;
    const FxSettings& getFxSettings (int fxIndex) const noexcept;

    Sequencer& getSequencer() noexcept { return sequencer; }

    /** Plays a note immediately on `track` using that track's current voice
        settings. Used for "play along" MIDI input; the note ends on its own
        after one step's worth of gate time. */
    void playNote (int track, int note, float velocity);

    void setMasterGainDb (float db) noexcept { masterGainDb.store (clamp (db, -60.0f, 12.0f)); }
    void setFocusTrack (int track) noexcept { focusTrack.store (clamp (track, 0, kMaxTracks - 1)); }
    void setSwing (float amount01) noexcept { sequencer.setSwing (amount01); }
    void setFreeRunTempo (double bpm) noexcept { sequencer.setFreeRunTempo (bpm); }

    /** Note events produced by the last process() call, offsets relative to the
        start of the block. Used by the plugin wrapper to emit MIDI. */
    int getLastEventCount() const noexcept { return lastEventCount; }
    const StepEvent* getLastEvents() const noexcept { return lastEvents; }

    float getOutputPeak() const noexcept { return outputPeak.load (std::memory_order_relaxed); }

    /** Wet amount the FX is actually using this block: base amount scaled by
        trigger velocity, or the sequenced lane value when the lane is driving it.
        0 when the FX is not engaged. UI can show this to explain why a soft
        MIDI hit feels weaker than a hard one. */
    float getEffectiveAmount (int fxIndex) const noexcept
    {
        return effectiveAmount[clamp (fxIndex, 0, kMaxFx - 1)].load (std::memory_order_relaxed);
    }

    /** True when the FX is actually punching right now, from any source:
        pad / MIDI / automation hold, or a sequenced punch lane. Use this for UI
        feedback; FxSettings::engaged is only the manual trigger input. */
    bool isFxEngaged (int fxIndex) const noexcept
    {
        return fxEngaged[clamp (fxIndex, 0, kMaxFx - 1)].load (std::memory_order_relaxed);
    }

    /** Records the velocity of whatever just punched an FX (MIDI note or pad). */
    void setPunchVelocity (int fxIndex, float velocity) noexcept
    {
        if (fxIndex < 0 || fxIndex >= kMaxFx)
            return;
        fxSettings[fxIndex].velocity.store (clamp (velocity, 0.0f, 1.0f),
                                            std::memory_order_relaxed);
    }
    double getSampleRate() const noexcept { return sampleRate; }
    int getMaxBlockSize() const noexcept { return maxBlockSize; }

private:
    void processChunk (float* const* outputChannels, int numChannels,
                       int outputOffset, int numSamples, const TransportInfo& transport);
    void applyFxRouting();
    void renderVoices (int startSample, int count);

    Pattern pattern;
    Sequencer sequencer;
    SynthEngine synth;
    FxChain masterChain;
    FxChain focusChain;
    FxSettings fxSettings[kMaxFx];

    std::vector<float> trackBuffer[kMaxTracks][2];
    std::vector<float> mixL, mixR;

    Ramp masterGainRamp;
    Ramp trackLevelRamp[kMaxTracks];

    StepEvent lastEvents[kMaxEventsPerBlock];
    int lastEventCount = 0;

    std::atomic<float> masterGainDb { -3.0f };
    std::atomic<int>   focusTrack { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> effectiveAmount[kMaxFx];
    std::atomic<bool>  fxEngaged[kMaxFx];
    std::atomic<bool>  midiOutEnabled { true };

    double sampleRate = 44100.0;
    int    maxBlockSize = kMaxBlockSize;
    double samplesPerStepCache = 6000.0;
};

} // namespace opxy
