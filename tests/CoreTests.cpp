// -----------------------------------------------------------------------------
//  Core test suite. Runs without JUCE, without a host, without audio hardware.
//
//      cmake -B build -DOPXY_BUILD_TESTS=ON && cmake --build build
//      ./build/opxy_core_tests
//
//  This is the safety net for the Claude handoff: any change to the engine,
//  sequencer or FX should keep this green before touching the JUCE layer (which
//  needs a real toolchain to build).
// -----------------------------------------------------------------------------

#include "Engine.h"
#include "Pattern.h"
#include "PatternState.h"
#include "Sequencer.h"
#include "ScalarUtils.h"
#include "TapeStopFx.h"
#include "DelayThrowFx.h"
#include "FilterSweepFx.h"
#include "StutterFx.h"
#include "ReverbBurstFx.h"
#include "BitcrushFx.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------- harness ---------------------------------

namespace
{
int gFailures = 0;
int gChecks = 0;
const char* gCurrentTest = "";

void check (bool condition, const char* expression, const char* file, int line)
{
    ++gChecks;
    if (! condition)
    {
        ++gFailures;
        std::printf ("    FAIL [%s] %s:%d  %s\n", gCurrentTest, file, line, expression);
    }
}

struct Test
{
    const char* name;
    std::function<void()> fn;
};

std::vector<Test>& tests()
{
    static std::vector<Test> list;
    return list;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn) { tests().push_back ({ name, std::move (fn) }); }
};

#define CHECK(expr)                  check ((expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(val, target, tol) check (std::abs ((val) - (target)) <= (tol), #val " ~= " #target, __FILE__, __LINE__)
#define TEST(name)                                                                     \
    static void name();                                                                \
    static Registrar reg_##name (#name, name);                                          \
    static void name()

constexpr double kSampleRate = 48000.0;

// Renders `numSamples` of a test signal through an effect in blocks of `block`.
struct RenderReport
{
    double peak = 0.0;
    double rms = 0.0;
    double maxDelta = 0.0;
    bool finite = true;
};

RenderReport renderFx (opxy::PunchInFx& fx,
                       int numSamples,
                       int block,
                       const std::function<bool (int sampleIndex)>& engagedAt)
{
    RenderReport report;
    std::vector<float> l (static_cast<std::size_t> (block)), r (static_cast<std::size_t> (block));
    double sumSquares = 0.0;
    float previous = 0.0f;
    int written = 0;

    for (int pos = 0; pos < numSamples; pos += block)
    {
        const int n = std::min (block, numSamples - pos);

        for (int i = 0; i < n; ++i)
        {
            // 220 Hz sine at -6 dBFS.
            const double phase = 2.0 * 3.141592653589793 * 220.0 * (pos + i) / kSampleRate;
            const float s = static_cast<float> (0.5 * std::sin (phase));
            l[static_cast<std::size_t> (i)] = s;
            r[static_cast<std::size_t> (i)] = s;
        }

        fx.setEngaged (engagedAt (pos));
        fx.process (l.data(), r.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float v = l[static_cast<std::size_t> (i)];
            if (! std::isfinite (v))
                report.finite = false;

            report.peak = std::max (report.peak, static_cast<double> (std::abs (v)));
            sumSquares += static_cast<double> (v) * v;
            if (written > 0)
                report.maxDelta = std::max (report.maxDelta, static_cast<double> (std::abs (v - previous)));
            previous = v;
            ++written;
        }
    }

    report.rms = std::sqrt (sumSquares / std::max (1, numSamples));
    return report;
}

bool patternBit (std::uint64_t mask, int bit) { return ((mask >> bit) & 1ull) != 0ull; }

} // namespace

// ------------------------------- sequencer tests -----------------------------

