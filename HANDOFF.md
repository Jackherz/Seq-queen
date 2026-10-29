# Handoff — read this before touching anything

You are picking up **Seq Queen**: OP-XY-style punch-in effects for Ableton, written
in C++17 + JUCE 8. It is **two devices** that leave sequencing to the host:

* **Seq Queen FX** (`Source/Devices/FxPlugin`, core `FxBus`): six punch-in FX
  (tape stop, delay throw, filter, stutter, reverb, bitcrush) as an audio effect.
  Triggered by pads, MIDI notes C1–F1 from a clip routed in with *MIDI To*
  (velocity sets depth), or each effect's *Punch* automation parameter.
* **Seq Queen Notes** (`Source/Devices/NotesPlugin`, core `NoteFx`): twelve
  OP-XY-style note punches (mute, repeat, fill, half fill, kick+snare, hats, octave
  up/down, shorten, random, skip, echo). An instrument that outputs MIDI and
  silence, because Live won't host third-party MIDI effects; the instrument track
  takes it as *MIDI From*. Trigger notes are C-2–B-2 so they never collide with a
  Drum Rack.

### Status as of 2026-10-29

* Both devices build (VST3 + AU), pass `auval` and pluginval strictness 8, and
  were checked by eye with the snapshot tool. 40 core tests green.
* **Neither has been tried in Live yet.** That is the next step (section 4).
* **History.** This started as one all-in-one instrument (8-track p-lock sequencer
  + synth + the same FX, in `Source/Plugin`). The user found it confusing and
  overwhelming and wants Live's own sequencer, so it was split into the two devices
  and then removed (git history has it: last commit with it is `de90cc5`). Its
  sequencer, synth and pattern code are still in `Source/Core` because the tests
  and the demo renderer use them; no plugin does.
* **Keep the devices simple and self-explaining.** Every pad says what it does and
  which key triggers it; every knob has a real name and a value in real units.
  That was the user's main complaint about the instrument.
* **JUCE 8.0.14, not 7.** JUCE 7.0.12 cannot build against the macOS 15+ SDK
  (`CGWindowListCreateImage` was removed; `juceaide` fails at configure time) and
  no JUCE 7 release fixes it.

---

## 1. Build

```bash
cmake --preset mac-release            # first run fetches JUCE 8.0.14
cmake --build --preset mac-release -j8
```

Both devices copy themselves into `~/Library/Audio/Plug-Ins/` (`COPY_PLUGIN_AFTER_BUILD`).

Engine only — no JUCE needed, so this must always work, on any machine:

```bash
cmake --preset core
cmake --build --preset core
ctest --preset core                      # 40 tests
```

### Looking at the UI without a host

```bash
./build/mac-release/seqqueen_snapshot_artefacts/Release/seqqueen_snapshot build/snap
```

Writes `fx.png` and `notes.png` at 2x with a pad held in each, plus processing and
draw cost. No window or screen-recording permission needed: use it after every UI
change.

### Validation

```bash
auval -v aumf Sqfx Sqqn          # Seq Queen FX (MusicEffect, so the AU takes MIDI)
auval -v aumu Sqnt Sqqn          # Seq Queen Notes
pluginval --strictness-level 8 --validate "~/Library/Audio/Plug-Ins/VST3/Seq Queen FX.vst3"
```

pluginval isn't in this repo; there is a copy at `~/Documents/AI/silt-plugin/pluginval/`.

### Build notes

* **`-Wfloat-equal` in `ScalarUtils.h`** (`Ramp::isSmoothing`) is the one remaining
  warning. The exact compare is intentional: the ramp snaps exactly to its target
  and a core test relies on it.
* Useful switches: `-DOPXY_JUCE_PATH=/path/to/JUCE` (skip the download),
  `-DOPXY_BUILD_PLUGIN=OFF`, `-DOPXY_UNIVERSAL_BUILD=ON` (arm64 + x86_64).

---

## 2. What exists, and where

