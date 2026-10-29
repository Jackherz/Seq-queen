// Kept apart from FxProcessor.cpp so the snapshot tool can link every
// processor without three competing createPluginFilter() definitions.
#include "FxProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeqQueenFxProcessor();
}
