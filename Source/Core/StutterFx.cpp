#include "StutterFx.h"

#include <cmath>

namespace opxy
{

void StutterFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    history.prepare (sampleRate, 2.0f, 2);
    reset();
}

void StutterFx::reset()
{
    PunchInFx::reset();
    history.reset();
    writeAbs = 0.0;
    loopStart = 0.0;
    playPos = 0.0;
    looping = false;
}

void StutterFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        sliceSeconds = 0.02f + value * 0.48f;         // 20 ms .. 500 ms
    else if (index == 1)
        rateParam = value;
}

float StutterFx::getPunchParam (int index) const
{
    if (index == 0)
        return (sliceSeconds - 0.02f) / 0.48f;
    if (index == 1)
        return rateParam;
    return 0.0f;
}

void StutterFx::onEngage (bool nowEngaged)
{
    if (nowEngaged)
    {
        // Grab the slice that just played, so the repeat feels like a natural
        // continuation of what the listener just heard.
        loopLength = clamp (static_cast<double> (sliceSeconds * sampleRate),
                            32.0, static_cast<double> (history.getSize() / 2));
        loopStart  = writeAbs - loopLength;
        if (loopStart < 0.0)
            loopStart = 0.0;

        playPos = loopStart;
        playbackRate = 0.5 + static_cast<double> (rateParam) * 1.5;
        looping = true;
        loopElapsed = 0.0;
        fadeSamples = std::max (1.0, static_cast<double> (sampleRate) * 0.004); // 4 ms
    }
    else
    {
        looping = false;
    }
}

void StutterFx::record (float left, float right) noexcept
{
    history.write (0, left);
    history.write (1, right);
    history.advance();
    writeAbs += 1.0;
}

void StutterFx::listen (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        record (left[i], right[i]);
}

void StutterFx::processWet (float* left, float* right, int numSamples)
{
    const int size = history.getSize();
    if (size <= 8)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        // Keep a rolling history of the dry input.
        record (left[i], right[i]);

        if (looping && loopLength > 1.0)
        {
            if (playPos >= loopStart + loopLength)
            {
                // Wrap: restart the slice and restart the declick window.
                playPos = loopStart + std::fmod (playPos - loopStart, loopLength);
                loopElapsed = 0.0;
            }

            const float delay = static_cast<float> (writeAbs - playPos);
            float fadeGain = 1.0f;

            // Fade in/out at the slice edges so the repeat doesn't crack.
            const double remaining = (loopStart + loopLength) - playPos;
            const double fadeIn  = std::min (1.0, loopElapsed / fadeSamples);
            const double fadeOut = std::min (1.0, remaining / fadeSamples);
            fadeGain = static_cast<float> (std::min (fadeIn, fadeOut));

            left[i]  = history.read (0, delay) * fadeGain;
            right[i] = history.read (1, delay) * fadeGain;

            playPos += playbackRate;
            loopElapsed += 1.0;
        }
    }
}

} // namespace opxy
