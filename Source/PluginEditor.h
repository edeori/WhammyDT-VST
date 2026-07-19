#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WhirlDTLookAndFeel.h"
#include "WhirlDTControls.h"

class WhirlDTAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WhirlDTAudioProcessorEditor (WhirlDTAudioProcessor&);
    ~WhirlDTAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static juce::String withLetterSpacing (const juce::String& text);

    WhirlDTAudioProcessor& audioProcessor;
    WhirlDTLookAndFeel lookAndFeel;

    juce::Image backgroundImage;
    juce::Image logoImage;
    juce::Rectangle<float> logoBounds;

    juce::Label manufacturerLabel, versionLabel, titleLabel;
    juce::Label harmonyPanelLabel, dropTunePanelLabel;
    juce::Label dropTuneSwitchLabel, momentarySwitchLabel;

    WhirlSelectorKnob whirlSelector;
    DropTuneSelectorKnob dropTuneSelector;
    ExpressionTreadle treadle;

    FootswitchButton whirlFootswitch { "Whirl", false };
    FootswitchButton dropTuneFootswitch { "Drop Tune", false };
    FootswitchButton momentaryFootswitch { "Momentary", true };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> whirlModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> whirlPedalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dropTuneModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> whirlBypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dropTuneBypassAttachment;

    // Momentary doesn't use a plain click (see FootswitchButton's momentary
    // mode - it drives the parameter directly from mouseDown/mouseUp), but
    // still wants a ButtonAttachment for the other direction: host
    // automation of this parameter should light the LED even though no
    // local click ever happens.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> momentaryAttachment;

    juce::Rectangle<float> leftPanelBounds, rightPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhirlDTAudioProcessorEditor)
};
