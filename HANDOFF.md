# Handoff — read this before touching anything

You are picking up **Seq Queen**, an OP-XY-inspired instrument plugin: an 8-track
p-lock step sequencer plus six momentary punch-in FX, written in C++17 + JUCE 8.

The architecture is in two halves:

* **`Source/Core/` — JUCE-free engine.** 28 unit tests, ~82k assertions, all
  green, and it renders the audio in `demo/opxy_demo.wav`.
* **`Source/Plugin/` — JUCE wrapper.** Builds and links on macOS (Apple clang 17,
  SDK 26.2, arm64) into VST3, AU and Standalone with zero warnings from the
  plugin sources.

### Status as of 2026-10-29 (first real macOS build)

* **Moved from JUCE 7.0.12 to JUCE 8.0.14.** 7.0.12 cannot build against the
  macOS 15+ SDK (`CGWindowListCreateImage` was removed; `juceaide` fails at
  configure time) and no JUCE 7 release fixes it. The only source changes needed
  were the `juce::Font` constructors in `Theme.h` (now `FontOptions`).
  `PositionInfo` still returns `juce::Optional` in JUCE 8, so the playhead code is
  unchanged.
* **Validated:** `auval -v aumu Sqxy Sqqn` passes; pluginval strictness 8 passes
  on the VST3 (editor open, state restore, parameter fuzzing, bus layouts).
* **UI checked by eye** via the new headless renderer (`seqqueen_snapshot`, below).
  Fixed on the way: `Â·` mojibake in three captions (UTF-8 literals passed as
  `const char*`), the mixer's column captions drawn across rows 4–5 instead of the
  header, track/engine labels overlapping in the grid, FX lane names truncated
  ("STUTTE"), and the footer wordmark overlapping the serial.
* **Not yet done:** a smoke test in Ableton (section 4). That is the next step.

Your job, in order:

1. **Smoke-test in Ableton** (section 4).
2. **Keep the tests green.**
3. **Work down the "Next tasks" list** at the end of this document.

---

## 1. Build

macOS (the target platform — this is going into Ableton on a Mac):

```bash
cmake --preset mac-release            # first run fetches JUCE 8.0.14
cmake --build --preset mac-release -j8
```

The plugin copies itself into `~/Library/Audio/Plug-Ins/` automatically
(`COPY_PLUGIN_AFTER_BUILD TRUE` in `CMakeLists.txt`).

Engine only — no JUCE needed, so this must always work, on any machine:

```bash
cmake --preset core
cmake --build --preset core
ctest --preset core                      # 28 tests
./build/core/opxy_demo demo/opxy_demo.wav
```

### Looking at the UI without a host

```bash
cmake --build --preset mac-release --target seqqueen_snapshot
./build/mac-release/seqqueen_snapshot_artefacts/Release/seqqueen_snapshot build/snap
```

Runs the starter groove for two seconds, then writes `main.png` and `lanes.png`
at 2x, plus the output peak and processing/draw cost. No window or screen
recording permission needed — use it after every UI change.

### Syntax check (no full build needed)

If you only want to know whether the plugin *sources* still make sense — fast,
and it works without freetype/X11/ALSA:

```bash
tools/check_plugin_syntax.sh /path/to/JUCE
```

It writes a throwaway `JuceHeader.h`, then `g++ -fsyntax-only`s every file in
`Source/Plugin/`. All eight pass as of this handoff. It is a first pass, not a
substitute for building.

Useful switches: `-DOPXY_JUCE_PATH=/path/to/JUCE` (skip the download),
`-DOPXY_BUILD_PLUGIN=OFF`, `-DOPXY_UNIVERSAL_BUILD=ON` (arm64 + x86_64).

### Build notes

* **Don't go back to JUCE 7** on a current Mac: see the status note above.
* **`-Wfloat-equal` in `ScalarUtils.h`** (`Ramp::isSmoothing`) is the one remaining
  warning. The exact compare is intentional: the ramp snaps exactly to its target
  and a core test relies on it.
* **`PLUGIN_CODE Sqxy` may already be taken** by another plugin on the machine. If AU
  validation complains or the two collide, change `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE`
  and bump the AU `PLUGIN_CODE`.
