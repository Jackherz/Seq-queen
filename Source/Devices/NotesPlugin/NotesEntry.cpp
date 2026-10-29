// Kept apart from NotesProcessor.cpp so the snapshot tool can link every
// processor without three competing createPluginFilter() definitions.
#include "NotesProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeqQueenNotesProcessor();
}
