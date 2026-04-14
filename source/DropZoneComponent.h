#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DropZoneComponent : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFileDropped;

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() > 0;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (files.size() == 0)
            return;
        if (onFileDropped)
            onFileDropped (juce::File (files[0]));
    }

    void fileDragEnter (const juce::StringArray&, int, int) override
    {
        dragging = true;
        repaint();
    }

    void fileDragExit (const juce::StringArray&) override
    {
        dragging = false;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (dragging ? juce::Colours::orange : juce::Colours::grey);
        g.drawRoundedRectangle (r.reduced (2.0f), 8.0f, 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (15.0f);
        g.drawFittedText (
            "Drop WAV/MP3 here",
            getLocalBounds().reduced (10),
            juce::Justification::centred,
            1);
    }

private:
    bool dragging = false;
};

