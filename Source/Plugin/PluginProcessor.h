#pragma once

// -----------------------------------------------------------------------------
//  JUCE wrapper around opxy::Engine.
//
//  Responsibilities, in order of importance:
//    1. Own the engine and keep it in sync with the host (transport, parameters).
//    2. Translate MIDI input into punch-in FX holds and playable notes.
//    3. Optionally emit the sequencer as MIDI so it can drive other tracks.
//    4. Save/load: APVTS parameters + the step grid blob.
//
//  It is an instrument (IS_SYNTH), so it makes its own sound: drop it on a MIDI
//  track in Ableton and it plays. It also passes punch-in FX over its own output.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "ParamIDs.h"
#include "../Core/Engine.h"

class SeqQueenAudioProcessor : public juce::AudioProcessor
{
public:
    SeqQueenAudioProcessor();
    ~SeqQueenAudioProcessor() override = default;

    // --- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Seq Queen"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // --- Engine access (message thread + audio thread) -----------------------
    opxy::Engine& getEngine() noexcept { return engine; }
    const opxy::Engine& getEngine() const noexcept { return engine; }
    opxy::Pattern& getPattern() noexcept { return engine.getPattern(); }
    opxy::Sequencer& getSequencer() noexcept { return engine.getSequencer(); }

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    /** Momentary punch-in hold from the UI pads (does not touch host automation). */
    void setPunchHeld (int fxIndex, bool held);
    bool isPunchHeld (int fxIndex) const;
    bool isPunchEngaged (int fxIndex) const;

    /** True while the punch FX note range is being used, for UI hinting. */
    int getFxNoteBase() const { return fxNoteBase.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    struct TrackParamPointers
    {
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* engine = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* probability = nullptr;
        std::atomic<float>* euclid = nullptr;
        std::atomic<float>* hits = nullptr;
        std::atomic<float>* offset = nullptr;
        std::atomic<float>* ratchet = nullptr;
        std::atomic<float>* randPitch = nullptr;
        std::atomic<float>* randVel = nullptr;
        std::atomic<float>* scale = nullptr;
        std::atomic<float>* root = nullptr;
        std::atomic<float>* mute = nullptr;
        std::atomic<float>* noise = nullptr;
    };

    struct FxParamPointers
    {
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* macroA = nullptr;
        std::atomic<float>* macroB = nullptr;
        std::atomic<float>* ramp = nullptr;
        std::atomic<float>* target = nullptr;
        std::atomic<float>* automationOn = nullptr;
    };

    void cacheParameterPointers();
    void pushParametersToEngine();
    void handleIncomingMidi (const juce::MidiBuffer& midiMessages, int numSamples);
    void writeGeneratedMidi (juce::MidiBuffer& midiMessages, int numSamples);

    opxy::Engine engine;

    TrackParamPointers trackParams[opxy::kMaxTracks];
    FxParamPointers fxParams[opxy::kMaxFx];

    std::atomic<float>* pMasterGain = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pTempo = nullptr;
    std::atomic<float>* pFocusTrack = nullptr;
    std::atomic<float>* pMidiOut = nullptr;
    std::atomic<float>* pFxNotes = nullptr;
    std::atomic<float>* pFxNoteBase = nullptr;

    std::atomic<bool> uiHeld[opxy::kMaxFx] {};
    std::atomic<bool> midiHeld[opxy::kMaxFx] {};

    struct PendingNoteOff
    {
        int channel = 1;
        int note = 60;
        int samplesRemaining = 0;
    };

    std::vector<PendingNoteOff> pendingNoteOffs;
    std::atomic<int> fxNoteBase { 36 };
    bool sawPlayhead = false;

    static constexpr const char* kPatternProperty = "pattern";
    static constexpr const char* kStateVersionProperty = "stateVersion";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenAudioProcessor)
};
