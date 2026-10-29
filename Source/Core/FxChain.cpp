#include "FxChain.h"

#include "TapeStopFx.h"
#include "DelayThrowFx.h"
#include "FilterSweepFx.h"
#include "StutterFx.h"
#include "ReverbBurstFx.h"
#include "BitcrushFx.h"

namespace opxy
{

FxChain::FxChain()
{
    // Order must match FxType / kMaxFx. Effects processes in this order.
    fx.push_back (std::make_unique<TapeStopFx>());     // 0 TapeStop
    fx.push_back (std::make_unique<DelayThrowFx>());   // 1 DelayThrow
    fx.push_back (std::make_unique<FilterSweepFx>());  // 2 FilterSweep
    fx.push_back (std::make_unique<StutterFx>());      // 3 Stutter
    fx.push_back (std::make_unique<ReverbBurstFx>());  // 4 ReverbBurst
    fx.push_back (std::make_unique<BitcrushFx>());     // 5 Bitcrush
}

void FxChain::prepare (double sampleRate, int maxBlockSize)
{
    for (auto& f : fx)
        f->prepare (sampleRate, maxBlockSize);
}

void FxChain::reset()
{
    for (auto& f : fx)
        f->reset();
}

void FxChain::process (float* left, float* right, int numSamples)
{
    for (auto& f : fx)
        if (f->isActive())
            f->process (left, right, numSamples);
}

} // namespace opxy
