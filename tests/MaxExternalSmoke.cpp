// -----------------------------------------------------------------------------
//  Smoke test for seqqueen.notes.mxo without Max.
//
//  Loads the real bundle with dlopen and provides the handful of Max API
//  functions it uses, with a fake transport (ITM) and a manually driven clock.
//  Checks the parts the core tests can't: MIDI byte parsing, outlet order, the
//  FX trigger notes, pad messages, and the clock-driven generators.
//
//      seqqueen_max_smoke path/to/seqqueen.notes.mxo
// -----------------------------------------------------------------------------

#include "ext.h"
#include "ext_obex.h"
#include "ext_itm.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>

namespace
{
struct FakeClass
{
    method newMethod = nullptr;
    method freeMethod = nullptr;
    long size = 0;
    std::map<std::string, method> methods;
};

FakeClass gClass;
int gOutletCount = 0;
std::vector<int> gMidi;                                       // ints out of the outlet
std::map<std::string, std::vector<std::vector<double>>> gMessages;   // "punch", "active"
std::map<t_symbol*, std::string> gSymbols;
void* gClockOwner = nullptr;
method gClockFn = nullptr;

// The fake Live transport, as [plugsync~] would report it: the position only
// updates once per 128-sample audio buffer, like Live's.
double gTempo = 120.0, gBeats = 0.0, gReportedBeats = 0.0, gSinceReportMs = 0.0;
void* gObject = nullptr;
double gSystemMs = 1000.0;   // the CPU clock: always moves
long gRunning = 0;

int gFailures = 0;
void expect (bool ok, const char* what)
{
    std::printf ("%s %s\n", ok ? "  ok  " : "  FAIL", what);
    gFailures += ok ? 0 : 1;
}
} // namespace

extern "C"
{
t_class* class_new (C74_CONST char*, C74_CONST method mnew, C74_CONST method mfree, long size, C74_CONST method, short, ...)
{
    gClass.newMethod = mnew;
    gClass.freeMethod = mfree;
    gClass.size = size;
    return reinterpret_cast<t_class*> (&gClass);
}
t_max_err class_addmethod (t_class*, C74_CONST method m, C74_CONST char* name, ...)
{
    gClass.methods[name] = m;
    return MAX_ERR_NONE;
}
t_max_err class_register (t_symbol*, t_class*) { return MAX_ERR_NONE; }
void* object_alloc (t_class*) { return std::calloc (1, static_cast<std::size_t> (gClass.size)); }
t_max_err object_free (void*) { return MAX_ERR_NONE; }
void object_error (t_object*, C74_CONST char* s, ...) { std::printf ("object_error: %s\n", s); }
t_symbol* gensym (C74_CONST char* name)
{
    static std::map<std::string, t_symbol> table;
    t_symbol* s = &table[name];
    gSymbols[s] = name;
    return s;
}
void* outlet_new (void*, C74_CONST char*) { return reinterpret_cast<void*> (static_cast<intptr_t> (++gOutletCount)); }
void* outlet_int (void*, t_atom_long n)
{
    gMidi.push_back (static_cast<int> (n));
    return nullptr;
}
void* outlet_anything (void*, t_symbol* s, short ac, t_atom* av)
{
    if (gSymbols[s] == "sync")   // the patch polls [plugsync~]; answers come straight back
    {
        reinterpret_cast<void (*) (void*, double)> (gClass.methods["beats"]) (gObject, gReportedBeats);
        reinterpret_cast<void (*) (void*, double)> (gClass.methods["tempo"]) (gObject, gTempo);
        reinterpret_cast<void (*) (void*, long)> (gClass.methods["playing"]) (gObject, gRunning);
        return nullptr;
    }
    std::vector<double> values;
    for (int i = 0; i < ac; ++i)
        values.push_back (av[i].a_type == A_FLOAT ? av[i].a_w.w_float : static_cast<double> (av[i].a_w.w_long));
    gMessages[gSymbols[s]].push_back (values);
    return nullptr;
}
t_max_err atom_setlong (t_atom* a, t_atom_long b) { a->a_type = A_LONG; a->a_w.w_long = b; return MAX_ERR_NONE; }
t_max_err atom_setfloat (t_atom* a, double b) { a->a_type = A_FLOAT; a->a_w.w_float = b; return MAX_ERR_NONE; }
int snprintf_zero (char* buffer, size_t count, const char* format, ...)
{
    va_list args;
    va_start (args, format);
    const int written = std::vsnprintf (buffer, count, format, args);
    va_end (args);
    return written;
}
t_clock* clock_new (void* obj, method fn) { gClockOwner = obj; gClockFn = fn; return reinterpret_cast<t_clock*> (&gClockFn); }
void clock_fdelay (void*, double) {}
void clock_unset (void*) {}
double systimer_gettime() { return gSystemMs; }
}

