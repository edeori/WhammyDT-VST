#include "WhirlDTDSP.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace WhirlDTDSP
{
namespace
{
    // Values are taken from measured.json in the July 2026 Whammy profiling run.
    // Gain is the median of level-staircase sections 20-24, avoiding broadband
    // and dry/wet harmonic content that biases the global RMS comparison. The
    // pitch endpoints come from the hardware mode labels; measured gain and
    // median section latency is retained as diagnostic metadata only and is not
    // added to the runtime signal path. MeasurementMapper's static
    // transfer polynomial is deliberately not used for shifted paths because a
    // sample-to-sample transfer fit is not meaningful once pitch has changed.
    constexpr std::array<ModeModel, 21> whirlModes {{
        // Harmony: the shifted voice is mixed with a dry voice matched only to
        // the pitch shifter's unavoidable internal latency.
        {  12.0f, -12.0f,  2.845278f, 282.615370f, true,  true,  "down oct up oct" },
        {  -5.0f,  -7.0f,  2.883813f, 280.164440f, true,  true,  "down 5th down 4th" },
        {  -3.0f,  -5.0f,  2.813474f, 281.862958f, true,  true,  "down 4th down 3rd" },
        {  10.0f,   7.0f,  2.662012f, 281.708974f, true,  true,  "up 5th up 7th" },
        {   9.0f,   7.0f,  2.699602f, 282.683478f, true,  true,  "up 5th up 6th" },
        {   7.0f,   5.0f,  2.839482f, 282.456954f, true,  true,  "up 4th up 5th" },
        {   5.0f,   4.0f,  2.774308f, 282.634216f, true,  true,  "up 3rd up 4th" },
        {   4.0f,   3.0f,  2.795936f, 282.341967f, true,  true,  "up3b rd up 3rd" },
        {   4.0f,   2.0f,  2.788577f, 282.131484f, true,  true,  "up 2nd up 3rd" },

        // Whammy: wet-only pitch paths.  The final 2-octave-up take is present
        // in the raw WAV pair but was omitted from measured.json; the offline
        // validator recovers its stable staircase median directly.
        {  24.0f,   0.0f, -2.840000f,    0.000000f, false, true,  "up 2 oct raw take" },
        {  12.0f,   0.0f, -0.753264f, 1428.000000f, false, true,  "up oct 2 oct" },
        {   7.0f,   0.0f, -0.413253f, 1216.000000f, false, true,  "up 5th" },
        {   5.0f,   0.0f, -0.358293f, 1123.000000f, false, true,  "up 4nd" },
        {  -2.0f,   0.0f, -0.251675f,  894.000000f, false, true,  "down 2nd" },
        {  -5.0f,   0.0f, -0.187720f,  832.500000f, false, true,  "down 4th" },
        {  -7.0f,   0.0f, -0.177683f,  784.000000f, false, true,  "down 5th" },
        { -12.0f,   0.0f, -0.128137f,  687.500000f, false, true,  "down oct" },
        { -24.0f,   0.0f, -0.644613f,  560.500000f, false, true,  "down 2 oct" },
        { -36.0f,   0.0f, -2.861254f,  496.000000f, false, true,  "dive bomb" },

        // Deep follows the stable level-staircase measurement. Shallow's
        // measured -4 cent offset was too restrained in playing tests, so its
        // toe endpoint is deliberately strengthened to -7.5 cents: clearly
        // audible while retaining useful separation from Deep (-11.4 cents).
        {  -0.075f, 0.0f, -0.691991f,  288.818075f, true,  false, "shallow" },
        {  -0.114f, 0.0f,  2.408823f,  283.507438f, true,  true,  "deep" }
    }};

    constexpr std::array<ModeModel, 18> dropTuneModes {{
        // These trims start from the stable hardware staircase medians, then
        // compensate the fixed spectral engine's own steady-state gain.  They
        // deliberately do not use the clipped/global capture RMS.
        {  12.0f,  12.0f,  1.719687f,  281.966808f, true,  true, "shift up oct + dry" },
        {  12.0f,  12.0f, -2.245035f, 1440.000000f, false, true, "shift up oct" },
        {   7.0f,   7.0f,  0.160488f, 1222.500000f, false, true, "shift up 7" },
        {   6.0f,   6.0f,  0.241014f, 1177.000000f, false, true, "shift up 6" },
        {   5.0f,   5.0f,  0.200632f, 1120.000000f, false, true, "shift up 5" },
        {   4.0f,   4.0f,  0.499681f, 1084.500000f, false, true, "shift up 4" },
        {   3.0f,   3.0f,  0.287998f, 1045.500000f, false, true, "shift up 3" },
        {   2.0f,   2.0f,  0.435673f, 1006.000000f, false, true, "shift up 2" },
        {   1.0f,   1.0f,  0.434091f,  975.000000f, false, true, "shift up 1" },
        {  -1.0f,  -1.0f,  0.348705f,  920.500000f, false, true, "shift down 1" },
        {  -2.0f,  -2.0f,  0.345731f,  883.500000f, false, true, "shift down 2" },
        {  -3.0f,  -3.0f,  0.446508f,  868.500000f, false, true, "shift down 3" },
        {  -4.0f,  -4.0f,  0.640455f,  845.500000f, false, true, "shift down 4" },
        {  -5.0f,  -5.0f,  0.626934f,  810.500000f, false, true, "shift down 5" },
        {  -6.0f,  -6.0f,  0.435570f,  791.000000f, false, true, "shift down 6" },
        {  -7.0f,  -7.0f,  0.425494f,  765.000000f, false, true, "shift down 7" },
        { -12.0f, -12.0f,  0.432293f,  673.000000f, false, true, "shift down oct" },
        { -12.0f, -12.0f,  5.204447f,  282.365227f, true,  true, "shift down oct + dry" }
    }};
}

const ModeModel& getWhirlModeModel (int modeIndex) noexcept
{
    return whirlModes[static_cast<size_t> (juce::jlimit (0, static_cast<int> (whirlModes.size()) - 1, modeIndex))];
}

const ModeModel& getDropTuneModeModel (int modeIndex) noexcept
{
    return dropTuneModes[static_cast<size_t> (juce::jlimit (0, static_cast<int> (dropTuneModes.size()) - 1, modeIndex))];
}

void Processor::PitchStage::SpectralShifter::prepare (double sampleRate, int channelCount)
{
    currentSampleRate = juce::jmax (1.0, sampleRate);
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.assign (static_cast<size_t> (fftSize), 0.0f);
    for (int i = 0; i < fftSize; ++i)
        window[static_cast<size_t> (i)] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                                  * static_cast<float> (i) / static_cast<float> (fftSize - 1));

    // Input history only ever needs to hold one analysis window's worth of
    // recent samples.
    const auto inputHistorySize = juce::nextPowerOfTwo (fftSize * 2);

    // Generous headroom around the fftSize the resample reader must always
    // stay behind the writer by (see runHop/processSample), plus room for
    // the reader/writer gap to drift a little as the ratio changes
    // continuously (the expression treadle sweeps every Whammy mode's ratio
    // in real time) without the ring ever wrapping into itself.
    const auto overlapSize = juce::nextPowerOfTwo (fftSize * 8);

    channels.resize (static_cast<size_t> (juce::jmax (1, channelCount)));
    for (auto& channel : channels)
    {
        channel.inputHistory.assign (static_cast<size_t> (inputHistorySize), 0.0f);
        channel.inputMask = inputHistorySize - 1;
        channel.previousPhase.assign (static_cast<size_t> (numBins), 0.0f);
        channel.synthesisPhase.assign (static_cast<size_t> (numBins), 0.0f);
        channel.overlapAdd.assign (static_cast<size_t> (overlapSize), 0.0f);
        channel.overlapNorm.assign (static_cast<size_t> (overlapSize), 0.0f);
        channel.overlapMask = static_cast<int64_t> (overlapSize) - 1;
        channel.fftData.assign (static_cast<size_t> (fftSize * 2), 0.0f);
        channel.magnitude.assign (static_cast<size_t> (numBins), 0.0f);
        channel.phase.assign (static_cast<size_t> (numBins), 0.0f);
        channel.regionPeak.assign (static_cast<size_t> (numBins), 0);
    }
    reset();
}

