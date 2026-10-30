cmake_minimum_required(VERSION 3.22)

project(SeqQueen VERSION 0.1.0 LANGUAGES C CXX)

# -----------------------------------------------------------------------------
#  What lives in this repo:
#
#    opxy_core          the JUCE-free engine (punch-in FX, note punches, and the
#                       old instrument's sequencer + synth)
#    opxy_core_tests    unit tests for the engine  -> no host, no GUI, no audio
#    opxy_demo          offline demo renderer      -> writes a WAV you can listen to
#    SeqQueenFX         audio effect: six punch-in FX         -> needs JUCE
#    SeqQueenNotes      MIDI device: twelve note punches      -> needs JUCE
#    seqqueen_snapshot  renders both editors to PNG           -> needs JUCE
#
#  The first three build with nothing but a C++17 compiler, which is why the
#  engine can be tested on any machine before JUCE is in place.
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(OPXY_BUILD_PLUGIN "Build the VST3/AU devices (requires JUCE)" ON)
option(OPXY_BUILD_TESTS  "Build the JUCE-free core tests" ON)
option(OPXY_BUILD_DEMO   "Build the offline demo renderer" ON)
option(OPXY_FETCH_JUCE   "Download JUCE via FetchContent if no local copy is supplied" ON)
option(OPXY_UNIVERSAL_BUILD "Build a universal macOS binary (arm64 + x86_64)" OFF)