```
Source/Core/                     ← JUCE-FREE. Never include JUCE here.
  Types.h                        constants, enums (FxType, ...), TransportInfo
  ScalarUtils.h                  clamp/lerp/pan/Ramp (linear ramp with exact snapping)
  Prng.h                         deterministic xorshift (tests rely on it)
  PunchInFx.h/.cpp               base class: momentary ramp, dry/wet capture, two mix styles
  TapeStopFx                     rate-slaved delay line, freeze + catch-up on release
  DelayThrowFx                   send-style feedback delay, tail rings after release
  FilterSweepFx                  resonant TPT low-pass, glided on engage/release
  StutterFx                      slice freeze + repeat with edge declick
  ReverbBurstFx                  4 comb + 2 allpass per channel, send-style, 1.2 s decay
  BitcrushFx                     bit-depth + sample-rate reduction, light drive
  FxChain.h/.cpp                 the six effects in one bus, processed in enum order
  FxBus.h/.cpp                   FxChain as a standalone insert + velocity rule (FX device)
  NoteFx.h/.cpp                  the twelve note punches on the host's grid (Notes device)
  Engine, Sequencer, SynthEngine,
  Pattern, PatternState          the old instrument; used by tests + demo only

Source/Devices/
  FxPlugin/FxProcessor.*         params, MIDI-note triggers, pads, mono/stereo
  FxPlugin/FxEditor.*            six pads, four named knobs each, trigger-note knob
  NotesPlugin/NotesProcessor.*   transport → NoteFx, MIDI in → NoteFx → MIDI out
  NotesPlugin/NotesEditor.*      twelve pads in two rows, settings row
  */…Entry.cpp                   createPluginFilter(), kept apart for the snapshot tool
  Shared/DeviceUi.h              PunchPad, LabelledKnob, deviceHeader, Live note names
  Shared/Theme.h                 colours, fonts, panel helpers
  Shared/SeqQueenLookAndFeel.*   knob/combo/button drawing

tests/CoreTests.cpp              the safety net. Add to it with every engine change.
tools/snapshot.cpp               headless editor renderer
tools/render_demo.cpp            offline renderer for demo/opxy_demo.wav
docs/ARCHITECTURE.md             how the devices and the core fit together
docs/ADDING_AN_FX.md             recipe for adding a seventh punch-in effect
```

---

## 3. Design decisions that must not be undone casually

1. **`Source/Core` stays JUCE-free.** It is the only reason the engine can be tested
   without a toolchain. The devices adapt JUCE to it, never the reverse.
2. **UI/audio sharing uses atomics, not locks.** Pads write `std::atomic` flags,
   parameters are read through APVTS raw pointers, `FxSettings` is all atomics. Do
   not introduce a mutex on the audio thread.
3. **Two FX mixing styles exist on purpose.** `replace` (dry/wet crossfade in the base
   class) for tape stop/filter/stutter/bitcrush; `send` (FX overrides `process()`)
   for delay throw and reverb burst, because their tails must stay audible after
   release. See `docs/ADDING_AN_FX.md`.
4. **The host sequences; Seq Queen punches (decided 2026-10-29).** This used to be
   "keep the sequencer and the FX in one plugin" so punches could be sequenced on
   the same grid as the notes. The user preferred Live's own sequencer. Sequencing
   the punches still works: they are notes in a Live clip, on Live's grid. Costs,
   accepted knowingly: routing (MIDI To / MIDI From) instead of one device, VST3 for
   reliable MIDI routing in Live, and no "target one internal track" option (put Seq
   Queen FX on that track instead). The user has Live Suite but chose VST3 over Max
   for Live.
