#include "WhirlDTDSP.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <iostream>
#include <numeric>

namespace
{
    bool expect (bool condition, const char* message)
    {
        if (! condition)
            std::cerr << "FAILED: " << message << '\n';
        return condition;
    }

    float findDominantFrequency (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        constexpr int fftOrder = 15;
        constexpr int fftSize = 1 << fftOrder;
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (fftSize,
                                                     juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> data (static_cast<size_t> (fftSize * 2), 0.0f);
        const auto start = buffer.getNumSamples() - fftSize;
        std::copy_n (buffer.getReadPointer (0, start), fftSize, data.begin());
        window.multiplyWithWindowingTable (data.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (data.data());

        const auto firstBin = juce::roundToInt (50.0 * fftSize / sampleRate);
        const auto lastBin = juce::roundToInt (1000.0 * fftSize / sampleRate);
        int peakBin = firstBin;
        for (int bin = firstBin + 1; bin <= lastBin; ++bin)
            if (data[static_cast<size_t> (bin)] > data[static_cast<size_t> (peakBin)])
                peakBin = bin;

        return static_cast<float> (peakBin * sampleRate / fftSize);
    }

    float findZeroCrossingFrequency (const juce::AudioBuffer<float>& buffer, double sampleRate, int analysisSamples)
    {
        const auto* data = buffer.getReadPointer (0);
        const auto start = buffer.getNumSamples() - analysisSamples;
        int crossings = 0;
        for (int i = start + 1; i < start + analysisSamples; ++i)
            if (data[i - 1] <= 0.0f && data[i] > 0.0f)
                ++crossings;
        return static_cast<float> (crossings) * static_cast<float> (sampleRate) / static_cast<float> (analysisSamples);
    }

    struct RenderResult
    {
        float frequency = 0.0f;
        float zeroCrossingFrequency = 0.0f;
        float rms = 0.0f;
        float peak = 0.0f;
    };

    RenderResult renderPitchShift (float inputFrequency, int dropTuneMode)
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 4;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);

        juce::AudioBuffer<float> rendered (1, totalSamples);
        WhirlDTDSP::Parameters parameters;
        parameters.dropTuneMode = dropTuneMode;
        parameters.dropTuneEnabled = true;

        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0, sample, 0.25f * std::sin (static_cast<float> (phase)));
                phase += juce::MathConstants<double>::twoPi * inputFrequency / sampleRate;
            }

            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        constexpr int measurementSamples = 44100;
        const auto measurementStart = totalSamples - measurementSamples;
        return { findDominantFrequency (rendered, sampleRate),
                 findZeroCrossingFrequency (rendered, sampleRate, measurementSamples),
                 rendered.getRMSLevel (0, measurementStart, measurementSamples),
                 rendered.getMagnitude (0, measurementStart, measurementSamples) };
    }

    // Renders a steady 220 Hz tone through one whirl-selector mode (bypassing
    // drop tune) and returns the output RMS once the FFT/smoothing transient
    // has settled, so it can be compared against the dry input RMS.
    float renderWhirlModeOutputRms (int whirlMode, float pedalPosition)
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 2;
        constexpr float amplitude = 0.4f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);

        WhirlDTDSP::Parameters parameters;
        parameters.whirlMode = whirlMode;
        parameters.whirlEnabled = true;
        parameters.pedalPosition = pedalPosition;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0, sample, amplitude * std::sin (static_cast<float> (phase)));
                phase += juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
            }

            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        constexpr int measurementSamples = 44100 / 2;
        const auto measurementStart = totalSamples - measurementSamples;
        return rendered.getRMSLevel (0, measurementStart, measurementSamples);
    }

    float renderDropTuneModeOutputRms (int dropTuneMode, float inputFrequency)
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 2;
        constexpr float amplitude = 0.4f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);

        WhirlDTDSP::Parameters parameters;
        parameters.dropTuneMode = dropTuneMode;
        parameters.dropTuneEnabled = true;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0, sample, amplitude * std::sin (static_cast<float> (phase)));
                phase += juce::MathConstants<double>::twoPi * inputFrequency / sampleRate;
            }

            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        constexpr int measurementSamples = 44100 / 2;
        const auto measurementStart = totalSamples - measurementSamples;
        return rendered.getRMSLevel (0, measurementStart, measurementSamples);
    }

    enum class Stage { dropTune, whirl };

    // Renders a decaying, vibrato'd, harmonically rich note - much closer to
    // an actual played guitar note than a stationary pure tone - and reports
    // the largest sample-to-sample jumps in the output, to directly check for
    // discontinuities/clicks rather than inferring their presence indirectly
    // from level or pitch measurements. Returns false and logs a failure if
    // any jump exceeds what this signal's own (band-limited, amplitude ~0.4)
    // content could plausibly produce. Covers either selector so it can
    // reach Whirl's Whammy group too, not just Drop Tune.
    bool checkForClicks (Stage stage, int mode, const char* label)
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 3;
        constexpr float fundamental = 196.0f; // G3

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);
        WhirlDTDSP::Parameters parameters;
        if (stage == Stage::dropTune)
        {
            parameters.dropTuneMode = mode;
            parameters.dropTuneEnabled = true;
        }
        else
        {
            parameters.whirlMode = mode;
            parameters.whirlEnabled = true;
        }

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const auto t = static_cast<float> (offset + sample) / static_cast<float> (sampleRate);
                const auto envelope = std::exp (-t * 0.6f);
                const auto vibrato = 1.0f + 0.006f * std::sin (juce::MathConstants<float>::twoPi * 5.5f * t);
                const auto value = 0.5f * std::sin (static_cast<float> (phase))
                                  + 0.22f * std::sin (2.0f * static_cast<float> (phase))
                                  + 0.1f * std::sin (3.0f * static_cast<float> (phase));
                block.setSample (0, sample, 0.4f * envelope * value);
                phase += juce::MathConstants<double>::twoPi * fundamental * vibrato / sampleRate;
            }
            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        const auto* data = rendered.getReadPointer (0);
        int hugeJumps = 0;
        constexpr float hugeThreshold = 0.15f;
        for (int i = 1; i < totalSamples; ++i)
            if (std::abs (data[i] - data[i - 1]) > hugeThreshold)
                ++hugeJumps;

        return expect (hugeJumps == 0, (juce::String (label) + " has audible discontinuities").toRawUTF8());
    }
}