set(OPXY_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (optional)")
set(OPXY_JUCE_TAG "8.0.14" CACHE STRING "JUCE tag to fetch when using FetchContent")

if (APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
endif()

if (APPLE AND OPXY_UNIVERSAL_BUILD)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

# -----------------------------------------------------------------------------
#  Core library (no JUCE, no dependencies)
# -----------------------------------------------------------------------------
add_library(opxy_core STATIC
    Source/Core/Engine.cpp
    Source/Core/Sequencer.cpp
    Source/Core/Pattern.cpp
    Source/Core/PatternState.cpp
    Source/Core/SynthEngine.cpp
    Source/Core/PunchInFx.cpp
    Source/Core/TapeStopFx.cpp
    Source/Core/DelayThrowFx.cpp
    Source/Core/FilterSweepFx.cpp
    Source/Core/StutterFx.cpp
    Source/Core/ReverbBurstFx.cpp
    Source/Core/BitcrushFx.cpp
    Source/Core/ReverseFx.cpp
    Source/Core/GateFx.cpp
    Source/Core/HighPassFx.cpp
    Source/Core/PitchShiftFx.cpp
    Source/Core/FxChain.cpp
    Source/Core/FxBus.cpp
    Source/Core/NoteFx.cpp
)

target_include_directories(opxy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if (MSVC)
    target_compile_options(opxy_core PRIVATE /W4)
else()
    target_compile_options(opxy_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
#  Tests
# -----------------------------------------------------------------------------
if (OPXY_BUILD_TESTS)
    enable_testing()

    add_executable(opxy_core_tests tests/CoreTests.cpp)
    target_link_libraries(opxy_core_tests PRIVATE opxy_core)
    add_test(NAME opxy_core_tests COMMAND opxy_core_tests)
endif()

# -----------------------------------------------------------------------------
#  Offline demo renderer (writes demo/opxy_demo.wav)
# -----------------------------------------------------------------------------
if (OPXY_BUILD_DEMO)
    add_executable(opxy_demo tools/render_demo.cpp)
    target_link_libraries(opxy_demo PRIVATE opxy_core)
endif()

# -----------------------------------------------------------------------------
#  The devices
# -----------------------------------------------------------------------------
if (OPXY_BUILD_PLUGIN)
    if (OPXY_JUCE_PATH AND EXISTS "${OPXY_JUCE_PATH}/CMakeLists.txt")
        message(STATUS "SeqQueen: using local JUCE at ${OPXY_JUCE_PATH}")
        add_subdirectory("${OPXY_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
        message(STATUS "SeqQueen: using vendored JUCE in external/JUCE")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    elseif (OPXY_FETCH_JUCE)
        message(STATUS "SeqQueen: fetching JUCE ${OPXY_JUCE_TAG} (set OPXY_JUCE_PATH to skip this)")
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${OPXY_JUCE_TAG}
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    else()
        message(FATAL_ERROR
            "JUCE not found. Pass -DOPXY_JUCE_PATH=/path/to/JUCE, drop a checkout in "
            "external/JUCE, or enable -DOPXY_FETCH_JUCE=ON.")
    endif()

    # -------------------------------------------------------------------------
    #  The two devices: punch FX as an audio effect, note punches as a MIDI
    #  device. Both leave the sequencing to the host.
    # -------------------------------------------------------------------------
    set(OPXY_FX_DEVICE_SOURCES
        Source/Devices/FxPlugin/FxProcessor.cpp
        Source/Devices/FxPlugin/FxEditor.cpp)
    set(OPXY_NOTES_DEVICE_SOURCES
        Source/Devices/NotesPlugin/NotesProcessor.cpp
        Source/Devices/NotesPlugin/NotesEditor.cpp)

    function(opxy_finish_device target)
        juce_generate_juce_header(${target})
        target_link_libraries(${target}
            PRIVATE
                opxy_core
                juce::juce_audio_utils
                juce::juce_gui_extra
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_lto_flags
                juce::juce_recommended_warning_flags)
        target_compile_definitions(${target} PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1)
    endfunction()

    # MusicEffect ("aumf") rather than plain Effect so the AU accepts MIDI notes.
    juce_add_plugin(SeqQueenFX
        COMPANY_NAME            "Seq Queen"
        BUNDLE_ID               "com.seqqueen.fx"
        PLUGIN_MANUFACTURER_CODE Sqqn
        PLUGIN_CODE             Sqfx
        FORMATS                 VST3 AU
        PRODUCT_NAME            "Seq Queen FX"
        DESCRIPTION             "Six punch-in FX, played from pads or MIDI notes"
        IS_SYNTH                FALSE
        NEEDS_MIDI_INPUT        TRUE
        NEEDS_MIDI_OUTPUT       FALSE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_CATEGORIES         "Fx"
        AU_MAIN_TYPE            "kAudioUnitType_MusicEffect"
    )
    target_sources(SeqQueenFX PRIVATE
        ${OPXY_FX_DEVICE_SOURCES}
        Source/Devices/FxPlugin/FxEntry.cpp
        Source/Devices/Shared/SeqQueenLookAndFeel.cpp)
    opxy_finish_device(SeqQueenFX)

    # Live won't load third-party MIDI effects, so this is an instrument that
    # outputs MIDI (and silence). Other tracks pick it as their "MIDI From".
    juce_add_plugin(SeqQueenNotes
        COMPANY_NAME            "Seq Queen"
        BUNDLE_ID               "com.seqqueen.notes"
        PLUGIN_MANUFACTURER_CODE Sqqn
        PLUGIN_CODE             Sqnt
        FORMATS                 VST3 AU
        PRODUCT_NAME            "Seq Queen Notes"
        DESCRIPTION             "OP-XY style note punches between a clip and an instrument"
        IS_SYNTH                TRUE
        NEEDS_MIDI_INPUT        TRUE
        NEEDS_MIDI_OUTPUT       TRUE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_CATEGORIES         "Instrument"
        AU_MAIN_TYPE            "kAudioUnitType_MusicDevice"
    )
    target_sources(SeqQueenNotes PRIVATE
        ${OPXY_NOTES_DEVICE_SOURCES}
        Source/Devices/NotesPlugin/NotesEntry.cpp
        Source/Devices/Shared/SeqQueenLookAndFeel.cpp)
    opxy_finish_device(SeqQueenNotes)

    # Headless editor renderer: writes PNGs of the UI, no host or window needed.
    juce_add_console_app(seqqueen_snapshot PRODUCT_NAME "seqqueen-snapshot")
    juce_generate_juce_header(seqqueen_snapshot)
    target_sources(seqqueen_snapshot PRIVATE
        tools/snapshot.cpp
        ${OPXY_FX_DEVICE_SOURCES}
        ${OPXY_NOTES_DEVICE_SOURCES}
        Source/Devices/Shared/SeqQueenLookAndFeel.cpp
    )
    target_compile_definitions(seqqueen_snapshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Seq Queen"
    )
    target_link_libraries(seqqueen_snapshot
        PRIVATE
            opxy_core
            juce::juce_audio_utils
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
    )
endif()

# -----------------------------------------------------------------------------
#  Handy summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "SeqQueen ${PROJECT_VERSION}")
message(STATUS "  plugin      : ${OPXY_BUILD_PLUGIN}")
message(STATUS "  core tests  : ${OPXY_BUILD_TESTS}")
message(STATUS "  demo        : ${OPXY_BUILD_DEMO}")
message(STATUS "")
