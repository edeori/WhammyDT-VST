#include "WhirlDTDSP.h"
#include "WhirlDTControls.h"

#include <cmath>
#include <iostream>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

float dominantFrequency (const std::vector<float>& samples, double sampleRate)
{
    constexpr int order = 15;
    constexpr int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data (size * 2, 0.0f);
    std::copy (samples.end() - size, samples.end(), data.begin());
    juce::dsp::WindowingFunction<float> window (size, juce::dsp::WindowingFunction<float>::hann);
    window.multiplyWithWindowingTable (data.data(), size);
    fft.performFrequencyOnlyForwardTransform (data.data());
    const auto peak = std::max_element (data.begin() + 1, data.begin() + size / 2);
    return static_cast<float> (std::distance (data.begin(), peak) * sampleRate / size);
}
}

namespace WhirlDTDSP
{
// Friend access keeps the signal-path invariants testable without exposing
// mutable DSP internals as part of the plugin's public API.
struct RegressionTestAccess
{
    using SpectralShifter = Processor::PitchStage::SpectralShifter;

    static bool checkPeakRegions()
    {
        SpectralShifter shifter;
        shifter.prepare (44100.0, 1);
        const auto ratio = std::pow (2.0f, 7.0f / 12.0f);
        for (int section = 0; section < 4; ++section)
        {
            for (int i = 0; i < 44100; ++i)
            {
                const auto phase = juce::MathConstants<double>::twoPi * i / 44100.0;
                const auto signal = section == 3 ? 0.0 :
                    0.2 * std::sin ((220.0 + section * 71.0) * phase)
                  + 0.1 * std::sin ((660.0 + section * 113.0) * phase);
                shifter.processSample (0, static_cast<float> (signal), ratio);
            }

            const auto& channel = shifter.channels[0];
            std::vector<int> peaks;
            for (int bin = 0; bin < SpectralShifter::numBins; ++bin)
            {
                const auto index = static_cast<size_t> (bin);
                if ((bin == 0 || channel.magnitude[index] >= channel.magnitude[index - 1])
                    && (bin + 1 == SpectralShifter::numBins || channel.magnitude[index] >= channel.magnitude[index + 1]))
                    peaks.push_back (bin);
            }

            // Definition-based oracle: every bin must belong to a real peak
            // at minimum distance. Either peak is acceptable for a tie.
            for (int bin = 0; bin < SpectralShifter::numBins; ++bin)
            {
                const auto assigned = channel.regionPeak[static_cast<size_t> (bin)];
                int distance = SpectralShifter::numBins;
                for (const auto peak : peaks)
                    distance = std::min (distance, std::abs (peak - bin));
                if (! expect (std::find (peaks.begin(), peaks.end(), assigned) != peaks.end()
                              && std::abs (assigned - bin) == distance,
                              "Every FFT bin must follow its nearest current magnitude peak"))
                    return false;
            }
        }
        return true;
    }

    static bool checkSpectralTimeline (double sampleRate, float ratio, int seconds, bool sweep,
                                      bool abrupt = false)
    {
        SpectralShifter shifter;
        shifter.prepare (sampleRate, 1);
        const auto total = static_cast<int> (sampleRate * seconds);
        std::vector<float> lastSecond (static_cast<size_t> (sampleRate));
        double earlyEnergy = 0.0, lateEnergy = 0.0;
        for (int i = 0; i < total; ++i)
        {
            const auto liveRatio = abrupt ? ((i / 441) % 2 == 0 ? 0.125f : 4.0f) : sweep
                ? static_cast<float> (std::pow (2.0, -3.0 + 5.0 * (0.5 + 0.5 * std::sin (i / (sampleRate * 0.2)))))
                : ratio;
            auto& channel = shifter.channels[0];
            const auto previousRead = channel.readCursor;
            const auto previousWrite = channel.writeCursor;
            const auto clearedBeforeWrite = channel.clearedUpTo;
            const auto input = static_cast<float> (0.2 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / sampleRate));
            const auto output = shifter.processSample (0, input, liveRatio);

            if (! expect (std::isfinite (output) && std::abs (output) < 1.0f,
                          "Long spectral render must remain finite and bounded")
                || ! expect (std::abs (channel.readCursor - previousRead - liveRatio) < 1.0e-8,
                             "Spectral reader must advance at the requested ratio without rewinds")
                || ! expect (channel.writeCursor - channel.readCursor > 1.0
                             && channel.writeCursor - channel.readCursor < 4096.0,
                             "Writer/reader distance must stay bounded under fixed and changing ratios"))
                return false;

            if (channel.writeCursor != previousWrite
                && ! expect (channel.writeCursor + SpectralShifter::fftSize - 1
                             - static_cast<int64_t> (channel.overlapAdd.size()) < clearedBeforeWrite,
                             "An FFT frame must never reuse an uncleared ring-buffer slot"))
                return false;

            if (i >= static_cast<int> (sampleRate) && i < static_cast<int> (sampleRate * 2.0))
                earlyEnergy += output * output;
            if (i >= total - static_cast<int> (sampleRate))
            {
                lateEnergy += output * output;
                lastSecond[static_cast<size_t> (i - total + static_cast<int> (sampleRate))] = output;
            }
        }

