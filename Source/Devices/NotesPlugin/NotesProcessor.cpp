#include "NotesProcessor.h"
#include "NotesEditor.h"

namespace
{
constexpr int kMaxEvents = 1024;
}

juce::AudioProcessorValueTreeState::ParameterLayout SeqQueenNotesProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto addNote = [&layout] (const char* id, const char* name, int defaultNote)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, 0, 127, defaultNote));
    };

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "trigger_note", 1 },
                                                           "Trigger Note", 0, 127 - opxy::kNumNoteFx + 1, 0));
    addNote ("kick_note", "Kick Note", 36);
    addNote ("snare_note", "Snare Note", 38);
    addNote ("hat_note", "Hat Note", 42);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "repeat_rate", 1 },
                                                              "Repeat Rate",
                                                              juce::StringArray { "1/8", "1/16", "1/32" }, 1));
    return layout;
}

SeqQueenNotesProcessor::SeqQueenNotesProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SeqQueenNotes", createLayout())
{
    triggerNote = state.getRawParameterValue ("trigger_note");
    kickNote    = state.getRawParameterValue ("kick_note");
    snareNote   = state.getRawParameterValue ("snare_note");
    hatNote     = state.getRawParameterValue ("hat_note");
    repeatRate  = state.getRawParameterValue ("repeat_rate");

    input.reserve (kMaxEvents);
    output.resize (kMaxEvents);
}

bool SeqQueenNotesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SeqQueenNotesProcessor::prepareToPlay (double sampleRate, int)
{
    noteFx.prepare (sampleRate);
    passThrough.ensureSize (4096);
}

int SeqQueenNotesProcessor::getTriggerNote() const
{
    return static_cast<int> (std::lround (triggerNote->load()));
}

void SeqQueenNotesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();   // makes no sound of its own
    const int numSamples = buffer.getNumSamples();

    auto& settings = noteFx.getSettings();
    settings.baseNote.store (getTriggerNote());
    settings.kickNote.store (static_cast<int> (std::lround (kickNote->load())));
    settings.snareNote.store (static_cast<int> (std::lround (snareNote->load())));
    settings.hatNote.store (static_cast<int> (std::lround (hatNote->load())));
    settings.repeatRate.store (static_cast<int> (std::lround (repeatRate->load())));

    opxy::TransportInfo transport;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            transport.playing = position->getIsPlaying();
            if (const auto bpm = position->getBpm(); bpm.hasValue())
                transport.bpm = *bpm;
            if (const auto ppq = position->getPpqPosition(); ppq.hasValue())
            {
                transport.hasPpq = true;
                transport.ppqAtBlockStart = *ppq;
            }
        }
    }
    transportRunning.store (transport.playing && transport.hasPpq);

    // Notes go through the punch engine; everything else (CCs, pitch bend,
    // aftertouch) passes straight through.
    input.clear();
    passThrough.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOnOrOff() && input.size() < static_cast<std::size_t> (kMaxEvents))
        {
            opxy::NoteEvent e;
            e.sampleOffset = metadata.samplePosition;
            e.channel = message.getChannel();
            e.note = message.getNoteNumber();
            e.velocity = message.getFloatVelocity();
            e.on = message.isNoteOn();
            input.push_back (e);
        }
        else
        {
            passThrough.addEvent (message, metadata.samplePosition);
        }
    }

    const int count = noteFx.process (input.data(), static_cast<int> (input.size()), numSamples, transport,
                                      output.data(), static_cast<int> (output.size()));

    midi.swapWith (passThrough);
    for (int k = 0; k < count; ++k)
    {
        const auto& e = output[static_cast<std::size_t> (k)];
        midi.addEvent (e.on ? juce::MidiMessage::noteOn (e.channel, e.note, e.velocity)
                            : juce::MidiMessage::noteOff (e.channel, e.note),
                       e.sampleOffset);
    }
}

juce::AudioProcessorEditor* SeqQueenNotesProcessor::createEditor()
{
    return new SeqQueenNotesEditor (*this);
}

void SeqQueenNotesProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SeqQueenNotesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}
