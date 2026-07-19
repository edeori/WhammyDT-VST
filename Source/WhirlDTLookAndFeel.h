#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    WhirlDT's visual theme: the Moth Production house style shared with
    PhaseLock/StringLife - dark brushed-metal panels, gold section labels,
    a cyan glow accent for interactive/lit elements - applied to a Whirl
    DT-style hardware layout (same knobs, LEDs, footswitches, expression
    treadle as the real pedal, just restyled to match the product family).
*/
class WhirlDTLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WhirlDTLookAndFeel();

    juce::Font getLabelFont (juce::Label&) override;

    static const juce::Colour background;
    static const juce::Colour panel;
    static const juce::Colour gold;
    static const juce::Colour goldDim;
    static const juce::Colour accent;
    static const juce::Colour textPrimary;
    static const juce::Colour textDim;

    // Kept from the Whirl DT-accurate red control panels (house palette
    // everywhere else, but this red reads as the pedal's own identity, not
    // something to normalise away).
    static const juce::Colour panelRedLight;
    static const juce::Colour panelRedDark;

    // Loads the shared moth_logo.png at exactly `pixelHeight` px tall,
    // bolding/thresholding its hairline artwork so it survives the final
    // draw-time scale-to-points without diluting away to near-invisible
    // grey (its native art is a sparse hard-edged line drawing - any
    // further resampling averages the opaque line pixels with their
    // transparent neighbours into low coverage, which reads as almost
    // invisible against a near-black background even though no single
    // source pixel is low-alpha).
    static juce::Image loadMothLogo (int pixelHeight);
};