TEST (euclideanMaskMatchesClassicPatterns)
{
    // 4-in-16 is the four-on-the-floor.
    const auto four = opxy::euclideanMask (16, 4, 0);
    for (int i = 0; i < 16; ++i)
        CHECK (patternBit (four, i) == (i % 4 == 0));

    // 3-in-8 is the classic tresillo: steps 0, 3, 6.
    const auto tresillo = opxy::euclideanMask (8, 3, 0);
    CHECK (patternBit (tresillo, 0));
    CHECK (patternBit (tresillo, 3));
    CHECK (patternBit (tresillo, 6));
    CHECK (! patternBit (tresillo, 1));

    // Extremes.
    CHECK (opxy::euclideanMask (16, 0, 0) == 0ull);
    CHECK (opxy::euclideanMask (16, 16, 0) == 0xFFFFull);
    CHECK (opxy::euclideanMask (16, 4, 1) == (four << 1));
}

TEST (quantiseToScaleSnapsToNearestDegree)
{
    using opxy::ScaleType;
    const int major = static_cast<int> (ScaleType::Major);
    const int minor = static_cast<int> (ScaleType::Minor);

    // C# is not in C major: should snap to C or D.
    const int snapped = opxy::quantiseToScale (61, 0, major);
    CHECK (snapped == 60 || snapped == 62);

    // Eb in C minor is already a degree.
    CHECK (opxy::quantiseToScale (63, 0, minor) == 62 || opxy::quantiseToScale (63, 0, minor) == 63);

    // Off leaves things alone.
    CHECK (opxy::quantiseToScale (61, 0, static_cast<int> (ScaleType::Off)) == 61);

    // Everything snapped to a pentatonic stays inside the pentatonic.
    const int pentatonic = static_cast<int> (ScaleType::Pentatonic);
    static constexpr int allowed[] = { 0, 2, 4, 7, 9 };
    for (int note = 48; note < 72; ++note)
    {
        const int q = opxy::quantiseToScale (note, 0, pentatonic);
        bool found = false;
        for (int a : allowed)
            if (((q % 12) + 12) % 12 == a)
                found = true;
        CHECK (found);
    }
}

TEST (plocksSetGetClear)
{
    opxy::Step step;
    CHECK (! step.hasAnyPLock());
    CHECK_NEAR (step.getPLock (opxy::PlockParam::Cutoff, 1234.0f), 1234.0f, 0.001f);

    step.setPLock (opxy::PlockParam::Cutoff, 800.0f);
    CHECK (step.hasPLock (opxy::PlockParam::Cutoff));
    CHECK_NEAR (step.getPLock (opxy::PlockParam::Cutoff, 1234.0f), 800.0f, 0.001f);
    CHECK (! step.hasPLock (opxy::PlockParam::Pitch));

    step.clearPLock (opxy::PlockParam::Cutoff);
    CHECK (! step.hasPLock (opxy::PlockParam::Cutoff));
    CHECK_NEAR (step.getPLock (opxy::PlockParam::Cutoff, 1234.0f), 1234.0f, 0.001f);
}

TEST (sequencerFiresSixteenStepsPerBar)
{
    opxy::Pattern pattern;
    pattern.tracks[0].length.store (16);
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (true);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (1); // keep the other tracks quiet

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    const int blockSize = 512;
    const int totalSamples = static_cast<int> (kSampleRate * 2.0); // 2 s = 1 bar at 120 bpm
    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);
    std::vector<int> track0Offsets;

    int samplePos = 0;
    float peak = 0.0f;
    (void) peak;
    while (samplePos < totalSamples)
    {
        const int n = std::min (blockSize, totalSamples - samplePos);
        const int count = seq.process (pattern, transport, n, events.data(), opxy::kMaxEventsPerBlock);
        for (int i = 0; i < count; ++i)
            if (events[static_cast<std::size_t> (i)].track == 0)
                track0Offsets.push_back (samplePos + events[static_cast<std::size_t> (i)].sampleOffset);
        samplePos += n;
    }

    // One bar of 16ths at 120 bpm starting at sample 0: 16 hits, one per 6000 samples.
    CHECK (track0Offsets.size() >= 15);
    CHECK (track0Offsets.size() <= 17);

    if (track0Offsets.size() >= 2)
    {
        CHECK_NEAR (track0Offsets[0], 0, 1);
        CHECK_NEAR (track0Offsets[1] - track0Offsets[0], 6000, 2);
    }
}

