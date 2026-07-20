#include "PluginProcessor.h"
#include "PluginEditor.h"

WhirlDTAudioProcessor::WhirlDTAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

WhirlDTAudioProcessor::~WhirlDTAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout WhirlDTAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { WhirlDTParam::whirlMode, 1 },
        "Whirl Mode",
        WhirlDTParam::whirlModeChoices,
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { WhirlDTParam::whirlBypass, 1 },
        "Whirl",
        false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { WhirlDTParam::whirlPedal, 1 },
        "Whirl Pedal",
        juce::NormalisableRange<float> (0.0f, 1.0f),
        0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { WhirlDTParam::dropTuneMode, 1 },
        "Drop Tune Mode",
        WhirlDTParam::dropTuneModeChoices,
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { WhirlDTParam::dropTuneBypass, 1 },
        "Drop Tune",
        false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { WhirlDTParam::momentary, 1 },
        "Momentary",
        false));

    return { params.begin(), params.end() };
}

void WhirlDTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dspProcessor.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples (dspProcessor.getLatencySamples());
}

void WhirlDTAudioProcessor::releaseResources()
{
    dspProcessor.reset();
}

bool WhirlDTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void WhirlDTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumOutputChannels(); channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto loadInt = [this] (const char* parameterId)
    {
        return juce::roundToInt (apvts.getRawParameterValue (parameterId)->load());
    };
    const auto loadBool = [this] (const char* parameterId)
    {
        return apvts.getRawParameterValue (parameterId)->load() >= 0.5f;
    };

    WhirlDTDSP::Parameters parameters;
    parameters.whirlMode = loadInt (WhirlDTParam::whirlMode);
    parameters.whirlEnabled = loadBool (WhirlDTParam::whirlBypass);
    parameters.pedalPosition = apvts.getRawParameterValue (WhirlDTParam::whirlPedal)->load();
    parameters.dropTuneMode = loadInt (WhirlDTParam::dropTuneMode);
    parameters.dropTuneEnabled = loadBool (WhirlDTParam::dropTuneBypass)
                              || loadBool (WhirlDTParam::momentary);

    dspProcessor.process (buffer, parameters);
}

juce::AudioProcessorEditor* WhirlDTAudioProcessor::createEditor()
{
    return new WhirlDTAudioProcessorEditor (*this);
}

bool WhirlDTAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String WhirlDTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WhirlDTAudioProcessor::acceptsMidi() const
{
    return false;
}

bool WhirlDTAudioProcessor::producesMidi() const
{
    return false;
}

bool WhirlDTAudioProcessor::isMidiEffect() const
{
    return false;
}

double WhirlDTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int WhirlDTAudioProcessor::getNumPrograms()
{
    return 1;
}

int WhirlDTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void WhirlDTAudioProcessor::setCurrentProgram (int)
{
}

const juce::String WhirlDTAudioProcessor::getProgramName (int)
{
    return {};
}

void WhirlDTAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void WhirlDTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WhirlDTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhirlDTAudioProcessor();
}
