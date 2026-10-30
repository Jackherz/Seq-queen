// -----------------------------------------------------------------------------
//  seqqueen.notes: the Seq Queen note punches as a Max external, for the
//  Max for Live device that sits in front of an instrument on one track.
//
//  Same engine as the VST3 (opxy::NoteFx); this file only adapts Max to it:
//
//    inlet      raw MIDI bytes from [midiin]
//               padpress <i>, padbang <i>   from the pad textbuttons (see below)
//               pad <i> <0|1>     pad pressed / released (direct form, for tests)
//               latch <0|1>       pads latch (press on, press off) or are momentary
//               playing <0|1>, tempo <bpm>, beats <position>
//                                 Live's transport, from [plugsync~]
//               kick|snare|hat <note>, rate <0-2>, swing <0-0.6>
//    outlet     one outlet, so the patch never depends on outlet order:
//               <int>                 raw MIDI bytes for [midiout]
//               punch <fx> <strength> punch Seq Queen FX (0 = release); notes
//                                     C0..A0 (24-33) are Seq Queen FX's triggers
//               active <i> <0|1>      a note punch became active / inactive,
//                                     for lighting the pads
//               sync                  "poll [plugsync~] now": sent every tick, the
//                                     answers come straight back in (above)
//               The patch splits them with [route punch active sync].
//
//  Timing: NoteFx counts in samples, Max in milliseconds. A 1 ms clock (one
//  audio vector when Live runs the scheduler in audio interrupt) advances a
//  virtual 48 kHz sample clock by the real time elapsed (systimer, the CPU
//  clock). The host grid, tempo and play state come from [plugsync~], polled
//  every tick; between Live's updates (one per audio buffer) the position is
//  extrapolated from the tempo and never runs backwards. (Max's global ITM
//  transport turned out not to follow Live's playhead inside a device: the grid
//  punches never fired. Its clock also stops with the transport, which stalled
//  note-offs.) Incoming notes are processed on arrival, so pass-through adds no
//  delay.
// -----------------------------------------------------------------------------

#include "ext.h"
#include "ext_obex.h"

#include "NoteFx.h"

#include <algorithm>
#include <cmath>
#include <new>

namespace
{
constexpr double kVirtualRate = 48000.0;
constexpr int kFxFirstNote = 24;      // C0 in Live's numbering
constexpr int kNumFxTriggers = 10;    // Seq Queen FX has ten effects
constexpr int kMaxIn = 256;
constexpr int kMaxOut = 1024;
constexpr double kPressBangWindowMs = 5.0;   // a press's own bang arrives with it
constexpr double kMaxExtrapolationMs = 100.0;
} // namespace

struct t_sqnotes
{
    t_object ob;
    void* outlet;
    t_clock* clock;

    opxy::NoteFx* fx;
    opxy::NoteEvent* in;
    opxy::NoteEvent* out;
    int numIn;

    // Running-status MIDI parser for note messages.
    int status;
    int data[2];
    int have;

    double lastMs;
    bool shownActive[opxy::kNumNoteFx];

    // Pads: momentary (held while pressed) or latched (press on, press off).
    bool latchMode;
    bool latched[opxy::kNumNoteFx];
    double lastPressMs[opxy::kNumNoteFx];

    // Live's transport, as last reported by [plugsync~].
    bool hostPlaying;
    double hostTempo;
    double hostBeats;          // last reported position, in quarter notes
    double hostBeatsMs;        // when that position changed
    bool wasPlaying;
    double lastPpq;            // where the previous step ended
};

static t_class* sqnotes_class = nullptr;

