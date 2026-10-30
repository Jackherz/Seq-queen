#include "FxChain.h"

#include "TapeStopFx.h"
#include "DelayThrowFx.h"
#include "FilterSweepFx.h"
#include "StutterFx.h"
#include "ReverbBurstFx.h"
#include "BitcrushFx.h"
#include "ReverseFx.h"
#include "GateFx.h"
#include "HighPassFx.h"
#include "PitchShiftFx.h"

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
    fx.push_back (std::make_unique<ReverseFx>());      // 6 Reverse
    fx.push_back (std::make_unique<GateFx>());         // 7 Gate
    fx.push_back (std::make_unique<HighPassFx>());     // 8 HighPass
    fx.push_back (std::make_unique<PitchShiftFx>());   // 9 PitchShift
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
    {
        if (f->isActive())
            f->process (left, right, numSamples);
        else
            f->listen (left, right, numSamples);
    }
}

void FxChain::setTransport (const TransportInfo& transport)
{
    for (auto& f : fx)
        f->setTransport (transport);
}

} // namespace opxy
