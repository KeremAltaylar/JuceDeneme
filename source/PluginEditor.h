#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

#include "SamplerTabComponent.h"
#include "SequencerTabComponent.h"
#include "FxTabComponent.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    SamplerTabComponent sampler1;
    SamplerTabComponent sampler2;
    SequencerTabComponent sequencer;
    FxTabComponent fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
