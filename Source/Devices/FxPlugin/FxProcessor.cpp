#include "FxProcessor.h"
#include "FxEditor.h"

namespace
{
// Same IDs as the instrument's FX parameters ("fx_tape_amount"...), so the
// meaning of each one is identical across both plugins.
juce::String fxId (int f, const juce::String& name)
{
    return "fx_" + juce::String (opxy::fxTypeId (static_cast<opxy::FxType> (f))) + "_" + name;
}

//                                              tape   delay  filter stut   verb   crush  rev    gate   hipass pitch
constexpr float kDefaultAmount[opxy::kMaxFx] = { 1.0f,  0.9f,  1.0f,  1.0f,  0.85f, 0.9f,  1.0f,  1.0f,  1.0f,  1.0f };
constexpr float kDefaultA[opxy::kMaxFx]      = { 0.25f, 0.3f,  0.75f, 0.2f,  0.6f,  0.7f,  0.5f,  0.667f, 0.75f, 0.0f };
constexpr float kDefaultB[opxy::kMaxFx]      = { 0.2f,  0.45f, 0.5f,  0.66f, 0.4f,  0.5f,  0.333f, 0.3f, 0.4f,  0.4f };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout SeqQueenFxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "trigger_note", 1 },
                                                           "Trigger Note", 0, 121, 36));

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        const juce::String prefix = juce::String (opxy::fxTypeName (static_cast<opxy::FxType> (f))) + " ";
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { fxId (f, "amount"), 1 },
                                                                 prefix + "Mix", unit, kDefaultAmount[f]));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { fxId (f, "a"), 1 },
                                                                 prefix + "Macro A", unit, kDefaultA[f]));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { fxId (f, "b"), 1 },
                                                                 prefix + "Macro B", unit, kDefaultB[f]));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { fxId (f, "velamt"), 1 },
                                                                 prefix + "Velocity Amount", unit, 0.7f));
        // 0 = off, above 0 = punched at that strength. Automation, and the Max for
        // Live device on the same track, punch through this.
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { fxId (f, "punch"), 1 },
                                                                 prefix + "Punch", unit, 0.0f));
    }
    return layout;
}

SeqQueenFxProcessor::SeqQueenFxProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SeqQueenFx", createLayout())
{
    triggerNote = state.getRawParameterValue ("trigger_note");
    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        fxParams[f].amount   = state.getRawParameterValue (fxId (f, "amount"));
        fxParams[f].a        = state.getRawParameterValue (fxId (f, "a"));
        fxParams[f].b        = state.getRawParameterValue (fxId (f, "b"));
        fxParams[f].velocity = state.getRawParameterValue (fxId (f, "velamt"));
        fxParams[f].punch    = state.getRawParameterValue (fxId (f, "punch"));
        padHeld[f].store (false);
    }
}

bool SeqQueenFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SeqQueenFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bus.prepare (sampleRate, samplesPerBlock);
    bus.reset();
    monoScratch.setSize (1, juce::jmax (samplesPerBlock, 4096));
    for (auto& held : noteHeld)
        held = false;
}

int SeqQueenFxProcessor::getTriggerNote() const
{
    return static_cast<int> (std::lround (triggerNote->load()));
}

void SeqQueenFxProcessor::setPadHeld (int fx, bool held)
{
    if (fx >= 0 && fx < opxy::kMaxFx)
    {
        if (held)
            bus.setPunchVelocity (fx, 1.0f);   // pads are full strength
        padHeld[fx].store (held);
    }
}

void SeqQueenFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int base = getTriggerNote();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOnOrOff())
        {
            const int f = message.getNoteNumber() - base;
            if (f >= 0 && f < opxy::kMaxFx)
            {
                noteHeld[f] = message.isNoteOn();
                if (message.isNoteOn())
                    bus.setPunchVelocity (f, message.getFloatVelocity());
            }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (auto& held : noteHeld)
                held = false;
        }
    }
    midi.clear();

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        auto& s = bus.getSettings (f);
        s.amount.store (fxParams[f].amount->load());
        s.paramA.store (fxParams[f].a->load());
        s.paramB.store (fxParams[f].b->load());
        s.velocitySensitivity.store (fxParams[f].velocity->load());

        // A held note or pad has already set its own velocity; the Punch
        // parameter's value is its strength.
        const float punch = fxParams[f].punch->load();
        const bool byParameter = punch > 0.001f && ! noteHeld[f] && ! padHeld[f].load();
        if (byParameter)
            bus.setPunchVelocity (f, punch);
        s.engaged.store (noteHeld[f] || padHeld[f].load() || byParameter);
    }

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
    bus.setTransport (transport);

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 2)
    {
        bus.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }
    else if (buffer.getNumChannels() == 1)
    {
        // Mono: run the stereo bus on a copy and fold back down.
        monoScratch.setSize (1, numSamples, false, false, true);
        monoScratch.copyFrom (0, 0, buffer, 0, 0, numSamples);
        bus.process (buffer.getWritePointer (0), monoScratch.getWritePointer (0), numSamples);
        buffer.addFrom (0, 0, monoScratch, 0, 0, numSamples);
        buffer.applyGain (0.5f);
    }
}

juce::AudioProcessorEditor* SeqQueenFxProcessor::createEditor()
{
    return new SeqQueenFxEditor (*this);
}

void SeqQueenFxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SeqQueenFxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}
