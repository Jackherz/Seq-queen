#include "PluginEditor.h"
#include "Theme.h"

namespace
{
constexpr int kHeaderHeight = 54;
constexpr int kGap = 8;
constexpr int kPunchHeight = 84;
constexpr int kPLockHeight = 140;
constexpr int kMixerHeight = 8 * 28 + 24;
}

SeqQueenAudioProcessorEditor::SeqQueenAudioProcessorEditor (SeqQueenAudioProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc (processor),
      stepGrid (processor),
      punchBar (processor),
      pLockPanel (processor),
      trackMixer (processor)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1020, 740, 1900, 1200);
    setSize (1180, 810);

    auto& state = proc.getAPVTS();

    addAndMakeVisible (stepGrid);
    addAndMakeVisible (punchBar);
    addAndMakeVisible (pLockPanel);
    addAndMakeVisible (trackMixer);

    stepGrid.onStepSelected = [this] (int track, int step)
    {
        pLockPanel.setSelectedStep (track, step);
        setStatusMessage ("Right-click selects a step without toggling it. "
                          "Lock a parameter in the row below to make a p-lock.");
    };

    // ---- header controls ---------------------------------------------------
    theme::styleRotary (masterGain);
    masterGain.setRange (-24.0, 6.0, 0.1);
    masterGain.setDoubleClickReturnValue (true, -3.0);
    addAndMakeVisible (masterGain);
    masterGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "master_gain", masterGain);

    theme::styleRotary (swing);
    swing.setRange (0.0, 1.0, 0.001);
    swing.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (swing);
    swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "swing", swing);

    theme::styleRotary (tempo);
    tempo.setRange (40.0, 240.0, 0.1);
    tempo.setDoubleClickReturnValue (true, 120.0);
    addAndMakeVisible (tempo);
    tempoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "tempo", tempo);

    focusTrack.addItemList (opxy::params::trackChoices(), 1);
    theme::styleCombo (focusTrack);
    addAndMakeVisible (focusTrack);
    focusTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, "focus_track", focusTrack);

    visibleSteps.addItem ("16 steps", 1);
    visibleSteps.addItem ("32 steps", 2);
    visibleSteps.addItem ("64 steps", 3);
    visibleSteps.setSelectedId (1);
    theme::styleCombo (visibleSteps);
    visibleSteps.onChange = [this]
    {
        const int steps = visibleSteps.getSelectedId() == 1 ? 16
                        : visibleSteps.getSelectedId() == 2 ? 32 : 64;
        stepGrid.setVisibleSteps (steps);
    };
    addAndMakeVisible (visibleSteps);

    for (auto* button : { &midiOutButton, &fxNotesButton })
    {
        button->setClickingTogglesState (true);
        theme::styleToggle (*button);
        addAndMakeVisible (*button);
    }

    midiOutAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "midi_out", midiOutButton);
    fxNotesAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "fx_notes", fxNotesButton);
    fxNotesButton.setColour (juce::TextButton::buttonOnColourId, opxy::theme::accentAlt);

    // Swaps the grid between the 8 note tracks and the 6 sequenced punch lanes.
    laneModeButton.setClickingTogglesState (true);
    opxy::theme::styleToggle (laneModeButton);
    laneModeButton.setColour (juce::TextButton::buttonOnColourId, opxy::theme::accentAlt);
    laneModeButton.onClick = [this]
    {
        const bool lanes = laneModeButton.getToggleState();
        stepGrid.setMode (lanes ? opxy::StepGridComponent::Mode::FxLanes
                                : opxy::StepGridComponent::Mode::Tracks);
        setStatusMessage (lanes
            ? "FX lanes: draw where each punch-in FX fires. Right-click a lane to clear it."
            : "Hold a punch pad, or send MIDI notes "
                + juce::String (proc.getFxNoteBase()) + "-"
                + juce::String (proc.getFxNoteBase() + opxy::kMaxFx - 1)
                + " to punch the six FX.");
    };
    addAndMakeVisible (laneModeButton);

    status.setFont (opxy::theme::monoFont (10.0f));
    status.setColour (juce::Label::textColourId, opxy::theme::textDim);
    status.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (status);

    setStatusMessage ("Hold a punch pad, or send MIDI notes "
                      + juce::String (proc.getFxNoteBase()) + "-"
                      + juce::String (proc.getFxNoteBase() + opxy::kMaxFx - 1)
                      + " to punch the six FX.");

    pLockPanel.setSelectedStep (stepGrid.getSelectedTrack(), stepGrid.getSelectedStep());
}

