#include "FxBus.h"

#include <algorithm>

namespace opxy
{

FxBus::FxBus()
{
    for (auto& a : effectiveAmount)
        a.store (0.0f);
    for (auto& e : engaged)
        e.store (false);
}

void FxBus::prepare (double sampleRate, int newMaxBlockSize)
{
    maxBlockSize = clamp (newMaxBlockSize, 16, kMaxBlockSize);
    chain.prepare (sampleRate, kMaxBlockSize);
}

void FxBus::reset()
{
    chain.reset();
}

FxSettings& FxBus::getSettings (int fxIndex) noexcept
{
    return settings[clamp (fxIndex, 0, kMaxFx - 1)];
}

const FxSettings& FxBus::getSettings (int fxIndex) const noexcept
{
    return settings[clamp (fxIndex, 0, kMaxFx - 1)];
}

void FxBus::setPunchVelocity (int fxIndex, float velocity) noexcept
{
    if (fxIndex >= 0 && fxIndex < kMaxFx)
        settings[fxIndex].velocity.store (clamp (velocity, 0.0f, 1.0f), std::memory_order_relaxed);
}

float FxBus::getEffectiveAmount (int fxIndex) const noexcept
{
    return effectiveAmount[clamp (fxIndex, 0, kMaxFx - 1)].load (std::memory_order_relaxed);
}

bool FxBus::isEngaged (int fxIndex) const noexcept
{
    return engaged[clamp (fxIndex, 0, kMaxFx - 1)].load (std::memory_order_relaxed);
}

void FxBus::applySettings()
{
    // Same velocity rule as the instrument (Engine::applyFxRouting), so a soft
    // trigger note punches less in both devices.
    for (int f = 0; f < kMaxFx; ++f)
    {
        const FxSettings& s = settings[f];
        const bool on = s.engaged.load (std::memory_order_relaxed);
        const float sensitivity = clamp (s.velocitySensitivity.load (std::memory_order_relaxed), 0.0f, 1.0f);
        const float velocity = clamp (s.velocity.load (std::memory_order_relaxed), 0.0f, 1.0f);
        const float amount = clamp (s.amount.load (std::memory_order_relaxed)
                                        * (1.0f - sensitivity * (1.0f - velocity)),
                                    0.0f, 1.0f);

        PunchInFx& fx = chain[f];
        fx.setAmount (amount);
        fx.setRampSeconds (s.rampSeconds.load (std::memory_order_relaxed));
        fx.setPunchParam (0, s.paramA.load (std::memory_order_relaxed));
        fx.setPunchParam (1, s.paramB.load (std::memory_order_relaxed));
        fx.setEngaged (on);

        effectiveAmount[f].store (on ? amount : 0.0f, std::memory_order_relaxed);
        engaged[f].store (on, std::memory_order_relaxed);
    }
}

void FxBus::process (float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    applySettings();

    for (int offset = 0; offset < numSamples;)
    {
        const int chunk = std::min (numSamples - offset, maxBlockSize);
        chain.process (left + offset, right + offset, chunk);
        offset += chunk;
    }
}

} // namespace opxy
