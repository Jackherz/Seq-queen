#pragma once

#include "NotesProcessor.h"
#include "../Shared/DeviceUi.h"
#include "../Shared/SeqQueenLookAndFeel.h"

class SeqQueenNotesEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeqQueenNotesEditor (SeqQueenNotesProcessor&);
    ~SeqQueenNotesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;

    SeqQueenNotesProcessor& proc;
    opxy::LookAndFeel lookAndFeel;

    std::unique_ptr<opxy::ui::PunchPad> pads[opxy::kNumNoteFx];

    opxy::ui::LabelledKnob triggerKnob { "trigger notes" };
    opxy::ui::LabelledKnob kickKnob { "kick note" };
    opxy::ui::LabelledKnob snareKnob { "snare note" };
    opxy::ui::LabelledKnob hatKnob { "hat note" };
    opxy::ui::LabelledKnob swingKnob { "groove swing" };
    juce::ComboBox repeatRate;
    std::unique_ptr<SliderAttachment> triggerAttachment, kickAttachment, snareAttachment, hatAttachment, swingAttachment;
    std::unique_ptr<ComboAttachment> repeatAttachment;

    int shownTrigger = -1;
    bool shownRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenNotesEditor)
};
