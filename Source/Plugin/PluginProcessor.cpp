#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Core/PatternState.h"

namespace
{
constexpr int kMaxPendingNoteOffs = 128;
}

SeqQueenAudioProcessor::SeqQueenAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", opxy::params::createLayout())
{
    cacheParameterPointers();
    pendingNoteOffs.reserve (kMaxPendingNoteOffs);

    // A friendly starting point: kick on track 1, a bass, a hat and a pluck.
    auto& pattern = engine.getPattern();

    pattern.tracks[0].engine.store (static_cast<int> (opxy::EngineType::Sine));
    pattern.tracks[0].decay.store (0.22f);
    pattern.tracks[0].cutoff.store (900.0f);
    pattern.tracks[0].level.store (0.85f);
    for (int step = 0; step < 16; step += 4)
    {
        auto& s = pattern.tracks[0].steps[step];
        s.active.store (true);
        s.note.store (36.0f);
        s.velocity.store (1.0f);
        s.gate.store (0.4f);
    }

    pattern.tracks[1].engine.store (static_cast<int> (opxy::EngineType::Saw));
    pattern.tracks[1].decay.store (0.3f);
    pattern.tracks[1].cutoff.store (700.0f);
    pattern.tracks[1].resonance.store (0.35f);
    for (int step = 2; step < 16; step += 4)
    {
        auto& s = pattern.tracks[1].steps[step];
        s.active.store (true);
        s.note.store (step < 8 ? 36.0f : 43.0f);
        s.gate.store (0.6f);
    }

    pattern.tracks[2].engine.store (static_cast<int> (opxy::EngineType::Noise));
    pattern.tracks[2].decay.store (0.06f);
    pattern.tracks[2].cutoff.store (9000.0f);
    pattern.tracks[2].level.store (0.35f);
    for (int step = 0; step < 16; step += 2)
    {
        auto& s = pattern.tracks[2].steps[step];
        s.active.store (true);
        s.gate.store (0.15f);
        s.velocity.store (step % 4 == 0 ? 0.8f : 0.45f);
    }

    pattern.tracks[3].engine.store (static_cast<int> (opxy::EngineType::Square));
    pattern.tracks[3].decay.store (0.25f);
    pattern.tracks[3].cutoff.store (2600.0f);
    pattern.tracks[3].level.store (0.4f);
    const int leadNotes[8] = { 60, 63, 67, 70, 67, 63, 60, 58 };
    for (int i = 0; i < 8; ++i)
    {
        auto& s = pattern.tracks[3].steps[i * 2];
        s.active.store (true);
        s.note.store (static_cast<float> (leadNotes[i]));
        s.gate.store (0.35f);
        s.setPLock (opxy::PlockParam::Cutoff, i % 2 == 0 ? 3000.0f : 900.0f);
    }

    // Tracks 5..8 stay empty until the user fills them in.
    for (int t = 4; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (16);
}

// ------------------------------ parameters ----------------------------------

void SeqQueenAudioProcessor::cacheParameterPointers()
{
    const auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    pMasterGain  = get ("master_gain");
    pSwing       = get ("swing");
    pTempo       = get ("tempo");
    pFocusTrack  = get ("focus_track");
    pMidiOut     = get ("midi_out");
    pFxNotes     = get ("fx_notes");
    pFxNoteBase  = get ("fx_note_base");

    for (int t = 0; t < opxy::kMaxTracks; ++t)
    {
        auto& p = trackParams[t];
        p.level       = get (opxy::params::trackId (t, "level"));
        p.pan         = get (opxy::params::trackId (t, "pan"));
        p.engine      = get (opxy::params::trackId (t, "engine"));
        p.cutoff      = get (opxy::params::trackId (t, "cutoff"));
        p.resonance   = get (opxy::params::trackId (t, "resonance"));
        p.decay       = get (opxy::params::trackId (t, "decay"));
        p.length      = get (opxy::params::trackId (t, "length"));
        p.probability = get (opxy::params::trackId (t, "probability"));
        p.euclid      = get (opxy::params::trackId (t, "euclid"));
        p.hits        = get (opxy::params::trackId (t, "hits"));
        p.offset      = get (opxy::params::trackId (t, "offset"));
        p.ratchet     = get (opxy::params::trackId (t, "ratchet"));
        p.randPitch   = get (opxy::params::trackId (t, "randpitch"));
        p.randVel     = get (opxy::params::trackId (t, "randvel"));
        p.scale       = get (opxy::params::trackId (t, "scale"));
        p.root        = get (opxy::params::trackId (t, "root"));
        p.mute        = get (opxy::params::trackId (t, "mute"));
        p.noise       = get (opxy::params::trackId (t, "noise"));
    }

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        auto& p = fxParams[f];
        p.amount       = get (opxy::params::fxId (f, "amount"));
        p.macroA       = get (opxy::params::fxId (f, "a"));
        p.macroB       = get (opxy::params::fxId (f, "b"));
        p.ramp         = get (opxy::params::fxId (f, "ramp"));
        p.target       = get (opxy::params::fxId (f, "target"));
        p.automationOn = get (opxy::params::fxId (f, "on"));
        p.velocityAmount = get (opxy::params::fxId (f, "velamt"));
    }
}

