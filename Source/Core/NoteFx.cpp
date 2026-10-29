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

    for (auto& channel : mapped)
        for (auto& note : channel)
            note = -1;
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
    std::int16_t& slot = mapped[channel - 1][note];

    const int base = settings.baseNote.load (std::memory_order_relaxed);
    if (isTriggerNote (note, base))
    {
        const int f = note - base;
        noteHeld[f] = event.on;
        if (event.on)
            triggerVelocity[f] = std::max (0.05f, event.velocity);
        if (! event.on && f == static_cast<int> (NoteFxType::Skip))
            skipCounter = 0;
        return;
    }

    if (! event.on)
    {
        if (slot >= 0)
            emit (time, channel, slot, 0.0f, false);
        slot = -1;
        return;
    }

    // ---- note-on --------------------------------------------------------------
    const auto tolerance = static_cast<std::int64_t> (sampleRate * 0.002);
    lastInputOn = time;
    for (int p = 0; p < numPending; ++p)
        if (pending[p].fillCandidate && std::llabs (pending[p].boundary - time) <= tolerance)
            pending[p--] = pending[--numPending];

    rememberPitch (note);

    if (slot >= 0)   // retriggered while still sounding: close the old one first
        emit (time, channel, slot, 0.0f, false);

    if (held (NoteFxType::Mute) || (held (NoteFxType::Skip) && (skipCounter++ % 2) == 1))
    {
        slot = -2;
        return;
    }

    int outNote = note;
    if (held (NoteFxType::Random) && pitchPoolSize > 1)
        outNote = pitchPool[prng.nextInt (pitchPoolSize)];
    if (held (NoteFxType::OctaveUp))
        outNote += 12;
    if (held (NoteFxType::OctaveDown))
        outNote -= 12;
    outNote = clamp (outNote, 0, 127);

    emit (time, channel, outNote, event.velocity, true);

    hasLastHit = true;
    lastHitChannel = channel;
    lastHitNote = outNote;
    lastHitVelocity = event.velocity;

    const auto thirtySecond = static_cast<std::int64_t> (samplesPerBeat / kTicksPerBeat);

    if (held (NoteFxType::Shorten))
    {
        schedule (time + thirtySecond, channel, outNote, 0.0f, false);
        slot = -2;
    }
    else
    {
        slot = static_cast<std::int16_t> (outNote);
    }

    if (held (NoteFxType::Echo))
        playGenerated (time + static_cast<std::int64_t> (samplesPerBeat * 0.5), channel, outNote,
                       event.velocity * 0.55f, thirtySecond);
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

    if (held (NoteFxType::Repeat) && hasLastHit)
    {
        const int rate = clamp (settings.repeatRate.load (std::memory_order_relaxed), 0, 2);
        const int every = rate == 0 ? 4 : (rate == 1 ? 2 : 1);
        if (positiveMod (tick, every) == 0)
            playGenerated (time, lastHitChannel, lastHitNote, lastHitVelocity,
                           std::max<std::int64_t> (1, thirtySecond * every / 2));
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
            playGenerated (time, channel, settings.kickNote.load (std::memory_order_relaxed), velocity, thirtySecond);
        if (step == 4 || step == 12)
            playGenerated (time, channel, settings.snareNote.load (std::memory_order_relaxed), velocity, thirtySecond);
    }

    if (held (NoteFxType::Hats) && onSixteenth)
    {
        const float velocity = triggerVelocity[static_cast<int> (NoteFxType::Hats)]
                             * (onEighth ? 1.0f : 0.6f);
        playGenerated (time, channel, settings.hatNote.load (std::memory_order_relaxed), velocity, thirtySecond);
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
