#include "PitchShiftFx.h"

#include <cmath>

namespace opxy
{

int PitchShiftFx::semitones (float value) noexcept
{
    return static_cast<int> (std::lround (-12.0f + clamp (value, 0.0f, 1.0f) * 24.0f));
}

float PitchShiftFx::windowSeconds (float value) noexcept
{
    return 0.02f + clamp (value, 0.0f, 1.0f) * 0.1f;
}

void PitchShiftFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    line.prepare (sampleRate, 0.25f, 2);
    reset();
}

void PitchShiftFx::reset()
{
    PunchInFx::reset();
    line.reset();
    phase = 0.0;
}

void PitchShiftFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        intervalParam = value;
    else if (index == 1)
        windowParam = value;
}

float PitchShiftFx::getPunchParam (int index) const
{
    return index == 0 ? intervalParam : (index == 1 ? windowParam : 0.0f);
}

void PitchShiftFx::listen (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        line.write (0, left[i]);
        line.write (1, right[i]);
        line.advance();
    }
}

void PitchShiftFx::processWet (float* left, float* right, int numSamples)
{
    constexpr double pi = 3.14159265358979323846;
    const double ratio = std::pow (2.0, semitones (intervalParam) / 12.0);
    const double window = windowSeconds (windowParam) * sampleRate;
    // The delay changes by (1 - ratio) samples per sample: a shrinking delay
    // reads faster than real time (pitch up), a growing one slower (down).
    const double step = (1.0 - ratio) / window;

    for (int i = 0; i < numSamples; ++i)
    {
        line.write (0, left[i]);
        line.write (1, right[i]);
        line.advance();

        phase += step;
        phase -= std::floor (phase);
        const double other = phase + 0.5 - std::floor (phase + 0.5);

        const auto delayA = static_cast<float> (phase * window + 2.0);
        const auto delayB = static_cast<float> (other * window + 2.0);
        const double sinA = std::sin (pi * phase);
        const auto gainA = static_cast<float> (sinA * sinA);
        const float gainB = 1.0f - gainA;

        left[i]  = line.read (0, delayA) * gainA + line.read (0, delayB) * gainB;
        right[i] = line.read (1, delayA) * gainA + line.read (1, delayB) * gainB;
    }
}

} // namespace opxy
