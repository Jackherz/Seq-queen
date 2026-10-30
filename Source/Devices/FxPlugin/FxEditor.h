#pragma once

#include "FxProcessor.h"
#include "../Shared/DeviceUi.h"
#include "../Shared/SeqQueenLookAndFeel.h"

class SeqQueenFxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeqQueenFxEditor (SeqQueenFxProcessor&);
    ~SeqQueenFxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Column
    {
        std::unique_ptr<opxy::ui::PunchPad> pad;
        std::unique_ptr<opxy::ui::LabelledKnob> knobs[4];   // mix, A, B, velocity
        std::unique_ptr<Attachment> attachments[4];
    };

    void timerCallback() override;

    SeqQueenFxProcessor& proc;
    opxy::LookAndFeel lookAndFeel;
    Column columns[opxy::kMaxFx];
    opxy::ui::LabelledKnob triggerKnob { "trigger notes" };
    std::unique_ptr<Attachment> triggerAttachment;
    int shownTrigger = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqQueenFxEditor)
};