void SeqQueenAudioProcessor::pushParametersToEngine()
{
    auto& pattern = engine.getPattern();

    for (int t = 0; t < opxy::kMaxTracks; ++t)
    {
        const auto& p = trackParams[t];
        auto& track = pattern.tracks[t];

        if (p.level != nullptr)       track.level.store (p.level->load());
        if (p.pan != nullptr)         track.pan.store (p.pan->load());
        if (p.engine != nullptr)      track.engine.store (static_cast<int> (std::lround (p.engine->load())));
        if (p.cutoff != nullptr)      track.cutoff.store (p.cutoff->load());
        if (p.resonance != nullptr)   track.resonance.store (p.resonance->load());
        if (p.decay != nullptr)       track.decay.store (p.decay->load());
        if (p.length != nullptr)      track.length.store (static_cast<int> (std::lround (p.length->load())));
        if (p.probability != nullptr) track.probability.store (p.probability->load());
        if (p.euclid != nullptr)      track.euclidOn.store (p.euclid->load() > 0.5f);
        if (p.hits != nullptr)        track.euclidHits.store (static_cast<int> (std::lround (p.hits->load())));
        if (p.offset != nullptr)      track.euclidOffset.store (static_cast<int> (std::lround (p.offset->load())));
        if (p.ratchet != nullptr)     track.ratchet.store (static_cast<int> (std::lround (p.ratchet->load())));
        if (p.randPitch != nullptr)   track.randomPitch.store (p.randPitch->load());
        if (p.randVel != nullptr)     track.randomVel.store (p.randVel->load());
        if (p.scale != nullptr)       track.scaleType.store (static_cast<int> (std::lround (p.scale->load())));
        if (p.root != nullptr)        track.scaleRoot.store (static_cast<int> (std::lround (p.root->load())));
        if (p.mute != nullptr)        track.muted.store (p.mute->load() > 0.5f);
        if (p.noise != nullptr)       track.noiseOn.store (p.noise->load() > 0.5f);
    }

    for (int f = 0; f < opxy::kMaxFx; ++f)
    {
        const auto& p = fxParams[f];
        auto& settings = engine.getFxSettings (f);

        if (p.amount != nullptr) settings.amount.store (p.amount->load());
        if (p.macroA != nullptr) settings.paramA.store (p.macroA->load());
        if (p.macroB != nullptr) settings.paramB.store (p.macroB->load());
        if (p.ramp != nullptr)   settings.rampSeconds.store (p.ramp->load());
        if (p.target != nullptr) settings.target.store (static_cast<int> (std::lround (p.target->load())));
        if (p.velocityAmount != nullptr) settings.velocitySensitivity.store (p.velocityAmount->load());

        const bool automated = p.automationOn != nullptr && p.automationOn->load() > 0.5f;
        const bool held = uiHeld[f].load() || midiHeld[f].load();
        settings.engaged.store (automated || held);
    }

    if (pMasterGain != nullptr) engine.setMasterGainDb (pMasterGain->load());
    if (pSwing != nullptr)      engine.setSwing (pSwing->load());
    if (pTempo != nullptr)      engine.setFreeRunTempo (static_cast<double> (pTempo->load()));
    if (pFocusTrack != nullptr) engine.setFocusTrack (static_cast<int> (std::lround (pFocusTrack->load())));
    if (pFxNoteBase != nullptr) fxNoteBase.store (static_cast<int> (std::lround (pFxNoteBase->load())));
}

