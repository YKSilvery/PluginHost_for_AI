#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
TestGainPluginProcessor::TestGainPluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

TestGainPluginProcessor::~TestGainPluginProcessor() = default;

// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TestGainPluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("gain", 1),
        "Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("frequency", 1),
        "Frequency",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.3f), // skew for log
        440.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("mix", 1),
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("bypass", 1),
        "Bypass",
        false));

    return { params.begin(), params.end() };
}

// ============================================================================
void TestGainPluginProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    currentPhase = 0.0;
}

void TestGainPluginProcessor::releaseResources()
{
    currentPhase = 0.0;
}

// ============================================================================
void TestGainPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Read parameters
    float gainDB    = apvts.getRawParameterValue ("gain")->load();
    float frequency = apvts.getRawParameterValue ("frequency")->load();
    float mix       = apvts.getRawParameterValue ("mix")->load();
    bool  bypassed  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (bypassed)
        return; // pass-through

    float gainLinear = juce::Decibels::decibelsToGain (gainDB);
    double phaseInc  = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Generate tone
        float tone = static_cast<float> (std::sin (currentPhase));
        currentPhase += phaseInc;
        if (currentPhase >= juce::MathConstants<double>::twoPi)
            currentPhase -= juce::MathConstants<double>::twoPi;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float dry = buffer.getSample (ch, sample);
            float out = (dry * (1.0f - mix) + tone * mix) * gainLinear;
            buffer.setSample (ch, sample, out);
        }
    }
}

// ============================================================================
juce::AudioProcessorEditor* TestGainPluginProcessor::createEditor()
{
    return new TestGainPluginEditor (*this);
}

// ============================================================================
void TestGainPluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary (*xml, destData);
}

void TestGainPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestGainPluginProcessor();
}
