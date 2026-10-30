#!/usr/bin/env python3
"""Builds "Seq Queen Notes.amxd", the one-track Max for Live MIDI effect.

    python3 tools/m4l/build_device.py OUT.amxd

The device goes in front of an instrument on a MIDI track:

    [midiin] -> [seqqueen.notes] -> [route punch active] -> [midiout]
                                          |         |
             10 x [live.remote~] <- punch |         | active -> pad lights
                     ^ ids from [js seqqueen-link.js] (finds Seq Queen FX)

An .amxd is a 32-byte header ("ampf", device type, "meta", "ptch" + length)
followed by the patcher as JSON, the same layout as Live's own
"Max MIDI Effect.amxd" template. The external (seqqueen.notes.mxo) and the
link script are installed into a Max package by tools/m4l/install.sh.
"""

import json
import struct
import sys

# Must match kEngineVersion in Source/Max/seqqueen_notes.cpp.
ENGINE_VERSION = 4

# ------------------------------------------------------------------ layout --
PAD_W, PAD_H, GAP = 62, 24, 4
ROW_Y = (24, 64)
PADS_X = 8
SETTINGS_X = PADS_X + 8 * (PAD_W + GAP) + 10
DEVICE_W = SETTINGS_X + 170

RED = [0.88, 0.31, 0.24, 1.0]
GREEN = [0.42, 0.82, 0.55, 1.0]
CYAN = [0.30, 0.79, 0.85, 1.0]
VIOLET = [0.60, 0.55, 0.91, 1.0]
AMBER = [0.94, 0.48, 0.24, 1.0]
PAD_OFF = [0.17, 0.17, 0.21, 1.0]
PAD_TEXT = [0.86, 0.86, 0.86, 1.0]
PAD_TEXT_ON = [0.06, 0.06, 0.06, 1.0]

