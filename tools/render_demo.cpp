// -----------------------------------------------------------------------------
//  Offline demo renderer.
//
//  Builds a groove on the 8-track engine and renders it to a WAV, punching in
//  every effect along the way. This exists so the sound can be checked without
//  building the plugin, opening a DAW, or owning a JUCE toolchain.
//
//      cmake --build build/core --target opxy_demo
//      ./build/core/opxy_demo demo/opxy_demo.wav
//
//  Everything here is deterministic: same input, same WAV.
// -----------------------------------------------------------------------------

#include "Engine.h"
#include "Pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{

constexpr double kSampleRate = 48000.0;
constexpr double kBpm = 120.0;
constexpr double kSecondsPerBeat = 60.0 / kBpm;
constexpr int kBarBeats = 4;
constexpr int kNumBars = 16;

struct PunchEvent
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    int fx = 0;
    opxy::FxTarget target = opxy::FxTarget::Master;
    const char* note = "";
};

struct Section
{
    int bar = 0;
    const char* name = "";
};

double barTime (int bar) { return bar * kBarBeats * kSecondsPerBeat; }

float limitToUnity (float value)
{
    return value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
}

void writeWav (const std::string& path, const std::vector<float>& left, const std::vector<float>& right)
{
    const int numFrames = static_cast<int> (left.size());
    const int channels = 2;
    const int bitsPerSample = 16;
    const int bytesPerFrame = channels * bitsPerSample / 8;
    const int dataSize = numFrames * bytesPerFrame;

    std::FILE* file = std::fopen (path.c_str(), "wb");
    if (file == nullptr)
    {
        std::printf ("could not open %s for writing\n", path.c_str());
        return;
    }

    const auto write32 = [file] (std::uint32_t value)
    {
        const std::uint8_t bytes[4] = { static_cast<std::uint8_t> (value & 0xFF),
                                        static_cast<std::uint8_t> ((value >> 8) & 0xFF),
                                        static_cast<std::uint8_t> ((value >> 16) & 0xFF),
                                        static_cast<std::uint8_t> ((value >> 24) & 0xFF) };
        std::fwrite (bytes, 1, 4, file);
    };

    const auto write16 = [file] (std::uint16_t value)
    {
        const std::uint8_t bytes[2] = { static_cast<std::uint8_t> (value & 0xFF),
                                        static_cast<std::uint8_t> ((value >> 8) & 0xFF) };
        std::fwrite (bytes, 1, 2, file);
    };

    std::fwrite ("RIFF", 1, 4, file);
    write32 (static_cast<std::uint32_t> (36 + dataSize));
    std::fwrite ("WAVE", 1, 4, file);

    std::fwrite ("fmt ", 1, 4, file);
    write32 (16);
    write16 (1);                                            // PCM
    write16 (static_cast<std::uint16_t> (channels));
    write32 (static_cast<std::uint32_t> (kSampleRate));
    write32 (static_cast<std::uint32_t> (kSampleRate * bytesPerFrame));
    write16 (static_cast<std::uint16_t> (bytesPerFrame));
    write16 (static_cast<std::uint16_t> (bitsPerSample));

    std::fwrite ("data", 1, 4, file);
    write32 (static_cast<std::uint32_t> (dataSize));

    for (int i = 0; i < numFrames; ++i)
    {
        const float l = limitToUnity (left[static_cast<std::size_t> (i)]);
        const float r = limitToUnity (right[static_cast<std::size_t> (i)]);
        const auto li = static_cast<std::int16_t> (std::lround (l * 32767.0f));
        const auto ri = static_cast<std::int16_t> (std::lround (r * 32767.0f));
        write16 (static_cast<std::uint16_t> (li));
        write16 (static_cast<std::uint16_t> (ri));
    }

    std::fclose (file);
    std::printf ("wrote %s  (%.1f s, %d frames)\n", path.c_str(),
                 static_cast<double> (numFrames) / kSampleRate, numFrames);
}

