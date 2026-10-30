#pragma once

// Stereo ring buffer with fractional delay read (linear interpolation).
// Used by the tape-stop and delay-throw punch-in effects.
//
// Allocation happens only in prepare(); process() never allocates, which is a
// hard requirement for the audio thread.

#include "ScalarUtils.h"
#include <cstdint>
#include <vector>

namespace opxy
{

class DelayLine
{
public:
    void prepare (double sr, float maxDelaySeconds, int numChannels = 2)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        channels = clamp (numChannels, 1, 2);
        const int size = static_cast<int> (sampleRate * maxDelaySeconds) + 8;
        for (int ch = 0; ch < channels; ++ch)
            buffer[ch].assign (static_cast<std::size_t> (size), 0.0f);
        writePos = 0;
    }

    void reset()
    {
        for (int ch = 0; ch < channels; ++ch)
            for (auto& s : buffer[ch])
                s = 0.0f;
        writePos = 0;
    }

    int getSize() const noexcept { return channels > 0 ? static_cast<int> (buffer[0].size()) : 0; }

    void write (int channel, float sample) noexcept
    {
        if (channel < channels && ! buffer[channel].empty())
            buffer[channel][static_cast<std::size_t> (writePos)] = sanitise (sample);
    }

    void advance() noexcept
    {
        if (getSize() > 0)
            writePos = (writePos + 1) % getSize();
    }

    int getWritePos() const noexcept { return writePos; }

    /** Reads `delaySamples` behind the write head, interpolating. */
    float read (int channel, float delaySamples) const noexcept
    {
        const int size = getSize();
        if (size == 0 || channel >= channels)
            return 0.0f;

        float readPos = static_cast<float> (writePos) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float> (size);
        while (readPos >= static_cast<float> (size))
            readPos -= static_cast<float> (size);

        const int   i0   = static_cast<int> (readPos);
        const int   i1   = (i0 + 1) % size;
        const float frac = readPos - static_cast<float> (i0);

        const auto& data = buffer[channel];
        return data[static_cast<std::size_t> (i0)] * (1.0f - frac)
             + data[static_cast<std::size_t> (i1)] * frac;
    }

    double getSampleRate() const noexcept { return sampleRate; }

private:
    double sampleRate = 44100.0;
    int    channels = 2;
    int    writePos = 0;
    std::vector<float> buffer[2];
};

} // namespace opxy