int main()
{
    bool passed = true;

    passed &= expect (WhirlDTDSP::getWhirlModeModel (0).mixesDry, "Harmony modes must retain dry signal");
    passed &= expect (WhirlDTDSP::getWhirlModeModel (9).directlyMeasured,
                      "The recovered raw two-octave-up take must remain marked as measured");
    passed &= expect (std::abs (WhirlDTDSP::getDropTuneModeModel (8).toeSemitones - 1.0f) < 0.001f,
                      "Drop Tune +1 mapping is incorrect");
    passed &= expect (std::abs (WhirlDTDSP::getDropTuneModeModel (16).toeSemitones + 12.0f) < 0.001f,
                      "Drop Tune octave-down mapping is incorrect");
    const auto& shallow = WhirlDTDSP::getWhirlModeModel (19);
    const auto& deep = WhirlDTDSP::getWhirlModeModel (20);
    passed &= expect (std::abs (shallow.toeSemitones + 0.075f) < 0.001f,
                      "Detune Shallow must use the strengthened -7.5 cent endpoint");
    passed &= expect (std::abs (shallow.toeSemitones) < std::abs (deep.toeSemitones),
                      "Detune Shallow must remain less intense than Deep");

    passed &= checkForClicks (Stage::dropTune, 8, "Drop Tune +1 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 7, "Drop Tune +2 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 6, "Drop Tune +3 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 5, "Drop Tune +4 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 4, "Drop Tune +5 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 3, "Drop Tune +6 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 2, "Drop Tune +7 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 1, "Drop Tune Oct Up (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 12, "Drop Tune -3 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::dropTune, 16, "Drop Tune Oct Dn (decaying/vibrato G3)");

    // Whirl's Whammy group had zero click coverage before this - it's one of
    // the two sections the modern spectral engine now handles.
    passed &= checkForClicks (Stage::whirl, 9, "Whirl Whammy +24 raw take (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::whirl, 10, "Whirl Whammy +12 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::whirl, 13, "Whirl Whammy -2 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::whirl, 17, "Whirl Whammy -24 (decaying/vibrato G3)");
    passed &= checkForClicks (Stage::whirl, 18, "Whirl Whammy Dive Bomb (decaying/vibrato G3)");

    // Dive bomb continuously sweeps the treadle across the whole ratio range
    // the spectral engine has to cover (r=0.125 to r=1.0 and back) in one
    // gesture - the single highest-risk path for the geometric hop scaling
    // and the granular-character blend, both of which are most stressed at
    // that extreme, so it gets its own dedicated sweep rather than relying
    // on the fixed-pedal checks above.
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 3;
        constexpr float fundamental = 196.0f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);
        WhirlDTDSP::Parameters parameters;
        parameters.whirlMode = 18; // dive bomb
        parameters.whirlEnabled = true;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const auto t = static_cast<float> (offset + sample) / static_cast<float> (sampleRate);
                const auto envelope = std::exp (-t * 0.3f);
                const auto value = 0.5f * std::sin (static_cast<float> (phase))
                                  + 0.22f * std::sin (2.0f * static_cast<float> (phase))
                                  + 0.1f * std::sin (3.0f * static_cast<float> (phase));
                block.setSample (0, sample, 0.4f * envelope * value);
                phase += juce::MathConstants<double>::twoPi * fundamental / sampleRate;
            }
            const auto sweepPhase = static_cast<float> (offset) / static_cast<float> (totalSamples);
            parameters.pedalPosition = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * sweepPhase);
            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        const auto* data = rendered.getReadPointer (0);
        int hugeJumps = 0;
        constexpr float hugeThreshold = 0.15f;
        for (int i = 1; i < totalSamples; ++i)
            if (std::abs (data[i] - data[i - 1]) > hugeThreshold)
                ++hugeJumps;

        passed &= expect (hugeJumps == 0, "Dive bomb pedal sweep has audible discontinuities");
    }

    // Switching whirlMode from Harmony (8, legacy-only) to Whammy (9,
    // hybrid) mid-render exercises the hybridBlend crossfade directly -
    // nothing else does, since the fixed-mode checks above never cross that
    // engine boundary.
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 2;
        constexpr float fundamental = 196.0f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);
        WhirlDTDSP::Parameters parameters;
        parameters.whirlEnabled = true;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0, sample, 0.4f * std::sin (static_cast<float> (phase)));
                phase += juce::MathConstants<double>::twoPi * fundamental / sampleRate;
            }
            parameters.whirlMode = offset > totalSamples / 2 ? 9 : 8;
            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        const auto* data = rendered.getReadPointer (0);
        int hugeJumps = 0;
        constexpr float hugeThreshold = 0.15f;
        for (int i = 1; i < totalSamples; ++i)
            if (std::abs (data[i] - data[i - 1]) > hugeThreshold)
                ++hugeJumps;

        passed &= expect (hugeJumps == 0,
                          "Mode switch across the Harmony/Whammy engine boundary has audible discontinuities");
    }

    // At the heel, every Whammy mode's ratio is exactly 1.0 - the spectral
    // engine should behave as a clean near-identity pass-through there, a
    // useful sanity check that the resample/normalisation plumbing isn't
    // subtly wrong even at the trivial ratio.
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 2;
        constexpr float inputFrequency = 220.0f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);
        WhirlDTDSP::Parameters parameters;
        parameters.whirlMode = 9; // Whammy +24 raw take: heel = 0st = unity ratio
        parameters.whirlEnabled = true;
        parameters.pedalPosition = 1.0f;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0, sample, 0.4f * std::sin (static_cast<float> (phase)));
                phase += juce::MathConstants<double>::twoPi * inputFrequency / sampleRate;
            }
            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        const auto outputFrequency = findDominantFrequency (rendered, sampleRate);
        passed &= expect (std::abs (outputFrequency - inputFrequency) < 3.0f,
                          "Whammy engine at unity ratio (heel) is not a clean near-identity pass-through");
    }

    {
        WhirlDTDSP::Processor processor;
        constexpr int blockSize = 64;
        processor.prepare (44100.0, blockSize, 2);
        const auto latency = processor.getLatencySamples();
        const auto totalSamples = latency + 512;
        // Identical L/R content: the plugin now always sums its input down
        // to the real Whammy DT's single mono signal path before processing
        // (see Processor::process), so a stereo buffer with genuinely
        // different per-channel content would no longer be expected to
        // survive bypass unchanged per channel - that's the point of the
        // mono-sum, not a bug in it. This still verifies the case that
        // matters: a mono guitar signal merely duplicated across both
        // channels must come back out unprocessed (after the reported fixed
        // latency), not altered or narrowed.
        juce::AudioBuffer<float> buffer (2, totalSamples);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < totalSamples; ++sample)
                buffer.setSample (channel, sample,
                                  0.1f * std::sin (0.013f * static_cast<float> (sample)));

        juce::AudioBuffer<float> expected;
        expected.makeCopyOf (buffer);
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, offset, count);
            processor.process (block, {});
        }

        float maximumError = 0.0f;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < totalSamples; ++sample)
            {
                const auto expectedSample = sample >= latency ? expected.getSample (channel, sample - latency) : 0.0f;
                maximumError = juce::jmax (maximumError,
                                           std::abs (buffer.getSample (channel, sample) - expectedSample));
            }
        passed &= expect (maximumError < 1.0e-7f,
                          "Bypassed DSP must be bit-transparent after its reported fixed latency");
    }

    // The real Whammy DT has a single mono instrument input - genuinely
    // different left/right content (not just a mono signal duplicated to
    // both channels) must be averaged down to one signal before either
    // stage processes it, and the shifted result must come back out
    // identical on every output channel, not processed independently per
    // channel (which would be incoherent: the granular engine's pitch
    // tracking and grain phase, and the Signalsmith/spectral engines'
    // analysis, would each run on unrelated content).
    {
        WhirlDTDSP::Processor processor;
        constexpr int blockSize = 64;
        processor.prepare (44100.0, blockSize, 2);
        WhirlDTDSP::Parameters parameters;
        parameters.dropTuneMode = 1; // shift up oct
        parameters.dropTuneEnabled = true;

        constexpr int totalSamples = 44100;
        juce::AudioBuffer<float> buffer (2, totalSamples);
        for (int sample = 0; sample < totalSamples; ++sample)
        {
            buffer.setSample (0, sample, 0.3f * std::sin (0.05f * static_cast<float> (sample)));
            buffer.setSample (1, sample, 0.3f * std::sin (0.031f * static_cast<float> (sample) + 1.0f));
        }

        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, offset, count);
            processor.process (block, parameters);
        }

        float maximumChannelDifference = 0.0f;
        for (int sample = 0; sample < totalSamples; ++sample)
            maximumChannelDifference = juce::jmax (maximumChannelDifference,
                std::abs (buffer.getSample (0, sample) - buffer.getSample (1, sample)));

        passed &= expect (maximumChannelDifference < 1.0e-6f,
                          "Differing left/right input must produce identical output on every channel (mono sum)");
    }

    const auto octaveUp = renderPitchShift (110.0f, 1);
    const auto octaveDown = renderPitchShift (220.0f, 16);
    // Tight now that the grain period is retuned to a multiple of the
    // detected input period (updatePitchSynchronousGrainPeriod), not just
    // WSOLA-nudged at each wrap - see WhirlDTDSP.cpp for why that's what
    // actually removes the phase mismatch between overlapping grains instead
    // of just reducing it.
    passed &= expect (std::abs (octaveUp.frequency - 220.0f) < 3.0f, "Octave-up pitch ratio is inaccurate");
    passed &= expect (std::abs (octaveDown.frequency - 110.0f) < 3.0f, "Octave-down pitch ratio is inaccurate");
    passed &= expect (std::isfinite (octaveUp.rms) && octaveUp.rms > 0.01f && octaveUp.peak < 2.0f,
                      "Octave-up output level is invalid");
    passed &= expect (std::isfinite (octaveDown.rms) && octaveDown.rms > 0.01f && octaveDown.peak < 2.0f,
                      "Octave-down output level is invalid");

    // Every upward fixed interval must keep the requested tone as the dominant
    // spectral component.  Octaves alone can hide granular modulation errors
    // because their grain phases happen to line up particularly well.
    for (int mode = 2; mode <= 8; ++mode)
    {
        constexpr float inputFrequency = 82.41f;
        const auto rendered = renderPitchShift (inputFrequency, mode);
        const auto expectedSemitones = WhirlDTDSP::getDropTuneModeModel (mode).toeSemitones;
        const auto expectedFrequency = inputFrequency * std::pow (2.0f, expectedSemitones / 12.0f);
        const auto fftErrorSemitones = 12.0f * std::log2 (rendered.frequency / expectedFrequency);
        const auto crossingErrorSemitones = 12.0f * std::log2 (rendered.zeroCrossingFrequency / expectedFrequency);
        passed &= expect (juce::jmin (std::abs (fftErrorSemitones), std::abs (crossingErrorSemitones)) < 0.25f,
                          (juce::String ("Drop Tune mode ") + juce::String (mode)
                           + " dominant-frequency error exceeds 0.25 semitone (expected "
                           + juce::String (expectedFrequency, 2) + " Hz, got "
                           + juce::String (rendered.frequency, 2) + " Hz, zero-cross "
                           + juce::String (rendered.zeroCrossingFrequency, 2) + " Hz)").toRawUTF8());
    }

    // Amplitude-wobble check: sustaining a tone through a mode must not
    // produce an audible tremolo. Two overlapping grains reading the same
    // periodic content at a phase offset that isn't a multiple of the
    // input's period beat against each other, heard as a slow "wobble" in
    // level even when the average pitch and gain both look fine - this is
    // the actual complaint pitch-synchronising the grain period (rather than
    // just WSOLA-nudging each wrap) was meant to fix, so check it directly
    // instead of only checking the average pitch/level.
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 3;
        constexpr float amplitude = 0.4f;

        const struct { int mode; float frequency; const char* label; } wobbleCases[] = {
            { 8, 146.83f, "Drop Tune +1 @D3" },
            { 12, 196.0f, "Drop Tune -3 @G3" },
            { 16, 82.41f, "Drop Tune Oct Dn @E2" },
        };

        for (const auto& testCase : wobbleCases)
        {
            WhirlDTDSP::Processor processor;
            processor.prepare (sampleRate, blockSize, 1);
            WhirlDTDSP::Parameters parameters;
            parameters.dropTuneMode = testCase.mode;
            parameters.dropTuneEnabled = true;

            juce::AudioBuffer<float> rendered (1, totalSamples);
            double phase = 0.0;
            for (int offset = 0; offset < totalSamples; offset += blockSize)
            {
                const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, samplesThisBlock);
                for (int sample = 0; sample < samplesThisBlock; ++sample)
                {
                    block.setSample (0, sample, amplitude * std::sin (static_cast<float> (phase)));
                    phase += juce::MathConstants<double>::twoPi * testCase.frequency / sampleRate;
                }
                processor.process (block, parameters);
                rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
            }

            constexpr int windowSamples = 4410; // 100ms
            constexpr int measurementStart = 44100; // skip the first second to settle
            std::vector<float> windowRms;
            for (int start = measurementStart; start + windowSamples <= totalSamples; start += windowSamples)
                windowRms.push_back (rendered.getRMSLevel (0, start, windowSamples));

            const auto meanRms = std::accumulate (windowRms.begin(), windowRms.end(), 0.0f) / (float) windowRms.size();
            auto maxDeviationDb = 0.0f;
            for (auto level : windowRms)
                maxDeviationDb = juce::jmax (maxDeviationDb,
                                             std::abs (juce::Decibels::gainToDecibels (level)
                                                     - juce::Decibels::gainToDecibels (meanRms)));

            passed &= expect (maxDeviationDb < 2.0f,
                              (juce::String (testCase.label) + " wobbles more than +/-2dB across a sustained note")
                                  .toRawUTF8());
        }
    }

    {
        // A harmonically rich, guitar-note-like tone (fundamental + 3
        // overtones) rather than a pure sine, since that's what the grain-bank
        // transition actually has to work with in practice.
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int totalSamples = 44100 * 4;
        constexpr float fundamental = 220.0f;

        WhirlDTDSP::Processor processor;
        processor.prepare (sampleRate, blockSize, 1);
        WhirlDTDSP::Parameters parameters;
        parameters.dropTuneMode = 16; // octave down, expect ~110Hz
        parameters.dropTuneEnabled = true;

        juce::AudioBuffer<float> rendered (1, totalSamples);
        double phase = 0.0;
        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, samplesThisBlock);
            for (int sample = 0; sample < samplesThisBlock; ++sample)
            {
                const auto value = 0.5f * std::sin (static_cast<float> (phase))
                                  + 0.25f * std::sin (2.0f * static_cast<float> (phase))
                                  + 0.125f * std::sin (3.0f * static_cast<float> (phase))
                                  + 0.06f * std::sin (4.0f * static_cast<float> (phase));
                block.setSample (0, sample, 0.3f * value);
                phase += juce::MathConstants<double>::twoPi * fundamental / sampleRate;
            }

            processor.process (block, parameters);
            rendered.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
        }

        const auto harmonicFundamental = findDominantFrequency (rendered, sampleRate);
        passed &= expect (std::abs (harmonicFundamental - 110.0f) < 15.0f,
                          "Octave-down on a harmonically rich tone is inaccurate");
    }

    if (passed)
        std::cout << "WhirlDT DSP tests passed (up=" << octaveUp.frequency
                  << " Hz, down=" << octaveDown.frequency << " Hz)\n";

    // Level modelling: the rendered steady-state level must follow the stable
    // staircase median measured from the hardware, not an artificial unity
    // target and not the distorted/global RMS portions of the capture.
    {
        constexpr float inputAmplitude = 0.4f;
        const auto inputRms = inputAmplitude / std::sqrt (2.0f);
        const auto inputRmsDb = juce::Decibels::gainToDecibels (inputRms);

        // Parameters::pedalPosition is 0 = toe, 1 = heel (see WhirlDTDSP.h).
        const struct { int mode; float pedal; const char* label; } cases[] = {
            { 0, 0.0f, "Harmony Oct Dn/Oct Up (toe, +12st)" },
            { 0, 1.0f, "Harmony Oct Dn/Oct Up (heel, -12st)" },
            { 10, 0.0f, "Whammy 1 Oct Up (toe, +12st)" },
            { 18, 0.0f, "Dive Bomb (toe, -36st)" },
            { 19, 0.0f, "Detune Shallow (toe, -0.075st)" },
            { 20, 0.0f, "Detune Deep (toe, -0.114st)" },
        };

        for (const auto& testCase : cases)
        {
            const auto outputRms = renderWhirlModeOutputRms (testCase.mode, testCase.pedal);
            const auto outputRmsDb = juce::Decibels::gainToDecibels (outputRms);
            const auto deviation = outputRmsDb - inputRmsDb;

            const auto expectedDeviation = WhirlDTDSP::getWhirlModeModel (testCase.mode).outputGainDb;
            if (std::abs (deviation - expectedDeviation) >= 1.5f)
            {
                std::cerr << "FAILED: " << testCase.label << " output is " << deviation
                          << " dB relative to dry input (expected " << expectedDeviation
                          << " dB +/-1.5 dB)\n";
                passed = false;
            }
        }

        // Drop Tune, checked at two input frequencies: the underlying bin-shift
        // pitch shifter's own gain deviation is frequency-dependent as well as
        // ratio-dependent, so a single frequency isn't enough to trust the fix.
        const struct { int mode; float frequency; const char* label; } dropTuneCases[] = {
            { 1, 220.0f, "Drop Tune Oct Up @220Hz" },
            { 1, 110.0f, "Drop Tune Oct Up @110Hz" },
            { 16, 220.0f, "Drop Tune Oct Dn @220Hz" },
        };

        for (const auto& testCase : dropTuneCases)
        {
            const auto outputRms = renderDropTuneModeOutputRms (testCase.mode, testCase.frequency);
            const auto outputRmsDb = juce::Decibels::gainToDecibels (outputRms);
            const auto deviation = outputRmsDb - inputRmsDb;

            const auto expectedDeviation = WhirlDTDSP::getDropTuneModeModel (testCase.mode).outputGainDb;
            if (std::abs (deviation - expectedDeviation) >= 1.5f)
            {
                std::cerr << "FAILED: " << testCase.label << " output is " << deviation
                          << " dB relative to dry input (expected " << expectedDeviation
                          << " dB +/-1.5 dB)\n";
                passed = false;
            }
        }

        if (passed)
            std::cout << "Measured-level check passed (all sampled modes within +/-1.5 dB of target)\n";
    }

    // Sweep every selector position (including the pedal treadle and the
    // enable/bypass transition) at a few sample rates to catch indexing bugs,
    // NaN/Inf blow-ups, or unbounded output that the two targeted cases above
    // wouldn't reveal.
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        constexpr int blockSize = 256;
        constexpr int totalSamples = 8192;

        for (int whirlMode = 0; whirlMode < 21; ++whirlMode)
        {
            WhirlDTDSP::Processor processor;
            processor.prepare (sampleRate, blockSize, 2);

            WhirlDTDSP::Parameters parameters;
            parameters.whirlMode = whirlMode;
            parameters.whirlEnabled = true;

            juce::AudioBuffer<float> buffer (2, totalSamples);
            double phase = 0.0;
            for (int offset = 0; offset < totalSamples; offset += blockSize)
            {
                const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (2, samplesThisBlock);
                for (int sample = 0; sample < samplesThisBlock; ++sample)
                {
                    const auto value = 0.4f * std::sin (static_cast<float> (phase));
                    block.setSample (0, sample, value);
                    block.setSample (1, sample, value);
                    phase += juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
                }

                // Sweep the pedal from heel to toe over the render and flip
                // enabled mid-stream, to exercise the smoothing ramps.
                parameters.pedalPosition = static_cast<float> (offset) / static_cast<float> (totalSamples);
                parameters.whirlEnabled = offset > totalSamples / 4;

                processor.process (block, parameters);
                buffer.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
                buffer.copyFrom (1, offset, block, 1, 0, samplesThisBlock);
            }

            for (int channel = 0; channel < 2; ++channel)
            {
                const auto* data = buffer.getReadPointer (channel);
                for (int sample = 0; sample < totalSamples; ++sample)
                {
                    if (! std::isfinite (data[sample]) || std::abs (data[sample]) > 10.0f)
                    {
                        std::cerr << "FAILED: whirl mode " << whirlMode << " at " << sampleRate
                                  << " Hz produced a non-finite or unbounded sample (" << data[sample]
                                  << ") at index " << sample << " channel " << channel << '\n';
                        passed = false;
                    }
                }
            }
        }

        for (int dropTuneMode = 0; dropTuneMode < 18; ++dropTuneMode)
        {
            WhirlDTDSP::Processor processor;
            processor.prepare (sampleRate, blockSize, 1);

            WhirlDTDSP::Parameters parameters;
            parameters.dropTuneMode = dropTuneMode;
            parameters.dropTuneEnabled = true;

            juce::AudioBuffer<float> buffer (1, totalSamples);
            double phase = 0.0;
            for (int offset = 0; offset < totalSamples; offset += blockSize)
            {
                const auto samplesThisBlock = juce::jmin (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, samplesThisBlock);
                for (int sample = 0; sample < samplesThisBlock; ++sample)
                {
                    block.setSample (0, sample, 0.4f * std::sin (static_cast<float> (phase)));
                    phase += juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
                }

                processor.process (block, parameters);
                buffer.copyFrom (0, offset, block, 0, 0, samplesThisBlock);
            }

            const auto* data = buffer.getReadPointer (0);
            for (int sample = 0; sample < totalSamples; ++sample)
            {
                if (! std::isfinite (data[sample]) || std::abs (data[sample]) > 10.0f)
                {
                    std::cerr << "FAILED: drop tune mode " << dropTuneMode << " at " << sampleRate
                              << " Hz produced a non-finite or unbounded sample (" << data[sample]
                              << ") at index " << sample << '\n';
                    passed = false;
                }
            }
        }
    }

    if (passed)
        std::cout << "Mode sweep across sample rates passed with no NaN/Inf/unbounded output\n";

    return passed ? 0 : 1;
}
