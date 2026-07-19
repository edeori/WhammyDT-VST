#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace WhirlDTParam
{
    inline constexpr const char* whirlMode      = "whirlMode";
    inline constexpr const char* whirlBypass    = "whirlBypass";
    inline constexpr const char* whirlPedal     = "whirlPedal";
    inline constexpr const char* dropTuneMode    = "dropTuneMode";
    inline constexpr const char* dropTuneBypass  = "dropTuneBypass";
    inline constexpr const char* momentary       = "momentary";

    // Order matches the physical selector knob: Harmony effects (dry signal
    // retained), then Whirl/bend effects, then Detune effects — see the
    // Whirl DT owner's manual "Guided Tour" / "Effects" sections.
    inline const juce::StringArray whirlModeChoices {
        // Harmony
        "Oct Dn / Oct Up",
        "5th Dn / 4th Dn",
        "4th Dn / 3rd Dn",
        "5th Up / 7th Up",
        "5th Up / 6th Up",
        "4th Up / 5th Up",
        "3rd Up / 4th Up",
        "Min 3rd Up / 3rd Up",
        "2nd Up / 3rd Up",
        // Whirl
        "2 Oct Up",
        "1 Oct Up",
        "5th Up",
        "4th Up",
        "2nd Dn",
        "4th Dn",
        "5th Dn",
        "1 Oct Dn",
        "2 Oct Dn",
        "Dive Bomb",
        // Detune
        "Shallow",
        "Deep"
    };

    inline constexpr int harmonyCount = 9;
    inline constexpr int whirlCount  = 10;
    inline constexpr int detuneCount  = 2;

    // Order matches the Drop Tune selector: Shift Up arc (top half of the
    // knob) followed by the Shift Down arc (bottom half).
    inline const juce::StringArray dropTuneModeChoices {
        "Oct + Dry Up",
        "Oct Up",
        "7 Up",
        "6 Up",
        "5 Up",
        "4 Up",
        "3 Up",
        "2 Up",
        "1 Up",
        "1 Dn",
        "2 Dn",
        "3 Dn",
        "4 Dn",
        "5 Dn",
        "6 Dn",
        "7 Dn",
        "Oct Dn",
        "Oct + Dry Dn"
    };

    inline constexpr int shiftUpCount   = 9;
    inline constexpr int shiftDownCount = 9;
}

class WhirlDTAudioProcessor : public juce::AudioProcessor
{
public:
    WhirlDTAudioProcessor();
    ~WhirlDTAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhirlDTAudioProcessor)
};