void Processor::PitchStage::SpectralShifter::reset()
{
    for (auto& channel : channels)
    {
        std::fill (channel.inputHistory.begin(), channel.inputHistory.end(), 0.0f);
        channel.inputWritePos = 0;
        channel.samplesUntilAnalysis = baseHop;
        std::fill (channel.previousPhase.begin(), channel.previousPhase.end(), 0.0f);
        channel.havePreviousPhase = false;
        std::fill (channel.synthesisPhase.begin(), channel.synthesisPhase.end(), 0.0f);
        std::fill (channel.overlapAdd.begin(), channel.overlapAdd.end(), 0.0f);
        std::fill (channel.overlapNorm.begin(), channel.overlapNorm.end(), 0.0f);
        // Start the reader a full window behind the writer so the first
        // samples ever read come from a position that will have already
        // been fully overlap-added by the time it's reached, rather than
        // reading into not-yet-written silence at the ring's origin.
        channel.writeCursor = fftSize;
        channel.clearedUpTo = 0;
        channel.readCursor = 0.0;
    }
}

void Processor::PitchStage::SpectralShifter::runHop (ChannelState& channel, float ratio) noexcept
{
    // Geometric-mean hop scaling: Hs/Ha = ratio exactly, and Ha*Hs stays
    // constant, so degradation is bounded at both ends of the ratio range
    // instead of blowing up on one side - Whammy alone needs r=0.125 (dive
    // bomb) through r=4 (+24st) in the very same continuously-swept mode,
    // so a fixed hop pair (which only degrades gracefully on one side) can't
    // cover it.
    const auto clampedRatio = juce::jlimit (0.125f, 4.0f, ratio);
    const auto analysisHop = juce::jmax (1.0f, static_cast<float> (baseHop) / std::sqrt (clampedRatio));
    const auto synthesisHop = juce::jmax (1.0f, static_cast<float> (baseHop) * std::sqrt (clampedRatio));

    for (int i = 0; i < fftSize; ++i)
    {
        const auto historyIndex = (channel.inputWritePos - fftSize + i) & channel.inputMask;
        channel.fftData[static_cast<size_t> (i)] = channel.inputHistory[static_cast<size_t> (historyIndex)]
                                                  * window[static_cast<size_t> (i)];
    }
    for (int i = fftSize; i < fftSize * 2; ++i)
        channel.fftData[static_cast<size_t> (i)] = 0.0f;

    fft->performRealOnlyForwardTransform (channel.fftData.data(), true);

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto real = channel.fftData[static_cast<size_t> (bin * 2)];
        const auto imag = channel.fftData[static_cast<size_t> (bin * 2 + 1)];
        channel.magnitude[static_cast<size_t> (bin)] = std::sqrt (real * real + imag * imag);
        channel.phase[static_cast<size_t> (bin)] = std::atan2 (imag, real);
    }

    if (! channel.havePreviousPhase)
    {
        // First-ever hop: there is no prior phase to derive a frequency
        // estimate from yet, so seed synthesis phase directly from analysis
        // phase (equivalent to a zero-frequency-deviation assumption for
        // this one frame only) and start estimating from the next hop.
        std::copy (channel.phase.begin(), channel.phase.end(), channel.synthesisPhase.begin());
        std::copy (channel.phase.begin(), channel.phase.end(), channel.previousPhase.begin());
        channel.havePreviousPhase = true;
    }
    else
    {
        // Identity phase locking (Laroche-Dolson): find each local magnitude
        // peak, assign every other bin to its nearest peak (a "region of
        // influence"), advance only the peaks' phase by a true-frequency
        // estimate, and lock every other bin in a region to move in
        // lockstep with its peak, keeping the *analysis frame's* relative
        // phase between them. Advancing every bin's phase independently
        // (what an earlier, simpler phase vocoder attempt in this project
        // did) lets bins belonging to the same harmonic partial drift out
        // of relative alignment from one hop to the next - heard as
        // smearing/robotic "phasiness" on real harmonic signal even though
        // each bin's own frequency estimate is individually accurate. Only
        // the peaks' phase relationships to the input actually matter for
        // that; everything else just needs to ride along with its peak.
        int peakCount = 0;
        int lastPeak = -1;
        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto magnitude = channel.magnitude[static_cast<size_t> (bin)];
            const auto isPeak = (bin == 0 || magnitude >= channel.magnitude[static_cast<size_t> (bin - 1)])
                              && (bin == numBins - 1 || magnitude >= channel.magnitude[static_cast<size_t> (bin + 1)]);
            if (isPeak)
            {
                // Every bin between the previous peak and this one belongs
                // to whichever of the two is nearer.
                const auto midpoint = lastPeak < 0 ? 0 : (lastPeak + bin) / 2;
                for (int fill = juce::jmax (0, midpoint); fill < bin; ++fill)
                    channel.regionPeak[static_cast<size_t> (fill)] = lastPeak < 0 ? bin : lastPeak;
                channel.regionPeak[static_cast<size_t> (bin)] = bin;
                lastPeak = bin;
                ++peakCount;
            }
        }
        // Bins after the last peak all belong to it.
        for (int fill = lastPeak + 1; fill < numBins; ++fill)
            channel.regionPeak[static_cast<size_t> (fill)] = lastPeak < 0 ? 0 : lastPeak;
        if (peakCount == 0)
            channel.regionPeak[0] = 0;

        constexpr auto twoPi = juce::MathConstants<float>::twoPi;
        const auto trueFrequencyAt = [&] (int bin) noexcept
        {
            const auto binFrequency = twoPi * static_cast<float> (bin) / static_cast<float> (fftSize);
            const auto expectedAdvance = binFrequency * analysisHop;
            auto delta = channel.phase[static_cast<size_t> (bin)] - channel.previousPhase[static_cast<size_t> (bin)] - expectedAdvance;
            delta -= twoPi * std::floor ((delta + juce::MathConstants<float>::pi) / twoPi);
            return binFrequency + delta / analysisHop;
        };

        // Peaks first, in their own pass: a non-peak bin's assigned peak can
        // be at a *higher* bin index (the next peak along, if it's the
        // nearer of the two straddling a region boundary), so advancing
        // peaks and non-peaks in a single forward sweep would read some
        // peaks' synthesis phase before this hop had updated it.
        for (int bin = 0; bin < numBins; ++bin)
            if (channel.regionPeak[static_cast<size_t> (bin)] == bin)
                channel.synthesisPhase[static_cast<size_t> (bin)] += trueFrequencyAt (bin) * synthesisHop;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const auto peak = channel.regionPeak[static_cast<size_t> (bin)];
            if (bin != peak)
            {
                // Ride along with the assigned peak's synthesis phase,
                // preserving this frame's own phase offset from it - this
                // is the actual "locking" step, applied to every non-peak
                // bin instead of independently accumulating its own phase.
                channel.synthesisPhase[static_cast<size_t> (bin)] = channel.synthesisPhase[static_cast<size_t> (peak)]
                    + (channel.phase[static_cast<size_t> (bin)] - channel.phase[static_cast<size_t> (peak)]);
            }
        }

        std::copy (channel.phase.begin(), channel.phase.end(), channel.previousPhase.begin());
    }

    for (int bin = 0; bin < numBins; ++bin)
    {
        const auto magnitude = channel.magnitude[static_cast<size_t> (bin)];
        const auto synthPhase = channel.synthesisPhase[static_cast<size_t> (bin)];
        channel.fftData[static_cast<size_t> (bin * 2)] = magnitude * std::cos (synthPhase);
        channel.fftData[static_cast<size_t> (bin * 2 + 1)] = magnitude * std::sin (synthPhase);
    }

    fft->performRealOnlyInverseTransform (channel.fftData.data());

    const auto hopSamples = juce::jmax (1, juce::roundToInt (synthesisHop));
    for (int i = 0; i < fftSize; ++i)
    {
        const auto idx = static_cast<size_t> ((channel.writeCursor + i) & channel.overlapMask);
        const auto windowValue = window[static_cast<size_t> (i)];
        channel.overlapAdd[idx] += channel.fftData[static_cast<size_t> (i)] * windowValue;
        channel.overlapNorm[idx] += windowValue * windowValue;
    }
    channel.writeCursor += hopSamples;
}

