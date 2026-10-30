#include "BitcrushFx.h"

#include <cmath>

namespace opxy
{

void BitcrushFx::prepare (double newSampleRate, int newMaxBlockSize)
{
    PunchInFx::prepare (newSampleRate, newMaxBlockSize);
    reset();
}

void BitcrushFx::reset()
{
    PunchInFx::reset();
    heldL = heldR = 0.0f;
    counter = 0;
}

void BitcrushFx::setPunchParam (int index, float value)
{
    value = clamp (value, 0.0f, 1.0f);
    if (index == 0)
        bitsReduction = value;
    else if (index == 1)
        rateReduction = value;
}

float BitcrushFx::getPunchParam (int index) const
{
    if (index == 0)
        return bitsReduction;
    if (index == 1)
        return rateReduction;
    return 0.0f;
}

void BitcrushFx::processWet (float* left, float* right, int numSamples)
{
    // 16 bits down to 3 bits, and 1x down to ~24x sample-rate decimation.
    const float bits = 16.0f - bitsReduction * 13.0f;
    const float levels = std::pow (2.0f, bits) * 0.5f;
    const int decimation = std::max (1, static_cast<int> (1.0f + rateReduction * 23.0f));
    const float drive = 1.0f + bitsReduction * 0.6f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (counter <= 0)
        {
            // Sample-and-hold: this is the "sample rate reduction" half.
            heldL = std::round (clamp (left[i] * drive, -1.2f, 1.2f) * levels) / levels;
            heldR = std::round (clamp (right[i] * drive, -1.2f, 1.2f) * levels) / levels;
            counter = decimation;
        }
        --counter;

        left[i]  = heldL;
        right[i] = heldR;
    }
}

} // namespace opxy
