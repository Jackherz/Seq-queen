#pragma once

// The editor is deliberately built from plain components and hand-drawn paint()
// code rather than images or a custom LookAndFeel: it compiles fast, resizes
// cleanly, and is easy for the next agent to rearrange.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components/StepGrid.h"
#include "Components/PunchPads.h"
#include "Components/PLockPanel.h"
#include "Components/TrackMixer.h"

class SeqQueenAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SeqQueenAudioProcessorEditor (SeqQueenAudioProcessor& processor);
    ~SeqQueenAudioProcessorEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void setStatusMessage (const juce::String& message);

    SeqQueenAudioProcessor& proc;

    opxy::StepGridComponent stepGrid;
    opxy::PunchBarComponent punchBar;
    opxy::PLockPanelComponent pLockPanel;
    opxy::TrackMixerComponent trackMixer;

    juce::Slider masterGain, swing, tempo;
    juce::ComboBox focusTrack, visibleSteps;
    juce::TextButton midiOutButton { "MIDI OUT" };
    juce::TextButton fxNotesButton { "FX NOTES" };
    juce::Label status;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterGainAtt, swingAtt, tempoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> focusTrackAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAtt, fxNotesAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenAudioProcessorEditor)
};
