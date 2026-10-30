#include "SeqQueenLookAndFeel.h"

namespace opxy
{

LookAndFeel::LookAndFeel()
{
    // Base colours: anything JUCE draws that we do not override still lands on
    // the right side of the palette.
    setColour (juce::ResizableWindow::backgroundColourId, ui::voidColour);
    setColour (juce::PopupMenu::backgroundColourId, ui::panelRaised);
    setColour (juce::PopupMenu::textColourId, ui::bone);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::voidColour);
    setColour (juce::PopupMenu::headerTextColourId, ui::textDim);
    setColour (juce::Label::textColourId, ui::bone);
    setColour (juce::TextEditor::backgroundColourId, ui::panelSunken);
    setColour (juce::TextEditor::textColourId, ui::bone);
    setColour (juce::TextEditor::outlineColourId, ui::hairlineLit);
    setColour (juce::TextEditor::focusedOutlineColourId, ui::amber);
    setColour (juce::TooltipWindow::backgroundColourId, ui::panelRaised);
    setColour (juce::TooltipWindow::textColourId, ui::bone);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPos, float rotaryStartAngle,
                                    float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const float diameter = static_cast<float> (juce::jmin (width, height)) - 8.0f;
    const float radius = diameter * 0.5f;
    const juce::Point<float> centre (static_cast<float> (x) + static_cast<float> (width) * 0.5f,
                                     static_cast<float> (y) + static_cast<float> (height) * 0.5f);

    // Tick ring (the poster's panel dials).
    ui::tickRing (g, centre, radius - 1.0f, rotaryStartAngle, rotaryEndAngle, 20,
                  ui::hairlineLit, 3.5f);

    // The dial body is an outline, not a filled knob.
    g.setColour (ui::hairlineLit);
    g.drawEllipse (centre.x - radius + 3.0f, centre.y - radius + 3.0f,
                   (radius - 3.0f) * 2.0f, (radius - 3.0f) * 2.0f, 1.0f);

    // Value arc.
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 4.5f, radius - 4.5f, 0.0f,
                       rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer + centre dot.
    const juce::Point<float> tip (centre.x + std::cos (angle) * (radius - 6.0f),
                                  centre.y + std::sin (angle) * (radius - 6.0f));
    g.setColour (ui::bone);
    g.drawLine (centre.x, centre.y, tip.x, tip.y, 1.3f);
    g.fillEllipse (centre.x - 1.5f, centre.y - 1.5f, 3.0f, 3.0f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPos, float minSliderPos, float maxSliderPos,
                                    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    const auto accent = slider.findColour (juce::Slider::trackColourId);
    const auto trackColour = slider.findColour (juce::Slider::backgroundColourId);
    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        g.setColour (trackColour);
        g.drawLine (static_cast<float> (x), centreY,
                    static_cast<float> (x + width), centreY, 1.0f);

        g.setColour (accent);
        g.drawLine (static_cast<float> (x), centreY, sliderPos, centreY, 2.0f);

        // A small bone tick for the handle keeps it minimal.
        g.setColour (ui::bone);
        g.drawLine (sliderPos, centreY - 4.0f, sliderPos, centreY + 4.0f, 1.4f);
        return;
    }

