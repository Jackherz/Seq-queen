#include "TrackMixer.h"
#include "../Theme.h"

namespace opxy
{
namespace
{
// Column layout, shared by resized() and paint() through the caption widths.
constexpr int kRowHeight = 22;
constexpr int kHeaderHeight = 28;   // section title + column captions
constexpr int kLabelWidth = 74;
constexpr int kEngineWidth = 84;
constexpr int kKnobWidth = 40;
constexpr int kScaleWidth = 78;
constexpr int kToggleWidth = 24;
constexpr int kNumKnobs = 7;   // level, cut, res, dec, prob, hits, ratchet
} // namespace

TrackMixerComponent::TrackMixerComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    auto& state = proc.getAPVTS();

    for (int t = 0; t < kMaxTracks; ++t)
    {
        Row& row = rows[t];

        row.name.setText ("TRACK " + juce::String (t + 1), juce::dontSendNotification);
        row.name.setFont (ui::monoFont (8.5f, true));
        row.name.setColour (juce::Label::textColourId, ui::boneDim);
        addAndMakeVisible (row.name);

        row.engine.addItemList (params::engineChoices(), 1);
        ui::styleCombo (row.engine);
        addAndMakeVisible (row.engine);
        row.engineAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::trackId (t, "engine"), row.engine);

        struct Strip
        {
            juce::Slider* slider;
            const char* id;
            double min, max, interval, skew;
        };

        const Strip strips[] = {
            { &row.level,       "level",       0.0,   1.2,    0.001, -1.0 },
            { &row.cutoff,      "cutoff",      40.0,  18000.0, 1.0, 1100.0 },
            { &row.resonance,   "resonance",   0.0,   0.95,   0.001, -1.0 },
            { &row.decay,       "decay",       0.02,  3.0,    0.001, 0.4 },
            { &row.probability, "probability", 0.0,   1.0,    0.001, -1.0 },
            { &row.hits,        "hits",        0.0,   64.0,   1.0,   -1.0 },
            { &row.ratchet,     "ratchet",     1.0,   8.0,    1.0,   -1.0 },
        };

        for (const auto& strip : strips)
        {
            strip.slider->setRange (strip.min, strip.max, strip.interval);
            if (strip.skew > 0.0)
                strip.slider->setSkewFactorFromMidPoint (strip.skew);
            ui::styleLinear (*strip.slider);
            addAndMakeVisible (*strip.slider);
        }

        row.levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "level"), row.level);
        row.cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "cutoff"), row.cutoff);
        row.resonanceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "resonance"), row.resonance);
        row.decayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "decay"), row.decay);
        row.probabilityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "probability"), row.probability);
        row.hitsAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "hits"), row.hits);
        row.ratchetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "ratchet"), row.ratchet);

        row.scale.addItemList (params::scaleChoices(), 1);
        ui::styleCombo (row.scale);
        addAndMakeVisible (row.scale);
        row.scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::trackId (t, "scale"), row.scale);

        for (auto* button : { &row.euclid, &row.mute })
        {
            button->setClickingTogglesState (true);
            ui::styleToggle (*button);
            addAndMakeVisible (*button);
        }

        row.euclid.setColour (juce::TextButton::buttonOnColourId, ui::green);
        row.mute.setColour (juce::TextButton::buttonOnColourId, ui::red);

        row.euclidAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::trackId (t, "euclid"), row.euclid);
        row.muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::trackId (t, "mute"), row.mute);

        // Length is edited in the header of each row as a stepped number; the
        // slider doubles as the hit target for dragging.
        row.length.setRange (1.0, 64.0, 1.0);
        row.length.setSliderStyle (juce::Slider::IncDecButtons);
        row.length.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 26, 16);
        row.length.setColour (juce::Slider::textBoxTextColourId, ui::boneDim);
        row.length.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        row.length.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (row.length);
        row.lengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "length"), row.length);

    }
}

void TrackMixerComponent::resized()
{
    auto area = getLocalBounds().reduced (14, 8);
    area.removeFromTop (kHeaderHeight + 6);

    for (int t = 0; t < kMaxTracks; ++t)
    {
        Row& row = rows[t];
        auto line = area.removeFromTop (kRowHeight);
        auto controls = line.withSizeKeepingCentre (line.getWidth(), 16);

        row.name.setBounds (controls.removeFromLeft (kLabelWidth - 22));
        row.length.setBounds (controls.removeFromLeft (20).reduced (0, 1));
        row.engine.setBounds (controls.removeFromLeft (kEngineWidth).reduced (2, 0));

        auto knobArea = controls.removeFromLeft (kKnobWidth * kNumKnobs);
        juce::Slider* strips[kNumKnobs] = { &row.level, &row.cutoff, &row.resonance,
                                            &row.decay, &row.probability, &row.hits,
                                            &row.ratchet };
        for (auto* strip : strips)
            strip->setBounds (knobArea.removeFromLeft (kKnobWidth).reduced (4, 0));

        row.scale.setBounds (controls.removeFromLeft (kScaleWidth).reduced (2, 0));
        row.euclid.setBounds (controls.removeFromLeft (kToggleWidth).reduced (1, 1));
        row.mute.setBounds (controls.removeFromLeft (kToggleWidth).reduced (1, 1));
    }
}

void TrackMixerComponent::paint (juce::Graphics& g)
{
    ui::panelBox (g, getLocalBounds());

    auto header = getLocalBounds().reduced (14, 8);
    ui::sectionHeader (g, header.removeFromTop (14), "04", "voices",
                       juce::CharPointer_UTF8 ("8 tracks · polyrhythm lengths · brains"));

    header = header.withTrimmedTop (kHeaderHeight - 22).withHeight (12);
    header.removeFromLeft (kLabelWidth - 22);
    ui::caption (g, header.removeFromLeft (20), "len", ui::textFaint);
    header.removeFromLeft (kEngineWidth);
    header.removeFromLeft (4);

    static const char* captions[kNumKnobs] = { "lvl", "cut", "res", "dec", "prob", "hits", "rtch" };
    for (int i = 0; i < kNumKnobs; ++i)
        ui::caption (g, header.removeFromLeft (kKnobWidth), captions[i], ui::textFaint);

    header.removeFromLeft (2);
    ui::caption (g, header.removeFromLeft (kScaleWidth), "scale", ui::textFaint);
    ui::caption (g, header.removeFromLeft (kToggleWidth), "eu", ui::textFaint);
    ui::caption (g, header.removeFromLeft (kToggleWidth), "mu", ui::textFaint);

    // Row separators + a mute/engine state hint on the left gutter.
    auto rowArea = getLocalBounds().reduced (14, 8);
    rowArea.removeFromTop (kHeaderHeight + 6);

    for (int t = 0; t < kMaxTracks; ++t)
    {
        auto line = rowArea.removeFromTop (kRowHeight);
        g.setColour (ui::hairline.withAlpha (0.6f));
        g.drawHorizontalLine (line.getBottom() - 1, static_cast<float> (line.getX()),
                              static_cast<float> (line.getRight()));

        if (t % 2 == 0)
        {
            g.setColour (juce::Colour (0xff101318));
            g.fillRect (line.reduced (0, 1));
        }
    }
}

} // namespace opxy
