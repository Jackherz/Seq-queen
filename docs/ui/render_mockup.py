#!/usr/bin/env python3
"""
Seq Queen UI mockup renderer.

This is the design spec: it draws the plugin's 1180x810 editor at 2x and
downsamples, so the PNGs in this folder are exactly what the JUCE code in
Source/Plugin/ is meant to paint.

    pip install pillow
    python3 docs/ui/render_mockup.py

Outputs: docs/ui/mockup-main.png, docs/ui/mockup-lanes.png (+ @2x versions)

Design language, taken from the reference sheets:
  * near-black void with charcoal panels and 1px hairlines (technical poster)
  * bone/cream as the "on" state instead of white (poster, OP-1 screens)
  * tiny uppercase labels with wide letter-spacing, monospace for values
  * tick-ring dials, dot-matrix texture, corner ticks, barcode serial strip
  * four saturated "patch cable" accents: amber, cyan, green, violet
"""

import os
from PIL import Image, ImageDraw, ImageFont

MODE = os.environ.get("MODE", "tracks")   # "tracks" or "lanes"
S = 2                      # supersample factor
W, H = 1180, 810           # plugin editor size (must match PluginEditor.cpp)

# ----------------------------------------------------------------- palette ---
VOID        = (11, 12, 14)
PANEL       = (20, 22, 26)
PANEL_HI    = (27, 30, 35)
HAIR        = (42, 46, 53)
HAIR_HI     = (58, 64, 73)
BONE        = (237, 231, 217)
BONE_DIM    = (150, 152, 145)
DIM         = (124, 130, 136)
FAINT       = (74, 80, 88)

AMBER       = (240, 122, 60)
CYAN        = (76, 201, 216)
GREEN       = (107, 208, 140)
VIOLET      = (154, 140, 232)
RED         = (224, 80, 60)

FONT_DIR = "/usr/share/fonts/truetype/dejavu/"
def mono(size, bold=False):
    name = "DejaVuSansMono-Bold.ttf" if bold else "DejaVuSansMono.ttf"
    return ImageFont.truetype(FONT_DIR + name, int(size * S))

