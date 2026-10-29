#!/usr/bin/env bash
# -----------------------------------------------------------------------------
#  Syntax-check the JUCE plugin sources WITHOUT a working JUCE build.
#
#  The plugin needs JUCE's graphics module to *compile*, which needs freetype,
#  X11 and a window toolkit. This script only needs JUCE's *headers*, so it can
#  catch typos, wrong signatures and missing members in CI or on a bare box.
#
#      tools/check_plugin_syntax.sh /path/to/JUCE
#
#  It writes a throwaway JuceHeader.h that includes the modules the plugin uses,
#  then runs `g++ -fsyntax-only` over every plugin TU. Link errors and anything
#  that only surfaces in a real build (plugin formats, bundles, the AU wrapper)
#  are NOT covered - it is a fast first pass, not a substitute for building.
# -----------------------------------------------------------------------------
set -uo pipefail

JUCE_PATH="${1:-${OPXY_JUCE_PATH:-}}"
if [[ -z "${JUCE_PATH}" || ! -d "${JUCE_PATH}/modules" ]]; then
    echo "usage: $0 /path/to/JUCE   (or set OPXY_JUCE_PATH)" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHIM="$(mktemp -d)"
trap 'rm -rf "${SHIM}"' EXIT

cat > "${SHIM}/JuceHeader.h" <<'HEADER'
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
HEADER

SOURCES=(
    "${ROOT}/Source/Plugin/ParamIDs.cpp"
    "${ROOT}/Source/Plugin/SeqQueenLookAndFeel.cpp"
    "${ROOT}/Source/Plugin/PluginProcessor.cpp"
    "${ROOT}/Source/Plugin/Components/StepGrid.cpp"
    "${ROOT}/Source/Plugin/Components/PunchPads.cpp"
    "${ROOT}/Source/Plugin/Components/PLockPanel.cpp"
    "${ROOT}/Source/Plugin/Components/TrackMixer.cpp"
    "${ROOT}/Source/Plugin/PluginEditor.cpp"
)

FAILED=0
for source in "${SOURCES[@]}"; do
    printf '%-58s' "$(basename "${source}")"
    if g++ -std=c++17 -fsyntax-only \
        -I "${SHIM}" -I "${JUCE_PATH}/modules" \
        -I "${ROOT}/Source/Core" -I "${ROOT}/Source/Plugin" \
        -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
        -DJUCE_MODULE_AVAILABLE_juce_dsp=1 -DJUCE_STRICT_REFCOUNTEDPOINTER=1 \
        -DJucePlugin_Name='"Seq Queen"' \
        -DJucePlugin_Build_VST3=0 -DJucePlugin_Build_AU=0 -DJucePlugin_Build_Standalone=0 \
        -Wall -Wextra -Wno-unused-parameter \
        "${source}" 2> "${SHIM}/out.txt"; then
        echo "ok"
    else
        echo "FAIL"
        sed 's/^/    /' "${SHIM}/out.txt" | head -20
        FAILED=1
    fi
done

if [[ ${FAILED} -ne 0 ]]; then
    echo
    echo "syntax check failed (this is NOT a full build - see the header comment)"
    exit 1
fi

echo
echo "all plugin TUs syntax-check against JUCE headers"
