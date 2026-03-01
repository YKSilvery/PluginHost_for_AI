#pragma once
#include "PluginProcessor.h"

/**
 * Simple editor for the Test Gain Plugin.
 * Provides sliders for gain, frequency, mix and a bypass toggle.
 */
class TestGainPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit TestGainPluginEditor (TestGainPluginProcessor& processor);
    ~TestGainPluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    TestGainPluginProcessor& processorRef;

    juce::Label titleLabel;

    juce::Slider gainSlider;
    juce::Label  gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    juce::Slider frequencySlider;
    juce::Label  frequencyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> frequencyAttachment;

    juce::Slider mixSlider;
    juce::Label  mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestGainPluginEditor)
};