def sans(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(FONT_DIR + name, int(size * S))

img = Image.new("RGB", (W * S, H * S), VOID)
d = ImageDraw.Draw(img)

def _rebind(new_img):
    """Rebinding img invalidates Draw, so re-create it."""
    global img, d
    img = new_img
    d = ImageDraw.Draw(img)

def px(v):            # design px -> device px
    return int(round(v * S))

def rect(x, y, w, h, outline=None, fill=None, width=1, radius=0):
    box = [px(x), px(y), px(x + w - 1), px(y + h - 1)]
    if radius:
        d.rounded_rectangle(box, radius=px(radius), outline=outline, fill=fill,
                            width=px(width))
    else:
        d.rectangle(box, outline=outline, fill=fill, width=px(width))

def line(x1, y1, x2, y2, colour, width=1):
    d.line([px(x1), px(y1), px(x2), px(y2)], fill=colour, width=max(1, px(width)))

def text(x, y, string, font, colour, tracking=0.0, anchor="la"):
    """Draws text with optional letter-spacing (tracking in design px)."""
    if tracking <= 0:
        d.text((px(x), px(y)), string, font=font, fill=colour, anchor=anchor)
        return
    step = tracking * S
    widths = [d.textlength(ch, font=font) for ch in string]
    total = sum(widths) + step * (len(string) - 1)
    start = px(x)
    if anchor[0] == "m":
        start -= total / 2
    elif anchor[0] == "r":
        start -= total
    cursor = start
    for ch, wdt in zip(string, widths):
        d.text((cursor, px(y)), ch, font=font, fill=colour, anchor="l" + anchor[1])
        cursor += wdt + step

def label(x, y, string, colour=DIM, size=8.5, tracking=1.6):
    text(x, y, string.upper(), mono(size, bold=True), colour, tracking=tracking)

def corner_ticks(x, y, w, h, colour=HAIR, length=6):
    for cx, cy, dx, dy in ((x, y, 1, 1), (x + w, y, -1, 1),
                           (x, y + h, 1, -1), (x + w, y + h, -1, -1)):
        line(cx, cy, cx + dx * length, cy, colour)
        line(cx, cy, cx, cy + dy * length, colour)

def dot_grid(x, y, w, h, step=9, radius=1.0, colour=HAIR):
    yy = y
    while yy < y + h:
        xx = x
        while xx < x + w:
            d.ellipse([px(xx - radius), px(yy - radius),
                       px(xx + radius), px(yy + radius)], fill=colour)
            xx += step
        yy += step

def dial(cx, cy, r, value, colour=AMBER, ticks=26, start=-225, end=45,
         text_value=None, text_label=None):
    """Tick-ring rotary, like the poster's panel dials."""
    import math
    # tick ring
    for i in range(ticks + 1):
        a = math.radians(start + (end - start) * i / ticks)
        inner = r + 3.0
        outer = r + (6.0 if i % 4 == 0 else 4.5)
        line(cx + math.cos(a) * inner, cy + math.sin(a) * inner,
             cx + math.cos(a) * outer, cy + math.sin(a) * outer,
             HAIR_HI if i % 4 == 0 else HAIR, 1)
    # ring
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)],
              outline=HAIR_HI, width=px(1))
    # value arc
    a0, a1 = math.radians(start), math.radians(start + (end - start) * value)
    steps = max(2, int(abs(a1 - a0) * 24))
    pts = [(px(cx + math.cos(a0 + (a1 - a0) * t / steps) * (r - 1.5)),
            px(cy + math.sin(a0 + (a1 - a0) * t / steps) * (r - 1.5)))
           for t in range(steps + 1)]
    if len(pts) > 1:
        d.line(pts, fill=colour, width=px(2), joint="curve")
    # pointer
    line(cx, cy, cx + math.cos(a1) * (r - 4), cy + math.sin(a1) * (r - 4), BONE, 1.4)
    d.ellipse([px(cx - 1.6), px(cy - 1.6), px(cx + 1.6), px(cy + 1.6)], fill=BONE)

    if text_value:
        text(cx, cy + r + 13, text_value, mono(9), BONE, anchor="ma")
    if text_label:
        label(cx, cy + r + 24, text_label, FAINT, 7.5, tracking=1.2)

def section_header(x, y, w, number, title, right_text=None):
    label(x, y, f"{number} · {title}", BONE_DIM, 9, tracking=2.2)
    line(x, y + 13, x + w, y + 13, HAIR)
    if right_text:
        text(x + w, y, right_text, mono(8), FAINT, tracking=1.2, anchor="ra")

def _additive(layer, strength=1.0):
    """Adds `layer` onto the canvas instead of blending it.

    Crucially the layer is transparent (RGBA) so its untouched pixels add
    nothing; blending an opaque black layer is what nuked the earlier render.
    """
    from PIL import ImageChops
    base = img.convert("RGB")
    glow_rgb = layer.convert("RGBA")
    alpha = glow_rgb.split()[3]
    if strength != 1.0:
        alpha = Image.eval(alpha, lambda v: int(v * strength))
    coloured = Image.merge("RGBA", (*glow_rgb.convert("RGB").split(), alpha))
    white = Image.new("RGB", base.size, (0, 0, 0))
    added = ImageChops.add(base, Image.composite(coloured.convert("RGB"), white, alpha))
    _rebind(added)

def glow(x, y, w, h, colour, radius=0, spread=3, alpha=40):
    """Soft outer glow: concentric translucent rings, added to the canvas."""
    layer = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    for i in range(spread, 0, -1):
        a = int(alpha * (1.0 - i / (spread + 1)) ** 1.5)
        if a <= 0:
            continue
        ld.rounded_rectangle([px(x - i), px(y - i), px(x + w - 1 + i), px(y + h - 1 + i)],
                             radius=px(radius + i), outline=(*colour, a), width=px(2))
    _additive(layer)

