#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "DropZoneComponent.h"
#include "PluginProcessor.h"
#include "WaveformComponent.h"

class SamplerTabComponent : public juce::Component
{
public:
    SamplerTabComponent (PluginProcessor& p, int slotIndexIn)
        : processor (p), slotIndex (slotIndexIn)
    {
        addAndMakeVisible (dropZone);
        addAndMakeVisible (waveform);
        addAndMakeVisible (analyzeButton);
        addAndMakeVisible (statusLabel);

        analyzeButton.setButtonText ("Analyze Onsets");

        dropZone.onFileDropped = [this] (const juce::File& file)
        {
            loadedFile = file;
            waveform.setFile (file);
            processor.loadSampleFromFile (slotIndex, file);
            refresh();
        };

        analyzeButton.onClick = [this]
        {
            processor.analyzeOnsets (slotIndex);
        };

        refresh();
    }

    void refresh()
    {
        const int count = processor.getOnsetCount (slotIndex);
        const auto fileName = loadedFile.existsAsFile() ? loadedFile.getFileName() : juce::String ("(none)");
        statusLabel.setText ("File: " + fileName + "    Onsets: " + juce::String (count), juce::dontSendNotification);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        statusLabel.setBounds (area.removeFromTop (22));
        area.removeFromTop (6);
        auto top = area.removeFromTop (140);
        dropZone.setBounds (top.removeFromLeft (200));
        top.removeFromLeft (10);
        waveform.setBounds (top);
        area.removeFromTop (10);
        analyzeButton.setBounds (area.removeFromTop (28).removeFromLeft (160));
    }

private:
    PluginProcessor& processor;
    const int slotIndex = 0;

    DropZoneComponent dropZone;
    WaveformComponent waveform;
    juce::TextButton analyzeButton;
    juce::Label statusLabel;
    juce::File loadedFile;
};

