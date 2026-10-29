#pragma once

// -----------------------------------------------------------------------------
//  Parameter IDs and layout.
//
//  Everything the host can automate lives here as AudioProcessorValueTreeState
//  parameters. Step data (which steps are on, and their p-locks) is NOT a
//  parameter: it is saved in the state ValueTree via opxy::serialiseSteps().
//
//  ID conventions, keep them stable once a session has been saved:
//      t<track>_<name>        e.g. t3_cutoff
//      fx_<fxName>_<name>     e.g. fx_tape_amount, fx_tape_a
//      <global name>          e.g. master_gain
// -----------------------------------------------------------------------------

#include <JuceHeader.h>
#include "../Core/Types.h"

namespace opxy
{
namespace params
{

inline juce::String trackId (int track, const juce::String& name)
{
    return "t" + juce::String (track) + "_" + name;
}

inline juce::String fxId (int fxIndex, const juce::String& name)
{
    const auto type = static_cast<FxType> (juce::jlimit (0, static_cast<int> (FxType::Count) - 1, fxIndex));
    return "fx_" + juce::String (fxTypeId (type)) + "_" + name;
}

juce::StringArray engineChoices();
juce::StringArray scaleChoices();
juce::StringArray fxTargetChoices();
juce::StringArray trackChoices();

/** Builds the whole parameter layout (globals + 8 tracks + 6 punch FX). */
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace params
} // namespace opxy
