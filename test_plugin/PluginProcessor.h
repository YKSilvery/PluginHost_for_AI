#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

/**
 * Test Gain Plugin — a minimal VST3 plugin for validating the headless host.
 *
 * Parameters (via APVTS):
 *   - gain      : Output gain in dB  [-60, +12]
 *   - frequency : Tone generator Hz  [20, 20000]
 *   - mix       : Dry/wet mix        [0, 1]   (0=pass-through, 1=tone only)
 *   - bypass    : Plugin bypass       bool
 *
 * Processing: output = input * gain * (1-mix) + sine(freq) * gain * mix
 */
class TestGainPluginProcessor : public juce::AudioProcessor
{
public:
    TestGainPluginProcessor();
    ~TestGainPluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    // Tone generator phase
    double currentPhase = 0.0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestGainPluginProcessor)
};