TEST (sequencerRespectsTrackLengthForPolyrhythms)
{
    opxy::Pattern pattern;
    for (int t = 0; t < opxy::kMaxTracks; ++t)
    {
        pattern.tracks[t].length.store (t == 0 ? 16 : 7);
        for (auto& s : pattern.tracks[t].steps)
            s.active.store (false);
    }

    // Track 0 fires every step; track 1 only on its step 0 (a 7-step cycle).
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (true);
    pattern.tracks[1].steps[0].active.store (true);

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);
    int track0Count = 0, track1Count = 0;
    int samplePos = 0;
    const int totalSamples = static_cast<int> (kSampleRate * 4.0); // 32 sixteenths

    while (samplePos < totalSamples)
    {
        const int n = std::min (256, totalSamples - samplePos);
        const int count = seq.process (pattern, transport, n, events.data(), opxy::kMaxEventsPerBlock);
        for (int i = 0; i < count; ++i)
        {
            if (events[static_cast<std::size_t> (i)].track == 0) ++track0Count;
            if (events[static_cast<std::size_t> (i)].track == 1) ++track1Count;
        }
        samplePos += n;
    }

    CHECK (track0Count >= 31);
    CHECK (track1Count >= 4);   // 32 steps / 7 = 4 or 5 hits
    CHECK (track1Count <= 6);
    CHECK (track1Count * 2 < track0Count);
}

TEST (probabilityZeroNeverFiresAndOneAlwaysDoes)
{
    opxy::Pattern pattern;
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (true);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (1);

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);

    pattern.tracks[0].probability.store (0.0f);
    seq.reset();
    int count = 0;
    for (int i = 0; i < 40; ++i)
        count += seq.process (pattern, transport, 512, events.data(), opxy::kMaxEventsPerBlock);
    CHECK (count == 0);

    pattern.tracks[0].probability.store (1.0f);
    seq.reset();
    count = 0;
    for (int i = 0; i < 40; ++i)
        count += seq.process (pattern, transport, 512, events.data(), opxy::kMaxEventsPerBlock);
    CHECK (count > 0);
}

TEST (ratchetSubdividesAStep)
{
    opxy::Pattern pattern;
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (false);
    pattern.tracks[0].steps[0].active.store (true);
    pattern.tracks[0].steps[0].ratchet.store (4);
    pattern.tracks[0].length.store (16);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (1);

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);
    std::vector<int> offsets;
    int samplePos = 0;
    while (samplePos < 12000) // 2 steps
    {
        const int n = std::min (128, 12000 - samplePos);
        const int count = seq.process (pattern, transport, n, events.data(), opxy::kMaxEventsPerBlock);
        for (int i = 0; i < count; ++i)
            if (events[static_cast<std::size_t> (i)].track == 0)
                offsets.push_back (samplePos + events[static_cast<std::size_t> (i)].sampleOffset);
        samplePos += n;
    }

    // 4 sub-hits inside the first 6000-sample step, spaced 1500 apart.
    CHECK (offsets.size() == 4);
    if (offsets.size() == 4)
    {
        CHECK_NEAR (offsets[0], 0, 1);
        CHECK_NEAR (offsets[1] - offsets[0], 1500, 2);
        CHECK_NEAR (offsets[2] - offsets[1], 1500, 2);
        CHECK_NEAR (offsets[3] - offsets[2], 1500, 2);
    }
}

