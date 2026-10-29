#pragma once

// Editor for the currently selected step:
//   * the step's own values (note, velocity, gate, ratchet, micro-timing)
//   * the six p-lock slots (pitch, cutoff, resonance, decay, velocity, pan)
//
// A p-lock only exists while its LOCK button is lit; that is how a step
// "overrides" the track value instead of always carrying a copy of it.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace opxy
{

class PLockPanelComponent : public juce::Component
{
public:
    explicit PLockPanelComponent (SeqQueenAudioProcessor& processor);
    ~PLockPanelComponent() override = default;

    void setSelectedStep (int track, int step);
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct PLockUI
    {
        PlockParam param { PlockParam::Pitch };
        juce::Slider value;
        juce::TextButton lock { "LOCK" };
    };

    Step* currentStep() const;
    const Track* currentTrack() const;
    float defaultValueFor (PlockParam param) const;
    void applyStepValue (int which, double value);

    SeqQueenAudioProcessor& proc;
    int selectedTrack = 0;
    int selectedStep = 0;

    juce::Slider note, velocity, gate, ratchet, nudge;
    juce::TextButton clearPLocks { "CLEAR P-LOCKS" };
    juce::TextButton clearStep { "CLEAR STEP" };
    juce::Label title;

    std::vector<std::unique_ptr<PLockUI>> pLocks;
};

} // namespace opxy
