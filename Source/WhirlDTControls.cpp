#include "WhirlDTControls.h"
#include "WhirlDTLookAndFeel.h"
#include "WhirlDTFonts.h"
#include "BinaryData.h"

namespace
{
    // Both selector knobs are drawn at this exact diameter so they read as
    // one consistent control, not two differently-sized ones.
    constexpr float selectorKnobDiameter = 80.0f;

    juce::Image loadKnobImage()
    {
        return juce::ImageFileFormat::loadFrom (BinaryData::freshknob_png, (size_t) BinaryData::freshknob_pngSize);
    }

    // The knob artwork isn't centred in its own 1254x1254 canvas (measured
    // opaque bounding box is centred around (630, 556), ~70px above the
    // canvas centre) - rotating the full image around the canvas centre
    // therefore visibly orbited off the knob's own centre. Cropping a
    // square centred on the artwork's true centre before rotating fixes
    // that; the rotation pivot then really is the knob's own centre.
    constexpr int knobArtCropX = 328;
    constexpr int knobArtCropY = 254;
    constexpr int knobArtCropSize = 604;

    // Draws the shared fresh-knob face, rotated to `angle` (radians) and
    // clipped to the given circular bounds.
    void drawKnobFace (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds, float angle)
    {
        if (! image.isValid())
        {
            g.setGradientFill (juce::ColourGradient (WhirlDTLookAndFeel::panel.brighter (0.2f), bounds.getX(), bounds.getY(),
                                                       WhirlDTLookAndFeel::panel.darker (0.4f), bounds.getRight(), bounds.getBottom(), false));
            g.fillEllipse (bounds);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (bounds, 1.5f);
            return;
        }

        g.saveState();
        juce::Path clip;
        clip.addEllipse (bounds);
        g.reduceClipRegion (clip);
        g.addTransform (juce::AffineTransform::rotation (angle, bounds.getCentreX(), bounds.getCentreY()));
        g.drawImage (image, (int) bounds.getX(), (int) bounds.getY(), (int) bounds.getWidth(), (int) bounds.getHeight(),
                     knobArtCropX, knobArtCropY, knobArtCropSize, knobArtCropSize);
        g.restoreState();
    }
}

//==============================================================================
FootswitchButton::FootswitchButton (const juce::String& name, bool isMomentary)
    : juce::Button (name), momentary (isMomentary)
{
    if (! momentary)
        setClickingTogglesState (true);
}

void FootswitchButton::mouseDown (const juce::MouseEvent& e)
{
    if (momentary)
    {
        setToggleState (true, juce::dontSendNotification);
        repaint();
        if (onMomentaryPress)
            onMomentaryPress (true);
    }
    else
    {
        Button::mouseDown (e);
    }
}

void FootswitchButton::mouseUp (const juce::MouseEvent& e)
{
    if (momentary)
    {
        setToggleState (false, juce::dontSendNotification);
        repaint();
        if (onMomentaryPress)
            onMomentaryPress (false);
    }
    else
    {
        Button::mouseUp (e);
    }
}

void FootswitchButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    auto ledArea = bounds.removeFromTop (bounds.getHeight() * 0.30f);
    auto switchArea = bounds;

    bool lit = getToggleState();

    auto ledD = juce::jmin (ledArea.getWidth(), ledArea.getHeight()) * 0.5f;
    auto ledBounds = juce::Rectangle<float> (ledD, ledD).withCentre (ledArea.getCentre());

    if (lit)
    {
        juce::DropShadow glow (WhirlDTLookAndFeel::accent.withAlpha (0.55f), juce::roundToInt (ledD * 1.6f), {});
        juce::Path ledPath;
        ledPath.addEllipse (ledBounds);
        glow.drawForPath (g, ledPath);
    }

    g.setGradientFill (juce::ColourGradient (lit ? WhirlDTLookAndFeel::accent.brighter (0.4f) : juce::Colour (0xff2a2b2f),
                                              ledBounds.getCentreX(), ledBounds.getY(),
                                              lit ? WhirlDTLookAndFeel::accent.darker (0.5f) : juce::Colour (0xff0c0c0e),
                                              ledBounds.getCentreX(), ledBounds.getBottom(), false));
    g.fillEllipse (ledBounds);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (ledBounds, 1.0f);

    auto switchD = juce::jmin (switchArea.getWidth(), switchArea.getHeight());
    auto outerR = juce::Rectangle<float> (switchD, switchD).withCentre (switchArea.getCentre());

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c0c0e),
                                              outerR.getCentreX(), outerR.getY(),
                                              WhirlDTLookAndFeel::panel.darker (isButtonDown ? 0.3f : 0.0f),
                                              outerR.getCentreX(), outerR.getBottom(), false));
    g.fillEllipse (outerR);
    g.setColour ((lit ? WhirlDTLookAndFeel::accent : WhirlDTLookAndFeel::textDim).withAlpha (lit ? 0.9f : 0.4f));
    g.drawEllipse (outerR, 1.5f);

    auto capBounds = outerR.reduced (outerR.getWidth() * 0.16f);
    juce::ColourGradient capGrad (WhirlDTLookAndFeel::panel.brighter (isButtonDown ? -0.05f : 0.18f),
                                   capBounds.getX(), capBounds.getY(),
                                   juce::Colour (0xff0a0a0c),
                                   capBounds.getRight(), capBounds.getBottom(), false);
    g.setGradientFill (capGrad);
    g.fillEllipse (capBounds);

    if (isMouseOverButton)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillEllipse (outerR);
    }
}

//==============================================================================
WhirlSelectorKnob::WhirlSelectorKnob()
    : knobImage (loadKnobImage())
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setRange (0, (double) WhirlDTParam::whirlModeChoices.size() - 1, 1);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (130);
}

