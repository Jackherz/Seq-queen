#include "StepGrid.h"
#include "../Theme.h"

#include <cmath>

namespace opxy
{

StepGridComponent::StepGridComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void StepGridComponent::setVisibleSteps (int steps)
{
    visibleSteps = juce::jlimit (1, kMaxSteps, steps);
    repaint();
}

void StepGridComponent::setMode (Mode newMode)
{
    if (mode == newMode)
        return;
    mode = newMode;
    dragging = false;
    repaint();
}

void StepGridComponent::setSelectedStep (int track, int step)
{
    selectedTrack = juce::jlimit (0, kMaxTracks - 1, track);
    selectedStep = juce::jlimit (0, kMaxSteps - 1, step);
    repaint();
}

void StepGridComponent::timerCallback()
{
    // Cheap and predictable: the grid owns no parameter attachments, so a plain
    // repaint is the simplest way to follow the playhead.
    repaint();
}

juce::Rectangle<int> StepGridComponent::rowBounds (int row) const
{
    const int top = headerHeight;
    const int available = juce::jmax (1, getHeight() - headerHeight);
    const int count = juce::jmax (1, rowCount());
    const int height = available / count;
    return { 0, top + row * height, getWidth(), height };
}

juce::Rectangle<float> StepGridComponent::padBounds (int track, int step) const
{
    const auto row = rowBounds (track).toFloat();
    const float gridWidth = static_cast<float> (getWidth() - labelWidth);
    const float padWidth = gridWidth / static_cast<float> (visibleSteps);
    const float x = static_cast<float> (labelWidth) + padWidth * static_cast<float> (step);

    return juce::Rectangle<float> (x, row.getY() + 2.0f, padWidth - 2.0f, row.getHeight() - 4.0f);
}

int StepGridComponent::trackAt (juce::Point<int> position) const
{
    if (position.y < headerHeight)
        return -1;
    const int available = juce::jmax (1, getHeight() - headerHeight);
    const int count = juce::jmax (1, rowCount());
    const int rowHeight = juce::jmax (1, available / count);
    const int row = (position.y - headerHeight) / rowHeight;
    return juce::isPositiveAndBelow (row, count) ? row : -1;
}

int StepGridComponent::stepAt (juce::Point<int> position) const
{
    if (position.x < labelWidth)
        return -1;
    const float gridWidth = static_cast<float> (getWidth() - labelWidth);
    if (gridWidth <= 0.0f)
        return -1;
    const float padWidth = gridWidth / static_cast<float> (visibleSteps);
    const int step = static_cast<int> ((static_cast<float> (position.x - labelWidth)) / padWidth);
    return juce::isPositiveAndBelow (step, visibleSteps) ? step : -1;
}

void StepGridComponent::applyToStep (int track, int step, bool active)
{
    if (! juce::isPositiveAndBelow (track, kMaxTracks) || ! juce::isPositiveAndBelow (step, kMaxSteps))
        return;

    auto& pattern = proc.getPattern();
    pattern.tracks[track].steps[step].active.store (active);
    repaint();
}

void StepGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const int track = trackAt (e.getPosition());
    const int step = stepAt (e.getPosition());

    if (track < 0 || step < 0)
        return;

    if (mode == Mode::FxLanes)
    {
        if (e.mods.isPopupMenu())
        {
            clearLane (track);
            return;
        }

        // First edit switches the lane on, so drawing a punch just works.
        auto& pattern = proc.getPattern();
        if (! pattern.isFxLaneEnabled (track))
            pattern.setFxLaneEnabled (track, true);

        dragging = true;
        dragActiveState = pattern.getFxLaneStep (track, step) <= 0.0f;
        toggleLaneStep (track, step);
        return;
    }

    selectedTrack = track;
    selectedStep = step;

    if (onStepSelected != nullptr && (track != lastNotifiedTrack || step != lastNotifiedStep))
    {
        lastNotifiedTrack = track;
        lastNotifiedStep = step;
        onStepSelected (track, step);
    }

    if (e.mods.isPopupMenu())
    {
        // Right click only selects, so a step can be p-locked without toggling it.
        repaint();
        return;
    }

    auto& pattern = proc.getPattern();
    const bool current = pattern.tracks[track].steps[step].active.load();
    dragActiveState = ! current;
    dragging = true;

