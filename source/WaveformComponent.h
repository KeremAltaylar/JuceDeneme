#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SampleData.h"

class WaveformComponent : public juce::Component
{
public:
    void setSample (SampleData::Ptr sampleIn)
    {
        sample = std::move (sampleIn);
        rebuildPeaks();
        repaint();
    }

    void clear()
    {
        sample = nullptr;
        peakCount = 0;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.25f));
        g.setColour (juce::Colours::white.withAlpha (0.8f));

        auto r = getLocalBounds().reduced (6);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);

        if (sample != nullptr && peakCount > 0)
        {
            g.setColour (juce::Colours::aqua.withAlpha (0.9f));
            const auto midY = (float) r.getCentreY();
            const float halfH = (float) r.getHeight() * 0.45f;

            juce::Path path;
            path.preallocateSpace ((size_t) (peakCount * 3));

            for (int i = 0; i < peakCount; ++i)
            {
                const float x = (float) r.getX() + ((float) i / (float) (peakCount - 1)) * (float) r.getWidth();
                const float a = peaks[(size_t) i];
                const float y1 = midY - a * halfH;
                const float y2 = midY + a * halfH;
                path.startNewSubPath (x, y1);
                path.lineTo (x, y2);
            }

            g.strokePath (path, juce::PathStrokeType (1.0f));
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.setFont (13.0f);
            g.drawFittedText ("No sample loaded", r, juce::Justification::centred, 1);
        }
    }

private:
    void rebuildPeaks()
    {
        peakCount = 0;
        if (sample == nullptr)
            return;

        const auto& audio = sample->getAudio();
        const int numSamples = audio.getNumSamples();
        const int numChannels = audio.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        peakCount = (int) peaks.size();
        const int stride = juce::jmax (1, numSamples / peakCount);

        for (int i = 0; i < peakCount; ++i)
        {
            const int start = i * stride;
            const int end = juce::jmin (numSamples, start + stride);
            float maxAbs = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* x = audio.getReadPointer (ch);
                for (int s = start; s < end; ++s)
                    maxAbs = juce::jmax (maxAbs, std::abs (x[s]));
            }
            peaks[(size_t) i] = juce::jlimit (0.0f, 1.0f, maxAbs);
        }
    }

    SampleData::Ptr sample;
    std::array<float, 1024> peaks {};
    int peakCount = 0;
};
