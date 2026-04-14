#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterIDs
{
    inline constexpr const char* transportPlay = "transportPlay";
    inline constexpr const char* tempoBpm = "tempoBpm";
    inline constexpr const char* autoLength = "autoLength";

    inline constexpr const char* speed = "speed";
    inline constexpr const char* warp = "warp";
    inline constexpr const char* sequenceLength = "sequenceLength";

    inline constexpr const char* drive = "drive";
    inline constexpr const char* tone = "tone";
    inline constexpr const char* delayMix = "delayMix";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* delayTimeMs = "delayTimeMs";
    inline constexpr const char* delaySync = "delaySync";
    inline constexpr const char* delayDivision = "delayDivision";
    inline constexpr const char* reverbMix = "reverbMix";
    inline constexpr const char* reverbRoomSize = "reverbRoomSize";
    inline constexpr const char* reverbDamping = "reverbDamping";

    inline juce::String stepId (int stepIndexZeroBased)
    {
        return "step" + juce::String (stepIndexZeroBased + 1);
    }

    inline juce::String stepOnsetId (int stepIndexZeroBased)
    {
        return "step" + juce::String (stepIndexZeroBased + 1) + "Onset";
    }

    inline juce::String stepLengthMsId (int stepIndexZeroBased)
    {
        return "step" + juce::String (stepIndexZeroBased + 1) + "LengthMs";
    }

    inline juce::String stepSamplerId (int stepIndexZeroBased)
    {
        return "step" + juce::String (stepIndexZeroBased + 1) + "Sampler";
    }
}

struct Parameters
{
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterIDs::transportPlay,
            "Play",
            false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::tempoBpm,
            "Tempo",
            juce::NormalisableRange<float> (40.0f, 240.0f, 0.01f, 1.0f),
            120.0f));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterIDs::autoLength,
            "Auto-Length",
            true));

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

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::drive,
            "Drive",
            juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f, 1.0f),
            6.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::tone,
            "Tone",
            juce::NormalisableRange<float> (200.0f, 12000.0f, 1.0f, 0.5f),
            4000.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::delayMix,
            "Delay Mix",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
            0.2f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::delayFeedback,
            "Delay Feedback",
            juce::NormalisableRange<float> (0.0f, 0.95f, 0.0001f, 1.0f),
            0.35f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::delayTimeMs,
            "Delay Time",
            juce::NormalisableRange<float> (1.0f, 1000.0f, 0.01f, 0.5f),
            250.0f));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterIDs::delaySync,
            "Delay Sync",
            true));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            ParameterIDs::delayDivision,
            "Delay Division",
            juce::StringArray { "1/4", "1/8", "1/16", "1/32" },
            2));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::reverbMix,
            "Reverb Mix",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
            0.15f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::reverbRoomSize,
            "Reverb Room Size",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
            0.5f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterIDs::reverbDamping,
            "Reverb Damping",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
            0.5f));

        for (int i = 0; i < 16; ++i)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                ParameterIDs::stepId (i),
                "Step " + juce::String (i + 1),
                false));

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                ParameterIDs::stepSamplerId (i),
                "Step " + juce::String (i + 1) + " Sampler",
                juce::StringArray { "Sampler 1", "Sampler 2" },
                0));

            layout.add (std::make_unique<juce::AudioParameterInt> (
                ParameterIDs::stepOnsetId (i),
                "Step " + juce::String (i + 1) + " Onset",
                0,
                255,
                0));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                ParameterIDs::stepLengthMsId (i),
                "Step " + juce::String (i + 1) + " Length",
                juce::NormalisableRange<float> (0.0f, 2000.0f, 0.01f, 1.0f),
                0.0f));
        }

        return layout;
    }
};
