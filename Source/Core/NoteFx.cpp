#include "NoteFx.h"
#include "ScalarUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace opxy
{
namespace
{
constexpr int kTicksPerBeat = 8;      // the grid runs in 32nd notes
constexpr int kMaxTicksPerBlock = 64;
constexpr int kOffHeadroom = 16;      // pending slots kept free so note-offs always fit
constexpr int kStepsPerBar = 16;

std::int64_t positiveMod (std::int64_t value, std::int64_t divisor) noexcept
{
    return ((value % divisor) + divisor) % divisor;
}
} // namespace

const char* noteFxName (NoteFxType type) noexcept
{
    switch (type)
    {
        case NoteFxType::Mute:       return "Mute";
        case NoteFxType::Repeat:     return "Repeat";
        case NoteFxType::Fill:       return "Fill";
        case NoteFxType::HalfFill:   return "Half Fill";
        case NoteFxType::KickSnare:  return "Kick+Snare";
        case NoteFxType::Hats:       return "Hats";
        case NoteFxType::OctaveUp:   return "Octave Up";
        case NoteFxType::OctaveDown: return "Octave Down";
        case NoteFxType::Shorten:    return "Shorten";
        case NoteFxType::Random:     return "Random";
        case NoteFxType::Skip:       return "Skip";
        case NoteFxType::Echo:       return "Echo";
        case NoteFxType::Arp:        return "Arp";
        case NoteFxType::Chord:      return "Chord";
        case NoteFxType::Strum:      return "Strum";
        case NoteFxType::Groove:     return "Groove";
        case NoteFxType::Count:      break;
    }
    return "?";
}

const char* noteFxDescription (NoteFxType type) noexcept
{
    switch (type)
    {
        case NoteFxType::Mute:       return "silences incoming notes";
        case NoteFxType::Repeat:     return "rolls the last note";
        case NoteFxType::Fill:       return "fills empty 16ths with the last hit";
        case NoteFxType::HalfFill:   return "fills empty 8ths with the last hit";
        case NoteFxType::KickSnare:  return "adds a kick + snare beat";
        case NoteFxType::Hats:       return "adds 16th hi-hats";
        case NoteFxType::OctaveUp:   return "everything up an octave";
        case NoteFxType::OctaveDown: return "everything down an octave";
        case NoteFxType::Shorten:    return "chops notes short";
        case NoteFxType::Random:     return "swaps in recently played notes";
        case NoteFxType::Skip:       return "plays every other note";
        case NoteFxType::Echo:       return "repeats each note an 8th later";
        case NoteFxType::Arp:        return "plays held notes one at a time";
        case NoteFxType::Chord:      return "adds a fifth and an octave";
        case NoteFxType::Strum:      return "strums chords, flams single hits";
        case NoteFxType::Groove:     return "swing, accents, loose timing";
        case NoteFxType::Count:      break;
    }
    return "";
}

NoteFx::NoteFx()
{
    for (auto& p : padHeld)
        p.store (false);
    for (auto& a : activeForUi)
        a.store (false);
    reset();
}

void NoteFx::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    samplesPerBeat = sampleRate * 0.5;
    reset();
}

void NoteFx::reset()
{
    clock = 0;
    lastTick = -1;
    lastInputOn = -1000000;
    numPending = 0;
    hasLastHit = false;
    skipCounter = 0;
    pitchPoolSize = 0;
    pitchPoolNext = 0;

    for (int f = 0; f < kNumNoteFx; ++f)
    {
        noteHeld[f] = false;
        triggerVelocity[f] = 1.0f;
    }

    for (auto& channel : slots)
        for (auto& slot : channel)
            slot = Slot();

    numHeld = 0;
    arpIndex = 0;
    strumCount = 0;
    lastStrumOn = -1000000;
}

void NoteFx::setPadHeld (int fx, bool isHeld) noexcept
{
    if (fx >= 0 && fx < kNumNoteFx)
        padHeld[fx].store (isHeld, std::memory_order_relaxed);
}

bool NoteFx::isActive (int fx) const noexcept
{
    return fx >= 0 && fx < kNumNoteFx && activeForUi[fx].load (std::memory_order_relaxed);
}

