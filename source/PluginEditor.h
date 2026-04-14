#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

#include "DropZoneComponent.h"
#include "WaveformComponent.h"
#include "StepGridComponent.h"

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

    DropZoneComponent dropZone;
    WaveformComponent waveform;

    juce::Slider speedKnob;
    juce::ToggleButton warpToggle { "Warp" };
    juce::Slider sequenceLengthSlider;
    StepGridComponent stepGrid;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<ButtonAttachment> warpAttachment;
    std::unique_ptr<SliderAttachment> sequenceLengthAttachment;

    juce::File currentFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
