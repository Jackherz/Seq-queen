// Kept apart from NotesProcessor.cpp so the snapshot tool can link every
// processor without two competing createPluginFilter() definitions.
#include "NotesProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeqQueenNotesProcessor();
}