bool NoteFx::held (NoteFxType type) const noexcept
{
    const int f = static_cast<int> (type);
    return noteHeld[f] || padHeld[f].load (std::memory_order_relaxed);
}

int NoteFx::process (const NoteEvent* input, int numInput, int numSamples,
                     const TransportInfo& transport, NoteEvent* output, int maxOutput)
{
    out = output;
    outCount = 0;
    outMax = output != nullptr ? std::max (0, maxOutput) : 0;
    blockSamples = std::max (1, numSamples);
    samplesPerBeat = sampleRate * 60.0 / std::max (transport.bpm, 20.0);
    ppqValid = transport.playing && transport.hasPpq;
    blockPpq = transport.ppqAtBlockStart;

    // ---- grid points (32nd notes) inside this block -------------------------
    std::int64_t tickIndex[kMaxTicksPerBlock];
    int tickOffset[kMaxTicksPerBlock];
    int numTicks = 0;

    if (transport.playing && transport.hasPpq && numSamples > 0)
    {
        const double ppq = transport.ppqAtBlockStart;
        auto tick = static_cast<std::int64_t> (std::ceil (ppq * kTicksPerBeat - 1.0e-7));

        // Hosts round ppq a little differently from block to block; never play a
        // grid point twice. A jump back of more than a few ticks is a relocate
        // or loop, and starts the grid over.
        if (tick <= lastTick && lastTick - tick < 4)
            tick = lastTick + 1;

        for (; numTicks < kMaxTicksPerBlock; ++tick)
        {
            const double offset = (static_cast<double> (tick) / kTicksPerBeat - ppq) * samplesPerBeat;
            const auto rounded = static_cast<int> (std::llround (std::max (0.0, offset)));
            if (rounded >= numSamples)
                break;
            tickIndex[numTicks] = tick;
            tickOffset[numTicks] = rounded;
            ++numTicks;
        }
    }
    else
    {
        lastTick = -1;
    }

    // ---- input and grid, in time order (input first on a tie, so a real note
    //      on the grid point cancels a fill there) -----------------------------
    int i = 0, t = 0;
    while (i < numInput || t < numTicks)
    {
        if (i < numInput && (t >= numTicks || input[i].sampleOffset <= tickOffset[t]))
        {
            const int offset = clamp (input[i].sampleOffset, 0, blockSamples - 1);
            handleInput (input[i], clock + offset);
            ++i;
        }
        else
        {
            handleTick (tickIndex[t], clock + tickOffset[t]);
            ++t;
        }
    }

    // ---- anything scheduled to happen inside this block ----------------------
    const std::int64_t blockEnd = clock + numSamples;
    for (bool emitted = true; emitted;)
    {
        emitted = false;
        for (int p = 0; p < numPending; ++p)
        {
            if (pending[p].time >= blockEnd)
                continue;

            const Pending due = pending[p];
            pending[p] = pending[--numPending];
            --p;
            emitted = true;

            if (due.fillCandidate)
                playGenerated (due.time, due.channel, due.note, due.velocity,
                               static_cast<std::int64_t> (samplesPerBeat / kTicksPerBeat));
            else
                emit (due.time, due.channel, due.note, due.velocity, due.on);
        }
    }

    std::stable_sort (output, output + outCount, [] (const NoteEvent& a, const NoteEvent& b)
    {
        if (a.sampleOffset != b.sampleOffset)
            return a.sampleOffset < b.sampleOffset;
        return ! a.on && b.on;
    });

    for (int f = 0; f < kNumNoteFx; ++f)
        activeForUi[f].store (held (static_cast<NoteFxType> (f)), std::memory_order_relaxed);

    clock = blockEnd;
    return outCount;
}

