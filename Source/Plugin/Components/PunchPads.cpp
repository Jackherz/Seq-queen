#include "PunchPads.h"
#include "../Theme.h"

namespace opxy
{
namespace
{
/** Second macro knob, described per FX so the pad says what it does. */
juce::String macroCaption (int fxIndex, float a, float b)
{
    switch (static_cast<FxType> (fxIndex))
    {
        case FxType::TapeStop:
            return "STOP " + juce::String (0.05f + a * 1.45f, 2) + "S";
        case FxType::DelayThrow:
            return juce::String (30 + static_cast<int> (a * 1000.0f)) + "MS FB"
                   + juce::String (static_cast<int> (b * 90.0f));
        case FxType::FilterSweep:
            return "DEPTH " + juce::String (static_cast<int> (a * 100.0f)) + "%";
        case FxType::Stutter:
            return juce::String (static_cast<int> (20 + a * 480.0f)) + "MS "
                   + juce::String (0.5 + b * 1.5, 1) + "X";
        case FxType::ReverbBurst:
            return "DECAY " + juce::String (static_cast<int> (a * 100.0f)) + "%";
        case FxType::Bitcrush:
            return juce::String (static_cast<int> (16.0f - a * 13.0f)) + " BIT";
        case FxType::Count:
            break;
    }
    return {};
}
} // namespace

PunchPadComponent::PunchPadComponent (SeqQueenAudioProcessor& processor, int fxIndex)
    : proc (processor), index (fxIndex)
{
    title = fxTypeName (static_cast<FxType> (fxIndex));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (25);
}

void PunchPadComponent::timerCallback()
{
    // Cheap: only repaint when something visible actually changed.
    const bool engaged = proc.isPunchEngaged (index);
    const float amount = proc.getEngine().getEffectiveAmount (index);

    if (engaged != lastEngaged || std::abs (amount - lastAmount) > 0.01f)
    {
        lastEngaged = engaged;
        lastAmount = amount;
        repaint();
    }
}

void PunchPadComponent::mouseDown (const juce::MouseEvent&)
{
    proc.setPunchHeld (index, true);
    repaint();
}

void PunchPadComponent::mouseUp (const juce::MouseEvent&)
{
    proc.setPunchHeld (index, false);
    repaint();
}

void PunchPadComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = ui::fxAccent (index);
    const auto& settings = proc.getEngine().getFxSettings (index);

    const bool engaged = proc.isPunchEngaged (index);
    const bool held = proc.isPunchHeld (index);
    const float amount = proc.getEngine().getEffectiveAmount (index);

    // Body: sunken when idle, warm tint when punched.
    g.setColour (engaged ? colour.withAlpha (0.14f) : ui::panelRaised);
    g.fillRect (bounds.reduced (0.5f));

    // Engagement glow: two translucent rings, cheapest possible stand-in for
    // the reference sheets' neon bloom.
    if (engaged)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.drawRect (bounds.reduced (0.5f).expanded (2.0f), 2.0f);
        g.setColour (colour.withAlpha (0.09f));
        g.drawRect (bounds.reduced (0.5f).expanded (4.5f), 2.0f);
    }

    g.setColour (engaged ? colour : (held ? colour.withAlpha (0.7f) : ui::hairlineLit));
    g.drawRect (bounds.reduced (0.5f), engaged ? 1.4f : 1.0f);

    // Accent stripe along the top edge: the colour code for this FX.
    g.setColour (engaged ? colour : colour.withAlpha (0.5f));
    g.fillRect (bounds.getX() + 1.0f, bounds.getY() + 1.0f, bounds.getWidth() - 2.0f, 2.0f);

    auto area = getLocalBounds().reduced (9, 0);

    // Name
    ui::text (g, area.withTrimmedTop (7).withHeight (12),
              title.toUpperCase(), ui::monoFont (9.0f, true),
              engaged ? ui::bone : ui::boneDim);

    // Trigger hint: MIDI note + target bus
    const int note = proc.getFxNoteBase() + index;
    juce::String noteText = juce::MidiMessage::getMidiNoteName (note, true, true, 3)
                              + "  " + juce::String (note);
    ui::text (g, area.withTrimmedTop (20).withHeight (10), noteText,
              ui::monoFont (7.0f), ui::textFaint);

    const bool onFocusTrack = settings.target.load() == static_cast<int> (FxTarget::FocusTrack);
    juce::String targetText = "MSTR";
    if (onFocusTrack)
    {
        const int focus = proc.getFocusTrack();
        targetText = "TRK " + juce::String (focus + 1);
    }
    ui::text (g, area.withTrimmedTop (20).withHeight (10), targetText,
              ui::monoFont (7.0f), engaged ? colour : ui::textFaint,
              juce::Justification::centredRight);

    // Macro caption + amount meter along the bottom.
    ui::text (g, area.withTrimmedTop (31).withHeight (9),
              macroCaption (index, settings.paramA.load(), settings.paramB.load()),
              ui::monoFont (6.6f), ui::textFaint);

    const auto meter = juce::Rectangle<float> (bounds.getX() + 9.0f, bounds.getBottom() - 8.0f,
                                              bounds.getWidth() - 18.0f, 1.0f);
    g.setColour (ui::hairline);
    g.fillRect (meter);

    if (amount > 0.001f)
    {
        g.setColour (engaged ? colour : colour.withAlpha (0.6f));
        g.fillRect (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, amount)));

        // Engagement dot, top right.
        g.fillEllipse (bounds.getRight() - 10.0f, bounds.getY() + 7.0f, 3.5f, 3.5f);
    }
}

