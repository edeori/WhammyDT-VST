#include "WhirlDTControls.h"
#include "WhirlDTLookAndFeel.h"
#include "WhirlDTFonts.h"
#include "BinaryData.h"

#include <array>
#include <cmath>

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

    // Clockwise order around the physical selector.  The public parameter
    // indices deliberately stay in their original host/automation order
    // (Harmony, Whammy, Detune); this table is only the dial's mechanical
    // order.  Keeping those two concepts separate avoids breaking saved
    // sessions while making the knob move continuously through neighbouring
    // labels instead of jumping from one side of the panel to the other.
    constexpr std::array<int, 21> whirlDialPositionToMode {
        9, 10, 11, 12, 13, 14, 15, 16, 17, 18, // Whammy, top to bottom
        20, 19,                                  // Deep, then Shallow
        8, 7, 6, 5, 4, 3, 2, 1, 0              // Harmony, bottom to top
    };

    int dialPositionForWhirlMode (int mode) noexcept
    {
        for (int position = 0; position < static_cast<int> (whirlDialPositionToMode.size()); ++position)
            if (whirlDialPositionToMode[static_cast<size_t> (position)] == mode)
                return position;

        return 0;
    }

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
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setRange (0, (double) WhirlDTParam::whirlModeChoices.size() - 1, 1);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (180);
}

void WhirlSelectorKnob::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = static_cast<double> (dialPositionForWhirlMode (juce::roundToInt (getValue())));
    dragStartY = e.position.y;
}

void WhirlSelectorKnob::mouseDrag (const juce::MouseEvent& e)
{
    // Work in physical dial positions, not the host parameter's grouped
    // Harmony/Whammy/Detune index order.  Adjacent drag steps now always move
    // to the adjacent printed position around the panel.
    const auto stepCount = static_cast<double> (whirlDialPositionToMode.size());
    const auto sensitivity = 180.0;
    const auto rawPosition = dragStartValue
                           + (dragStartY - e.position.y) * (stepCount - 1.0) / sensitivity;

    auto wrapped = std::fmod (rawPosition, stepCount);
    if (wrapped < 0.0)
        wrapped += stepCount;

    const auto position = juce::roundToInt (wrapped) % static_cast<int> (whirlDialPositionToMode.size());
    setValue (whirlDialPositionToMode[static_cast<size_t> (position)], juce::sendNotificationSync);
}

