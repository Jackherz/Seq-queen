#pragma once

// -----------------------------------------------------------------------------
//  The plugin's LookAndFeel.
//
//  Only a handful of JUCE widgets are on screen (rotaries, combos, text
//  buttons), and all of them are drawn as thin-stroke line art so the editor
//  reads as one instrument rather than "JUCE controls in a dark room".
//
//  Design rules enforced here:
//    * 1px hairlines, no gradients, no shadows except a faint engagement glow
//    * values are mono, captions are wide-tracked uppercase mono
//    * on = bone fill with void text; off = outlined, no fill
//    * tick-ring rotaries with a bone pointer and a coloured value arc
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "Theme.h"

namespace opxy
{

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    ~LookAndFeel() override = default;

    // --- rotaries -----------------------------------------------------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    // --- linear sliders (mixer strips) --------------------------------------
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // --- buttons ------------------------------------------------------------
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // --- combos -------------------------------------------------------------
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;

    // --- misc ---------------------------------------------------------------
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    juce::Font getLabelFont (juce::Label&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeel)
};

} // namespace opxy
