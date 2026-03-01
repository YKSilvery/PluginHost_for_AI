#include "PluginEditor.h"

// ============================================================================
TestGainPluginEditor::TestGainPluginEditor (TestGainPluginProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // Title
    titleLabel.setText ("Test Gain Plugin", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setName ("TitleLabel");
    addAndMakeVisible (titleLabel);

    // Gain slider
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    gainSlider.setName ("GainSlider");
    addAndMakeVisible (gainSlider);
    gainLabel.setText ("Gain (dB)", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setName ("GainLabel");
    addAndMakeVisible (gainLabel);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "gain", gainSlider);

    // Frequency slider
    frequencySlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    frequencySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    frequencySlider.setName ("FrequencySlider");
    addAndMakeVisible (frequencySlider);
    frequencyLabel.setText ("Frequency (Hz)", juce::dontSendNotification);
    frequencyLabel.setJustificationType (juce::Justification::centred);
    frequencyLabel.setName ("FrequencyLabel");
    addAndMakeVisible (frequencyLabel);
    frequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "frequency", frequencySlider);

    // Mix slider
    mixSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    mixSlider.setName ("MixSlider");
    addAndMakeVisible (mixSlider);
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setName ("MixLabel");
    addAndMakeVisible (mixLabel);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "mix", mixSlider);

    // Bypass toggle
    bypassButton.setName ("BypassButton");
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getAPVTS(), "bypass", bypassButton);

    setSize (500, 350);
}

TestGainPluginEditor::~TestGainPluginEditor() = default;

// ============================================================================
void TestGainPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2a3a));  // dark background

    // Draw a subtle border around each knob area
    g.setColour (juce::Colour (0xff4a4a5a));
    auto area = getLocalBounds().reduced (10).withTrimmedTop (50);
    int sliderWidth = area.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto col = area.removeFromLeft (sliderWidth);
        g.drawRoundedRectangle (col.toFloat().reduced (5), 8.0f, 1.0f);
    }
}

// ============================================================================
void TestGainPluginEditor::resized()
{
    auto area = getLocalBounds();

    // Title at top
    titleLabel.setBounds (area.removeFromTop (50));

    // Bypass at bottom
    auto bottomArea = area.removeFromBottom (35);
    bypassButton.setBounds (bottomArea.withSizeKeepingCentre (120, 30));

    // Three knobs side by side
    area.reduce (10, 5);
    int sliderWidth = area.getWidth() / 3;

    auto gainArea = area.removeFromLeft (sliderWidth);
    gainLabel.setBounds (gainArea.removeFromTop (20));
    gainSlider.setBounds (gainArea.reduced (5));

    auto freqArea = area.removeFromLeft (sliderWidth);
    frequencyLabel.setBounds (freqArea.removeFromTop (20));
    frequencySlider.setBounds (freqArea.reduced (5));

    auto mixArea = area;
    mixLabel.setBounds (mixArea.removeFromTop (20));
    mixSlider.setBounds (mixArea.reduced (5));
}