// -----------------------------------------------------------------------------

PunchBarComponent::PunchBarComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    for (int f = 0; f < kMaxFx; ++f)
    {
        auto pad = std::make_unique<PunchPadComponent> (processor, f);
        addAndMakeVisible (*pad);
        pads.push_back (std::move (pad));
    }
}

void PunchBarComponent::resized()
{
    auto area = getLocalBounds().withTrimmedTop (22).withTrimmedBottom (14);
    const int gap = 8;
    const int count = static_cast<int> (pads.size());
    if (count == 0)
        return;

    const int padWidth = juce::jmax (40, (area.getWidth() - gap * (count - 1)) / count);

    for (auto& pad : pads)
    {
        pad->setBounds (area.removeFromLeft (padWidth));
        area.removeFromLeft (gap);
    }
}

void PunchBarComponent::paint (juce::Graphics& g)
{
    ui::panelBox (g, getLocalBounds());
    ui::sectionHeader (g, getLocalBounds().reduced (14, 8), "02", "punch-in fx",
                       "hold to engage · velocity -> depth");

    // Bus rail: the patch-cable destination along the bottom of the section.
    const float railY = static_cast<float> (getBottom() - 8);
    g.setColour (ui::hairline);
    g.drawHorizontalLine (static_cast<int> (railY), 14.0f, static_cast<float> (getWidth() - 14));
    ui::caption (g, { 14, getBottom() - 7, 120, 9 }, "master bus", ui::textFaint);
    ui::text (g, { getWidth() - 134, getBottom() - 7, 120, 9 }, "FOCUS TRACK",
              ui::monoFont (6.6f), ui::textFaint, juce::Justification::centredRight);

    // Short coloured stubs from each pad into the rail: which bus gets punched.
    for (int f = 0; f < static_cast<int> (pads.size()); ++f)
    {
        const auto& pad = pads[static_cast<std::size_t> (f)];
        const auto& settings = proc.getEngine().getFxSettings (f);
        const bool onFocusTrack = settings.target.load() == static_cast<int> (FxTarget::FocusTrack);
        const bool engaged = proc.isPunchEngaged (f);

        const float x0 = static_cast<float> (pad->getBounds().getCentreX());
        const float y0 = static_cast<float> (pad->getBottom() + 1);
        const float x1 = onFocusTrack ? static_cast<float> (getWidth() - 30) : x0;

        g.setColour (engaged ? ui::fxAccent (f) : ui::fxAccent (f).withAlpha (0.22f));
        if (std::abs (x1 - x0) < 1.0f)
        {
            g.drawLine (x0, y0, x0, railY, engaged ? 1.4f : 1.0f);
        }
        else
        {
            juce::Path path;
            path.startNewSubPath (x0, y0);
            path.cubicTo (x0, railY - 2.0f, x1, y0 + 2.0f, x1, railY);
            g.strokePath (path, juce::PathStrokeType (engaged ? 1.4f : 1.0f));
        }

        if (engaged)
        {
            g.fillEllipse (x0 - 1.5f, y0 - 1.0f, 3.0f, 3.0f);
        }
    }
}

} // namespace opxy