void NoteFx::handleInput (const NoteEvent& event, std::int64_t time)
{
    const int channel = clamp (event.channel, 1, 16);
    const int note = clamp (event.note, 0, 127);
    Slot& slot = slots[channel - 1][note];

    const int base = settings.baseNote.load (std::memory_order_relaxed);
    if (isTriggerNote (note, base))
    {
        const int f = note - base;
        noteHeld[f] = event.on;
        if (event.on)
            triggerVelocity[f] = std::max (0.05f, event.velocity);
        if (! event.on && f == static_cast<int> (NoteFxType::Skip))
            skipCounter = 0;
        if (event.on && f == static_cast<int> (NoteFxType::Arp))
            arpIndex = 0;
        return;
    }

    if (! event.on)
    {
        for (int k = 0; k < numHeld; ++k)
        {
            if (heldNotes[k] == note && heldChannels[k] == channel)
            {
                for (int j = k + 1; j < numHeld; ++j)
                {
                    heldNotes[j - 1] = heldNotes[j];
                    heldChannels[j - 1] = heldChannels[j];
                    heldVelocities[j - 1] = heldVelocities[j];
                }
                --numHeld;
                break;
            }
        }
        releaseSlot (slot, channel, time);
        return;
    }

    // ---- note-on --------------------------------------------------------------
    const auto tolerance = static_cast<std::int64_t> (sampleRate * 0.002);
    lastInputOn = time;
    for (int p = 0; p < numPending; ++p)
        if (pending[p].fillCandidate && std::llabs (pending[p].boundary - time) <= tolerance)
            pending[p--] = pending[--numPending];

    rememberPitch (note);

    bool alreadyHeld = false;
    for (int k = 0; k < numHeld; ++k)
        alreadyHeld = alreadyHeld || (heldNotes[k] == note && heldChannels[k] == channel);
    if (! alreadyHeld && numHeld < kMaxHeld)
    {
        heldNotes[numHeld] = note;
        heldChannels[numHeld] = channel;
        heldVelocities[numHeld] = event.velocity;
        ++numHeld;
    }

    releaseSlot (slot, channel, time);   // retriggered while still sounding

    if (held (NoteFxType::Mute) || (held (NoteFxType::Skip) && (skipCounter++ % 2) == 1))
        return;

    // The arp plays held notes on the grid instead of when they arrive.
    if (held (NoteFxType::Arp) && ppqValid)
        return;

    int outNote = note;
    if (held (NoteFxType::Random) && pitchPoolSize > 1)
        outNote = pitchPool[prng.nextInt (pitchPoolSize)];
    if (held (NoteFxType::OctaveUp))
        outNote += 12;
    if (held (NoteFxType::OctaveDown))
        outNote -= 12;
    outNote = clamp (outNote, 0, 127);

    float velocity = event.velocity;
    std::int64_t start = time;

    if (held (NoteFxType::Groove))
    {
        start += grooveDelay (time);
        velocity = grooveVelocity (time, velocity);
    }

    if (held (NoteFxType::Strum))
    {
        // Notes arriving together are a chord: spread them out. The first note
        // of each group gets a soft grace hit ahead of it, which is the flam on
        // a single drum hit and the pick noise on a strum.
        const auto together = static_cast<std::int64_t> (sampleRate * 0.003);
        strumCount = time - lastStrumOn <= together ? strumCount + 1 : 0;
        lastStrumOn = time;

        const auto flam = static_cast<std::int64_t> (sampleRate * 0.022);
        if (strumCount == 0)
            playGenerated (start, channel, outNote, velocity * 0.45f, flam / 2);
        start += flam + strumCount * static_cast<std::int64_t> (sampleRate * 0.03);
    }

    slot = Slot();
    slot.notes[0] = static_cast<std::int16_t> (outNote);
    if (held (NoteFxType::Chord))
    {
        if (outNote + 7 <= 127)
            slot.notes[1] = static_cast<std::int16_t> (outNote + 7);
        if (outNote + 12 <= 127)
            slot.notes[2] = static_cast<std::int16_t> (outNote + 12);
    }
    slot.onTime = start;
    slot.active = true;

    for (int k = 0; k < 3; ++k)
        if (slot.notes[k] >= 0)
            startNote (start, channel, slot.notes[k], k == 0 ? velocity : velocity * 0.85f);

    hasLastHit = true;
    lastHitChannel = channel;
    lastHitNote = outNote;
    lastHitVelocity = velocity;

    const auto thirtySecond = static_cast<std::int64_t> (samplesPerBeat / kTicksPerBeat);

    if (held (NoteFxType::Shorten))
    {
        for (int k = 0; k < 3; ++k)
            if (slot.notes[k] >= 0)
                schedule (start + thirtySecond, channel, slot.notes[k], 0.0f, false);
        slot.active = false;
    }

    if (held (NoteFxType::Echo))
        for (int k = 0; k < 3; ++k)
            if (slot.notes[k] >= 0)
                playGenerated (start + static_cast<std::int64_t> (samplesPerBeat * 0.5), channel,
                               slot.notes[k], velocity * 0.55f, thirtySecond);
}

