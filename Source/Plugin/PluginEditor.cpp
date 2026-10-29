#include "PluginEditor.h"
#include "Theme.h"

namespace
{
// Panel geometry, in design pixels. The mockups in docs/ui are drawn from these
// same numbers, so the PNGs and the plugin agree.
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 54;
constexpr int kGap = 8;
constexpr int kGridHeight = 226;
constexpr int kPunchHeight = 104;
constexpr int kPLockHeight = 140;
constexpr int kMixerHeight = 8 * 22 + 52;
constexpr int kFooterHeight = 14;

constexpr int kEngineWidth = 84;
constexpr int kScaleWidth = 78;
} // namespace

SeqQueenAudioProcessorEditor::SeqQueenAudioProcessorEditor (SeqQueenAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      stepGrid (p),
      punchBar (p),
      pLockPanel (p),
      trackMixer (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1080, 760, 1900, 1200);
    setSize (1180, 810);

    // One LookAndFeel for the whole editor: thin line art everywhere.
    setLookAndFeel (&lookAndFeel);

    auto& state = proc.getAPVTS();

    addAndMakeVisible (stepGrid);
    addAndMakeVisible (punchBar);
    addAndMakeVisible (pLockPanel);
    addAndMakeVisible (trackMixer);

    stepGrid.onStepSelected = [this] (int track, int step)
    {
        pLockPanel.setSelectedStep (track, step);
        setStatusMessage ("Right-click a step to select it without toggling. "
                          "Arm a LOCK slot to write a p-lock on that step.");
    };

    // ---- header controls ---------------------------------------------------
    const auto addKnob = [this, &state] (juce::Slider& slider, const juce::String& id,
                                         double min, double max, double defaultValue)
    {
        opxy::ui::styleRotary (slider);
        slider.setRange (min, max, 0.01);
        slider.setDoubleClickReturnValue (true, defaultValue);
        addAndMakeVisible (slider);
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, id, slider));
    };

    addKnob (masterGain, "master_gain", -24.0, 6.0, -3.0);
    addKnob (swing, "swing", 0.0, 1.0, 0.0);
    addKnob (tempo, "tempo", 40.0, 240.0, 120.0);

    focusTrack.addItemList (opxy::params::trackChoices(), 1);
    opxy::ui::styleCombo (focusTrack);
    addAndMakeVisible (focusTrack);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, "focus_track", focusTrack));

    visibleSteps.addItem ("16 STEPS", 1);
    visibleSteps.addItem ("32 STEPS", 2);
    visibleSteps.addItem ("64 STEPS", 3);
    visibleSteps.setSelectedId (1);
    opxy::ui::styleCombo (visibleSteps);
    visibleSteps.onChange = [this]
    {
        const int steps = visibleSteps.getSelectedId() == 1 ? 16
                        : visibleSteps.getSelectedId() == 2 ? 32 : 64;
        stepGrid.setVisibleSteps (steps);
    };
    addAndMakeVisible (visibleSteps);

    // Grid mode toggle: 8 note tracks <-> 6 sequenced punch lanes.
    laneModeButton.setClickingTogglesState (true);
    opxy::ui::styleToggle (laneModeButton);
    laneModeButton.setColour (juce::TextButton::buttonOnColourId, opxy::ui::green);
    laneModeButton.onClick = [this]
    {
        const bool lanes = laneModeButton.getToggleState();
        stepGrid.setMode (lanes ? opxy::StepGridComponent::Mode::FxLanes
                                : opxy::StepGridComponent::Mode::Tracks);
        setStatusMessage (lanes
            ? "FX lanes: draw where each punch-in FX fires. Bar height is depth. "
              "Right-click a lane to clear it."
            : holdPadMessage());
    };
    addAndMakeVisible (laneModeButton);

    for (auto* button : { &midiOutButton, &fxNotesButton })
    {
        button->setClickingTogglesState (true);
        opxy::ui::styleToggle (*button);
        addAndMakeVisible (*button);
    }

    midiOutButton.setColour (juce::TextButton::buttonOnColourId, opxy::ui::amber);
    fxNotesButton.setColour (juce::TextButton::buttonOnColourId, opxy::ui::cyan);

    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "midi_out", midiOutButton));
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "fx_notes", fxNotesButton));

    status.setFont (opxy::ui::monoFont (8.0f));
    status.setColour (juce::Label::textColourId, opxy::ui::textFaint);
    status.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (status);

    setStatusMessage (holdPadMessage());
    pLockPanel.setSelectedStep (stepGrid.getSelectedTrack(), stepGrid.getSelectedStep());
}