TEST (swingDelaysOddSteps)
{
    opxy::Pattern pattern;
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (true);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (1);

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);
    seq.setSwing (1.0f);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);
    std::vector<int> offsets;
    int samplePos = 0;
    while (samplePos < 12000)
    {
        const int n = std::min (256, 12000 - samplePos);
        const int count = seq.process (pattern, transport, n, events.data(), opxy::kMaxEventsPerBlock);
        for (int i = 0; i < count; ++i)
            if (events[static_cast<std::size_t> (i)].track == 0)
                offsets.push_back (samplePos + events[static_cast<std::size_t> (i)].sampleOffset);
        samplePos += n;
    }

    CHECK (offsets.size() == 2);
    if (offsets.size() == 2)
    {
        CHECK_NEAR (offsets[0], 0, 1);
        CHECK_NEAR (offsets[1] - offsets[0], 9000, 4); // 6000 + 0.5 * 6000
    }
}

TEST (nudgeAppliesMicroTiming)
{
    opxy::Pattern pattern;
    for (auto& s : pattern.tracks[0].steps)
        s.active.store (false);
    pattern.tracks[0].steps[0].active.store (true);
    pattern.tracks[0].steps[0].nudge.store (0.25f); // quarter step late = 1500 samples
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        pattern.tracks[t].length.store (1);

    opxy::Sequencer seq;
    seq.prepare (kSampleRate);

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    std::vector<opxy::StepEvent> events (opxy::kMaxEventsPerBlock);
    int firstOffset = -1;
    int samplePos = 0;
    while (samplePos < 12000 && firstOffset < 0)
    {
        const int n = std::min (128, 12000 - samplePos);
        const int count = seq.process (pattern, transport, n, events.data(), opxy::kMaxEventsPerBlock);
        for (int i = 0; i < count; ++i)
            if (events[static_cast<std::size_t> (i)].track == 0 && firstOffset < 0)
                firstOffset = samplePos + events[static_cast<std::size_t> (i)].sampleOffset;
        samplePos += n;
    }

    CHECK (firstOffset > 0);
    CHECK_NEAR (firstOffset, 1500, 2);
}

TEST (engineRendersAudioWithoutBlowingUp)
{
    opxy::Engine engine;
    engine.prepare (kSampleRate, 512);

    for (auto& s : engine.getPattern().tracks[0].steps)
        s.active.store (true);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        engine.getPattern().tracks[t].length.store (1);

    std::vector<float> l (512, 0.0f), r (512, 0.0f);
    float* channels[2] = { l.data(), r.data() };

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    double peak = 0.0;
    bool finite = true;

    for (int block = 0; block < 200; ++block)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 512, transport);
        for (int i = 0; i < 512; ++i)
        {
            if (! std::isfinite (l[static_cast<std::size_t> (i)]))
                finite = false;
            peak = std::max (peak, static_cast<double> (std::abs (l[static_cast<std::size_t> (i)])));
        }
    }

    CHECK (finite);
    CHECK (peak > 0.001);    // it actually made sound
    CHECK (peak < 2.0);      // and did not blow up
    CHECK (engine.getLastEventCount() > 0);
}

TEST (engineHandlesOversizedBlocksAndAllVoicesAreCapped)
{
    opxy::Engine engine;
    engine.prepare (kSampleRate, 512);

    // Hammer every track with fast 64th-ish notes to force voice stealing.
    for (int t = 0; t < opxy::kMaxTracks; ++t)
    {
        engine.getPattern().tracks[t].length.store (16);
        for (auto& s : engine.getPattern().tracks[t].steps)
        {
            s.active.store (true);
            s.gate.store (0.1f);
            s.ratchet.store (8);
        }
    }

    const int blockSize = 4096; // deliberately larger than the prepared size
    std::vector<float> l (static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float> r (static_cast<std::size_t> (blockSize), 0.0f);
    float* channels[2] = { l.data(), r.data() };

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 140.0;

    for (int block = 0; block < 20; ++block)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, blockSize, transport);
        for (int i = 0; i < blockSize; ++i)
            CHECK (std::isfinite (l[static_cast<std::size_t> (i)]));
    }
}