float Processor::PitchStage::SpectralShifter::processSample (int channelIndex, float input, float ratio) noexcept
{
    auto& channel = channels[static_cast<size_t> (channelIndex)];

    channel.inputHistory[static_cast<size_t> (channel.inputWritePos)] = input;
    channel.inputWritePos = (channel.inputWritePos + 1) & channel.inputMask;

    if (--channel.samplesUntilAnalysis <= 0)
    {
        runHop (channel, ratio);
        const auto clampedRatio = juce::jlimit (0.125f, 4.0f, ratio);
        const auto analysisHop = static_cast<float> (baseHop) / std::sqrt (clampedRatio);
        channel.samplesUntilAnalysis = juce::jmax (1, juce::roundToInt (analysisHop));
    }

    // The reader must always stay at least a full window behind the writer:
    // a position less than fftSize behind the write cursor may still
    // receive further overlap-add contributions from frames not yet
    // written, so reading it now would divide by a partial (too-small)
    // normalisation sum. Clamping here (rather than assuming it never
    // happens) matters specifically because the live ratio can drift
    // continuously, so the reader/writer gap isn't perfectly constant.
    const auto safeReadLimit = static_cast<double> (channel.writeCursor - fftSize);
    channel.readCursor = juce::jmin (channel.readCursor, safeReadLimit);

    const auto base = static_cast<int64_t> (std::floor (channel.readCursor));
    const auto fraction = static_cast<float> (channel.readCursor - static_cast<double> (base));
    const auto index0 = static_cast<size_t> (base & channel.overlapMask);
    const auto index1 = static_cast<size_t> ((base + 1) & channel.overlapMask);

    constexpr float epsilon = 1.0e-6f;
    const auto sample0 = channel.overlapAdd[index0] / juce::jmax (epsilon, channel.overlapNorm[index0]);
    const auto sample1 = channel.overlapAdd[index1] / juce::jmax (epsilon, channel.overlapNorm[index1]);
    const auto output = sample0 + fraction * (sample1 - sample0);

    channel.readCursor += static_cast<double> (ratio);

    // Lazily zero positions well behind the reader so stale contributions
    // from a previous lap around the ring don't corrupt future frames once
    // the writer wraps back around to reuse the same physical slots.
    const auto clearLimit = base - fftSize;
    while (channel.clearedUpTo < clearLimit)
    {
        const auto idx = static_cast<size_t> (channel.clearedUpTo & channel.overlapMask);
        channel.overlapAdd[idx] = 0.0f;
        channel.overlapNorm[idx] = 0.0f;
        ++channel.clearedUpTo;
    }

    return output;
}

