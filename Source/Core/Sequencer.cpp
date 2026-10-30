#include "Sequencer.h"

#include <cmath>

namespace opxy
{

void Sequencer::prepare (double sr) noexcept
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    samplesPerStep = (60.0 / freeRunTempo) * sampleRate * 0.25;
    reset();
}

void Sequencer::reset() noexcept
{
    stepPos = 0.0;
    lastBoundary = -1;
    pendingCount = 0;
    running = false;
    wasPlaying = false;
    sampleCounter = 0;
    for (auto& s : currentStep)
        s.store (-1, std::memory_order_relaxed);
}

int Sequencer::getCurrentStep (int track) const noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return -1;
    return currentStep[track].load (std::memory_order_relaxed);
}

void Sequencer::flushPending() noexcept
{
    pendingCount = 0;
}

void Sequencer::scheduleHit (const PendingHit& hit) noexcept
{
    if (pendingCount >= static_cast<int> (sizeof (pending) / sizeof (pending[0])))
        return;

    // Insertion sort by time: the queue stays tiny (a block's worth of hits).
    int index = pendingCount;
    while (index > 0 && pending[index - 1].time > hit.time)
    {
        pending[index] = pending[index - 1];
        --index;
    }
    pending[index] = hit;
    ++pendingCount;
}

