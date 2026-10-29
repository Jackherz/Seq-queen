#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace opxy
{
namespace
{
/** Gentle soft clip above 0.8 so eight loud tracks can't hard-clip the master.
    Fully transparent below the knee, which keeps normal material untouched. */
float softClip (float x) noexcept
{
    constexpr float knee = 0.8f;
    const float magnitude = std::abs (x);
    if (magnitude <= knee)
        return x;

    const float over = (magnitude - knee) / (1.0f - knee);
    const float shaped = knee + (1.0f - knee) * std::tanh (over);
    return x < 0.0f ? -shaped : shaped;
}
} // namespace

Engine::Engine()
{
    for (int t = 0; t < kMaxTracks; ++t)
    {
        trackBuffer[t][0].assign (kMaxBlockSize, 0.0f);
        trackBuffer[t][1].assign (kMaxBlockSize, 0.0f);
    }
    mixL.assign (kMaxBlockSize, 0.0f);
    mixR.assign (kMaxBlockSize, 0.0f);
}

void Engine::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlockSize = clamp (newMaxBlockSize, 16, kMaxBlockSize);

    for (int t = 0; t < kMaxTracks; ++t)
    {
        trackBuffer[t][0].assign (static_cast<std::size_t> (kMaxBlockSize), 0.0f);
        trackBuffer[t][1].assign (static_cast<std::size_t> (kMaxBlockSize), 0.0f);
    }
    mixL.assign (static_cast<std::size_t> (kMaxBlockSize), 0.0f);
    mixR.assign (static_cast<std::size_t> (kMaxBlockSize), 0.0f);

    sequencer.prepare (sampleRate);
    synth.prepare (sampleRate);
    masterChain.prepare (sampleRate, kMaxBlockSize);
    focusChain.prepare (sampleRate, kMaxBlockSize);

    masterGainRamp.prepare (sampleRate);
    masterGainRamp.setTimeSeconds (0.02f);
    masterGainRamp.setTarget (dbToGain (masterGainDb.load()));
    masterGainRamp.snapToTarget();

    for (auto& ramp : trackLevelRamp)
    {
        ramp.prepare (sampleRate);
        ramp.setTimeSeconds (0.02f);
        ramp.setTarget (1.0f);
        ramp.snapToTarget();
    }

    samplesPerStepCache = 60.0 / 120.0 * sampleRate * 0.25;
    lastEventCount = 0;
}

void Engine::reset()
{
    sequencer.reset();
    synth.reset();
    masterChain.reset();
    focusChain.reset();
    for (int t = 0; t < kMaxTracks; ++t)
    {
        std::fill (trackBuffer[t][0].begin(), trackBuffer[t][0].end(), 0.0f);
        std::fill (trackBuffer[t][1].begin(), trackBuffer[t][1].end(), 0.0f);
    }
    std::fill (mixL.begin(), mixL.end(), 0.0f);
    std::fill (mixR.begin(), mixR.end(), 0.0f);
    lastEventCount = 0;
    outputPeak.store (0.0f);
}

FxSettings& Engine::getFxSettings (int fxIndex) noexcept
{
    return fxSettings[clamp (fxIndex, 0, kMaxFx - 1)];
}

const FxSettings& Engine::getFxSettings (int fxIndex) const noexcept
{
    return fxSettings[clamp (fxIndex, 0, kMaxFx - 1)];
}

void Engine::playNote (int track, int note, float velocity)
{
    track = clamp (track, 0, kMaxTracks - 1);
    const Track& t = pattern.tracks[track];

    StepEvent event;
    event.track       = track;
    event.step        = -1;
    event.note        = clamp (note, 0, 127);
    event.velocity    = clamp (velocity, 0.0f, 1.0f);
    event.gateSteps   = 2.0f;
    event.engine      = t.engine.load (std::memory_order_relaxed);
    event.cutoff      = t.cutoff.load (std::memory_order_relaxed);
    event.resonance   = t.resonance.load (std::memory_order_relaxed);
    event.decay       = t.decay.load (std::memory_order_relaxed);
    event.pan         = t.pan.load (std::memory_order_relaxed);
    event.level       = t.level.load (std::memory_order_relaxed);
    event.noiseLayer  = t.noiseOn.load (std::memory_order_relaxed);

    synth.startNote (event, sequencer.getSamplesPerStep());
}

void Engine::applyFxRouting()
{
    for (int f = 0; f < kMaxFx; ++f)
    {
        FxSettings& s = fxSettings[f];

        const bool engaged      = s.engaged.load (std::memory_order_relaxed);
        const bool onFocusTrack = s.target.load (std::memory_order_relaxed)
                                    == static_cast<int> (FxTarget::FocusTrack);
        const float amount      = s.amount.load (std::memory_order_relaxed);
        const float paramA      = s.paramA.load (std::memory_order_relaxed);
        const float paramB      = s.paramB.load (std::memory_order_relaxed);
        const float ramp        = s.rampSeconds.load (std::memory_order_relaxed);

        PunchInFx& target = onFocusTrack ? focusChain[f] : masterChain[f];
        PunchInFx& other  = onFocusTrack ? masterChain[f] : focusChain[f];

        for (PunchInFx* fx : { &target, &other })
        {
            fx->setAmount (amount);
            fx->setRampSeconds (ramp);
            fx->setPunchParam (0, paramA);
            fx->setPunchParam (1, paramB);
        }

        target.setEngaged (engaged);
        other.setEngaged (false);
    }
}

