#include "SynthEditor.h"

static void setupKnob (juce::Slider& slider, juce::Label& label,
                        const juce::String& text, juce::Component* parent)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    parent->addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setName (text + "Label");
    parent->addAndMakeVisible (label);
}

// ============================================================================
TestSynthEditor::TestSynthEditor (TestSynthProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    titleLabel.setText ("Test Synth Plugin", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setName ("TitleLabel");
    addAndMakeVisible (titleLabel);

    // Oscillator type
    oscLabel.setText ("Waveform", juce::dontSendNotification);
    oscLabel.setJustificationType (juce::Justification::centred);
    oscLabel.setName ("OscLabel");
    addAndMakeVisible (oscLabel);

    oscTypeBox.addItem ("Sine",   1);
    oscTypeBox.addItem ("Saw",    2);
    oscTypeBox.addItem ("Square", 3);
    oscTypeBox.setName ("OscTypeBox");
    addAndMakeVisible (oscTypeBox);
    oscAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getAPVTS(), "osc_type", oscTypeBox);

    // ADSR + Volume knobs
    setupKnob (attackSlider,  attackLabel,  "Attack",  this);
    setupKnob (decaySlider,   decayLabel,   "Decay",   this);
    setupKnob (sustainSlider, sustainLabel, "Sustain", this);
    setupKnob (releaseSlider, releaseLabel, "Release", this);
    setupKnob (volumeSlider,  volumeLabel,  "Volume",  this);

    attackAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "attack",  attackSlider);
    decayAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "decay",   decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "sustain", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "release", releaseSlider);
    volumeAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getAPVTS(), "volume",  volumeSlider);

    setSize (600, 400);
}

TestSynthEditor::~TestSynthEditor() = default;

// ============================================================================
void TestSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a2a3a));

    g.setColour (juce::Colour (0xff3a4a5a));
    auto area = getLocalBounds().reduced (10).withTrimmedTop (80);
    int knobWidth = area.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto col = area.removeFromLeft (knobWidth);
        g.drawRoundedRectangle (col.toFloat().reduced (3), 6.0f, 1.0f);
    }
}

// ============================================================================
void TestSynthEditor::resized()
{
    auto area = getLocalBounds();

    titleLabel.setBounds (area.removeFromTop (40));

    // Oscillator row
    auto oscRow = area.removeFromTop (40).reduced (100, 5);
    oscLabel.setBounds (oscRow.removeFromLeft (80));
    oscTypeBox.setBounds (oscRow.reduced (10, 2));

    // ADSR + Volume row
    area.reduce (10, 5);
    int knobWidth = area.getWidth() / 5;

    auto makeKnobArea = [&] () -> juce::Rectangle<int> {
        return area.removeFromLeft (knobWidth);
    };

    auto a = makeKnobArea();
    attackLabel.setBounds (a.removeFromTop (20));
    attackSlider.setBounds (a.reduced (3));

    auto d = makeKnobArea();
    decayLabel.setBounds (d.removeFromTop (20));
    decaySlider.setBounds (d.reduced (3));

    auto s = makeKnobArea();
    sustainLabel.setBounds (s.removeFromTop (20));
    sustainSlider.setBounds (s.reduced (3));

    auto r = makeKnobArea();
    releaseLabel.setBounds (r.removeFromTop (20));
    releaseSlider.setBounds (r.reduced (3));

    auto v = area;
    volumeLabel.setBounds (v.removeFromTop (20));
    volumeSlider.setBounds (v.reduced (3));
}
