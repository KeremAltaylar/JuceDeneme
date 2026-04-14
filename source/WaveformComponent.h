#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class WaveformComponent : public juce::Component, private juce::ChangeListener
{
public:
    WaveformComponent()
        : thumbnailCache (8), thumbnail (2048, formatManager, thumbnailCache)
    {
        formatManager.registerBasicFormats();
        thumbnail.addChangeListener (this);
        thread.startThread();
        thumbnail.setBackgroundThread (&thread);
    }

    ~WaveformComponent() override
    {
        thumbnail.removeChangeListener (this);
        thumbnail.setBackgroundThread (nullptr);
        thread.stopThread (1000);
    }

    void setFile (const juce::File& file)
    {
        currentFile = file;
        thumbnail.setSource (new juce::FileInputSource (file));
        repaint();
    }

    void clear()
    {
        currentFile = {};
        thumbnail.setSource (nullptr);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.25f));
        g.setColour (juce::Colours::white.withAlpha (0.8f));

        auto r = getLocalBounds().reduced (6);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);

        if (thumbnail.getTotalLength() > 0.0)
        {
            g.setColour (juce::Colours::aqua.withAlpha (0.9f));
            thumbnail.drawChannels (g, r, 0.0, thumbnail.getTotalLength(), 1.0f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (13.0f);
            g.drawFittedText ("No sample loaded", r, juce::Justification::centred, 1);
        }
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        repaint();
    }

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;
    juce::TimeSliceThread thread { "Waveform" };
    juce::File currentFile;
};

