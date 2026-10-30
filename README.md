# Seq Queen

OP-XY-style **punch-in effects** for Ableton, for not-quite-commercial,
definitely-personal use. Ableton does the sequencing; Seq Queen does the punches.
Two devices:

* **Seq Queen FX** (audio effect). Ten momentary effects: tape stop, delay throw,
  filter, stutter, reverb, bitcrush, reverse, gate/pump, high-pass sweep and pitch
  shift. Hold a pad, play its note, or let Seq Queen Notes punch it.
* **Seq Queen Notes** (MIDI device, as Max for Live or VST3). Your clip plays
  through it on its way to your Drum Rack or synth. Hold one of sixteen trigger
  notes to mute, roll, fill, add kick/snare or hats, shift octaves, chop,
  randomise, thin out, echo, arpeggiate, stack chords, strum or groove what comes
  out, the way the OP-XY's punch-in keys do.

In Live Suite they sit on **one track**: Seq Queen Notes in front of your
instrument, Seq Queen FX after it, one clip driving both.

Built with C++17 and JUCE 8.

![status](https://img.shields.io/badge/core-50%20tests%20green-brightgreen)
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

## In Ableton (Live 12 Suite): one track

The easy way. Everything lives on the track with your instrument, and one clip
drives it all:

```
[ Seq Queen Notes (Max for Live) ] → [ Drum Rack or synth ] → [ Seq Queen FX ]
```

1. Rescan plug-ins after building (Settings → Plug-Ins → Rescan).
2. From the Browser, drag **User Library → Seq Queen → Seq Queen Notes** onto a
   MIDI track, **in front of** (to the left of) your Drum Rack or synth.
3. Drag **Plug-Ins → Seq Queen FX** (VST3) onto the same track, **after** the
   instrument. The Notes device's bottom line should change to
   *"FX: linked to Seq Queen FX"*.
4. Put your clip on that track. Hold these notes in it (or click the pads):

| Notes | What they do |
|---|---|
| **C-2 – D#-1** | the 16 note punches (table below), on the Notes device's pads |
| **C0 – A0** | the 10 Seq Queen FX: tape stop, delay, filter, stutter, reverb, bitcrush, reverse, gate, high-pass, pitch. Velocity = depth |
| everything else | plays your instrument as normal |

| Note | Punch | | Note | Punch |
|---|---|---|---|---|
| C-2 | Mute | | A-2 | Random (swaps in recent notes) |
| C#-2 | Repeat (1/8, 1/16 or 1/32) | | A#-2 | Skip every other note |
| D-2 | Fill empty 16ths with the last hit | | B-2 | Echo an 8th later |
| D#-2 | Fill empty 8ths | | C-1 | Arp: held notes one at a time |
| E-2 | Add kick + snare | | C#-1 | Chord: add a fifth + octave |
| F-2 | Add 16th hats | | D-1 | Strum chords / flam single hits |
| F#-2 | Octave up | | D#-1 | Groove: swing, accents, loose timing |
| G-2 | Octave down | | | |

Pads light up while their punch is on, from a click or a note in the clip. The
**Pads** switch picks **Hold** (on while you hold the mouse down) or **Latch**
(click on, click off).

Repeat, the fills, kick+snare, hats and arp follow Live's 16th grid, so they only
play while Live is playing. Kick/snare/hat notes default to the Drum Rack pads C1,
D1 and F#1; the Notes device has settings for them, the repeat/arp rate and swing.

The Max for Live parts install with the build: the device into your User Library,
and its engine (`seqqueen.notes.mxo`) plus link script into
`~/Documents/Max 9/Packages/Seq Queen/`.

## In any DAW: two tracks, with routing

The VST3/AU devices work without Max for Live too, using MIDI routing:

* **Seq Queen Notes** (Instruments) on its own MIDI track with your clip; on the
  instrument's track set **MIDI From** → that track → *Seq Queen Notes* and
  **Monitor → In**. Trigger notes as above (C-2 – D#-1).
* **Seq Queen FX** (Audio Effects) on any track or bus; hold its pads, or from a
  MIDI track set **MIDI To** → that track → *Seq Queen FX* and play **C1 – A1**
  (its own trigger notes when used this way).

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
    FxChain.*          all ten effects in one bus
    ReverseFx.* GateFx.* HighPassFx.* PitchShiftFx.*
    FxBus.*            the chain as a standalone insert       -> Seq Queen FX
    NoteFx.*           the sixteen note punches               -> Seq Queen Notes
    Engine.* Sequencer.* SynthEngine.* Pattern.* PatternState.*
                       the old instrument's sequencer + synth (tests + demo only)
  Devices/
    FxPlugin/          Seq Queen FX processor + editor
    NotesPlugin/       Seq Queen Notes processor + editor
    Shared/            Theme, LookAndFeel, pads and labelled knobs
  Max/               seqqueen.notes external + seqqueen-link.js (Max for Live)
tests/CoreTests.cpp    50 tests, no JUCE required
tests/MaxExternalSmoke.cpp  loads the real .mxo with a stand-in for Max
tools/m4l/             builds and installs the Max for Live device
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