void Processor::PitchStage::prepare (double sampleRate, int channelCount)
{
    currentSampleRate = juce::jmax (1.0, sampleRate);

    // 35ms nominal: long enough to contain a full cycle of low guitar
    // fundamentals (low E ~= 82Hz => 12ms period) with headroom, short enough
    // that the grain-crossfade rate (roughly 1/grainPeriod, so ~28Hz) stays
    // below where it would itself be heard as a warble. The *actual* grain
    // period is retuned periodically to a whole multiple of the detected
    // input pitch (see updatePitchSynchronousGrainPeriod) and never exceeds
    // this nominal value - see that method's comment for why - so sizing the
    // buffer from it now stays valid for the buffer's whole lifetime.
    maximumGrainPeriodSamples = 0.035f * static_cast<float> (currentSampleRate);
    grainPeriodSamples[0] = maximumGrainPeriodSamples;
    grainPeriodSamples[1] = maximumGrainPeriodSamples;
    grainBankCrossfadeStep = 1.0f / (0.040f * static_cast<float> (currentSampleRate));
    samplesUntilPitchUpdate = 0;

    // Grains can lag up to one full period behind the write head; size the
    // ring buffer with plenty of headroom beyond that.
    const auto delayBufferSize = juce::nextPowerOfTwo (juce::roundToInt (maximumGrainPeriodSamples) * 4 + 8);
    delayBufferMask = delayBufferSize - 1;

    channels.resize (static_cast<size_t> (juce::jmax (1, channelCount)));
    for (auto& channel : channels)
        channel.delayBuffer.assign (static_cast<size_t> (delayBufferSize), 0.0f);

    // 80ms: long enough to average over several cycles even for low guitar
    // fundamentals, short enough to track real level changes (picking
    // dynamics, mode switches) without audible pumping.
    envelopeCoefficient = 1.0f - std::exp (-1.0f / (0.08f * static_cast<float> (currentSampleRate)));

    semitones.reset (currentSampleRate, 0.030);
    dryBlend.reset (currentSampleRate, 0.020);
    outputGain.reset (currentSampleRate, 0.020);
    enabledMix.reset (currentSampleRate, 0.008);

    spectralShifter.prepare (currentSampleRate, channelCount);
    // ~65ms: long enough that a mode-selector switch across the Whammy/Shift
    // Up boundary can't click (matches the order of the existing grain-bank
    // crossfade duration), short enough that it isn't sluggish for what is,
    // after all, a discrete user action (turning a knob), not automation.
    hybridBlendProgressStep = 1.0f / (0.065f * static_cast<float> (currentSampleRate));

    reset();
}

void Processor::PitchStage::reset()
{
    grainPeriodSamples[0] = maximumGrainPeriodSamples;
    grainPeriodSamples[1] = maximumGrainPeriodSamples;
    activeGrainBank = 0;
    grainBankTransitionActive = false;
    grainBankCrossfade = 0.0f;
    samplesUntilPitchUpdate = 0;
    pendingConfirmPeriod = -1.0f;
    pendingConfirmCount = 0;
    lastConfirmedDetectedPeriod = -1.0f;

    for (auto& channel : channels)
    {
        std::fill (channel.delayBuffer.begin(), channel.delayBuffer.end(), 0.0f);
        channel.writePosition = 0;
        // Evenly spaced (quarter-period apart) so a wrap in any one grain is
        // always masked by the others being away from their own wrap point.
        for (int bank = 0; bank < 2; ++bank)
            for (int grain = 0; grain < 4; ++grain)
                channel.grainDelay[bank][grain] = grainPeriodSamples[bank]
                                                        * (static_cast<float> (grain) / 4.0f);
        channel.dryEnvelope = 0.0f;
        channel.wetEnvelope = 0.0f;
        channel.antiAliasState1 = 0.0f;
        channel.antiAliasState2 = 0.0f;
    }

    semitones.setCurrentAndTargetValue (0.0f);
    dryBlend.setCurrentAndTargetValue (0.0f);
    outputGain.setCurrentAndTargetValue (1.0f);
    enabledMix.setCurrentAndTargetValue (0.0f);
    frameRatio = 1.0f;
    frameAntiAliasCoefficient = 1.0f;

    spectralShifter.reset();
    hybridEngineTarget = false;
    hybridBlendProgress = 0.0f;
}

void Processor::PitchStage::setTarget (const ModeModel& model, float pedalPosition, bool enabled,
                                       bool usesHybridEngine) noexcept
{
    const auto pedal = juce::jlimit (0.0f, 1.0f, pedalPosition);
    semitones.setTargetValue (model.toeSemitones + pedal * (model.heelSemitones - model.toeSemitones));

    // The stable level-staircase median from the hardware profiling run is a
    // real part of each mode's response.  It is applied after the shifter's
    // own light level normalisation, while measured recording latency remains
    // metadata-only and is never inserted into the signal path.
    dryBlend.setTargetValue (model.mixesDry ? 1.0f : 0.0f);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (model.outputGainDb));
    enabledMix.setTargetValue (enabled ? 1.0f : 0.0f);
    hybridEngineTarget = usesHybridEngine;
}

