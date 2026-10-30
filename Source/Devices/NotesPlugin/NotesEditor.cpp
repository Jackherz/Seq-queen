#include "NotesEditor.h"

namespace
{
using opxy::NoteFxType;

constexpr int kWidth = 900;
constexpr int kHeight = 534;
constexpr int kMargin = 16;
constexpr int kHeaderHeight = 64;
constexpr int kRowLabelHeight = 18;
constexpr int kPadHeight = 78;
constexpr int kSettingsHeight = 92;
constexpr int kFooterHeight = 26;
constexpr int kPadsPerRow = 6;
constexpr int kNumRows = 3;

/** Colour by what the punch does: adds notes, removes them, moves pitch, or
    reshapes timing. */
juce::Colour accentFor (NoteFxType type)
{
    switch (type)
    {
        case NoteFxType::Mute:
            return opxy::ui::red;
        case NoteFxType::Repeat:
        case NoteFxType::Fill:
        case NoteFxType::HalfFill:
        case NoteFxType::KickSnare:
        case NoteFxType::Hats:
            return opxy::ui::green;
        case NoteFxType::OctaveUp:
        case NoteFxType::OctaveDown:
        case NoteFxType::Random:
            return opxy::ui::cyan;
        case NoteFxType::Arp:
        case NoteFxType::Chord:
        case NoteFxType::Strum:
        case NoteFxType::Groove:
            return opxy::ui::amber;
        case NoteFxType::Shorten:
        case NoteFxType::Skip:
        case NoteFxType::Echo:
        case NoteFxType::Count:
            break;
    }
    return opxy::ui::violet;
}

juce::String noteValue (double v)
{
    return opxy::ui::liveNoteName (juce::roundToInt (v));
}
} // namespace

SeqQueenNotesEditor::SeqQueenNotesEditor (SeqQueenNotesProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lookAndFeel);

    for (int f = 0; f < opxy::kNumNoteFx; ++f)
    {
        const auto type = static_cast<NoteFxType> (f);
        pads[f] = std::make_unique<opxy::ui::PunchPad> (opxy::noteFxName (type),
                                                        opxy::noteFxDescription (type),
                                                        accentFor (type));
        pads[f]->onHold   = [this, f] (bool held) { proc.setPadHeld (f, held); };
        pads[f]->isActive = [this, f] { return proc.isPunching (f); };
        addAndMakeVisible (*pads[f]);
    }

    triggerKnob.formatValue = [] (double v)
    {
        const int base = juce::roundToInt (v);
        return opxy::ui::liveNoteName (base) + " - " + opxy::ui::liveNoteName (base + opxy::kNumNoteFx - 1);
    };
    for (auto* knob : { &kickKnob, &snareKnob, &hatKnob })
        knob->formatValue = noteValue;

    swingKnob.formatValue = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };

    for (auto* knob : { &triggerKnob, &kickKnob, &snareKnob, &hatKnob, &swingKnob })
        addAndMakeVisible (*knob);

    triggerAttachment = std::make_unique<SliderAttachment> (proc.state, "trigger_note", triggerKnob.slider);
    kickAttachment    = std::make_unique<SliderAttachment> (proc.state, "kick_note", kickKnob.slider);
    snareAttachment   = std::make_unique<SliderAttachment> (proc.state, "snare_note", snareKnob.slider);
    hatAttachment     = std::make_unique<SliderAttachment> (proc.state, "hat_note", hatKnob.slider);
    swingAttachment   = std::make_unique<SliderAttachment> (proc.state, "groove_swing", swingKnob.slider);

    opxy::ui::styleCombo (repeatRate);
    repeatRate.addItemList ({ "1/8", "1/16", "1/32" }, 1);
    addAndMakeVisible (repeatRate);
    repeatAttachment = std::make_unique<ComboAttachment> (proc.state, "repeat_rate", repeatRate);

    setSize (kWidth, kHeight);
    startTimerHz (10);
    timerCallback();
}