SeqQueenAudioProcessorEditor::~SeqQueenAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::String SeqQueenAudioProcessorEditor::holdPadMessage() const
{
    const int base = proc.getFxNoteBase();
    return "Hold a pad, send MIDI notes " + juce::String (base) + "-"
         + juce::String (base + opxy::kMaxFx - 1)
         + " (soft hit = gentler punch), or draw the FX lanes.";
}

void SeqQueenAudioProcessorEditor::setStatusMessage (const juce::String& message)
{
    status.setText (message, juce::dontSendNotification);
}

// ------------------------------------------------------------------- paint ---

void SeqQueenAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (opxy::ui::voidColour);

    // ---- header ------------------------------------------------------------
    const auto header = juce::Rectangle<int> (kMargin, kMargin,
                                              getWidth() - 2 * kMargin, kHeaderHeight);
    opxy::ui::panelBox (g, header);

    // Wordmark: solid square with a notch, then tracked title.
    auto mark = header.reduced (12, 0).withSizeKeepingCentre (0, 0);
    const int markX = header.getX() + 12;
    const int markY = header.getY() + 15;
    g.setColour (opxy::ui::amber);
    g.fillRect (markX, markY, 13, 13);
    g.setColour (opxy::ui::voidColour);
    g.fillRect (markX + 6, markY, 7, 13);
    g.fillRect (markX + 6, markY + 5, 7, 3);
    juce::ignoreUnused (mark);

    g.setColour (opxy::ui::bone);
    g.setFont (opxy::ui::sansFont (19.0f, true).withExtraKerningFactor (0.16f));
    g.drawText ("SEQ QUEEN", header.getX() + 32, header.getY() + 10, 240, 22,
                juce::Justification::centredLeft, false);

    g.setColour (opxy::ui::textFaint);
    g.setFont (opxy::ui::monoFont (7.0f));
    g.drawText ("8-TRACK P-LOCK SEQUENCER / PUNCH-IN FX",
                header.getX() + 33, header.getY() + 34, 320, 10,
                juce::Justification::centredLeft, false);

    // Captions over each header control.
    const auto captionAbove = [&g] (juce::Component* component, const juce::String& text)
    {
        opxy::ui::caption (g, { component->getX(), component->getY() - 11,
                                component->getWidth(), 10 }, text);
    };

    captionAbove (&masterGain, "master");
    captionAbove (&swing, "swing");
    captionAbove (&tempo, "tempo");
    captionAbove (&focusTrack, "focus");
    captionAbove (&visibleSteps, "grid");

    // Transport readout, right aligned.
    const auto& sequencer = proc.getSequencer();
    const double stepPos = sequencer.getStepPosition();
    const int bar = sequencer.isRunning() ? static_cast<int> (stepPos / 16.0) + 1 : 0;
    const int beat = sequencer.isRunning()
                       ? static_cast<int> (std::fmod (stepPos / 4.0, 4.0)) + 1 : 0;

    juce::String position = bar > 0 ? "BAR " + juce::String (bar).paddedLeft ('0', 2)
                                        + "  ·  BEAT " + juce::String (beat)
                                    : juce::String ("STOPPED");
    opxy::ui::text (g, { header.getRight() - 220, header.getY() + 9, 206, 10 },
                    position, opxy::ui::monoFont (8.0f), opxy::ui::boneDim,
                    juce::Justification::centredRight);

    // Running LED.
    g.setColour (sequencer.isRunning() ? opxy::ui::amber : opxy::ui::textFaint);
    g.fillEllipse (static_cast<float> (header.getRight() - 232),
                   static_cast<float> (header.getY() + 12), 4.0f, 4.0f);

    // ---- footer strip ------------------------------------------------------
    const auto footer = juce::Rectangle<int> (kMargin, getHeight() - kMargin - kFooterHeight,
                                              getWidth() - 2 * kMargin, kFooterHeight);
    auto footerText = footer;
    opxy::ui::text (g, footerText.removeFromLeft (60), "SEQ QUEEN", opxy::ui::monoFont (7.0f, true),
                    opxy::ui::textFaint);

    juce::String serial;
    serial << "SQQN-" << juce::String (proc.getFxNoteBase()).paddedLeft ('0', 2)
           << " / " << (proc.getSampleRate() > 0 ? juce::String (proc.getSampleRate() / 1000.0, 1) + " KHZ"
                                                 : juce::String ("-- KHZ"))
           << " / " << juce::String (opxy::kMaxVoices) << " VOICES";

    opxy::ui::text (g, footerText, serial, opxy::ui::monoFont (7.0f), opxy::ui::textFaint,
                    juce::Justification::centredLeft);

    // Barcode: the poster's serial strip, used as a visual full stop.
    const int barsX = footer.getRight() - 74;
    const int pattern[] = { 2, 1, 3, 1, 1, 2, 4, 1, 1, 3, 1, 2, 1, 2, 3, 1, 1, 2 };
    int x = barsX;
    for (int i = 0; i < static_cast<int> (sizeof (pattern) / sizeof (pattern[0])) && x < footer.getRight(); ++i)
    {
        g.setColour ((i % 3 == 0) ? opxy::ui::boneDim : opxy::ui::textFaint);
        g.fillRect (x, footer.getY() + 1, pattern[i], 9);
        x += pattern[i] + 2;
    }
}