void WhirlSelectorKnob::paint (juce::Graphics& g)
{
    using namespace WhirlDTParam;

    auto bounds = getLocalBounds().toFloat().reduced (3.0f, 2.0f);
    int currentIndex = (int) std::round (getValue());

    auto plate = bounds.reduced (2.0f);
    auto mainPlate = plate;
    auto detuneBar = mainPlate.removeFromBottom (34.0f);
    juce::Path plateClip;
    plateClip.addRoundedRectangle (plate, 8.0f);

    g.saveState();
    g.reduceClipRegion (plateClip);

    auto harmonyHalf = mainPlate.withRight (mainPlate.getCentreX());
    g.setGradientFill (juce::ColourGradient (WhirlDTLookAndFeel::panelRedLight.brighter (0.08f),
                                              harmonyHalf.getX(), harmonyHalf.getY(),
                                              WhirlDTLookAndFeel::panelRedDark,
                                              harmonyHalf.getRight(), harmonyHalf.getBottom(), false));
    g.fillRect (harmonyHalf);

    auto whammyHalf = mainPlate.withLeft (mainPlate.getCentreX());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff29292d),
                                              whammyHalf.getX(), whammyHalf.getY(),
                                              juce::Colour (0xff101013),
                                              whammyHalf.getRight(), whammyHalf.getBottom(), false));
    g.fillRect (whammyHalf);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6d1c6),
                                              detuneBar.getX(), detuneBar.getY(),
                                              juce::Colour (0xffaaa69e),
                                              detuneBar.getX(), detuneBar.getBottom(), false));
    g.fillRect (detuneBar);
    g.restoreState();

    g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.36f));
    g.drawRoundedRectangle (plate.reduced (0.5f), 8.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.52f));
    g.drawVerticalLine (juce::roundToInt (mainPlate.getCentreX()), mainPlate.getY(), mainPlate.getBottom());
    g.drawHorizontalLine (juce::roundToInt (detuneBar.getY()), detuneBar.getX(), detuneBar.getRight());

    auto headerArea = mainPlate.removeFromTop (23.0f).reduced (9.0f, 3.0f);
    auto harmonyHeader = headerArea.removeFromLeft (headerArea.getWidth() * 0.5f);
    auto whammyHeader = headerArea;
    g.setFont (WhirlDTFonts::uiMedium (11.0f));
    g.setColour (WhirlDTLookAndFeel::textPrimary.withAlpha (0.94f));
    g.drawText ("HARMONY", harmonyHeader, juce::Justification::centredLeft);
    g.drawText ("WHAMMY", whammyHeader, juce::Justification::centredRight);

    auto listArea = mainPlate.reduced (7.0f, 4.0f);
    constexpr float knobGap = selectorKnobDiameter + 10.0f;
    auto leftColumn = listArea.removeFromLeft ((listArea.getWidth() - knobGap) * 0.5f);
    auto knobArea = listArea.removeFromLeft (knobGap);
    auto rightColumn = listArea;
    auto knobCircle = knobArea.withSizeKeepingCentre (selectorKnobDiameter, selectorKnobDiameter);
    auto knobCentre = knobCircle.getCentre();

    static const juce::StringArray harmonyLabels {
        "OCT DN / OCT UP", "5TH DN / 4TH DN", "4TH DN / 3RD DN",
        "5TH UP / 7TH UP", "5TH UP / 6TH UP", "4TH UP / 5TH UP",
        "3RD UP / 4TH UP", "MIN3 UP / 3RD UP", "2ND UP / 3RD UP"
    };
    static const juce::StringArray whammyLabels {
        "2 OCT UP", "OCT UP", "5TH UP", "4TH UP", "2ND DOWN",
        "4TH DOWN", "5TH DOWN", "OCT DOWN", "2 OCT DOWN", "DIVE BOMB"
    };

    auto drawModeRow = [&g] (juce::Rectangle<float> row, const juce::String& label,
                             bool harmonySide, bool active)
    {
        row = row.reduced (1.5f, 2.0f);
        const auto corner = row.getHeight() * 0.45f;

        if (active)
        {
            juce::DropShadow glow (WhirlDTLookAndFeel::accent.withAlpha (0.40f), 8, {});
            juce::Path glowPath;
            glowPath.addRoundedRectangle (row, corner);
            glow.drawForPath (g, glowPath);
        }

        const auto topColour = harmonySide ? juce::Colour (0xff242427)
                                           : WhirlDTLookAndFeel::panelRedLight.darker (0.10f);
        const auto bottomColour = harmonySide ? juce::Colour (0xff09090b)
                                              : WhirlDTLookAndFeel::panelRedDark.darker (0.25f);
        g.setGradientFill (juce::ColourGradient (active ? juce::Colour (0xff30363a) : topColour,
                                                  row.getX(), row.getY(), bottomColour,
                                                  row.getRight(), row.getBottom(), false));
        g.fillRoundedRectangle (row, corner);
        g.setColour ((active ? WhirlDTLookAndFeel::accent
                             : (harmonySide ? juce::Colour (0xff5b252a) : WhirlDTLookAndFeel::panelRedLight))
                         .withAlpha (active ? 0.95f : 0.76f));
        g.drawRoundedRectangle (row.reduced (0.5f), corner, active ? 1.3f : 0.8f);

        auto ledArea = harmonySide ? row.removeFromLeft (11.0f) : row.removeFromRight (11.0f);
        const auto led = juce::Rectangle<float> (4.5f, 4.5f).withCentre (ledArea.getCentre());
        g.setColour (active ? WhirlDTLookAndFeel::accent : juce::Colour (0xff4b2428));
        g.fillEllipse (led);

        g.setColour (active ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
        g.setFont (WhirlDTFonts::uiMedium (7.7f));
        g.drawFittedText (label, row.toNearestInt(), juce::Justification::centred, 1, 0.68f);
    };

    const auto harmonyRowHeight = leftColumn.getHeight() / (float) harmonyCount;
    for (int index = 0; index < harmonyCount; ++index)
    {
        auto row = leftColumn.withTrimmedTop (harmonyRowHeight * (float) index)
                             .withHeight (harmonyRowHeight);
        const auto active = currentIndex == index;
        drawModeRow (row, harmonyLabels[index], true, active);
    }

    const auto whammyRowHeight = rightColumn.getHeight() / (float) whirlCount;
    for (int index = 0; index < whirlCount; ++index)
    {
        auto row = rightColumn.withTrimmedTop (whammyRowHeight * (float) index)
                              .withHeight (whammyRowHeight);
        const auto modeIndex = harmonyCount + index;
        const auto active = currentIndex == modeIndex;
        drawModeRow (row, whammyLabels[index], false, active);
    }

    const auto shallowActive = currentIndex == harmonyCount + whirlCount;
    const auto deepActive = currentIndex == harmonyCount + whirlCount + 1;
    auto detuneInner = detuneBar.reduced (8.0f, 4.0f);
    auto shallowArea = detuneInner.removeFromLeft (detuneInner.getWidth() * 0.38f);
    auto deepArea = detuneInner.removeFromRight (detuneInner.getWidth() * 0.38f);
    auto detuneTitleArea = detuneInner;

    auto drawDetuneChoice = [&g] (juce::Rectangle<float> area, const juce::String& text,
                                  bool ledOnLeft, bool active)
    {
        if (active)
        {
            g.setColour (juce::Colour (0xff22262a));
            g.fillRoundedRectangle (area, area.getHeight() * 0.45f);
            g.setColour (WhirlDTLookAndFeel::accent);
            g.drawRoundedRectangle (area.reduced (0.5f), area.getHeight() * 0.45f, 1.2f);
        }

        auto ledArea = ledOnLeft ? area.removeFromLeft (11.0f) : area.removeFromRight (11.0f);
        g.setColour (active ? WhirlDTLookAndFeel::accent : juce::Colour (0xff5b252a));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (ledArea.getCentre()));
        g.setColour (active ? WhirlDTLookAndFeel::textPrimary : juce::Colour (0xff353538));
        g.setFont (WhirlDTFonts::uiMedium (8.2f));
        g.drawFittedText (text, area.toNearestInt(), juce::Justification::centred, 1, 0.72f);
    };

    drawDetuneChoice (shallowArea, "SHALLOW", true, shallowActive);
    drawDetuneChoice (deepArea, "DEEP", false, deepActive);
    g.setColour (juce::Colour (0xff29292c));
    g.setFont (WhirlDTFonts::uiMedium (9.0f));
    g.drawFittedText ("DETUNE", detuneTitleArea.toNearestInt(), juce::Justification::centred, 1, 0.72f);

    // The pointer follows the same clockwise mechanical order used by
    // mouseDrag(): Whammy top-to-bottom, Deep, Shallow, then Harmony
    // bottom-to-top.  Mode indices remain untouched for session compatibility.
    constexpr float whammyStartDegrees = 45.0f, whammyEndDegrees = 135.0f;
    constexpr float deepDegrees = 160.0f, shallowDegrees = 200.0f;
    constexpr float harmonyBottomDegrees = 225.0f, harmonyTopDegrees = 315.0f;

    float knobDegrees;
    if (currentIndex < harmonyCount)
        knobDegrees = juce::jmap ((float) currentIndex, 0.0f, (float) (harmonyCount - 1),
                                 harmonyTopDegrees, harmonyBottomDegrees);
    else if (currentIndex < harmonyCount + whirlCount)
        knobDegrees = juce::jmap ((float) (currentIndex - harmonyCount), 0.0f, (float) (whirlCount - 1),
                                 whammyStartDegrees, whammyEndDegrees);
    else
        knobDegrees = currentIndex == harmonyCount + whirlCount ? shallowDegrees : deepDegrees;

    const auto knobAngle = juce::degreesToRadians (knobDegrees);
    drawKnobFace (g, knobImage, knobCircle, knobAngle);

    const auto indicatorStart = knobCentre
                              + juce::Point<float> (std::sin (knobAngle), -std::cos (knobAngle)) * 15.0f;
    const auto indicatorEnd = knobCentre
                            + juce::Point<float> (std::sin (knobAngle), -std::cos (knobAngle)) * 31.0f;
    g.setColour (WhirlDTLookAndFeel::textPrimary.withAlpha (0.82f));
    g.drawLine ({ indicatorStart, indicatorEnd }, 2.1f);
    g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.75f));
    g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (indicatorEnd));
}

