#include "PLockPanel.h"
#include "../Theme.h"

#include <cmath>

namespace opxy
{
namespace
{
constexpr int kStepControlWidth = 62;
constexpr int kDividerX = 366;

juce::String formatStepValue (int index, float value)
{
    switch (index)
    {
        case 0: return juce::String (static_cast<int> (std::lround (value)));          // note
        case 1: return juce::String (value, 2);                                        // velocity
        case 2: return value >= 1.0f ? juce::String (value, 2) + " st"
                                     : "1/" + juce::String (static_cast<int> (std::lround (1.0f / juce::jmax (0.02f, value))));
        case 3: return juce::String (static_cast<int> (std::lround (value)));
        case 4: return (value >= 0.0f ? "+" : "") + juce::String (value, 2);
        default: break;
    }
    return juce::String (value, 2);
}

juce::String formatPLock (PlockParam param, float value)
{
    switch (param)
    {
        case PlockParam::Pitch:     return (value >= 0.0f ? "+" : "") + juce::String (static_cast<int> (std::lround (value)));
        case PlockParam::Cutoff:    return value >= 1000.0f ? juce::String (value / 1000.0f, 1) + "K"
                                                            : juce::String (static_cast<int> (std::lround (value)));
        case PlockParam::Resonance:
        case PlockParam::Decay:
        case PlockParam::Velocity:  return juce::String (value, 2);
        case PlockParam::Pan:       return std::abs (value) < 0.02f ? juce::String ("C")
                                        : (value < 0.0f ? "L" : "R")
                                            + juce::String (static_cast<int> (std::lround (std::abs (value) * 100.0f)));
        case opxy::PlockParam::Count:     break;
    }
    return juce::String (value, 2);
}
} // namespace

PLockPanelComponent::PLockPanelComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    title.setFont (ui::monoFont (8.0f, true));
    title.setColour (juce::Label::textColourId, ui::textFaint);
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    // --- step values --------------------------------------------------------
    const auto setUpStepSlider = [this] (juce::Slider& slider, double min, double max,
                                         double interval, double skewMidPoint)
    {
        ui::styleRotary (slider);
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

    // Step values are bone, so they read as "the note itself"; p-locks are cyan.
    for (auto* slider : { &note, &velocity, &gate, &ratchet, &nudge })
        slider->setColour (juce::Slider::rotarySliderFillColourId, ui::boneDim);

    note.onValueChange     = [this] { applyStepValue (0, note.getValue()); };
    velocity.onValueChange = [this] { applyStepValue (1, velocity.getValue()); };
    gate.onValueChange     = [this] { applyStepValue (2, gate.getValue()); };
    ratchet.onValueChange  = [this] { applyStepValue (3, ratchet.getValue()); };
    nudge.onValueChange    = [this] { applyStepValue (4, nudge.getValue()); };

    // --- p-lock slots -------------------------------------------------------
    for (int i = 0; i < kNumPlockParams; ++i)
    {
        auto ui = std::make_unique<PLockUI>();
        ui->param = static_cast<PlockParam> (i);

        ui::styleRotary (ui->value);
        ui->value.setRange (0.0, 1.0, 0.001);
        ui->value.setColour (juce::Slider::rotarySliderFillColourId, ui::textFaint);

        ui::styleToggle (ui->lock);
        ui->lock.setClickingTogglesState (true);
        ui->lock.setButtonText ("LOCK");
        ui->lock.setColour (juce::TextButton::buttonOnColourId, ui::cyan);

        addAndMakeVisible (ui->value);
        addAndMakeVisible (ui->lock);
        pLocks.push_back (std::move (ui));
    }

    // Musical ranges per lock, and the busiest ones are pre-armed as examples.
    const auto setRange = [this] (PlockParam param, double min, double max,
                                  double interval, double skew, double defaultValue)
    {
        auto* slot = pLocks[static_cast<std::size_t> (param)].get();
        slot->value.setRange (min, max, interval);
        if (skew > 0.0)
            slot->value.setSkewFactorFromMidPoint (skew);
        slot->value.setDoubleClickReturnValue (true, defaultValue);
    };

    setRange (PlockParam::Pitch,     -24.0, 24.0,  1.0,  -1.0,  0.0);
    setRange (PlockParam::Cutoff,     40.0, 18000.0, 1.0, 1100.0, 3000.0);
    setRange (PlockParam::Resonance,   0.0, 0.95,  0.001, -1.0,  0.3);
    setRange (PlockParam::Decay,       0.02, 3.0,  0.001, 0.4,   0.3);
    setRange (PlockParam::Velocity,    0.0, 1.0,   0.001, -1.0,  0.8);
    setRange (PlockParam::Pan,        -1.0, 1.0,   0.001, -1.0,  0.0);

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
            {
                step->setPLock (raw->param, static_cast<float> (raw->value.getValue()));
                repaint();
            }
        };
    }

    // --- utility buttons ----------------------------------------------------
    for (auto* button : { &clearPLocks, &clearStep })
    {
        ui::styleToggle (*button);
        button->setColour (juce::TextButton::buttonOnColourId, ui::red);
        button->setColour (juce::TextButton::textColourOffId, ui::textFaint);
        addAndMakeVisible (*button);
    }

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
        case opxy::PlockParam::Pitch:     return 0.0f;
        case PlockParam::Cutoff:    return track->cutoff.load();
        case PlockParam::Resonance: return track->resonance.load();
        case PlockParam::Decay:     return track->decay.load();
        case PlockParam::Velocity:  return currentStep() != nullptr ? currentStep()->velocity.load() : 0.8f;
        case PlockParam::Pan:       return track->pan.load();
        case opxy::PlockParam::Count:     break;
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
    repaint();
}