void SeqQueenAudioProcessorEditor::setStatusMessage (const juce::String& message)
{
    status.setText (message, juce::dontSendNotification);
}

void SeqQueenAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (opxy::theme::background);

    auto header = getLocalBounds().reduced (12, 0).removeFromTop (kHeaderHeight);

    g.setColour (opxy::theme::text);
    g.setFont (opxy::theme::bigFont (20.0f));
    g.drawText ("SEQ QUEEN", header.removeFromLeft (150), juce::Justification::centredLeft, false);

    g.setColour (opxy::theme::textDim);
    g.setFont (opxy::theme::monoFont (10.0f));
    g.drawText ("8-track p-lock sequencer  /  punch-in FX",
                header.removeFromLeft (240), juce::Justification::centredLeft, false);

    // Captions under the header knobs.
    const auto drawKnobCaption = [&g] (juce::Rectangle<int> area, const juce::String& text)
    {
        g.setColour (opxy::theme::textDim);
        g.setFont (opxy::theme::labelFont (9.0f));
        g.drawText (text, area.removeFromBottom (11), juce::Justification::centred, false);
    };

    auto controls = getLocalBounds().reduced (12, 0).removeFromTop (kHeaderHeight);
    controls.removeFromLeft (400);
    controls.removeFromTop (16);

    const int knobWidth = 58;

    drawKnobCaption (controls.removeFromLeft (knobWidth), "MASTER");
    drawKnobCaption (controls.removeFromLeft (knobWidth), "SWING");
    drawKnobCaption (controls.removeFromLeft (knobWidth), "TEMPO");

    controls.removeFromLeft (6);
    g.setColour (opxy::theme::textDim);
    g.setFont (opxy::theme::labelFont (9.0f));
    g.drawText ("FOCUS", controls.removeFromLeft (96).removeFromBottom (11),
                juce::Justification::centred, false);
}

void SeqQueenAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // ---- header ------------------------------------------------------------
    auto header = area.removeFromTop (kHeaderHeight);
    header.removeFromLeft (400);
    header.removeFromTop (16);

    const int knobWidth = 58;
    const int knobHeight = kHeaderHeight - 22;

    masterGain.setBounds (header.removeFromLeft (knobWidth).withHeight (knobHeight));
    swing.setBounds (header.removeFromLeft (knobWidth).withHeight (knobHeight));
    tempo.setBounds (header.removeFromLeft (knobWidth).withHeight (knobHeight));

    header.removeFromLeft (6);
    focusTrack.setBounds (header.removeFromLeft (96).withHeight (22).withY (header.getY() + 6));

    header.removeFromLeft (10);
    visibleSteps.setBounds (header.removeFromLeft (92).withHeight (22).withY (header.getY() + 6));

    header.removeFromLeft (10);
    laneModeButton.setBounds (header.removeFromLeft (86).withHeight (22).withY (header.getY() + 6));
    header.removeFromLeft (6);
    midiOutButton.setBounds (header.removeFromLeft (82).withHeight (22).withY (header.getY() + 6));
    header.removeFromLeft (6);
    fxNotesButton.setBounds (header.removeFromLeft (82).withHeight (22).withY (header.getY() + 6));

    status.setBounds (header.removeFromLeft (juce::jmax (0, header.getWidth())).withHeight (22)
                            .withY (header.getY() + 6));

    area.removeFromTop (kGap);

    // ---- fixed-height sections from the bottom up -------------------------
    trackMixer.setBounds (area.removeFromBottom (kMixerHeight));
    area.removeFromBottom (kGap);
    pLockPanel.setBounds (area.removeFromBottom (kPLockHeight));
    area.removeFromBottom (kGap);
    punchBar.setBounds (area.removeFromBottom (kPunchHeight));
    area.removeFromBottom (kGap);

    // ---- the grid takes whatever is left ----------------------------------
    stepGrid.setBounds (area);
}