float Processor::PitchStage::readInterpolated (const ChannelBuffer& channel, float delayInSamples) const noexcept
{
    const auto readPosition = static_cast<float> (channel.writePosition) - delayInSamples;
    const auto base = static_cast<int> (std::floor (readPosition));
    const auto fraction = readPosition - static_cast<float> (base);
    const auto first = channel.delayBuffer[static_cast<size_t> (base & delayBufferMask)];
    const auto second = channel.delayBuffer[static_cast<size_t> ((base + 1) & delayBufferMask)];
    return first + fraction * (second - first);
}

float Processor::PitchStage::estimatePeriodSamples (const ChannelBuffer& channel, float& confidence) const noexcept
{
    confidence = 0.0f;
    // Covers roughly 63Hz-1100Hz at 44.1kHz: below a dropped-tuning low
    // string, above typical bends/harmonics. Widened slightly past standard
    // guitar range on both ends for headroom rather than tuned precisely,
    // since a wrong-by-an-octave estimate here just picks a different (still
    // valid) grain-period multiple, not a wrong pitch shift - the shift
    // amount is still set entirely by semitones/frameRatio elsewhere.
    constexpr int minPeriod = 40;
    constexpr int maxPeriod = 700;
    constexpr int analysisLength = 256;

    if (channel.delayBuffer.size() < static_cast<size_t> (maxPeriod + analysisLength))
        return -1.0f;

    // Integer delays only (no readInterpolated/fractional interpolation
    // needed for a coarse pitch estimate), and the reference block is
    // precomputed once instead of being re-read for every candidate lag -
    // it doesn't depend on lag at all, and re-reading it per lag was half
    // the cost of the original version of this method for no benefit.
    const auto base = channel.writePosition;
    std::array<float, analysisLength> reference {};
    for (int i = 0; i < analysisLength; ++i)
        reference[static_cast<size_t> (i)] = channel.delayBuffer[static_cast<size_t> ((base - i) & delayBufferMask)];

    const auto correlationAt = [&] (int lag) noexcept
    {
        float sumProduct = 0.0f, sumSquaresA = 0.0f, sumSquaresB = 0.0f;
        for (int i = 0; i < analysisLength; ++i)
        {
            const auto a = reference[static_cast<size_t> (i)];
            const auto b = channel.delayBuffer[static_cast<size_t> ((base - i - lag) & delayBufferMask)];
            sumProduct += a * b;
            sumSquaresA += a * a;
            sumSquaresB += b * b;
        }
        return sumProduct / (std::sqrt (sumSquaresA * sumSquaresB) + 1.0e-9f);
    };

    // Normalised correlation confidence threshold: below this, treat the
    // input as not clearly pitched (silence, noise, a complex chord) and
    // leave the grain period wherever it last was rather than chase a weak,
    // unreliable estimate.
    float bestPeriod = -1.0f;
    float bestScore = 0.35f;

    // Coarse pass every 2 samples across the full range, then refine +/-1
    // around the best coarse lag - evaluating every single lag in that range
    // at full analysisLength was the dominant cost of this method, run every
    // ~23ms on the audio thread; halving both the lag step and the analysis
    // length (plus removing the redundant reference re-reads above) cuts the
    // total work roughly eightfold; period precision only needs to be good
    // enough to pick the right small integer multiplier, not sample-exact.
    for (int lag = minPeriod; lag <= maxPeriod; lag += 2)
    {
        const auto score = correlationAt (lag);
        if (score > bestScore)
        {
            bestScore = score;
            bestPeriod = static_cast<float> (lag);
        }
    }

    if (bestPeriod > 0.0f)
    {
        const auto coarseLag = static_cast<int> (bestPeriod);
        for (int lag = juce::jmax (minPeriod, coarseLag - 1); lag <= juce::jmin (maxPeriod, coarseLag + 1); ++lag)
        {
            if (lag == coarseLag)
                continue;

            const auto score = correlationAt (lag);
            if (score > bestScore)
            {
                bestScore = score;
                bestPeriod = static_cast<float> (lag);
            }
        }
    }

    if (bestPeriod > 0.0f)
        confidence = bestScore;

    return bestPeriod;
}

