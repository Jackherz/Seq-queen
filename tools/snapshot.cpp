// seqqueen_snapshot: renders both devices' editors to PNGs without a host or a
// window, so UI changes can be checked from the command line (and on machines
// where a screenshot needs a permission the terminal doesn't have).
//
//   seqqueen_snapshot out-folder
//
// Writes fx.png (Seq Queen FX, bitcrush pad held) and notes.png (Seq Queen
// Notes, hats pad held), then prints rough processing and drawing costs.

#include "../Source/Devices/FxPlugin/FxProcessor.h"
#include "../Source/Devices/NotesPlugin/NotesProcessor.h"

#include <iostream>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;

/** Runs `seconds` of processing; the input is a 220 Hz sine so the FX has
    something to chew on. Returns the output peak. */
float play (juce::AudioProcessor& proc, double seconds)
{
    float peak = 0.0f;
    juce::AudioBuffer<float> buffer (juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels()), kBlock);
    juce::MidiBuffer midi;
    for (int done = 0; done < static_cast<int> (seconds * kSampleRate); done += kBlock)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample (ch, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                                          * static_cast<float> (done + i) / static_cast<float> (kSampleRate)));
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

void report (const char* name, juce::AudioProcessor& proc, juce::Component& editor)
{
    const auto started = juce::Time::getMillisecondCounterHiRes();
    const float peak = play (proc, 10.0);
    const double ms = juce::Time::getMillisecondCounterHiRes() - started;

    constexpr int frames = 60;
    const auto drawStarted = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < frames; ++i)
        editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
    const double drawMs = (juce::Time::getMillisecondCounterHiRes() - drawStarted) / frames;

    std::cout << name << ": processing " << juce::String (ms / 100.0, 2) << "% of one core, output peak "
              << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS, full-editor draw "
              << juce::String (drawMs, 2) << " ms\n";
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

    {
        SeqQueenFxProcessor fx;
        fx.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        fx.prepareToPlay (kSampleRate, kBlock);
        fx.setPadHeld (static_cast<int> (opxy::FxType::Bitcrush), true);
        std::unique_ptr<juce::AudioProcessorEditor> editor (fx.createEditor());
        editor->setVisible (true);
        settle (fx);
        snapshot (*editor, outDir.getChildFile ("fx.png"));
        report ("fx", fx, *editor);
    }

    {
        SeqQueenNotesProcessor notes;
        notes.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        notes.prepareToPlay (kSampleRate, kBlock);
        notes.setPadHeld (static_cast<int> (opxy::NoteFxType::Hats), true);
        std::unique_ptr<juce::AudioProcessorEditor> editor (notes.createEditor());
        editor->setVisible (true);
        settle (notes);
        snapshot (*editor, outDir.getChildFile ("notes.png"));
        report ("notes", notes, *editor);
    }
    return 0;
}