    applyToStep (track, step, dragActiveState);
}

void StepGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const int track = trackAt (e.getPosition());
    const int step = stepAt (e.getPosition());
    if (track < 0 || step < 0)
        return;

    if (mode == Mode::FxLanes)
    {
        proc.getPattern().setFxLaneStep (track, step, dragActiveState ? 1.0f : 0.0f);
        repaint();
        return;
    }

    applyToStep (track, step, dragActiveState);
}

void StepGridComponent::toggleLaneStep (int fx, int step)
{
    proc.getPattern().setFxLaneStep (fx, step, dragActiveState ? 1.0f : 0.0f);
    repaint();
}

void StepGridComponent::clearLane (int fx)
{
    proc.getPattern().clearFxLane (fx);
    proc.getPattern().setFxLaneEnabled (fx, false);
    repaint();
}

void StepGridComponent::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void StepGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);

    if (mode == Mode::FxLanes)
    {
        paintFxLanes (g);
        return;
    }

    paintTracks (g);
}

void StepGridComponent::paintFxLanes (juce::Graphics& g)
{
    auto& pattern = proc.getPattern();
    const int laneLength = pattern.getLaneLength();

    // Which lane step the playhead is on (the lanes share the pattern clock).
    int currentLaneStep = -1;
    if (proc.getSequencer().isRunning())
    {
        const auto stepPos = static_cast<int> (std::floor (proc.getSequencer().getStepPosition()));
        currentLaneStep = ((stepPos % laneLength) + laneLength) % laneLength;
    }

    // Step numbers.
    g.setFont (theme::monoFont (10.0f));
    for (int step = 0; step < visibleSteps; ++step)
    {
        if (step % kStepsPerBeat != 0)
            continue;
        const auto bounds = padBounds (0, step);
        g.setColour (step % 16 == 0 ? theme::textDim : theme::gridLine.brighter (0.4f));
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<int> (static_cast<int> (bounds.getX()), 0,
                                          static_cast<int> (bounds.getWidth() * kStepsPerBeat), headerHeight),
                    juce::Justification::centredLeft, false);
    }

    for (int f = 0; f < kMaxFx; ++f)
    {
        const auto row = rowBounds (f);
        const bool enabled = pattern.isFxLaneEnabled (f);
        const bool engaged = proc.getEngine().isFxEngaged (f);

        if (f % 2 == 0)
        {
            g.setColour (theme::background.withAlpha (0.45f));
            g.fillRect (row);
        }

        g.setColour (enabled ? theme::text : theme::textDim);
        g.setFont (theme::labelFont (10.0f));
        g.drawText (juce::String (fxTypeName (static_cast<FxType> (f))).toUpperCase(),
                    row.withTrimmedLeft (10).withWidth (labelWidth - 14),
                    juce::Justification::centredLeft, true);

        g.setColour (enabled ? theme::accentAlt : theme::textDim.withAlpha (0.6f));
        g.setFont (theme::monoFont (9.0f));
        g.drawText (enabled ? "SEQ" : "off",
                    juce::Rectangle<int> (labelWidth - 40, row.getY(), 36, row.getHeight()),
                    juce::Justification::centredRight, false);

        for (int step = 0; step < visibleSteps; ++step)
        {
            const auto pad = padBounds (f, step);
            const float value = pattern.getFxLaneStep (f, step);
            const bool beyond = step >= laneLength;
            const bool isCurrent = step == currentLaneStep;

            juce::Colour fill = value > 0.0f ? theme::accent.withAlpha (0.35f + 0.65f * value)
                                             : theme::stepOff;
            if (beyond)
                fill = fill.withAlpha (0.22f);

            g.setColour (fill);
            g.fillRoundedRectangle (pad, 3.0f);

            if (step % kStepsPerBeat == 0)
            {
                g.setColour (theme::gridLine.brighter (0.35f));
                g.drawRoundedRectangle (pad.reduced (0.5f), 3.0f, 1.0f);
            }

            if (isCurrent)
            {
                g.setColour (theme::stepOn.withAlpha (engaged ? 0.9f : 0.45f));
                g.drawRoundedRectangle (pad.reduced (1.0f), 3.0f, 1.4f);
            }
        }
    }
}