namespace
{
void sendBytes (std::initializer_list<int> bytes)
{
    auto fn = reinterpret_cast<void (*) (void*, long)> (gClass.methods["int"]);
    for (int b : bytes)
        fn (gObject, b);
}

void advanceMs (double ms)
{
    // 1 ms clock ticks, with the fake transport moving at gTempo while running.
    for (double t = 0.0; t < ms; t += 1.0)
    {
        gSystemMs += 1.0;
        if (gRunning)
        {
            gBeats += gTempo / 60000.0;
            gSinceReportMs += 1.0;
            if (gSinceReportMs >= 128.0 / 48.0)
            {
                gSinceReportMs = 0.0;
                gReportedBeats = gBeats;
            }
        }
        reinterpret_cast<void (*) (void*)> (gClockFn) (gClockOwner);
    }
}

int countNoteOns (int note)
{
    int count = 0;
    for (std::size_t i = 0; i + 2 < gMidi.size(); i += 3)
        count += ((gMidi[i] & 0xF0) == 0x90 && gMidi[i + 1] == note && gMidi[i + 2] > 0) ? 1 : 0;
    return count;
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf ("usage: seqqueen_max_smoke path/to/seqqueen.notes.mxo\n");
        return 2;
    }

    const std::string binary = std::string (argv[1]) + "/Contents/MacOS/seqqueen.notes";
    void* handle = dlopen (binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        std::printf ("dlopen failed: %s\n", dlerror());
        return 1;
    }
    auto extMain = reinterpret_cast<void (*) (void*)> (dlsym (handle, "ext_main"));
    expect (extMain != nullptr, "ext_main exported");
    extMain (nullptr);
    expect (gClass.methods.count ("int") && gClass.methods.count ("pad") && gClass.methods.count ("swing"),
            "methods registered");

    gObject = reinterpret_cast<void* (*) (t_symbol*, long, t_atom*)> (gClass.newMethod) (nullptr, 0, nullptr);
    expect (gObject != nullptr && gOutletCount == 1, "object created with one outlet");

    // Pass-through: a note goes straight out, unchanged, with no clock tick needed.
    sendBytes ({ 0x90, 60, 100 });
    expect (gMidi.size() == 3 && gMidi[0] == 0x90 && gMidi[1] == 60 && gMidi[2] == 100, "note-on passes through at once");
    sendBytes ({ 0x80, 60, 0 });
    expect (gMidi.size() == 6 && gMidi[3] == 0x80 && gMidi[4] == 60, "note-off passes through");

    // Running status and a note-on with velocity 0 (= note-off).
    gMidi.clear();
    sendBytes ({ 0x91, 62, 90, 62, 0 });
    expect (gMidi.size() == 6 && gMidi[0] == 0x91 && gMidi[3] == 0x81, "running status + velocity-0 note-off");

    // Non-note messages pass through byte for byte.
    gMidi.clear();
    sendBytes ({ 0xB0, 7, 100 });
    expect (gMidi == std::vector<int> ({ 0xB0, 7, 100 }), "CC passes through");