* **AU validation**: run `auval -v aumu Sqxy Sqqn` after building.

---

## 2. What exists, and where

```
Source/Core/                     ← JUCE-FREE. Never include JUCE here.
  Types.h                        constants, Enums (EngineType, FxType, PlockParam), StepEvent
  ScalarUtils.h                  clamp/lerp/pan/Ramp (linear ramp with exact snapping)
  Prng.h                         deterministic xorshift (tests + demo rely on it)
  Pattern.h/.cpp                 Step (atomics), Track (voice settings + brains), Pattern
  Sequencer.h/.cpp               the clock: host PPQ or free-run, swing, nudge, ratchet,
                                 pending-hit queue so hits never quantise to block edges
  SynthEngine.h/.cpp             24-voice pool, polyBLEP oscs, per-voice SVF, ADSR
  PunchInFx.h/.cpp               base class: momentary ramp, dry/wet capture, two mix styles
  TapeStopFx                     rate-slaved delay line, freeze + catch-up on release
  DelayThrowFx                   send-style feedback delay, tail rings after release
  FilterSweepFx                  resonant TPT low-pass, glided on engage/release
  StutterFx                      slice freeze + repeat with edge declick
  ReverbBurstFx                  4 comb + 2 allpass per channel, send-style, 1.2 s decay
  BitcrushFx                     bit-depth + sample-rate reduction, light drive
  FxChain.h/.cpp                 the six effects in one bus, processed in enum order
  Engine.h/.cpp                  sequencer → voices → per-track buffers → focus FX →
                                 sum → master FX → soft clip → output
  PatternState.h/.cpp            "opxy-steps v1" text serialisation of the step grid

Source/Plugin/                   ← JUCE layer
  ParamIDs.h/.cpp                every automatable parameter + the APVTS layout
  Theme.h                        colours, fonts, small styling helpers
  PluginProcessor.h/.cpp         engine ownership, param push, transport, MIDI in/out,
                                 punch holds, save/load
  PluginEditor.h/.cpp            layout + header controls
  Components/StepGrid.*          8 x N grid: toggle, drag-paint, right-click select
  Components/PunchPads.*         6 momentary pads (+ automation-driven glow)
  Components/PLockPanel.*        selected step: note/vel/gate/ratchet/nudge + 6 p-locks
  Components/TrackMixer.*        per-track engine, filter, brains, length, mute

tests/CoreTests.cpp              the safety net. Add to it with every engine change.
tools/render_demo.cpp            offline renderer for demo/opxy_demo.wav
docs/ARCHITECTURE.md             signal flow + threading model (read this one)
docs/PLOCKS.md                   user-facing semantics of p-locks and brains
docs/ADDING_AN_FX.md             recipe for adding a seventh punch-in effect
```

---

## 3. Design decisions that must not be undone casually

1. **`Source/Core` stays JUCE-free.** It is the only reason the engine can be tested
   without a toolchain. The plugin layer adapts JUCE to it, never the reverse.
2. **UI/audio sharing uses `std::atomic` fields on `Pattern`/`FxSettings`, not locks.**
   The message thread writes, the audio thread reads; a fumbled edit costs one block.
   Do not introduce a mutex on the audio thread.
3. **The sequencer schedules hits into a small fixed pending queue.** Ratchets, nudge
   and swing all produce hits that can land after the current block. Do not "simplify"
   this to fire-on-block-boundary; polyrhythms and ratchets break immediately.
4. **Two FX mixing styles exist on purpose.** `replace` (dry/wet crossfade in the base
   class) for tape stop/filter/stutter/bitcrush; `send` (FX overrides `process()`)
   for delay throw and reverb burst, because their tails must stay audible after
   release. See `docs/ADDING_AN_FX.md`.
5. **The engine soft-clips above 0.8** (`softClip()` in `Engine.cpp`) so eight loud
   tracks can't hard-clip. It is deliberately transparent below the knee.
6. **The instrument makes its own sound.** The original brief proposed an audio
   effect; the user chose "instrument with internal voices", so `IS_SYNTH TRUE` and
   there is no input bus.