void StepGridComponent::paintTracks (juce::Graphics& g)
{
    auto& pattern = proc.getPattern();

    // Step numbers, one label per beat.
    g.setFont (theme::monoFont (10.0f));
    for (int step = 0; step < visibleSteps; ++step)
    {
        if (step % kStepsPerBeat != 0)
            continue;

        const auto bounds = padBounds (0, step);
        g.setColour (step % 16 == 0 ? theme::textDim : theme::gridLine.brighter (0.4f));
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<int> (static_cast<int> (bounds.getX()), 0,
                                          static_cast<int> (bounds.getWidth() * kStepsPerBeat), headerHeight),
                    juce::Justification::centredLeft, false);
    }

    for (int track = 0; track < kMaxTracks; ++track)
    {
        const auto row = rowBounds (track);
        const auto& trackData = pattern.tracks[track];
        const int length = trackData.getClampedLength();
        const bool muted = trackData.muted.load();
        const bool euclid = trackData.euclidOn.load();

        if (track % 2 == 0)
        {
            g.setColour (theme::background.withAlpha (0.45f));
            g.fillRect (row);
        }

        // --- track label ----------------------------------------------------
        g.setColour (muted ? theme::textDim : theme::text);
        g.setFont (theme::labelFont (11.0f));
        g.drawText ("TRACK " + juce::String (track + 1),
                    row.withTrimmedLeft (10).withWidth (labelWidth - 14),
                    juce::Justification::centredLeft, true);

        g.setColour (theme::textDim);
        g.setFont (theme::monoFont (9.0f));
        g.drawText (juce::String (length) + (euclid ? " euc" : " st"),
                    juce::Rectangle<int> (labelWidth - 42, row.getY(), 38, row.getHeight()),
                    juce::Justification::centredRight, false);

        // --- pads ------------------------------------------------------------
        for (int step = 0; step < visibleSteps; ++step)
        {
            const auto pad = padBounds (track, step);
            const auto& stepData = trackData.steps[step];

            const bool beyondLength = step >= length;
            const bool active = stepData.active.load();
            const bool isCurrentStep = proc.getSequencer().getCurrentStep (track) == step
                                        && proc.getSequencer().isRunning();
            const bool isSelected = (track == selectedTrack && step == selectedStep);

            juce::Colour fill = active ? theme::stepOn : theme::stepOff;

            if (beyondLength)
                fill = fill.withAlpha (0.22f);

            g.setColour (fill);
            g.fillRoundedRectangle (pad, 3.0f);

            // Beat markers get a brighter outline so the grid reads as bars.
            if (step % kStepsPerBeat == 0)
            {
                g.setColour (theme::gridLine.brighter (0.35f));
                g.drawRoundedRectangle (pad.reduced (0.5f), 3.0f, 1.0f);
            }

            if (isCurrentStep)
            {
                g.setColour (theme::accent);
                g.fillRoundedRectangle (pad.reduced (1.0f), 3.0f);
            }

            if (isSelected)
            {
                g.setColour (theme::accentAlt);
                g.drawRoundedRectangle (pad.reduced (1.0f), 3.0f, 1.6f);
            }

            // P-lock indicator.
            if (stepData.hasAnyPLock())
            {
                g.setColour (theme::accentAlt);
                const float dotSize = juce::jmax (3.0f, pad.getWidth() * 0.16f);
                g.fillEllipse (pad.getRight() - dotSize - 2.0f,
                               pad.getBottom() - dotSize - 2.0f, dotSize, dotSize);
            }

            // Ratchet indicator: small ticks at the bottom of the pad.
            const int ratchet = stepData.ratchet.load();
            if (ratchet > 1 && active && pad.getWidth() > 10.0f)
            {
                g.setColour (theme::accent.withAlpha (0.85f));
                const float tickWidth = juce::jmax (1.0f, pad.getWidth() * 0.06f);
                for (int r = 0; r < ratchet; ++r)
                {
                    const float x = pad.getX() + 3.0f + static_cast<float> (r) * (tickWidth * 1.8f);
                    g.fillRect (x, pad.getBottom() - 4.0f, tickWidth, 2.0f);
                }
            }
        }
    }
}

} // namespace opxy
