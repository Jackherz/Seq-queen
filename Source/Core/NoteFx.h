#pragma once

// -----------------------------------------------------------------------------
//  NoteFx: OP-XY style punch-in effects that work on notes, not audio.
//
//  This is what the "Seq Queen Notes" plugin runs. The host's clip plays
//  through it; holding one of twelve trigger notes (an octave starting at
//  `baseNote`) changes what comes out the other side, the way the OP-XY's
//  punch-in keys do:
//
//      C  mute         C# repeat       D  fill         D# half fill
//      E  kick+snare   F  hats         F# octave up    G  octave down
//      G# shorten      A  random       A# skip         B  echo
//
//  Trigger notes are swallowed; everything else is passed through (possibly
//  transformed), and the generators add notes on the host's 16th grid while
//  the transport runs. Every note-on that leaves has a matching note-off, even
//  when a trigger is released halfway through a note.
//
//  Threading: process() runs on the audio thread. The UI only touches the
//  atomics in Settings, setPadHeld() and isActive().
// -----------------------------------------------------------------------------

#include "Types.h"
#include "Prng.h"
#include <atomic>
#include <cstdint>

namespace opxy
{

enum class NoteFxType : int
{
    Mute = 0,
    Repeat,
    Fill,
    HalfFill,
    KickSnare,
    Hats,
    OctaveUp,
    OctaveDown,
    Shorten,
    Random,
    Skip,
    Echo,
    Count
};

inline constexpr int kNumNoteFx = static_cast<int> (NoteFxType::Count);

const char* noteFxName (NoteFxType type) noexcept;
const char* noteFxDescription (NoteFxType type) noexcept;

/** One MIDI note message with its position in the block. Channel is 1..16. */
struct NoteEvent
{
    int   sampleOffset = 0;
    int   channel      = 1;
    int   note         = 60;
    float velocity     = 0.8f;   // 0..1, ignored for note-offs
    bool  on           = true;
};

class NoteFx
{
public:
    struct Settings
    {
        std::atomic<int> baseNote   { 0 };    // first trigger note (0 = C-2 in Live)
        std::atomic<int> kickNote   { 36 };   // notes the generators play (Drum Rack defaults)
        std::atomic<int> snareNote  { 38 };
        std::atomic<int> hatNote    { 42 };
        std::atomic<int> repeatRate { 1 };    // 0 = 1/8, 1 = 1/16, 2 = 1/32
    };

    static constexpr int kMaxPending = 256;
    static constexpr int kPitchPool  = 8;

    NoteFx();

    void prepare (double sampleRate);
    void reset();

    /** `input` must be sorted by sampleOffset. Writes at most `maxOutput`
        events to `output`, sorted by offset (note-offs first on a tie), and
        returns how many it wrote. */
    int process (const NoteEvent* input, int numInput, int numSamples,
                 const TransportInfo& transport, NoteEvent* output, int maxOutput);

    Settings& getSettings() noexcept { return settings; }

    /** On-screen pad: holds a punch exactly like its trigger note would. */
    void setPadHeld (int fx, bool held) noexcept;

    /** True while a punch is held by its note or its pad. For the UI. */
    bool isActive (int fx) const noexcept;

    static bool isTriggerNote (int note, int baseNote) noexcept
    {
        return note >= baseNote && note < baseNote + kNumNoteFx;
    }

private:
    struct Pending
    {
        std::int64_t time = 0;
        std::int64_t boundary = -1;   // fill candidates: the grid point they belong to
        int   channel = 1;
        int   note = 0;
        float velocity = 0.0f;
        bool  on = false;
        bool  fillCandidate = false;
    };

    bool held (NoteFxType type) const noexcept;
    void handleInput (const NoteEvent& event, std::int64_t time);
    void handleTick (std::int64_t tick, std::int64_t time);
    void schedule (std::int64_t time, int channel, int note, float velocity, bool on,
                   bool fillCandidate = false, std::int64_t boundary = -1);
    void playGenerated (std::int64_t time, int channel, int note, float velocity, std::int64_t length);
    void emit (std::int64_t time, int channel, int note, float velocity, bool on);
    void rememberPitch (int note);

    Settings settings;
    Prng prng { 0x5EC0FFEEu };

    double sampleRate = 44100.0;
    double samplesPerBeat = 22050.0;
    std::int64_t clock = 0;            // absolute sample at the start of this block
    std::int64_t lastTick = -1;        // last 32nd-note grid index handled
    std::int64_t lastInputOn = -1000000;

    bool  noteHeld[kNumNoteFx] {};
    float triggerVelocity[kNumNoteFx] {};
    std::atomic<bool> padHeld[kNumNoteFx];
    std::atomic<bool> activeForUi[kNumNoteFx];

    // What each sounding input note was turned into: -1 nothing sounding,
    // -2 swallowed (muted/skipped/shortened, so its note-off must be too).
    std::int16_t mapped[16][128];

    int pitchPool[kPitchPool] {};
    int pitchPoolSize = 0;
    int pitchPoolNext = 0;

    bool  hasLastHit = false;
    int   lastHitChannel = 1;
    int   lastHitNote = 36;
    float lastHitVelocity = 0.8f;
    int   skipCounter = 0;

    Pending pending[kMaxPending];
    int numPending = 0;

    // Per-call output state.
    NoteEvent* out = nullptr;
    int outCount = 0;
    int outMax = 0;
    int blockSamples = 0;
};

} // namespace opxy
