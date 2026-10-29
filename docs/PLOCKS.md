# P-locks, brains and punches — how they actually behave

This is the behavioural spec. If the code and this document disagree, this document is
what the tests were written against.

## P-locks (parameter locks)

A p-lock is "on *this* step, use *this* value instead of the track's value".

* Six parameters can be locked per step: **pitch** (semitone offset), **cutoff**,
  **resonance**, **decay**, **velocity**, **pan**.
* A lock only exists while it is armed. `Step::getPLock(param, fallback)` returns the
  locked value if the bit is set, otherwise the track default.
* Locks are resolved when the step fires, and baked into the note that starts a voice.
  So a p-locked cutoff applies to that note's whole envelope — it is not a
  filter automation curve, it is a per-note setting. (Same as the OP-XY.)
* Pitch and velocity additionally stack with the randomisers: pitch lock is added to
  the step note, then the random range is applied, then the scale quantiser snaps it.
* The grid shows a small cyan dot on any step that carries at least one lock.

**Editor flow:** right-click a step to select it without toggling it, then arm a
`LOCK` button in the p-lock row and dial its value. Locked slots light up teal.

## Brains (generative modifiers, per track)

| Brain | Range | Behaviour |
|---|---|---|
| Probability | 0–1 | chance a step fires at all; evaluated once per step per pass |
| Euclidean | on/off, hits 0–64, offset 0–63 | **replaces** the hand-drawn pattern with a Euclidean rhythm: step *i* fires when `(i * hits) % length < hits` (rotated by offset) |
| Ratchet | 1–8 | each firing step is subdivided into that many evenly spaced re-triggers; sub-hits are shortened and marked non-primary |
| Random pitch | 0–24 semitones | uniform offset added per firing step, before scale quantising |
| Random velocity | 0–1 | uniform ±amount/2 velocity jitter |
| Scale | Off/Major/Minor/Dorian/Pentatonic/Chromatic + root | quantises the final note to the nearest scale degree |

Notes on interaction:

* The Euclid brain **replaces** the step toggles rather than OR-ing with them. That
  keeps a generated rhythm predictable while you tweak hits/offset.
* Track ratchet and per-step ratchet multiply, capped at 8 sub-hits.
* Probability is evaluated at fire time with a deterministic PRNG seeded per engine, so
  a given performance repeats until the plugin is recreated (useful for testing; not
  "true random" per bar).
* Micro-timing (`nudge`, ±0.5 step) shifts the step without touching the grid.

## Punch-in FX

Momentary by design: hold → effect ramps in; release → ramps out. Two behaviours:

* **Replace-style** (tape stop, filter, stutter, bitcrush) — the base class crossfades
  dry ↔ wet over the ramp time ("Amount" is the wet target).
* **Send-style** (delay throw, reverb burst) — the FX overrides `process()` and rides
  on top of the dry signal, with an internal release time so the tail decays naturally
  instead of being cut off.

| FX | Macro A | Macro B | Notes |
|---|---|---|---|
| Tape Stop | stop time 50 ms–1.5 s | start time 50 ms–1.5 s | freezes, then pitches back up while catching up (capped at 6×) |
| Delay Throw | delay time 30–900 ms | feedback 0–0.9 | input gated on release, tail runs out over ~2.5× delay time |
| Filter | sweep depth | resonance boost | resonant low-pass, 25 ms glide |
| Stutter | slice 20–500 ms | playback rate 0.5–2× | 4 ms declick at slice edges |
| Reverb | decay | damping | 4 combs + 2 allpasses per channel, 1.2 s release |
| Bitcrush | bits (16→3) | decimation (1→24×) | light drive, no dry removal |

**Routing**: each FX targets either the **master bus** or the **focus track**
(`focus_track` parameter). Two chains exist so you can, for example, stutter the lead
while the reverb hits the whole mix.

**Ways to trigger**:

* hold a pad (full strength);
* hold a MIDI note in the FX range (default C1–F1) — **the note's velocity scales the
  punch**, so a soft hit is a gentle punch and a hard one is full-on (per-FX
  `Velocity Amount`, default 70%, 0 = ignore velocity);
* automate the `fx_*_on` parameter;
* **sequence it** on the FX's punch lane (below).

## Sequenced punch FX (the punch lanes)

Each FX has a 64-step automation lane (`Pattern::fxLane`). Lane value = the punch amount
for that 16th, 0 = off. The lane follows the pattern clock, so punches stay locked to the
grid and survive tempo changes and loops.

* The lane **overrides** the amount knob for that step (a lane step of 0.4 is a
  half-strength punch); velocity scaling does not apply to lane-driven punches, because
  the lane already states the amount.
* Consecutive lane steps keep the FX held, so a two-step lane value is a two-step punch
  and long tails (delay, reverb) behave naturally.
* The lane is silent while the transport is stopped.
* `Engine::isFxEngaged(fx)` reports the *resolved* state (lane, pad, MIDI or automation),
  which is what the UI lights up. `FxSettings::engaged` is only the manual trigger input.
* Lanes are saved with the pattern (v2 blob, see `PatternState.h`).

## Tempo and grid

* Sequencer resolution is a 16th note; track lengths 1–64 give polyrhythms.
* Swing delays every odd 16th by `swing × 0.5` of a step (0 = straight, 1 = maximum).
* Tempo comes from the host. The `tempo` parameter is only used when no playhead is
  available (standalone / free-run).
