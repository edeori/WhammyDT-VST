#include "WhirlDTDSP.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    juce::String safeFileName (juce::String name)
    {
        name = name.toLowerCase().replaceCharacter ('+', 'p').replaceCharacter ('-', 'm');
        return name.retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 _")
                   .replaceCharacter (' ', '_');
    }

    bool writeWave (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        file.deleteFile();
        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat format;
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            format.createWriterFor (stream.release(), sampleRate,
                                    static_cast<unsigned int> (audio.getNumChannels()),
                                    32, {}, 0));
        return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: WhirlDTGuitarRender <input.wav> <output-directory> [start-seconds] [duration-seconds]\n";
        return 2;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader> (
        formats.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr)
    {
        std::cerr << "Could not open input WAV\n";
        return 2;
    }

    const auto startSeconds = argc >= 4 ? juce::String (argv[3]).getDoubleValue() : 0.0;
    const auto durationSeconds = argc >= 5 ? juce::String (argv[4]).getDoubleValue()
                                           : static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
    const auto startSample = juce::jlimit<int64_t> (0, reader->lengthInSamples,
                                                     static_cast<int64_t> (std::llround (startSeconds * reader->sampleRate)));
    const auto sampleCount = static_cast<int> (juce::jlimit<int64_t> (
        0, reader->lengthInSamples - startSample,
        static_cast<int64_t> (std::llround (durationSeconds * reader->sampleRate))));
    const auto channelCount = juce::jmax (1, static_cast<int> (reader->numChannels));

    juce::AudioBuffer<float> input (channelCount, sampleCount);
    reader->read (&input, 0, sampleCount, startSample, true, true);

    const juce::File outputDirectory (argv[2]);
    if (! outputDirectory.createDirectory())
    {
        std::cerr << "Could not create output directory\n";
        return 2;
    }

    if (! writeWave (outputDirectory.getChildFile ("dry.wav"), input, reader->sampleRate))
        return 2;

    constexpr int blockSize = 256;
    bool printedLatency = false;
    for (int mode = 0; mode < 18; ++mode)
    {
        WhirlDTDSP::Processor processor;
        processor.prepare (reader->sampleRate, blockSize, channelCount);
        if (! printedLatency)
        {
            std::cout << "DSP latency: " << processor.getLatencySamples() << " samples ("
                      << (1000.0 * processor.getLatencySamples() / reader->sampleRate) << " ms)\n";
            printedLatency = true;
        }

        WhirlDTDSP::Parameters parameters;
        parameters.dropTuneMode = mode;
        parameters.dropTuneEnabled = true;

        juce::AudioBuffer<float> rendered;
        rendered.makeCopyOf (input);
        for (int offset = 0; offset < sampleCount; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, sampleCount - offset);
            juce::AudioBuffer<float> block (rendered.getArrayOfWritePointers(), channelCount, offset, count);
            processor.process (block, parameters);
        }

        const auto& model = WhirlDTDSP::getDropTuneModeModel (mode);
        const auto name = juce::String (mode).paddedLeft ('0', 2) + "_" + safeFileName (model.measurementKey) + ".wav";
        if (! writeWave (outputDirectory.getChildFile (name), rendered, reader->sampleRate))
            return 2;

        auto peak = 0.0f;
        double sumSquares = 0.0;
        double sumDifferences = 0.0;
        int64_t differenceCount = 0;
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto* data = rendered.getReadPointer (channel);
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                peak = juce::jmax (peak, std::abs (data[sample]));
                sumSquares += static_cast<double> (data[sample]) * data[sample];
                if (sample > 0)
                {
                    sumDifferences += std::abs (static_cast<double> (data[sample]) - data[sample - 1]);
                    ++differenceCount;
                }
            }
        }

        const auto rms = std::sqrt (sumSquares / static_cast<double> (sampleCount * channelCount));
        const auto meanDifference = sumDifferences / static_cast<double> (juce::jmax<int64_t> (1, differenceCount));
        std::cout << mode << " " << model.measurementKey
                  << " | RMS=" << juce::Decibels::gainToDecibels (static_cast<float> (rms))
                  << " dBFS peak=" << juce::Decibels::gainToDecibels (peak)
                  << " dBFS mean|dx|=" << meanDifference << '\n';
    }

    return 0;
}
