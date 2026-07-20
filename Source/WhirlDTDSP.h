#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#endif
#include <signalsmith-stretch/signalsmith-stretch.h>
#if defined (__clang__)
 #pragma clang diagnostic pop
#endif

#include <memory>
#include <vector>

namespace WhirlDTDSP
{
    struct ModeModel
    {
        float toeSemitones = 0.0f;
        float heelSemitones = 0.0f;
        // outputGainDb is the stable level-staircase median and is part of the
        // model. measuredLatencySamplesAt44k1 remains diagnostic metadata only:
        // measurement/recording delay is never inserted into the signal path.
        float outputGainDb = 0.0f;
        float measuredLatencySamplesAt44k1 = 0.0f;
        bool mixesDry = false;
        bool directlyMeasured = true;
        const char* measurementKey = "";
    };

    const ModeModel& getWhirlModeModel (int modeIndex) noexcept;
    const ModeModel& getDropTuneModeModel (int modeIndex) noexcept;

    struct Parameters
    {
        int whirlMode = 0;
        bool whirlEnabled = false;
        float pedalPosition = 0.0f; // 0 = toe, 1 = heel (matches the UI treadle)
        int dropTuneMode = 0;
        bool dropTuneEnabled = false;
    };

    class Processor
    {
    public:
        void prepare (double sampleRate, int maximumBlockSize, int channelCount);
        void reset();
        void process (juce::AudioBuffer<float>& buffer, const Parameters& parameters) noexcept;
        int getLatencySamples() const noexcept;

    private:
        // Time-domain granular pitch shifter: four overlapping read pointers
        // ("grains") walk back through a short recent-history delay buffer at
        // `pitchRatio` samples per sample - reading faster than real time
        // raises pitch, slower lowers it - each windowed by a raised-cosine
        // that reaches zero exactly when a grain has to jump (wrap) back to
        // stay within the buffer. The four grains are offset a quarter-period
        // apart, so at least one is always near its window peak while another
        // is at its near-silent wrap point, which hides the jump.
        //
        // This replaces an earlier naive STFT phase-vocoder implementation.
        // That vocoder was accurate on pure test tones but produced heavy
        // "phasiness"/smearing artifacts on real (harmonic, transient) guitar
        // signal - a well-known weakness of *unlocked* frequency-domain phase
        // reconstruction. Because this approach only ever reads and
        // crossfades real recent samples, it has no equivalent failure mode
        // for polyphonic or transient input, and the crossfade is
        // self-normalising (the four windows sum to a constant at every
        // instant), so it needs no per-mode gain correction to stay
        // level-matched. This engine alone still handles Harmony, Detune, and
        // Drop Tune's Shift Down arc.
        //
        // Whirl's Whammy group and Drop Tune's plain Shift Up arc instead
        // blend this granular output with SpectralShifter below - a properly
        // *phase-locked* STFT vocoder - since those sections are wet-only
        // (nothing dilutes the granular engine's own crossfade character) and
        // reach much wider ratios, where the earlier phasiness fix doesn't
        // apply (identity phase locking, not the absence of any spectral
        // engine, is what actually solves phasiness) - see SpectralShifter's
        // own comment and PitchStage::processSample for the blend.
        class PitchStage
        {
        public:
            void prepare (double sampleRate, int channelCount);
            void reset();

            // usesHybridEngine selects the modern spectral engine (blended
            // with a minority of this class's own granular output for
            // character) for Whirl's Whammy group and Drop Tune's plain
            // Shift Up arc - see Processor::process for the exact mode
            // ranges. It is not inferred from ModeModel::mixesDry, which
            // doesn't disambiguate Detune (also mixesDry) from those two
            // sections.
            void setTarget (const ModeModel& model, float pedalPosition, bool enabled,
                            bool usesHybridEngine) noexcept;
            float processSample (int channel, float input) noexcept;
            void finishSample() noexcept;

        private:
            struct ChannelBuffer
            {
                std::vector<float> delayBuffer;
                int writePosition = 0;
                float grainDelay[2][4] = {};

                // Real-time level match, per channel (so hard-panned/decorrelated
                // stereo content is tracked and corrected independently). The
                // grain crossfade is self-normalising on its own, but mixing in
                // the dry branch for harmony modes still needs a light trim -
                // see the comment in setTarget().
                float dryEnvelope = 0.0f;
                float wetEnvelope = 0.0f;

                // Two gently cascaded one-pole sections suppress content which
                // would be shifted above Nyquist and fold back as metallic,
                // inharmonic aliases.  They are transparent at unity/downshift.
                float antiAliasState1 = 0.0f;
                float antiAliasState2 = 0.0f;
            };

