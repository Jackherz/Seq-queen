#!/bin/sh
# Installs the Seq Queen Notes Max for Live device for the current user:
#   ~/Music/Ableton/User Library/Seq Queen/Seq Queen Notes.amxd   (the device)
#   ~/Documents/Max 9/Packages/Seq Queen/externals/seqqueen.notes.mxo
#   ~/Documents/Max 9/Packages/Seq Queen/javascript/seqqueen-link.js
# Live's Max searches its Packages folder, so the device finds both by name.
#
#   tools/m4l/install.sh path/to/seqqueen.notes.mxo
set -eu

MXO="$1"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PACKAGE="$HOME/Documents/Max 9/Packages/Seq Queen"
DEVICES="$HOME/Music/Ableton/User Library/Seq Queen"

mkdir -p "$PACKAGE/externals" "$PACKAGE/javascript" "$DEVICES"
rm -rf "$PACKAGE/externals/seqqueen.notes.mxo"
cp -R "$MXO" "$PACKAGE/externals/"
cp "$ROOT/Source/Max/seqqueen-link.js" "$PACKAGE/javascript/"
python3 "$ROOT/tools/m4l/build_device.py" "$DEVICES/Seq Queen Notes.amxd"
echo "installed into $PACKAGE and $DEVICES"