7. **The sequencer and the punch FX stay in one plugin.** It was worth considering a
   split (sequencer plugin + FX plugin) — the answer is no, because the user's favourite
   OP-XY feature is *sequencing the punches*, and that needs both halves in one process:
   the punch lane resolves against the same 16th grid that fires the notes, and an FX
   must be able to target one internal track. Across two plugins that becomes MIDI
   routing into an effect, which AU in Live doesn't even support. The FX engine is
   sequencer-independent though (`FxChain` + `FxSettings`), so a standalone FX-only
   build can be added later as an extra target — see `docs/ARCHITECTURE.md`
   ("Splitting into two plugins"), budget a day.

### Punch triggering: velocity and lanes

* Pads and MIDI notes both punch; **MIDI velocity scales the wet amount** through the
  per-FX `Velocity Amount` parameter (default 0.7, 0 = ignore). Pads are full strength.
* Each FX also has a **sequenced punch lane** (`Pattern::fxLane`, 16 steps by default,
  up to 64). Lane value = wet amount for that 16th, 0 = off; consecutive non-zero steps
  hold the punch. It overrides the amount knob and ignores velocity.
* `Engine::isFxEngaged(fx)` = resolved state (lane OR pad OR MIDI OR automation).
  `FxSettings::engaged` is only the manual trigger input. **Always use the former for
  UI feedback** — using the latter is a bug the tests caught once already.

---

## 4. Verification checklist after building

```bash
ctest --preset core                       # must stay at 0 failures
./build/core/opxy_demo demo/opxy_demo.wav # should sound like the committed demo
```

In Ableton:

1. Drop it on a MIDI track → press play → you should hear a kick/snare-ish pattern
   out of the box (the constructor fills in a 4-track starter groove).
2. Hold each punch pad: tape stop sags and recovers, delay throw rings after release,
   filter swoops, stutter repeats, reverb washes, bitcrush fizzes. **No clicks** at
   press or release.
3. Change focus track to 4, set a pad's target to *Focus Track*, punch it: only the
   lead should be affected.
4. Save the Live set, reopen: grid + p-locks + all parameters restored.
5. Move the transport: the playhead in the grid follows bar 1 / bar 5 / loop points.
6. Enable MIDI OUT, route the track to another track: notes arrive on channels 1–8.

---

## 5. Next tasks, in priority order

| # | Task | Notes |
|---|---|---|
| 1 | Ableton smoke test | the build, auval and pluginval pass; nobody has played it in Live yet (section 4) |
| 2 | Real-instrument pass | audition every engine + FX, tune default levels and FX macros so it sounds good out of the box (`PluginProcessor` ctor + `defaultAmounts/ParamA/ParamB` in `ParamIDs.cpp`) |
| 3 | Punch-lane UI polish | the grid's `FX LANES` mode toggles 0/1 values only. Add: drag to set an amount (vertical drag), per-lane length, a lane clear/randomise button, and a "record pad presses into the lane" mode (arm, hold pads while the transport runs, write the gesture into the lane at the current step) |
| 4 | Per-step pitch editing UI | the p-lock panel edits the selected step; a roll view (drag note in a mini piano-roll per track) would make it much faster to use |
| 5 | Pattern chaining / song mode | add a `Pattern` bank (e.g. 8 patterns) + a chain list; keep `Engine` holding the *active* pattern and swap at bar boundaries |
| 6 | Copy/paste bars, clear track, randomise | pure UI + `Pattern` helpers; `euclideanMask()` and the brains give you the randomiser for free |
| 7 | Punch FX recording into automation | when a pad is held, write the `fx_*_on` parameter via `setValueNotifyingHost` so the gesture can be captured by the host |
| 8 | Tempo-synced FX macros | stutter slice / delay time in note values from the host BPM rather than free seconds |
| 9 | Sample playback per track | biggest remaining feature; add a `SampleEngine` next to `SynthEngine` behind the same `StepEvent` interface, load from a `FileChooser` in the mixer |
| 10 | MIDI learn for punch pads | map CC → `fx_*_on`, or notes → pads (already partly done in `handleIncomingMidi`) |
| 11 | Presets | `AudioProcessor::getNumPrograms` is 1; a small factory preset list of patterns would show the instrument off |

