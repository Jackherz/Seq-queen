#pragma once

// Minimal ADSR written from scratch so the core has no JUCE dependency.
// Deliberately percussive-friendly: attack in ms, decay shaped by the track
// "decay" parameter (which is itself p-lockable), fixed short release.

#include "ScalarUtils.h"

namespace opxy
{

class Adsr
{
public:
    enum class Stage { Idle = 0, Attack, Decay, Sustain, Release };

    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
        releaseStep = 1.0f;
    }

    void noteOn (float attackSeconds, float decaySeconds, float sustainLevel) noexcept
    {
        attackStep  = stepForTime (clamp (attackSeconds, 0.0005f, 5.0f));
        decayStep   = stepForTime (clamp (decaySeconds, 0.001f, 20.0f));
        sustain     = clamp (sustainLevel, 0.0f, 1.0f);
        stage       = Stage::Attack;
    }

    void noteOff (float releaseSeconds) noexcept
    {
        releaseStep = stepForTime (clamp (releaseSeconds, 0.001f, 20.0f));
        stage = Stage::Release;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    float getLevel() const noexcept { return level; }

    /** Advances the envelope one sample and returns the new level. */
    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                break;

            case Stage::Attack:
                level += attackStep;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level -= decayStep * (1.0f - sustain);
                if (level <= sustain)
                {
                    level = sustain;
                    stage = sustain > 0.0f ? Stage::Sustain : Stage::Idle;
                    if (stage == Stage::Idle)
                        level = 0.0f;
                }
                break;

            case Stage::Sustain:
                break;

            case Stage::Release:
                level -= releaseStep * (1.0f + level);
                if (level <= 0.0001f)
                {
                    level = 0.0f;
                    stage = Stage::Idle;
                }
                break;
        }
        return level;
    }

private:
    float stepForTime (float seconds) const noexcept
    {
        return 1.0f / std::max (1.0f, static_cast<float> (sampleRate) * seconds);
    }

    double sampleRate = 44100.0;
    Stage  stage = Stage::Idle;
    float  level = 0.0f;
    float  attackStep = 0.01f, decayStep = 0.001f, releaseStep = 1.0f;
    float  sustain = 0.0f;
};

} // namespace opxy
