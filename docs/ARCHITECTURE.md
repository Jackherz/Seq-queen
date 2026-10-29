# Architecture

Two layers, one rule: **`Source/Core` never includes JUCE.**

```
Ableton                         Source/Devices (JUCE)            Source/Core (plain C++17)

audio track ──────────────────► Seq Queen FX  ─── settings ───►  FxBus
MIDI clip ── MIDI To ─────────►  • pads, params    (atomics)       └─ FxChain: 6 × PunchInFx
                                 • notes C1–F1 → hold + velocity

MIDI clip ────────────────────► Seq Queen Notes ── events ────►  NoteFx
                                 • transport (ppq, bpm)            • transforms passing notes
instrument track ◄─ MIDI From ── • MIDI out                       • generators on a 32nd grid
                                 • CC / bend pass straight through
```

## Seq Queen FX, per block

1. Read incoming MIDI: notes `trigger_note … trigger_note + 5` set a held flag and
   record velocity per effect; all MIDI is then cleared (it is an audio effect).
2. For each effect, engaged = MIDI note held **or** pad held **or** `fx_*_on`
   automation; parameters are copied into `FxSettings`.
3. `FxBus::process` scales each wet amount by velocity
   (`amount × (1 − velamt × (1 − velocity))`), pushes macros and ramp time, and runs
   the six-effect `FxChain` in place, chunked at `kMaxBlockSize`.

Mono tracks run the stereo chain on a preallocated scratch copy and fold back down.

## Seq Queen Notes, per block

1. Read the host transport: playing, bpm, ppq.
2. Split incoming MIDI: note on/offs become `NoteEvent`s; everything else passes
   through untouched.
3. `NoteFx::process` merges the input notes and this block's 32nd-note grid points in
   time order (input first on a tie):
   * **Trigger notes** (twelve from `trigger_note`) set held flags and are swallowed.
   * **Input notes** are muted/skipped, transposed (octave, random), shortened or
     echoed. A per-channel, per-pitch table remembers what each sounding input note
     became, so its note-off closes the right output pitch even if the trigger was
     released in between.
   * **Grid points** (only while the transport runs) fire repeat, fill, half fill,
     kick+snare and hats.
4. Everything with a future time (generated note-offs, echoes, fill candidates) sits
   in a fixed pending queue keyed by absolute sample time and is emitted in the
   block it falls in. Output is sorted, note-offs first on a tie.

### Timing details

* The grid is derived from ppq every block, so it follows loops and relocates. A
  `lastTick` guard stops a grid point from firing twice when hosts round ppq
  differently between blocks; a jump back of more than a few ticks restarts it.
* Fills are *candidates*: they commit 2 ms after their grid point and are cancelled
  if a real note arrives within 2 ms either side, so fills never double a real hit.
* The pending queue keeps 16 slots for note-offs only; when it is full, new
  note-ons are dropped rather than ever losing an off.

## Threading

| Data | Writer → reader | Mechanism |
|---|---|---|
| Parameters | host → audio | APVTS raw parameter pointers |
| Pad holds | UI → audio | `std::atomic<bool>` per pad |
| FX settings | processor → `FxBus` | `std::atomic` fields on `FxSettings` |
| Lit pads, depth meters | audio → UI | `std::atomic` in `FxBus` / `NoteFx`, polled by a timer |

No locks on the audio thread, no allocation after `prepareToPlay`.

## Memory

FX delay lines are allocated in `prepare()`: roughly 3 MB at 48 kHz for the tape
stop, delay throw and stutter buffers. `NoteFx` is fixed-size (a 16×128 table and a
256-entry queue).

## Why two devices

The project began as one instrument with its own 8-track sequencer, synth and these
FX. It was split because the user wants Ableton's sequencer, and the all-in-one UI was
too much. Live doesn't host third-party MIDI effects, which is why Seq Queen Notes is
an instrument that outputs MIDI. The instrument's sequencer and synth
(`Engine`, `Sequencer`, `SynthEngine`, `Pattern`, `PatternState`) remain in the core,
used only by the tests and `tools/render_demo.cpp`.

## Extending it

* New punch-in FX → `docs/ADDING_AN_FX.md`.
* New note punch → add to `NoteFxType` (append, so saved trigger layouts keep their
  keys), give it a name and description in `NoteFx.cpp`, handle it in
  `handleInput` (transforms) or `handleTick` (generators), pick its colour in
  `NotesEditor.cpp`, and add a test. The editor lays pads out six per row.
