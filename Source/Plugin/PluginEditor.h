#pragma once

// The editor is plain components plus hand-drawn chrome: no images, no binary
// resources, so the whole skin is searchable source and resizes cleanly.
// Panel geometry here matches docs/ui/render_mockup.py (the design spec).

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Components/StepGrid.h"
#include "Components/PunchPads.h"
#include "Components/PLockPanel.h"
#include "Components/TrackMixer.h"
#include "SeqQueenLookAndFeel.h"

class SeqQueenAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SeqQueenAudioProcessorEditor (SeqQueenAudioProcessor& processor);
    ~SeqQueenAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void setStatusMessage (const juce::String& message);
    juce::String holdPadMessage() const;

    SeqQueenAudioProcessor& proc;
    opxy::LookAndFeel lookAndFeel;

    opxy::StepGridComponent stepGrid;
    opxy::PunchBarComponent punchBar;
    opxy::PLockPanelComponent pLockPanel;
    opxy::TrackMixerComponent trackMixer;

    juce::Slider masterGain, swing, tempo;
    juce::ComboBox focusTrack, visibleSteps;
    juce::TextButton midiOutButton { "MIDI OUT" };
    juce::TextButton fxNotesButton { "FX NOTES" };
    juce::TextButton laneModeButton { "FX LANES" };
    juce::Label status;

    // One list keeps the header's parameter wiring tidy; order does not matter.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenAudioProcessorEditor)
};
