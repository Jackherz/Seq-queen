#pragma once

// -----------------------------------------------------------------------------
//  Small UI pieces shared by the two devices (Seq Queen FX, Seq Queen Notes).
//
//  The rule for both editors: every control says what it does. A pad shows its
//  name, the key that triggers it and one line of plain description; a knob
//  shows its name and current value underneath.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "Theme.h"

#include <functional>

namespace opxy
{
namespace ui
{

/** Live's note names: middle C (60) is C3, note 0 is C-2. */
inline juce::String liveNoteName (int note)
{
    return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, note), true, true, 3);
}

/** A momentary pad: press and hold to punch. Lights while its effect is active
    from any source (pad, trigger note, automation). */
class PunchPad : public juce::Component, private juce::Timer
{
public:
    PunchPad (juce::String padName, juce::String padDescription, juce::Colour padAccent)
        : name (std::move (padName)), description (std::move (padDescription)), accent (padAccent)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    std::function<void (bool)> onHold;   // pad pressed / released
    std::function<bool()> isActive;      // lit state, from the processor
    std::function<float()> getLevel;     // optional 0..1 meter (e.g. effective depth)

    void setKeyLabel (const juce::String& label)
    {
        if (label != keyLabel)
        {
            keyLabel = label;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const bool active = lastActive;

        g.setColour (active ? accent.withAlpha (0.18f) : panelRaised);
        g.fillRect (bounds);
        g.setColour (active ? accent : hairlineLit);
        g.drawRect (bounds, 1.0f);
        g.setColour (accent.withAlpha (active ? 1.0f : 0.7f));
        g.fillRect (bounds.withHeight (3.0f));

        auto area = getLocalBounds().reduced (10, 8);
        area.removeFromTop (3);

        auto top = area.removeFromTop (16);
        text (g, top, keyLabel, monoFont (9.0f), active ? accent : textDim, juce::Justification::centredRight);
        text (g, top, name.toUpperCase(), monoFont (10.5f, true), active ? bone : boneDim);

        area.removeFromTop (4);
        g.setColour (active ? bone.withAlpha (0.85f) : textDim);
        g.setFont (monoFont (8.5f));
        g.drawFittedText (description, area.removeFromTop (24), juce::Justification::topLeft, 2, 1.0f);

        if (getLevel != nullptr)
        {
            const auto meter = getLocalBounds().reduced (10, 8).removeFromBottom (3).toFloat();
            g.setColour (hairline);
            g.fillRect (meter);
            g.setColour (accent);
            g.fillRect (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, lastLevel)));
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onHold != nullptr)
            onHold (true);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onHold != nullptr)
            onHold (false);
    }

private:
    void timerCallback() override
    {
        const bool active = isActive != nullptr && isActive();
        const float level = getLevel != nullptr ? getLevel() : 0.0f;
        if (active != lastActive || std::abs (level - lastLevel) > 0.01f)
        {
            lastActive = active;
            lastLevel = level;
            repaint();
        }
    }

    juce::String name, description, keyLabel;
    juce::Colour accent;
    bool lastActive = false;
    float lastLevel = 0.0f;
};

/** Rotary knob with its name and live value written underneath. */
class LabelledKnob : public juce::Component
{
public:
    explicit LabelledKnob (juce::String knobName, juce::Colour accentColour = amber)
        : name (std::move (knobName))
    {
        styleRotary (slider);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    juce::Slider slider;

    /** Formats the value line; defaults to the parameter's own text. */
    std::function<juce::String (double)> formatValue;

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromBottom (24);
        slider.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                                      area.getHeight()));
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().removeFromBottom (24);
        const auto value = formatValue != nullptr ? formatValue (slider.getValue())
                                                  : slider.getTextFromValue (slider.getValue());
        text (g, area.removeFromTop (12), value, monoFont (8.5f), bone, juce::Justification::centred);
        text (g, area, name.toUpperCase(), captionFont (7.5f), textDim, juce::Justification::centred);
    }

private:
    juce::String name;
};

/** Small heading + one plain sentence, top-left of each device. */
inline void deviceHeader (juce::Graphics& g, juce::Rectangle<int> area,
                          const juce::String& title, const juce::String& subtitle)
{
    g.setColour (amber);
    g.fillRect (area.getX(), area.getY() + 4, 8, 18);
    auto row = area.withTrimmedLeft (18);
    g.setColour (bone);
    g.setFont (sansFont (18.0f, true).withExtraKerningFactor (0.16f));
    g.drawText (title, row.removeFromTop (26), juce::Justification::centredLeft, false);
    text (g, row.removeFromTop (14), subtitle, monoFont (9.0f), textDim);
}

} // namespace ui
} // namespace opxy
