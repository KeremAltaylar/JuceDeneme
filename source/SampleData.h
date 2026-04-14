#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class SampleData : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    SampleData (juce::AudioBuffer<float> dataIn, double sourceSampleRateIn)
        : data (std::move (dataIn)), sourceSampleRate (sourceSampleRateIn)
    {
    }

    const juce::AudioBuffer<float>& getAudio() const { return data; }
    double getSourceSampleRate() const { return sourceSampleRate; }

private:
    juce::AudioBuffer<float> data;
    double sourceSampleRate = 44100.0;
};