def cable(x1, y1, x2, y2, colour, bend=0.4):
    """Quadratic patch cable: three additive passes (halo, body, core)."""
    pts = []
    steps = 48
    mx = x1 + (x2 - x1) * 0.5
    my = y1 + (y2 - y1) * 0.5 - abs(x2 - x1) * bend * 0.35
    for i in range(steps + 1):
        t = i / steps
        xx = (1 - t) ** 2 * x1 + 2 * (1 - t) * t * mx + t ** 2 * x2
        yy = (1 - t) ** 2 * y1 + 2 * (1 - t) * t * my + t ** 2 * y2
        pts.append((px(xx), px(yy)))

    for width, alpha in ((7, 22), (4, 44), (2, 255)):
        layer = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
        ImageDraw.Draw(layer).line(pts, fill=(*colour, alpha), width=px(width), joint="curve")
        _additive(layer)

# ---------------------------------------------------------------- chrome ----
def frame(x, y, w, h, fill=PANEL, ticks=True):
    rect(x, y, w, h, fill=fill)
    rect(x, y, w, h, outline=HAIR)
    if ticks:
        corner_ticks(x, y, w, h, HAIR_HI, 5)

# ---------------------------------------------------------------- header ----
M = 12
HEADER_H = 54
frame(M, M, W - 2 * M, HEADER_H)

# wordmark: chunky geometric mark + tracked title, per the logo sheet
rect(M + 12, M + 15, 13, 13, fill=AMBER)
rect(M + 18, M + 15, 7, 13, fill=VOID)
rect(M + 18, M + 20, 7, 3, fill=AMBER)
text(M + 32, M + 11, "SEQ QUEEN", sans(19, bold=True), BONE, tracking=2.6)
label(M + 33, M + 36, "8-track p-lock sequencer / punch-in fx", FAINT, 7.5, tracking=1.5)

# header dials
for i, (name, value, shown) in enumerate([("master", 0.62, "-3.0"),
                                          ("swing", 0.30, "0.30"),
                                          ("tempo", 0.40, "120")]):
    cx = M + 372 + i * 62
    label(cx - 16, M + 5, name, FAINT, 7, tracking=1.2)
    dial(cx, M + 27, 12, value, AMBER, ticks=18)
    text(cx, M + 41, shown, mono(8), BONE, tracking=0.6, anchor="ma")

def combo(x, y, w, value, arrow=True):
    rect(x, y, w, 20, outline=HAIR_HI, fill=PANEL_HI)
    text(x + 8, y + 6, value, mono(8.5), BONE, tracking=0.8)
    if arrow:
        line(x + w - 14, y + 8, x + w - 9, y + 13, FAINT, 1)
        line(x + w - 9, y + 13, x + w - 4, y + 8, FAINT, 1)

def toggle(x, y, w, value, colour=AMBER, on=False, h=20):
    rect(x, y, w, h, outline=(colour if on else HAIR_HI), fill=(colour if on else PANEL_HI))
    text(x + w / 2, y + 6, value, mono(8.5, bold=on),
         (VOID if on else BONE_DIM), tracking=1.0, anchor="ma")

label(M + 534, M + 6, "focus", FAINT, 7, tracking=1.4)
label(M + 638, M + 6, "grid", FAINT, 7, tracking=1.4)

combo(M + 534, M + 17, 96, "TRACK 4")
combo(M + 638, M + 17, 84, "16 STEPS")
toggle(M + 730, M + 17, 78, "FX LANES", GREEN, on=True)
toggle(M + 816, M + 17, 76, "MIDI OUT", AMBER, on=False)
toggle(M + 900, M + 17, 78, "FX NOTES", CYAN, on=True)

# transport / position readout, right aligned under the toggles
text(W - M - 14, M + 12, "BAR 01  ·  4/4  ·  120.00", mono(8), BONE_DIM,
     tracking=1.1, anchor="ra")
