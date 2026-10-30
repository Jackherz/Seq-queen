#pragma once

// -----------------------------------------------------------------------------
//  PunchInFx: base class for every momentary, performative effect.
//
//  Contract for a new effect (see docs/ADDING_AN_FX.md):
//    1. Derive from PunchInFx, implement getName() + processWet().
//    2. Use onEngage(true/false) to reset or arm internal state.
//    3. Override setPunchParam()/getPunchParam() for its two macro knobs.
//    4. Leave process() alone for "replace"-style effects (dry/wet crossfade is
//       handled for you). Override process() only for "send"-style effects that
//       need a tail to keep sounding after release (delay throw, reverb burst).
//
//  Two mixing styles exist on purpose:
//    replace: out = dry * (1 - mix) + wet * mix        (tape stop, filter, ...)
//    send:    out = dry + wet * level                 (delay throw, reverb burst)
//  The send style is what lets a delay trail ring out after you let go.
// -----------------------------------------------------------------------------

#include "Types.h"
#include "ScalarUtils.h"
#include <vector>

namespace opxy
{

class PunchInFx
{
public:
    virtual ~PunchInFx() = default;

    /** Human readable name, used by the UI and the demo renderer. */
    virtual const char* getName() const = 0;

    virtual void prepare (double newSampleRate, int newMaxBlockSize);
    virtual void reset();

    void setEngaged (bool shouldBeEngaged);
    bool isEngaged() const noexcept { return engaged; }

    /** Wet amount, 0..1. Momentary engagement ramps towards this value. */
    void setAmount (float newAmount) noexcept { amount = clamp (newAmount, 0.0f, 1.0f); }
    float getAmount() const noexcept { return amount; }

    /** Crossfade time for the momentary press/release. */
    void setRampSeconds (float seconds) noexcept;

    /** Two generic macro knobs per effect (0..1 UI space, effect decides the mapping). */
    virtual void setPunchParam (int index, float value) { (void) index; (void) value; }
    virtual float getPunchParam (int index) const { (void) index; return 0.0f; }

    /** True while the effect still needs to be processed (engaged, ramping, or tailing). */
    bool isActive() const noexcept
    {
        return engaged
            || wetRamp.getCurrent() > 0.0005f
            || wetRamp.isSmoothing()
            || hasTail();
    }

    /** Processes in place. `left`/`right` must hold at least maxBlockSize samples. */
    virtual void process (float* left, float* right, int numSamples);

    /** Called instead of process() while the effect is idle. Effects that replay
        recent audio (stutter, reverse, pitch) record it here, so a punch grabs the
        beat that just played rather than whatever was left from the last punch. */
    virtual void listen (const float* left, const float* right, int numSamples)
    {
        (void) left; (void) right; (void) numSamples;
    }

    /** Host tempo and position for this block, for tempo-synced effects. */
    virtual void setTransport (const TransportInfo& transport) { (void) transport; }

protected:
    /** Called whenever engagement changes. Use for one-shot state resets. */
    virtual void onEngage (bool nowEngaged) { ignore (nowEngaged); }

    /** Effect DSP. Writes the fully wet signal into the buffers. */
    virtual void processWet (float* left, float* right, int numSamples) = 0;

    /** Override for send-style effects that keep sounding after release. */
    virtual bool hasTail() const { return false; }

    /** Copies the incoming audio so processWet() can overwrite the buffer safely. */
    void captureDry (const float* left, const float* right, int numSamples);

    float dryLeft (int i) const noexcept  { return dryL[static_cast<std::size_t> (i)]; }
    float dryRight (int i) const noexcept { return dryR[static_cast<std::size_t> (i)]; }

    static void ignore (bool) noexcept {}

    double sampleRate = 44100.0;
    int    maxBlockSize = kMaxBlockSize;
    float  amount = 1.0f;
    bool   engaged = false;

    std::vector<float> dryL, dryR;
    Ramp wetRamp;
};

} // namespace opxy