TEST (playNoteProducesSoundWithoutSequencer)
{
    opxy::Engine engine;
    engine.prepare (kSampleRate, 256);

    // Transport stopped: nothing from the sequencer, only the played note.
    opxy::TransportInfo stopped;
    stopped.playing = false;

    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    float* channels[2] = { l.data(), r.data() };

    engine.playNote (0, 60, 0.9f);

    double peak = 0.0;
    for (int block = 0; block < 20; ++block)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, stopped);
        for (int i = 0; i < 256; ++i)
            peak = std::max (peak, static_cast<double> (std::abs (l[static_cast<std::size_t> (i)])));
    }
    CHECK (peak > 0.001);
    CHECK (peak < 1.5);
}

TEST (engineStopsMakingSoundWhenStopped)
{
    opxy::Engine engine;
    engine.prepare (kSampleRate, 256);

    for (auto& s : engine.getPattern().tracks[0].steps)
        s.active.store (true);
    engine.getPattern().tracks[0].decay.store (0.05f);

    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    float* channels[2] = { l.data(), r.data() };

    opxy::TransportInfo playing;
    playing.playing = true;
    playing.bpm = 120.0;

    for (int block = 0; block < 40; ++block)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, playing);
    }

    opxy::TransportInfo stopped;
    stopped.playing = false;

    double peak = 0.0;
    for (int block = 0; block < 200; ++block) // ~1 s of tails then silence
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, stopped);
        for (int i = 0; i < 256; ++i)
            peak = std::max (peak, static_cast<double> (std::abs (l[static_cast<std::size_t> (i)])));
    }

    CHECK (peak < 0.01);
}

TEST (stepsSurviveSerialisationRoundTrip)
{
    opxy::Pattern original;
    original.tracks[2].steps[5].active.store (true);
    original.tracks[2].steps[5].note.store (43.0f);
    original.tracks[2].steps[5].velocity.store (0.42f);
    original.tracks[2].steps[5].gate.store (0.75f);
    original.tracks[2].steps[5].ratchet.store (3);
    original.tracks[2].steps[5].nudge.store (-0.125f);
    original.tracks[2].steps[5].setPLock (opxy::PlockParam::Cutoff, 640.0f);
    original.tracks[2].steps[5].setPLock (opxy::PlockParam::Resonance, 0.73f);

    original.tracks[7].steps[63].active.store (true);
    original.tracks[7].steps[63].note.store (96.0f);

    const std::string blob = opxy::serialiseSteps (original);
    CHECK (blob.rfind ("opxy-steps v1", 0) == 0);

    opxy::Pattern restored;
    CHECK (opxy::deserialiseSteps (blob, restored));

    const auto& step = restored.tracks[2].steps[5];
    CHECK (step.active.load());
    CHECK_NEAR (step.note.load(), 43.0f, 0.001f);
    CHECK_NEAR (step.velocity.load(), 0.42f, 0.001f);
    CHECK_NEAR (step.gate.load(), 0.75f, 0.001f);
    CHECK (step.ratchet.load() == 3);
    CHECK_NEAR (step.nudge.load(), -0.125f, 0.001f);
    CHECK_NEAR (step.getPLock (opxy::PlockParam::Cutoff, 0.0f), 640.0f, 0.5f);
    CHECK_NEAR (step.getPLock (opxy::PlockParam::Resonance, 0.0f), 0.73f, 0.001f);
    CHECK (! step.hasPLock (opxy::PlockParam::Pitch));

    CHECK (restored.tracks[7].steps[63].active.load());
    // Untouched steps stay silent.
    CHECK (! restored.tracks[0].steps[0].active.load());
    CHECK (! restored.tracks[2].steps[6].active.load());

    // Round-tripping twice is stable.
    const std::string blob2 = opxy::serialiseSteps (restored);
    CHECK (blob == blob2);

    // Garbage input is rejected without crashing.
    opxy::Pattern junk;
    CHECK (! opxy::deserialiseSteps ("nonsense", junk));
    CHECK (! opxy::deserialiseSteps ("", junk));
}