            float readInterpolated (const ChannelBuffer& channel, float delayInSamples) const noexcept;

            // Estimates the input's own pitch period via normalised
            // autocorrelation of recent history in `channel` (typically
            // channels[0], used as the shared reference for both channels).
            // Returns the period in samples, or a negative value if nothing in
            // the guitar frequency range correlates with enough confidence
            // (silence, noise, or a very complex chord) - the caller should
            // leave the grain period as-is in that case. `confidence` receives
            // the normalised correlation score at the winning lag: right after
            // a note onset, the analysis window still spans both the old and
            // new note for a few checks, which reliably scores lower than a
            // clean, single-pitch window even when it clears the accept
            // threshold - the caller uses this to tell a genuine, clean note
            // change apart from a transient, contaminated estimate.
            float estimatePeriodSamples (const ChannelBuffer& channel, float& confidence) const noexcept;

            // Chooses a pitch-synchronous period for an inactive grain bank,
            // then crossfades to it.  Both banks keep reading at the requested
            // pitch ratio throughout, avoiding both a hard read-head jump and
            // the temporary pitch bend caused by rescaling live read heads.
            void updatePitchSynchronousGrainPeriod() noexcept;

            // Modern spectral (phase-locked phase vocoder) pitch engine.
            // Used only for Whirl's Whammy group and Drop Tune's plain Shift
            // Up arc: the granular engine above already tracks pitch
            // accurately there too, but with no dry signal to dilute it
            // (both sections are wet-only), its raw grain-crossfade
            // character is fully audible, and at Whammy's widest ratios its
            // own grain-period multiplier (see updatePitchSynchronousGrainPeriod)
            // gets stressed. This runs unconditionally every sample for
            // every stage regardless of the active mode (see processSample)
            // so it is always warm - it never needs a cold start right at a
            // mode-switch boundary, only its blend weight into the output
            // changes there.
            class SpectralShifter
            {
            public:
                void prepare (double sampleRate, int channelCount);
                void reset();

                // ratio is the already-smoothed, live pitch ratio
                // (2^(semitones/12)); this is read continuously, not
                // snapshotted per mode, since the expression treadle sweeps
                // every Whammy mode's ratio from 1.0 to its extreme in real
                // time.
                float processSample (int channelIndex, float input, float ratio) noexcept;

            private:
                static constexpr int fftOrder = 11;
                static constexpr int fftSize = 1 << fftOrder;      // 2048
                static constexpr int numBins = fftSize / 2 + 1;    // 1025
                static constexpr int baseHop = fftSize / 4;        // 512, ~11.6ms @44.1kHz

                struct ChannelState
                {
                    // Circular history of raw input samples, long enough to
                    // always contain a full analysis window.
                    std::vector<float> inputHistory;
                    int inputMask = 0;
                    int inputWritePos = 0;
                    int samplesUntilAnalysis = 0;

                    std::vector<float> previousPhase;
                    bool havePreviousPhase = false;
                    std::vector<float> synthesisPhase;

                    // Overlap-added, time-stretched signal, plus a parallel
                    // window^2 sum used to normalise it on readout - required
                    // because the synthesis hop varies continuously with the
                    // live ratio here, so no single fixed-hop COLA constant
                    // applies the way it would in a textbook fixed-ratio
                    // vocoder.
                    std::vector<float> overlapAdd;
                    std::vector<float> overlapNorm;
                    int64_t overlapMask = 0;
                    int64_t writeCursor = 0;
                    int64_t clearedUpTo = 0;

                    // Fractional read position into the stretched-time
                    // buffer above, advanced by `ratio` per output sample -
                    // this step is what actually produces the pitch shift;
                    // the analysis/synthesis above only time-stretches.
                    double readCursor = 0.0;

                    std::vector<float> fftData;
                    std::vector<float> magnitude;
                    std::vector<float> phase;

                    // regionPeak[k] holds the bin index of the magnitude
                    // peak bin `k` is assigned to (its "region of
                    // influence") - identity phase locking (Laroche-Dolson):
                    // only each peak's phase is advanced by its own
                    // true-frequency estimate; every other bin keeps the
                    // *analysis frame's* relative phase to its assigned
                    // peak. This is what suppresses inter-partial phase
                    // decorrelation ("phasiness"/smearing) on real harmonic
                    // signal, which is specifically why an earlier, simpler
                    // per-bin-independent phase vocoder was abandoned for
                    // this project (see README).
                    std::vector<int> regionPeak;
                };

                void runHop (ChannelState& channel, float ratio) noexcept;