// -------------------------------- audio ------------------------------------

void SeqQueenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 256));
    engine.reset();
    pendingNoteOffs.clear();
    sawPlayhead = false;
    pushParametersToEngine();
}

void SeqQueenAudioProcessor::releaseResources()
{
    engine.reset();
}

void SeqQueenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // The engine writes (additively) into every channel, so anything it does not
    // touch must be cleared explicitly.
    const int outputChannels = juce::jmin (numChannels, 2);
    for (int ch = outputChannels; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
    {
        midiMessages.clear();
        return;
    }

    pushParametersToEngine();

    // ---- transport ---------------------------------------------------------
    opxy::TransportInfo transport;
    transport.bpm = pTempo != nullptr ? static_cast<double> (pTempo->load()) : 120.0;
    bool playheadReportsPosition = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            sawPlayhead = true;
            transport.playing = position->getIsPlaying();
            playheadReportsPosition = true;

            const auto bpm = position->getBpm();
            if (bpm.hasValue())
                transport.bpm = bpm.orFallback (120.0);

            const auto ppq = position->getPpqPosition();
            if (ppq.hasValue())
            {
                transport.hasPpq = true;
                transport.ppqAtBlockStart = ppq.orFallback (0.0);
            }
        }
    }

    // Standalone / no playhead: free run so the sequencer still plays.
    if (! playheadReportsPosition)
        transport.playing = true;

    // ---- MIDI in -----------------------------------------------------------
    handleIncomingMidi (midiMessages, numSamples);
    midiMessages.clear();

    // ---- render ------------------------------------------------------------
    engine.process (buffer.getArrayOfWritePointers(), numChannels, numSamples, transport);

    // ---- MIDI out ----------------------------------------------------------
    writeGeneratedMidi (midiMessages, numSamples);
}

void SeqQueenAudioProcessor::handleIncomingMidi (const juce::MidiBuffer& midiMessages, int)
{

    const bool fxNotesEnabled = pFxNotes == nullptr || pFxNotes->load() > 0.5f;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOnOrOff())
        {
            const int note = message.getNoteNumber();
            const int fxIndex = note - fxNoteBase.load();

            if (fxNotesEnabled && fxIndex >= 0 && fxIndex < opxy::kMaxFx)
            {
                midiHeld[fxIndex].store (message.isNoteOn());
                // Velocity is part of the gesture: tap = subtle, hit = destroy.
                if (message.isNoteOn())
                    engine.setPunchVelocity (fxIndex, message.getFloatVelocity());
                continue;
            }

            if (message.isNoteOn())
            {
                // Play-along: incoming notes play on the focused track.
                const int focus = pFocusTrack != nullptr
                                    ? static_cast<int> (std::lround (pFocusTrack->load()))
                                    : 0;
                engine.playNote (focus, note, message.getFloatVelocity());
            }
            continue;
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (auto& held : midiHeld)
                held.store (false);
        }
    }
}

