#pragma once
#include "SynthProcessor.h"

/**
 * Simple editor for the Test Synth Plugin.
 * Shows oscillator selector, ADSR knobs, and volume.
 */
class TestSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit TestSynthEditor (TestSynthProcessor& processor);
    ~TestSynthEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    TestSynthProcessor& processorRef;

    juce::Label titleLabel;

    juce::ComboBox oscTypeBox;
    juce::Label    oscLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oscAttachment;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider, volumeSlider;
    juce::Label  attackLabel,  decayLabel,  sustainLabel,  releaseLabel,  volumeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attackAttachment, decayAttachment, sustainAttachment, releaseAttachment, volumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestSynthEditor)
};
