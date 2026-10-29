#pragma once

// One bus' worth of punch-in effects: all six types, in a fixed order.
// The engine owns two chains (master and focus track); the routing parameter of
// each effect decides which chain actually engages it.

#include "Types.h"
#include "PunchInFx.h"
#include <atomic>
#include <memory>
#include <vector>

namespace opxy
{

/** UI/automation-facing state for one punch-in effect. All atomics: the UI
    thread writes, the audio thread reads once per block. */
struct FxSettings
{
    std::atomic<bool>  engaged { false };
    std::atomic<float> amount  { 0.8f };    // wet amount 0..1
    std::atomic<float> paramA  { 0.5f };    // macro knob 1
    std::atomic<float> paramB  { 0.5f };    // macro knob 2
    std::atomic<int>   target  { static_cast<int> (FxTarget::Master) };
    std::atomic<float> rampSeconds { 0.03f };

    FxSettings() = default;
    FxSettings (const FxSettings&) = delete;
    FxSettings& operator= (const FxSettings&) = delete;
};

class FxChain
{
public:
    FxChain();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    PunchInFx& operator[] (int index) { return *fx[static_cast<std::size_t> (index)]; }
    const PunchInFx& operator[] (int index) const { return *fx[static_cast<std::size_t> (index)]; }

    int size() const noexcept { return kMaxFx; }

    /** Runs every effect that is currently active, in place. */
    void process (float* left, float* right, int numSamples);

private:
    std::vector<std::unique_ptr<PunchInFx>> fx;
};

} // namespace opxy