                std::vector<ChannelState> channels;
                std::unique_ptr<juce::dsp::FFT> fft;
                std::vector<float> window;
                double currentSampleRate = 44100.0;
            };

            std::vector<ChannelBuffer> channels;
            int delayBufferMask = 0;
            float grainPeriodSamples[2] = { 0.0f, 0.0f };
            float maximumGrainPeriodSamples = 0.0f;
            int activeGrainBank = 0;
            bool grainBankTransitionActive = false;
            float grainBankCrossfade = 0.0f;
            float grainBankCrossfadeStep = 0.0f;
            int samplesUntilPitchUpdate = 0;

            // Hysteresis for updatePitchSynchronousGrainPeriod(): the raw
            // autocorrelation estimate is noisy enough on real (harmonic,
            // vibrato'd, decaying) signal to occasionally flip between two
            // candidate periods from one ~23ms check to the next, which
            // forced a bank transition on nearly every single check instead of only
            // when the pitch genuinely changed. Requiring the same candidate
            // several times in a row before acting on it filters that out.
            float pendingConfirmPeriod = -1.0f;
            int pendingConfirmCount = 0;

            // Raw (pre-multiplier) detected period from the last confirmed
            // retune. Comparing each new estimate against this - not against
            // the derived grain period, which is deliberately kept near a
            // constant ~35ms across all notes - tells genuine note changes
            // (the input's own fundamental has moved) apart from in-note
            // noise (vibrato/harmonics briefly outscoring the fundamental),
            // so the former can use fewer required confirmations.
            float lastConfirmedDetectedPeriod = -1.0f;
            double currentSampleRate = 44100.0;
            float envelopeCoefficient = 0.0f;

            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> semitones;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryBlend;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enabledMix;

            float frameRatio = 1.0f;
            float frameAntiAliasCoefficient = 1.0f;
            float frameDryBlend = 0.0f;
            float frameOutputGain = 1.0f;
            float frameEnabledMix = 0.0f;

            SpectralShifter spectralShifter;

            // Ramps toward 1 (hybrid engine active) or 0 (pure granular)
            // whenever usesHybridEngine changes, i.e. a mode-selector switch
            // crosses the Whammy/Shift-Up boundary. Progress (not the blend
            // weight itself) is what's linearly stepped, and the blend
            // weight is a sin^2 of that progress - the same zero-end-slope
            // shape already used for grain-bank retuning (grainBankCrossfade)
            // - so this transition can't click either, matching the bar the
            // rest of this class already holds itself to. Because
            // spectralShifter runs unconditionally regardless of this blend
            // (see the class comment above), there's no cold-start risk to
            // guard against here - only the mix weight moves.
            bool hybridEngineTarget = false;
            float hybridBlendProgress = 0.0f;
            float hybridBlendProgressStep = 0.0f;
        };

        // Drop Tune has to remain stable on noisy, polyphonic and transient DI
        // material, where the note-tracked grain period above can legitimately
        // become ambiguous.  This stage uses Signalsmith Stretch's polyphonic
        // frequency-domain mapping for every Drop Tune interval, so the engine
        // never changes at the selector's up/down boundary.  Its fixed
        // algorithmic latency is mirrored by the dry/bypass path.
        class DropTuneStage
        {
        public:
            void prepare (double sampleRate, int maximumBlockSize, int channelCount);
            void reset();
            void process (juce::AudioBuffer<float>& buffer, const ModeModel& model, bool enabled) noexcept;
            int getLatencySamples() const noexcept { return latencySamples; }

        private:
            struct ChannelState
            {
                std::vector<float> dryDelay;
            };

            signalsmith::stretch::SignalsmithStretch<float> shifter { 0 };
            juce::AudioBuffer<float> shiftedBuffer;
            std::vector<const float*> inputPointers;
            std::vector<float*> outputPointers;
            std::vector<ChannelState> channels;
            int preparedChannels = 0;
            int maximumBlockSize = 0;
            int latencySamples = 0;
            int dryDelayPosition = 0;
            double currentSampleRate = 44100.0;

            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> semitones;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryBlend;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
            juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enabledMix;
        };

        // The real Whammy DT has a single mono instrument input. Whatever the
        // host sends (mono or stereo, and if stereo, whether or not the two
        // channels actually carry the same content) is averaged down to this
        // one buffer before either stage sees it, and the result is copied
        // back out to every output channel afterwards - see process(). Both
        // stages are therefore always prepared for exactly one channel,
        // regardless of what the host's bus layout reports.
        juce::AudioBuffer<float> monoBuffer;

        PitchStage whirlStage;
        DropTuneStage dropTuneStage;
        int preparedChannels = 0;
    };
}