# The sixteen punches, in NoteFxType order (trigger note = index, from C-2).
PADS = [
    ("Mute", RED), ("Repeat", GREEN), ("Fill", GREEN), ("Half Fill", GREEN),
    ("Kick+Snr", GREEN), ("Hats", GREEN), ("Oct Up", CYAN), ("Oct Down", CYAN),
    ("Shorten", VIOLET), ("Random", CYAN), ("Skip", VIOLET), ("Echo", VIOLET),
    ("Arp", AMBER), ("Chord", AMBER), ("Strum", AMBER), ("Groove", AMBER),
]
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def live_note_name(note):
    return NOTE_NAMES[note % 12] + str(note // 12 - 2)


# --------------------------------------------------------------- patcher --
boxes, lines = [], []
_next_id = [0]


def box(maxclass, rect, presentation_rect=None, **attrs):
    _next_id[0] += 1
    obj_id = "obj-%d" % _next_id[0]
    b = {"id": obj_id, "maxclass": maxclass, "patching_rect": rect}
    if presentation_rect is not None:
        b["presentation"] = 1
        b["presentation_rect"] = presentation_rect
    b.update(attrs)
    boxes.append({"box": b})
    return obj_id


def newobj(text, rect, inlets, outlets, outlettype=None):
    return box("newobj", rect, text=text, numinlets=inlets, numoutlets=outlets,
               outlettype=outlettype if outlettype is not None else [""] * outlets,
               fontname="Arial Bold", fontsize=10.0)


def comment(text, rect, presentation_rect=None, size=9.0, colour=None):
    attrs = dict(text=text, numinlets=1, numoutlets=0, fontsize=size, fontname="Ableton Sans Medium")
    if colour is not None:
        attrs["textcolor"] = colour
    return box("comment", rect, presentation_rect, **attrs)


def connect(source, outlet, destination, inlet):
    lines.append({"patchline": {"source": [source, outlet], "destination": [destination, inlet]}})


def parameter(longname, shortname, kind, initial, **extra):
    values = {"parameter_longname": longname, "parameter_shortname": shortname,
              "parameter_type": kind, "parameter_initial_enable": 1, "parameter_initial": [initial]}
    values.update(extra)
    return {"parameter_enable": 1, "varname": longname,
            "saved_attribute_attributes": {"valueof": values}}


# Core chain
midiin = newobj("midiin", [20, 20, 45, 20], 1, 1, ["int"])
engine = newobj("seqqueen.notes", [20, 60, 95, 20], 1, 1)
router = newobj("route punch active sync engine", [20, 100, 180, 20], 2, 5)
midiout = newobj("midiout", [20, 300, 50, 20], 1, 0)
connect(midiin, 0, engine, 0)
connect(engine, 0, router, 0)
connect(router, 4, midiout, 0)        # everything that isn't punch/active/sync/engine: MIDI bytes

# Live's transport, from two sources. Every engine tick sends "sync"; both
# [plugsync~] (play state 0, tempo 5, position in beats 6) and [transport]
# (resolution 3, tempo 4, state 6, raw ticks 7) answer straight back in, and the
# engine uses whichever is actually moving (plugsync~ alone never moved in Live).
sync_bang = newobj("t b b", [240, 140, 40, 20], 1, 2, ["bang", "bang"])
plugsync = newobj("plugsync~", [240, 170, 120, 20], 1, 9)
transport = newobj("transport", [420, 170, 120, 20], 2, 9)
connect(router, 2, sync_bang, 0)
connect(sync_bang, 1, plugsync, 0)
connect(sync_bang, 0, transport, 0)
for source, outlet, message, x in ((plugsync, 0, "playing", 240), (plugsync, 5, "tempo", 320),
                                   (plugsync, 6, "beats", 400), (transport, 3, "tres", 480),
                                   (transport, 4, "ttempo", 560), (transport, 6, "tstate", 640),
                                   (transport, 7, "tticks", 720)):
    forward = newobj("prepend " + message, [x, 200, 75, 20], 2, 1)
    connect(source, outlet, forward, 0)
    connect(forward, 0, engine, 0)

# Seq Queen FX link: punch <fx> <strength> -> live.remote~ per effect.
# live.thisdevice is a plain object (maxclass newobj), not a UI box; written as a
# UI box it silently failed to load, so the link script never started.
this_device = newobj("live.thisdevice", [400, 20, 85, 20], 1, 3, ["bang", "int", "int"])
link = newobj("js seqqueen-link.js", [400, 60, 120, 20], 1, 11)
connect(this_device, 0, link, 0)
punch_route = newobj("route 0 1 2 3 4 5 6 7 8 9", [140, 140, 160, 20], 2, 11)
connect(router, 0, punch_route, 0)
for f in range(10):
    remote = newobj("live.remote~ @normalized 1", [140 + f * 30, 260 + (f % 2) * 24, 140, 20], 2, 0)
    connect(punch_route, f, remote, 0)
    connect(link, f, remote, 1)

# Pads. A textbutton in button mode bangs its left outlet on press and again on
# release, and sends its text out the middle outlet on press only. The engine gets
# "padpress i" (middle) and "padbang i" (left) and works out press vs release
# itself, whichever order they arrive in, then decides momentary vs latched.
# ([mousestate] was tried first: at the release bang it could still read the
# button as down, so pads never let go.) The engine's "active i 0|1" recolours
# the pad: lit in its group colour while that punch is on from any source.
COLOUR_KEYS = ("bgcolor", "bgoncolor", "textcolor", "textoncolor", "bordercolor", "borderoncolor")


def fixed_colours():
    """Mark colours as our own, so the Live theme doesn't replace them."""
    return {key: {"expression": ""} for key in COLOUR_KEYS}


def rgba(colour):
    return " ".join("%.3f" % c for c in colour)


active_route = newobj("route " + " ".join(str(i) for i in range(16)), [700, 100, 300, 20], 2, 17)
connect(router, 1, active_route, 0)
for i, (name, colour) in enumerate(PADS):
    row, col = divmod(i, 8)
    x = PADS_X + col * (PAD_W + GAP)
    y = ROW_Y[row]
    px, py = 700 + col * 90, 160 + row * 200
    pad = box("textbutton", [px, py, 60, 20], [x, y, PAD_W, PAD_H],
              text=name, texton=name, mode=0, fontsize=9.5, fontname="Ableton Sans Bold",
              numinlets=1, numoutlets=3, outlettype=["", "", "int"], rounded=3.0,
              bgcolor=PAD_OFF, bgoncolor=colour, textcolor=PAD_TEXT, textoncolor=PAD_TEXT_ON,
              bordercolor=colour, borderoncolor=colour,
              saved_attribute_attributes=fixed_colours())
    comment(live_note_name(i), [px, py + 160, 40, 18],
            [x, y + PAD_H + 1, PAD_W, 14], size=8.0, colour=[0.55, 0.57, 0.6, 1.0])
    pressed = newobj("t b", [px + 40, py + 26, 30, 20], 1, 1, ["bang"])
    press_msg = box("message", [px + 40, py + 50, 80, 20], text="padpress %d" % i,
                    numinlets=2, numoutlets=1, outlettype=[""])
    bang_msg = box("message", [px, py + 74, 80, 20], text="padbang %d" % i,
                   numinlets=2, numoutlets=1, outlettype=[""])
    connect(pad, 1, pressed, 0)          # middle outlet: text, on press only
    connect(pressed, 0, press_msg, 0)
    connect(pad, 0, bang_msg, 0)         # left outlet: bang on press and release
    connect(press_msg, 0, engine, 0)
    connect(bang_msg, 0, engine, 0)

    which = newobj("sel 0 1", [px, py - 60, 50, 20], 1, 3)
    lit = box("message", [px + 40, py - 36, 200, 20], text="bgcolor %s, textcolor %s" % (rgba(colour), rgba(PAD_TEXT_ON)),
              numinlets=2, numoutlets=1, outlettype=[""])
    unlit = box("message", [px, py - 36, 200, 20], text="bgcolor %s, textcolor %s" % (rgba(PAD_OFF), rgba(PAD_TEXT)),
                numinlets=2, numoutlets=1, outlettype=[""])
    connect(active_route, i, which, 0)
    connect(which, 0, unlit, 0)
    connect(which, 1, lit, 0)
    connect(unlit, 0, pad, 0)
    connect(lit, 0, pad, 0)

# Settings (saved with the Live set)
settings_y = 24
for label, longname, short, message, initial in (
        ("Kick", "Kick Note", "Kick", "kick", 36),
        ("Snare", "Snare Note", "Snare", "snare", 38),
        ("Hat", "Hat Note", "Hat", "hat", 42)):
    comment(label, [1100, settings_y + 300, 40, 18], [SETTINGS_X, settings_y, 40, 16])
    number = box("live.numbox", [1150, settings_y + 300, 44, 15], [SETTINGS_X + 40, settings_y, 44, 15],
                 numinlets=1, numoutlets=2, outlettype=["", "float"],
                 **parameter(longname, short, 1, initial, parameter_mmax=127.0, parameter_unitstyle=8))
    send = newobj("prepend " + message, [1200, settings_y + 300, 80, 20], 2, 1)
    connect(number, 0, send, 0)
    connect(send, 0, engine, 0)
    connect(this_device, 0, number, 0)    # resend the saved value on load
    settings_y += 20

comment("Rate", [1100, 400, 40, 18], [SETTINGS_X, settings_y, 40, 16])
rate = box("live.menu", [1150, 400, 50, 15], [SETTINGS_X + 40, settings_y, 50, 15],
           numinlets=1, numoutlets=3, outlettype=["", "", "float"],
           **parameter("Repeat Rate", "Rate", 2, 1, parameter_enum=["1/8", "1/16", "1/32"]))
rate_send = newobj("prepend rate", [1200, 400, 80, 20], 2, 1)
connect(rate, 0, rate_send, 0)
connect(rate_send, 0, engine, 0)
connect(this_device, 0, rate, 0)
settings_y += 20

comment("Pads", [1100, 430, 40, 18], [SETTINGS_X, settings_y, 40, 16])
latch = box("live.text", [1150, 430, 50, 15], [SETTINGS_X + 40, settings_y, 50, 15],
            text="Hold", texton="Latch", mode=1, numinlets=1, numoutlets=2, outlettype=["", ""],
            **parameter("Pad Mode", "Pads", 2, 0, parameter_enum=["Hold", "Latch"], parameter_mmax=1.0))
latch_send = newobj("prepend latch", [1200, 430, 80, 20], 2, 1)
connect(latch, 0, latch_send, 0)
connect(latch_send, 0, engine, 0)
connect(this_device, 0, latch, 0)

swing = box("live.dial", [1300, 300, 44, 48], [SETTINGS_X + 104, 22, 50, 48],
            numinlets=1, numoutlets=2, outlettype=["", "float"],
            **parameter("Groove Swing", "Swing", 0, 33.0, parameter_mmax=60.0, parameter_unitstyle=5))
swing_scale = newobj("/ 100.", [1300, 360, 50, 20], 2, 1, ["float"])
swing_send = newobj("prepend swing", [1300, 390, 80, 20], 2, 1)
connect(swing, 0, swing_scale, 0)
connect(swing_scale, 0, swing_send, 0)
connect(swing_send, 0, engine, 0)
connect(this_device, 0, swing, 0)

# Words
comment("SEQ QUEEN NOTES", [400, 400, 120, 18], [PADS_X, 4, 140, 18], size=11.0, colour=AMBER)
hint = comment("Click a pad, or hold C-2 to D#-1 in your clip. C0-A0 punch Seq Queen FX. Green pads need Live playing.",
               [400, 430, 400, 18], [PADS_X, 104, 8 * (PAD_W + GAP) + 160, 16], size=8.5)

# Engine check. Max keeps the first copy of an external it loads until Live
# quits, so after an update Live can be running the old engine with the new
# patch (it happened: the grid punches silently never worked). On load: start a
# 1 s timer, ask the engine "hello"; the right "engine N" reply stops the timer,
# anything else (no reply from an old engine, or a wrong number) shows a warning.
check = newobj("t b b", [400, 500, 40, 20], 1, 2, ["bang", "bang"])
timeout = newobj("delay 1000", [460, 530, 70, 20], 2, 1, ["bang"])
hello = box("message", [400, 530, 50, 20], text="hello", numinlets=2, numoutlets=1, outlettype=[""])
version_ok = newobj("sel %d" % ENGINE_VERSION, [400, 560, 50, 20], 2, 2, ["bang", ""])
stop = box("message", [400, 590, 40, 20], text="stop", numinlets=2, numoutlets=1, outlettype=[""])
warn = box("message", [520, 590, 420, 20],
           text="set RESTART LIVE: an older Seq Queen engine is still loaded (quit Live and reopen it)",
           numinlets=2, numoutlets=1, outlettype=[""])
connect(this_device, 0, check, 0)
connect(check, 1, timeout, 0)          # right fires first: timer running before the reply
connect(check, 0, hello, 0)
connect(hello, 0, engine, 0)
connect(router, 3, version_ok, 0)
connect(version_ok, 0, stop, 0)
connect(stop, 0, timeout, 0)
connect(version_ok, 1, warn, 0)        # a reply, but the wrong version
connect(timeout, 0, warn, 0)           # no reply at all
connect(warn, 0, hint, 0)
status = comment("FX: looking for Seq Queen FX...", [400, 460, 400, 18],
                 [PADS_X, 120, 8 * (PAD_W + GAP) + 160, 16], size=8.5, colour=[0.55, 0.57, 0.6, 1.0])
connect(link, 10, status, 0)

# Every key Live's own "Max MIDI Effect.amxd" template has. Max assumes some of
# them exist: without project.searchpath it crashes while loading the device
# (it took Live down with it the first time). Keep this complete.
patcher = {
    "patcher": {
        "fileversion": 1,
        "appversion": {"major": 9, "minor": 1, "revision": 5, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": [40.0, 80.0, 1500.0, 700.0],
        "openrect": [0.0, 0.0, float(DEVICE_W), 169.0],
        "bglocked": 0,
        "openinpresentation": 1,
        "default_fontsize": 10.0,
        "default_fontface": 0,
        "default_fontname": "Arial Bold",
        "gridonopen": 1,
        "gridsize": [8.0, 8.0],
        "gridsnaponopen": 1,
        "objectsnaponopen": 1,
        "statusbarvisible": 2,
        "toolbarvisible": 1,
        "lefttoolbarpinned": 0,
        "toptoolbarpinned": 0,
        "righttoolbarpinned": 0,
        "bottomtoolbarpinned": 0,
        "toolbars_unpinned_last_save": 0,
        "tallnewobj": 0,
        "boxanimatetime": 500,
        "enablehscroll": 1,
        "enablevscroll": 1,
        "devicewidth": float(DEVICE_W),
        "description": "OP-XY style note punches. Goes in front of your instrument; "
                       "also drives Seq Queen FX after it on the same track.",
        "digest": "",
        "tags": "",
        "style": "",
        "subpatcher_template": "",
        "title": "Seq Queen Notes",
        "boxes": boxes,
        "lines": lines,
        "dependency_cache": [],
        "latency": 0,
        "project": {
            "version": 1,
            "creationdate": 3590052786,
            "modificationdate": 3590052786,
            "viewrect": [0.0, 0.0, 300.0, 500.0],
            "autoorganize": 1,
            "hideprojectwindow": 1,
            "showdependencies": 1,
            "autolocalize": 0,
            "contents": {"patchers": {}},
            "layout": {},
            "searchpath": {},
            "detailsvisible": 0,
            "amxdtype": 1835887981,
            "readonly": 0,
            "devpathtype": 0,
            "devpath": ".",
            "sortmode": 0,
            "viewmode": 0,
        },
        "autosave": 0,
    }
}


def write_amxd(path):
    body = json.dumps(patcher, indent="\t").encode("utf-8") + b"\n\x00"
    header = (b"ampf" + struct.pack("<I", 4) + b"mmmm"          # MIDI effect
              + b"meta" + struct.pack("<I", 4) + b"\x00\x00\x00\x00"
              + b"ptch" + struct.pack("<I", len(body)))
    with open(path, "wb") as f:
        f.write(header + body)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: build_device.py OUT.amxd")
    write_amxd(sys.argv[1])
    print("wrote %s (%d boxes, %d cords, %d px wide)" % (sys.argv[1], len(boxes), len(lines), DEVICE_W))