text(W - M - 14, M + 26, "HOLD PAD / MIDI C1-F1 / FX LANE", mono(7.2), FAINT,
     tracking=1.0, anchor="ra")
d.ellipse([px(W - M - 14 - 118), px(M + 38), px(W - M - 14 - 112), px(M + 44)], fill=AMBER)

# ------------------------------------------------------------------ grid ----
GRID_Y = 74
GRID_H = 230 if MODE == "tracks" else 224
frame(M, GRID_Y, W - 2 * M, GRID_H)

if MODE == "tracks":
    section_header(M + 14, GRID_Y + 9, W - 2 * M - 28, "01", "pattern",
                   "16 steps · 4/4 · playhead follows host")
else:
    section_header(M + 14, GRID_Y + 9, W - 2 * M - 28, "01", "punch lanes",
                   "sequenced punch-in fx · bar height = depth")

grid_x = M + 96
grid_w = W - 2 * M - 28 - 84
pad_gap = 2
pad_w = (grid_w - pad_gap * 15) / 16
pads_top = GRID_Y + 32
row_h = 22                      # 8 tracks x 22 + 32 header + 14 ruler = 222
lanes_h = 28                    # 6 lanes x 28  + 32 header + 14 ruler = 214
content_bottom = GRID_Y + GRID_H - 16

# beat separators
for beat in range(1, 4):
    bx = grid_x + beat * (pad_w + pad_gap) * 4 - pad_gap / 2
    line(bx, pads_top - 4, bx, content_bottom, HAIR, 1)

# faint dot-matrix texture behind the label gutter (reference sheets)
dot_grid(M + 14, pads_top - 2, 66, row_h * 8 - 6, step=6, radius=0.6,
         colour=(28, 31, 37))

MODE = os.environ.get("MODE", "tracks")

pattern = [
    # (label, engine tag, [step indices on], plock steps, ratchet steps)
    ("track 1", "sine", [0, 4, 8, 12], [], []),
    ("track 2", "saw", [2, 6, 10, 14], [10], []),
    ("track 3", "noise", [0, 2, 4, 6, 8, 10, 12, 14], [], [7, 15]),
    ("track 4", "square", [0, 2, 5, 8, 11, 13], [0, 2, 5, 8, 11], []),
    ("track 5", "noise", [4, 12], [], []),
    ("track 6", "tri", [0, 4, 8], [], []),
    ("track 7", "—", [], [], []),
    ("track 8", "—", [], [], []),
]
PLAYHEAD_STEP = 5

LANES = [
    ("tape stop", AMBER, "seq", [0, 0, 0, 0.9, 0, 0, 0, 0], []),
    ("delay throw", VIOLET, "seq", [0, 0.6, 0, 0, 0, 0, 0.6, 0], []),
    ("filter", CYAN, "seq", [0.8, 0.8, 0.7, 0.7, 0.6, 0.6, 0.5, 0.5], []),
    ("stutter", GREEN, "seq", [1, 0, 1, 0, 1, 0, 1, 0], []),
    ("reverb", BONE, "off", [0, 0, 0, 0, 0, 0, 0, 0], []),
    ("bitcrush", RED, "seq", [0, 0, 0.8, 0, 0, 0, 0.5, 0], []),
]

