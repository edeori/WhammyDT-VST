#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/**
    Bespoke controls that recreate the Whirl DT's physical interface:
    footswitches with an LED, the two multi-position rotary selectors
    (Whirl/Harmony/Detune list-style, Drop Tune arc-style), and the big
    expression treadle - restyled to the Moth Production house look
    (PhaseLock/StringLife's shared fresh-knob art, gold/cyan accents).
    Nothing here copies DigiTech's actual artwork, only the functional
    layout (same knobs, same switches, same pedal).
*/

//==============================================================================
class FootswitchButton : public juce::Button
{
public:
    explicit FootswitchButton (const juce::String& name, bool isMomentary = false);

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (bool)> onMomentaryPress;

private:
    bool momentary;
};

//==============================================================================
class WhirlSelectorKnob : public juce::Slider
{
public:
    WhirlSelectorKnob();

    void paint (juce::Graphics&) override;

private:
    juce::Image knobImage;
};

//==============================================================================
class DropTuneSelectorKnob : public juce::Slider
{
public:
    DropTuneSelectorKnob();

    void paint (juce::Graphics&) override;

private:
    juce::Image knobImage;
};

//==============================================================================
class ExpressionTreadle : public juce::Slider
{
public:
    ExpressionTreadle();

    void paint (juce::Graphics&) override;

    // Dragging down (toe down) should increase the value, opposite of
    // Slider's default LinearVertical mapping (up = max) - flip it here
    // since Slider has no built-in setInverted().
    double proportionOfLengthToValue (double proportion) override;
    double valueToProportionOfLength (double value) override;
};