void Processor::PitchStage::updatePitchSynchronousGrainPeriod() noexcept
{
    if (channels.empty())
        return;

    float confidence = 0.0f;
    const auto detectedPeriod = estimatePeriodSamples (channels[0], confidence);
    if (detectedPeriod <= 0.0f)
    {
        pendingConfirmCount = 0;
        return;
    }

    // The multiplier must itself be a multiple of the grain count (4): the
    // four grains are spaced a quarter-period-of-the-grain apart, and unless
    // grainPeriod/4 is *also* a whole multiple of the detected pitch period,
    // adjacent grains still overlap at an arbitrary phase offset even though
    // the wrap point now aligns - which beats audibly between wraps exactly
    // like the original, un-synchronised version did. Forcing every quarter
    // of the grain period onto a period boundary is what actually removes
    // the mismatch between *every* pair of grains, not just at the wrap.
    constexpr int grainCount = 4;
    const auto quarterMultiplier = juce::jlimit (1, 4,
        juce::roundToInt (maximumGrainPeriodSamples / (static_cast<float> (grainCount) * detectedPeriod)));
    const auto multiplier = grainCount * quarterMultiplier;
    const auto newGrainPeriod = detectedPeriod * static_cast<float> (multiplier);

    if (grainBankTransitionActive || std::abs (newGrainPeriod - grainPeriodSamples[activeGrainBank]) < 1.0f)
    {
        pendingConfirmCount = 0;
        return;
    }

    // Require the same candidate period to come up several checks in a row
    // before acting on it - a single noisy window shouldn't be enough to
    // force a retune, only a period that's actually held up. How many checks
    // are required depends on whether the input's own detected period has
    // moved well away from the last note we locked onto (a real note change -
    // in a phrase, notes change every few hundred ms, and a slow lock here is
    // heard as a wrong/dissonant pitch for the first part of every new note)
    // or stayed close to it (vibrato/harmonics briefly outscoring the
    // fundamental on the *same* note, which is exactly the noise the original
    // five-check hysteresis was added to filter out). Comparing against the
    // raw detected period rather than the derived grain period matters here:
    // the grain period is deliberately kept near a constant ~35ms across
    // every note (see the quarterMultiplier scaling above), so it does not
    // itself reflect how far the pitch has actually moved.
    const auto relativeMove = lastConfirmedDetectedPeriod > 0.0f
        ? std::abs (detectedPeriod - lastConfirmedDetectedPeriod) / lastConfirmedDetectedPeriod
        : 1.0f;
    constexpr float noteChangeThreshold = 0.06f; // ~1 semitone
    // A big move alone isn't enough to fast-track: right after a real note
    // onset, the analysis window still spans both the old and new note for a
    // few checks, and that transient, contaminated window can also look like
    // a big move but scores lower on correlation than a clean single pitch.
    // Only skip the slow path when the estimate is both far from the last
    // note *and* confidently periodic.
    constexpr float highConfidence = 0.5f;
    const auto requiredConfirmations = (relativeMove > noteChangeThreshold && confidence > highConfidence) ? 2 : 5;
    if (std::abs (newGrainPeriod - pendingConfirmPeriod) < 1.0f)
    {
        ++pendingConfirmCount;
    }
    else
    {
        pendingConfirmPeriod = newGrainPeriod;
        pendingConfirmCount = 1;
    }

    if (pendingConfirmCount < requiredConfirmations)
        return;

    pendingConfirmCount = 0;
    lastConfirmedDetectedPeriod = detectedPeriod;

    const auto inactiveBank = 1 - activeGrainBank;
    grainPeriodSamples[inactiveBank] = newGrainPeriod;
    for (auto& channel : channels)
        for (int grain = 0; grain < 4; ++grain)
        {
            const auto phase = channel.grainDelay[activeGrainBank][grain]
                             / grainPeriodSamples[activeGrainBank];
            channel.grainDelay[inactiveBank][grain] = phase * newGrainPeriod;
        }

    grainBankCrossfade = 0.0f;
    grainBankTransitionActive = true;
}

float Processor::PitchStage::processSample (int channelIndex, float input) noexcept
{
    jassert (channelIndex >= 0 && channelIndex < static_cast<int> (channels.size()));
    auto& channel = channels[static_cast<size_t> (channelIndex)];

    channel.antiAliasState1 += frameAntiAliasCoefficient * (input - channel.antiAliasState1);
    channel.antiAliasState2 += frameAntiAliasCoefficient * (channel.antiAliasState1 - channel.antiAliasState2);
    channel.delayBuffer[static_cast<size_t> (channel.writePosition)] = channel.antiAliasState2;

    // Four grains, each reading the delay buffer at frameRatio samples per
    // sample (faster than real time raises pitch, slower lowers it), windowed
    // by a raised-cosine that is zero exactly when the grain wraps back
    // toward the write head to stay in bounds. Evenly offsetting four grains
    // by a quarter period each means their windows sum to a constant (2.0,
    // normalised below) at every instant, so any one grain's wrap is masked
    // by the other three - *and*, because grainPeriodSamples is tuned to a
    // whole multiple of the detected input period and the grains are exactly
    // a quarter of that period apart (updatePitchSynchronousGrainPeriod),
    // every pair of overlapping grains reads the input at a phase offset that
    // is itself a whole number of periods, i.e. no offset at all. Without
    // that, two grains reading the same periodic content at a fixed but
    // arbitrary phase offset phase-modulate the crossfade - heard as a
    // continuous amplitude "wobble" even while the average pitch and level
    // both look fine, on *every* mode, not just extreme ratios. Each wrap's
    // destination is wrapped by the exact same period for every grain.  An
    // independent per-grain splice search used to perturb their quarter-period
    // spacing, creating strong modulation sidebands on non-octave upshifts.
    const auto renderBank = [this, &channel] (int bank) noexcept
    {
        const auto period = grainPeriodSamples[bank];
        auto wet = 0.0f;
        for (auto& delay : channel.grainDelay[bank])
        {
            const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * delay / period);
            wet += readInterpolated (channel, delay) * window;

            delay += (1.0f - frameRatio);
            if (delay < 0.0f)
                delay += period;
            else if (delay >= period)
                delay -= period;
        }
        return wet * 0.5f; // four quarter-spaced Hann grains sum to 2.0
    };

    auto wet = renderBank (activeGrainBank);
    if (grainBankTransitionActive)
    {
        const auto nextWet = renderBank (1 - activeGrainBank);
        // Squared-sine/cosine weights sum to one and reach zero slope at both
        // ends, so starting or finishing a bank transition cannot click.
        const auto angle = grainBankCrossfade * juce::MathConstants<float>::halfPi;
        const auto nextWeight = std::sin (angle) * std::sin (angle);
        wet = wet * (1.0f - nextWeight) + nextWet * nextWeight;
    }

    // Modern spectral engine, crossfaded in for Whirl's Whammy group and
    // Drop Tune's plain Shift Up arc (see Processor::process for the mode
    // ranges): those sections are wet-only, so this granular engine's own
    // crossfade character is fully audible with nothing to dilute it, and at
    // Whammy's widest ratios its grain-period multiplier is stressed. This
    // always runs (see SpectralShifter's own class comment for why it must
    // never be gated).
    //
    // This crossfades directly to the spectral engine's own output rather
    // than summing a "character" dose of the granular signal into it - an
    // earlier version did that, and measurably caused destructive
    // cancellation: both engines independently pitch-shift the same input
    // but with different internal group delay, so linearly summing their
    // waveforms is summing two differently-phased copies of the same
    // signal, which can null out large amounts of level rather than add
    // character - worst at Dive Bomb, where the summed level came out
    // *quieter* than either engine alone (confirmed by direct RMS
    // measurement, not inferred). Crossfading to one coherent signal instead
    // of summing two independently-phased ones has no such risk.
    const auto wetSpectral = spectralShifter.processSample (channelIndex, input, frameRatio);

    // Same zero-end-slope squared-sine shape as the grain-bank crossfade
    // above, so a mode-selector switch across the engine boundary can't
    // click either.
    const auto hybridAngle = hybridBlendProgress * juce::MathConstants<float>::halfPi;
    const auto hybridWeight = std::sin (hybridAngle) * std::sin (hybridAngle);
    wet = wet + hybridWeight * (wetSpectral - wet);

    const auto branchNormalisation = 1.0f / std::sqrt (1.0f + frameDryBlend * frameDryBlend);
    const auto effected = (wet + input * frameDryBlend) * branchNormalisation;

    // Light adaptive trim: the grain crossfade alone is already unity, but
    // mixing in the dry branch for harmony modes can still drift a little
    // depending on how correlated the shifted and dry voices happen to be.
    // A much tighter range than a from-scratch adaptive gain would need,
    // since there's no large systematic error left to correct here.
    channel.dryEnvelope += envelopeCoefficient * (input * input - channel.dryEnvelope);
    channel.wetEnvelope += envelopeCoefficient * (effected * effected - channel.wetEnvelope);
    constexpr float epsilon = 1.0e-9f;
    const auto levelMatchGain = juce::jlimit (0.5f, 2.0f,
        std::sqrt ((channel.dryEnvelope + epsilon) / (channel.wetEnvelope + epsilon)));

    return input + frameEnabledMix * (effected * levelMatchGain * frameOutputGain - input);
}