if MODE == "lanes":
    for row, (name, colour, state, values, _) in enumerate(LANES):
        y = pads_top + row * lanes_h
        if row % 2 == 0:
            rect(M + 1, y - 1, W - 2 * M - 2, lanes_h - 1, fill=(15, 17, 20))
        # hairline between lanes keeps the rows readable without a box per row
        line(grid_x, y + lanes_h - 2, grid_x + grid_w, y + lanes_h - 2, (20, 23, 27), 1)

        d.ellipse([px(M + 14), px(y + 8), px(M + 18), px(y + 12)], fill=colour)
        text(M + 24, y + 1, name.upper(), mono(8.5, bold=True),
             BONE_DIM if state == "seq" else FAINT, tracking=1.4)
        text(M + 24, y + 11, "SEQ" if state == "seq" else "OFF", mono(6.8),
             colour if state == "seq" else FAINT, tracking=1.0)

        for step in range(16):
            x = grid_x + step * (pad_w + pad_gap)
            value = values[step % len(values)]
            if value > 0:
                bar_h = (lanes_h - 6) * max(0.12, value)
                rect(x, y + lanes_h - 3 - bar_h, pad_w, bar_h, fill=colour, radius=1.5)
            else:
                rect(x, y + 3, pad_w, lanes_h - 6, outline=(18, 21, 25), radius=2)

        if state == "seq" and row == 2:
            pass
else:
  for row, (name, engine, on_steps, plocks, ratchets) in enumerate(pattern):
      y = pads_top + row * row_h
      if row % 2 == 0:
          rect(M + 1, y - 1, W - 2 * M - 2, row_h - 1, fill=(15, 17, 20))

      text(M + 14, y + 3, name.upper(), mono(8.5, bold=True),
           (BONE_DIM if on_steps else FAINT), tracking=1.4)
      text(M + 14, y + 13, engine.upper(), mono(7), FAINT, tracking=1.0)

      for step in range(16):
          x = grid_x + step * (pad_w + pad_gap)
          on = step in on_steps
          radius = 1.5
          if on:
              rect(x, y, pad_w, 18, fill=BONE, radius=radius)
              if step in plocks:
                  d.ellipse([px(x + pad_w - 6), px(y + 12), px(x + pad_w - 3), px(y + 15)], fill=CYAN)
          else:
              rect(x, y, pad_w, 18, outline=(HAIR_HI if step % 4 == 0 else HAIR), radius=radius)
          if step == PLAYHEAD_STEP:
              rect(x, y, pad_w, 18, outline=AMBER, width=1.4, radius=radius)
          if step in ratchets:
              for k in range(3):
                  line(x + 3 + k * 3.2, y + 15, x + 4.6 + k * 3.2, y + 15, AMBER, 1)

# playhead: a translucent column plus a centre line, glowing like a scope trace
ph_x = grid_x + PLAYHEAD_STEP * (pad_w + pad_gap) + pad_w / 2
col_x = grid_x + PLAYHEAD_STEP * (pad_w + pad_gap) - pad_gap / 2
const_rows = 8 if MODE == "tracks" else 6
const_row_h = row_h if MODE == "tracks" else lanes_h
rect(col_x, pads_top - 4, pad_w + pad_gap, const_row_h * const_rows + 2, fill=(24, 20, 16))
line(col_x, pads_top - 4, col_x, content_bottom, (58, 40, 26), 1)
line(ph_x, pads_top - 4, ph_x, content_bottom, AMBER, 1)
d.ellipse([px(ph_x - 2), px(pads_top - 8), px(ph_x + 2), px(pads_top - 4)], fill=AMBER)

# step ruler: every 4th step numbered, ticks on every other step
ruler_y = content_bottom + 4
line(grid_x, ruler_y, grid_x + grid_w, ruler_y, HAIR, 1)
for step in range(16):
    x = grid_x + step * (pad_w + pad_gap)
    if step % 4 == 0:
        text(x, ruler_y + 2, f"{step + 1:02d}", mono(7), FAINT, tracking=0.6)
        line(x, ruler_y, x, ruler_y + 3, HAIR_HI, 1)
    elif step % 2 == 0:
        line(x, ruler_y, x, ruler_y + 2, HAIR, 1)

# ------------------------------------------------------------ punch bar ----
PUNCH_Y = GRID_Y + GRID_H + 8
PUNCH_H = 104
frame(M, PUNCH_Y, W - 2 * M, PUNCH_H)
section_header(M + 14, PUNCH_Y + 9, W - 2 * M - 28, "02", "punch-in fx",
               "hold to engage · velocity → depth")

