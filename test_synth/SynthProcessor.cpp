#include "SynthProcessor.h"
#include "SynthEditor.h"

// ============================================================================
TestSynthProcessor::TestSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

TestSynthProcessor::~TestSynthProcessor() = default;

// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TestSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("osc_type", 1),
        "Oscillator",
        juce::StringArray { "Sine", "Saw", "Square" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("attack", 1),
        "Attack",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.4f),
        0.01f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("decay", 1),
        "Decay",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.4f),
        0.1f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("sustain", 1),
        "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("release", 1),
        "Release",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.4f),
        0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("volume", 1),
        "Volume",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f),
        -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

// ============================================================================
void TestSynthProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    for (auto& v : voices)
        v.kill();
}

void TestSynthProcessor::releaseResources()
{
    for (auto& v : voices)
        v.kill();
}

// ============================================================================
float TestSynthProcessor::generateSample (SynthVoice& voice, int oscType)
{
    float sample = 0.0f;
    double p = voice.phase;

    switch (oscType)
    {
        case 0: // Sine
            sample = static_cast<float> (std::sin (p * juce::MathConstants<double>::twoPi));
            break;
        case 1: // Saw
            sample = static_cast<float> (2.0 * p - 1.0);
            break;
        case 2: // Square
            sample = p < 0.5 ? 1.0f : -1.0f;
            break;
    }

    voice.phase += voice.phaseInc;
    if (voice.phase >= 1.0)
        voice.phase -= 1.0;

    return sample;
}

void TestSynthProcessor::processEnvelope (SynthVoice& voice,
                                           float attack, float decay,
                                           float sustain, float release)
{
    float dt = 1.0f / static_cast<float> (currentSampleRate);

    switch (voice.envStage)
    {
        case SynthVoice::EnvStage::Attack:
            voice.envLevel += dt / std::max (attack, 0.001f);
            if (voice.envLevel >= 1.0f)
            {
                voice.envLevel = 1.0f;
                voice.envStage = SynthVoice::EnvStage::Decay;
            }
            break;

        case SynthVoice::EnvStage::Decay:
            voice.envLevel -= dt / std::max (decay, 0.001f) * (1.0f - sustain);
            if (voice.envLevel <= sustain)
            {
                voice.envLevel = sustain;
                voice.envStage = SynthVoice::EnvStage::Sustain;
            }
            break;

        case SynthVoice::EnvStage::Sustain:
            voice.envLevel = sustain;
            break;

        case SynthVoice::EnvStage::Release:
            voice.envLevel -= dt / std::max (release, 0.001f) * voice.envLevel;
            if (voice.envLevel <= 0.001f)
            {
                voice.envLevel = 0.0f;
                voice.kill();
            }
            break;

        case SynthVoice::EnvStage::Off:
            voice.envLevel = 0.0f;
            break;
    }
}

// ============================================================================
void TestSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    buffer.clear();

    // Read parameters
    int   oscType  = static_cast<int> (apvts.getRawParameterValue ("osc_type")->load());
    float attack   = apvts.getRawParameterValue ("attack")->load();
    float decay    = apvts.getRawParameterValue ("decay")->load();
    float sustain  = apvts.getRawParameterValue ("sustain")->load();
    float release  = apvts.getRawParameterValue ("release")->load();
    float volumeDB = apvts.getRawParameterValue ("volume")->load();
    float volume   = juce::Decibels::decibelsToGain (volumeDB);

    // Process MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            // Find a free voice (or steal the oldest)
            SynthVoice* target = nullptr;
            for (auto& v : voices)
            {
                if (! v.isActive)
                {
                    target = &v;
                    break;
                }
            }
            if (target == nullptr)
                target = &voices[0]; // steal first voice

            target->noteOn (msg.getNoteNumber(),
                            msg.getFloatVelocity(),
                            currentSampleRate,
                            msg.getChannel());
        }
        else if (msg.isNoteOff())
        {
            for (auto& v : voices)
            {
                if (v.isActive && v.noteNumber == msg.getNoteNumber())
                {
                    v.noteOff();
                    break;
                }
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices)
                v.kill();
        }
    }

    // Render voices
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mixedSample = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.isActive)
                continue;

            processEnvelope (voice, attack, decay, sustain, release);
            float osc = generateSample (voice, oscType);
            mixedSample += osc * voice.envLevel * voice.velocity;
        }

        mixedSample *= volume;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, sample, mixedSample);
    }
}

// ============================================================================
juce::AudioProcessorEditor* TestSynthProcessor::createEditor()
{
    return new TestSynthEditor (*this);
}

void TestSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary (*xml, destData);
}

void TestSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestSynthProcessor();
}