void Processor::PitchStage::finishSample() noexcept
{
    for (auto& channel : channels)
        channel.writePosition = (channel.writePosition + 1) & delayBufferMask;

    // Roughly every 11.6ms: frequent enough that even the slow (same-note
    // jitter) hysteresis path settles well within a typical note's duration,
    // infrequent enough that the (cheap but non-trivial) autocorrelation
    // search isn't running every sample for no benefit.
    if (--samplesUntilPitchUpdate <= 0)
    {
        updatePitchSynchronousGrainPeriod();
        samplesUntilPitchUpdate = 512;
    }

    if (grainBankTransitionActive)
    {
        grainBankCrossfade += grainBankCrossfadeStep;
        if (grainBankCrossfade >= 1.0f)
        {
            activeGrainBank = 1 - activeGrainBank;
            grainBankCrossfade = 0.0f;
            grainBankTransitionActive = false;
        }
    }

    if (hybridEngineTarget)
        hybridBlendProgress = juce::jmin (1.0f, hybridBlendProgress + hybridBlendProgressStep);
    else
        hybridBlendProgress = juce::jmax (0.0f, hybridBlendProgress - hybridBlendProgressStep);

    const auto currentSemitones = semitones.getNextValue();
    frameRatio = std::pow (2.0f, currentSemitones / 12.0f);
    if (frameRatio <= 1.0f)
    {
        frameAntiAliasCoefficient = 1.0f;
    }
    else
    {
        // Keep the shifted spectrum just below Nyquist.  A smooth one-pole
        // coefficient (cascaded twice per channel above) avoids coefficient
        // zippering while the treadle moves.
        const auto normalisedCutoff = 0.45f / frameRatio;
        frameAntiAliasCoefficient = 1.0f
                                  - std::exp (-juce::MathConstants<float>::twoPi * normalisedCutoff);
    }
    frameDryBlend = dryBlend.getNextValue();
    frameOutputGain = outputGain.getNextValue();
    frameEnabledMix = enabledMix.getNextValue();
}

void Processor::DropTuneStage::prepare (double sampleRate, int requestedMaximumBlockSize, int channelCount)
{
    currentSampleRate = juce::jmax (1.0, sampleRate);
    preparedChannels = juce::jmax (1, channelCount);
    maximumBlockSize = juce::jmax (1, requestedMaximumBlockSize);

    // About 85 ms of analysis with 8x overlap is the lowest-latency setting
    // which still resolves a low-E fundamental accurately.  The denser hop
    // keeps pick attacks from being represented by only one or two frames;
    // the library's default quality preset is longer still and felt detached
    // from live playing.
    const auto windowSamples = juce::jmax (2048, juce::roundToInt (currentSampleRate * (4096.0 / 48000.0)));
    shifter.configure (preparedChannels, windowSamples, windowSamples / 8, false);
    latencySamples = shifter.inputLatency() + shifter.outputLatency();

    shiftedBuffer.setSize (preparedChannels, maximumBlockSize, false, true, false);
    inputPointers.resize (static_cast<size_t> (preparedChannels));
    outputPointers.resize (static_cast<size_t> (preparedChannels));
    channels.resize (static_cast<size_t> (preparedChannels));
    for (auto& channel : channels)
        channel.dryDelay.assign (static_cast<size_t> (juce::jmax (1, latencySamples)), 0.0f);

    semitones.reset (currentSampleRate, 0.030);
    dryBlend.reset (currentSampleRate, 0.020);
    outputGain.reset (currentSampleRate, 0.020);
    enabledMix.reset (currentSampleRate, 0.008);
    reset();
}

void Processor::DropTuneStage::reset()
{
    shifter.reset();
    shiftedBuffer.clear();
    for (auto& channel : channels)
        std::fill (channel.dryDelay.begin(), channel.dryDelay.end(), 0.0f);
    dryDelayPosition = 0;

    semitones.setCurrentAndTargetValue (0.0f);
    dryBlend.setCurrentAndTargetValue (0.0f);
    outputGain.setCurrentAndTargetValue (1.0f);
    enabledMix.setCurrentAndTargetValue (0.0f);
}

