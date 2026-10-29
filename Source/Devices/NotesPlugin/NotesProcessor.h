#pragma once

// -----------------------------------------------------------------------------
//  Seq Queen Notes: OP-XY style note punches between a clip and an instrument.
//
//  Live doesn't host third-party MIDI effects, so this is an instrument that
//  makes no sound and outputs MIDI:
//    1. Put it on a MIDI track and play your clip on that track.
//    2. On the instrument track (Drum Rack, synth...), set MIDI From -> that
//       track -> Seq Queen Notes, and Monitor -> In.
//    3. Hold the trigger notes (C-2..B-2 by default, below anything a Drum Rack
//       uses) in the clip, from a keyboard, or with the pads.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "../../Core/NoteFx.h"

class SeqQueenNotesProcessor : public juce::AudioProcessor
{
public:
    SeqQueenNotesProcessor();

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Seq Queen Notes"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void setPadHeld (int fx, bool held) { noteFx.setPadHeld (fx, held); }
    bool isPunching (int fx) const { return noteFx.isActive (fx); }
    int getTriggerNote() const;
    bool isTransportRunning() const { return transportRunning.load(); }

    juce::AudioProcessorValueTreeState state;

private:
    opxy::NoteFx noteFx;
    std::atomic<float>* triggerNote = nullptr;
    std::atomic<float>* kickNote = nullptr;
    std::atomic<float>* snareNote = nullptr;
    std::atomic<float>* hatNote = nullptr;
    std::atomic<float>* repeatRate = nullptr;
    std::atomic<bool> transportRunning { false };

    std::vector<opxy::NoteEvent> input, output;
    juce::MidiBuffer passThrough;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenNotesProcessor)
};
