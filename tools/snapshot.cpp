// seqqueen_snapshot: renders the editor to PNGs without a host or a window, so
// UI changes can be checked from the command line (and on machines where a
// screenshot needs a permission the terminal doesn't have).
//
//   seqqueen_snapshot out-folder
//
// Runs the starter groove for two seconds so the playhead and meters have
// something to show, then writes main.png (note tracks) and lanes.png (the
// punch-FX lanes view), then fx.png and notes.png for the two devices with a
// pad held in each. Also prints the output peak and rough processing and
// drawing costs of the instrument.

#include "../Source/Plugin/PluginProcessor.h"
#include "../Source/Devices/FxPlugin/FxProcessor.h"
#include "../Source/Devices/NotesPlugin/NotesProcessor.h"

#include <iostream>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;

float play (juce::AudioProcessor& proc, double seconds)
{
    float peak = 0.0f;
    juce::AudioBuffer<float> buffer (juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels()), kBlock);
    juce::MidiBuffer midi;
    for (int done = 0; done < static_cast<int> (seconds * kSampleRate); done += kBlock)
    {
        buffer.clear();
        midi.clear();
        proc.processBlock (buffer, midi);
        peak = juce::jmax (peak, buffer.getMagnitude (0, kBlock));
    }
    return peak;
}

void snapshot (juce::Component& editor, const juce::File& file)
{
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << file.getFullPathName() << "\n";
}

void settle (juce::AudioProcessor& proc)
{
    play (proc, 0.05);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: seqqueen_snapshot out-folder\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    outDir.createDirectory();

    SeqQueenAudioProcessor proc;
    proc.setRateAndBufferSizeDetails (kSampleRate, kBlock);
    proc.prepareToPlay (kSampleRate, kBlock);
    play (proc, 2.0);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setVisible (true);
    settle (proc);
    snapshot (*editor, outDir.getChildFile ("main.png"));

    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child);
            button != nullptr && button->getButtonText().containsIgnoreCase ("lanes"))
            button->triggerClick();
    settle (proc);
    snapshot (*editor, outDir.getChildFile ("lanes.png"));

    {
        const auto started = juce::Time::getMillisecondCounterHiRes();
        const float peak = play (proc, 10.0);
        const double ms = juce::Time::getMillisecondCounterHiRes() - started;
        std::cout << "processing: " << juce::String (ms / 100.0, 2) << "% of one core, output peak "
                  << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS\n";
    }

    constexpr int frames = 60;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < frames; ++i)
        editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    std::cout << "full-editor draw: "
              << juce::String ((juce::Time::getMillisecondCounterHiRes() - started) / frames, 2)
              << " ms per frame\n";

    editor = nullptr;
    proc.releaseResources();

    // ---- Seq Queen FX: bitcrush held ------------------------------------------
    {
        SeqQueenFxProcessor fx;
        fx.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        fx.prepareToPlay (kSampleRate, kBlock);
        fx.setPadHeld (static_cast<int> (opxy::FxType::Bitcrush), true);
        std::unique_ptr<juce::AudioProcessorEditor> fxEditor (fx.createEditor());
        fxEditor->setVisible (true);
        settle (fx);
        snapshot (*fxEditor, outDir.getChildFile ("fx.png"));
    }

    // ---- Seq Queen Notes: hats held --------------------------------------------
    {
        SeqQueenNotesProcessor notes;
        notes.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        notes.prepareToPlay (kSampleRate, kBlock);
        notes.setPadHeld (static_cast<int> (opxy::NoteFxType::Hats), true);
        std::unique_ptr<juce::AudioProcessorEditor> notesEditor (notes.createEditor());
        notesEditor->setVisible (true);
        settle (notes);
        snapshot (*notesEditor, outDir.getChildFile ("notes.png"));
    }
    return 0;
}
