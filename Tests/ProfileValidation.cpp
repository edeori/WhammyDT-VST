#include "WhirlDTDSP.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

namespace
{
    enum class Stage { bypass, dropTune, whirl };

    struct TakeSpec
    {
        const char* name;
        Stage stage;
        int mode;
        float pedal;
    };

    // Recording order in the July 2026 profiling WAVs.  The final 2-octave-up
    // take exists in the raw pair even though it was not exported into the
    // supplied measured.json, so it is validated directly from the WAVs too.
    constexpr std::array<TakeSpec, 40> takes {{
        { "bypass",                 Stage::bypass,   0, 0.0f },
        { "shift up 1",             Stage::dropTune, 8, 0.0f },
        { "shift up 2",             Stage::dropTune, 7, 0.0f },
        { "shift up 3",             Stage::dropTune, 6, 0.0f },
        { "shift up 4",             Stage::dropTune, 5, 0.0f },
        { "shift up 5",             Stage::dropTune, 4, 0.0f },
        { "shift up 6",             Stage::dropTune, 3, 0.0f },
        { "shift up 7",             Stage::dropTune, 2, 0.0f },
        { "shift up oct",           Stage::dropTune, 1, 0.0f },
        { "shift up oct + dry",     Stage::dropTune, 0, 0.0f },
        { "shift down 1",           Stage::dropTune, 9, 0.0f },
        { "shift down 2",           Stage::dropTune, 10, 0.0f },
        { "shift down 3",           Stage::dropTune, 11, 0.0f },
        { "shift down 4",           Stage::dropTune, 12, 0.0f },
        { "shift down 5",           Stage::dropTune, 13, 0.0f },
        { "shift down 6",           Stage::dropTune, 14, 0.0f },
        { "shift down 7",           Stage::dropTune, 15, 0.0f },
        { "shift down oct",         Stage::dropTune, 16, 0.0f },
        { "shift down oct + dry",   Stage::dropTune, 17, 0.0f },
        { "shallow",                Stage::whirl,    19, 0.0f },
        { "up 2nd up 3rd",          Stage::whirl,    8, 0.0f },
        { "up3b rd up 3rd",         Stage::whirl,    7, 0.0f },
        { "up 3rd up 4th",          Stage::whirl,    6, 0.0f },
        { "up 4th up 5th",          Stage::whirl,    5, 0.0f },
        { "up 5th up 6th",          Stage::whirl,    4, 0.0f },
        { "up 5th up 7th",          Stage::whirl,    3, 0.0f },
        { "down 4th down 3rd",      Stage::whirl,    2, 0.0f },
        { "down 5th down 4th",      Stage::whirl,    1, 0.0f },
        { "down oct up oct",        Stage::whirl,    0, 0.0f },
        { "deep",                   Stage::whirl,    20, 0.0f },
        { "dive bomb",              Stage::whirl,    18, 0.0f },
        { "down 2 oct",             Stage::whirl,    17, 0.0f },
        { "down oct",               Stage::whirl,    16, 0.0f },
        { "down 5th",               Stage::whirl,    15, 0.0f },
        { "down 4th",               Stage::whirl,    14, 0.0f },
        { "down 2nd",               Stage::whirl,    13, 0.0f },
        { "up 4nd",                 Stage::whirl,    12, 0.0f },
        { "up 5th",                 Stage::whirl,    11, 0.0f },
        { "up oct 2 oct",           Stage::whirl,    10, 0.0f },
        { "up 2 oct (raw take)",    Stage::whirl,    9, 0.0f }
    }};

    struct SectionRange
    {
        int64_t start = 0;
        int count = 0;
        double dryPitchHz = 0.0;
        juce::String descriptorLabel;
    };

    struct ValidationResult
    {
        juce::String name;
        double hardwareGainDb = 0.0;
        double pluginGainDb = 0.0;
        double gainErrorDb = 0.0;
        double hardwarePitchSemitones = 0.0;
        double pluginPitchSemitones = 0.0;
        double pitchErrorSemitones = 0.0;
        bool hasPitchResult = false;
        double hardwareInharmonicity = 0.0;
        double pluginInharmonicity = 0.0;
        double inharmonicityErrorRatio = 0.0;
        bool hasInharmonicityResult = false;
        bool exportedMeasurement = false;
    };