void WhirlSelectorKnob::paint (juce::Graphics& g)
{
    using namespace WhirlDTParam;

    auto bounds = getLocalBounds().toFloat();
    int currentIndex = (int) std::round (getValue());

    auto headerArea = bounds.removeFromTop (16.0f);
    auto detuneArea = bounds.removeFromBottom (20.0f);

    g.setFont (WhirlDTFonts::uiMedium (11.0f));
    g.setColour (WhirlDTLookAndFeel::gold);
    g.drawText ("HARMONY", headerArea.removeFromLeft (headerArea.getWidth() * 0.5f), juce::Justification::centredLeft);
    g.drawText ("WHIRL", headerArea, juce::Justification::centredRight);

    auto listArea = bounds;
    const float knobGap = selectorKnobDiameter + 12.0f;
    auto leftCol  = listArea.removeFromLeft ((listArea.getWidth() - knobGap) * 0.5f);
    auto knobArea = listArea.removeFromLeft (knobGap);
    auto rightCol = listArea;

    auto drawRow = [&g] (juce::Rectangle<float> row, const juce::String& text, bool ledOnLeft, bool active)
    {
        const float ledSize = 6.0f;
        auto ledBounds = ledOnLeft ? row.removeFromLeft (ledSize + 6.0f).withSizeKeepingCentre (ledSize, ledSize)
                                    : row.removeFromRight (ledSize + 6.0f).withSizeKeepingCentre (ledSize, ledSize);

        g.setColour (active ? WhirlDTLookAndFeel::accent : juce::Colour (0xff35363a));
        g.fillEllipse (ledBounds);
        if (active)
        {
            g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.35f));
            g.fillEllipse (ledBounds.expanded (3.0f));
        }

        g.setColour (active ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
        g.setFont (WhirlDTFonts::uiLight (active ? 10.5f : 10.0f));
        g.drawText (text, row, ledOnLeft ? juce::Justification::centredLeft : juce::Justification::centredRight);
    };

    auto rowHeightL = leftCol.getHeight() / (float) harmonyCount;
    for (int i = 0; i < harmonyCount; ++i)
        drawRow (leftCol.removeFromTop (rowHeightL), whirlModeChoices[i], false, currentIndex == i);

    auto rowHeightR = rightCol.getHeight() / (float) whirlCount;
    for (int i = 0; i < whirlCount; ++i)
        drawRow (rightCol.removeFromTop (rowHeightR), whirlModeChoices[harmonyCount + i], true, currentIndex == harmonyCount + i);

    auto knobCircle = knobArea.withSizeKeepingCentre (selectorKnobDiameter, selectorKnobDiameter);
    auto totalChoices = (float) (whirlModeChoices.size() - 1);
    auto angle = juce::jmap ((float) currentIndex, 0.0f, totalChoices,
                              juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f));
    drawKnobFace (g, knobImage, knobCircle, angle);

    bool shallowActive = currentIndex == harmonyCount + whirlCount;
    bool deepActive    = currentIndex == harmonyCount + whirlCount + 1;

    const float thirdW = detuneArea.getWidth() / 3.0f;
    auto shallowR = detuneArea.removeFromLeft (thirdW);
    auto deepR    = detuneArea.removeFromRight (thirdW);
    auto midR     = detuneArea;

    g.setFont (WhirlDTFonts::uiLight (shallowActive ? 10.5f : 10.0f));
    g.setColour (shallowActive ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
    g.drawText ("SHALLOW", shallowR, juce::Justification::centred);

    g.setFont (WhirlDTFonts::uiLight (deepActive ? 10.5f : 10.0f));
    g.setColour (deepActive ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
    g.drawText ("DEEP", deepR, juce::Justification::centred);

    g.setFont (WhirlDTFonts::uiMedium (10.0f));
    g.setColour (WhirlDTLookAndFeel::gold);
    g.drawText ("DETUNE", midR, juce::Justification::centred);
}

//==============================================================================
DropTuneSelectorKnob::DropTuneSelectorKnob()
    : knobImage (loadKnobImage())
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setRange (0, (double) WhirlDTParam::dropTuneModeChoices.size() - 1, 1);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (130);
}

void DropTuneSelectorKnob::paint (juce::Graphics& g)
{
    using namespace WhirlDTParam;

    auto bounds = getLocalBounds().toFloat();
    int currentIndex = (int) std::round (getValue());

    auto headerTop = bounds.removeFromTop (14.0f);
    auto headerBottom = bounds.removeFromBottom (14.0f);

    g.setFont (WhirlDTFonts::uiMedium (11.0f));
    g.setColour (WhirlDTLookAndFeel::gold);
    g.drawText ("SHIFT UP", headerTop, juce::Justification::centred);
    g.drawText ("SHIFT DOWN", headerBottom, juce::Justification::centred);

    auto circleArea = bounds;
    auto centre = circleArea.getCentre();
    auto knobCircle = juce::Rectangle<float> (selectorKnobDiameter, selectorKnobDiameter).withCentre (centre);

    auto totalChoices = (float) (dropTuneModeChoices.size() - 1);
    auto angle = juce::jmap ((float) currentIndex, 0.0f, totalChoices,
                              juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f));
    drawKnobFace (g, knobImage, knobCircle, angle);

    const float ledRadius = selectorKnobDiameter * 0.5f + 20.0f;
    const float labelRadius = selectorKnobDiameter * 0.5f + 34.0f;

    // Short engraving-style labels for the arc (the full descriptive names
    // live in dropTuneModeChoices for host/automation display).
    static const juce::StringArray arcLabels {
        "+OD", "+Oct", "+7", "+6", "+5", "+4", "+3", "+2", "+1",
        "-1", "-2", "-3", "-4", "-5", "-6", "-7", "-Oct", "-OD"
    };

    auto placeArc = [&] (int startIdx, int count, float startDeg, float endDeg)
    {
        for (int i = 0; i < count; ++i)
        {
            int idx = startIdx + i;
            float t = count > 1 ? (float) i / (float) (count - 1) : 0.0f;
            float deg = juce::jmap (t, 0.0f, 1.0f, startDeg, endDeg);
            float rad = juce::degreesToRadians (deg);

            juce::Point<float> ledPos (centre.x + ledRadius * std::sin (rad), centre.y - ledRadius * std::cos (rad));
            juce::Point<float> labelPos (centre.x + labelRadius * std::sin (rad), centre.y - labelRadius * std::cos (rad));

            bool active = currentIndex == idx;
            auto ledB = juce::Rectangle<float> (6.0f, 6.0f).withCentre (ledPos);
            g.setColour (active ? WhirlDTLookAndFeel::accent : juce::Colour (0xff35363a));
            g.fillEllipse (ledB);
            if (active)
            {
                g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.35f));
                g.fillEllipse (ledB.expanded (3.0f));
            }

            g.setColour (active ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
            g.setFont (WhirlDTFonts::uiLight (active ? 10.5f : 10.0f));
            juce::Rectangle<float> textB (28.0f, 11.0f);
            textB.setCentre (labelPos);
            g.drawText (arcLabels[idx], textB, juce::Justification::centred);
        }
    };

    placeArc (0, shiftUpCount, -80.0f, 80.0f);
    placeArc (shiftUpCount, shiftDownCount, 100.0f, 260.0f);
}

