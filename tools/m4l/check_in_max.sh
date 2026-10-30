#!/bin/sh
# Loads a .amxd inside the standalone Max app (in an [amxd~] object, which uses
# the same device loader as Live) and checks that Max survives it. Run this
# before trying a changed device in Live: a broken device file crashes Live.
#
#   tools/m4l/check_in_max.sh path/to/device.amxd
#
# Opens a Max window for ~10 seconds; Max quits by itself.
set -eu

DEVICE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
WORK="$(mktemp -d)"
RESULT="$WORK/result.txt"
PATCH="$WORK/check.maxpat"

python3 - "$DEVICE" "$RESULT" "$PATCH" <<'PY'
import json, sys
device, result, patch = sys.argv[1:4]
boxes, lines = [], []
def box(i, text, ins, outs, cls="newobj"):
    boxes.append({"box": {"id": i, "maxclass": cls, "text": text, "numinlets": ins, "numoutlets": outs,
                          "outlettype": [""] * outs, "patching_rect": [20, 20 + len(boxes) * 30, 300, 20]}})
def cord(s, o, t, i):
    lines.append({"patchline": {"source": [s, o], "destination": [t, i]}})
box("device", 'amxd~ "%s"' % device, 2, 2)
box("load", "loadbang", 1, 1)
box("wait", "delay 4000", 2, 1)
box("note", 'add loaded, write "%s"' % result, 2, 1, "message")
box("text", "text", 1, 3)
box("wait2", "delay 5000", 2, 1)
box("quit", ";\rmax quit", 2, 1, "message")
cord("load", 0, "wait", 0); cord("wait", 0, "note", 0); cord("note", 0, "text", 0)
cord("load", 0, "wait2", 0); cord("wait2", 0, "quit", 0)
json.dump({"patcher": {"fileversion": 1, "classnamespace": "box", "rect": [50, 50, 500, 400],
                       "boxes": boxes, "lines": lines}}, open(patch, "w"))
PY

# After any crash Max replays the crashed session on its next launch, which would
# crash again and make every later check fail. Start from a clean slate.
rm -f "$HOME/Library/Application Support/Cycling '74/Max 9/Crash Recovery/"maxworkspace-*.txt

if pgrep -x Max >/dev/null; then
    echo "Max is already running; quit it first." >&2
    exit 2
fi

open -a Max "$PATCH"
# Max can take half a minute to start; wait for it to appear, then to quit.
i=0
while [ $i -lt 60 ] && ! pgrep -x Max >/dev/null; do sleep 1; i=$((i + 1)); done
i=0
while [ $i -lt 90 ] && pgrep -x Max >/dev/null; do sleep 1; i=$((i + 1)); done
sleep 1

if [ ! -s "$RESULT" ]; then
    echo "FAILED: Max did not get past loading $(basename "$DEVICE") (see ~/Library/Logs/DiagnosticReports/Max-*.ips)" >&2
    exit 1
fi

# Surviving isn't enough: an object that fails to load is silently dropped along
# with its patch cords (that's how a broken live.thisdevice slipped through).
# Max logs those; the Live API itself only exists in Live, so its errors here
# are expected and ignored.
LOG="$HOME/Library/Application Support/Cycling '74/Max 9/Logs/Max.log"
PROBLEMS="$(grep -E "error\]" "$LOG" | grep -i -E "patchcord|no such object|couldn't|could not|not found" \
            | grep -v -i "liveapi\|live api" || true)"
if [ -n "$PROBLEMS" ]; then
    echo "FAILED: Max loaded $(basename "$DEVICE") but reported problems:" >&2
    echo "$PROBLEMS" | sed 's/^.*error\] \[[0-9]*\] /  /' | sort | uniq -c >&2
    exit 1
fi
echo "ok: Max loaded $(basename "$DEVICE") with every object intact"
