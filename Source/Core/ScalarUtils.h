#pragma once

// Small, dependency-free helpers. Kept header-only and constexpr where possible
// so they are trivially usable from the audio thread.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace opxy
{

template <typename T>
constexpr T clamp (T value, T lo, T hi) noexcept
{
    return value < lo ? lo : (value > hi ? hi : value);
}

constexpr float lerp (float a, float b, float t) noexcept  { return a + (b - a) * t; }

constexpr double lerp (double a, double b, double t) noexcept { return a + (b - a) * t; }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

inline float midiNoteToHz (float note) noexcept
{
    return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f);
}

/** Equal-power pan. pan = -1 hard left, +1 hard right. */
inline void panGains (float pan, float& gainL, float& gainR) noexcept
{
    const float angle = (clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265358979f;
    gainL = std::cos (angle);
    gainR = std::sin (angle);
}

/** Flush denormals / NaN / inf to zero. Cheap audio-thread hygiene. */
inline float sanitise (float x) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;
    return std::abs (x) < 1.0e-25f ? 0.0f : x;
}

/** One-pole smoothing coefficient for a given time constant. */
inline float onePoleCoeff (double sampleRate, float timeSeconds) noexcept
{
    if (timeSeconds <= 0.0f || sampleRate <= 0.0)
        return 1.0f;
    return static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * timeSeconds)));
}

/** Linear ramp generator: rises/falls towards a target over a set time. */
class Ramp
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void reset() noexcept { current = target; }

    void setTimeSeconds (float seconds) noexcept
    {
        timeSeconds = clamp (seconds, 0.0005f, 10.0f);
        stepPerSample = 1.0f / std::max (1.0f, static_cast<float> (sampleRate) * timeSeconds);
    }

    void setTarget (float t) noexcept
    {
        target = t;
        // Linear ramp in full-scale units: a 0 -> 1 move takes `timeSeconds`.
        stepPerSample = 1.0f / std::max (1.0f, static_cast<float> (sampleRate) * timeSeconds);
    }

    /** Re-arm the ramp rate (e.g. when the user changes the glide time). */
    void setTimeSecondsAndRetarget (float seconds) noexcept
    {
        setTimeSeconds (seconds);
        setTarget (target);
    }

    void snapToTarget() noexcept { current = target; }

    float next() noexcept
    {
        if (current < target)
        {
            current += stepPerSample;
            if (current >= target - 1.0e-6f)
                current = target; // snap: avoid never quite arriving
        }
        else if (current > target)
        {
            current -= stepPerSample;
            if (current <= target + 1.0e-6f)
                current = target;
        }
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept  { return target; }
    bool  isSmoothing() const noexcept { return current != target; }

private:
    double sampleRate = 44100.0;
    float  timeSeconds = 0.03f;
    float  stepPerSample = 0.0f;
    float  current = 0.0f;
    float  target = 0.0f;
};

} // namespace opxy
