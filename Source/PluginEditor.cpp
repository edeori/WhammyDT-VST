#include "PluginEditor.h"
#include "WhirlDTFonts.h"
#include "BinaryData.h"

juce::String WhirlDTAudioProcessorEditor::withLetterSpacing (const juce::String& text)
{
    juce::String result;

    for (int i = 0; i < text.length(); ++i)
    {
        result << text[i];
        if (i < text.length() - 1)
            result << " ";
    }

    return result;
}

WhirlDTAudioProcessorEditor::WhirlDTAudioProcessorEditor (WhirlDTAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    backgroundImage = juce::ImageFileFormat::loadFrom (BinaryData::background_png, (size_t) BinaryData::background_pngSize);

    manufacturerLabel.setText (withLetterSpacing ("MOTH PRODUCTION"), juce::dontSendNotification);
    manufacturerLabel.setFont (WhirlDTFonts::uiLight (10.0f));
    manufacturerLabel.setColour (juce::Label::textColourId, WhirlDTLookAndFeel::textDim);
    addAndMakeVisible (manufacturerLabel);

    versionLabel.setText ("v" + juce::String (JucePlugin_VersionString), juce::dontSendNotification);
    versionLabel.setFont (WhirlDTFonts::uiLight (10.0f));
    versionLabel.setColour (juce::Label::textColourId, WhirlDTLookAndFeel::textDim);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    titleLabel.setText ("WHIRL DT", juce::dontSendNotification);
    titleLabel.setFont (WhirlDTFonts::title (22.0f));
    titleLabel.setColour (juce::Label::textColourId, WhirlDTLookAndFeel::textPrimary);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    auto setupPanelLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (withLetterSpacing (text), juce::dontSendNotification);
        l.setFont (WhirlDTFonts::uiMedium (13.0f));
        l.setColour (juce::Label::textColourId, WhirlDTLookAndFeel::gold);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupPanelLabel (harmonyPanelLabel, "WHIRL");
    setupPanelLabel (dropTunePanelLabel, "DROP TUNE");

    auto setupSwitchLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (withLetterSpacing (text), juce::dontSendNotification);
        l.setFont (WhirlDTFonts::uiLight (9.5f));
        l.setColour (juce::Label::textColourId, WhirlDTLookAndFeel::textDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupSwitchLabel (dropTuneSwitchLabel, "DROP TUNE");
    setupSwitchLabel (momentarySwitchLabel, "MOMENTARY");

    for (auto* l : { &manufacturerLabel, &versionLabel, &titleLabel, &harmonyPanelLabel, &dropTunePanelLabel,
                      &dropTuneSwitchLabel, &momentarySwitchLabel })
        l->setInterceptsMouseClicks (false, false);

    addAndMakeVisible (whirlSelector);
    addAndMakeVisible (dropTuneSelector);
    addAndMakeVisible (treadle);
    addAndMakeVisible (whirlFootswitch);
    addAndMakeVisible (dropTuneFootswitch);
    addAndMakeVisible (momentaryFootswitch);

    whirlModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, WhirlDTParam::whirlMode, whirlSelector);
    whirlPedalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, WhirlDTParam::whirlPedal, treadle);
    dropTuneModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, WhirlDTParam::dropTuneMode, dropTuneSelector);
    whirlBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, WhirlDTParam::whirlBypass, whirlFootswitch);
    dropTuneBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, WhirlDTParam::dropTuneBypass, dropTuneFootswitch);

    // The attachment gives us host automation -> LED (it listens to the
    // parameter and calls setToggleState on the button); local mouse
    // presses bypass the attachment's own click handling entirely (see
    // FootswitchButton's momentary mode) and drive the parameter directly
    // below, for the other direction.
    momentaryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, WhirlDTParam::momentary, momentaryFootswitch);

    if (auto* momentaryParam = audioProcessor.apvts.getParameter (WhirlDTParam::momentary))
    {
        momentaryFootswitch.onMomentaryPress = [momentaryParam] (bool down)
        {
            momentaryParam->beginChangeGesture();
            momentaryParam->setValueNotifyingHost (down ? 1.0f : 0.0f);
            momentaryParam->endChangeGesture();
        };
    }

    logoImage = WhirlDTLookAndFeel::loadMothLogo (110);

    setSize (1000, 600);
}

