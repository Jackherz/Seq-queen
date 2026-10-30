#include "SynthEngine.h"

#include <cmath>

namespace opxy
{

void SynthEngine::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;

    voices.resize (static_cast<std::size_t> (kMaxVoices));
    for (int i = 0; i < kMaxVoices; ++i)
    {
        Voice& v = voices[static_cast<std::size_t> (i)];
        v.active = false;
        v.voiceIndex = i;
        v.osc.prepare (sampleRate);
        v.filterL.prepare (sampleRate);
        v.filterR.prepare (sampleRate);
        v.env.prepare (sampleRate);
        v.dcL.prepare (sampleRate);
        v.dcR.prepare (sampleRate);
    }
}

void SynthEngine::reset()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.osc.reset();
        v.filterL.reset();
        v.filterR.reset();
        v.env.reset();
        v.dcL.reset();
        v.dcR.reset();
    }
}

int SynthEngine::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& v : voices)
        if (v.active)
            ++count;
    return count;
}

bool SynthEngine::anyVoiceActive() const
{
    for (const auto& v : voices)
        if (v.active)
            return true;
    return false;
}

Voice& SynthEngine::findFreeVoice()
{
    // Prefer a free voice, newest-first so the pool ages fairly.
    for (auto& v : voices)
        if (! v.active)
            return v;

    // Otherwise steal the oldest voice.
    Voice* oldest = &voices[0];
    for (auto& v : voices)
        if (v.age > oldest->age)
            oldest = &v;
    return *oldest;
}

void SynthEngine::startNote (const StepEvent& event, double samplesPerStep)
{
    Voice& v = findFreeVoice();

    v.active  = true;
    v.track   = clamp (event.track, 0, kMaxTracks - 1);
    v.age     = 0.0f;
    v.engine  = static_cast<EngineType> (clamp (event.engine, 0, static_cast<int> (EngineType::Count) - 1));
    v.level   = clamp (event.level, 0.0f, 2.0f);
    v.velocityGain = std::pow (clamp (event.velocity, 0.0f, 1.0f), 1.4f);

    v.osc.prepare (sampleRate);
    v.osc.setType (v.engine);
    v.osc.setFrequency (midiNoteToHz (static_cast<float> (event.note)));
    v.osc.randomisePhase (v.rng);

    v.filterL.reset();
    v.filterR.reset();
    v.dcL.reset();
    v.dcR.reset();
    v.filterL.setParams (event.cutoff, event.resonance);
    v.filterR.setParams (event.cutoff, event.resonance);

    v.noiseAmount = event.noiseLayer ? 0.6f : 0.0f;

    const float decay = clamp (event.decay, 0.01f, 20.0f);
    // Percussive by design: near-instant attack, exponential-ish decay to zero,
    // plus a fixed short release so gated notes stop cleanly.
    v.env.noteOn (0.002f, decay, 0.0f);
    v.releaseSeconds = 0.06f;

    panGains (event.pan, v.panL, v.panR);

    v.gateSamples  = std::max (16.0f, static_cast<float> (event.gateSteps * samplesPerStep));
    v.elapsed      = 0.0f;
    v.gateReleased = false;

    ++voiceCounter;
}

void SynthEngine::renderTrack (int track, float* left, float* right, int startSample, int count)
{
    if (count <= 0 || left == nullptr || right == nullptr)
        return;

    for (auto& v : voices)
    {
        if (! v.active || v.track != track)
            continue;

        const float amp = v.level * v.velocityGain * 0.55f; // leaves headroom for 8 tracks

        for (int i = 0; i < count; ++i)
        {
            if (v.gateReleased && ! v.env.isActive())
            {
                v.active = false;
                break;
            }

            const float noise = v.rng.nextBipolar();
            float sample = v.osc.process (v.engine, noise);
            if (v.noiseAmount > 0.0f)
                sample = sample * (1.0f - v.noiseAmount) + noise * v.noiseAmount;

            float sL = v.dcL.process (v.filterL.process (sample, Svf::Mode::LowPass));
            float sR = v.dcR.process (v.filterR.process (sample, Svf::Mode::LowPass));

            const float envValue = v.env.next();
            const float gain = amp * envValue;

            const int index = startSample + i;
            left[index]  += sL * gain * v.panL;
            right[index] += sR * gain * v.panR;

            v.elapsed += 1.0f;
            if (! v.gateReleased && v.elapsed >= v.gateSamples)
            {
                v.env.noteOff (v.releaseSeconds);
                v.gateReleased = true;
            }
        }

        v.age += static_cast<float> (count);
    }
}

} // namespace opxy
