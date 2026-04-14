#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), stepGrid (p.getAPVTS())
{
    addAndMakeVisible (inspectButton);
    addAndMakeVisible (dropZone);
    addAndMakeVisible (waveform);
    addAndMakeVisible (speedKnob);
    addAndMakeVisible (warpToggle);
    addAndMakeVisible (sequenceLengthSlider);
    addAndMakeVisible (stepGrid);

    speedKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    speedKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

    sequenceLengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sequenceLengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);

    speedAttachment = std::make_unique<SliderAttachment> (processorRef.getAPVTS(), ParameterIDs::speed, speedKnob);
    warpAttachment = std::make_unique<ButtonAttachment> (processorRef.getAPVTS(), ParameterIDs::warp, warpToggle);
    sequenceLengthAttachment = std::make_unique<SliderAttachment> (processorRef.getAPVTS(), ParameterIDs::sequenceLength, sequenceLengthSlider);

    dropZone.onFileDropped = [this] (const juce::File& file)
    {
        currentFile = file;
        waveform.setFile (file);
        processorRef.loadSampleFromFile (file);
    };

    processorRef.onSampleLoaded = [this]
    {
        repaint();
    };

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (640, 420);
}

PluginEditor::~PluginEditor()
{
    processorRef.onSampleLoaded = nullptr;
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (14.0f);
    g.drawFittedText (
        juce::String (PRODUCT_NAME_WITHOUT_VERSION) + "  v" + VERSION,
        getLocalBounds().removeFromTop (24).reduced (10, 0),
        juce::Justification::centredLeft,
        1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (30);
    inspectButton.setBounds (header.removeFromRight (120).reduced (6, 2));

    auto top = area.removeFromTop (110).reduced (10, 6);
    dropZone.setBounds (top.removeFromLeft (160));
    top.removeFromLeft (10);
    waveform.setBounds (top);

    auto controls = area.removeFromTop (80).reduced (10, 6);
    speedKnob.setBounds (controls.removeFromLeft (120));
    controls.removeFromLeft (10);
    warpToggle.setBounds (controls.removeFromTop (22));
    controls.removeFromTop (10);
    sequenceLengthSlider.setBounds (controls.removeFromTop (22));

    auto grid = area.reduced (10, 6);
    stepGrid.setBounds (grid);
}