fx = [
    ("tape stop", AMBER, "c1 (36)", "mstr", "stop 0.35s", 0.0),
    ("delay throw", VIOLET, "c#1 (37)", "mstr", "1/8 fb 45%", 0.0),
    ("filter", CYAN, "d1 (38)", "trk 4", "sweep 75%", 0.85),
    ("stutter", GREEN, "d#1 (39)", "trk 4", "1/16 · 0.5x", 0.0),
    ("reverb", BONE, "e1 (40)", "mstr", "size 60%", 0.0),
    ("bitcrush", RED, "f1 (41)", "mstr", "8 bit", 0.0),
]

pad_gap_fx = 8
fx_w = (W - 2 * M - 28 - pad_gap_fx * 5) / 6
fx_y = PUNCH_Y + 30

for i, (name, colour, note, target, macro, level) in enumerate(fx):
    x = M + 14 + i * (fx_w + pad_gap_fx)
    engaged = level > 0
    rect(x, fx_y, fx_w, 46, fill=(PANEL_HI if not engaged else (34, 27, 20)),
         outline=(colour if engaged else HAIR_HI), width=(1.6 if engaged else 1))
    if engaged:
        glow(x, fx_y, fx_w, 46, colour, spread=4, alpha=90)
        rect(x, fx_y, fx_w, 46, outline=colour, width=1.6)
    # accent stripe on the top edge: the colour code for that FX
    rect(x + 1, fx_y + 1, fx_w - 2, 2, fill=colour if engaged else tuple(int(c * 0.5) for c in colour))

    text(x + 8, fx_y + 8, name.upper(), mono(8.6, bold=True),
         (BONE if engaged else BONE_DIM), tracking=1.4)
    text(x + 8, fx_y + 21, note.upper(), mono(7.0), FAINT, tracking=1.0)
    text(x + fx_w - 8, fx_y + 21, target.upper(), mono(7.0),
         (colour if engaged else FAINT), tracking=1.0, anchor="ra")
    text(x + 8, fx_y + 32, macro.upper(), mono(6.6), FAINT, tracking=0.8)

    # amount meter
    line(x + 8, fx_y + 40, x + fx_w - 8, fx_y + 40, HAIR, 1)
    if level > 0:
        line(x + 8, fx_y + 40, x + 8 + (fx_w - 16) * level, fx_y + 40, colour, 2)
        d.ellipse([px(x + fx_w - 9), px(fx_y + 6), px(x + fx_w - 5), px(fx_y + 10)], fill=colour)

# ----------------------------------------------------------------- plock ----
PLOCK_Y = PUNCH_Y + PUNCH_H + 8
PLOCK_H = 140
frame(M, PLOCK_Y, W - 2 * M, PLOCK_H)
section_header(M + 14, PLOCK_Y + 9, W - 2 * M - 28 - 236, "03", "step edit",
               None)
text(M + 140, PLOCK_Y + 9, "STEP 06 / TRACK 4   ·   P-LOCKS  2/6   ·   1 LOCKED FIELD",
     mono(7.6), FAINT, tracking=1.1)

toggle(W - M - 14 - 96, PLOCK_Y + 6, 96, "CLEAR STEP", RED, on=False, h=16)
toggle(W - M - 14 - 96 - 8 - 110, PLOCK_Y + 6, 110, "CLEAR P-LOCKS", RED, on=False, h=16)

step_dials = [("note", 0.34, "62"), ("velocity", 0.82, "0.82"), ("gate", 0.25, "1/4"),
              ("ratchet", 0.14, "1"), ("nudge", 0.58, "+0.02")]
for i, (name, value, shown) in enumerate(step_dials):
    cx = M + 40 + i * 62
    dial(cx, PLOCK_Y + 60, 17, value, BONE_DIM, ticks=16, text_value=shown)
    label(cx - 16, PLOCK_Y + 112, name, FAINT, 7.5, tracking=1.2)