void PLockPanelComponent::setSelectedStep (int track, int step)
{
    selectedTrack = juce::jlimit (0, kMaxTracks - 1, track);
    selectedStep = juce::jlimit (0, kMaxSteps - 1, step);
    refresh();
}

void PLockPanelComponent::refresh()
{
    auto header = "STEP " + juce::String (selectedStep + 1).paddedLeft ('0', 2)
                + " / TRACK " + juce::String (selectedTrack + 1);
    title.setText (header, juce::dontSendNotification);

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
        ui->value.setValue (value, juce::dontSendNotification);

        // Locked slots glow cyan; unarmed ones stay neutral so a step's state
        // reads at a glance from across the room.
        ui->value.setColour (juce::Slider::rotarySliderFillColourId,
                             locked ? ui::cyan : ui::textFaint);
    }

    repaint();
}

void PLockPanelComponent::resized()
{
    auto area = getLocalBounds().reduced (14, 8);
    area.removeFromTop (24); // section header

    auto headerRow = getLocalBounds().reduced (14, 8).removeFromTop (16);
    headerRow.removeFromLeft (kDividerX - 14 - 8);
    clearStep.setBounds (headerRow.removeFromRight (86));
    headerRow.removeFromRight (6);
    clearPLocks.setBounds (headerRow.removeFromRight (108));

    // Left block: the step's own values.
    for (auto* slider : { &note, &velocity, &gate, &ratchet, &nudge })
        slider->setBounds (area.removeFromLeft (kStepControlWidth).withTrimmedTop (18).withHeight (46));

    area.removeFromLeft (14);

    // Right block: the six p-lock slots.
    const int lockWidth = juce::jmax (48, area.getWidth() / kNumPlockParams);
    for (auto& ui : pLocks)
    {
        auto slot = area.removeFromLeft (lockWidth);
        ui->lock.setBounds (slot.removeFromTop (14).reduced (4, 0));
        ui->value.setBounds (slot.withTrimmedTop (2).withHeight (48));
    }
}

void PLockPanelComponent::paint (juce::Graphics& g)
{
    ui::panelBox (g, getLocalBounds());

    const auto headerArea = getLocalBounds().reduced (14, 8).removeFromTop (14);
    ui::caption (g, headerArea, juce::CharPointer_UTF8 ("03  ·  step edit"), ui::boneDim);
    title.setBounds (headerArea.withTrimmedLeft (140));

    g.setColour (ui::hairline);
    g.drawHorizontalLine (headerArea.getBottom() + 2, 14.0f, static_cast<float> (getWidth() - 14));

    // Divider between step values and p-locks.
    g.drawVerticalLine (kDividerX, static_cast<float> (headerArea.getBottom() + 8),
                        static_cast<float> (getHeight() - 16));

    // Captions under the step rotaries.
    static const char* stepCaptions[] = { "note", "velocity", "gate", "ratchet", "nudge" };
    const int captionY = getHeight() - 26;
    for (int i = 0; i < 5; ++i)
        ui::caption (g, { 14 + i * kStepControlWidth, captionY, kStepControlWidth, 10 },
                     stepCaptions[i], ui::textFaint);

    // Values under each rotary, so the panel is readable without hovering.
    const Step* step = currentStep();
    if (step != nullptr)
    {
        const float values[] = { step->note.load(), step->velocity.load(), step->gate.load(),
                                 static_cast<float> (step->ratchet.load()), step->nudge.load() };
        for (int i = 0; i < 5; ++i)
            ui::text (g, { 14 + i * kStepControlWidth, captionY - 12, kStepControlWidth, 10 },
                      formatStepValue (i, values[i]), ui::monoFont (8.0f), ui::bone,
                      juce::Justification::centred);
    }

    // P-lock captions + values, cyan when armed.
    const int lockWidth = juce::jmax (48, (getWidth() - 14 - kDividerX - 14) / kNumPlockParams);
    for (auto& ui : pLocks)
    {
        const int index = static_cast<int> (ui->param);
        const int x = kDividerX + 14 + index * lockWidth;
        const bool locked = step != nullptr && step->hasPLock (ui->param);
        const float value = locked ? step->getPLock (ui->param, 0.0f) : defaultValueFor (ui->param);

        ui::text (g, { x, captionY - 12, lockWidth, 10 }, formatPLock (ui->param, value),
                  ui::monoFont (8.0f), locked ? ui::cyan : ui::boneDim,
                  juce::Justification::centred);
        const juce::String captionText (plockParamName (ui->param));
        ui::caption (g, { x, captionY, lockWidth, 10 }, captionText,
                     locked ? ui::cyan : ui::textFaint);
    }
}

} // namespace opxy
