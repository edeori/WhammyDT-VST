#include "WhirlDTLookAndFeel.h"
#include "WhirlDTFonts.h"
#include "BinaryData.h"

const juce::Colour WhirlDTLookAndFeel::background   { 0xff17171a };
const juce::Colour WhirlDTLookAndFeel::panel        { 0xff232326 };
const juce::Colour WhirlDTLookAndFeel::gold         { 0xfff2b705 };
const juce::Colour WhirlDTLookAndFeel::goldDim      { 0xffb08300 };
const juce::Colour WhirlDTLookAndFeel::accent       { 0xff4fd8ea };
const juce::Colour WhirlDTLookAndFeel::textPrimary  { 0xffececec };
const juce::Colour WhirlDTLookAndFeel::textDim      { 0xff9a9a9e };
const juce::Colour WhirlDTLookAndFeel::panelRedLight { 0xff8a1c24 };
const juce::Colour WhirlDTLookAndFeel::panelRedDark  { 0xff420d12 };

WhirlDTLookAndFeel::WhirlDTLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, textPrimary);
}

juce::Font WhirlDTLookAndFeel::getLabelFont (juce::Label& label)
{
    // LookAndFeel_V2::drawLabel() calls this directly instead of
    // label.getFont(), so returning a hardcoded font here would silently
    // discard every per-label setFont() call the editor makes.
    return label.getFont();
}

namespace
{
    bool isOpaqueAt (const juce::Image::BitmapData& data, int x, int y, juce::uint8 threshold)
    {
        if (x < 0 || y < 0 || x >= data.width || y >= data.height)
            return false;

        return ((juce::PixelARGB*) data.getPixelPointer (x, y))->getAlpha() >= threshold;
    }
}

juce::Image WhirlDTLookAndFeel::loadMothLogo (int pixelHeight)
{
    auto raw = juce::ImageFileFormat::loadFrom (BinaryData::moth_logo_png, (size_t) BinaryData::moth_logo_pngSize);
    if (! raw.isValid() || pixelHeight <= 0)
        return {};

    constexpr float nativeAspect = 592.0f / 380.0f;
    int width = juce::roundToInt ((float) pixelHeight * nativeAspect);

    auto scaled = raw.rescaled (width, pixelHeight, juce::Graphics::highResamplingQuality)
                      .convertedToFormat (juce::Image::ARGB);

    juce::Image result (juce::Image::ARGB, width, pixelHeight, true);
    juce::Image::BitmapData srcData (scaled, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstData (result, juce::Image::BitmapData::writeOnly);

    constexpr juce::uint8 threshold = 40;
    const int dilate = juce::jmax (1, pixelHeight / 60);

    for (int y = 0; y < dstData.height; ++y)
    {
        for (int x = 0; x < dstData.width; ++x)
        {
            bool opaque = false;
            for (int dy = -dilate; dy <= dilate && ! opaque; ++dy)
                for (int dx = -dilate; dx <= dilate && ! opaque; ++dx)
                    if (isOpaqueAt (srcData, x + dx, y + dy, threshold))
                        opaque = true;

            auto* dst = (juce::PixelARGB*) dstData.getPixelPointer (x, y);
            juce::uint8 v = opaque ? 255 : 0;
            dst->setARGB (v, v, v, v);
        }
    }

    return result;
}