WhirlDTAudioProcessorEditor::~WhirlDTAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void WhirlDTAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds);
    else
        g.fillAll (WhirlDTLookAndFeel::background);

    if (logoImage.isValid() && ! logoBounds.isEmpty())
        g.drawImage (logoImage, logoBounds, juce::RectanglePlacement::centred);

    auto drawPanel = [&g] (juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (WhirlDTLookAndFeel::panelRedLight, r.getX(), r.getY(),
                                                  WhirlDTLookAndFeel::panelRedDark, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (WhirlDTLookAndFeel::accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);
    };

    drawPanel (leftPanelBounds);
    drawPanel (rightPanelBounds);
}

void WhirlDTAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Matches the header separator line baked into background.png.
    auto header = area.removeFromTop (76);
    auto headerFullWidth = header;

    // The background chassis has flush corner screws around (25,25)/(975,25)
    // - inset well past them (screw + a clear margin) so the labels don't
    // sit on top of that graphic.
    auto manuArea = header.removeFromLeft (240);
    manuArea.removeFromLeft (38);
    manufacturerLabel.setBounds (manuArea.reduced (0, 4));

    auto versionArea = header.removeFromRight (100);
    versionArea.removeFromRight (38);
    versionLabel.setBounds (versionArea.reduced (0, 4));

    // Matches PhaseLock's own header logo size (29pt in a 78pt-tall header,
    // ~37% of the header height) - the house convention this is meant to
    // follow, not a one-off value.
    constexpr float logoAspect = 592.0f / 380.0f;
    const float logoH = 28.0f;
    const float logoW = logoH * logoAspect;
    logoBounds = juce::Rectangle<float> (logoW, logoH).withCentre (headerFullWidth.toFloat().getCentre());

    titleLabel.setBounds (area.removeFromTop (34));

    // Same reasoning as the header: the chassis has flush corner screws
    // around (25,575)/(975,575) - keep the red panels' edges clear of them.
    area.removeFromLeft (48);
    area.removeFromRight (48);
    area.removeFromTop (16);
    area.removeFromBottom (44);

    const int treadleWidth = 250;
    auto treadleBounds = area.withSizeKeepingCentre (treadleWidth, area.getHeight());
    treadle.setBounds (treadleBounds);

    const int sideWidth = (area.getWidth() - treadleWidth) / 2 - 14;
    auto leftArea = area.removeFromLeft (sideWidth);
    auto rightArea = area.removeFromRight (sideWidth);

    leftPanelBounds = leftArea.toFloat();
    rightPanelBounds = rightArea.toFloat();

    auto leftInner = leftArea.reduced (14);
    harmonyPanelLabel.setBounds (leftInner.removeFromTop (20));
    leftInner.removeFromTop (4);
    auto leftFootRow = leftInner.removeFromBottom (90);
    whirlSelector.setBounds (leftInner);

    whirlFootswitch.setBounds (leftFootRow.withSizeKeepingCentre (82, 82));

    auto rightInner = rightArea.reduced (14);
    dropTunePanelLabel.setBounds (rightInner.removeFromTop (20));
    rightInner.removeFromTop (4);
    auto rightFootRow = rightInner.removeFromBottom (100);
    dropTuneSelector.setBounds (rightInner);

    auto rightLabelRow = rightFootRow.removeFromTop (16);
    dropTuneSwitchLabel.setBounds (rightLabelRow.removeFromLeft (rightLabelRow.getWidth() / 2));
    momentarySwitchLabel.setBounds (rightLabelRow);

    auto dropTuneFootArea = rightFootRow.removeFromLeft (rightFootRow.getWidth() / 2);
    auto momentaryFootArea = rightFootRow;
    dropTuneFootswitch.setBounds (dropTuneFootArea.withSizeKeepingCentre (72, 72));
    momentaryFootswitch.setBounds (momentaryFootArea.withSizeKeepingCentre (72, 72));
}
