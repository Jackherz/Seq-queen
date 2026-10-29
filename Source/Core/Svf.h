#pragma once

// Topology-preserving-transform (TPT) state variable filter.
// Stable at high resonance, cheap, and gives LP/BP/HP from one structure.
// Used both for per-voice filtering and for the FilterSweep punch-in FX.

#include "ScalarUtils.h"
#include <cmath>

namespace opxy
{

class Svf
{
public:
    enum class Mode : int { LowPass = 0, BandPass, HighPass };

    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        ic1 = ic2 = 0.0f;
    }

    /** cutoffHz is clamped to a musically safe range; resonance 0..1. */
    void setParams (float cutoffHz, float resonance) noexcept
    {
        const float c = clamp (cutoffHz, 20.0f, static_cast<float> (sampleRate * 0.45));
        const float r = clamp (resonance, 0.0f, 0.99f);

        // Pre-warped integrator coefficient.
        const float wd = 2.0f * 3.14159265f * c;
        const float T  = 1.0f / static_cast<float> (sampleRate);
        const float wa = (2.0f / T) * std::tan (wd * T * 0.5f);
        g = wa * T * 0.5f;

        // Damping: 2.0 is critically damped, small values = high resonance.
        k = 2.0f - 1.98f * r;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float in, Mode mode) noexcept
    {
        const float v3 = in - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        ic1 = sanitise (ic1);
        ic2 = sanitise (ic2);

        switch (mode)
        {
            case Mode::LowPass:  return v2;
            case Mode::BandPass: return v1;
            case Mode::HighPass: return in - k * v1 - v2;
        }
        return v2;
    }

private:
    double sampleRate = 44100.0;
    float g = 0.5f, k = 1.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

/** One-pole high-pass used to remove DC / sub-sonic build-up from oscillators. */
class DcBlocker
{
public:
    void prepare (double sr) noexcept
    {
        coeff = static_cast<float> (1.0 - (2.0 * 3.141592653589793 * 12.0 / (sr > 0.0 ? sr : 44100.0)));
        x1 = y1 = 0.0f;
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float in) noexcept
    {
        const float y = in - x1 + coeff * y1;
        x1 = in;
        y1 = sanitise (y);
        return y;
    }

private:
    float coeff = 0.998f;
    float x1 = 0.0f, y1 = 0.0f;
};

} // namespace opxy