//==============================================================================
ExpressionTreadle::ExpressionTreadle()
{
    setSliderStyle (juce::Slider::LinearVertical);
    setRange (0.0, 1.0);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
}

double ExpressionTreadle::proportionOfLengthToValue (double proportion)
{
    return juce::Slider::proportionOfLengthToValue (1.0 - proportion);
}

double ExpressionTreadle::valueToProportionOfLength (double value)
{
    return 1.0 - juce::Slider::valueToProportionOfLength (value);
}

void ExpressionTreadle::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float value = (float) getValue();

    float topInset = bounds.getWidth() * 0.04f;
    float bottomInset = bounds.getWidth() * 0.16f;

    juce::Path shape;
    shape.startNewSubPath (bounds.getX() + topInset, bounds.getY());
    shape.lineTo (bounds.getRight() - topInset, bounds.getY());
    shape.lineTo (bounds.getRight() - bottomInset, bounds.getBottom());
    shape.lineTo (bounds.getX() + bottomInset, bounds.getBottom());
    shape.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c1e), bounds.getX(), bounds.getY(),
                                              juce::Colour (0xff0a0a0b), bounds.getX(), bounds.getBottom(), false));
    g.fillPath (shape);

    g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.35f));
    g.strokePath (shape, juce::PathStrokeType (1.5f));

    g.saveState();
    g.reduceClipRegion (shape);

    const int numRidges = 14;
    for (int i = 0; i < numRidges; ++i)
    {
        float t = (float) i / (float) (numRidges - 1);
        float y = bounds.getY() + t * bounds.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.drawLine (bounds.getX(), y, bounds.getRight(), y, 1.5f);
    }

    float sheenY = juce::jmap (value, 0.0f, 1.0f,
                                bounds.getY() + bounds.getHeight() * 0.15f,
                                bounds.getBottom() - bounds.getHeight() * 0.15f);
    juce::ColourGradient sheen (WhirlDTLookAndFeel::accent.withAlpha (0.10f), bounds.getX(), sheenY - 40.0f,
                                 juce::Colours::transparentBlack, bounds.getX(), sheenY + 40.0f, false);
    g.setGradientFill (sheen);
    g.fillRect (bounds);

    g.restoreState();

    g.setColour (WhirlDTLookAndFeel::textDim);
    g.setFont (WhirlDTFonts::uiMedium (11.0f));
    g.drawText ("TOE", bounds.removeFromTop (18.0f), juce::Justification::centred);
    g.drawText ("HEEL", bounds.removeFromBottom (18.0f), juce::Justification::centred);
}
