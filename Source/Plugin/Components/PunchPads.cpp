#include "PunchPads.h"
#include "../Theme.h"

namespace opxy
{

PunchPadComponent::PunchPadComponent (SeqQueenAudioProcessor& processor, int fxIndex)
    : proc (processor), index (fxIndex)
{
    title = fxTypeName (static_cast<FxType> (fxIndex));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (25);
}

void PunchPadComponent::timerCallback()
{
    repaint();
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
    const bool engaged = proc.isPunchEngaged (index);
    const bool held = proc.isPunchHeld (index);

    auto face = engaged ? theme::accent
                        : (held ? theme::accentDim : theme::panelLight);
    if (engaged)
        face = face.brighter (0.06f);

    g.setColour (face);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (engaged ? theme::background.withAlpha (0.85f) : theme::text);
    g.setFont (theme::labelFont (12.0f));
    g.drawFittedText (juce::String (title).toUpperCase(),
                      getLocalBounds().reduced (6, 4).withTrimmedBottom (22),
                      juce::Justification::centred, 2);

    // The MIDI note that punches this FX, so the mapping is discoverable without
    // a manual. Shown as a note name plus its number (C1 = 36, and some DAWs
    // number octaves differently, so the number disambiguates).
    const auto& settings = proc.getEngine().getFxSettings (index);
    const int note = proc.getFxNoteBase() + index;
    const bool fxNotesOn = [this]
    {
        if (auto* value = proc.getAPVTS().getRawParameterValue ("fx_notes"))
            return value->load() > 0.5f;
        return true;
    }();

    g.setColour (engaged ? theme::background.withAlpha (0.75f)
                         : (fxNotesOn ? theme::textDim : theme::textDim.withAlpha (0.45f)));
    g.setFont (theme::monoFont (9.0f));
    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3)
                  + " (" + juce::String (note) + ")",
                getLocalBounds().reduced (6, 3).withTrimmedBottom (10),
                juce::Justification::bottomLeft, false);

    // Which bus is being punched: master or the focused track.
    const int target = settings.target.load();
    juce::String targetText = "MSTR";

    if (target == static_cast<int> (FxTarget::FocusTrack))
    {
        // The focused track index is a parameter; read it back from APVTS.
        if (auto* value = proc.getAPVTS().getRawParameterValue ("focus_track"))
            targetText = "TRK " + juce::String (static_cast<int> (std::lround (value->load())) + 1);
    }

    g.setColour (engaged ? theme::background.withAlpha (0.7f) : theme::textDim);
    g.setFont (theme::monoFont (9.0f));
    g.drawText (targetText, getLocalBounds().reduced (6, 3), juce::Justification::bottomRight, false);

    // Amount meter along the bottom edge: shows the *effective* wet amount, so a
    // soft MIDI hit visibly meters lower than a hard one.
    const float amount = proc.getEngine().getEffectiveAmount (index);
    if (amount > 0.001f)
    {
        g.setColour (engaged ? theme::background.withAlpha (0.5f) : theme::accent.withAlpha (0.65f));
        const float width = bounds.getWidth() * juce::jlimit (0.0f, 1.0f, amount);
        g.fillRect (bounds.getX() + 6.0f, bounds.getBottom() - 5.0f,
                    juce::jmax (0.0f, width - 12.0f), 2.0f);
    }
}

// -----------------------------------------------------------------------------

PunchBarComponent::PunchBarComponent (SeqQueenAudioProcessor& processor)
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
    auto area = getLocalBounds().withTrimmedTop (14);
    const int gap = 6;
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
    theme::drawCaption (g, getLocalBounds().removeFromTop (14), "Punch-in FX  (hold to engage)");
}

} // namespace opxy