static void sqnotes_step (t_sqnotes* x)
{
    // Ask [plugsync~] for Live's transport; its answers arrive (re-entrantly)
    // as playing/tempo/beats before outlet_anything returns.
    outlet_anything (x->outlet, gensym ("sync"), 0, nullptr);

    const double now = systimer_gettime();
    const double elapsedMs = std::max (0.0, now - x->lastMs);
    x->lastMs = now;

    // At least one sample, so notes arriving in the same millisecond still move
    // the engine forward (and stay in order).
    const int numSamples = std::max (1, static_cast<int> (std::lround (elapsedMs * kVirtualRate / 1000.0)));

    opxy::TransportInfo transport;
    transport.playing = x->hostPlaying;
    transport.bpm = x->hostTempo > 1.0 ? x->hostTempo : 120.0;
    transport.hasPpq = true;

    // Live reports a new position once per audio buffer; in between, move on at
    // the tempo so the 32nd grid stays smooth.
    double ppqNow = x->hostBeats;
    if (x->hostPlaying)
        ppqNow += std::min (now - x->hostBeatsMs, kMaxExtrapolationMs) / 60000.0 * transport.bpm;

    // Continuous playback: start exactly where the last step ended, so no grid
    // point is skipped or played twice. Otherwise (start, relocate, loop jump)
    // just take the new position.
    const bool continuous = x->hostPlaying && x->wasPlaying
                         && ppqNow >= x->lastPpq && ppqNow - x->lastPpq < 0.5;
    if (x->hostPlaying && x->wasPlaying && ppqNow < x->lastPpq && x->lastPpq - ppqNow < 0.05)
        ppqNow = x->lastPpq;   // a fresh report slightly behind our estimate: don't step back
    transport.ppqAtBlockStart = continuous ? x->lastPpq
                                           : ppqNow - (elapsedMs / 1000.0) * transport.bpm / 60.0;
    x->lastPpq = ppqNow;
    x->wasPlaying = x->hostPlaying;

    // Whatever arrived since the last step happened "now", at the block's end.
    for (int i = 0; i < x->numIn; ++i)
        x->in[i].sampleOffset = numSamples - 1;

    const int count = x->fx->process (x->in, x->numIn, numSamples, transport, x->out, kMaxOut);
    x->numIn = 0;

    for (int i = 0; i < count; ++i)
    {
        const opxy::NoteEvent& e = x->out[i];
        const int channel = std::max (1, std::min (16, e.channel)) - 1;
        outlet_int (x->outlet, (e.on ? 0x90 : 0x80) | channel);
        outlet_int (x->outlet, e.note);
        outlet_int (x->outlet, e.on ? std::max (1, static_cast<int> (std::lround (e.velocity * 127.0f))) : 0);
    }

    for (int f = 0; f < opxy::kNumNoteFx; ++f)
    {
        const bool active = x->fx->isActive (f);
        if (active != x->shownActive[f])
        {
            x->shownActive[f] = active;
            t_atom atoms[2];
            atom_setlong (atoms, f);
            atom_setlong (atoms + 1, active ? 1 : 0);
            outlet_anything (x->outlet, gensym ("active"), 2, atoms);
        }
    }
}

static void sqnotes_tick (t_sqnotes* x)
{
    sqnotes_step (x);
    clock_fdelay (x->clock, 1.0);
}

static void sqnotes_note (t_sqnotes* x, int status, int note, int velocity)
{
    const bool on = (status & 0xF0) == 0x90 && velocity > 0;
    const int fxIndex = note - kFxFirstNote;

    if (fxIndex >= 0 && fxIndex < kNumFxTriggers)
    {
        t_atom atoms[2];
        atom_setlong (atoms, fxIndex);
        atom_setfloat (atoms + 1, on ? std::max (0.01, velocity / 127.0) : 0.0);
        outlet_anything (x->outlet, gensym ("punch"), 2, atoms);
        return;
    }

    if (x->numIn < kMaxIn)
    {
        opxy::NoteEvent& e = x->in[x->numIn++];
        e.channel = (status & 0x0F) + 1;
        e.note = note;
        e.velocity = static_cast<float> (velocity) / 127.0f;
        e.on = on;
    }
    sqnotes_step (x);
}

static void sqnotes_int (t_sqnotes* x, long byte)
{
    const int b = static_cast<int> (byte) & 0xFF;

    if (b >= 0xF8)                    // realtime: pass straight through, keep parsing
    {
        outlet_int (x->outlet, b);
        return;
    }

    if (b & 0x80)
    {
        x->status = b;
        x->have = 0;
        const int type = b & 0xF0;
        if (type != 0x80 && type != 0x90)
            outlet_int (x->outlet, b);   // not a note: pass the whole message on
        return;
    }

    const int type = x->status & 0xF0;
    if (type != 0x80 && type != 0x90)
    {
        outlet_int (x->outlet, b);
        return;
    }

    x->data[x->have++] = b;
    if (x->have == 2)
    {
        x->have = 0;                   // running status: the next pair reuses it
        sqnotes_note (x, x->status, x->data[0], x->data[1]);
    }
}

static void sqnotes_pad (t_sqnotes* x, long index, long pressed)
{
    if (index < 0 || index >= opxy::kNumNoteFx)
        return;
    const int i = static_cast<int> (index);

    if (x->latchMode)
    {
        if (pressed != 0)   // each press flips it; releases don't matter
        {
            x->latched[i] = ! x->latched[i];
            x->fx->setPadHeld (i, x->latched[i]);
        }
    }
    else
    {
        x->fx->setPadHeld (i, pressed != 0);
    }
}

// The pad textbuttons bang their left outlet on press and again on release, and
// send their text out the middle outlet on press only. A bang within a few ms of
// a press is that press's own bang; any other bang is the release. This works
// whichever of the two outlets fires first.
static void sqnotes_padpress (t_sqnotes* x, long index)
{
    if (index < 0 || index >= opxy::kNumNoteFx)
        return;
    x->lastPressMs[index] = systimer_gettime();
    sqnotes_pad (x, index, 1);
}

static void sqnotes_padbang (t_sqnotes* x, long index)
{
    if (index < 0 || index >= opxy::kNumNoteFx)
        return;
    if (systimer_gettime() - x->lastPressMs[index] >= kPressBangWindowMs)
        sqnotes_pad (x, index, 0);
}

