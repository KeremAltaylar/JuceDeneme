#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p)
    , processorRef (p)
    , sampler1 (p, 0)
    , sampler2 (p, 1)
    , sequencer (p)
    , fx (p.getAPVTS())
{
    addAndMakeVisible (inspectButton);

    addAndMakeVisible (tabs);
    tabs.addTab ("Sampler 1", juce::Colours::transparentBlack, &sampler1, false);
    tabs.addTab ("Sampler 2", juce::Colours::transparentBlack, &sampler2, false);
    tabs.addTab ("Sequencer", juce::Colours::transparentBlack, &sequencer, false);
    tabs.addTab ("FX", juce::Colours::transparentBlack, &fx, false);

    processorRef.onSampleLoaded = [this] (int slot)
    {
        if (slot == 0)
            sampler1.refresh();
        else if (slot == 1)
            sampler2.refresh();
    };

    processorRef.onOnsetsAnalyzed = [this] (int slot)
    {
        if (slot == 0)
            sampler1.refresh();
        else if (slot == 1)
            sampler2.refresh();
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
    setSize (860, 520);
}

PluginEditor::~PluginEditor()
{
    processorRef.onSampleLoaded = nullptr;
    processorRef.onOnsetsAnalyzed = nullptr;
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
    inspectButton.setBounds (header.removeFromRight (140).reduced (6, 2));
    tabs.setBounds (area);
}