line(M + 356, PLOCK_Y + 34, M + 356, PLOCK_Y + PLOCK_H - 12, HAIR)

locks = [("pitch", "+0", False), ("cutoff", "3.2k", True), ("res", "0.35", True),
         ("decay", "0.28", False), ("vel", "0.90", False), ("pan", "L12", False)]
lock_w = (W - 2 * M - 28 - 356 - 20) / 6
for i, (name, shown, locked) in enumerate(locks):
    x = M + 14 + 356 + i * lock_w
    rect(x + 4, PLOCK_Y + 34, lock_w - 10, 14,
         fill=(CYAN if locked else PANEL_HI),
         outline=(CYAN if locked else HAIR_HI))
    text(x + lock_w / 2 - 1, PLOCK_Y + 37, "lock", mono(6.6, bold=True),
         (VOID if locked else FAINT), tracking=1.2, anchor="ma")
    dial(x + lock_w / 2 - 1, PLOCK_Y + 76, 15, 0.45,
         (CYAN if locked else FAINT), ticks=14, text_value=shown)
    label(x + 2, PLOCK_Y + 104, name, (CYAN if locked else FAINT), 7.5, tracking=1.1)

# ----------------------------------------------------------------- mixer ----
MIX_Y = PLOCK_Y + PLOCK_H + 8
MIX_H = 210
frame(M, MIX_Y, W - 2 * M, MIX_H)
section_header(M + 14, MIX_Y + 9, W - 2 * M - 28, "04", "voices",
               "8 tracks · polyrhythm lengths · brains")

cols = [("engine", 84), ("lvl", 40), ("cut", 40), ("res", 40), ("dec", 40),
        ("prob", 40), ("hits", 40), ("rtch", 40), ("scale", 78), ("euc", 28),
        ("mute", 28)]
cx0 = M + 14 + 74
cx = cx0
for name, width in cols:
    label(cx, MIX_Y + 28, name, FAINT, 7, tracking=1.1)
    cx += width

voices = [
    ("track 1", "sine", "0.95", "1.2k", "0.10", "0.26", "1.00", "4", "1", "—", False, False),
    ("track 2", "saw", "0.50", "620", "0.45", "0.18", "1.00", "5", "1", "minor", True, False),
    ("track 3", "noise", "0.22", "11k", "0.05", "0.05", "0.62", "—", "3", "—", False, False),
    ("track 4", "square", "0.30", "2.4k", "0.20", "0.28", "1.00", "—", "2", "minor", False, False),
    ("track 5", "noise", "0.26", "3.2k", "0.10", "0.14", "0.80", "—", "1", "—", False, False),
    ("track 6", "tri", "0.16", "1.8k", "0.20", "1.20", "1.00", "3", "1", "—", True, False),
    ("track 7", "—", "0.00", "—", "—", "—", "—", "—", "—", "—", False, True),
    ("track 8", "—", "0.00", "—", "—", "—", "—", "—", "—", "—", False, True),
]

ry = MIX_Y + 42
for i, row in enumerate(voices):
    name, engine, lvl, cut, res, dec, prob, hits, rtch, scale, euc, mute = row
    if i % 2 == 0:
        rect(M + 1, ry - 1, W - 2 * M - 2, 20, fill=(15, 17, 20))
    text(M + 14, ry + 4, name.upper(), mono(8, bold=True),
         (BONE_DIM if lvl != "0.00" else FAINT), tracking=1.2)

    x = cx0
    rect(x, ry + 2, 74, 15, outline=HAIR, fill=PANEL_HI)
    text(x + 6, ry + 6, engine.upper(), mono(7.4), BONE_DIM, tracking=0.8)
    x += 84
    # level bar behind the value, so the row reads like a meter strip
    try:
        level_px = float(lvl) * 34
    except ValueError:
        level_px = 0
    if level_px > 0:
        line(x, ry + 16, x + level_px, ry + 16, AMBER, 1.2)
    for value in (lvl, cut, res, dec, prob, hits, rtch):
        text(x, ry + 5, value.upper(), mono(7.6), BONE if value != "—" else FAINT, tracking=0.6)
        x += 40
    rect(x, ry + 2, 68, 15, outline=HAIR, fill=PANEL_HI)
    text(x + 6, ry + 6, scale.upper(), mono(7.2), BONE_DIM, tracking=0.8)
    x += 78
    rect(x + 4, ry + 2, 16, 15, outline=(GREEN if euc else HAIR),
         fill=(GREEN if euc else None))
    text(x + 12, ry + 6, "e", mono(7, bold=True), (VOID if euc else FAINT), anchor="ma")
    x += 28
    rect(x + 4, ry + 2, 16, 15, outline=(RED if mute else HAIR),
         fill=(RED if mute else None))
    text(x + 12, ry + 6, "m", mono(7, bold=True), (VOID if mute else FAINT), anchor="ma")
    ry += 20

