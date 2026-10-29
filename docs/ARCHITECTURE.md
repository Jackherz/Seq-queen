# Architecture

Two halves, one rule: **`Source/Core` never includes JUCE.**

```
Source/Plugin (JUCE)                    Source/Core (plain C++17)
┌───────────────────────┐              ┌──────────────────────────────────────┐
│ AudioProcessor        │  parameters  │ Engine                               │
│  • APVTS params       │ ───────────► │  Sequencer ──► StepEvent[]           │
│  • transport read     │  transport   │      │                               │
│  • MIDI in / out      │ ───────────► │      ▼ (sample-accurate offsets)     │
│  • save/load          │              │  SynthEngine (24 voices)             │
│  • punch holds        │              │      │ per-track stereo buffers       │
│ Editor + components   │ ◄─────────── │      ▼                               │
└───────────────────────┘  atomics     │  focus-track FX chain                │
                                       │      │ sum                           │
                                       │      ▼                               │
                                       │  master FX chain ──► soft clip ──►out│
                                       └──────────────────────────────────────┘
```

## Per-block signal flow (`Engine::processChunk`)

1. **Clear per-track buffers** (8 × stereo × `kMaxBlockSize`).
2. **Sequencer** produces `StepEvent`s with sample offsets inside the block. P-locks
   are resolved *here*, at fire time, so each note carries its own cutoff/decay/pan.
3. **FX routing** (after the clock, so the punch lanes resolve against this block's
   step): for each of the six FX, combine the trigger sources — pad/MIDI/automation
   hold **or** a sequenced punch lane — scale the wet amount by trigger velocity, then
   push amount / macros / ramp to whichever bus is armed (master or focus track).
4. **Voices are started at their exact offsets**, and the block is rendered in
   segments between events (`renderVoices(pos, count)`).
5. **Focus-track FX** run on that track's stereo buffer only.
6. **Sum** all eight tracks with smoothed level/mute ramps into the master bus.
7. **Master FX** chain.
8. **Master gain → soft clip (knee 0.8) → output channels**, with a decaying peak meter.

## Timing

* One 16th note = `60 / bpm / 4` seconds.
* With a host playhead, the clock is *derived from ppq* every block (no drift, and the
  plugin follows loops/relocates). A ppq jump larger than a quarter step triggers a
  resync: pending hits are dropped and the step counter realigns.
* Without a playhead (standalone, free-run) the clock accumulates block sizes.
* **Pending queue.** Swing, per-step nudge and ratchet sub-hits all produce hit times
  that can fall outside the current block. They go into a fixed-size pending queue
  (sorted by absolute sample time) and are emitted in whichever block they land in.
  This is why ratchets and polyrhythms stay rhythmically correct at any block size.
* Tracks are independent: `stepForTrack = globalStep % trackLength`, so track 3 at
  seven steps runs against track 1 at sixteen without any extra clock.

## Threading

| Data | Owner | Mechanism |
|---|---|---|
| Step grid, brains, track settings | UI + audio | `std::atomic` fields on `opxy::Pattern` |
| FX amount/macros/target/engaged | UI + audio | `std::atomic` fields on `opxy::FxSettings` |
| Step playhead for LEDs | audio → UI | `std::atomic<int>` per track in `Sequencer` |
| Peak meter | audio → UI | `std::atomic<float>` in `Engine` |
| Automation values | host → audio | `APVTS` raw parameter pointers, cached in the processor |
| Sequenced punch-FX lanes | UI + audio | `std::atomic` cells on `opxy::Pattern` |
| Whole-pattern save/load | message thread | text blob via `PatternState`, stored in the state `ValueTree` |

The message thread only ever writes single atomics, so an edit can never tear. There is
no lock on the audio thread anywhere in the core. Multi-field consistency (e.g. loading
a saved pattern) happens while the engine is not running.

## Memory

All buffers are allocated in `prepare()` and never resized on the audio thread:

| Buffer | Size |
|---|---|
| per-track stereo buffers | 8 × 2 × 2048 floats ≈ 128 KB |
| tape stop lines | 2 × 3 s × 48 k ≈ 1.1 MB per bus |
| delay throw lines | 2 × 2 s × 48 k ≈ 0.8 MB per bus |
| stutter history | 2 × 2 s × 48 k ≈ 0.8 MB per bus |
| reverb (4 combs + 2 allpass per channel) | small, a few tens of KB |

Two FX chains exist (master + focus), so roughly 6 MB of delay memory in total for
44.1/48 kHz. `Engine` accepts blocks of any size and chunks internally at
`kMaxBlockSize` (2048), so hosts with huge buffers are safe.

## Why an instrument, not an effect

The original brief was an audio effect. The user picked "instrument with internal
voices" so the plugin is self-contained: sequencer → synth → punch FX all inside one
plugin, no external sound source needed. It still emits MIDI (opt-in) so the sequencer
can drive other tracks if you want.

## Splitting into two plugins (not done, deliberately)

The FX engine (`PunchInFx` subclasses + `FxChain` + `FxSettings`) has **no dependency on
the sequencer** — `FxChain::prepare(sampleRate, maxBlockSize)` and `process(l, r, n)` is
the whole interface. So a separate "Seq Queen FX" audio-effect plugin, and/or a
sequencer-only build, could be added as an extra CMake target reusing `opxy_core`,
without touching the DSP.

It is not done because the killer feature — sequencing the punches — needs both halves
in one process: the lane has to resolve against the same 16th grid that triggers the
notes, and the FX must be able to target a single internal track. Splitting now would
mean routing MIDI from one plugin into another just to recreate a feature that is
trivial in-process (and fragile: AU effects don't accept MIDI in Live).

If you do want a standalone FX plugin later, add a second `juce_add_plugin` target whose
processor owns an `FxChain` + `FxSettings[6]` and a gain stage, and reuse
`Source/Plugin/Components/PunchPads.*` as its UI. Budget a day.

## Extending it

* New punch-in FX → `docs/ADDING_AN_FX.md`.
* New synth engine → add an `EngineType`, extend `Oscillator::process`, map it in
  `engineChoices()`.
* New plockable parameter → add to `PlockParam`, `Types.h` names, `PLockPanel`
  default value + range, and resolve it in `Sequencer::process`.
* Patterns / song mode → the natural seam is a `PatternBank` owning several
  `opxy::Pattern`s with the `Engine` pointing at the active one, swapped at bar starts.