void Engine::renderVoices (int startSample, int count)
{
    if (count <= 0)
        return;

    for (int t = 0; t < kMaxTracks; ++t)
        synth.renderTrack (t, trackBuffer[t][0].data(), trackBuffer[t][1].data(), startSample, count);
}

void Engine::process (float* const* outputChannels,
                      int numChannels,
                      int numSamples,
                      const TransportInfo& transport)
{
    lastEventCount = 0;

    if (outputChannels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
        if (outputChannels[ch] != nullptr)
            std::fill (outputChannels[ch], outputChannels[ch] + numSamples, 0.0f);

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min (numSamples - offset, maxBlockSize);

        TransportInfo chunkTransport = transport;
        if (transport.hasPpq && transport.bpm > 1.0)
        {
            const double secondsPerBeat = 60.0 / transport.bpm;
            chunkTransport.ppqAtBlockStart += (static_cast<double> (offset) / sampleRate) / secondsPerBeat;
        }

        processChunk (outputChannels, numChannels, offset, chunk, chunkTransport);
        offset += chunk;
    }
}

void Engine::processChunk (float* const* outputChannels,
                           int numChannels,
                           int outputOffset,
                           int numSamples,
                           const TransportInfo& transport)
{
    // ---- 1. Clear per-track buffers -----------------------------------------
    for (int t = 0; t < kMaxTracks; ++t)
    {
        std::fill (trackBuffer[t][0].begin(), trackBuffer[t][0].begin() + numSamples, 0.0f);
        std::fill (trackBuffer[t][1].begin(), trackBuffer[t][1].begin() + numSamples, 0.0f);
    }

    // ---- 2. Momentary FX routing / parameter updates ------------------------
    applyFxRouting();

    // ---- 3. Sequencer events for this chunk ---------------------------------
    StepEvent events[kMaxEventsPerBlock];
    const int numEvents = sequencer.process (pattern, transport, numSamples, events, kMaxEventsPerBlock);

    for (int i = 0; i < numEvents && lastEventCount < kMaxEventsPerBlock; ++i)
    {
        lastEvents[lastEventCount] = events[i];
        lastEvents[lastEventCount].sampleOffset += outputOffset;
        ++lastEventCount;
    }

    // ---- 4. Render voices, starting notes at their exact sample offsets ------
    samplesPerStepCache = sequencer.getSamplesPerStep();

    int pos = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const int eventOffset = clamp (events[i].sampleOffset, 0, numSamples - 1);
        if (eventOffset > pos)
        {
            renderVoices (pos, eventOffset - pos);
            pos = eventOffset;
        }
        synth.startNote (events[i], samplesPerStepCache);
    }
    renderVoices (pos, numSamples - pos);

    // ---- 5. Focus-track punch-in FX ----------------------------------------
    const int focus = clamp (focusTrack.load (std::memory_order_relaxed), 0, kMaxTracks - 1);
    focusChain.process (trackBuffer[focus][0].data(), trackBuffer[focus][1].data(), numSamples);

    // ---- 6. Sum tracks into the master bus ---------------------------------
    masterGainRamp.setTarget (dbToGain (masterGainDb.load (std::memory_order_relaxed)));

    for (int t = 0; t < kMaxTracks; ++t)
    {
        const float level = pattern.tracks[t].muted.load (std::memory_order_relaxed)
                              ? 0.0f
                              : pattern.tracks[t].level.load (std::memory_order_relaxed);
        Ramp& ramp = trackLevelRamp[t];
        ramp.setTarget (level);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;
        for (int t = 0; t < kMaxTracks; ++t)
        {
            const float g = trackLevelRamp[t].next();
            sumL += trackBuffer[t][0][static_cast<std::size_t> (i)] * g;
            sumR += trackBuffer[t][1][static_cast<std::size_t> (i)] * g;
        }
        mixL[static_cast<std::size_t> (i)] = sumL;
        mixR[static_cast<std::size_t> (i)] = sumR;
    }

    // ---- 7. Master punch-in FX ---------------------------------------------
    masterChain.process (mixL.data(), mixR.data(), numSamples);

    // ---- 8. Master gain and output ----------------------------------------
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGainRamp.next();
        float l = softClip (sanitise (mixL[static_cast<std::size_t> (i)] * g));
        float r = softClip (sanitise (mixR[static_cast<std::size_t> (i)] * g));

        peak = std::max (peak, std::max (std::abs (l), std::abs (r)));

        if (numChannels == 1)
        {
            outputChannels[0][outputOffset + i] += (l + r) * 0.5f;
        }
        else
        {
            outputChannels[0][outputOffset + i] += l;
            outputChannels[1][outputOffset + i] += r;

            for (int ch = 2; ch < numChannels; ++ch)
                if (outputChannels[ch] != nullptr)
                    outputChannels[ch][outputOffset + i] += (ch % 2 == 0 ? l : r);
        }
    }

    // Decaying peak meter (UI reads this; no smoothing needed on the audio side).
    const float previousPeak = outputPeak.load (std::memory_order_relaxed);
    outputPeak.store (std::max (peak, previousPeak * 0.85f), std::memory_order_relaxed);
}

} // namespace opxy