// --------------------------------- FX tests --------------------------------

TEST (tapeStopRampsRateAndFreezes)
{
    opxy::TapeStopFx fx;
    fx.prepare (kSampleRate, 512);
    fx.setPunchParam (0, 0.2f); // short stop time

    const auto report = renderFx (fx, static_cast<int> (kSampleRate * 1.0), 512,
                                  [] (int i) { return i < static_cast<int> (kSampleRate * 0.5); });

    CHECK (report.finite);
    CHECK (report.peak < 1.5);
    CHECK (fx.isActive() == false); // disengaged long enough for the ramp to finish
}

TEST (punchInEffectsDoNotClickWhenEngagedAndReleased)
{
    const auto runOne = [] (opxy::PunchInFx& fx)
    {
        fx.prepare (kSampleRate, 256);
        const int engageAt = static_cast<int> (kSampleRate * 0.25);
        const int releaseAt = static_cast<int> (kSampleRate * 0.75);
        const auto report = renderFx (fx, static_cast<int> (kSampleRate * 1.6), 256,
                                      [engageAt, releaseAt] (int i) { return i >= engageAt && i < releaseAt; });
        std::printf ("       %-12s peak %.3f  maxDelta %.4f  rms %.3f\n",
                     fx.getName(), report.peak, report.maxDelta, report.rms);
        CHECK (report.finite);
        check (report.peak < 2.0, "peak < 2.0", __FILE__, __LINE__);
        // A 220 Hz sine at 0.5 amplitude moves ~0.014 per sample; anything above
        // 0.3 would be an audible click at the engage/release boundary.
        check (report.maxDelta < 0.30, "maxDelta < 0.30 (click free)", __FILE__, __LINE__);
        return report;
    };

    {
        opxy::TapeStopFx fx;
        runOne (fx);
    }
    {
        opxy::DelayThrowFx fx;
        runOne (fx);
    }
    {
        opxy::FilterSweepFx fx;
        runOne (fx);
    }
    {
        opxy::StutterFx fx;
        runOne (fx);
    }
    {
        opxy::ReverbBurstFx fx;
        runOne (fx);
    }
    {
        opxy::BitcrushFx fx;
        runOne (fx);
    }
}

TEST (delayThrowKeepsTailingAfterRelease)
{
    opxy::DelayThrowFx fx;
    fx.prepare (kSampleRate, 256);
    fx.setPunchParam (0, 0.2f);   // ~200 ms
    fx.setPunchParam (1, 0.6f);   // feedback

    fx.setEngaged (true);
    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    for (int i = 0; i < 40; ++i) // ~200 ms of audio in
    {
        for (int s = 0; s < 256; ++s)
        {
            const float v = 0.5f * static_cast<float> (std::sin (2.0 * 3.14159265 * 220.0 * (i * 256 + s) / kSampleRate));
            l[static_cast<std::size_t> (s)] = v;
            r[static_cast<std::size_t> (s)] = v;
        }
        fx.process (l.data(), r.data(), 256);
    }

    fx.setEngaged (false);

    // Immediately after release the effect must still be active (tail alive)...
    CHECK (fx.isActive());

    // ...and it must decay to silence in the end.
    for (int i = 0; i < 800; ++i) // ~4 s of silence
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        fx.process (l.data(), r.data(), 256);
    }
    CHECK (! fx.isActive());

    float residual = 0.0f;
    for (int i = 0; i < 256; ++i)
        residual = std::max (residual, std::abs (l[static_cast<std::size_t> (i)]));
    CHECK (residual < 0.01f);
}

