#include "ReverseFx.h"

#include <algorithm>
#include <cmath>

namespace opxy
{

double ReverseFx::lengthBeats (float value) noexcept
{
    static constexpr double beats[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
    return beats[clamp (static_cast<int> (std::lround (clamp (value, 0.0f, 1.0f) * 4.0f)), 0, 4)];
}

void ReverseFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    history.prepare (sampleRate, 6.0f, 2);   // a bar at 60 bpm, three times over
    reset();
}

void ReverseFx::reset()
{
    PunchInFx::reset();
    history.reset();
    writeAbs = anchor = readOffset = 0.0;
    playing = false;
}

void ReverseFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        lengthParam = value;
    else if (index == 1)
        speedParam = value;
}

float ReverseFx::getPunchParam (int index) const
{
    return index == 0 ? lengthParam : (index == 1 ? speedParam : 0.0f);
}

void ReverseFx::setTransport (const TransportInfo& transport)
{
    if (transport.bpm > 20.0)
        bpm = transport.bpm;
}

void ReverseFx::record (float left, float right) noexcept
{
    history.write (0, left);
    history.write (1, right);
    history.advance();
    writeAbs += 1.0;
}

void ReverseFx::listen (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        record (left[i], right[i]);
}

void ReverseFx::capture() noexcept
{
    // Keep the segment short enough that the read head never falls off the
    // history, even at half speed: it can trail the write head by 3 segments.
    const double wanted = lengthBeats (lengthParam) * 60.0 / bpm * sampleRate;
    segmentLength = clamp (wanted, 64.0, static_cast<double> (history.getSize()) / 3.0 - 8.0);
    anchor = writeAbs;
    readOffset = 0.0;
}

void ReverseFx::onEngage (bool nowEngaged)
{
    // Keep reversing after release: the base class is still crossfading back to
    // dry, and switching the wet signal to dry mid-fade would click.
    if (nowEngaged)
    {
        playing = true;
        fadeSamples = std::max (1.0, sampleRate * 0.004);
        capture();
    }
}

void ReverseFx::processWet (float* left, float* right, int numSamples)
{
    const double speed = 0.5 + static_cast<double> (speedParam) * 1.5;

    for (int i = 0; i < numSamples; ++i)
    {
        record (left[i], right[i]);
        if (! playing)
            continue;

        if (readOffset >= segmentLength)
            capture();

        const auto delay = static_cast<float> (writeAbs - (anchor - readOffset));
        const double fade = std::min (1.0, std::min (readOffset, segmentLength - readOffset) / fadeSamples);
        left[i]  = history.read (0, delay) * static_cast<float> (fade);
        right[i] = history.read (1, delay) * static_cast<float> (fade);

        readOffset += speed;
    }
}

} // namespace opxy