---

## 6. Known rough edges (honest list)

* **Mixer strips are tiny.** The seven per-track controls in the VOICES panel are
  40 px wide linear sliders and read as tick marks; they work, but a later UI pass
  should give them more room or turn them into the tick-ring dials.
* **`render_demo.cpp` calls `barTime (13.5)`** but `barTime` takes an `int`, so the
  lead filter sweep in the demo ends at bar 13, not 13.5. Left as is so the
  committed WAV still matches; change the signature to `double` if you re-render.
* **Editor is tall** (1180 x 810 default, 1020 x 740 minimum). Laptops in Live's
  lower zone may need the standalone window or a scrollable host.
* **No undo.** Step edits write straight into `Pattern` atomics. If you add undo,
  route edits through a `ValueTree`-backed model instead.
* **`StutterFx` declick** fades 4 ms at slice edges — cheap, but a stutter that lands
  exactly on a transient can still tick. Test with drums.
* **`DelayThrowFx` delay-time smoothing** uses a fixed coefficient (0.0008/sample),
  so the smoothing time changes with host sample rate. Fine at 44.1/48 k, slightly
  different at 88.2/96 k. If it matters, make the coefficient sample-rate aware.
* **`Sequencer` resync heuristic** treats a ppq jump > 0.25 steps as a relocate/loop.
  Hosts that report ppq with low resolution could false-trigger it. It was verified
  with synthetic transport only, not with real hosts.
* **`Svf::setParams` is called per sample** in `FilterSweepFx` (that's how the glide
  works). It is cheap (one `tan` per sample) but it is the hottest line in the FX.
  If profiles complain, recompute `setParams` every 8–16 samples.
* **`Pattern` is non-copyable** (atomics). Anything that wants to snapshot a pattern
  must go through `serialiseSteps()`; a proper snapshot type would be a nice refactor.
* **MIDI out note-offs** are queued with a sample offset and emitted at the start of
  the block in which they expire, so they can be up to one block late. Acceptable for
  driving other instruments; not sample-accurate.
* **`getFxNoteBase()`** returns the cached value updated during `processBlock`, so the
  editor shows it correctly only after the first block runs.
* **Punch-lane editing is binary in the UI** (a lane step is 0 or 1). The engine
  supports any amount 0..1 and the serialiser round-trips it; only the grid's drag
  gesture is missing. Same for per-lane length (`Pattern::fxLaneSteps` is global).
* **Lanes are not host-automatable.** They are pattern data (saved in the state blob),
  not APVTS parameters, so a host cannot automate a lane. If that turns out to matter,
  expose a single `fx_*_seq` boolean parameter per FX and keep the lane values in the
  pattern.

---

## 7. Conventions

* C++17, 4-space indent, `opxy::` namespace for the core, `SeqQueen*` class names in
  the JUCE layer, parameter IDs `t<track>_<name>` / `fx_<fx>_<name>` (stable forever —
  they are saved in sessions).
* Member naming in the core is plain (`length`, `cutoff`); the JUCE layer uses the same
  names for parameters. Comments explain *why*, not *what*.
* Every engine change comes with a test in `tests/CoreTests.cpp`. The test file is the
  spec: read it to understand intended behaviour.
* Effects must always be click-free at engage/release; the `maxDelta < 0.30` assertion
  in `punchInEffectsDoNotClickWhenEngagedAndReleased` is the guard.

---

## 8. If something in the core looks wrong

Trust the tests first, then this list, then the code:

| Symptom | Look at |
|---|---|
| Steps fire late / early | `Sequencer::process`, `samplesPerStep`, swing/nudge maths |
| Polyrhythm drifts | `((k % length) + length) % length` in `Sequencer::process` |
| Ratchet hits missing | `pending` queue capacity + `scheduleHit()` insertion sort |
| P-lock has no effect | `getPLock()` calls in `Sequencer::process` (voice params are resolved *at fire time*) |
| FX audible after release forever | `hasTail()` in that FX |
| Output quiet | voice `amp` in `SynthEngine::renderTrack`, track `level`, master gain, soft clip |
| Output clipping | soft clip is bounded, so check for double-applied track gain |