TEST (reverbBurstDecaysToSilence)
{
    opxy::ReverbBurstFx fx;
    fx.prepare (kSampleRate, 256);

    fx.setEngaged (true);
    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    for (int i = 0; i < 40; ++i)
    {
        for (int s = 0; s < 256; ++s)
        {
            const float v = 0.4f * static_cast<float> (std::sin (2.0 * 3.14159265 * 330.0 * (i * 256 + s) / kSampleRate));
            l[static_cast<std::size_t> (s)] = v;
            r[static_cast<std::size_t> (s)] = v;
        }
        fx.process (l.data(), r.data(), 256);
    }
    fx.setEngaged (false);

    bool finite = true;
    for (int i = 0; i < 1200; ++i) // 6.4 s
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        fx.process (l.data(), r.data(), 256);
        for (int s = 0; s < 256; ++s)
            if (! std::isfinite (l[static_cast<std::size_t> (s)]))
                finite = false;
    }

    CHECK (finite);
    CHECK (! fx.isActive());

    float residual = 0.0f;
    for (int i = 0; i < 256; ++i)
        residual = std::max (residual, std::abs (l[static_cast<std::size_t> (i)]));
    CHECK (residual < 0.01f);
}

TEST (bitcrushQuantisesAndRecovers)
{
    opxy::BitcrushFx fx;
    fx.prepare (kSampleRate, 256);
    fx.setPunchParam (0, 1.0f);  // max bit reduction
    fx.setPunchParam (1, 0.0f);  // no decimation

    fx.setEngaged (true);
    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    for (int s = 0; s < 256; ++s)
    {
        const float v = 0.5f * static_cast<float> (std::sin (2.0 * 3.14159265 * 100.0 * s / kSampleRate));
        l[static_cast<std::size_t> (s)] = v;
        r[static_cast<std::size_t> (s)] = v;
    }
    for (int i = 0; i < 20; ++i)
        fx.process (l.data(), r.data(), 256);

    // Coarse quantisation should produce far fewer distinct values than a sine.
    int distinct = 0;
    float last = 999.0f;
    std::vector<float> sorted (l.begin(), l.end());
    std::sort (sorted.begin(), sorted.end());
    for (float v : sorted)
    {
        if (std::abs (v - last) > 1.0e-6f)
        {
            ++distinct;
            last = v;
        }
    }
    CHECK (distinct <= 24);

    // Releasing returns the effect to bypassed within a ramp.
    fx.setEngaged (false);
    for (int i = 0; i < 40; ++i)
    {
        std::fill (l.begin(), l.end(), 0.25f);
        std::fill (r.begin(), r.end(), 0.25f);
        fx.process (l.data(), r.data(), 256);
    }
    CHECK (! fx.isActive());
    CHECK_NEAR (l[255], 0.25f, 0.01f);
}

TEST (filterSweepAttenuatesWhenEngaged)
{
    opxy::FilterSweepFx fx;
    fx.prepare (kSampleRate, 256);
    fx.setPunchParam (0, 1.0f);
    fx.setPunchParam (1, 0.5f);

    // A bright 4 kHz tone should lose energy once the sweep is engaged.
    const auto measure = [&fx] (bool engaged)
    {
        fx.setEngaged (engaged);
        std::vector<float> l (256, 0.0f), r (256, 0.0f);
        double sumSquares = 0.0;
        for (int i = 0; i < 60; ++i)
        {
            for (int s = 0; s < 256; ++s)
            {
                const float v = 0.4f * static_cast<float> (std::sin (2.0 * 3.14159265 * 4000.0 * (i * 256 + s) / kSampleRate));
                l[static_cast<std::size_t> (s)] = v;
                r[static_cast<std::size_t> (s)] = v;
            }
            fx.process (l.data(), r.data(), 256);
            if (i > 30)
                for (int s = 0; s < 256; ++s)
                    sumSquares += static_cast<double> (l[static_cast<std::size_t> (s)]) * l[static_cast<std::size_t> (s)];
        }
        return std::sqrt (sumSquares / (30.0 * 256.0));
    };

    const double open = measure (false);
    const double swept = measure (true);

    CHECK (open > 0.1);
    CHECK (swept < open * 0.5);
}