    std::unique_ptr<juce::AudioFormatReader> makeReader (juce::AudioFormatManager& formats,
                                                         const juce::File& file)
    {
        return std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));
    }

    std::vector<int64_t> detectProfileStarts (juce::AudioFormatReader& reader)
    {
        const auto sampleRate = reader.sampleRate;
        const auto hop = juce::jmax (1, juce::roundToInt (sampleRate * 0.01));
        const auto leadingSilenceSamples = juce::roundToInt (sampleRate * 0.35);
        const auto profileLockoutSamples = static_cast<int64_t> (sampleRate * 49.8);
        const auto channelCount = juce::jmax (1, static_cast<int> (reader.numChannels));

        juce::AudioBuffer<float> block (channelCount, hop);
        std::vector<int64_t> starts;
        int64_t lockoutUntil = 0;

        for (int64_t position = 0; position < reader.lengthInSamples; position += hop)
        {
            const auto count = static_cast<int> (juce::jmin<int64_t> (hop, reader.lengthInSamples - position));
            block.clear();
            reader.read (&block, 0, count, position, true, true);

            double sumSquares = 0.0;
            for (int channel = 0; channel < channelCount; ++channel)
            {
                const auto* data = block.getReadPointer (channel);
                for (int sample = 0; sample < count; ++sample)
                    sumSquares += static_cast<double> (data[sample]) * data[sample];
            }
            const auto rms = std::sqrt (sumSquares / static_cast<double> (count * channelCount));

            // The rendered recording does not contain the JSON profile's sync
            // sweep.  Its first unmistakable event is the -24 dB sine after
            // 350 ms of leading silence.  Once found, ignore the complete
            // 49.8 s profile before looking for the next take; this is more
            // robust than requiring perfectly uninterrupted digital silence
            // in the variable-length gaps between hardware recordings.
            if (position >= lockoutUntil && rms > 0.005)
            {
                const auto profileStart = juce::jmax<int64_t> (0, position - leadingSilenceSamples);
                starts.push_back (profileStart);
                lockoutUntil = profileStart + profileLockoutSamples;
            }
        }

        return starts;
    }

    juce::AudioBuffer<float> readTake (juce::AudioFormatReader& reader, int64_t start, int sampleCount)
    {
        const auto channels = juce::jmax (1, static_cast<int> (reader.numChannels));
        juce::AudioBuffer<float> result (channels, sampleCount);
        result.clear();
        reader.read (&result, 0, sampleCount, start, true, true);
        return result;
    }

    double median (std::vector<double> values)
    {
        if (values.empty())
            return 0.0;

        std::sort (values.begin(), values.end());
        const auto middle = values.size() / 2;
        return values.size() % 2 == 0 ? 0.5 * (values[middle - 1] + values[middle]) : values[middle];
    }

    double rmsDb (const juce::AudioBuffer<float>& buffer, SectionRange range)
    {
        range.start = juce::jlimit<int64_t> (0, buffer.getNumSamples(), range.start);
        range.count = juce::jlimit (0, buffer.getNumSamples() - static_cast<int> (range.start), range.count);
        if (range.count == 0)
            return -160.0;

        double sumSquares = 0.0;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel, static_cast<int> (range.start));
            for (int sample = 0; sample < range.count; ++sample)
                sumSquares += static_cast<double> (data[sample]) * data[sample];
        }

        const auto meanSquare = sumSquares / static_cast<double> (range.count * buffer.getNumChannels());
        return juce::Decibels::gainToDecibels (std::sqrt (meanSquare), -160.0);
    }

    double targetSemitones (const TakeSpec& spec)
    {
        if (spec.stage == Stage::dropTune)
            return WhirlDTDSP::getDropTuneModeModel (spec.mode).toeSemitones;
        if (spec.stage == Stage::whirl)
        {
            const auto& model = WhirlDTDSP::getWhirlModeModel (spec.mode);
            return model.toeSemitones + spec.pedal * (model.heelSemitones - model.toeSemitones);
        }
        return 0.0;
    }

    double estimateFrequencyNear (const juce::AudioBuffer<float>& buffer, SectionRange range,
                                  double sampleRate, double expectedFrequency)
    {
        constexpr int fftOrder = 16;
        constexpr int fftSize = 1 << fftOrder;
        constexpr int analysisSamples = 16384;

        // Below 20 Hz the 550 ms section contains too few cycles for a useful
        // pitch comparison (notably Dive Bomb); report its level but do not
        // turn an underdetermined FFT peak into a misleading pitch error.
        if (expectedFrequency < 20.0 || expectedFrequency > sampleRate * 0.47)
            return 0.0;

        const auto available = juce::jmin (analysisSamples, range.count);
        const auto start = static_cast<int> (range.start) + juce::jmax (0, (range.count - available) / 2);
        std::vector<float> spectrum (static_cast<size_t> (fftSize * 2), 0.0f);
        const auto* input = buffer.getReadPointer (0, juce::jlimit (0, buffer.getNumSamples() - available, start));
        for (int i = 0; i < available; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                       * static_cast<float> (i) / static_cast<float> (available - 1));
            spectrum[static_cast<size_t> (i)] = input[i] * window;
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (spectrum.data());

        const auto binHz = sampleRate / static_cast<double> (fftSize);
        const auto lowBin = juce::jlimit (1, fftSize / 2 - 2,
                                         static_cast<int> (std::floor (expectedFrequency * 0.94 / binHz)));
        const auto highBin = juce::jlimit (lowBin + 1, fftSize / 2 - 2,
                                          static_cast<int> (std::ceil (expectedFrequency * 1.06 / binHz)));
        auto peakBin = lowBin;
        for (int bin = lowBin + 1; bin <= highBin; ++bin)
            if (spectrum[static_cast<size_t> (bin)] > spectrum[static_cast<size_t> (peakBin)])
                peakBin = bin;

        const auto left = std::log (spectrum[static_cast<size_t> (peakBin - 1)] + 1.0e-12f);
        const auto centre = std::log (spectrum[static_cast<size_t> (peakBin)] + 1.0e-12f);
        const auto right = std::log (spectrum[static_cast<size_t> (peakBin + 1)] + 1.0e-12f);
        const auto denominator = left - 2.0 * centre + right;
        const auto offset = std::abs (denominator) > 1.0e-12 ? 0.5 * (left - right) / denominator : 0.0;
        return (static_cast<double> (peakBin) + juce::jlimit (-0.5, 0.5, offset)) * binHz;
    }

    // Splits the spectral energy of a section into "at the expected harmonic
    // series" vs "everywhere else" and returns the latter as a fraction of
    // the total - i.e. how inharmonic/noisy the signal is relative to a
    // clean shifted tone. This exists specifically because pitch/gain error
    // (the only two things this tool measured before) both already passed
    // near-perfectly on the granular engine in the sections the user
    // reported as "digitally glitching" - average pitch accuracy doesn't
    // capture glitchiness at all. The target here isn't a perfectly clean
    // (zero) ratio: the user explicitly wants the plugin to track the real
    // hardware's own measured, slightly-glitchy character rather than
    // eliminate it outright, so this is compared against the hardware's own
    // ratio on the same section, not against zero.
    double estimateInharmonicityRatio (const juce::AudioBuffer<float>& buffer, SectionRange range,
                                       double sampleRate, double fundamentalFrequency)
    {
        constexpr int fftOrder = 16;
        constexpr int fftSize = 1 << fftOrder;
        constexpr int analysisSamples = 16384;

        if (fundamentalFrequency < 20.0 || fundamentalFrequency > sampleRate * 0.47)
            return 0.0;

        const auto available = juce::jmin (analysisSamples, range.count);
        const auto start = static_cast<int> (range.start) + juce::jmax (0, (range.count - available) / 2);
        std::vector<float> spectrum (static_cast<size_t> (fftSize * 2), 0.0f);
        const auto* input = buffer.getReadPointer (0, juce::jlimit (0, buffer.getNumSamples() - available, start));
        for (int i = 0; i < available; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                       * static_cast<float> (i) / static_cast<float> (available - 1));
            spectrum[static_cast<size_t> (i)] = input[i] * window;
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (spectrum.data());

        const auto binHz = sampleRate / static_cast<double> (fftSize);
        const auto numUsableBins = fftSize / 2 - 2;

        double harmonicEnergy = 0.0;
        double totalEnergy = 0.0;
        for (int bin = 1; bin <= numUsableBins; ++bin)
        {
            const auto magnitude = static_cast<double> (spectrum[static_cast<size_t> (bin)]);
            const auto energy = magnitude * magnitude;
            totalEnergy += energy;

            const auto frequency = bin * binHz;
            const auto harmonicNumber = std::round (frequency / fundamentalFrequency);
            if (harmonicNumber >= 1.0)
            {
                const auto nearestHarmonic = harmonicNumber * fundamentalFrequency;
                // Tolerance scales with the harmonic's own frequency (a
                // slightly mistuned/vibrato'd real note still lands near its
                // own harmonic series proportionally, not within a fixed bin
                // count), floored at 1.5 bins so low harmonics aren't
                // unreasonably narrow.
                const auto tolerance = juce::jmax (binHz * 1.5, nearestHarmonic * 0.02);
                if (std::abs (frequency - nearestHarmonic) <= tolerance)
                    harmonicEnergy += energy;
            }
        }

        if (totalEnergy <= 1.0e-18)
            return 0.0;

        return (totalEnergy - harmonicEnergy) / totalEnergy;
    }

    std::vector<SectionRange> getMeasurementSections (const juce::var& measurements, const juce::String& name)
    {
        std::vector<SectionRange> result;
        if (auto* measurementsObject = measurements.getDynamicObject())
        {
            const auto take = measurementsObject->getProperty (juce::Identifier (name));
            if (auto* takeObject = take.getDynamicObject())
                if (auto* sections = takeObject->getProperty ("sections").getArray())
                    for (const auto& section : *sections)
                        if (auto* object = section.getDynamicObject())
                            result.push_back ({ static_cast<int64_t> (object->getProperty ("startSample")),
                                                static_cast<int> (object->getProperty ("sampleCount")),
                                                static_cast<double> (object->getProperty ("dryPitchHz")),
                                                object->getProperty ("descriptorLabel").toString() });
        }
        return result;
    }

    void processTake (juce::AudioBuffer<float>& buffer, const TakeSpec& spec, double sampleRate)
    {
        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, 256, buffer.getNumChannels());

        WhirlDTDSP::Parameters parameters;
        if (spec.stage == Stage::dropTune)
        {
            parameters.dropTuneMode = spec.mode;
            parameters.dropTuneEnabled = true;
        }
        else if (spec.stage == Stage::whirl)
        {
            parameters.whirlMode = spec.mode;
            parameters.whirlEnabled = true;
            parameters.pedalPosition = spec.pedal;
        }

        for (int offset = 0; offset < buffer.getNumSamples(); offset += 256)
        {
            const auto count = juce::jmin (256, buffer.getNumSamples() - offset);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), offset, count);
            processor.process (block, parameters);
        }
    }
}

