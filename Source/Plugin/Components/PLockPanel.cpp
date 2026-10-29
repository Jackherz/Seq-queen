#include "PLockPanel.h"
#include "../Theme.h"

namespace opxy
{

PLockPanelComponent::PLockPanelComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    title.setFont (theme::labelFont (11.0f));
    title.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (title);

    // --- step values --------------------------------------------------------
    const auto setUpStepSlider = [this] (juce::Slider& slider, double min, double max,
                                        double interval, double skewMidPoint)
    {
        theme::styleRotary (slider);
        slider.setRange (min, max, interval);
        if (skewMidPoint > 0.0)
            slider.setSkewFactorFromMidPoint (skewMidPoint);
        slider.setDoubleClickReturnValue (true, (min + max) * 0.5);
        addAndMakeVisible (slider);
    };

    setUpStepSlider (note, 0.0, 127.0, 1.0, -1.0);
    setUpStepSlider (velocity, 0.0, 1.0, 0.001, -1.0);
    setUpStepSlider (gate, 0.02, 4.0, 0.01, -1.0);
    setUpStepSlider (ratchet, 1.0, 8.0, 1.0, -1.0);
    setUpStepSlider (nudge, -0.5, 0.5, 0.005, -1.0);

    note.onValueChange      = [this] { applyStepValue (0, note.getValue()); };
    velocity.onValueChange  = [this] { applyStepValue (1, velocity.getValue()); };
    gate.onValueChange      = [this] { applyStepValue (2, gate.getValue()); };
    ratchet.onValueChange   = [this] { applyStepValue (3, ratchet.getValue()); };
    nudge.onValueChange     = [this] { applyStepValue (4, nudge.getValue()); };

    // --- p-lock slots -------------------------------------------------------
    for (int i = 0; i < kNumPlockParams; ++i)
    {
        auto ui = std::make_unique<PLockUI>();
        ui->param = static_cast<PlockParam> (i);

        theme::styleRotary (ui->value);
        ui->value.setRange (0.0, 1.0, 0.001);

        theme::styleToggle (ui->lock);
        ui->lock.setClickingTogglesState (true);
        ui->lock.setButtonText ("LOCK");

        addAndMakeVisible (ui->value);
        addAndMakeVisible (ui->lock);
        pLocks.push_back (std::move (ui));
    }

    // Cutoff is more useful on a log scale, so give it a musical range.
    if (auto* cutoffUI = pLocks[static_cast<std::size_t> (PlockParam::Cutoff)].get())
    {
        cutoffUI->value.setRange (40.0, 18000.0, 1.0);
        cutoffUI->value.setSkewFactorFromMidPoint (1100.0);
    }

    for (auto& ui : pLocks)
    {
        ui->lock.onClick = [this, raw = ui.get()]
        {
            Step* step = currentStep();
            if (step == nullptr)
                return;

            if (raw->lock.getToggleState())
                step->setPLock (raw->param, static_cast<float> (raw->value.getValue()));
            else
                step->clearPLock (raw->param);

            refresh();
        };

        ui->value.onValueChange = [this, raw = ui.get()]
        {
            Step* step = currentStep();
            if (step == nullptr)
                return;

            if (raw->lock.getToggleState())
                step->setPLock (raw->param, static_cast<float> (raw->value.getValue()));
        };
    }

    // --- utility buttons ----------------------------------------------------
    theme::styleToggle (clearPLocks);
    theme::styleToggle (clearStep);
    addAndMakeVisible (clearPLocks);
    addAndMakeVisible (clearStep);

    clearPLocks.onClick = [this]
    {
        if (Step* step = currentStep())
            step->clearAllPLocks();
        refresh();
    };

    clearStep.onClick = [this]
    {
        if (Step* step = currentStep())
        {
            step->reset();
            refresh();
        }
    };

    refresh();
}

Step* PLockPanelComponent::currentStep() const
{
    if (! juce::isPositiveAndBelow (selectedTrack, kMaxTracks)
        || ! juce::isPositiveAndBelow (selectedStep, kMaxSteps))
        return nullptr;

    return &proc.getPattern().tracks[selectedTrack].steps[selectedStep];
}

const Track* PLockPanelComponent::currentTrack() const
{
    if (! juce::isPositiveAndBelow (selectedTrack, kMaxTracks))
        return nullptr;
    return &proc.getPattern().tracks[selectedTrack];
}

float PLockPanelComponent::defaultValueFor (PlockParam param) const
{
    const Track* track = currentTrack();
    if (track == nullptr)
        return 0.0f;

    switch (param)
    {
        case PlockParam::Pitch:     return 0.0f;                       // semitone offset
        case PlockParam::Cutoff:    return track->cutoff.load();
        case PlockParam::Resonance: return track->resonance.load();
        case PlockParam::Decay:     return track->decay.load();
        case PlockParam::Velocity:  return currentStep() != nullptr ? currentStep()->velocity.load() : 0.8f;
        case PlockParam::Pan:       return track->pan.load();
        case PlockParam::Count:     break;
    }
    return 0.0f;
}

