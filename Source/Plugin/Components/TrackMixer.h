#pragma once

// One row per track: engine, level, filter, brain controls (probability,
// Euclidean, ratchet, scale), step count, mute and the noise layer.
//
// Everything here is an APVTS parameter, so it is automatable and saved by the
// host; the processor copies these values into the engine every block.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace opxy
{

class TrackMixerComponent : public juce::Component
{
public:
    explicit TrackMixerComponent (SeqQueenAudioProcessor& processor);
    ~TrackMixerComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Row
    {
        juce::Label name;
        juce::ComboBox engine, scale;
        juce::Slider level, cutoff, resonance, decay, length, probability, hits, ratchet;
        juce::TextButton euclid { "E" };
        juce::TextButton mute { "M" };

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAtt, scaleAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            levelAtt, cutoffAtt, resonanceAtt, decayAtt, lengthAtt, probabilityAtt, hitsAtt, ratchetAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> euclidAtt, muteAtt;
    };

    SeqQueenAudioProcessor& proc;
    Row rows[kMaxTracks];

    static constexpr int rowHeight = 28;
    static constexpr int headerHeight = 16;
};

} // namespace opxy