5. **NoteFx guarantees a note-off for every note-on it sends**, including when a
   trigger is released mid-note (the off follows the note's *output* pitch) and
   when the pending queue fills up (note-ons give way first). The random-abuse test
   `noteFxNeverLeavesANoteHangingUnderRandomAbuse` is the guard.
6. **Fills wait 2 ms before committing** so a real note landing just after the grid
   point (late host timing, humanised clips) wins instead of doubling.
7. **Trigger notes are swallowed, everything else passes.** CCs, pitch bend and
   aftertouch go through Seq Queen Notes untouched.
8. **Velocity is part of the punch** in the FX device: wet amount ×
   `1 - velamt × (1 - velocity)`. Pads count as full strength. Same rule the
   instrument used, so it is covered by the existing velocity tests too.

---

## 4. Verification in Ableton (not done yet)

Setup is in README "In Ableton". Then:

**Seq Queen FX**
1. Hold each pad over a drum loop: tape stop sags and recovers, delay throw rings
   after release, filter swoops, stutter repeats, reverb washes, bitcrush fizzes.
   **No clicks** at press or release.
2. MIDI To from another track reaches it: C1–F1 in a clip punch on time; a soft
   note punches less than a hard one.
3. Save the set, reopen: knob values restored.

**Seq Queen Notes**
1. MIDI From → the Notes track → *Seq Queen Notes* shows up as a choice, and the
   clip plays the Drum Rack through it unchanged when nothing is held.
2. Hats / kick+snare / fill / repeat land on Live's grid, including after looping
   and relocating.
3. Stop the transport mid-punch and mid-note: nothing hangs.
4. The trigger notes (C-2–B-2) never reach the Drum Rack.

---

## 5. Next tasks, in priority order

| # | Task | Notes |
|---|---|---|
| 1 | Try both devices in Live | section 4 |
| 2 | Tune the punches | FX defaults in `FxProcessor.cpp` (`kDefault*`); note punch voicings (kick/snare pattern, hat accents, echo level) in `NoteFx::handleTick/handleInput` |
| 3 | Melodic punches | the OP-XY's melodic octave is undocumented; candidates: arpeggiate held chord, fifth/octave stack, strum, reverse order |
| 4 | Tempo-synced FX macros | stutter slice / delay time in note values from the host BPM rather than free seconds |
| 5 | Pad presses into automation | when an FX pad is held, write `fx_*_on` via `setValueNotifyingHost` so Live can record the gesture |
| 6 | Presets | a few starting points per device |
| 7 | Old instrument code in the core | `Engine`, `Sequencer`, `SynthEngine`, `Pattern`, `PatternState`, their tests and the demo renderer are unused by the devices; ask the user before removing them |

---

## 6. Known rough edges (honest list)

* **Live routing is unverified.** MIDI To into a VST3 audio effect and MIDI From a
  VST3 instrument's MIDI output are both documented Live features, but neither has
  been tried with these devices. The AU versions build and validate, but expect
  VST3 to be the one that routes reliably.
* **Notes' generators need a running transport** (they follow the host's ppq).
  Mute, octave, shorten, random, skip and echo work while stopped.
* **Random draws from the last 8 distinct pitches played**, so it does nothing
  until at least two different notes have come through.
* **Shorten and Echo schedule their own note-offs**; if the same pitch is played
  again inside that window (a 32nd), the scheduled off cuts the new note short.
* **`StutterFx` declick** fades 4 ms at slice edges — cheap, but a stutter that lands
  exactly on a transient can still tick. Test with drums.
* **`DelayThrowFx` delay-time smoothing** uses a fixed coefficient (0.0008/sample),
  so the smoothing time changes with host sample rate. Fine at 44.1/48 k.
* **`Svf::setParams` is called per sample** in `FilterSweepFx` (that's how the glide
  works). Cheap, but the hottest line in the FX.
* **`render_demo.cpp` calls `barTime (13.5)`** but `barTime` takes an `int`, so the
  lead filter sweep in the demo ends at bar 13, not 13.5. Left as is so the
  committed WAV still matches.

---

## 7. Conventions

* C++17, 4-space indent, `opxy::` namespace for the core, `SeqQueen*` class names in
  the JUCE layer. Parameter IDs are stable forever (they are saved in Live sets):
  `fx_<fx>_<name>` and `trigger_note` in the FX device; `trigger_note`,
  `kick_note`, `snare_note`, `hat_note`, `repeat_rate` in the Notes device.
* Comments explain *why*, not *what*.
* Every engine change comes with a test in `tests/CoreTests.cpp`. The test file is the
  spec: read it to understand intended behaviour.
* Effects must always be click-free at engage/release; the `maxDelta < 0.30` assertion
  in `punchInEffectsDoNotClickWhenEngagedAndReleased` is the guard.
* Notes shown to the user use Live's octave numbering (middle C = C3, note 0 = C-2):
  `ui::liveNoteName()`.

---

## 8. If something looks wrong

Trust the tests first, then this list, then the code:

| Symptom | Look at |
|---|---|
| FX audible after release forever | `hasTail()` in that FX |
| Punch too weak from MIDI | `Velocity Amount` knob; `FxBus::applySettings` |
| Generated notes off the grid | `NoteFx::process` tick maths (32nd grid from ppq) |
| Generated note played twice | the `lastTick` guard in `NoteFx::process` |
| Fill doubles a real note | fill tolerance (2 ms) in `handleInput` / `handleTick` |
| Stuck note downstream | `mapped[][]` bookkeeping in `handleInput`; queue headroom in `schedule` |
