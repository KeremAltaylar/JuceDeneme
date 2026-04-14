#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Parameters.h"
#include "StepSequencer.h"
#include "DualWarpingSamplerEngine.h"
#include "FxChain.h"

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

    void loadSampleFromFile (int slotIndex, const juce::File& file);
    void analyzeOnsets (int slotIndex);

    SampleData::Ptr getSampleForAudio (int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return nullptr;
        const juce::SpinLock::ScopedLockType lock (sharedDataLock);
        return sampleForAudio[(size_t) slotIndex];
    }

    SampleData::Ptr getLoadedSampleForUI (int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return nullptr;
        return loadedSampleForUI[(size_t) slotIndex];
    }

    int getOnsetCount (int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return 0;
        const juce::SpinLock::ScopedLockType lock (sharedDataLock);
        return onsetCount[(size_t) slotIndex];
    }

    int getOnsetSample (int slotIndex, int onsetIndex) const
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return 0;
        const juce::SpinLock::ScopedLockType lock (sharedDataLock);
        const int count = onsetCount[(size_t) slotIndex];
        onsetIndex = juce::jlimit (0, juce::jmax (0, count - 1), onsetIndex);
        return onsetSamples[(size_t) slotIndex][(size_t) onsetIndex];
    }

    std::function<void (int)> onSampleLoaded;
    std::function<void (int)> onOnsetsAnalyzed;

private:
    void updateSequencerCacheFromApvts();
    void updateEngineParamsFromApvts();

    juce::AudioFormatManager formatManager;
    DualWarpingSamplerEngine samplerEngine;
    FxChain fxChain;
    juce::dsp::Limiter<float> limiter;
    StepSequencer sequencer;
    juce::AudioProcessorValueTreeState apvts;

    juce::ThreadPool loaderPool { 1 };

    mutable juce::SpinLock sharedDataLock;

    std::array<bool, 16> stepCache {};
    std::array<int, 16> stepSamplerCache {};
    std::array<int, 16> stepOnsetCache {};
    std::array<float, 16> stepLengthMsCache {};

    double internalPpq = 0.0;

    std::atomic<int> lastPreparedBlockSize { 0 };
    std::atomic<int> lastPreparedNumChannels { 0 };

    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* warpParam = nullptr;
    std::atomic<float>* sequenceLengthParam = nullptr;
    std::array<std::atomic<float>*, 16> stepParams {};
    std::array<std::atomic<float>*, 16> stepSamplerParams {};
    std::array<std::atomic<float>*, 16> stepOnsetParams {};
    std::array<std::atomic<float>*, 16> stepLengthMsParams {};

    std::atomic<float>* transportPlayParam = nullptr;
    std::atomic<float>* tempoBpmParam = nullptr;
    std::atomic<float>* autoLengthParam = nullptr;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* delayMixParam = nullptr;
    std::atomic<float>* delayFeedbackParam = nullptr;
    std::atomic<float>* delayTimeMsParam = nullptr;
    std::atomic<float>* delaySyncParam = nullptr;
    std::atomic<float>* delayDivisionParam = nullptr;
    std::atomic<float>* reverbMixParam = nullptr;
    std::atomic<float>* reverbRoomSizeParam = nullptr;
    std::atomic<float>* reverbDampingParam = nullptr;

    std::array<SampleData::Ptr, 2> loadedSampleForUI;
    std::array<SampleData::Ptr, 2> sampleForAudio;

    static constexpr int maxOnsets = 256;
    std::array<std::array<int, (size_t) maxOnsets>, 2> onsetSamples {};
    std::array<int, 2> onsetCount { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
