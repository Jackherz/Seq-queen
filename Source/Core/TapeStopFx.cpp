#include "TapeStopFx.h"

#include <algorithm>
#include <cmath>

namespace opxy
{

void TapeStopFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    line.prepare (sampleRate, 3.0f, 2);
    reset();
}

void TapeStopFx::reset()
{
    PunchInFx::reset();
    line.reset();
    writeAbs = 1000.0;
    readAbs  = writeAbs - 2.0;
    rate = targetRate = 1.0f;
    rateStep = 0.0f;
    gainCompensation = 1.0f;
}

void TapeStopFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        stopTime = 0.05f + value * 1.45f;   // 50 ms .. 1.5 s
    else if (index == 1)
        startTime = 0.05f + value * 1.45f;
}

float TapeStopFx::getPunchParam (int index) const
{
    if (index == 0)
        return (stopTime - 0.05f) / 1.45f;
    if (index == 1)
        return (startTime - 0.05f) / 1.45f;
    return 0.0f;
}

void TapeStopFx::onEngage (bool nowEngaged)
{
    const float seconds = nowEngaged ? stopTime : startTime;
    const float samples = std::max (32.0f, static_cast<float> (sampleRate * seconds));

    targetRate = nowEngaged ? 0.0f : 1.0f;
    rateStep = std::abs (targetRate - rate) / samples;
    if (rateStep <= 0.0f)
        rateStep = 1.0f / samples;

    if (! nowEngaged)
    {
        // Catch the read head up to real time over the start time, capped so we
        // don't produce absurd chipmunk rates after a long hold.
        computeDelayGap();
    }
}

void TapeStopFx::computeDelayGap() noexcept
{
    const float gapSamples = std::max (0.0f, static_cast<float> (writeAbs - readAbs) - 2.0f);
    const float catchUp = gapSamples / std::max (1.0f, static_cast<float> (sampleRate * startTime));
    targetRate = clamp (1.0f + catchUp, 1.0f, 6.0f);
    rateStep = std::abs (targetRate - rate) / std::max (32.0f, static_cast<float> (sampleRate * startTime));
}

void TapeStopFx::processWet (float* left, float* right, int numSamples)
{
    const int lineSize = line.getSize();
    if (lineSize <= 8)
        return;

    const float maxDelay = static_cast<float> (lineSize - 8);

    for (int i = 0; i < numSamples; ++i)
    {
        line.write (0, left[i]);
        line.write (1, right[i]);

        // Advance the rate towards its target.
        if (rate < targetRate)      rate = std::min (targetRate, rate + rateStep);
        else if (rate > targetRate) rate = std::max (targetRate, rate - rateStep);

        line.advance();
        writeAbs += 1.0;

        readAbs += static_cast<double> (rate);

        // Keep the read head inside the buffer.
        if (writeAbs - readAbs > maxDelay)  readAbs = writeAbs - maxDelay;
        if (writeAbs - readAbs < 2.0)       readAbs = writeAbs - 2.0;

        const float delaySamples = static_cast<float> (writeAbs - readAbs);

        float l = line.read (0, delaySamples);
        float r = line.read (1, delaySamples);

        // As the tape stops, fade the frozen signal so it dies away instead of
        // buzzing on a single sample forever.
        gainCompensation = clamp (rate * 1.8f, 0.0f, 1.0f);
        const float gain = gainCompensation;

        left[i]  = l * gain;
        right[i] = r * gain;
    }

    // Fully idle again: snap the head back to real time so the next punch starts
    // from a clean state.
    if (! engaged && wetRamp.getCurrent() < 0.001f)
    {
        readAbs = writeAbs - 2.0;
        rate = targetRate = 1.0f;
        rateStep = 0.0f;
    }
}

} // namespace opxy
