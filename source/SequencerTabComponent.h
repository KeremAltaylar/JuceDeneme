#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters.h"
#include "PluginProcessor.h"

class SequencerTabComponent : public juce::Component
{
public:
    SequencerTabComponent (PluginProcessor& p)
        : processor (p), state (p.getAPVTS())
    {
        addAndMakeVisible (playToggle);
        addAndMakeVisible (tempoKnob);
        addAndMakeVisible (autoLengthToggle);
        addAndMakeVisible (sequenceLength);
        addAndMakeVisible (speedKnob);
        addAndMakeVisible (warpToggle);
        addAndMakeVisible (randomizeButton);

        addAndMakeVisible (grid);

        playToggle.setButtonText ("Play");
        autoLengthToggle.setButtonText ("Auto-Length");
        warpToggle.setButtonText ("Warp");
        randomizeButton.setButtonText ("Randomize");

        tempoKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        tempoKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        speedKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        speedKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        sequenceLength.setSliderStyle (juce::Slider::LinearHorizontal);
        sequenceLength.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);

        playAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::transportPlay, playToggle);
        tempoAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::tempoBpm, tempoKnob);
        autoLengthAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::autoLength, autoLengthToggle);
        sequenceLengthAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::sequenceLength, sequenceLength);
        speedAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::speed, speedKnob);
        warpAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::warp, warpToggle);

        randomizeButton.onClick = [this] { randomizeActiveSteps(); };
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);

        auto header = area.removeFromTop (86);
        auto left = header.removeFromLeft (380);
        playToggle.setBounds (left.removeFromLeft (70).removeFromTop (22));
        left.removeFromLeft (10);
        autoLengthToggle.setBounds (left.removeFromLeft (120).removeFromTop (22));
        left.removeFromLeft (10);
        randomizeButton.setBounds (left.removeFromLeft (120).removeFromTop (22));

        auto knobs = header;
        tempoKnob.setBounds (knobs.removeFromLeft (120));
        speedKnob.setBounds (knobs.removeFromLeft (120));
        warpToggle.setBounds (knobs.removeFromTop (22));
        knobs.removeFromTop (10);
        sequenceLength.setBounds (knobs.removeFromTop (22));

        area.removeFromTop (10);
        grid.setBounds (area);
    }

private:
    class Grid : public juce::Component
    {
    public:
        explicit Grid (juce::AudioProcessorValueTreeState& apvts)
            : state (apvts)
        {
            for (int i = 0; i < 16; ++i)
            {
                auto& t = stepToggles[(size_t) i];
                auto& s = samplerChoice[(size_t) i];
                auto& o = onsetIndex[(size_t) i];
                auto& l = lengthMs[(size_t) i];

                t.setButtonText (juce::String (i + 1));
                t.setClickingTogglesState (true);
                addAndMakeVisible (t);

                s.addItem ("S1", 1);
                s.addItem ("S2", 2);
                addAndMakeVisible (s);

                o.setSliderStyle (juce::Slider::LinearVertical);
                o.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                o.setRange (0.0, 255.0, 1.0);
                addAndMakeVisible (o);

                l.setSliderStyle (juce::Slider::LinearVertical);
                l.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                l.setRange (0.0, 2000.0, 1.0);
                addAndMakeVisible (l);

                toggleAttachments[(size_t) i] = std::make_unique<ButtonAttachment> (state, ParameterIDs::stepId (i), t);
                samplerAttachments[(size_t) i] = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::stepSamplerId (i), s);
                onsetAttachments[(size_t) i] = std::make_unique<SliderAttachment> (state, ParameterIDs::stepOnsetId (i), o);
                lengthAttachments[(size_t) i] = std::make_unique<SliderAttachment> (state, ParameterIDs::stepLengthMsId (i), l);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            const int cols = 8;
            const int rows = 2;
            const int pad = 6;
            const int cellW = (area.getWidth() - pad * (cols - 1)) / cols;
            const int cellH = (area.getHeight() - pad * (rows - 1)) / rows;

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    const int i = r * cols + c;
                    auto cell = juce::Rectangle<int> (
                        area.getX() + c * (cellW + pad),
                        area.getY() + r * (cellH + pad),
                        cellW,
                        cellH);

                    auto top = cell.removeFromTop (22);
                    stepToggles[(size_t) i].setBounds (top.removeFromLeft (cellW / 2));
                    samplerChoice[(size_t) i].setBounds (top);

                    auto half = cell.getWidth() / 2;
                    onsetIndex[(size_t) i].setBounds (cell.removeFromLeft (half).reduced (0, 4));
                    lengthMs[(size_t) i].setBounds (cell.reduced (0, 4));
                }
            }
        }

    private:
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
        using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

        juce::AudioProcessorValueTreeState& state;
        std::array<juce::ToggleButton, 16> stepToggles;
        std::array<juce::ComboBox, 16> samplerChoice;
        std::array<juce::Slider, 16> onsetIndex;
        std::array<juce::Slider, 16> lengthMs;

        std::array<std::unique_ptr<ButtonAttachment>, 16> toggleAttachments;
        std::array<std::unique_ptr<ComboBoxAttachment>, 16> samplerAttachments;
        std::array<std::unique_ptr<SliderAttachment>, 16> onsetAttachments;
        std::array<std::unique_ptr<SliderAttachment>, 16> lengthAttachments;
    };

    void randomizeActiveSteps()
    {
        juce::Random rng;

        for (int i = 0; i < 16; ++i)
        {
            if (auto* activeParam = state.getParameter (ParameterIDs::stepId (i)))
            {
                const float isActive = activeParam->getValue();
                if (isActive < 0.5f)
                    continue;
            }

            const int slot = rng.nextBool() ? 0 : 1;
            const int maxOnsets = processor.getOnsetCount (slot);
            const int onset = maxOnsets > 0 ? rng.nextInt (maxOnsets) : 0;

            if (auto* p = state.getParameter (ParameterIDs::stepSamplerId (i)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) slot));

            if (auto* p = state.getParameter (ParameterIDs::stepOnsetId (i)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) onset));

            if (auto* p = state.getParameter (ParameterIDs::stepLengthMsId (i)))
            {
                const float ms = rng.nextBool() ? 0.0f : (float) (50 + rng.nextInt (700));
                p->setValueNotifyingHost (p->convertTo0to1 (ms));
            }
        }
    }

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    juce::ToggleButton playToggle;
    juce::Slider tempoKnob;
    juce::ToggleButton autoLengthToggle;
    juce::Slider sequenceLength;
    juce::Slider speedKnob;
    juce::ToggleButton warpToggle;
    juce::TextButton randomizeButton;

    std::unique_ptr<ButtonAttachment> playAttachment;
    std::unique_ptr<SliderAttachment> tempoAttachment;
    std::unique_ptr<ButtonAttachment> autoLengthAttachment;
    std::unique_ptr<SliderAttachment> sequenceLengthAttachment;
    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<ButtonAttachment> warpAttachment;

    Grid grid { state };
};