int Sequencer::process (const Pattern& pattern,
                        const TransportInfo& transport,
                        int numSamples,
                        StepEvent* outEvents,
                        int maxEvents) noexcept
{
    if (numSamples <= 0 || outEvents == nullptr || maxEvents <= 0)
        return 0;

    const double bpm = transport.hasPpq && transport.bpm > 1.0 ? transport.bpm
                                                               : (transport.bpm > 1.0 ? transport.bpm : freeRunTempo);
    samplesPerStep = (60.0 / bpm) * sampleRate * 0.25;

    const bool playing = transport.playing;
    running = playing;

    if (! playing)
    {
        if (wasPlaying)
        {
            flushPending();
            for (auto& s : currentStep)
                s.store (-1, std::memory_order_relaxed);
            lastBoundary = -1;
            stepPos = 0.0;
        }
        wasPlaying = false;
        sampleCounter += numSamples;
        return 0;
    }

    const bool starting = ! wasPlaying;
    wasPlaying = true;

    if (starting)
    {
        flushPending();
        stepPos = transport.hasPpq ? transport.ppqAtBlockStart * kStepsPerBeat : 0.0;
        lastBoundary = static_cast<std::int64_t> (std::floor (stepPos)) - 1;
        for (auto& s : currentStep)
            s.store (-1, std::memory_order_relaxed);
    }
    else if (transport.hasPpq)
    {
        const double hostStepPos = transport.ppqAtBlockStart * kStepsPerBeat;
        const double expected    = stepPos + static_cast<double> (numSamples) / samplesPerStep;

        // A large jump means the host looped or the user relocated: resync.
        if (std::abs (hostStepPos - expected) > 0.25)
        {
            flushPending();
            lastBoundary = static_cast<std::int64_t> (std::floor (hostStepPos)) - 1;
        }
        stepPos = hostStepPos;
    }

    const double blockEndStep = stepPos + static_cast<double> (numSamples) / samplesPerStep;

    const std::int64_t firstBoundary = lastBoundary + 1;
    const std::int64_t finalBoundary = static_cast<std::int64_t> (std::floor (blockEndStep + 1.0e-9));

    for (std::int64_t k = firstBoundary; k <= finalBoundary; ++k)
    {
        // Swing: every odd 16th lands later by up to half a step.
        const double swingSamples = (k & 1) ? static_cast<double> (swing) * 0.5 * samplesPerStep : 0.0;
        const double offsetExact  = (static_cast<double> (k) - stepPos) * samplesPerStep + swingSamples;
        const double offsetClamped = std::max (0.0, offsetExact);

        for (int t = 0; t < kMaxTracks; ++t)
        {
            const Track& track = pattern.tracks[t];
            const int length = track.getClampedLength();
            const int stepIndex = static_cast<int> (((k % length) + length) % length);

            if (! trackStepFires (track, stepIndex, rng))
                continue;

            const Step& step = track.steps[stepIndex];

            PendingHit hit;
            hit.track = t;
            hit.step = stepIndex;

            const float noteValue = static_cast<float> (track.effectiveNote (stepIndex, rng));
            hit.note      = static_cast<int> (std::lround (noteValue));
            hit.velocity  = track.effectiveVelocity (stepIndex, rng);
            hit.primary   = true;

            const float gate = clamp (step.gate.load (std::memory_order_relaxed), 0.02f, 8.0f);
            const int ratchetCount = clamp (step.ratchet.load (std::memory_order_relaxed),
                                            1, kMaxRatchet)
                                   * clamp (track.ratchet.load (std::memory_order_relaxed), 1, kMaxRatchet);
            const int ratchets = clamp (ratchetCount, 1, kMaxRatchet);

            // Resolve the p-locked voice parameters once per step.
            hit.cutoff    = step.getPLock (PlockParam::Cutoff, track.cutoff.load (std::memory_order_relaxed));
            hit.resonance = step.getPLock (PlockParam::Resonance, track.resonance.load (std::memory_order_relaxed));
            hit.decay     = step.getPLock (PlockParam::Decay, track.decay.load (std::memory_order_relaxed));
            hit.pan       = step.getPLock (PlockParam::Pan, track.pan.load (std::memory_order_relaxed));
            hit.level     = track.level.load (std::memory_order_relaxed);
            hit.engine    = track.engine.load (std::memory_order_relaxed);
            hit.noise     = track.noiseOn.load (std::memory_order_relaxed);
            hit.gateSteps = gate;

            const double nudge = static_cast<double> (clamp (step.nudge.load (std::memory_order_relaxed),
                                                             -0.5f, 0.5f)) * samplesPerStep;
            const double baseTime = static_cast<double> (sampleCounter) + offsetClamped + nudge;

            hit.time = static_cast<std::int64_t> (std::llround (baseTime));
            scheduleHit (hit);

            // Ratchet: evenly spaced re-triggers inside the step.
            if (ratchets > 1)
            {
                const double subSpacing = samplesPerStep / static_cast<double> (ratchets);
                for (int r = 1; r < ratchets; ++r)
                {
                    PendingHit sub = hit;
                    sub.primary   = false;
                    sub.time      = static_cast<std::int64_t> (std::llround (baseTime + subSpacing * r));
                    sub.gateSteps = gate / static_cast<float> (ratchets);
                    scheduleHit (sub);
                }
            }
        }
    }

    if (finalBoundary >= lastBoundary)
        lastBoundary = finalBoundary;

    // Emit everything that lands inside this block.
    const std::int64_t blockEnd = sampleCounter + numSamples;
    int numEvents = 0;
    int remaining = 0;

    for (int i = 0; i < pendingCount; ++i)
    {
        const PendingHit& hit = pending[i];
        if (hit.time < blockEnd && numEvents < maxEvents)
        {
            StepEvent& e = outEvents[numEvents++];
            e.sampleOffset = static_cast<int> (clamp<std::int64_t> (hit.time - sampleCounter, 0, numSamples - 1));
            e.track        = hit.track;
            e.step         = hit.step;
            e.note         = hit.note;
            e.velocity     = hit.velocity;
            e.gateSteps    = hit.gateSteps;
            e.engine       = hit.engine;
            e.cutoff       = hit.cutoff;
            e.resonance    = hit.resonance;
            e.decay        = hit.decay;
            e.pan          = hit.pan;
            e.level        = hit.level;
            e.noiseLayer   = hit.noise;
            e.primary      = hit.primary;

            if (hit.primary)
                currentStep[hit.track].store (hit.step, std::memory_order_relaxed);
        }
        else
        {
            pending[remaining++] = hit;
        }
    }
    pendingCount = remaining;

    stepPos = blockEndStep;
    sampleCounter = blockEnd;

    return numEvents;
}

} // namespace opxy