static void sqnotes_playing (t_sqnotes* x, long on) { x->hostPlaying = on != 0; }
static void sqnotes_tempo (t_sqnotes* x, double bpm) { x->hostTempo = bpm; }
static void sqnotes_beats (t_sqnotes* x, double beats)
{
    // Repeated reports within one audio buffer carry the same position; only a
    // change restarts the extrapolation.
    if (beats != x->hostBeats)
    {
        x->hostBeats = beats;
        x->hostBeatsMs = systimer_gettime();
    }
}

static void sqnotes_latch (t_sqnotes* x, long on)
{
    x->latchMode = on != 0;

    // Switching mode lets go of everything, so nothing stays on invisibly.
    for (int i = 0; i < opxy::kNumNoteFx; ++i)
    {
        x->latched[i] = false;
        x->fx->setPadHeld (i, false);
    }
}

static void sqnotes_kick (t_sqnotes* x, long note)  { x->fx->getSettings().kickNote.store (static_cast<int> (note)); }
static void sqnotes_snare (t_sqnotes* x, long note) { x->fx->getSettings().snareNote.store (static_cast<int> (note)); }
static void sqnotes_hat (t_sqnotes* x, long note)   { x->fx->getSettings().hatNote.store (static_cast<int> (note)); }
static void sqnotes_rate (t_sqnotes* x, long rate)  { x->fx->getSettings().repeatRate.store (static_cast<int> (rate)); }
static void sqnotes_swing (t_sqnotes* x, double amount)
{
    x->fx->getSettings().swing.store (static_cast<float> (std::max (0.0, std::min (0.6, amount))));
}

static void sqnotes_assist (t_sqnotes*, void*, long io, long index, char* text)
{
    (void) index;
    if (io == ASSIST_INLET)
        snprintf_zero (text, 256, "MIDI bytes from midiin; pad, latch, kick, snare, hat, rate, swing");
    else
        snprintf_zero (text, 256, "MIDI bytes; punch <fx> <strength>; active <pad> <0/1>");
}

static void* sqnotes_new (t_symbol*, long, t_atom*)
{
    auto* x = static_cast<t_sqnotes*> (object_alloc (sqnotes_class));
    if (x == nullptr)
        return nullptr;

    x->outlet = outlet_new (x, nullptr);

    x->fx  = new (std::nothrow) opxy::NoteFx();
    x->in  = new (std::nothrow) opxy::NoteEvent[kMaxIn];
    x->out = new (std::nothrow) opxy::NoteEvent[kMaxOut];
    if (x->fx == nullptr || x->in == nullptr || x->out == nullptr)
    {
        object_error (reinterpret_cast<t_object*> (x), "out of memory");
        return x;
    }

    x->fx->prepare (kVirtualRate);
    x->fx->getSettings().baseNote.store (0);   // C-2: the sixteen punches sit below every Drum Rack pad
    x->numIn = 0;
    x->status = 0;
    x->have = 0;
    for (auto& shown : x->shownActive)
        shown = false;
    x->latchMode = false;
    for (auto& l : x->latched)
        l = false;
    for (auto& t : x->lastPressMs)
        t = -1.0e9;

    x->hostPlaying = false;
    x->hostTempo = 120.0;
    x->hostBeats = 0.0;
    x->hostBeatsMs = systimer_gettime();
    x->wasPlaying = false;
    x->lastPpq = 0.0;

    x->lastMs = systimer_gettime();
    x->clock = clock_new (x, reinterpret_cast<method> (sqnotes_tick));
    clock_fdelay (x->clock, 1.0);
    return x;
}

static void sqnotes_free (t_sqnotes* x)
{
    if (x->clock != nullptr)
    {
        clock_unset (x->clock);
        object_free (x->clock);
    }
    delete x->fx;
    delete[] x->in;
    delete[] x->out;
}

extern "C" void ext_main (void*)
{
    t_class* c = class_new ("seqqueen.notes",
                            reinterpret_cast<method> (sqnotes_new),
                            reinterpret_cast<method> (sqnotes_free),
                            sizeof (t_sqnotes), nullptr, A_GIMME, 0);

    class_addmethod (c, reinterpret_cast<method> (sqnotes_int),    "int",    A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_pad),    "pad",    A_LONG, A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_latch),  "latch",  A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_padpress), "padpress", A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_padbang),  "padbang",  A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_playing),  "playing",  A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_tempo),    "tempo",    A_FLOAT, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_beats),    "beats",    A_FLOAT, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_kick),   "kick",   A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_snare),  "snare",  A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_hat),    "hat",    A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_rate),   "rate",   A_LONG, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_swing),  "swing",  A_FLOAT, 0);
    class_addmethod (c, reinterpret_cast<method> (sqnotes_assist), "assist", A_CANT, 0);

    class_register (CLASS_BOX, c);
    sqnotes_class = c;
}
