#include "Pattern.h"

#include <cmath>

namespace opxy
{

std::uint64_t euclideanMask (int numSteps, int hits, int offset) noexcept
{
    numSteps = clamp (numSteps, 1, kMaxSteps);
    hits     = clamp (hits, 0, numSteps);

    std::uint64_t mask = 0u;
    if (hits == 0)
        return 0u;
    if (hits == numSteps)
        return (numSteps >= 64) ? ~0ull : ((1ull << numSteps) - 1ull);

    // Bresenham / Euclidean distribution: step i hits when (i * hits) mod n < hits.
    // This produces the classic Bjorklund spacing (e.g. 3-in-8 -> 0, 3, 6) while
    // being branch-light and allocation free.
    const int rotation = ((offset % numSteps) + numSteps) % numSteps;

    for (int i = 0; i < numSteps; ++i)
    {
        const int source = (i - rotation + numSteps) % numSteps;
        if ((source * hits) % numSteps < hits)
            mask |= (1ull << i);
    }
    return mask;
}

bool trackStepFires (const Track& track, int stepIndex, Prng& rng) noexcept
{
    const int length = track.getClampedLength();
    if (stepIndex < 0 || stepIndex >= length)
        return false;

    const bool euclidOn = track.euclidOn.load (std::memory_order_relaxed);
    bool fires = false;

    if (euclidOn)
    {
        const auto mask = euclideanMask (length,
                                        track.euclidHits.load (std::memory_order_relaxed),
                                        track.euclidOffset.load (std::memory_order_relaxed));
        fires = (mask >> stepIndex) & 1ull;
    }
    else
    {
        fires = track.steps[stepIndex].active.load (std::memory_order_relaxed);
    }

    if (! fires)
        return false;

    // Note on the Euclid + toggles interaction:
    // when the Euclidean brain is on it *replaces* the hand-drawn pattern, and
    // step toggles are ignored so the generated rhythm stays predictable.

    const float probability = clamp (track.probability.load (std::memory_order_relaxed), 0.0f, 1.0f);
    if (probability < 1.0f && rng.nextFloat() >= probability)
        return false;

    return true;
}

int quantiseToScale (int note, int root, int scaleTypeIndex) noexcept
{
    static constexpr int major[]      = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minor[]      = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int dorian[]     = { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr int pentatonic[] = { 0, 2, 4, 7, 9 };

    const int* intervals = nullptr;
    int count = 0;

    switch (static_cast<ScaleType> (clamp (scaleTypeIndex, 0, static_cast<int> (ScaleType::Count) - 1)))
    {
        case ScaleType::Major:      intervals = major;      count = 7; break;
        case ScaleType::Minor:      intervals = minor;      count = 7; break;
        case ScaleType::Dorian:     intervals = dorian;     count = 7; break;
        case ScaleType::Pentatonic: intervals = pentatonic; count = 5; break;
        case ScaleType::Chromatic:
        case ScaleType::Off:
        case ScaleType::Count:
            return note;
    }

    const int noteInOctave = ((note % 12) + 12) % 12;
    const int rootPc       = ((root % 12) + 12) % 12;
    const int relative     = ((noteInOctave - rootPc) % 12 + 12) % 12;

    int bestInterval = intervals[0];
    int bestDistance = 99;
    for (int i = 0; i < count; ++i)
    {
        const int distance = std::abs (relative - intervals[i]);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestInterval = intervals[i];
        }
    }

    const int quantisedInOctave = (rootPc + bestInterval) % 12;
    const int octaveBase        = note - noteInOctave;
    int result                  = octaveBase + quantisedInOctave;

    // Keep the result in the octave nearest the original note.
    while (result - note > 6)  result -= 12;
    while (note - result > 6)  result += 12;

    return clamp (result, 0, 127);
}

int Track::effectiveNote (int stepIndex, Prng& rng) const noexcept
{
    const auto& s = steps[stepIndex];
    float note = s.note.load (std::memory_order_relaxed);

    // Pitch p-lock is a semitone offset added to the step note.
    note += s.getPLock (PlockParam::Pitch, 0.0f);

    const float randomRange = randomPitch.load (std::memory_order_relaxed);
    if (randomRange > 0.0f)
        note += rng.nextBipolar() * randomRange;

    int midiNote = static_cast<int> (std::lround (note));

    midiNote = quantiseToScale (midiNote,
                                scaleRoot.load (std::memory_order_relaxed),
                                scaleType.load (std::memory_order_relaxed));

    return clamp (midiNote, 0, 127);
}

float Track::effectiveVelocity (int stepIndex, Prng& rng) const noexcept
{
    const auto& s = steps[stepIndex];
    float velocity = s.velocity.load (std::memory_order_relaxed);
    velocity = s.getPLock (PlockParam::Velocity, velocity);

    const float randomAmount = clamp (randomVel.load (std::memory_order_relaxed), 0.0f, 1.0f);
    if (randomAmount > 0.0f)
        velocity += rng.nextBipolar() * randomAmount * 0.5f;

    return clamp (velocity, 0.0f, 1.0f);
}

} // namespace opxy
