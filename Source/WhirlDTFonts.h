#pragma once

#include <juce_graphics/juce_graphics.h>

// Real embedded typefaces (Assets/Fonts/*.ttf), the same set used across
// PhaseLock/StringLife so WhirlDT reads as the same product family:
// Oxanium (bold, wide, techy) for the title, Barlow Condensed for
// everything else, Rajdhani for numeric readouts.
namespace WhirlDTFonts
{
    juce::Font title (float size);
    juce::Font uiLight (float size);
    juce::Font uiMedium (float size);
    juce::Font numeric (float size);
}