//==============================================================================
DropTuneSelectorKnob::DropTuneSelectorKnob()
    : knobImage (loadKnobImage())
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setRange (0, (double) WhirlDTParam::dropTuneModeChoices.size() - 1, 1);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (180);
    setRotaryParameters (0.0f, juce::MathConstants<float>::twoPi, false);
}

void DropTuneSelectorKnob::paint (juce::Graphics& g)
{
    using namespace WhirlDTParam;

    auto bounds = getLocalBounds().toFloat().reduced (3.0f, 2.0f);
    int currentIndex = (int) std::round (getValue());

    // A two-field selector plate mirrors the physical pedal's visual hierarchy:
    // raised intervals live on the red upper half and lowered intervals on the
    // dark lower half, while retaining the Moth Production palette.
    auto plate = bounds.reduced (2.0f);
    juce::Path plateClip;
    plateClip.addRoundedRectangle (plate, 8.0f);

    g.saveState();
    g.reduceClipRegion (plateClip);
    g.setGradientFill (juce::ColourGradient (WhirlDTLookAndFeel::panelRedLight.brighter (0.08f),
                                              plate.getX(), plate.getY(),
                                              WhirlDTLookAndFeel::panelRedDark,
                                              plate.getRight(), plate.getCentreY(), false));
    g.fillRect (plate.withBottom (plate.getCentreY() + 1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27272b),
                                              plate.getX(), plate.getCentreY(),
                                              juce::Colour (0xff111114),
                                              plate.getRight(), plate.getBottom(), false));
    g.fillRect (plate.withTop (plate.getCentreY()));
    g.restoreState();

    g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.36f));
    g.drawRoundedRectangle (plate.reduced (0.5f), 8.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.48f));
    g.drawHorizontalLine (juce::roundToInt (plate.getCentreY()), plate.getX(), plate.getRight());

    auto centre = plate.getCentre();
    auto knobCircle = juce::Rectangle<float> (selectorKnobDiameter, selectorKnobDiameter).withCentre (centre);

    // Increasing index turns the knob further clockwise (+degrees), matching
    // the harmony/whammy selector and standard rotary-control expectation.
    // Starting at 280 degrees (not 80) keeps Shift Up in the top half and
    // Shift Down in the bottom half, matching their static header labels:
    // simply flipping the sign here once already fixed the direction, but
    // going clockwise *from the same 80-degree start* swept Up down through
    // the bottom and Down up through the top - i.e. swapped which half each
    // section actually landed in.
    constexpr float firstPositionDegrees = 280.0f;
    constexpr float positionStepDegrees = 20.0f;
    const auto knobAngle = juce::degreesToRadians (firstPositionDegrees
                                                   + positionStepDegrees * (float) currentIndex);

    const auto labelRadius = juce::jmin (plate.getWidth() * 0.37f, plate.getHeight() * 0.345f);
    static const juce::StringArray positionLabels {
        "OCT\nDRY", "OCT", "7", "6", "5", "4", "3", "2", "1",
        "1", "2", "3", "4", "5", "6", "7", "OCT", "OCT\nDRY"
    };

    for (int index = 0; index < dropTuneModeChoices.size(); ++index)
    {
        const auto degrees = firstPositionDegrees + positionStepDegrees * (float) index;
        const auto radians = juce::degreesToRadians (degrees);
        const juce::Point<float> position (centre.x + labelRadius * std::sin (radians),
                                           centre.y - labelRadius * std::cos (radians));
        const auto isEndpoint = index == 0 || index == 1 || index == 16 || index == 17;
        const auto active = index == currentIndex;
        const auto cellWidth = isEndpoint ? 29.0f : 21.0f;
        auto cell = juce::Rectangle<float> (cellWidth, 29.0f).withCentre (position);

        if (active)
        {
            juce::DropShadow glow (WhirlDTLookAndFeel::accent.withAlpha (0.42f), 9, {});
            juce::Path glowPath;
            glowPath.addRoundedRectangle (cell, cell.getWidth() * 0.42f);
            glow.drawForPath (g, glowPath);
        }

        g.setGradientFill (juce::ColourGradient (active ? juce::Colour (0xff30363a)
                                                        : juce::Colour (0xff1d1d20),
                                                  cell.getX(), cell.getY(),
                                                  juce::Colour (0xff08080a),
                                                  cell.getRight(), cell.getBottom(), false));
        g.fillRoundedRectangle (cell, cell.getWidth() * 0.42f);
        g.setColour ((active ? WhirlDTLookAndFeel::accent : WhirlDTLookAndFeel::panelRedLight)
                         .withAlpha (active ? 0.95f : 0.72f));
        g.drawRoundedRectangle (cell.reduced (0.5f), cell.getWidth() * 0.42f, active ? 1.4f : 0.9f);

        auto textArea = cell.reduced (1.0f, 2.0f);
        textArea.removeFromBottom (6.0f);
        g.setColour (active ? WhirlDTLookAndFeel::textPrimary : WhirlDTLookAndFeel::textDim);
        g.setFont (WhirlDTFonts::uiMedium (isEndpoint ? 7.3f : 10.5f));
        g.drawFittedText (positionLabels[index], textArea.toNearestInt(),
                          juce::Justification::centred, isEndpoint ? 2 : 1, 0.72f);

        const auto ledBounds = juce::Rectangle<float> (4.5f, 4.5f)
                                   .withCentre ({ cell.getCentreX(), cell.getBottom() - 5.0f });
        g.setColour (active ? WhirlDTLookAndFeel::accent : juce::Colour (0xff4b2428));
        g.fillEllipse (ledBounds);
    }

    g.setFont (WhirlDTFonts::uiMedium (11.0f));
    g.setColour (WhirlDTLookAndFeel::textPrimary.withAlpha (0.92f));
    g.drawText ("SHIFT UP", plate.reduced (11.0f, 6.0f).withHeight (16.0f),
                juce::Justification::centredLeft);
    g.drawText ("SHIFT DOWN", plate.reduced (11.0f, 6.0f).removeFromBottom (16.0f),
                juce::Justification::centredLeft);

    drawKnobFace (g, knobImage, knobCircle, knobAngle);

    // The indicator makes every one of the 18 full-circle positions explicit,
    // even though the knob artwork itself is deliberately low-contrast.
    const auto indicatorStart = centre + juce::Point<float> (std::sin (knobAngle), -std::cos (knobAngle)) * 15.0f;
    const auto indicatorEnd = centre + juce::Point<float> (std::sin (knobAngle), -std::cos (knobAngle)) * 31.0f;
    g.setColour (WhirlDTLookAndFeel::textPrimary.withAlpha (0.82f));
    g.drawLine ({ indicatorStart, indicatorEnd }, 2.1f);
    g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.75f));
    g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (indicatorEnd));
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
