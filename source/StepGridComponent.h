#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class StepGridComponent : public juce::Component
{
public:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    explicit StepGridComponent (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        for (int i = 0; i < 16; ++i)
        {
            auto& b = buttons[(size_t) i];
            b.setButtonText (juce::String (i + 1));
            b.setClickingTogglesState (true);
            addAndMakeVisible (b);
            attachments[(size_t) i] = std::make_unique<ButtonAttachment> (state, "step" + juce::String (i + 1), b);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int cols = 8;
        const int rows = 2;
        const int pad = 6;
        const int w = (area.getWidth() - pad * (cols - 1)) / cols;
        const int h = (area.getHeight() - pad * (rows - 1)) / rows;

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                const int i = r * cols + c;
                auto x = area.getX() + c * (w + pad);
                auto y = area.getY() + r * (h + pad);
                buttons[(size_t) i].setBounds (x, y, w, h);
            }
        }
    }

private:
    juce::AudioProcessorValueTreeState& state;
    std::array<juce::ToggleButton, 16> buttons;
    std::array<std::unique_ptr<ButtonAttachment>, 16> attachments;
};

