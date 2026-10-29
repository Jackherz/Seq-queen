#include "ParamIDs.h"

namespace opxy
{
namespace params
{
namespace
{
using Range = juce::NormalisableRange<float>;

constexpr float kMinCutoff = 40.0f;
constexpr float kMaxCutoff = 18000.0f;

Range logarithmic (float minValue, float maxValue, float centreSkew = 0.3f)
{
    return Range (minValue, maxValue, 0.0f, centreSkew);
}
} // namespace

juce::StringArray engineChoices()
{
    juce::StringArray choices;
    for (int i = 0; i < static_cast<int> (EngineType::Count); ++i)
        choices.add (engineTypeName (static_cast<EngineType> (i)));
    return choices;
}

juce::StringArray scaleChoices()
{
    juce::StringArray choices;
    for (int i = 0; i < static_cast<int> (ScaleType::Count); ++i)
        choices.add (scaleTypeName (static_cast<ScaleType> (i)));
    return choices;
}

juce::StringArray fxTargetChoices()
{
    return { "Master", "Focus Track" };
}

juce::StringArray trackChoices()
{
    juce::StringArray choices;
    for (int i = 0; i < kMaxTracks; ++i)
        choices.add ("Track " + juce::String (i + 1));
    return choices;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto addFloat = [&layout] (const juce::String& id, const juce::String& name,
                                     Range range, float defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (id, name, range, defaultValue));
    };

    const auto addChoice = [&layout] (const juce::String& id, const juce::String& name,
                                      const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (id, name, choices, defaultIndex));
    };

    const auto addBool = [&layout] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (id, name, defaultValue));
    };

    // ------------------------------ globals ---------------------------------
    addFloat ("master_gain", "Master Gain", Range (-24.0f, 6.0f, 0.1f), -3.0f);
    addFloat ("swing", "Swing", Range (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("tempo", "Free Run Tempo", Range (40.0f, 240.0f, 0.1f), 120.0f);
    addChoice ("focus_track", "Focus Track", trackChoices(), 0);
    addBool ("midi_out", "MIDI Out", false);
    addBool ("fx_notes", "Punch FX via MIDI Notes", true);
    addFloat ("fx_note_base", "Punch FX Note Base", Range (0.0f, 108.0f, 1.0f), 36.0f);

    // ------------------------------- tracks ----------------------------------
    for (int t = 0; t < kMaxTracks; ++t)
    {
        const juce::String prefix = "Track " + juce::String (t + 1) + " ";

        addFloat (trackId (t, "level"), prefix + "Level", Range (0.0f, 1.2f, 0.001f), 0.7f);
        addFloat (trackId (t, "pan"),   prefix + "Pan", Range (-1.0f, 1.0f, 0.001f), 0.0f);
        addChoice (trackId (t, "engine"), prefix + "Engine", engineChoices(),
                   static_cast<int> (EngineType::Saw));
        addFloat (trackId (t, "cutoff"),    prefix + "Cutoff", logarithmic (kMinCutoff, kMaxCutoff), 3200.0f);
        addFloat (trackId (t, "resonance"), prefix + "Resonance", Range (0.0f, 0.95f, 0.001f), 0.15f);
        addFloat (trackId (t, "decay"),     prefix + "Decay", logarithmic (0.02f, 3.0f, 0.5f), 0.35f);
        addFloat (trackId (t, "length"),    prefix + "Steps", Range (1.0f, 64.0f, 1.0f), 16.0f);
        addFloat (trackId (t, "probability"), prefix + "Probability", Range (0.0f, 1.0f, 0.001f), 1.0f);

        addBool (trackId (t, "euclid"), prefix + "Euclidean", false);
        addFloat (trackId (t, "hits"),  prefix + "Euclid Hits", Range (0.0f, 64.0f, 1.0f), 4.0f);
        addFloat (trackId (t, "offset"), prefix + "Euclid Offset", Range (0.0f, 63.0f, 1.0f), 0.0f);
        addFloat (trackId (t, "ratchet"), prefix + "Ratchet", Range (1.0f, 8.0f, 1.0f), 1.0f);
        addFloat (trackId (t, "randpitch"), prefix + "Random Pitch", Range (0.0f, 24.0f, 0.1f), 0.0f);
        addFloat (trackId (t, "randvel"),   prefix + "Random Velocity", Range (0.0f, 1.0f, 0.001f), 0.0f);

        addChoice (trackId (t, "scale"), prefix + "Scale", scaleChoices(), 0);
        addFloat (trackId (t, "root"), prefix + "Scale Root", Range (0.0f, 11.0f, 1.0f), 0.0f);

        addBool (trackId (t, "mute"), prefix + "Mute", false);
        addBool (trackId (t, "noise"), prefix + "Noise Layer", false);
    }

    // ---------------------------- punch-in FX -------------------------------
    static constexpr float defaultAmounts[kMaxFx] = { 1.0f, 0.9f, 1.0f, 1.0f, 0.85f, 0.9f };
    static constexpr float defaultParamA[kMaxFx]  = { 0.25f, 0.3f, 0.75f, 0.2f, 0.6f, 0.7f };
    static constexpr float defaultParamB[kMaxFx]  = { 0.2f, 0.45f, 0.5f, 0.66f, 0.4f, 0.5f };

    for (int f = 0; f < kMaxFx; ++f)
    {
        const auto type = static_cast<FxType> (f);
        const juce::String prefix = juce::String (fxTypeName (type)) + " ";

        addFloat (fxId (f, "amount"), prefix + "Amount", Range (0.0f, 1.0f, 0.001f), defaultAmounts[f]);
        addFloat (fxId (f, "a"), prefix + "Macro A", Range (0.0f, 1.0f, 0.001f), defaultParamA[f]);
        addFloat (fxId (f, "b"), prefix + "Macro B", Range (0.0f, 1.0f, 0.001f), defaultParamB[f]);
        addFloat (fxId (f, "ramp"), prefix + "Ramp", Range (0.002f, 0.3f, 0.001f), 0.03f);
        // How much note velocity scales the punch strength (0 = ignore).
        addFloat (fxId (f, "velamt"), prefix + "Velocity Amount", Range (0.0f, 1.0f, 0.001f), 0.7f);
        addChoice (fxId (f, "target"), prefix + "Target", fxTargetChoices(), 0);

        // Automatable momentary state: a host curve or a MIDI CC can punch FX.
        addBool (fxId (f, "on"), prefix + "Punch", false);
    }

    return layout;
}

} // namespace params
} // namespace opxy
