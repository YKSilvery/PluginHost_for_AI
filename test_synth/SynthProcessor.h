#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

/**
 * Test Synth Plugin -- a minimal polyphonic VST3 synth for validating the headless host.
 *
 * Features:
 *   - 8-voice polyphonic sine/saw/square oscillator
 *   - ADSR envelope
 *   - Master volume
 *
 * Parameters (via APVTS):
 *   - osc_type  : Oscillator waveform  [0=sine, 1=saw, 2=square]
 *   - attack    : ADSR attack   [0.001, 5.0] seconds
 *   - decay     : ADSR decay    [0.001, 5.0] seconds
 *   - sustain   : ADSR sustain  [0.0, 1.0]
 *   - release   : ADSR release  [0.001, 5.0] seconds
 *   - volume    : Master volume [-60, 0] dB
 */

// ============================================================================
// Voice
// ============================================================================
struct SynthVoice
{
    bool   isActive    = false;
    int    noteNumber  = -1;
    int    midiChannel = 0;
    float  velocity    = 0.0f;
    double phase       = 0.0;
    double phaseInc    = 0.0;

    // ADSR state
    enum class EnvStage { Attack, Decay, Sustain, Release, Off };
    EnvStage envStage   = EnvStage::Off;
    float    envLevel   = 0.0f;

    void noteOn (int note, float vel, double sampleRate, int channel)
    {
        isActive    = true;
        noteNumber  = note;
        midiChannel = channel;
        velocity    = vel;
        phase       = 0.0;
        phaseInc    = 440.0 * std::pow (2.0, (note - 69) / 12.0) / sampleRate;
        envStage    = EnvStage::Attack;
        envLevel    = 0.0f;
    }

    void noteOff()
    {
        if (envStage != EnvStage::Off)
            envStage = EnvStage::Release;
    }

    void kill()
    {
        isActive  = false;
        envStage  = EnvStage::Off;
        envLevel  = 0.0f;
    }
};

// ============================================================================
// Processor
// ============================================================================
class TestSynthProcessor : public juce::AudioProcessor
{
public:
    TestSynthProcessor();
    ~TestSynthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

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
    static constexpr int kMaxVoices = 8;

    float generateSample (SynthVoice& voice, int oscType);
    void  processEnvelope (SynthVoice& voice, float attack, float decay,
                           float sustain, float release);

    juce::AudioProcessorValueTreeState apvts;
    std::array<SynthVoice, kMaxVoices> voices;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestSynthProcessor)
};
