#pragma once

// Visual language: dark panel, warm accent, one accent colour per role.
// Deliberately flat and hand-drawn (no images, no LookAndFeel subclass) so the
// editor stays easy to change without fighting a skin.

#include <JuceHeader.h>

namespace opxy
{
namespace theme
{

inline const juce::Colour background   { 0xff101218 };
inline const juce::Colour panel        { 0xff1a1d27 };
inline const juce::Colour panelLight   { 0xff232734 };
inline const juce::Colour gridLine     { 0xff2c3040 };
inline const juce::Colour stepOff      { 0xff2e3242 };
inline const juce::Colour stepOn       { 0xffefece2 };
inline const juce::Colour accent       { 0xffff6a1f };  // punch / playhead
inline const juce::Colour accentDim    { 0xff8a3a10 };
inline const juce::Colour accentAlt    { 0xff31d5c4 };  // p-locks / metadata
inline const juce::Colour text         { 0xffe9e9f0 };
inline const juce::Colour textDim      { 0xff8b90a2 };
inline const juce::Colour danger       { 0xffe0483e };

inline juce::Font labelFont (float height = 11.0f) { return juce::Font (height, juce::Font::bold); }
inline juce::Font bigFont (float height = 18.0f)   { return juce::Font (height, juce::Font::bold); }
inline juce::Font monoFont (float height = 12.0f)  { return juce::Font (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain); }

inline void styleRotary (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, gridLine);
    slider.setColour (juce::Slider::thumbColourId, text);
}

inline void styleLinear (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setColour (juce::Slider::backgroundColourId, gridLine);
}

inline void styleCombo (juce::ComboBox& combo)
{
    combo.setColour (juce::ComboBox::backgroundColourId, panelLight);
    combo.setColour (juce::ComboBox::textColourId, text);
    combo.setColour (juce::ComboBox::outlineColourId, gridLine);
    combo.setColour (juce::ComboBox::arrowColourId, textDim);
}

inline void styleToggle (juce::Button& button)
{
    button.setColour (juce::TextButton::buttonColourId, panelLight);
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setColour (juce::TextButton::textColourOffId, textDim);
    button.setColour (juce::TextButton::textColourOnId, background);
}

/** Draws a small uppercase caption. */
inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> area,
                         const juce::String& caption, juce::Colour colour = textDim)
{
    g.setColour (colour);
    g.setFont (labelFont (10.0f));
    g.drawText (caption.toUpperCase(), area, juce::Justification::centredLeft, false);
}

} // namespace theme
} // namespace opxy