// ----------------------------------------------------------------- resized ---

void SeqQueenAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin);

    // ---- header ------------------------------------------------------------
    auto header = area.removeFromTop (kHeaderHeight);
    header.removeFromLeft (360);

    const int knobWidth = 58;
    const int knobTop = header.getY() + 14;

    masterGain.setBounds (header.removeFromLeft (knobWidth).withY (knobTop).withHeight (30));
    swing.setBounds (header.removeFromLeft (knobWidth).withY (knobTop).withHeight (30));
    tempo.setBounds (header.removeFromLeft (knobWidth).withY (knobTop).withHeight (30));

    header.removeFromLeft (8);
    focusTrack.setBounds (header.removeFromLeft (96).withY (header.getY() + 17).withHeight (20));
    header.removeFromLeft (8);
    visibleSteps.setBounds (header.removeFromLeft (84).withY (header.getY() + 17).withHeight (20));

    header.removeFromLeft (10);
    laneModeButton.setBounds (header.removeFromLeft (78).withY (header.getY() + 17).withHeight (20));
    header.removeFromLeft (6);
    midiOutButton.setBounds (header.removeFromLeft (76).withY (header.getY() + 17).withHeight (20));
    header.removeFromLeft (6);
    fxNotesButton.setBounds (header.removeFromLeft (78).withY (header.getY() + 17).withHeight (20));

    // Status line lives under the toggles, right aligned.
    status.setBounds (header.getX() + 8, header.getY() + 36, juce::jmax (0, header.getWidth() - 240), 12);

    area.removeFromTop (kGap);

    // ---- fixed panels from the bottom up ----------------------------------
    area.removeFromBottom (kFooterHeight + 2);

    trackMixer.setBounds (area.removeFromBottom (kMixerHeight));
    area.removeFromBottom (kGap);
    pLockPanel.setBounds (area.removeFromBottom (kPLockHeight));
    area.removeFromBottom (kGap);
    punchBar.setBounds (area.removeFromBottom (kPunchHeight));
    area.removeFromBottom (kGap);

    // ---- the grid takes the rest ------------------------------------------
    stepGrid.setBounds (area);

    // The grid paints its own panel chrome and section header.
    juce::ignoreUnused (kGridHeight, kEngineWidth, kScaleWidth);
}
