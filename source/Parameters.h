#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterIDs
{
    inline constexpr const char* speed = "speed";
    inline constexpr const char* warp = "warp";
    inline constexpr const char* sequenceLength = "sequenceLength";

    inline juce::String stepId (int stepIndexZeroBased)
    {
        return "step" + juce::String (stepIndexZeroBased + 1);
    }
}

struct Parameters
{
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::speed,
            "Speed",
            juce::NormalisableRange<float> (0.25f, 4.0f, 0.001f, 0.5f),
            1.0f));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterIDs::warp,
            "Warp",
            true));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            ParameterIDs::sequenceLength,
            "Sequence Length",
            1,
            16,
            16));

        for (int i = 0; i < 16; ++i)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ParameterIDs::stepId (i),
                "Step " + juce::String (i + 1),
                false));
        }

        return layout;
    }
};

