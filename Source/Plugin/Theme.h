#pragma once

// -----------------------------------------------------------------------------
//  Design tokens and shared drawing helpers.
//
//  The look: OP-1/OP-XY HUD line art (thin strokes, tiny tracked uppercase
//  labels, dot matrices, neon readouts on black) crossed with a charcoal and
//  bone technical panel (hairline divisions, corner ticks, tick-ring dials,
//  serial-number strip). Four saturated accents act as the patch-cable colours:
//
//      amber   transport / playhead / primary action
//      cyan    p-locks and metadata
//      green   euclidean / "on" brains
//      violet  delay-line family
//      red     destructive (clear, mute)
//
//  Everything here is header-only and cheap: the editor hand-draws its chrome
//  rather than using images, so the whole skin is searchable source.
// -----------------------------------------------------------------------------

#include <JuceHeader.h>

namespace opxy
{
namespace ui
{

// ------------------------------------------------------------------ palette --
inline const juce::Colour voidColour   { 0xff0b0c0e };
inline const juce::Colour panel        { 0xff14161a };
inline const juce::Colour panelRaised  { 0xff1b1e23 };
inline const juce::Colour panelSunken  { 0xff101216 };
inline const juce::Colour hairline     { 0xff2a2e35 };
inline const juce::Colour hairlineLit  { 0xff3a4049 };
inline const juce::Colour bone         { 0xffede7d9 };
inline const juce::Colour boneDim      { 0xff969891 };
inline const juce::Colour textDim      { 0xff7c8288 };
inline const juce::Colour textFaint    { 0xff4a5058 };

inline const juce::Colour amber        { 0xfff07a3c };
inline const juce::Colour cyan         { 0xff4cc9d8 };
inline const juce::Colour green        { 0xff6bd08c };
inline const juce::Colour violet       { 0xff9a8ce8 };
inline const juce::Colour red          { 0xffe0503c };

/** Accent colour per punch-in FX, used on pads, stubs and lane rows. */
inline juce::Colour fxAccent (int fxIndex)
{
    switch (fxIndex)
    {
        case 0: return amber;    // tape stop
        case 1: return violet;   // delay throw
        case 2: return cyan;     // filter
        case 3: return green;    // stutter
        case 4: return bone;     // reverb
        case 5: return red;      // bitcrush
        default: break;
    }
    return amber;
}

// -------------------------------------------------------------------- fonts --
inline juce::Font monoFont (float height, bool bold = false)
{
    juce::Font font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                        bold ? juce::Font::bold : juce::Font::plain));
    return font.withExtraKerningFactor (0.06f);
}

inline juce::Font sansFont (float height, bool bold = false)
{
    juce::Font font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height,
                                        bold ? juce::Font::bold : juce::Font::plain));
    return font.withExtraKerningFactor (0.05f);
}

/** The tiny wide-tracked uppercase captions all over the reference sheets. */
inline juce::Font captionFont (float height = 8.5f)
{
    return monoFont (height, true).withExtraKerningFactor (0.22f);
}

// ----------------------------------------------------------------- drawing ---
inline void text (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& string,
                  const juce::Font& font, juce::Colour colour,
                  juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (colour);
    g.setFont (font);
    g.drawText (string, area, justification, false);
}

inline void caption (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& string,
                     juce::Colour colour = textDim)
{
    text (g, area, string.toUpperCase(), captionFont(), colour);
}

/** Panel body: sunken fill plus a hairline, with optional corner ticks. */
inline void panelBox (juce::Graphics& g, juce::Rectangle<int> area,
                      juce::Colour fill = ui::panel, bool ticks = true)
{
    auto bounds = area.toFloat();
    g.setColour (fill);
    g.fillRect (bounds);
    g.setColour (hairline);
    g.drawRect (bounds, 1.0f);

    if (ticks)
    {
        const float length = 5.0f;
        g.setColour (hairlineLit);
        for (const auto& corner : { juce::Point<float> (bounds.getX(), bounds.getY()),
                                    { bounds.getRight(), bounds.getY() },
                                    { bounds.getX(), bounds.getBottom() },
                                    { bounds.getRight(), bounds.getBottom() } })
        {
            const float dx = (corner.x < bounds.getCentreX()) ? 1.0f : -1.0f;
            const float dy = (corner.y < bounds.getCentreY()) ? 1.0f : -1.0f;
            g.drawLine (corner.x, corner.y, corner.x + dx * length, corner.y, 1.0f);
            g.drawLine (corner.x, corner.y, corner.x, corner.y + dy * length, 1.0f);
        }
    }
}

