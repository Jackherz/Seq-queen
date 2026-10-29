#pragma once

// Band-limited-enough oscillators. polyBLEP is used for saw/square/triangle so
// they don't alias badly at musical pitches, which is plenty for a groovebox
// voice and keeps the code small enough to read.

#include "Types.h"
#include "ScalarUtils.h"
#include "Prng.h"
#include <cmath>

namespace opxy
{

class Oscillator
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        phase = 0.0;
    }

    void reset() noexcept { phase = 0.0; }

    void setFrequency (float hz) noexcept
    {
        frequency = clamp (hz, 1.0f, static_cast<float> (sampleRate) * 0.48f);
    }

    float getFrequency() const noexcept { return frequency; }

    void setType (EngineType t) noexcept { type = t; }
    EngineType getType() const noexcept { return type; }

    /** Randomises the start phase so stacked voices don't phase-cancel. */
    void randomisePhase (Prng& rng) noexcept { phase = static_cast<double> (rng.nextFloat()); }

    float process (EngineType t, float noiseSample = 0.0f) noexcept
    {
        const double inc = static_cast<double> (frequency) / sampleRate;
        float out = 0.0f;

        switch (t)
        {
            case EngineType::Sine:
                out = static_cast<float> (std::sin (2.0 * 3.14159265358979323846 * phase));
                break;

            case EngineType::Saw:
                out = static_cast<float> (2.0 * phase - 1.0);
                out -= static_cast<float> (polyBlep (phase, inc));
                break;

            case EngineType::Square:
            {
                out = phase < 0.5 ? 1.0f : -1.0f;
                out += static_cast<float> (polyBlep (phase, inc));
                double phase2 = phase + 0.5;
                if (phase2 >= 1.0)
                    phase2 -= 1.0;
                out -= static_cast<float> (polyBlep (phase2, inc));
                break;
            }

            case EngineType::Triangle:
            {
                // Integrated square gives a triangle; DC is blocked per voice.
                const float sq = phase < 0.5 ? 1.0f : -1.0f;
                triState = 0.005f * sq + 0.995f * triState;
                out = triState * 4.0f;
                break;
            }

            case EngineType::Noise:
                out = noiseSample;
                break;

            case EngineType::Count:
                break;
        }

        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

private:
    static double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double sampleRate = 44100.0;
    double phase = 0.0;
    float  frequency = 440.0f;
    float  triState = 0.0f;
    EngineType type = EngineType::Saw;
};

} // namespace opxy