void NoteFx::releaseSlot (Slot& slot, int channel, std::int64_t time)
{
    if (! slot.active)
        return;

    // A strummed or grooved note may not have started yet: its note-off waits
    // until just after its note-on, never before it.
    for (const auto n : slot.notes)
    {
        if (n < 0)
            continue;
        if (slot.onTime >= time)
            schedule (slot.onTime + 1, channel, n, 0.0f, false);
        else
            emit (time, channel, n, 0.0f, false);
    }
    slot = Slot();
}

void NoteFx::startNote (std::int64_t time, int channel, int note, float velocity)
{
    if (time < clock + blockSamples)
        emit (time, channel, note, velocity, true);
    else
        schedule (time, channel, note, velocity, true);
}

double NoteFx::ppqAt (std::int64_t time) const noexcept
{
    return blockPpq + static_cast<double> (time - clock) / samplesPerBeat;
}

std::int64_t NoteFx::grooveDelay (std::int64_t time) noexcept
{
    // Notes can only be pushed later, never pulled early: a few ms of drift,
    // plus the swing push for anything sitting on an off-beat 16th.
    std::int64_t delay = prng.nextInt (static_cast<int> (sampleRate * 0.006) + 1);
    if (ppqValid)
    {
        const double sixteenths = ppqAt (time) * 4.0;
        const auto nearest = static_cast<std::int64_t> (std::llround (sixteenths));
        if (std::abs (sixteenths - static_cast<double> (nearest)) < 0.25 && positiveMod (nearest, 2) == 1)
            delay += static_cast<std::int64_t> (settings.swing.load (std::memory_order_relaxed)
                                                * samplesPerBeat * 0.25);
    }
    return delay;
}

float NoteFx::grooveVelocity (std::int64_t time, float velocity) noexcept
{
    float accent = 1.0f;
    if (ppqValid)
    {
        const auto nearest = static_cast<std::int64_t> (std::llround (ppqAt (time) * 4.0));
        accent = positiveMod (nearest, 4) == 0 ? 1.15f : (positiveMod (nearest, 2) == 1 ? 0.8f : 1.0f);
    }
    return clamp (velocity * accent * (0.92f + 0.16f * prng.nextFloat()), 0.05f, 1.0f);
}