        if (! sweep && ! abrupt)
        {
            const auto frequency = dominantFrequency (lastSecond, sampleRate);
            const auto levelDrift = 10.0 * std::log10 (lateEnergy / earlyEnergy);
            std::cout << "Spectral " << sampleRate << " Hz, ratio " << ratio << ", " << seconds
                      << " s: frequency=" << frequency << " Hz, level drift=" << levelDrift << " dB\n";
            return expect (std::abs (frequency - 220.0f * ratio) < 3.0f,
                           "Long spectral render must retain the requested pitch")
                && expect (lateEnergy / sampleRate > 0.001 && std::abs (levelDrift) < 1.0,
                           "Long spectral render must not lose level or drift by more than 1 dB");
        }
        return true;
    }
};
}

namespace
{
bool checkReset()
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
        for (const auto mode : { 0, 10, 19 })
            for (const bool reprepare : { false, true })
            {
                WhirlDTDSP::Processor processor;
                processor.prepare (sampleRate, 256, 1);
                WhirlDTDSP::Parameters active;
                active.whirlEnabled = true;
                active.whirlMode = mode;
                juce::AudioBuffer<float> warmup (1, 256);
                for (int block = 0; block < 64; ++block)
                {
                    warmup.clear();
                    processor.process (warmup, active);
                }
                if (reprepare)
                    processor.prepare (sampleRate, 256, 1);
                else
                    processor.reset();

                const auto latency = processor.getLatencySamples();
                juce::AudioBuffer<float> impulse (1, latency + 256);
                impulse.clear();
                impulse.setSample (0, 0, 0.5f);
                processor.process (impulse, {});
                for (int i = 0; i < impulse.getNumSamples(); ++i)
                    if (! expect (std::abs (impulse.getSample (0, i) - (i == latency ? 0.5f : 0.0f)) < 1.0e-7f,
                                  "Reset/reprepare must restore exact bypass, including the first sample"))
                        return false;
            }
    return true;
}

struct ParameterListener final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged (int, float) override
    {
        ++changes;
        if (! gestureActive)
            balanced = false;
    }
    void parameterGestureChanged (int, bool starting) override
    {
        if (gestureActive == starting)
            balanced = false;
        gestureActive = starting;
        starting ? ++starts : ++ends;
    }
    int starts = 0, ends = 0, changes = 0;
    bool gestureActive = false, balanced = true;
};

bool checkSelectorGestures()
{
    juce::AudioParameterChoice parameter ({ "testWhirl", 1 }, "Whirl", WhirlDTParam::whirlModeChoices, 0);
    ParameterListener listener;
    parameter.addListener (&listener);
    bool passed = true;
    {
        WhirlSelectorKnob knob;
        juce::SliderParameterAttachment attachment (parameter, knob);
        const auto event = [&knob] (float y, bool dragging, int modifiers = juce::ModifierKeys::leftButtonModifier)
        {
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 40.0f, y },
                                    juce::ModifierKeys (modifiers), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &knob, &knob, now, { 40.0f, 100.0f }, now, 1, dragging);
        };
        for (int drag = 0; drag < 2; ++drag)
        {
            const auto changesBefore = listener.changes;
            knob.mouseDown (event (100.0f, false));
            knob.mouseDrag (event (82.0f, true));
            knob.mouseDrag (event (64.0f, true));
            knob.mouseUp (event (64.0f, true, 0));
            passed &= expect (listener.starts == drag + 1 && listener.ends == drag + 1
                              && listener.changes > changesBefore && listener.balanced && ! listener.gestureActive,
                              "Each selector drag must send one balanced host gesture around its value changes");
        }

        const auto startsBefore = listener.starts;
        knob.setEnabled (false);
        knob.mouseDown (event (100.0f, false));
        knob.mouseDrag (event (0.0f, true));
        knob.mouseUp (event (0.0f, true, 0));
        passed &= expect (listener.starts == startsBefore, "A disabled selector must not start automation gestures");
    }
    parameter.removeListener (&listener);
    return passed;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    using Access = WhirlDTDSP::RegressionTestAccess;
    bool passed = Access::checkPeakRegions();
    passed &= Access::checkSpectralTimeline (44100.0, std::pow (2.0f, 7.0f / 12.0f), 240, false);
    passed &= Access::checkSpectralTimeline (44100.0, std::pow (2.0f, -7.0f / 12.0f), 240, false);
    passed &= Access::checkSpectralTimeline (44100.0, 1.0f, 60, true);
    passed &= Access::checkSpectralTimeline (44100.0, 1.0f, 4, false, true);
    for (const auto rate : { 48000.0, 96000.0 })
    {
        passed &= Access::checkSpectralTimeline (rate, std::pow (2.0f, 5.0f / 12.0f), 4, false);
        passed &= Access::checkSpectralTimeline (rate, 1.0f, 4, true);
    }
    passed &= checkReset();
    passed &= checkSelectorGestures();
    if (passed)
        std::cout << "All DSP and selector regression checks passed\n";
    return passed ? 0 : 1;
}