void Processor::DropTuneStage::process (juce::AudioBuffer<float>& buffer, const ModeModel& model,
                                        bool enabled) noexcept
{
    const auto channelsToProcess = juce::jmin (preparedChannels, buffer.getNumChannels());
    if (channelsToProcess <= 0 || buffer.getNumSamples() <= 0)
        return;

    semitones.setTargetValue (model.toeSemitones);
    dryBlend.setTargetValue (model.mixesDry ? 1.0f : 0.0f);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (model.outputGainDb));
    enabledMix.setTargetValue (enabled ? 1.0f : 0.0f);

    for (int offset = 0; offset < buffer.getNumSamples(); offset += maximumBlockSize)
    {
        const auto count = juce::jmin (maximumBlockSize, buffer.getNumSamples() - offset);
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            inputPointers[static_cast<size_t> (channel)] = buffer.getReadPointer (channel, offset);
            outputPointers[static_cast<size_t> (channel)] = shiftedBuffer.getWritePointer (channel);
        }

        // The selector is discrete, but hosts may automate it.  Move the map
        // over 30 ms and use this chunk's midpoint so a mode change cannot
        // throw the spectral peaks abruptly to unrelated bins.
        const auto startSemitones = semitones.getCurrentValue();
        const auto endSemitones = semitones.skip (count);
        const auto mappedSemitones = 0.5f * (startSemitones + endSemitones);

        // Keep the interval exact across the tonal range.  The optional
        // high-frequency tonality limit sounds useful on hiss in isolation,
        // but it also bends upper guitar partials by a different amount and
        // therefore makes the result chorusy/unstable.
        shifter.setTransposeSemitones (mappedSemitones);
        shifter.process (inputPointers.data(), count, outputPointers.data(), count);

        for (int sample = 0; sample < count; ++sample)
        {
            const auto dryAmount = dryBlend.getNextValue();
            const auto measuredGain = outputGain.getNextValue();
            const auto effectAmount = enabledMix.getNextValue();
            const auto mixNormalisation = 1.0f / std::sqrt (1.0f + dryAmount * dryAmount);

            for (int channel = 0; channel < channelsToProcess; ++channel)
            {
                const auto input = buffer.getSample (channel, offset + sample);
                auto& state = channels[static_cast<size_t> (channel)];
                auto& delayedSlot = state.dryDelay[static_cast<size_t> (dryDelayPosition)];
                const auto delayedDry = delayedSlot;
                delayedSlot = input;

                const auto shifted = shiftedBuffer.getSample (channel, sample);
                const auto effected = (shifted + delayedDry * dryAmount) * mixNormalisation * measuredGain;
                buffer.setSample (channel, offset + sample,
                                  delayedDry + effectAmount * (effected - delayedDry));
            }

            if (++dryDelayPosition >= latencySamples)
                dryDelayPosition = 0;
        }
    }
}

void Processor::prepare (double sampleRate, int maximumBlockSize, int channelCount)
{
    preparedChannels = juce::jmax (1, channelCount);
    // Both stages only ever see the mono-summed signal (see process()), so
    // they're always prepared for one channel regardless of the host's
    // actual channel count.
    whirlStage.prepare (sampleRate, 1);
    dropTuneStage.prepare (sampleRate, maximumBlockSize, 1);
    monoBuffer.setSize (1, juce::jmax (1, maximumBlockSize), false, false, true);
}

void Processor::reset()
{
    whirlStage.reset();
    dropTuneStage.reset();
}

int Processor::getLatencySamples() const noexcept
{
    return dropTuneStage.getLatencySamples();
}

void Processor::process (juce::AudioBuffer<float>& buffer, const Parameters& parameters) noexcept
{
    const auto channelsToProcess = juce::jmin (preparedChannels, buffer.getNumChannels());
    const auto sampleCount = buffer.getNumSamples();
    if (channelsToProcess <= 0 || sampleCount <= 0)
        return;

    // The real Whammy DT has one mono instrument input. Average whatever the
    // host actually sent down to that single signal before either stage
    // sees it - processing left/right independently would be incoherent for
    // genuinely different stereo content (the granular engine's pitch
    // tracking and grain phase, and the spectral/Signalsmith engines'
    // analysis, would each run on unrelated content per channel), and for
    // the common case of a mono guitar signal merely duplicated across both
    // channels it's simply redundant work for an identical result either
    // way. The result is copied back out to every channel below.
    monoBuffer.setSize (1, sampleCount, false, false, true);
    const auto inverseChannelCount = 1.0f / static_cast<float> (channelsToProcess);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        float sum = 0.0f;
        for (int channel = 0; channel < channelsToProcess; ++channel)
            sum += buffer.getSample (channel, sample);
        monoBuffer.setSample (0, sample, sum * inverseChannelCount);
    }

    // Whirl retains the measured granular/spectral hybrid, but only across
    // the ratio range the phase vocoder's geometric hop scaling can actually
    // track reliably. Beyond roughly two octaves downward (modes 17/18, -24
    // and -36 semitones) the analysis hop it needs grows to within a few
    // percent of the whole FFT window, which breaks the phase-unwrapping
    // step's implicit assumption that a bin's true frequency can't deviate
    // from its centre by more than about half a bin per analysis hop -
    // confirmed directly (not assumed) via real-hardware validation showing
    // a consistent ~1.2-2.2 semitone flat bias specifically on those two
    // modes and nowhere else, including mode 9 (+24 semitones, the
    // symmetric upward extreme): its analysis hop *shrinks* rather than
    // grows as the ratio moves away from 1.0, which only improves
    // phase-unwrapping headroom, so it measures accurately and stays in the
    // hybrid range. Drop Tune is processed afterwards, as one block, by its
    // dedicated polyphonic engine.
    const auto whirlUsesHybrid = parameters.whirlMode >= 9 && parameters.whirlMode <= 16;

    whirlStage.setTarget (getWhirlModeModel (parameters.whirlMode), parameters.pedalPosition,
                          parameters.whirlEnabled, whirlUsesHybrid);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        auto value = monoBuffer.getSample (0, sample);
        value = whirlStage.processSample (0, value);
        monoBuffer.setSample (0, sample, value);

        whirlStage.finishSample();
    }

    dropTuneStage.process (monoBuffer, getDropTuneModeModel (parameters.dropTuneMode),
                           parameters.dropTuneEnabled);

    for (int channel = 0; channel < channelsToProcess; ++channel)
        buffer.copyFrom (channel, 0, monoBuffer, 0, 0, sampleCount);
}
}