    // Vertical bars (unused today, styled for completeness).
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    g.setColour (trackColour);
    g.drawLine (centreX, static_cast<float> (y), centreX, static_cast<float> (y + height), 1.0f);
    g.setColour (accent);
    g.drawLine (centreX, sliderPos, centreX, static_cast<float> (y + height), 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                        const juce::Colour& backgroundColour,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const auto accent = backgroundColour;

    // Lit buttons are solid bone/accent; unlit ones are outlines only.
    if (on)
    {
        g.setColour (accent);
        g.fillRect (bounds);
        g.setColour (accent.brighter (0.25f));
        g.drawRect (bounds, 1.0f);
    }
    else
    {
        g.setColour (ui::panelRaised);
        g.fillRect (bounds);

        g.setColour (shouldDrawButtonAsDown ? ui::boneDim
                     : shouldDrawButtonAsHighlighted ? ui::hairlineLit.brighter (0.5f)
                                                     : ui::hairlineLit);
        g.drawRect (bounds, 1.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                  bool, bool)
{
    const bool on = button.getToggleState();
    g.setColour (on ? juce::Colour (ui::voidColour) : ui::textDim);
    g.setFont (ui::captionFont (9.0f));
    g.drawFittedText (button.getButtonText().toUpperCase(), button.getLocalBounds().reduced (6, 0),
                      juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                    bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setColour (on ? ui::bone : ui::panelRaised);
    g.fillRect (bounds);
    g.setColour (on ? ui::bone : (highlighted || down ? ui::hairlineLit.brighter (0.5f)
                                                      : ui::hairlineLit));
    g.drawRect (bounds, 1.0f);

    g.setColour (on ? ui::voidColour : ui::textDim);
    g.setFont (ui::captionFont (9.0f));
    g.drawFittedText (button.getButtonText().toUpperCase(), button.getLocalBounds().reduced (6, 0),
                      juce::Justification::centred, 1);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return ui::captionFont (juce::jlimit (8.0f, 10.0f, static_cast<float> (buttonHeight) * 0.42f));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                int, int, int, int, juce::ComboBox& box)
{
    const juce::Rectangle<float> bounds (0.5f, 0.5f, static_cast<float> (width) - 1.0f,
                                        static_cast<float> (height) - 1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (bounds);

    g.setColour (isButtonDown ? ui::amber : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRect (bounds, 1.0f);

    // Chevron on the right.
    const float cx = static_cast<float> (width) - 11.0f;
    const float cy = static_cast<float> (height) * 0.5f;
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (0.9f));
    g.drawLine (cx - 3.0f, cy - 1.5f, cx, cy + 1.8f, 1.0f);
    g.drawLine (cx, cy + 1.8f, cx + 3.0f, cy - 1.5f, 1.0f);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return ui::monoFont (juce::jlimit (8.0f, 11.0f, static_cast<float> (box.getHeight()) * 0.46f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, ui::bone);
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return ui::monoFont (11.0f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (ui::panelRaised);
    g.setColour (ui::hairlineLit);
    g.drawRect (0, 0, width, height, 1);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     bool isSeparator, bool isActive, bool isHighlighted,
                                     bool isTicked, bool hasSubMenu, const juce::String& text,
                                     const juce::String& shortcutKeyText,
                                     const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (ui::hairline);
        g.drawHorizontalLine (area.getCentreY(), static_cast<float> (area.getX()) + 6.0f,
                              static_cast<float> (area.getRight()) - 6.0f);
        return;
    }

    if (isHighlighted)
    {
        g.setColour (ui::amber.withAlpha (0.16f));
        g.fillRect (area);
        g.setColour (ui::amber);
        g.fillRect (area.getX(), area.getY(), 2, area.getHeight());
    }

    auto row = area.reduced (10, 0);
    const auto colour = isHighlighted ? ui::bone
                                      : (isActive ? ui::boneDim : ui::textFaint);
    juce::ignoreUnused (textColour, icon, hasSubMenu);

    ui::text (g, row, text.toUpperCase(), ui::monoFont (10.5f), colour);

    if (isTicked)
    {
        g.setColour (ui::amber);
        g.fillEllipse (static_cast<float> (area.getRight()) - 12.0f,
                       static_cast<float> (area.getCentreY()) - 2.0f, 4.0f, 4.0f);
    }

    if (shortcutKeyText.isNotEmpty())
        ui::text (g, row, shortcutKeyText, ui::monoFont (9.0f), ui::textFaint,
                  juce::Justification::centredRight);
}

juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

} // namespace opxy
