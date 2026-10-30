#pragma once

// -----------------------------------------------------------------------------
//  Shared constants, enums and small value types for the core engine.
//
//  IMPORTANT: nothing in Source/Core may include JUCE. The core is plain C++17
//  so it can be built and unit-tested without a GUI/audio stack (see
//  tests/CoreTests.cpp and CMakeLists.txt target "opxy_core").
// -----------------------------------------------------------------------------

#include <cstdint>

namespace opxy
{

inline constexpr int kMaxTracks       = 8;    // OP-XY has 8 tracks
inline constexpr int kMaxSteps        = 64;   // up to 64 steps per track
inline constexpr int kStepsPerBeat    = 4;    // 16th note resolution
inline constexpr int kMaxVoices       = 24;   // shared polyphony pool
inline constexpr int kMaxBlockSize    = 2048; // core renders at most this many samples per call
inline constexpr int kMaxEventsPerBlock = 256; // step/ratchet events per block (fixed, no allocation)
inline constexpr int kMaxRatchet      = 8;    // sub-hits per step
inline constexpr int kMaxSubHits      = kMaxTracks * kMaxRatchet;

/** Per-step parameters that can be "p-locked" (parameter locked) to a value. */
enum class PlockParam : int
{
    Pitch = 0,   // semitone offset applied to the step note
    Cutoff,      // filter cutoff in Hz
    Resonance,   // 0..1
    Decay,       // amp decay time in seconds
    Velocity,    // 0..1
    Pan,         // -1..1
    Count
};

inline constexpr int kNumPlockParams = static_cast<int> (PlockParam::Count);

/** Simple oscillator flavours. Stands in for OP-XY's per-track engines. */
enum class EngineType : int
{
    Sine = 0,
    Triangle,
    Saw,
    Square,
    Noise,
    Count
};

/** The punch-in effects. Order matters: it is the UI order and the FX index. */
enum class FxType : int
{
    TapeStop = 0,
    DelayThrow,
    FilterSweep,
    Stutter,
    ReverbBurst,
    Bitcrush,
    Reverse,       // appended: indices are saved in sessions, never reorder
    Gate,
    HighPass,
    PitchShift,
    Count
};

/** Number of punch-in FX types. Derived so appending to FxType is enough. */
inline constexpr int kMaxFx = static_cast<int> (FxType::Count);

/** Where a punch-in FX is applied: the master bus, or the focused track. */
enum class FxTarget : int
{
    Master = 0,
    FocusTrack,
    Count
};

/** Scale types used by the "quantise to scale" brain. Index 0 means "off". */
enum class ScaleType : int
{
    Off = 0,
    Major,
    Minor,
    Dorian,
    Pentatonic,
    Chromatic,
    Count
};

/** Transport information handed to the engine once per block. */
struct TransportInfo
{
    bool   playing    = false;
    bool   hasPpq     = false;   // true when the host reported a ppq position
    double ppqAtBlockStart = 0.0;
    double bpm        = 120.0;
};

/** A note event produced by the sequencer, with a sample offset in the block.
    Carries the fully resolved voice settings (p-locks already applied) so the
    synth engine does not have to re-read the pattern while rendering. */
struct StepEvent
{
    int   sampleOffset = 0;
    int   track        = 0;
    int   step         = 0;
    int   note         = 60;
    float velocity     = 0.8f;
    float gateSteps    = 0.5f;    // note length in 16th steps

    int   engine       = 2;       // EngineType
    float cutoff       = 3000.0f;
    float resonance    = 0.2f;
    float decay        = 0.35f;
    float pan          = 0.0f;
    float level        = 0.7f;
    bool  noiseLayer   = false;
    bool  primary      = true;    // false for ratchet sub-hits
};

inline const char* plockParamName (PlockParam p) noexcept
{
    switch (p)
    {
        case PlockParam::Pitch:     return "Pitch";
        case PlockParam::Cutoff:    return "Cutoff";
        case PlockParam::Resonance: return "Resonance";
        case PlockParam::Decay:     return "Decay";
        case PlockParam::Velocity:  return "Velocity";
        case PlockParam::Pan:       return "Pan";
        case PlockParam::Count:     break;
    }
    return "?";
}

inline const char* plockParamId (PlockParam p) noexcept
{
    switch (p)
    {
        case PlockParam::Pitch:     return "pitch";
        case PlockParam::Cutoff:    return "cutoff";
        case PlockParam::Resonance: return "resonance";
        case PlockParam::Decay:     return "decay";
        case PlockParam::Velocity:  return "velocity";
        case PlockParam::Pan:       return "pan";
        case PlockParam::Count:     break;
    }
    return "?";
}

inline const char* engineTypeName (EngineType e) noexcept
{
    switch (e)
    {
        case EngineType::Sine:     return "Sine";
        case EngineType::Triangle: return "Triangle";
        case EngineType::Saw:      return "Saw";
        case EngineType::Square:   return "Square";
        case EngineType::Noise:    return "Noise";
        case EngineType::Count:    break;
    }
    return "?";
}

inline const char* fxTypeName (FxType f) noexcept
{
    switch (f)
    {
        case FxType::TapeStop:    return "Tape Stop";
        case FxType::DelayThrow:  return "Delay Throw";
        case FxType::FilterSweep: return "Filter";
        case FxType::Stutter:     return "Stutter";
        case FxType::ReverbBurst: return "Reverb";
        case FxType::Bitcrush:    return "Bitcrush";
        case FxType::Reverse:     return "Reverse";
        case FxType::Gate:        return "Gate";
        case FxType::HighPass:    return "High-Pass";
        case FxType::PitchShift:  return "Pitch";
        case FxType::Count:       break;
    }
    return "?";
}

inline const char* fxTypeId (FxType f) noexcept
{
    switch (f)
    {
        case FxType::TapeStop:    return "tape";
        case FxType::DelayThrow:  return "delay";
        case FxType::FilterSweep: return "filter";
        case FxType::Stutter:     return "stutter";
        case FxType::ReverbBurst: return "reverb";
        case FxType::Bitcrush:    return "crush";
        case FxType::Reverse:     return "reverse";
        case FxType::Gate:        return "gate";
        case FxType::HighPass:    return "highpass";
        case FxType::PitchShift:  return "pitch";
        case FxType::Count:       break;
    }
    return "?";
}

inline const char* scaleTypeName (ScaleType s) noexcept
{
    switch (s)
    {
        case ScaleType::Off:        return "Off";
        case ScaleType::Major:      return "Major";
        case ScaleType::Minor:      return "Minor";
        case ScaleType::Dorian:     return "Dorian";
        case ScaleType::Pentatonic: return "Pentatonic";
        case ScaleType::Chromatic:  return "Chromatic";
        case ScaleType::Count:      break;
    }
    return "?";
}

} // namespace opxy