void PLockPanelComponent::applyStepValue (int which, double value)
{
    Step* step = currentStep();
    if (step == nullptr)
        return;

    switch (which)
    {
        case 0: step->note.store (static_cast<float> (value)); break;
        case 1: step->velocity.store (static_cast<float> (value)); break;
        case 2: step->gate.store (static_cast<float> (value)); break;
        case 3: step->ratchet.store (static_cast<int> (std::lround (value))); break;
        case 4: step->nudge.store (static_cast<float> (value)); break;
        default: break;
    }
}

void PLockPanelComponent::setSelectedStep (int track, int step)
{
    selectedTrack = juce::jlimit (0, kMaxTracks - 1, track);
    selectedStep = juce::jlimit (0, kMaxSteps - 1, step);
    refresh();
}

void PLockPanelComponent::refresh()
{
    title.setText ("STEP " + juce::String (selectedStep + 1)
                     + "  /  TRACK " + juce::String (selectedTrack + 1),
                   juce::dontSendNotification);

    const Step* step = currentStep();
    if (step == nullptr)
        return;

    note.setValue (step->note.load(), juce::dontSendNotification);
    velocity.setValue (step->velocity.load(), juce::dontSendNotification);
    gate.setValue (step->gate.load(), juce::dontSendNotification);
    ratchet.setValue (step->ratchet.load(), juce::dontSendNotification);
    nudge.setValue (step->nudge.load(), juce::dontSendNotification);

    for (auto& ui : pLocks)
    {
        const bool locked = step->hasPLock (ui->param);
        const float value = locked ? step->getPLock (ui->param, 0.0f)
                                   : defaultValueFor (ui->param);

        ui->lock.setToggleState (locked, juce::dontSendNotification);
        ui->lock.setColour (juce::TextButton::buttonOnColourId, theme::accentAlt);
        ui->value.setValue (value, juce::dontSendNotification);

        // Locked slots glow, unlocked ones stay neutral so the step reads at a glance.
        ui->value.setColour (juce::Slider::rotarySliderFillColourId,
                             locked ? theme::accentAlt : theme::textDim);
    }

    repaint();
}

void PLockPanelComponent::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto topRow = area.removeFromTop (20);
    title.setBounds (topRow.removeFromLeft (220));
    clearPLocks.setBounds (topRow.removeFromRight (120).reduced (0, 1));
    topRow.removeFromRight (6);
    clearStep.setBounds (topRow.removeFromRight (96).reduced (0, 1));

    area.removeFromTop (4);

    // Left block: the step's own values.
    const int stepControls = 5;
    const int slotWidth = 66;
    auto stepArea = area.removeFromLeft (stepControls * slotWidth);
    juce::Slider* stepSliders[stepControls] = { &note, &velocity, &gate, &ratchet, &nudge };
    for (auto* slider : stepSliders)
        slider->setBounds (stepArea.removeFromLeft (slotWidth).withTrimmedTop (26));

    area.removeFromLeft (14);

    // Right block: the p-lock slots.
    const int numLocks = static_cast<int> (pLocks.size());
    const int lockWidth = juce::jmax (52, area.getWidth() / juce::jmax (1, numLocks));

    for (auto& ui : pLocks)
    {
        auto slot = area.removeFromLeft (lockWidth);
        ui->lock.setBounds (slot.removeFromTop (22).reduced (2, 1));
        ui->value.setBounds (slot.withTrimmedTop (2));
    }
}

void PLockPanelComponent::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    auto area = getLocalBounds().reduced (10, 6);
    area.removeFromTop (24);

    const int stepControls = 5;
    const int slotWidth = 66;
    static const char* stepCaptions[stepControls] = { "NOTE", "VELOCITY", "GATE", "RATCHET", "NUDGE" };

    auto stepArea = area.removeFromLeft (stepControls * slotWidth);
    for (int i = 0; i < stepControls; ++i)
    {
        theme::drawCaption (g, stepArea.removeFromLeft (slotWidth).removeFromTop (14), stepCaptions[i]);
    }

    area.removeFromLeft (14);

    const int numLocks = static_cast<int> (pLocks.size());
    const int lockWidth = juce::jmax (52, area.getWidth() / juce::jmax (1, numLocks));
    juce::ignoreUnused (numLocks);

    // P-lock captions sit under the rotary values to keep the row compact.
    auto captionRow = getLocalBounds().reduced (10, 6).removeFromBottom (12);
    captionRow.removeFromLeft (stepControls * slotWidth);
    captionRow.removeFromLeft (14);
    for (auto& ui : pLocks)
    {
        auto slot = captionRow.removeFromLeft (lockWidth);
        theme::drawCaption (g, slot, plockParamName (ui->param));
    }
}

} // namespace opxy