    // FX trigger notes become punch messages and never reach the instrument.
    gMidi.clear();
    sendBytes ({ 0x90, 24 + 3, 127 });
    sendBytes ({ 0x80, 24 + 3, 0 });
    expect (gMidi.empty(), "FX trigger notes are swallowed");
    expect (gMessages["punch"].size() == 2 && gMessages["punch"][0][0] == 3 && gMessages["punch"][0][1] > 0.99
                && gMessages["punch"][1][1] == 0.0,
            "FX trigger note punches and releases Seq Queen FX");

    // Mute via its trigger note (C-2 = 0): the next note is swallowed.
    gMidi.clear();
    sendBytes ({ 0x90, 0, 100, 0x90, 64, 100, 0x80, 64, 0, 0x80, 0, 0 });
    expect (gMidi.empty(), "mute trigger swallows notes");

    // Hats pad while the transport runs: 1/16 hats at 120 bpm = 8 per second.
    gMidi.clear();
    gRunning = 1;
    reinterpret_cast<void (*) (void*, long, long)> (gClass.methods["pad"]) (gObject, 5, 1);
    advanceMs (1000.0);
    const int hats = countNoteOns (42);
    expect (hats >= 7 && hats <= 9, "hats pad adds 16th hats on the clock");
    expect (! gMessages["active"].empty() && gMessages["active"].back()[0] == 5 && gMessages["active"].back()[1] == 1,
            "pad light turns on");

    reinterpret_cast<void (*) (void*, long, long)> (gClass.methods["pad"]) (gObject, 5, 0);
    advanceMs (200.0);
    int ons = 0, offs = 0;
    for (std::size_t i = 0; i + 2 < gMidi.size(); i += 3)
        ((gMidi[i] & 0xF0) == 0x90 && gMidi[i + 2] > 0 ? ons : offs) += 1;
    expect (ons == offs, "every generated hat got its note-off");
    expect (gMessages["active"].back()[1] == 0, "pad light turns off");

    // Sequenced, like a clip: trigger notes with Live playing. These are the grid
    // punches that never fired when the transport came from Max's own ITM.
    gRunning = 1;
    gMidi.clear();
    sendBytes ({ 0x90, 4, 100 });              // E-2: Kick+Snare
    advanceMs (2000.0);                        // one bar at 120 bpm
    sendBytes ({ 0x80, 4, 0 });
    expect (countNoteOns (36) == 3 && countNoteOns (38) == 2, "kick+snare from a trigger note adds a bar's beat");

    gMidi.clear();
    sendBytes ({ 0x90, 40, 100, 0x80, 40, 0 });   // a hit to fill with
    sendBytes ({ 0x90, 2, 100 });                 // D-2: Fill
    advanceMs (1000.0);                           // eight 16ths
    sendBytes ({ 0x80, 2, 0 });
    advanceMs (100.0);
    expect (countNoteOns (40) >= 8 && countNoteOns (40) <= 9, "fill from a trigger note fills the 16ths");
    int fillOns = 0, fillOffs = 0;
    for (std::size_t i = 0; i + 2 < gMidi.size(); i += 3)
        if (gMidi[i + 1] == 40)
            ((gMidi[i] & 0xF0) == 0x90 && gMidi[i + 2] > 0 ? fillOns : fillOffs) += 1;
    expect (fillOns == fillOffs, "every fill note got its note-off");

    // Regression: with the transport stopped, a shortened note must still end on
    // time. Timing off the transport clock (which stops) left it hanging.
    gRunning = 0;
    gMidi.clear();
    const auto pad = reinterpret_cast<void (*) (void*, long, long)> (gClass.methods["pad"]);
    pad (gObject, 8, 1);                       // Shorten, held
    advanceMs (5.0);
    sendBytes ({ 0x90, 50, 100 });
    advanceMs (150.0);                         // a 32nd at 120 bpm is 62.5 ms
    pad (gObject, 8, 0);
    expect (gMidi.size() == 6 && gMidi[3] == 0x80 && gMidi[4] == 50,
            "shortened note ends on time with the transport stopped");
    sendBytes ({ 0x80, 50, 0 });

