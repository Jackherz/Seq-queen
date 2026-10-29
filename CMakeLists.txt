cmake_minimum_required(VERSION 3.22)

project(SeqQueen VERSION 0.1.0 LANGUAGES C CXX)

# -----------------------------------------------------------------------------
#  Three separate things live in this repo:
#
#    opxy_core       the JUCE-free engine (sequencer, synth, punch-in FX)
#    opxy_core_tests unit tests for the engine  -> no host, no GUI, no audio
#    opxy_demo       offline demo renderer      -> writes a WAV you can listen to
#    SeqQueen        the actual VST3/AU plugin  -> needs JUCE
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

option(OPXY_BUILD_PLUGIN "Build the VST3/AU/Standalone plugin (requires JUCE)" ON)
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
    Source/Core/FxChain.cpp
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
#  The plugin
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

    juce_add_plugin(SeqQueen
        COMPANY_NAME            "Seq Queen"
        BUNDLE_ID               "com.seqqueen.seqqueen"
        PLUGIN_MANUFACTURER_CODE Sqqn
        PLUGIN_CODE             Sqxy
        FORMATS                 VST3 AU Standalone
        PRODUCT_NAME            "Seq Queen"
        DESCRIPTION             "8-track p-lock step sequencer with momentary punch-in FX"
        IS_SYNTH                TRUE
        NEEDS_MIDI_INPUT        TRUE
        NEEDS_MIDI_OUTPUT       TRUE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        VST3_CATEGORIES         "Instrument"
        AU_MAIN_TYPE            "kAudioUnitType_MusicDevice"
    )

    juce_generate_juce_header(SeqQueen)

    target_sources(SeqQueen PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/ParamIDs.cpp
        Source/Plugin/SeqQueenLookAndFeel.cpp
        Source/Plugin/Components/StepGrid.cpp
        Source/Plugin/Components/PunchPads.cpp
        Source/Plugin/Components/PLockPanel.cpp
        Source/Plugin/Components/TrackMixer.cpp
    )

    target_link_libraries(SeqQueen
        PRIVATE
            opxy_core
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    target_compile_definitions(SeqQueen PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
    )

    # Headless editor renderer: writes PNGs of the UI, no host or window needed.
    juce_add_console_app(seqqueen_snapshot PRODUCT_NAME "seqqueen-snapshot")
    juce_generate_juce_header(seqqueen_snapshot)
    target_sources(seqqueen_snapshot PRIVATE
        tools/snapshot.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/ParamIDs.cpp
        Source/Plugin/SeqQueenLookAndFeel.cpp
        Source/Plugin/Components/StepGrid.cpp
        Source/Plugin/Components/PunchPads.cpp
        Source/Plugin/Components/PLockPanel.cpp
        Source/Plugin/Components/TrackMixer.cpp
    )
    target_compile_definitions(seqqueen_snapshot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Seq Queen"
        JucePlugin_IsSynth=1
    )
    target_link_libraries(seqqueen_snapshot
        PRIVATE
            opxy_core
            juce::juce_audio_utils
            juce::juce_dsp
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
