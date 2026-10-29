#pragma once

// The six punch-in FX pads. They are momentary: press and hold to punch the
// effect in, release to let it go. Host automation of the "Punch" parameter
// lights the same pads, so what you see matches what you hear.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace opxy
{

class PunchPadComponent : public juce::Component,
                          private juce::Timer
{
public:
    PunchPadComponent (SeqQueenAudioProcessor& processor, int fxIndex);
    ~PunchPadComponent() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    SeqQueenAudioProcessor& proc;
    int index = 0;
    juce::String title;

    bool lastEngaged = false;
    float lastAmount = -1.0f;
};

class PunchBarComponent : public juce::Component
{
public:
    explicit PunchBarComponent (SeqQueenAudioProcessor& processor);
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    SeqQueenAudioProcessor& proc;
    std::vector<std::unique_ptr<PunchPadComponent>> pads;
};

} // namespace opxy