/** Numbered section header separating the panels ("01 · PATTERN"). */
inline void sectionHeader (juce::Graphics& g, juce::Rectangle<int> area,
                           const juce::String& number, const juce::String& title,
                           const juce::String& rightText = {})
{
    auto row = area.removeFromTop (14);
    caption (g, row, number + "  ·  " + title, boneDim);

    g.setColour (hairline);
    g.drawHorizontalLine (area.getY() + 14, static_cast<float> (area.getX()),
                          static_cast<float> (area.getRight()));

    if (rightText.isNotEmpty())
        text (g, row, rightText.toUpperCase(), monoFont (7.5f), textFaint,
              juce::Justification::centredRight);
}

/** Fine dot matrix, used as texture in gutters and empty lanes. */
inline void dotGrid (juce::Graphics& g, juce::Rectangle<int> area,
                     float spacing = 6.0f, float radius = 0.6f,
                     juce::Colour colour = juce::Colour (0xff1e2229))
{
    g.setColour (colour);
    for (float y = static_cast<float> (area.getY()); y < area.getBottom(); y += spacing)
        for (float x = static_cast<float> (area.getX()); x < area.getRight(); x += spacing)
            g.fillEllipse (x, y, radius * 2.0f, radius * 2.0f);
}

/** A dial's tick ring, drawn by the LookAndFeel's rotary and by custom paint. */
inline void tickRing (juce::Graphics& g, juce::Point<float> centre, float radius,
                      float startAngle, float endAngle, int ticks,
                      juce::Colour colour = hairlineLit, float tickLength = 4.0f)
{
    g.setColour (colour);
    for (int i = 0; i <= ticks; ++i)
    {
        const float angle = startAngle + (endAngle - startAngle) * static_cast<float> (i)
                                           / static_cast<float> (ticks);
        const float inner = radius + 2.5f;
        const float outer = inner + ((i % 4 == 0) ? tickLength : tickLength * 0.65f);
        g.drawLine (centre.x + std::cos (angle) * inner, centre.y + std::sin (angle) * inner,
                    centre.x + std::cos (angle) * outer, centre.y + std::sin (angle) * outer,
                    (i % 4 == 0) ? 1.1f : 0.7f);
    }
}

/** Styled sliders/combos/buttons: applies the colours the LookAndFeel uses. */
inline void styleRotary (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, amber);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, hairlineLit);
    slider.setColour (juce::Slider::thumbColourId, bone);
}

inline void styleLinear (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, amber);
    slider.setColour (juce::Slider::backgroundColourId, hairline);
}

inline void styleCombo (juce::ComboBox& combo)
{
    combo.setColour (juce::ComboBox::backgroundColourId, panelRaised);
    combo.setColour (juce::ComboBox::textColourId, bone);
    combo.setColour (juce::ComboBox::outlineColourId, hairlineLit);
    combo.setColour (juce::ComboBox::arrowColourId, textFaint);
    combo.setColour (juce::ComboBox::focusedOutlineColourId, amber);
    combo.setJustificationType (juce::Justification::centredLeft);
}

inline void styleToggle (juce::Button& button)
{
    button.setColour (juce::TextButton::buttonColourId, panelRaised);
    button.setColour (juce::TextButton::buttonOnColourId, amber);
    button.setColour (juce::TextButton::textColourOffId, textDim);
    button.setColour (juce::TextButton::textColourOnId, voidColour);
}

} // namespace ui
} // namespace opxy
