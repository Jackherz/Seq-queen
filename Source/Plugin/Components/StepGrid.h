#pragma once

// The 8 x N step grid: click to toggle a step, drag to paint, right-click to
// select a step for p-lock editing without changing it.
//
// The grid reads the pattern directly (its fields are atomics) and follows the
// playhead on a 30 Hz timer, so no parameter plumbing is needed here.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace opxy
{

class StepGridComponent : public juce::Component,
                          private juce::Timer
{
public:
    /** The grid edits either the 8 note tracks or the 6 sequenced punch-FX lanes. */
    enum class Mode { Tracks, FxLanes };

    explicit StepGridComponent (SeqQueenAudioProcessor& processor);
    ~StepGridComponent() override = default;

    void setMode (Mode newMode);
    Mode getMode() const noexcept { return mode; }

    void setVisibleSteps (int steps);
    int getVisibleSteps() const noexcept { return visibleSteps; }

    void setSelectedStep (int track, int step);
    int getSelectedTrack() const noexcept { return selectedTrack; }
    int getSelectedStep() const noexcept { return selectedStep; }

    /** Called when the user selects a step (right-click or a fresh click). */
    std::function<void (int track, int step)> onStepSelected;

    void paint (juce::Graphics& g) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    int rowCount() const noexcept { return mode == Mode::Tracks ? kMaxTracks : kMaxFx; }
    juce::Rectangle<int> rowBounds (int row) const;
    void paintTracks (juce::Graphics& g);
    void paintFxLanes (juce::Graphics& g);
    void toggleLaneStep (int fx, int step);
    void clearLane (int fx);
    juce::Rectangle<float> padBounds (int track, int step) const;
    int trackAt (juce::Point<int> position) const;
    int stepAt (juce::Point<int> position) const;
    void applyToStep (int track, int step, bool active);

    SeqQueenAudioProcessor& proc;
    int visibleSteps = 16;
    int selectedTrack = 0;
    int selectedStep = 0;
    Mode mode = Mode::Tracks;
    bool dragging = false;
    bool dragActiveState = true;
    int lastNotifiedTrack = -1;
    int lastNotifiedStep = -1;

    static constexpr int labelWidth = 84;
    static constexpr int headerHeight = 18;
};

} // namespace opxy