void NoteFx::handleTick (std::int64_t tick, std::int64_t time)
{
    lastTick = tick;

    const bool onSixteenth = positiveMod (tick, 2) == 0;
    const bool onEighth    = positiveMod (tick, 4) == 0;
    const auto step        = static_cast<int> (positiveMod (tick / 2, kStepsPerBar));
    const auto thirtySecond = static_cast<std::int64_t> (samplesPerBeat / kTicksPerBeat);
    const auto tolerance   = static_cast<std::int64_t> (sampleRate * 0.002);
    const int channel      = hasLastHit ? lastHitChannel : 1;

    // Groove swings the generated notes too, so added hats sit with the clip.
    const bool groove = held (NoteFxType::Groove);
    const bool offBeat = onSixteenth && positiveMod (tick / 2, 2) == 1;
    const std::int64_t swingShift = groove && offBeat
        ? static_cast<std::int64_t> (settings.swing.load (std::memory_order_relaxed) * samplesPerBeat * 0.25)
        : 0;
    const auto feel = [this, groove, time] (float velocity)
    {
        return groove ? grooveVelocity (time, velocity) : velocity;
    };

    if (held (NoteFxType::Repeat) && hasLastHit)
    {
        const int rate = clamp (settings.repeatRate.load (std::memory_order_relaxed), 0, 2);
        const int every = rate == 0 ? 4 : (rate == 1 ? 2 : 1);
        if (positiveMod (tick, every) == 0)
            playGenerated (time + swingShift, lastHitChannel, lastHitNote, feel (lastHitVelocity),
                           std::max<std::int64_t> (1, thirtySecond * every / 2));
    }

    if (held (NoteFxType::Arp) && numHeld > 0)
    {
        const int rate = clamp (settings.repeatRate.load (std::memory_order_relaxed), 0, 2);
        const int every = rate == 0 ? 4 : (rate == 1 ? 2 : 1);
        if (positiveMod (tick, every) == 0)
        {
            // Up through the held notes, lowest first.
            int order[kMaxHeld];
            for (int k = 0; k < numHeld; ++k)
                order[k] = k;
            std::sort (order, order + numHeld, [this] (int a, int b) { return heldNotes[a] < heldNotes[b]; });

            const int k = order[arpIndex++ % numHeld];
            int note = heldNotes[k];
            if (held (NoteFxType::OctaveUp))
                note += 12;
            if (held (NoteFxType::OctaveDown))
                note -= 12;
            playGenerated (time + swingShift, heldChannels[k], clamp (note, 0, 127), feel (heldVelocities[k]),
                           std::max<std::int64_t> (1, thirtySecond * every / 2));
        }
    }

    // Fills wait a couple of milliseconds before they commit, so a real note that
    // lands just after the grid point (late host timing, humanised clips) wins.
    const bool fillHere = (held (NoteFxType::Fill) && onSixteenth)
                       || (held (NoteFxType::HalfFill) && onEighth);
    if (fillHere && hasLastHit && time - lastInputOn > tolerance)
        schedule (time + tolerance, lastHitChannel, lastHitNote, lastHitVelocity, true, true, time);

    if (held (NoteFxType::KickSnare) && onSixteenth)
    {
        const float velocity = triggerVelocity[static_cast<int> (NoteFxType::KickSnare)];
        if (step == 0 || step == 8 || step == 10)
            playGenerated (time + swingShift, channel, settings.kickNote.load (std::memory_order_relaxed), feel (velocity), thirtySecond);
        if (step == 4 || step == 12)
            playGenerated (time + swingShift, channel, settings.snareNote.load (std::memory_order_relaxed), feel (velocity), thirtySecond);
    }

    if (held (NoteFxType::Hats) && onSixteenth)
    {
        const float velocity = triggerVelocity[static_cast<int> (NoteFxType::Hats)]
                             * (onEighth ? 1.0f : 0.6f);
        playGenerated (time + swingShift, channel, settings.hatNote.load (std::memory_order_relaxed), feel (velocity), thirtySecond);
    }
}

void NoteFx::schedule (std::int64_t time, int channel, int note, float velocity, bool on,
                       bool fillCandidate, std::int64_t boundary)
{
    // Note-ons give up before note-offs do, so a full queue can never leave a
    // note hanging.
    const int limit = on ? kMaxPending - kOffHeadroom : kMaxPending;
    if (numPending >= limit)
        return;

    Pending& p = pending[numPending++];
    p.time = time;
    p.boundary = boundary;
    p.channel = channel;
    p.note = clamp (note, 0, 127);
    p.velocity = velocity;
    p.on = on;
    p.fillCandidate = fillCandidate;
}

void NoteFx::playGenerated (std::int64_t time, int channel, int note, float velocity, std::int64_t length)
{
    if (numPending > kMaxPending - kOffHeadroom - 1)
        return;
    schedule (time, channel, note, clamp (velocity, 0.0f, 1.0f), true);
    schedule (time + std::max<std::int64_t> (1, length), channel, note, 0.0f, false);
}

void NoteFx::emit (std::int64_t time, int channel, int note, float velocity, bool on)
{
    if (outCount >= outMax)
        return;

    NoteEvent& e = out[outCount++];
    e.sampleOffset = clamp (static_cast<int> (time - clock), 0, blockSamples - 1);
    e.channel = channel;
    e.note = clamp (note, 0, 127);
    e.velocity = on ? clamp (velocity, 0.0f, 1.0f) : 0.0f;
    e.on = on;
}

void NoteFx::rememberPitch (int note)
{
    for (int k = 0; k < pitchPoolSize; ++k)
        if (pitchPool[k] == note)
            return;

    pitchPool[pitchPoolNext] = note;
    pitchPoolNext = (pitchPoolNext + 1) % kPitchPool;
    pitchPoolSize = std::min (pitchPoolSize + 1, kPitchPool);
}

} // namespace opxy
