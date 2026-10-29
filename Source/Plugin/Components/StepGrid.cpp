#include "StepGrid.h"
#include "../Theme.h"

#include <cmath>

namespace opxy
{
namespace
{
// Layout constants, shared by paint() and the hit-testing so they can't drift.
constexpr int kLabelWidth = 92;
constexpr int kHeaderHeight = 16;
constexpr float kPadGap = 2.0f;
constexpr float kPadRadius = 1.5f;
}

StepGridComponent::StepGridComponent (SeqQueenAudioProcessor& processor)
    : proc (processor)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void StepGridComponent::setMode (Mode newMode)
{
    if (mode == newMode)
        return;
    mode = newMode;
    dragging = false;
    repaint();
}

void StepGridComponent::setVisibleSteps (int steps)
{
    visibleSteps = juce::jlimit (1, kMaxSteps, steps);
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
    // The grid owns no parameter attachments: a plain repaint is the simplest
    // way to follow the playhead and the lane clock.
    repaint();
}

juce::Rectangle<int> StepGridComponent::rowBounds (int row) const
{
    const int count = juce::jmax (1, rowCount());
    const int available = juce::jmax (1, getHeight() - kHeaderHeight);
    const int height = available / count;
    return { 0, kHeaderHeight + row * height, getWidth(), height };
}

juce::Rectangle<float> StepGridComponent::padBounds (int row, int step) const
{
    const auto rowArea = rowBounds (row).toFloat();
    const float gridWidth = static_cast<float> (getWidth() - kLabelWidth);
    const float padWidth = gridWidth / static_cast<float> (visibleSteps);
    const float x = static_cast<float> (kLabelWidth) + padWidth * static_cast<float> (step);

    return { x, rowArea.getY() + 3.0f, padWidth - kPadGap, rowArea.getHeight() - 7.0f };
}

int StepGridComponent::stepWidthAt (int row) const
{
    juce::ignoreUnused (row);
    const float gridWidth = static_cast<float> (getWidth() - kLabelWidth);
    return juce::jmax (1, static_cast<int> (gridWidth / static_cast<float> (visibleSteps)));
}

int StepGridComponent::trackAt (juce::Point<int> position) const
{
    if (position.y < kHeaderHeight)
        return -1;

    const int count = juce::jmax (1, rowCount());
    const int rowHeight = juce::jmax (1, (getHeight() - kHeaderHeight) / count);
    const int row = (position.y - kHeaderHeight) / rowHeight;
    return juce::isPositiveAndBelow (row, count) ? row : -1;
}

int StepGridComponent::stepAt (juce::Point<int> position) const
{
    if (position.x < kLabelWidth)
        return -1;

    const float gridWidth = static_cast<float> (getWidth() - kLabelWidth);
    if (gridWidth <= 0.0f)
        return -1;

    const float padWidth = gridWidth / static_cast<float> (visibleSteps);
    const int step = static_cast<int> ((static_cast<float> (position.x - kLabelWidth)) / padWidth);
    return juce::isPositiveAndBelow (step, visibleSteps) ? step : -1;
}

void StepGridComponent::applyToStep (int track, int step, bool active)
{
    if (! juce::isPositiveAndBelow (track, kMaxTracks) || ! juce::isPositiveAndBelow (step, kMaxSteps))
        return;

    proc.getPattern().tracks[track].steps[step].active.store (active);
    repaint();
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

    const bool current = proc.getPattern().tracks[track].steps[step].active.load();
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

void StepGridComponent::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

// ------------------------------------------------------------------- paint ---

void StepGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (ui::panelSunken);

    if (mode == Mode::FxLanes)
        paintFxLanes (g);
    else
        paintTracks (g);
}

namespace
{
/** Step ruler: numbers every beat, brighter at bar starts. */
void paintRuler (juce::Graphics& g, const juce::Rectangle<int>& area, int visibleSteps,
                 int labelWidth)
{
    const float gridWidth = static_cast<float> (area.getWidth() - labelWidth);
    const float padWidth = gridWidth / static_cast<float> (visibleSteps);

    for (int step = 0; step < visibleSteps; step += kStepsPerBeat)
    {
        const bool barStart = (step % 16) == 0;
        const auto x = static_cast<int> (static_cast<float> (labelWidth) + padWidth * static_cast<float> (step));
        ui::text (g, { x, area.getY(), static_cast<int> (padWidth * kStepsPerBeat), kHeaderHeight },
                  juce::String (step + 1).paddedLeft ('0', 2),
                  ui::monoFont (7.5f), barStart ? ui::boneDim : ui::textFaint);
    }
}

/** Beat separators plus the playhead column. */
void paintGridLines (juce::Graphics& g, const juce::Rectangle<int>& area, int visibleSteps,
                     int labelWidth, int playheadStep, bool playing)
{
    const float gridWidth = static_cast<float> (area.getWidth() - labelWidth);
    const float padWidth = gridWidth / static_cast<float> (visibleSteps);
    const float top = static_cast<float> (area.getY() + kHeaderHeight);
    const float bottom = static_cast<float> (area.getBottom());

    g.setColour (ui::hairline.withAlpha (0.85f));
    for (int beat = 1; beat * kStepsPerBeat < visibleSteps; ++beat)
    {
        const float x = static_cast<float> (labelWidth) + padWidth * static_cast<float> (beat * kStepsPerBeat);
        g.drawLine (x, top, x, bottom, 1.0f);
    }

    if (playheadStep >= 0 && playing)
    {
        const float columnX = static_cast<float> (labelWidth) + padWidth * static_cast<float> (playheadStep);
        g.setColour (ui::amber.withAlpha (0.07f));
        g.fillRect (columnX, top, padWidth, bottom - top);
        g.setColour (ui::amber.withAlpha (0.85f));
        g.drawLine (columnX, top, columnX, bottom, 1.0f);
        g.fillRect (columnX + padWidth - 1.0f, top, 1.0f, bottom - top);
    }
}
} // namespace

void StepGridComponent::paintTracks (juce::Graphics& g)
{
    auto& pattern = proc.getPattern();
    auto& sequencer = proc.getSequencer();

    paintRuler (g, getLocalBounds(), visibleSteps, kLabelWidth);

    const int playheadStep = sequencer.isRunning()
                               ? static_cast<int> (std::fmod (sequencer.getStepPosition(),
                                                              static_cast<double> (visibleSteps)))
                               : -1;
    paintGridLines (g, getLocalBounds(), visibleSteps, kLabelWidth, playheadStep, sequencer.isRunning());

    // Dot-matrix texture in the label gutter, straight off the reference sheets.
    ui::dotGrid (g, { 8, kHeaderHeight + 4, 10, getHeight() - kHeaderHeight - 10 },
                 7.0f, 0.65f, juce::Colour (0xff1c2027));

    for (int track = 0; track < kMaxTracks; ++track)
    {
        const auto row = rowBounds (track);
        const auto& trackData = pattern.tracks[track];
        const int length = trackData.getClampedLength();
        const bool muted = trackData.muted.load();
        const bool euclid = trackData.euclidOn.load();
        const bool hasSteps = [&trackData, length]
        {
            for (int s = 0; s < length; ++s)
                if (trackData.steps[s].active.load())
                    return true;
            return false;
        }();

        if (track % 2 == 0)
        {
            g.setColour (juce::Colour (0xff0f1115));
            g.fillRect (row);
        }

        // --- label -----------------------------------------------------------
        const auto labelArea = row.withTrimmedLeft (14).withWidth (kLabelWidth - 46);
        ui::text (g, labelArea.withTrimmedTop (3),
                  "TRACK " + juce::String (track + 1),
                  ui::monoFont (8.5f, true),
                  muted ? ui::textFaint : (hasSteps ? ui::boneDim : ui::textFaint));
        ui::text (g, labelArea.withTrimmedTop (13),
                  static_cast<EngineType> (trackData.engine.load()) < EngineType::Count
                      ? juce::String (engineTypeName (static_cast<EngineType> (trackData.engine.load()))).toUpperCase()
                      : juce::String ("—"),
                  ui::monoFont (6.8f), ui::textFaint);

        // length / brain flags, right-aligned in the gutter
        juce::String flags = juce::String (length);
        if (euclid)
            flags << " e";
        ui::text (g, row.withTrimmedLeft (kLabelWidth - 34).withWidth (28),
                  flags, ui::monoFont (6.8f), euclid ? ui::green : ui::textFaint,
                  juce::Justification::centredRight);

        // --- pads ------------------------------------------------------------
        for (int step = 0; step < visibleSteps; ++step)
        {
            const auto pad = padBounds (track, step);
            const auto& stepData = trackData.steps[step];
            const bool beyondLength = step >= length;
            const bool active = stepData.active.load();
            const bool selected = (track == selectedTrack && step == selectedStep);

            if (active)
            {
                g.setColour (ui::bone.withAlpha (beyondLength ? 0.28f : 1.0f));
                g.fillRect (pad);
            }
            else
            {
                g.setColour (beyondLength ? ui::hairline.withAlpha (0.45f) : ui::hairlineLit);
                g.drawRect (pad, 1.0f);
            }

            // Beat markers: small notch in the top-left of every beat pad.
            if (step % kStepsPerBeat == 0 && ! active)
            {
                g.setColour (ui::hairlineLit.brighter (0.3f));
                g.fillRect (pad.getX(), pad.getY(), 2.0f, 3.0f);
            }

            if (selected)
            {
                g.setColour (ui::amber);
                g.drawRect (pad.expanded (0.5f), 1.4f);
            }

            // P-lock marker: cyan dot, bottom right.
            if (stepData.hasAnyPLock())
            {
                g.setColour (ui::cyan);
                g.fillEllipse (pad.getRight() - 5.0f, pad.getBottom() - 5.0f, 3.0f, 3.0f);
            }

            // Ratchet ticks: amber dashes across the bottom edge.
            const int ratchet = stepData.ratchet.load();
            if (ratchet > 1 && active)
            {
                g.setColour (ui::amber);
                for (int r = 0; r < ratchet; ++r)
                {
                    const float x = pad.getX() + 2.5f + static_cast<float> (r) * 3.4f;
                    if (x < pad.getRight() - 2.0f)
                        g.fillRect (x, pad.getBottom() - 3.0f, 2.2f, 1.2f);
                }
            }
        }
    }
}

void StepGridComponent::paintFxLanes (juce::Graphics& g)
{
    auto& pattern = proc.getPattern();
    auto& sequencer = proc.getSequencer();
    const int laneLength = pattern.getLaneLength();

    paintRuler (g, getLocalBounds(), visibleSteps, kLabelWidth);

    int playheadStep = -1;
    if (sequencer.isRunning())
    {
        const int absolute = static_cast<int> (std::floor (sequencer.getStepPosition()));
        playheadStep = ((absolute % visibleSteps) + visibleSteps) % visibleSteps;
    }
    paintGridLines (g, getLocalBounds(), visibleSteps, kLabelWidth, playheadStep, sequencer.isRunning());

    ui::dotGrid (g, { 8, kHeaderHeight + 4, 10, getHeight() - kHeaderHeight - 10 },
                 7.0f, 0.65f, juce::Colour (0xff1c2027));

    for (int f = 0; f < kMaxFx; ++f)
    {
        const auto row = rowBounds (f);
        const auto colour = ui::fxAccent (f);
        const bool enabled = pattern.isFxLaneEnabled (f);
        const bool engaged = proc.getEngine().isFxEngaged (f);

        if (f % 2 == 0)
        {
            g.setColour (juce::Colour (0xff0f1115));
            g.fillRect (row);
        }

        // Label + state dot
        g.setColour (enabled ? colour : colour.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (static_cast<float> (row.getX() + 14),
                                            static_cast<float> (row.getCentreY()) - 1.5f, 3.0f, 3.0f));

        ui::text (g, row.withTrimmedLeft (24).withWidth (kLabelWidth - 58),
                  juce::String (fxTypeName (static_cast<FxType> (f))).toUpperCase(),
                  ui::monoFont (8.5f, true),
                  enabled ? ui::boneDim : ui::textFaint);

        ui::text (g, row.withTrimmedLeft (kLabelWidth - 34).withWidth (28),
                  enabled ? "SEQ" : "OFF", ui::monoFont (6.8f),
                  enabled ? colour : ui::textFaint, juce::Justification::centredRight);

        for (int step = 0; step < visibleSteps; ++step)
        {
            const auto pad = padBounds (f, step);
            const float value = pattern.getFxLaneStep (f, step);
            const bool beyond = step >= laneLength;

            if (value > 0.0f)
            {
                // Lane height encodes amount: a half-strength step is a half bar.
                const float h = pad.getHeight() * juce::jlimit (0.15f, 1.0f, value);
                g.setColour (colour.withAlpha (beyond ? 0.25f : 0.92f));
                g.fillRect (pad.withTop (pad.getBottom() - h));
            }
            else
            {
                g.setColour (beyond ? ui::hairline.withAlpha (0.4f) : ui::hairlineLit.withAlpha (0.75f));
                g.drawRect (pad, 1.0f);
            }

            if (engaged && pad.getHeight() > 6.0f)
            {
                g.setColour (colour.withAlpha (0.35f));
                g.drawRect (pad.reduced (0.5f), 1.0f);
            }
        }
    }
}

} // namespace opxy