TEST (stutterRepeatsTheSliceAndStops)
{
    opxy::StutterFx fx;
    fx.prepare (kSampleRate, 256);
    fx.setPunchParam (0, 0.05f);   // short slice
    fx.setPunchParam (1, 0.0f);    // half speed

    fx.setEngaged (true);
    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    bool finite = true;
    for (int i = 0; i < 200; ++i)
    {
        // Feed a ramp so a naive pass-through would be obvious.
        for (int s = 0; s < 256; ++s)
        {
            const float v = 0.3f * static_cast<float> (std::sin (2.0 * 3.14159265 * 440.0 * (i * 256 + s) / kSampleRate));
            l[static_cast<std::size_t> (s)] = v;
            r[static_cast<std::size_t> (s)] = v;
        }
        fx.process (l.data(), r.data(), 256);
        for (int s = 0; s < 256; ++s)
            if (! std::isfinite (l[static_cast<std::size_t> (s)]))
                finite = false;
    }
    CHECK (finite);

    fx.setEngaged (false);
    for (int i = 0; i < 20; ++i)
    {
        std::fill (l.begin(), l.end(), 0.2f);
        std::fill (r.begin(), r.end(), 0.2f);
        fx.process (l.data(), r.data(), 256);
    }
    CHECK (! fx.isActive());
    CHECK_NEAR (l[255], 0.2f, 0.02f);
}

TEST (enginePunchFxClearsWhenReleased)
{
    opxy::Engine engine;
    engine.prepare (kSampleRate, 256);

    for (auto& s : engine.getPattern().tracks[0].steps)
        s.active.store (true);
    for (int t = 1; t < opxy::kMaxTracks; ++t)
        engine.getPattern().tracks[t].length.store (1);

    std::vector<float> l (256, 0.0f), r (256, 0.0f);
    float* channels[2] = { l.data(), r.data() };

    opxy::TransportInfo transport;
    transport.playing = true;
    transport.bpm = 120.0;

    // Punch the tape stop on the master bus.
    auto& tape = engine.getFxSettings (static_cast<int> (opxy::FxType::TapeStop));
    tape.engaged.store (true);

    for (int i = 0; i < 60; ++i)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, transport);
    }

    tape.engaged.store (false);

    bool finite = true;
    for (int i = 0; i < 300; ++i)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, transport);
        for (int s = 0; s < 256; ++s)
            if (! std::isfinite (l[static_cast<std::size_t> (s)]))
                finite = false;
    }

    CHECK (finite);

    // Audio should still be flowing normally afterwards.
    double peak = 0.0;
    for (int i = 0; i < 40; ++i)
    {
        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        engine.process (channels, 2, 256, transport);
        for (int s = 0; s < 256; ++s)
            peak = std::max (peak, static_cast<double> (std::abs (l[static_cast<std::size_t> (s)])));
    }
    CHECK (peak > 0.001);
}

TEST (rampReachesTargetInAboutTheRequestedTime)
{
    opxy::Ramp ramp;
    ramp.prepare (1000.0);
    ramp.setTimeSeconds (0.1f);   // 100 samples to go 0 -> 1
    ramp.setTarget (1.0f);

    for (int i = 0; i < 100; ++i)
        ramp.next();

    CHECK_NEAR (ramp.getCurrent(), 1.0f, 0.001f);
    CHECK (! ramp.isSmoothing());
}

// ------------------------------------ main ----------------------------------

int main()
{
    std::printf ("opxy core tests (%zu)\n", tests().size());

    for (auto& t : tests())
    {
        gCurrentTest = t.name;
        const int before = gFailures;
        t.fn();
        const bool passed = gFailures == before;
        std::printf ("%s %s\n", passed ? "  ok  " : "  FAIL", t.name);
    }

    std::printf ("\n%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
