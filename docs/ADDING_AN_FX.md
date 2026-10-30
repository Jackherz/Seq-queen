# Adding a seventh punch-in FX

The pattern is the same every time: one new core class, one line in `FxChain`, one
row of defaults, one column in the FX device. Everything else (routing, ramping, tails, UI) is generic.

Worked example below: a **pitch-shift-ish "octave up" punch**, which is on the original
want-list.

## 1. Add the type

`Source/Core/Types.h`:

```cpp
enum class FxType : int
{
    TapeStop = 0,
    DelayThrow,
    FilterSweep,
    Stutter,
    ReverbBurst,
    Bitcrush,
    Octave,          // new
    Count
};
```

Then bump `kMaxFx` and extend the two switch statements (`fxTypeName`, `fxTypeId`).
`kMaxFx` is derived from `Count`, so the array sizes, the UI and the parameters all
follow automatically — but the *order* is ABI for existing sessions, so only ever
append.

## 2. Write the effect

`Source/Core/OctaveUpFx.h`

```cpp
#pragma once
#include "PunchInFx.h"

namespace opxy
{
class OctaveUpFx : public PunchInFx
{
public:
    const char* getName() const override { return "Octave"; }

    void prepare (double sr, int maxBlock) override;
    void reset() override;
    void setPunchParam (int index, float value) override;
    float getPunchParam (int index) const override;

protected:
    void processWet (float* left, float* right, int numSamples) override;
    // Replace-style: the base class handles the dry/wet crossfade, so no
    // process() override is needed. Use process() only if the tail must ring
    // out after release (see DelayThrowFx / ReverbBurstFx).
private:
    float transpose = 12.0f;   // semitones
};
}
```

`Source/Core/OctaveUpFx.cpp` implements `processWet()` writing the wet signal in place.
Whatever you write, keep these invariants:

* never allocate, never lock;
* output must be finite and bounded for any input (call `sanitise()`);
* at `mix → 0` the output must equal the input exactly (the base class does that for
  you if you use the default `process()`).

Simplest useful implementation: read the signal from a short delay line at 2× rate
(an octave up, with a bit of grit), or reuse `Svf`/`Oscillator` building blocks from the
core.

## 3. Register it

`Source/Core/FxChain.cpp`:

```cpp
#include "OctaveUpFx.h"
...
fx.push_back (std::make_unique<OctaveUpFx>());   // index must match FxType
```

## 4. Parameters

`Source/Devices/FxPlugin/FxProcessor.cpp` already loops over `kMaxFx`; add a value to
each default row:

```cpp
constexpr float kDefaultAmount[opxy::kMaxFx] = { 1.0f, 0.9f, 1.0f, 1.0f, 0.85f, 0.9f, 1.0f };
constexpr float kDefaultA[opxy::kMaxFx]      = { 0.25f, 0.3f, 0.75f, 0.2f, 0.6f, 0.7f, 0.5f };
constexpr float kDefaultB[opxy::kMaxFx]      = { 0.2f, 0.45f, 0.5f, 0.66f, 0.4f, 0.5f, 0.5f };
```

The IDs (`fx_octave_amount`, `fx_octave_a`, …) are generated from `fxTypeId()`, and
the trigger notes extend by one (C1–F#1), so no other parameter work is needed.

## 5. UI

`Source/Devices/FxPlugin/FxEditor.cpp` builds one column per `kMaxFx`, five per
row (`kPerRow`). Give the new
effect a one-line description in `describe()` and name its two knobs, with units, in
`macroFor()` — every control must say what it does. Give it a colour in
`ui::fxAccent()`, grow `kHeight` if it starts a new row, and check it with the
snapshot tool (`fx.png`).

The Max for Live device addresses effects by name: add the effect's display name
to `FX` in `Source/Max/seqqueen-link.js` and raise `kNumFxTriggers` in
`Source/Max/seqqueen_notes.cpp` (and the `route`/`live.remote~` count in
`tools/m4l/build_device.py`).

## 6. Tests

Add to `tests/CoreTests.cpp` (copy the shape of `bitcrushQuantisesAndRecovers`):

1. **Click-free**: add your FX to `punchInEffectsDoNotClickWhenEngagedAndReleased`.
   That test asserts `maxDelta < 0.30` across engage and release — it exists precisely
   to stop effects being added with audible steps.
2. **Behaviour**: prove the effect does its job (energy shifted, bandwidth changed,
   etc. — see `filterSweepAttenuatesWhenEngaged`).
3. **Recovery**: after release, `isActive()` must go false and the output must return
   to the dry signal within the tail window (see `stutterRepeatsAndStops`).

Run:

```bash
cmake --build --preset core && ctest --preset core
```

## 7. Demo

To hear it in the offline render, add a punch event in `tools/render_demo.cpp`:

```cpp
{ barTime (5) + kSecondsPerBeat, barTime (5) + 2.0 * kSecondsPerBeat,
  static_cast<int> (opxy::FxType::Octave), opxy::FxTarget::FocusTrack, "octave up" },
```

and re-render `demo/opxy_demo.wav`.

## Checklist

- [ ] `FxType` appended (never reordered)
- [ ] `fxTypeName` / `fxTypeId` updated
- [ ] class derives from `PunchInFx`, registered in `FxChain` in the right slot
- [ ] `prepare()` allocates, `processWet()` doesn't
- [ ] default macros added in `ParamIDs.cpp`
- [ ] click + behaviour + recovery tests pass
- [ ] audible in the demo render