SeqQueenNotesEditor::~SeqQueenNotesEditor()
{
    setLookAndFeel (nullptr);
}

void SeqQueenNotesEditor::timerCallback()
{
    const int base = proc.getTriggerNote();
    const bool running = proc.isTransportRunning();
    if (base == shownTrigger && running == shownRunning)
        return;

    shownTrigger = base;
    shownRunning = running;
    for (int f = 0; f < opxy::kNumNoteFx; ++f)
        pads[f]->setKeyLabel (opxy::ui::liveNoteName (base + f));
    repaint();
}

void SeqQueenNotesEditor::paint (juce::Graphics& g)
{
    using namespace opxy::ui;
    g.fillAll (voidColour);

    auto area = getLocalBounds().reduced (kMargin, 12);
    auto header = area.removeFromTop (kHeaderHeight);
    deviceHeader (g, header, "SEQ QUEEN NOTES",
                  "Your clip plays through this. Hold a pad or its note to change what comes out.");

    // Transport state: the green pads only add notes while the host is playing.
    auto status = header.removeFromRight (190).removeFromTop (26);
    g.setColour (shownRunning ? green : textFaint);
    g.fillEllipse (status.removeFromLeft (10).withSizeKeepingCentre (6, 6).toFloat());
    status.removeFromLeft (6);
    text (g, status, shownRunning ? "HOST PLAYING" : "STOPPED: GREEN PADS NEED PLAY",
          monoFont (8.5f), shownRunning ? green : textDim);

    const char* rows[kNumRows] = { "RHYTHM  -  mute, roll, fill, add drums",
                                   "PITCH + SHAPE  -  move, chop, thin out, echo",
                                   "PLAYING  -  arpeggiate, stack, strum, groove" };
    for (int row = 0; row < kNumRows; ++row)
    {
        text (g, area.removeFromTop (kRowLabelHeight), rows[row], captionFont (8.0f), textDim);
        area.removeFromTop (kPadHeight + 8);
    }

    text (g, area.removeFromTop (kRowLabelHeight), "SETTINGS", captionFont (8.0f), textDim);

    const auto rate = repeatRate.getBounds();
    text (g, rate.withY (rate.getBottom() + 6).withHeight (12), "REPEAT / ARP RATE",
          captionFont (7.5f), textDim, juce::Justification::centred);

    const auto footer = getLocalBounds().reduced (kMargin, 0).removeFromBottom (kFooterHeight);
    text (g, footer,
          "In Live: play your clip on this track. On your instrument track set MIDI From -> this track -> "
          "Seq Queen Notes, Monitor -> In.",
          monoFont (8.5f), textFaint);
}

void SeqQueenNotesEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 12);
    area.removeFromTop (kHeaderHeight);

    const int gap = 8;
    const int padWidth = (area.getWidth() - gap * (kPadsPerRow - 1)) / kPadsPerRow;

    for (int row = 0; row < kNumRows; ++row)
    {
        area.removeFromTop (kRowLabelHeight);
        auto line = area.removeFromTop (kPadHeight);
        area.removeFromTop (8);
        for (int k = 0; k < kPadsPerRow && row * kPadsPerRow + k < opxy::kNumNoteFx; ++k)
        {
            pads[row * kPadsPerRow + k]->setBounds (line.removeFromLeft (padWidth));
            line.removeFromLeft (gap);
        }
    }

    area.removeFromTop (kRowLabelHeight);
    auto settings = area.removeFromTop (kSettingsHeight - kRowLabelHeight + 8);
    triggerKnob.setBounds (settings.removeFromLeft (120));
    settings.removeFromLeft (gap);
    for (auto* knob : { &kickKnob, &snareKnob, &hatKnob, &swingKnob })
    {
        knob->setBounds (settings.removeFromLeft (90));
        settings.removeFromLeft (gap);
    }
    settings.removeFromLeft (16);
    repeatRate.setBounds (settings.removeFromLeft (110).withSizeKeepingCentre (110, 22).translated (0, -10));
}

