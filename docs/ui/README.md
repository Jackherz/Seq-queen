# UI — design spec and skin

The plugin's entire skin is drawn in `paint()` methods; there are no images, no
binary resources and no SVG. That means the design is searchable source, it
resizes cleanly, and you can change it without re-running anything.

This folder is the **spec** that the C++ is written against.

| File | What it is |
|---|---|
| `render_mockup.py` | The design spec, as code. Draws the 1180×810 editor at 2× and downsamples. |
| `mockup-main.png` / `@2x` | Track view: 8 tracks × 16 steps, p-locks, punch pads, mixer. |
| `mockup-main-lanes.png` / `@2x` | FX-lanes view: the same editor with the grid switched to sequenced punch FX. |

```bash
pip install pillow
python3 docs/ui/render_mockup.py              # tracks view
MODE=lanes python3 docs/ui/render_mockup.py   # lanes view
```

## Design language

Taken from the OP-XY / OP-1 HUD, technical-poster and Y2K-chrome reference
sheets: near-black void, charcoal panels, 1 px hairlines, bone/cream as the "on"
state instead of white, tiny wide-tracked uppercase labels, monospace values,
tick-ring dials, dot-matrix texture, corner ticks and a barcode serial strip.

Only four saturated accents, each meaning one thing:

| Token | Hex | Means |
|---|---|---|
| `amber` | `#F07A3C` | transport, playhead, primary/selected, ratchets |
| `cyan` | `#4CC9D8` | p-locks, hints, meta values |
| `green` | `#6BD08C` | Euclidean / brains being active |
| `violet` | `#9A8CE8` | delay throw (matches the delay FX) |
| `red` | `#E0503C` | destructive: mute, clear, bitcrush |

Neutrals: `void #0B0C0E`, `panel #14161A`, `panelRaised #1B1E23`,
`panelSunken #101216`, `hairline #2A2E35`, `hairlineLit #3A4049`,
`bone #EDE7D9`, `boneDim #969891`, `textDim #7C8288`, `textFaint #4A5058`.

## Layout

1180 × 810, 12 px margin, stacked bands:

```
header 54   wordmark · master/swing/tempo dials · focus + grid combos ·
            FX LANES / MIDI OUT / FX NOTES toggles · bar readout
grid   230  01 · PATTERN    (or 01 · PUNCH LANES in lanes mode)
punch  104  02 · PUNCH-IN FX + master-bus routing rail
edit   140  03 · STEP EDIT   five step rotaries + six p-lock slots
voices 210  04 · VOICES      8 mixer rows
footer       serial · build · sample rate · barcode
```

## Where it lives in C++

| Spec element | Code |
|---|---|
| Tokens, fonts, panel chrome, tick rings | `Source/Plugin/Theme.h` |
| Rotary/linear/combo/toggle styling | `Source/Plugin/SeqQueenLookAndFeel.*` |
| Bands and header/footer | `Source/Plugin/PluginEditor.cpp` |
| `01 · PATTERN` / `01 · PUNCH LANES` | `Source/Plugin/Components/StepGrid.cpp` |
| `02 · PUNCH-IN FX` | `Source/Plugin/Components/PunchPads.cpp` |
| `03 · STEP EDIT` | `Source/Plugin/Components/PLockPanel.cpp` |
| `04 · VOICES` | `Source/Plugin/Components/TrackMixer.cpp` |

If you change a number in `render_mockup.py`, change it in `PluginEditor.cpp`
too — the geometry constants at the top of that file mirror this file.

## Two rendering traps, already paid for

* PIL `Image.blend()` on an opaque `RGB` layer wipes the canvas to black. Glows
  and cables are composited with `ImageChops.add` on an `RGBA` layer — see
  `_additive()`.
* Long full-width patch cables across the editor read as scribbles. Routing is
  drawn as short drops into a labelled `master bus` rail instead.
