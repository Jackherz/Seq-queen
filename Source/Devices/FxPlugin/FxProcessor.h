#pragma once

// -----------------------------------------------------------------------------
//  Seq Queen FX: the six punch-in effects as an audio effect.
//
//  Put it on any track or bus. Punches come from:
//    * the on-screen pads (hold),
//    * MIDI notes: ten notes from `trigger_note` up (C1..A1 by default). In Live,
//      draw them in a clip on a MIDI track and set that track's "MIDI To" to this
//      track / Seq Queen FX. Note velocity sets how hard it punches.
//    * each effect's "Punch" parameter (0 = off, above 0 = strength): automation,
//      or the Seq Queen Max for Live device on the same track.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "../../Core/FxBus.h"

class SeqQueenFxProcessor : public juce::AudioProcessor
{
public:
    SeqQueenFxProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Seq Queen FX"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** On-screen pad, message thread. */
    void setPadHeld (int fx, bool held);
    bool isPunching (int fx) const { return bus.isEngaged (fx); }
    float getPunchDepth (int fx) const { return bus.getEffectiveAmount (fx); }
    int getTriggerNote() const;

    juce::AudioProcessorValueTreeState state;

private:
    struct FxParams
    {
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* a = nullptr;
        std::atomic<float>* b = nullptr;
        std::atomic<float>* velocity = nullptr;
        std::atomic<float>* punch = nullptr;
    };

    opxy::FxBus bus;
    FxParams fxParams[opxy::kMaxFx];
    std::atomic<float>* triggerNote = nullptr;
    std::atomic<bool> padHeld[opxy::kMaxFx];
    bool noteHeld[opxy::kMaxFx] {};
    juce::AudioBuffer<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenFxProcessor)
};