void setSteps (opxy::Track& track, std::initializer_list<int> steps, float note, float gate, float velocity)
{
    for (int step : steps)
    {
        if (step < 0 || step >= opxy::kMaxSteps)
            continue;
        auto& s = track.steps[step];
        s.active.store (true);
        s.note.store (note);
        s.gate.store (gate);
        s.velocity.store (velocity);
    }
}

} // namespace

int main (int argc, char** argv)
{
    const std::string outputPath = argc > 1 ? argv[1] : "demo/opxy_demo.wav";

    opxy::Engine engine;
    engine.prepare (kSampleRate, 256);
    engine.setMasterGainDb (-2.0f);
    engine.setFocusTrack (3);

    auto& pattern = engine.getPattern();

    // --- Track 1: kick -------------------------------------------------------
    {
        auto& track = pattern.tracks[0];
        track.engine.store (static_cast<int> (opxy::EngineType::Sine));
        track.decay.store (0.26f);
        track.cutoff.store (1200.0f);
        track.resonance.store (0.1f);
        track.level.store (0.95f);
        setSteps (track, { 0, 4, 8, 12 }, 36.0f, 0.4f, 1.0f);
        track.steps[14].active.store (true);
        track.steps[14].note.store (43.0f);
        track.steps[14].gate.store (0.2f);
        track.steps[14].velocity.store (0.6f);
    }

    // --- Track 2: bass -------------------------------------------------------
    {
        auto& track = pattern.tracks[1];
        track.engine.store (static_cast<int> (opxy::EngineType::Saw));
        track.decay.store (0.18f);
        track.cutoff.store (620.0f);
        track.resonance.store (0.45f);
        track.level.store (0.5f);
        track.pan.store (-0.1f);
        setSteps (track, { 3, 6, 10, 14 }, 36.0f, 0.5f, 0.9f);
        track.steps[6].note.store (36.0f);
        track.steps[10].note.store (43.0f);
        track.steps[14].note.store (41.0f);
        track.steps[10].setPLock (opxy::PlockParam::Cutoff, 1400.0f);
    }

    // --- Track 3: hats -------------------------------------------------------
    {
        auto& track = pattern.tracks[2];
        track.engine.store (static_cast<int> (opxy::EngineType::Noise));
        track.decay.store (0.05f);
        track.cutoff.store (11000.0f);
        track.resonance.store (0.05f);
        track.level.store (0.22f);
        for (int step = 0; step < 16; ++step)
        {
            auto& s = track.steps[step];
            s.active.store (true);
            s.gate.store (0.12f);
            s.velocity.store (step % 4 == 2 ? 0.9f : (step % 2 == 0 ? 0.5f : 0.28f));
            if (step == 7 || step == 15)
                s.ratchet.store (3);
        }
    }

    // --- Track 4: lead with p-locks -----------------------------------------
    {
        auto& track = pattern.tracks[3];
        track.engine.store (static_cast<int> (opxy::EngineType::Square));
        track.decay.store (0.28f);
        track.cutoff.store (2400.0f);
        track.resonance.store (0.2f);
        track.level.store (0.3f);
        track.pan.store (0.15f);

        const int notes[8] = { 60, 63, 67, 70, 67, 63, 60, 58 };
        const int steps[8] = { 0, 2, 5, 8, 11, 13, 16, 19 };

        for (int i = 0; i < 8; ++i)
        {
            auto& s = track.steps[steps[i]];
            s.active.store (true);
            s.note.store (static_cast<float> (notes[i]));
            s.gate.store (0.4f);
            s.velocity.store (i % 2 == 0 ? 0.9f : 0.65f);
            s.setPLock (opxy::PlockParam::Cutoff, i % 2 == 0 ? 4200.0f : 1100.0f);
            if (i == 4)
                s.setPLock (opxy::PlockParam::Resonance, 0.75f);
            if (i == 6)
                s.setPLock (opxy::PlockParam::Pitch, 12.0f);
            if (i == 7)
                s.setPLock (opxy::PlockParam::Pan, -0.7f);
        }
        track.length.store (32);
    }

    // --- Track 5: clap -------------------------------------------------------
    {
        auto& track = pattern.tracks[4];
        track.engine.store (static_cast<int> (opxy::EngineType::Noise));
        track.decay.store (0.14f);
        track.cutoff.store (3200.0f);
        track.level.store (0.26f);
        track.pan.store (0.2f);
        setSteps (track, { 4, 12 }, 60.0f, 0.2f, 0.8f);
    }

    // --- Track 6: pad, Euclidean --------------------------------------------
    {
        auto& track = pattern.tracks[5];
        track.engine.store (static_cast<int> (opxy::EngineType::Triangle));
        track.decay.store (1.2f);
        track.cutoff.store (1800.0f);
        track.level.store (0.16f);
        track.pan.store (-0.35f);
        track.euclidOn.store (true);
        track.euclidHits.store (3);
        track.steps[0].note.store (48.0f);
        track.steps[0].velocity.store (0.7f);
        track.steps[0].gate.store (2.0f);
        for (int step = 0; step < 16; ++step)
            track.steps[step].note.store (48.0f);
        for (int step = 0; step < 16; ++step)
            track.steps[step].gate.store (1.5f);
    }

    for (int t = 6; t < opxy::kMaxTracks; ++t)
    {
        pattern.tracks[t].level.store (0.0f);
        pattern.tracks[t].length.store (16);
    }

    // --- FX macro setup ------------------------------------------------------
    auto& tape   = engine.getFxSettings (static_cast<int> (opxy::FxType::TapeStop));
    auto& delay  = engine.getFxSettings (static_cast<int> (opxy::FxType::DelayThrow));
    auto& filter = engine.getFxSettings (static_cast<int> (opxy::FxType::FilterSweep));
    auto& stutter = engine.getFxSettings (static_cast<int> (opxy::FxType::Stutter));
    auto& reverb = engine.getFxSettings (static_cast<int> (opxy::FxType::ReverbBurst));
    auto& crush  = engine.getFxSettings (static_cast<int> (opxy::FxType::Bitcrush));

    tape.amount.store (1.0f);      tape.paramA.store (0.22f);  tape.paramB.store (0.18f);
    delay.amount.store (0.85f);    delay.paramA.store (0.28f); delay.paramB.store (0.5f);
    filter.amount.store (1.0f);    filter.paramA.store (0.8f); filter.paramB.store (0.7f);
    stutter.amount.store (1.0f);   stutter.paramA.store (0.14f); stutter.paramB.store (0.45f);
    reverb.amount.store (0.9f);    reverb.paramA.store (0.75f); reverb.paramB.store (0.35f);
    crush.amount.store (0.95f);    crush.paramA.store (0.75f); crush.paramB.store (0.6f);

    tape.target.store (static_cast<int> (opxy::FxTarget::Master));
    delay.target.store (static_cast<int> (opxy::FxTarget::Master));
    filter.target.store (static_cast<int> (opxy::FxTarget::Master));
    stutter.target.store (static_cast<int> (opxy::FxTarget::Master));
    reverb.target.store (static_cast<int> (opxy::FxTarget::Master));
    crush.target.store (static_cast<int> (opxy::FxTarget::Master));

    // --- sequenced punch FX ---------------------------------------------------
    // These are driven by the pattern's punch lanes rather than by the punch
    // list below: the sequencer fires them, which is the workflow this plugin
    // exists for. Bars 13-14 stutter the whole mix in 16ths; bar 15 tape-stops
    // on the "and" of every beat; bar 16 stays clean for the outro gesture.
    const int stutterLane = static_cast<int> (opxy::FxType::Stutter);
    const int tapeLane    = static_cast<int> (opxy::FxType::TapeStop);
    const int crushLane   = static_cast<int> (opxy::FxType::Bitcrush);

    pattern.setFxLaneEnabled (stutterLane, true);
    for (int step = 0; step < 16; ++step)
        pattern.setFxLaneStep (stutterLane, step, (step % 2 == 0) ? 1.0f : 0.0f);

    pattern.setFxLaneEnabled (tapeLane, true);
    for (int step = 0; step < 16; ++step)
        pattern.setFxLaneStep (tapeLane, step, (step % 8 == 3) ? 1.0f : 0.0f);

    pattern.setFxLaneEnabled (crushLane, true);
    pattern.setFxLaneStep (crushLane, 2, 0.8f);
    pattern.setFxLaneStep (crushLane, 6, 0.5f);

    // The lanes only drive FX while their own bar window is open, so gate them
    // with a per-bar enable below (bars 13-16).
    const auto applyLanes = [&] (int bar)
    {
        const bool lanesOpen = bar >= 12 && bar <= 15;
        pattern.setFxLaneEnabled (stutterLane, lanesOpen && bar == 12);
        pattern.setFxLaneEnabled (tapeLane, lanesOpen && bar == 14);
        pattern.setFxLaneEnabled (crushLane, lanesOpen && bar == 13);
    };

    // --- the performance -----------------------------------------------------
    std::vector<PunchEvent> punches = {
        { barTime (6) + 2 * kSecondsPerBeat, barTime (6) + 3.6 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::TapeStop), opxy::FxTarget::Master, "tape stop" },
        { barTime (7), barTime (7) + 2.2 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::DelayThrow), opxy::FxTarget::Master, "delay throw" },
        { barTime (8), barTime (8) + 3.2 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::FilterSweep), opxy::FxTarget::Master, "filter sweep" },
        { barTime (9) + 0.5 * kSecondsPerBeat, barTime (9) + 1.0 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::Stutter), opxy::FxTarget::Master, "stutter 1" },
        { barTime (9) + 1.4 * kSecondsPerBeat, barTime (9) + 2.0 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::Stutter), opxy::FxTarget::Master, "stutter 2" },
        { barTime (9) + 2.6 * kSecondsPerBeat, barTime (9) + 3.1 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::Stutter), opxy::FxTarget::Master, "stutter 3" },
        { barTime (10), barTime (10) + 2.0 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::ReverbBurst), opxy::FxTarget::Master, "reverb burst" },
        { barTime (11), barTime (11) + 1.8 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::Bitcrush), opxy::FxTarget::Master, "bitcrush" },
        { barTime (12), barTime (13.5),
          static_cast<int> (opxy::FxType::FilterSweep), opxy::FxTarget::FocusTrack, "lead filter sweep" },
        { barTime (15) + 2 * kSecondsPerBeat, barTime (15) + 3.4 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::TapeStop), opxy::FxTarget::Master, "final tape stop" },
        { barTime (15) + 3.5 * kSecondsPerBeat, barTime (16) + 1.0 * kSecondsPerBeat,
          static_cast<int> (opxy::FxType::ReverbBurst), opxy::FxTarget::Master, "final reverb" },
    };

    // Brains kick in for the last four bars.
    const double brainsStart = barTime (12);

    // Mute arrangement: build the groove up over the first four bars.
    const auto muteStateAt = [] (double seconds, int track)
    {
        const int bar = static_cast<int> (seconds / (kBarBeats * kSecondsPerBeat));
        switch (track)
        {
            case 0: return false;                   // kick from bar 1
            case 2: return false;                   // hats from bar 1
            case 1: return bar < 2;                 // bass from bar 3
            case 3: return bar < 4;                 // lead from bar 5
            case 4: return bar < 4;                 // clap from bar 5
            case 5: return bar < 8;                 // pad from bar 9
            default: return true;
        }
    };

    // --- render --------------------------------------------------------------
    const int totalSamples = static_cast<int> (barTime (kNumBars + 1) * kSampleRate);
    std::vector<float> left (static_cast<std::size_t> (totalSamples), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (totalSamples), 0.0f);

    const int blockSize = 256;
    std::vector<float> blockL (static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float> blockR (static_cast<std::size_t> (blockSize), 0.0f);
    float* channels[2] = { blockL.data(), blockR.data() };

    double peak = 0.0;
    int reportedSection = -1;

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = std::min (blockSize, totalSamples - pos);
        const double seconds = static_cast<double> (pos) / kSampleRate;
        const int bar = static_cast<int> (seconds / (kBarBeats * kSecondsPerBeat));

        if (bar != reportedSection)
        {
            reportedSection = bar;
            std::printf ("  bar %2d  %5.1fs\n", bar + 1, seconds);
        }

        // Host-style transport: report ppq so the host-sync path is exercised.
        opxy::TransportInfo transport;
        transport.playing = true;
        transport.bpm = kBpm;
        transport.hasPpq = true;
        transport.ppqAtBlockStart = seconds / kSecondsPerBeat;

        // Arrangement mutes + brains.
        for (int t = 0; t < 6; ++t)
            pattern.tracks[t].muted.store (muteStateAt (seconds, t));

        if (seconds >= brainsStart)
        {
            pattern.tracks[1].euclidOn.store (true);
            pattern.tracks[1].euclidHits.store (5);
            pattern.tracks[2].probability.store (0.62f);
            pattern.tracks[3].randomPitch.store (0.0f);
            pattern.tracks[3].scaleType.store (static_cast<int> (opxy::ScaleType::Minor));
            pattern.tracks[3].scaleRoot.store (0);
            pattern.tracks[3].ratchet.store (2);
            pattern.tracks[4].probability.store (0.8f);
        }
        else
        {
            pattern.tracks[1].euclidOn.store (false);
            pattern.tracks[2].probability.store (1.0f);
            pattern.tracks[3].scaleType.store (static_cast<int> (opxy::ScaleType::Off));
            pattern.tracks[3].ratchet.store (1);
            pattern.tracks[4].probability.store (1.0f);
        }

        // Punch engagement (manual gestures) and the sequenced lanes.
        for (int f = 0; f < opxy::kMaxFx; ++f)
            engine.getFxSettings (f).engaged.store (false);

        applyLanes (bar);

        for (const auto& punch : punches)
            if (seconds >= punch.startSeconds && seconds < punch.endSeconds)
                engine.getFxSettings (punch.fx).engaged.store (true);

        std::fill (blockL.begin(), blockL.end(), 0.0f);
        std::fill (blockR.begin(), blockR.end(), 0.0f);
        engine.process (channels, 2, n, transport);

        for (int i = 0; i < n; ++i)
        {
            const float l = blockL[static_cast<std::size_t> (i)];
            const float r = blockR[static_cast<std::size_t> (i)];
            left[static_cast<std::size_t> (pos + i)] = l;
            right[static_cast<std::size_t> (pos + i)] = r;
            peak = std::max (peak, static_cast<double> (std::max (std::abs (l), std::abs (r))));
        }
    }

    // --- normalise if needed -------------------------------------------------
    if (peak > 0.0)
    {
        const float gain = static_cast<float> (0.9 / peak);
        for (auto& sample : left)  sample *= gain;
        for (auto& sample : right) sample *= gain;
        std::printf ("normalised by %.2f dB\n", 20.0 * std::log10 (static_cast<double> (gain)));
    }

    std::printf ("\narrangement:\n");
    std::printf ("  bars 1-2   kick + hats\n");
    std::printf ("  bars 3-4   + bass\n");
    std::printf ("  bars 5-6   + lead (p-locked cutoff/resonance/pitch) + clap\n");
    std::printf ("\nsequenced punch lanes (fired by the step sequencer, not by hand):\n");
    std::printf ("  bar 13   stutter lane: every 16th (whole mix)\n");
    std::printf ("  bar 14   bitcrush lane: steps 3 and 7\n");
    std::printf ("  bar 15   tape stop lane: the 'and' of each beat (step 4, 12)\n");
    std::printf ("\nmanual punches:\n");
    for (const auto& punch : punches)
        std::printf ("  bar %2d    %s punch (%.2fs - %.2fs)\n",
                     static_cast<int> (punch.startSeconds / (kBarBeats * kSecondsPerBeat)) + 1,
                     punch.note, punch.startSeconds, punch.endSeconds);
    std::printf ("  bars 13-16 brains: euclidean bass, probability hats, ratchet + minor scale lead\n");

    writeWav (outputPath, left, right);
    return 0;
}