int main (int argc, char** argv)
{
    if (argc < 5)
    {
        std::cerr << "Usage: WhirlDTProfileValidation <dry.wav> <wet.wav> <measured.json> <report.json>\n";
        return 2;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto dryReader = makeReader (formats, juce::File (argv[1]));
    auto wetReader = makeReader (formats, juce::File (argv[2]));
    if (dryReader == nullptr || wetReader == nullptr)
    {
        std::cerr << "Could not open profiling WAV files\n";
        return 2;
    }
    if (std::abs (dryReader->sampleRate - wetReader->sampleRate) > 0.1)
    {
        std::cerr << "Dry/wet sample rates differ\n";
        return 2;
    }

    const auto measuredText = juce::File (argv[3]).loadFileAsString();
    const auto measuredRoot = juce::JSON::parse (measuredText);
    const auto measurements = measuredRoot.getProperty ("measurements", {});

    const auto dryStarts = detectProfileStarts (*dryReader);
    const auto wetStarts = detectProfileStarts (*wetReader);
    std::cout << "Detected " << dryStarts.size() << " dry and " << wetStarts.size() << " wet takes\n";
    if (dryStarts.size() < takes.size() || wetStarts.size() < takes.size())
    {
        std::cerr << "Not enough complete take markers for the 40 known recordings\n";
        return 2;
    }

    const auto sampleRate = dryReader->sampleRate;
    const auto takeSamples = juce::roundToInt (sampleRate * 49.78);
    std::vector<ValidationResult> results;
    juce::Array<juce::var> jsonResults;
    std::vector<double> phraseStudyPitchErrors;

    for (size_t takeIndex = 0; takeIndex < takes.size(); ++takeIndex)
    {
        const auto& spec = takes[takeIndex];
        auto dry = readTake (*dryReader, dryStarts[takeIndex], takeSamples);
        auto wet = readTake (*wetReader, wetStarts[takeIndex], takeSamples);
        auto rendered = dry;
        processTake (rendered, spec, sampleRate);

        auto sections = getMeasurementSections (measurements, spec.name);
        const auto hasExportedMeasurement = ! sections.empty();
        if (sections.empty())
        {
            // Same profile layout as every exported take.  Copy its ranges for
            // the raw-only final 2-octave-up recording.
            sections = getMeasurementSections (measurements, "up oct 2 oct");
        }

        const auto sectionSemitones = spec.stage != Stage::bypass ? targetSemitones (spec) : 0.0;
        const auto sectionExpectedOutput = 82.41 * std::pow (2.0, sectionSemitones / 12.0);

        std::vector<double> hardwareGains, pluginGains;
        std::vector<double> hardwareInharmonicities, pluginInharmonicities;
        for (int sectionIndex = 19; sectionIndex <= 23 && sectionIndex < static_cast<int> (sections.size()); ++sectionIndex)
        {
            const auto& section = sections[static_cast<size_t> (sectionIndex)];
            const auto dryLevel = rmsDb (dry, section);
            hardwareGains.push_back (rmsDb (wet, section) - dryLevel);
            pluginGains.push_back (rmsDb (rendered, section) - dryLevel);

            if (spec.stage != Stage::bypass)
            {
                hardwareInharmonicities.push_back (estimateInharmonicityRatio (wet, section, sampleRate, sectionExpectedOutput));
                pluginInharmonicities.push_back (estimateInharmonicityRatio (rendered, section, sampleRate, sectionExpectedOutput));
            }
        }

        ValidationResult result;
        result.name = spec.name;
        result.hardwareGainDb = median (hardwareGains);
        result.pluginGainDb = median (pluginGains);
        result.gainErrorDb = result.pluginGainDb - result.hardwareGainDb;
        result.exportedMeasurement = hasExportedMeasurement;

        if (! hardwareInharmonicities.empty())
        {
            result.hardwareInharmonicity = median (hardwareInharmonicities);
            result.pluginInharmonicity = median (pluginInharmonicities);
            result.inharmonicityErrorRatio = result.pluginInharmonicity - result.hardwareInharmonicity;
            result.hasInharmonicityResult = true;
        }

        if (spec.stage != Stage::bypass && spec.mode != 19 && spec.mode != 20 && ! sections.empty())
        {
            const auto semitones = targetSemitones (spec);
            const auto expectedInput = 82.41;
            const auto expectedOutput = expectedInput * std::pow (2.0, semitones / 12.0);
            const auto dryFrequency = estimateFrequencyNear (dry, sections[0], sampleRate, expectedInput);
            const auto hardwareFrequency = estimateFrequencyNear (wet, sections[0], sampleRate, expectedOutput);
            const auto pluginFrequency = estimateFrequencyNear (rendered, sections[0], sampleRate, expectedOutput);
            if (dryFrequency > 0.0 && hardwareFrequency > 0.0 && pluginFrequency > 0.0)
            {
                result.hardwarePitchSemitones = 12.0 * std::log2 (hardwareFrequency / dryFrequency);
                result.pluginPitchSemitones = 12.0 * std::log2 (pluginFrequency / dryFrequency);
                result.pitchErrorSemitones = result.pluginPitchSemitones - result.hardwarePitchSemitones;
                result.hasPitchResult = true;
            }

            // The single sustained tone checked above never exercises how the
            // grain-period pitch tracker keeps up with an actually changing
            // note - a static hold is the easiest possible case for it. The
            // profiling recordings also contain a short played phrase (a
            // handful of distinct, rapidly-successive notes, ~0.3s each) -
            // check every one of those individually instead of just the
            // opening held tone.
            for (const auto& section : sections)
            {
                if (! section.descriptorLabel.containsIgnoreCase ("Phrase") || section.dryPitchHz <= 0.0)
                    continue;

                const auto phraseExpectedOutput = section.dryPitchHz * std::pow (2.0, semitones / 12.0);
                const auto phraseDry = estimateFrequencyNear (dry, section, sampleRate, section.dryPitchHz);
                const auto phraseHardware = estimateFrequencyNear (wet, section, sampleRate, phraseExpectedOutput);
                const auto phrasePlugin = estimateFrequencyNear (rendered, section, sampleRate, phraseExpectedOutput);
                if (phraseDry <= 0.0 || phraseHardware <= 0.0 || phrasePlugin <= 0.0)
                    continue;

                const auto hardwareSt = 12.0 * std::log2 (phraseHardware / phraseDry);
                const auto pluginSt = 12.0 * std::log2 (phrasePlugin / phraseDry);
                const auto errorSt = pluginSt - hardwareSt;
                phraseStudyPitchErrors.push_back (std::abs (errorSt));
                std::cout << "  phrase " << section.descriptorLabel << " dry=" << juce::String (section.dryPitchHz, 1)
                          << "Hz hardware=" << juce::String (hardwareSt, 2) << "st plugin=" << juce::String (pluginSt, 2)
                          << "st error=" << juce::String (errorSt, 2) << "st\n";
            }
        }

        std::cout << result.name << ": gain hardware=" << juce::String (result.hardwareGainDb, 2)
                  << " dB plugin=" << juce::String (result.pluginGainDb, 2)
                  << " dB error=" << juce::String (result.gainErrorDb, 2) << " dB";
        if (result.hasPitchResult)
            std::cout << " | pitch hardware=" << juce::String (result.hardwarePitchSemitones, 2)
                      << " st plugin=" << juce::String (result.pluginPitchSemitones, 2)
                      << " st error=" << juce::String (result.pitchErrorSemitones, 2) << " st";
        if (result.hasInharmonicityResult)
            std::cout << " | inharmonicity hardware=" << juce::String (result.hardwareInharmonicity, 4)
                      << " plugin=" << juce::String (result.pluginInharmonicity, 4)
                      << " error=" << juce::String (result.inharmonicityErrorRatio, 4);
        std::cout << '\n';

        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty ("name", result.name);
        object->setProperty ("exportedMeasurement", result.exportedMeasurement);
        object->setProperty ("hardwareGainDb", result.hardwareGainDb);
        object->setProperty ("pluginGainDb", result.pluginGainDb);
        object->setProperty ("gainErrorDb", result.gainErrorDb);
        if (result.hasPitchResult)
        {
            object->setProperty ("hardwarePitchSemitones", result.hardwarePitchSemitones);
            object->setProperty ("pluginPitchSemitones", result.pluginPitchSemitones);
            object->setProperty ("pitchErrorSemitones", result.pitchErrorSemitones);
        }
        if (result.hasInharmonicityResult)
        {
            object->setProperty ("hardwareInharmonicity", result.hardwareInharmonicity);
            object->setProperty ("pluginInharmonicity", result.pluginInharmonicity);
            object->setProperty ("inharmonicityErrorRatio", result.inharmonicityErrorRatio);
        }
        jsonResults.add (juce::var (object.release()));
        results.push_back (result);
    }

    std::vector<double> absoluteGainErrors, absolutePitchErrors, inharmonicityErrors, absoluteInharmonicityErrors;
    for (const auto& result : results)
    {
        absoluteGainErrors.push_back (std::abs (result.gainErrorDb));
        if (result.hasPitchResult)
            absolutePitchErrors.push_back (std::abs (result.pitchErrorSemitones));
        if (result.hasInharmonicityResult)
        {
            inharmonicityErrors.push_back (result.inharmonicityErrorRatio);
            absoluteInharmonicityErrors.push_back (std::abs (result.inharmonicityErrorRatio));
        }
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("schema", "whirldt.profile-validation.v1");
    root->setProperty ("sampleRate", sampleRate);
    root->setProperty ("detectedDryTakes", static_cast<int> (dryStarts.size()));
    root->setProperty ("detectedWetTakes", static_cast<int> (wetStarts.size()));
    root->setProperty ("medianAbsoluteGainErrorDb", median (absoluteGainErrors));
    root->setProperty ("maximumAbsoluteGainErrorDb",
                       *std::max_element (absoluteGainErrors.begin(), absoluteGainErrors.end()));
    root->setProperty ("medianAbsolutePitchErrorSemitones", median (absolutePitchErrors));
    root->setProperty ("maximumAbsolutePitchErrorSemitones",
                       *std::max_element (absolutePitchErrors.begin(), absolutePitchErrors.end()));
    if (! inharmonicityErrors.empty())
    {
        root->setProperty ("medianInharmonicityErrorRatio", median (inharmonicityErrors));
        root->setProperty ("medianAbsoluteInharmonicityErrorRatio", median (absoluteInharmonicityErrors));
    }
    root->setProperty ("results", jsonResults);

    const auto report = juce::JSON::toString (juce::var (root.release()), true);
    const auto output = juce::File (argv[4]);
    if (! output.replaceWithText (report))
    {
        std::cerr << "Could not write report: " << output.getFullPathName() << '\n';
        return 2;
    }

    std::cout << "Median |gain error|: " << juce::String (median (absoluteGainErrors), 2)
              << " dB, max: " << juce::String (*std::max_element (absoluteGainErrors.begin(), absoluteGainErrors.end()), 2)
              << " dB\nMedian |pitch error|: " << juce::String (median (absolutePitchErrors), 3)
              << " st, max: " << juce::String (*std::max_element (absolutePitchErrors.begin(), absolutePitchErrors.end()), 3)
              << " st\nReport: " << output.getFullPathName() << '\n';

    if (! phraseStudyPitchErrors.empty())
        std::cout << "Phrase study (rapidly-changing notes) |pitch error|: median="
                  << juce::String (median (phraseStudyPitchErrors), 3) << " st, max="
                  << juce::String (*std::max_element (phraseStudyPitchErrors.begin(), phraseStudyPitchErrors.end()), 3)
                  << " st, n=" << static_cast<int> (phraseStudyPitchErrors.size()) << '\n';

    // Positive = plugin more inharmonic/glitchy than the real hardware on
    // that section; negative = cleaner than hardware. The target is not
    // zero - the user wants the hardware's own measured character tracked,
    // not eliminated - so median signed error close to 0 with a small
    // median |error| is the goal, not driving this to the largest possible
    // negative number.
    if (! inharmonicityErrors.empty())
        std::cout << "Inharmonicity error (plugin - hardware, +=more glitchy, -=cleaner than hardware): median="
                  << juce::String (median (inharmonicityErrors), 4) << ", median |error|="
                  << juce::String (median (absoluteInharmonicityErrors), 4)
                  << ", n=" << static_cast<int> (inharmonicityErrors.size()) << '\n';

    return 0;
}
