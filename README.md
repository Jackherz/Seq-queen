# Seq Queen

OP-XY-style **punch-in effects** for Ableton, for not-quite-commercial,
definitely-personal use. Ableton does the sequencing; Seq Queen does the punches.
Two devices:

* **Seq Queen FX** (audio effect). Six momentary effects (tape stop, delay throw,
  filter, stutter, reverb, bitcrush) on any track or bus. Hold a pad, or play its
  note (C1–F1) from a MIDI clip; harder notes punch harder.
* **Seq Queen Notes** (MIDI device). Your clip plays through it on its way to your
  Drum Rack or synth. Hold one of twelve trigger notes (C-2–B-2) to mute, roll,
  fill, add kick/snare or hats, shift octaves, chop, randomise, thin out or echo
  what comes out, the way the OP-XY's punch-in keys do.

Built with C++17 and JUCE 8.

![status](https://img.shields.io/badge/core-40%20tests%20green-brightgreen)
![status](https://img.shields.io/badge/devices-auval%20%2B%20pluginval-brightgreen)

> **Read this first if you are the next agent:** [`HANDOFF.md`](HANDOFF.md).

---

## Build (macOS)

Requirements: macOS 10.13+, Xcode command line tools, CMake 3.22+.
Ninja is optional (`brew install ninja`, then add `-G Ninja` to the configure step)
and just makes the build faster.

```bash
cmake --preset mac-release        # fetches JUCE 8.0.14 on first run (~1 min)
cmake --build --preset mac-release -j8
```

`COPY_PLUGIN_AFTER_BUILD` is on, so the devices land in:

```
~/Library/Audio/Plug-Ins/VST3/Seq Queen FX.vst3        (and Components/…component)
~/Library/Audio/Plug-Ins/VST3/Seq Queen Notes.vst3
```

Already have a JUCE checkout? Skip the download:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPXY_JUCE_PATH=/path/to/JUCE
```

Build only the engine (no JUCE, works anywhere):

```bash
cmake --preset core && cmake --build --preset core
ctest --preset core          # or ./build/core/opxy_core_tests
```

See the UI without opening a DAW:

```bash
./build/mac-release/seqqueen_snapshot_artefacts/Release/seqqueen_snapshot build/snap
```

## In Ableton (Live 11/12)

Rescan plug-ins (Settings → Plug-Ins) after building. Use the **VST3** versions:
Live routes MIDI to and from VST3 plug-ins, which both devices rely on.

### Seq Queen FX

1. Drop **Seq Queen FX** (Audio Effects) on a track or bus, e.g. your drum bus.
2. Hold its pads while Live plays. That's it for live use.
3. To sequence punches: make a new MIDI track, set its **MIDI To** to the FX's
   track and then **Seq Queen FX** in the second chooser. Draw notes **C1–F1** in
   a clip on that track: C1 tape stop, C#1 delay, D1 filter, D#1 stutter,
   E1 reverb, F1 bitcrush. Note length = how long it punches, velocity = how hard.

### Seq Queen Notes

1. Put **Seq Queen Notes** (Instruments) on a MIDI track and put your clip there.
2. On the track with your Drum Rack or synth: **MIDI From** → the Notes track →
   **Seq Queen Notes**, and **Monitor** → **In**.
3. Hold the trigger notes in the same clip (or play them, or hold the pads):

| Note | Punch | | Note | Punch |
|---|---|---|---|---|
| C-2 | Mute | | F#-2 | Octave up |
| C#-2 | Repeat (1/8, 1/16 or 1/32) | | G-2 | Octave down |
| D-2 | Fill empty 16ths with the last hit | | G#-2 | Shorten |
| D#-2 | Fill empty 8ths | | A-2 | Random (swaps in recent notes) |
| E-2 | Add kick + snare | | A#-2 | Skip every other note |
| F-2 | Add 16th hats | | B-2 | Echo an 8th later |

The green pads (repeat, fills, added drums) follow Live's 16th grid, so they only
play while the transport runs. Kick, snare and hat notes default to the Drum Rack
pads C1, D1 and F#1 and can be changed in the device.

## Hear the effects

`demo/opxy_demo.wav` (34 s) punches every effect into a groove. It was rendered with
the engine of the original all-in-one Seq Queen instrument (since removed; its
sequencer and synth are still in `Source/Core`), so the FX are the same DSP the
FX device runs. Regenerate it with:

```bash
cmake --preset core && cmake --build --preset core
./build/core/opxy_demo demo/opxy_demo.wav
```

## Layout

```
Source/
  Core/            JUCE-free engine — the part with tests (start here)
    PunchInFx.*        base class: momentary ramp + two mixing styles
    TapeStopFx.*  DelayThrowFx.*  FilterSweepFx.*
    StutterFx.*   ReverbBurstFx.*  BitcrushFx.*
    FxChain.*          the six effects in one bus
    FxBus.*            the chain as a standalone insert       -> Seq Queen FX
    NoteFx.*           the twelve note punches                -> Seq Queen Notes
    Engine.* Sequencer.* SynthEngine.* Pattern.* PatternState.*
                       the old instrument's sequencer + synth (tests + demo only)
  Devices/
    FxPlugin/          Seq Queen FX processor + editor
    NotesPlugin/       Seq Queen Notes processor + editor
    Shared/            Theme, LookAndFeel, pads and labelled knobs
tests/CoreTests.cpp    40 tests, no JUCE required
tools/snapshot.cpp     renders both editors to PNG without a host
tools/render_demo.cpp  offline demo renderer
docs/                  architecture, how to add an FX
```

## Status / next steps

See [`HANDOFF.md`](HANDOFF.md) for the prioritised list. Short version: try both
devices in Live, then tune how each punch sounds and feels.

## Licence

Personal learning project. JUCE 8 is used under its AGPLv3 option, so treat the
combined work as AGPLv3-compatible; the DSP is written from scratch here (no code
copied from the GPL projects in the original brief — they are references only).
