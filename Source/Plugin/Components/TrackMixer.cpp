#include "TrackMixer.h"
#include "../Theme.h"

namespace opxy
{

TrackMixerComponent::TrackMixerComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    auto& state = proc.getAPVTS();

    for (int t = 0; t < kMaxTracks; ++t)
    {
        Row& row = rows[t];

        row.name.setText ("TRACK " + juce::String (t + 1), juce::dontSendNotification);
        row.name.setFont (theme::labelFont (10.0f));
        row.name.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (row.name);

        // --- engine type ----------------------------------------------------
        row.engine.addItemList (params::engineChoices(), 1);
        theme::styleCombo (row.engine);
        addAndMakeVisible (row.engine);
        row.engineAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::trackId (t, "engine"), row.engine);

        // --- rotary controls ------------------------------------------------
        struct KnobSpec
        {
            juce::Slider* slider;
            const char* id;
            double min, max, interval, skew;
        };

        const KnobSpec knobs[] = {
            { &row.level,       "level",       0.0,   1.2,   0.001, -1.0 },
            { &row.cutoff,      "cutoff",      40.0,  18000.0, 1.0, 1100.0 },
            { &row.resonance,   "resonance",   0.0,   0.95,  0.001, -1.0 },
            { &row.decay,       "decay",       0.02,  3.0,   0.001, 0.4 },
            { &row.length,      "length",      1.0,   64.0,  1.0,   -1.0 },
            { &row.probability, "probability", 0.0,   1.0,   0.001, -1.0 },
            { &row.hits,        "hits",        0.0,   64.0,  1.0,   -1.0 },
            { &row.ratchet,     "ratchet",     1.0,   8.0,   1.0,   -1.0 },
        };

        for (const auto& spec : knobs)
        {
            spec.slider->setRange (spec.min, spec.max, spec.interval);
            if (spec.skew > 0.0)
                spec.slider->setSkewFactorFromMidPoint (spec.skew);
            theme::styleRotary (*spec.slider);
            addAndMakeVisible (*spec.slider);
        }

        row.levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "level"), row.level);
        row.cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "cutoff"), row.cutoff);
        row.resonanceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "resonance"), row.resonance);
        row.decayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "decay"), row.decay);
        row.lengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "length"), row.length);
        row.probabilityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "probability"), row.probability);
        row.hitsAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "hits"), row.hits);
        row.ratchetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::trackId (t, "ratchet"), row.ratchet);

        // --- scale ----------------------------------------------------------
        row.scale.addItemList (params::scaleChoices(), 1);
        theme::styleCombo (row.scale);
        addAndMakeVisible (row.scale);
        row.scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::trackId (t, "scale"), row.scale);

        // --- toggles --------------------------------------------------------
        for (auto* button : { &row.euclid, &row.mute })
        {
            button->setClickingTogglesState (true);
            theme::styleToggle (*button);
            addAndMakeVisible (*button);
        }

        row.euclidAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::trackId (t, "euclid"), row.euclid);
        row.muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::trackId (t, "mute"), row.mute);
        row.mute.setColour (juce::TextButton::buttonOnColourId, theme::danger);
    }
}

void TrackMixerComponent::resized()
{
    auto area = getLocalBounds().reduced (8, 4);
    area.removeFromTop (headerHeight - 4);

    for (int t = 0; t < kMaxTracks; ++t)
    {
        Row& row = rows[t];
        auto line = area.removeFromTop (rowHeight).reduced (0, 2);

        row.name.setBounds (line.removeFromLeft (58));

        juce::Rectangle<int> cells[] = {
            line.removeFromLeft (80),  // engine
            line.removeFromLeft (42),  // level
            line.removeFromLeft (42),  // cutoff
            line.removeFromLeft (42),  // resonance
            line.removeFromLeft (42),  // decay
            line.removeFromLeft (42),  // length
            line.removeFromLeft (42),  // probability
            line.removeFromLeft (42),  // hits
            line.removeFromLeft (42),  // ratchet
            line.removeFromLeft (84),  // scale
            line.removeFromLeft (26),  // euclid
            line.removeFromLeft (26),  // mute
        };

        row.engine.setBounds (cells[0].reduced (2, 3));
        row.level.setBounds (cells[1]);
        row.cutoff.setBounds (cells[2]);
        row.resonance.setBounds (cells[3]);
        row.decay.setBounds (cells[4]);
        row.length.setBounds (cells[5]);
        row.probability.setBounds (cells[6]);
        row.hits.setBounds (cells[7]);
        row.ratchet.setBounds (cells[8]);
        row.scale.setBounds (cells[9].reduced (2, 3));
        row.euclid.setBounds (cells[10].reduced (2, 4));
        row.mute.setBounds (cells[11].reduced (2, 4));
    }
}

void TrackMixerComponent::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    auto header = getLocalBounds().reduced (8, 4).removeFromTop (headerHeight);
    header.removeFromLeft (58);

    static const char* captions[] = { "ENGINE", "LVL", "CUT", "RES", "DEC", "STEPS",
                                     "PROB", "HITS", "RTCH", "SCALE", "EUC", "MUTE" };
    static const int widths[] = { 80, 42, 42, 42, 42, 42, 42, 42, 42, 84, 26, 26 };

    for (int i = 0; i < 12; ++i)
        theme::drawCaption (g, header.removeFromLeft (widths[i]), captions[i], theme::textDim);
}

} // namespace opxy
