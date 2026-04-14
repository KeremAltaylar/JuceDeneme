#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "StepSequencer.h"
#include "WarpingSamplerEngine.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    void loadSampleFromFile (const juce::File& file);

    SampleData::Ptr getLoadedSampleForUI() const { return loadedSampleForUI; }
    std::function<void()> onSampleLoaded;

private:
    void updateSequencerCacheFromApvts();
    void updateEngineParamsFromApvts();

    juce::AudioFormatManager formatManager;
    WarpingSamplerEngine samplerEngine;
    StepSequencer sequencer;
    juce::AudioProcessorValueTreeState apvts;

    juce::ThreadPool loaderPool { 1 };

    std::array<bool, 16> stepCache {};

    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* warpParam = nullptr;
    std::atomic<float>* sequenceLengthParam = nullptr;
    std::array<std::atomic<float>*, 16> stepParams {};

    SampleData::Ptr loadedSampleForUI;
    std::atomic<SampleData*> sampleForAudio { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