    if (gClass.methods.count ("latch") == 0)
    {
        std::printf ("  FAIL no latch method\n\n%d failures\n", ++gFailures);
        return 1;
    }

    // Real pads: textbutton sends "padpress" (middle outlet, press only) and
    // "padbang" (left outlet, press and release). Either may fire first on press.
    {
        const auto press = reinterpret_cast<void (*) (void*, long)> (gClass.methods["padpress"]);
        const auto bang = reinterpret_cast<void (*) (void*, long)> (gClass.methods["padbang"]);
        bool ok = press != nullptr && bang != nullptr;
        for (int order = 0; order < 2 && ok; ++order)
        {
            if (order == 0) { press (gObject, 0); bang (gObject, 0); }
            else            { bang (gObject, 0); press (gObject, 0); }
            advanceMs (50.0);
            gMidi.clear();
            sendBytes ({ 0x90, 60, 100, 0x80, 60, 0 });
            const bool mutedWhileHeld = gMidi.empty();
            bang (gObject, 0);                 // release
            advanceMs (5.0);
            sendBytes ({ 0x90, 61, 100, 0x80, 61, 0 });
            ok = mutedWhileHeld && gMidi.size() == 6 && gMidi[1] == 61;
        }
        expect (ok, "pad press/release works whichever textbutton outlet fires first");

        // Pressing one pad after another must not keep the first one held.
        press (gObject, 6); bang (gObject, 6);       // Octave Up, held
        advanceMs (50.0);
        bang (gObject, 6);                           // let go
        advanceMs (5.0);
        press (gObject, 7); bang (gObject, 7);       // Octave Down, held
        advanceMs (50.0);
        gMidi.clear();
        sendBytes ({ 0x90, 60, 100, 0x80, 60, 0 });
        expect (gMidi.size() == 6 && gMidi[1] == 48, "pressing a new pad doesn't keep the previous one held");
        bang (gObject, 7);
        advanceMs (5.0);
    }

    // Momentary pads: Mute only while pressed.
    const auto latch = reinterpret_cast<void (*) (void*, long)> (gClass.methods["latch"]);
    gMidi.clear();
    pad (gObject, 0, 1);
    advanceMs (2.0);
    sendBytes ({ 0x90, 51, 100, 0x80, 51, 0 });
    pad (gObject, 0, 0);
    advanceMs (2.0);
    sendBytes ({ 0x90, 52, 100, 0x80, 52, 0 });
    expect (gMidi.size() == 6 && gMidi[1] == 52, "momentary mute: muted while pressed, back on release");

    // Latched pads: press once to latch, again to let go.
    latch (gObject, 1);
    gMidi.clear();
    pad (gObject, 0, 1);
    pad (gObject, 0, 0);
    advanceMs (2.0);
    sendBytes ({ 0x90, 53, 100, 0x80, 53, 0 });
    const bool mutedWhileLatched = gMidi.empty();
    pad (gObject, 0, 1);
    pad (gObject, 0, 0);
    advanceMs (2.0);
    sendBytes ({ 0x90, 54, 100, 0x80, 54, 0 });
    expect (mutedWhileLatched && gMidi.size() == 6 && gMidi[1] == 54, "latched mute: on with one press, off with the next");
    expect (gMessages["active"].back()[0] == 0 && gMessages["active"].back()[1] == 0, "latched pad light goes off again");

    // Switching modes lets go of a latched pad.
    pad (gObject, 0, 1);
    latch (gObject, 0);
    advanceMs (2.0);
    gMidi.clear();
    sendBytes ({ 0x90, 55, 100, 0x80, 55, 0 });
    expect (gMidi.size() == 6, "changing pad mode releases latched pads");

    reinterpret_cast<void (*) (void*)> (gClass.freeMethod) (gObject);
    std::printf ("\n%d failures\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
