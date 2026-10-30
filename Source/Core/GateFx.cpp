#include "GateFx.h"

#include <cmath>

namespace opxy
{

double GateFx::divisionBeats (float value) noexcept
{
    static constexpr double beats[] = { 1.0, 0.5, 0.25, 0.125 };
    return beats[clamp (static_cast<int> (std::lround (clamp (value, 0.0f, 1.0f) * 3.0f)), 0, 3)];
}

void GateFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    reset();
}

void GateFx::reset()
{
    PunchInFx::reset();
    freeBeats = 0.0;
    gain = 1.0f;
}

void GateFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        rateParam = value;
    else if (index == 1)
        shapeParam = value;
}

float GateFx::getPunchParam (int index) const
{
    return index == 0 ? rateParam : (index == 1 ? shapeParam : 0.0f);
}

void GateFx::setTransport (const TransportInfo& transport)
{
    if (transport.bpm > 20.0)
        bpm = transport.bpm;
    locked = transport.playing && transport.hasPpq;
    blockPpq = transport.ppqAtBlockStart;
}

void GateFx::processWet (float* left, float* right, int numSamples)
{
    const double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double division = divisionBeats (rateParam);
    const float coeff = onePoleCoeff (sampleRate, 0.0015f);

    for (int i = 0; i < numSamples; ++i)
    {
        const double beats = locked ? blockPpq + i / samplesPerBeat
                                    : freeBeats + i / samplesPerBeat;
        const double phase = beats / division - std::floor (beats / division);

        const float gate = phase < 0.5 ? 1.0f : 0.0f;
        float pump = static_cast<float> (std::min (1.0, phase / 0.75));
        pump = pump * pump * (3.0f - 2.0f * pump);   // smoothstep swell

        const float target = gate + (pump - gate) * shapeParam;
        gain += (target - gain) * coeff;

        left[i]  *= gain;
        right[i] *= gain;
    }

    freeBeats += numSamples / samplesPerBeat;
}

} // namespace opxy