# ------------------------------------------------------- routing stubs ------
# Each FX chip gets a short stub into a shared bus rail at the bottom of the
# section: the poster's patch-cable idea, kept contained so it informs rather
# than scribbles over the controls. Height of the stub encodes nothing; the
# colour and endpoint do.
RAIL_Y = PUNCH_Y + PUNCH_H - 12
rail = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
ImageDraw.Draw(rail).line([px(M + 14), px(RAIL_Y), px(W - M - 14), px(RAIL_Y)],
                          fill=(*HAIR_HI, 190), width=px(1))
_additive(rail)

label(M + 14, RAIL_Y - 9, "master bus", FAINT, 6.6, tracking=1.2)
text(W - M - 14, RAIL_Y - 9, "FOCUS TRACK", mono(6.6), FAINT, tracking=1.2, anchor="ra")

# Routing: every pad has a hairline drop into the shared bus rail, and an
# engaged pad brightens its drop and gets a junction dot. Kept vertical so the
# section still reads as a panel, not as patch leads across the controls.
for i, (name, colour, note, target, macro, level) in enumerate(fx):
    x = M + 14 + i * (fx_w + pad_gap_fx) + fx_w / 2
    y0 = fx_y + 46
    line(x, y0, x, RAIL_Y, HAIR_HI if level > 0 else (26, 29, 34), 1)
    if level > 0:
        cable(x, y0, x, RAIL_Y, colour, bend=0.0)
        d.ellipse([px(x - 2), px(RAIL_Y - 2), px(x + 2), px(RAIL_Y + 2)], fill=colour)
    else:
        d.ellipse([px(x - 1), px(RAIL_Y - 1), px(x + 1), px(RAIL_Y + 1)], fill=HAIR_HI)

# ----------------------------------------------------------------- footer ---
FOOT_Y = MIX_Y + MIX_H + 4
text(M + 4, FOOT_Y + 2, "SEQ QUEEN", mono(7, bold=True), FAINT, tracking=2.0)
text(M + 96, FOOT_Y + 2, "SQQN-0001 / BUILD 0.2.0 / 48.0 kHz / 256 smp", mono(7), FAINT, tracking=1.0)

# barcode
bx = W - M - 120
for k, width in enumerate([1, 2, 1, 3, 1, 1, 2, 4, 1, 1, 3, 1, 2, 1, 1, 2, 3, 1, 1, 2]):
    line(bx, FOOT_Y - 1, bx, FOOT_Y + 8, FAINT if k % 3 else BONE_DIM, width)
    bx += width + 1.4

# ------------------------------------------------------------ downsample ----
suffix = "" if MODE == "tracks" else "-lanes"
out = img.resize((W, H), Image.LANCZOS)
out.save(f"docs/ui/mockup-main{suffix}.png")
img.resize((W * 2, H * 2), Image.LANCZOS).save(f"docs/ui/mockup-main{suffix}@2x.png")
print(f"wrote docs/ui/mockup-main{suffix}.png (+@2x)")