void SeqQueenAudioProcessor::writeGeneratedMidi (juce::MidiBuffer& midiMessages, int numSamples)
{
    const bool midiOutEnabled = pMidiOut != nullptr && pMidiOut->load() > 0.5f;

    // Note-offs that were scheduled in earlier blocks.
    for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end();)
    {
        if (it->samplesRemaining < numSamples)
        {
            if (midiOutEnabled)
                midiMessages.addEvent (juce::MidiMessage::noteOff (it->channel, it->note),
                                       juce::jlimit (0, numSamples - 1, it->samplesRemaining));
            it = pendingNoteOffs.erase (it);
        }
        else
        {
            it->samplesRemaining -= numSamples;
            ++it;
        }
    }

    if (! midiOutEnabled)
        return;

    const int count = engine.getLastEventCount();
    const auto* events = engine.getLastEvents();
    const double samplesPerStep = engine.getSequencer().getSamplesPerStep();

    for (int i = 0; i < count; ++i)
    {
        const auto& event = events[i];
        if (! event.primary)
            continue;

        const int channel = juce::jlimit (1, 16, event.track + 1);
        const auto velocity = (juce::uint8) juce::jlimit (1.0f, 127.0f, event.velocity * 127.0f);

        midiMessages.addEvent (juce::MidiMessage::noteOn (channel, event.note, velocity),
                               juce::jlimit (0, numSamples - 1, event.sampleOffset));

        PendingNoteOff noteOff;
        noteOff.channel = channel;
        noteOff.note = event.note;
        noteOff.samplesRemaining = event.sampleOffset
                                 + static_cast<int> (event.gateSteps * samplesPerStep);

        if (static_cast<int> (pendingNoteOffs.size()) < kMaxPendingNoteOffs)
            pendingNoteOffs.push_back (noteOff);
    }
}

// ------------------------------ punch in FX --------------------------------

void SeqQueenAudioProcessor::setPunchHeld (int fxIndex, bool held)
{
    if (fxIndex < 0 || fxIndex >= opxy::kMaxFx)
        return;

    uiHeld[fxIndex].store (held);

    // A pad has no velocity, so treat it as a full strength hit.
    if (held)
        engine.setPunchVelocity (fxIndex, 1.0f);
}

int SeqQueenAudioProcessor::getFocusTrack() const
{
    if (pFocusTrack != nullptr)
        return juce::jlimit (0, opxy::kMaxTracks - 1,
                             static_cast<int> (std::lround (pFocusTrack->load())));
    return 0;
}

bool SeqQueenAudioProcessor::isPunchHeld (int fxIndex) const
{
    if (fxIndex < 0 || fxIndex >= opxy::kMaxFx)
        return false;
    return uiHeld[fxIndex].load() || midiHeld[fxIndex].load();
}

bool SeqQueenAudioProcessor::isPunchEngaged (int fxIndex) const
{
    // Resolved state: true when punched by a pad, a MIDI note, automation, *or*
    // a sequenced punch lane, so the pad lights up either way.
    return engine.isFxEngaged (fxIndex);
}

// -------------------------------- state ------------------------------------

void SeqQueenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // The step grid is not a parameter, so it rides along as a string property.
    state.setProperty (kPatternProperty,
                       juce::String (opxy::serialiseSteps (engine.getPattern())),
                       nullptr);
    state.setProperty (kStateVersionProperty, 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SeqQueenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    apvts.replaceState (state);

    const auto patternText = state.getProperty (kPatternProperty).toString();
    if (patternText.isNotEmpty())
        opxy::deserialiseSteps (patternText.toStdString(), engine.getPattern());

    pushParametersToEngine();
}

// -------------------------------- buses ------------------------------------

bool SeqQueenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Instrument: no input bus, mono or stereo output.
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* SeqQueenAudioProcessor::createEditor()
{
    return new SeqQueenAudioProcessorEditor (*this);
}

// --------------------------- plugin entry point -----------------------------

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeqQueenAudioProcessor();
}
