#pragma once

// Deterministic, allocation-free PRNG (xorshift32).
// Deterministic matters here: unit tests need repeatable "probability" and
// "randomize" behaviour, and reproducible renders for the demo tool.

#include <cstdint>

namespace opxy
{

class Prng
{
public:
    explicit Prng (std::uint32_t seed = 0x9E3779B9u) noexcept { setSeed (seed); }

    void setSeed (std::uint32_t seed) noexcept
    {
        state = seed == 0u ? 0x9E3779B9u : seed;
    }

    std::uint32_t nextUInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /** Uniform float in [0, 1). */
    float nextFloat() noexcept
    {
        // 24 significant bits keeps us inside [0, 1) exactly.
        return static_cast<float> (nextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    /** Uniform float in [-1, 1). */
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

    /** Uniform integer in [0, bound). Returns 0 for a non-positive bound. */
    int nextInt (int bound) noexcept
    {
        if (bound <= 0)
            return 0;
        return static_cast<int> (nextUInt() % static_cast<std::uint32_t> (bound));
    }

private:
    std::uint32_t state = 0x9E3779B9u;
};

} // namespace opxy
