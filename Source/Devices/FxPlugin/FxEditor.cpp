#include "FxEditor.h"

namespace
{
using opxy::FxType;

constexpr int kWidth = 900;
constexpr int kHeight = 372;
constexpr int kMargin = 16;
constexpr int kHeaderHeight = 64;
constexpr int kPadHeight = 86;
constexpr int kKnobSize = 70;
constexpr int kFooterHeight = 26;

const char* describe (FxType type)
{
    switch (type)
    {
        case FxType::TapeStop:    return "slows to a halt like a tape machine";
        case FxType::DelayThrow:  return "throws into an echo that rings on";
        case FxType::FilterSweep: return "sweeps a resonant low-pass down";
        case FxType::Stutter:     return "loops a short slice of the beat";
        case FxType::ReverbBurst: return "washes into a big reverb burst";
        case FxType::Bitcrush:    return "crunches into lo-fi digital grit";
        case FxType::Count:       break;
    }
    return "";
}

/** What macro A / B mean for each effect, and how to print their values.
    Mappings mirror each effect's setPunchParam(). */
struct Macro
{
    const char* name;
    std::function<juce::String (double)> format;
};

juce::String percent (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
juce::String seconds (double s) { return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms"
                                                 : juce::String (s, 2) + " s"; }

Macro macroFor (FxType type, int index)
{
    switch (type)
    {
        case FxType::TapeStop:
            return index == 0 ? Macro { "stop time",  [] (double v) { return seconds (0.05 + v * 1.45); } }
                              : Macro { "restart",    [] (double v) { return seconds (0.05 + v * 1.45); } };
        case FxType::DelayThrow:
            return index == 0 ? Macro { "time",       [] (double v) { return seconds (0.03 + v * 0.87); } }
                              : Macro { "feedback",   [] (double v) { return percent (v * 0.9); } };
        case FxType::FilterSweep:
            return index == 0 ? Macro { "sweep",      percent }
                              : Macro { "resonance",  percent };
        case FxType::Stutter:
            return index == 0 ? Macro { "slice",      [] (double v) { return seconds (0.02 + v * 0.48); } }
                              : Macro { "speed",      [] (double v) { return juce::String (0.5 + v * 1.5, 2) + "x"; } };
        case FxType::ReverbBurst:
            return index == 0 ? Macro { "decay",      percent }
                              : Macro { "damping",    percent };
        case FxType::Bitcrush:
            return index == 0 ? Macro { "bits",       [] (double v) { return juce::String (juce::roundToInt (16.0 - v * 13.0)) + " bit"; } }
                              : Macro { "downsample", percent };
        case FxType::Count:
            break;
    }
    return { "", percent };
}

juce::String fxId (int f, const juce::String& name)
{
    return "fx_" + juce::String (opxy::fxTypeId (static_cast<FxType> (f))) + "_" + name;
}
} // namespace

SeqQueenFxEditor::SeqQueenFxEditor (SeqQueenFxProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lookAndFeel);

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        const auto type = static_cast<FxType> (f);
        const auto accent = opxy::ui::fxAccent (f);
        auto& column = columns[f];

        column.pad = std::make_unique<opxy::ui::PunchPad> (opxy::fxTypeName (type), describe (type), accent);
        column.pad->onHold   = [this, f] (bool held) { proc.setPadHeld (f, held); };
        column.pad->isActive = [this, f] { return proc.isPunching (f); };
        column.pad->getLevel = [this, f] { return proc.getPunchDepth (f); };
        addAndMakeVisible (*column.pad);

        const Macro a = macroFor (type, 0), b = macroFor (type, 1);
        const char* names[4] = { "mix", a.name, b.name, "velocity" };
        const char* ids[4] = { "amount", "a", "b", "velamt" };

        for (int k = 0; k < 4; ++k)
        {
            column.knobs[k] = std::make_unique<opxy::ui::LabelledKnob> (names[k], accent);
            column.knobs[k]->formatValue = k == 1 ? a.format : (k == 2 ? b.format : percent);
            addAndMakeVisible (*column.knobs[k]);
            column.attachments[k] = std::make_unique<Attachment> (proc.state, fxId (f, ids[k]),
                                                                  column.knobs[k]->slider);
        }
        column.knobs[3]->slider.setTooltip ("How much a soft trigger note punches less. 0 = always full.");
    }

    triggerKnob.formatValue = [] (double v)
    {
        const int base = juce::roundToInt (v);
        return opxy::ui::liveNoteName (base) + " - " + opxy::ui::liveNoteName (base + opxy::kMaxFx - 1);
    };
    addAndMakeVisible (triggerKnob);
    triggerAttachment = std::make_unique<Attachment> (proc.state, "trigger_note", triggerKnob.slider);

    setSize (kWidth, kHeight);
    startTimerHz (10);
    timerCallback();
}

SeqQueenFxEditor::~SeqQueenFxEditor()
{
    setLookAndFeel (nullptr);
}

void SeqQueenFxEditor::timerCallback()
{
    const int base = proc.getTriggerNote();
    if (base == shownTrigger)
        return;

    shownTrigger = base;
    for (int f = 0; f < opxy::kMaxFx; ++f)
        columns[f].pad->setKeyLabel (opxy::ui::liveNoteName (base + f));
    repaint();
}

void SeqQueenFxEditor::paint (juce::Graphics& g)
{
    g.fillAll (opxy::ui::voidColour);

    auto area = getLocalBounds().reduced (kMargin, 12);
    opxy::ui::deviceHeader (g, area.removeFromTop (kHeaderHeight),
                            "SEQ QUEEN FX",
                            "Hold a pad, or play its note from a MIDI clip. Harder notes punch harder.");

    const auto footer = getLocalBounds().reduced (kMargin, 0).removeFromBottom (kFooterHeight);
    opxy::ui::text (g, footer,
                    "In Live: on a MIDI track, set MIDI To -> this track -> Seq Queen FX, then draw notes "
                        + opxy::ui::liveNoteName (shownTrigger) + "-"
                        + opxy::ui::liveNoteName (shownTrigger + opxy::kMaxFx - 1) + " in a clip.",
                    opxy::ui::monoFont (8.5f), opxy::ui::textFaint);
}

void SeqQueenFxEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 12);
    area.removeFromBottom (kFooterHeight - 12);

    auto header = area.removeFromTop (kHeaderHeight);
    triggerKnob.setBounds (header.removeFromRight (110).withTrimmedTop (-6));

    const int gap = 8;
    const int columnWidth = (area.getWidth() - gap * (opxy::kMaxFx - 1)) / opxy::kMaxFx;

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        auto column = area.removeFromLeft (columnWidth);
        area.removeFromLeft (gap);

        columns[f].pad->setBounds (column.removeFromTop (kPadHeight));
        column.removeFromTop (8);

        for (int row = 0; row < 2; ++row)
        {
            auto line = column.removeFromTop (kKnobSize + 10);
            const int half = line.getWidth() / 2;
            columns[f].knobs[row * 2]->setBounds (line.removeFromLeft (half));
            columns[f].knobs[row * 2 + 1]->setBounds (line);
        }
    }
}
