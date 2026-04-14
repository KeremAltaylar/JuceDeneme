#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
     , apvts (*this, nullptr, "PARAMETERS", Parameters::createLayout())
{
    formatManager.registerBasicFormats();

    samplerEngine.setSampleSource (0, [this]() { return getSampleForAudio (0); });
    samplerEngine.setSampleSource (1, [this]() { return getSampleForAudio (1); });

    speedParam = apvts.getRawParameterValue (ParameterIDs::speed);
    warpParam = apvts.getRawParameterValue (ParameterIDs::warp);
    sequenceLengthParam = apvts.getRawParameterValue (ParameterIDs::sequenceLength);
    for (int i = 0; i < 16; ++i)
        stepParams[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::stepId (i));

    transportPlayParam = apvts.getRawParameterValue (ParameterIDs::transportPlay);
    tempoBpmParam = apvts.getRawParameterValue (ParameterIDs::tempoBpm);
    autoLengthParam = apvts.getRawParameterValue (ParameterIDs::autoLength);

    for (int i = 0; i < 16; ++i)
    {
        stepSamplerParams[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::stepSamplerId (i));
        stepOnsetParams[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::stepOnsetId (i));
        stepLengthMsParams[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::stepLengthMsId (i));
    }

    driveParam = apvts.getRawParameterValue (ParameterIDs::drive);
    toneParam = apvts.getRawParameterValue (ParameterIDs::tone);
    delayMixParam = apvts.getRawParameterValue (ParameterIDs::delayMix);
    delayFeedbackParam = apvts.getRawParameterValue (ParameterIDs::delayFeedback);
    delayTimeMsParam = apvts.getRawParameterValue (ParameterIDs::delayTimeMs);
    delaySyncParam = apvts.getRawParameterValue (ParameterIDs::delaySync);
    delayDivisionParam = apvts.getRawParameterValue (ParameterIDs::delayDivision);
    reverbMixParam = apvts.getRawParameterValue (ParameterIDs::reverbMix);
    reverbRoomSizeParam = apvts.getRawParameterValue (ParameterIDs::reverbRoomSize);
    reverbDampingParam = apvts.getRawParameterValue (ParameterIDs::reverbDamping);
}

PluginProcessor::~PluginProcessor()
{
    const juce::SpinLock::ScopedLockType lock (sharedDataLock);
    for (auto& s : sampleForAudio)
        s = nullptr;
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastPreparedBlockSize = juce::jmax (1, juce::jmax (samplesPerBlock, 16384));
    lastPreparedNumChannels = juce::jmax (1, getTotalNumOutputChannels());

    samplerEngine.prepare (sampleRate, lastPreparedBlockSize.load (std::memory_order_relaxed), lastPreparedNumChannels);
    sequencer.prepare (sampleRate);

    fxChain.prepare (sampleRate, lastPreparedBlockSize.load (std::memory_order_relaxed), lastPreparedNumChannels);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) lastPreparedBlockSize.load (std::memory_order_relaxed), (juce::uint32) lastPreparedNumChannels };
    limiter.prepare (spec);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const int preparedBlockSize = lastPreparedBlockSize.load (std::memory_order_relaxed);

    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::clear (buffer.getWritePointer (ch), numSamples);

    updateEngineParamsFromApvts();
    updateSequencerCacheFromApvts();

    const int seqLen = (int) (sequenceLengthParam != nullptr ? sequenceLengthParam->load() : 16.0f);

    const bool internalPlay = transportPlayParam != nullptr ? (transportPlayParam->load() >= 0.5f) : false;
    double bpm = tempoBpmParam != nullptr ? (double) tempoBpmParam->load() : 120.0;
    bool hostPlaying = false;
    bool isPlaying = internalPlay;
    double ppqStart = internalPpq;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (hostPlaying)
            {
                isPlaying = true;
                if (auto hostBpm = pos->getBpm())
                    if (*hostBpm > 0.0)
                        bpm = *hostBpm;

                if (auto hostPpq = pos->getPpqPosition())
                    ppqStart = *hostPpq;
            }
        }
    }

    std::array<StepSequencer::Event, 64> events {};
    const int eventCount = sequencer.createStepStartEvents (
        events,
        numSamples,
        seqLen,
        isPlaying,
        bpm,
        ppqStart);

    const bool autoLengthEnabled = autoLengthParam != nullptr ? (autoLengthParam->load() >= 0.5f) : true;
    const double secondsPerBar = bpm > 0.0 ? (60.0 / bpm) * 4.0 : 2.0;
    const int autoLengthSamples = (int) std::round ((secondsPerBar / (double) juce::jmax (1, seqLen)) * getSampleRate());

    int renderedUpTo = 0;
    for (int i = 0; i < eventCount; ++i)
    {
        const auto e = events[(size_t) i];
        const int segmentLen = juce::jlimit (0, numSamples - renderedUpTo, e.sampleOffset - renderedUpTo);
        if (segmentLen > 0)
        {
            samplerEngine.render (buffer, renderedUpTo, segmentLen);
            renderedUpTo += segmentLen;
        }

        const int stepIndex = juce::jlimit (0, 15, e.stepIndex);
        if (stepCache[(size_t) stepIndex])
        {
            const int slot = juce::jlimit (0, 1, stepSamplerCache[(size_t) stepIndex]);
            const int onsetIndex = juce::jmax (0, stepOnsetCache[(size_t) stepIndex]);
            const int onsetSample = getOnsetSample (slot, onsetIndex);

            const float lengthMsOverride = stepLengthMsCache[(size_t) stepIndex];
            int lengthSamples = autoLengthSamples;
            if (! autoLengthEnabled && lengthMsOverride > 0.0f)
                lengthSamples = (int) std::round ((double) lengthMsOverride * 0.001 * getSampleRate());
            else if (autoLengthEnabled && lengthMsOverride > 0.0f)
                lengthSamples = (int) std::round ((double) lengthMsOverride * 0.001 * getSampleRate());

            lengthSamples = juce::jlimit (1, (int) (getSampleRate() * 4.0), lengthSamples);
            samplerEngine.trigger (slot, onsetSample, lengthSamples, 1.0f);
        }
    }

    if (renderedUpTo < numSamples)
        samplerEngine.render (buffer, renderedUpTo, numSamples - renderedUpTo);

    fxChain.setParameters (
        driveParam != nullptr ? driveParam->load() : 0.0f,
        toneParam != nullptr ? toneParam->load() : 4000.0f,
        delayMixParam != nullptr ? delayMixParam->load() : 0.0f,
        delayFeedbackParam != nullptr ? delayFeedbackParam->load() : 0.0f,
        delayTimeMsParam != nullptr ? delayTimeMsParam->load() : 250.0f,
        delaySyncParam != nullptr ? (delaySyncParam->load() >= 0.5f) : true,
        delayDivisionParam != nullptr ? (int) delayDivisionParam->load() : 2,
        reverbMixParam != nullptr ? reverbMixParam->load() : 0.0f,
        reverbRoomSizeParam != nullptr ? reverbRoomSizeParam->load() : 0.5f,
        reverbDampingParam != nullptr ? reverbDampingParam->load() : 0.5f,
        bpm);

    fxChain.process (buffer);

    {
        juce::dsp::AudioBlock<float> block (buffer);
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = juce::jmin (preparedBlockSize, numSamples - offset);
            auto subBlock = block.getSubBlock ((size_t) offset, (size_t) chunk);
            juce::dsp::ProcessContextReplacing<float> ctx (subBlock);
            limiter.process (ctx);
            offset += chunk;
        }
    }

    if (! hostPlaying && internalPlay)
        internalPpq += (bpm / (60.0 * getSampleRate())) * (double) numSamples;

    midiMessages.clear();
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void PluginProcessor::updateSequencerCacheFromApvts()
{
    for (int i = 0; i < 16; ++i)
    {
        const auto* p = stepParams[(size_t) i];
        stepCache[(size_t) i] = (p != nullptr && p->load() >= 0.5f);

        const auto* s = stepSamplerParams[(size_t) i];
        stepSamplerCache[(size_t) i] = s != nullptr ? (int) s->load() : 0;

        const auto* o = stepOnsetParams[(size_t) i];
        stepOnsetCache[(size_t) i] = o != nullptr ? (int) o->load() : 0;

        const auto* l = stepLengthMsParams[(size_t) i];
        stepLengthMsCache[(size_t) i] = l != nullptr ? l->load() : 0.0f;
    }
}

void PluginProcessor::updateEngineParamsFromApvts()
{
    const float speed = speedParam != nullptr ? speedParam->load() : 1.0f;
    const bool warpEnabled = warpParam != nullptr ? (warpParam->load() >= 0.5f) : true;
    samplerEngine.setParameters (speed, warpEnabled);
}

namespace
{
    class SampleLoadJob final : public juce::ThreadPoolJob
    {
    public:
        SampleLoadJob (juce::AudioFormatManager& fm, juce::File f, int slot, std::function<void (int, SampleData::Ptr)> cb)
            : ThreadPoolJob ("SampleLoadJob"), formatManager (fm), file (std::move (f)), callback (std::move (cb))
            , slotIndex (slot)
        {
        }

        JobStatus runJob() override
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
            if (reader == nullptr)
            {
                juce::MessageManager::callAsync ([cb = callback, slot = slotIndex]() { cb (slot, nullptr); });
                return jobHasFinished;
            }

            const int numChannels = (int) reader->numChannels;
            const juce::int64 numSamples64 = reader->lengthInSamples;
            if (numChannels <= 0 || numSamples64 <= 0 || numSamples64 > (juce::int64) (30 * reader->sampleRate))
            {
                juce::MessageManager::callAsync ([cb = callback, slot = slotIndex]() { cb (slot, nullptr); });
                return jobHasFinished;
            }

            juce::AudioBuffer<float> audio (numChannels, (int) numSamples64);
            reader->read (&audio, 0, (int) numSamples64, 0, true, true);

            auto sample = SampleData::Ptr (new SampleData (std::move (audio), reader->sampleRate));
            juce::MessageManager::callAsync ([cb = callback, sample, slot = slotIndex]() { cb (slot, sample); });
            return jobHasFinished;
        }

    private:
        juce::AudioFormatManager& formatManager;
        juce::File file;
        std::function<void (int, SampleData::Ptr)> callback;
        int slotIndex = 0;
    };

    class OnsetAnalyzeJob final : public juce::ThreadPoolJob
    {
    public:
        OnsetAnalyzeJob (SampleData::Ptr sampleIn, int slot, int maxOnsetsIn, std::function<void (int, std::array<int, 256>, int)> cb)
            : ThreadPoolJob ("OnsetAnalyzeJob"), sample (std::move (sampleIn)), slotIndex (slot), maxOnsets (maxOnsetsIn), callback (std::move (cb))
        {
        }

        JobStatus runJob() override
        {
            std::array<int, 256> onsets {};
            int count = 0;

            if (sample == nullptr)
            {
                juce::MessageManager::callAsync ([cb = callback, slot = slotIndex, onsets, count]() { cb (slot, onsets, count); });
                return jobHasFinished;
            }

            const auto& audio = sample->getAudio();
            const int numSamples = audio.getNumSamples();
            const int numChannels = audio.getNumChannels();

            if (numSamples <= 1 || numChannels <= 0)
            {
                juce::MessageManager::callAsync ([cb = callback, slot = slotIndex, onsets, count]() { cb (slot, onsets, count); });
                return jobHasFinished;
            }

            const int frameSize = 1024;
            const int hopSize = 512;
            const int minInterval = 2048;
            const double alpha = 0.01;
            const double thresholdRatio = 3.0;
            const double risingRatio = 1.5;

            onsets[(size_t) count++] = 0;
            int lastOnset = 0;
            double meanEnergy = 0.0;
            double lastEnergy = 0.0;

            for (int pos = 0; pos + frameSize < numSamples; pos += hopSize)
            {
                double e = 0.0;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* x = audio.getReadPointer (ch) + pos;
                    for (int i = 0; i < frameSize; ++i)
                    {
                        const double v = (double) x[i];
                        e += v * v;
                    }
                }

                if (meanEnergy == 0.0)
                    meanEnergy = e;
                else
                    meanEnergy = (1.0 - alpha) * meanEnergy + alpha * e;

                const bool isRising = (lastEnergy > 0.0) ? (e > lastEnergy * risingRatio) : true;
                const bool isAbove = (meanEnergy > 0.0) ? (e > meanEnergy * thresholdRatio) : false;

                if (isAbove && isRising && (pos - lastOnset) >= minInterval)
                {
                    if (count < juce::jmin (maxOnsets, (int) onsets.size()))
                    {
                        onsets[(size_t) count++] = pos;
                        lastOnset = pos;
                    }
                    else
                    {
                        break;
                    }
                }

                lastEnergy = e;
            }

            juce::MessageManager::callAsync ([cb = callback, slot = slotIndex, onsets, count]() { cb (slot, onsets, count); });
            return jobHasFinished;
        }

    private:
        SampleData::Ptr sample;
        int slotIndex = 0;
        int maxOnsets = 256;
        std::function<void (int, std::array<int, 256>, int)> callback;
    };
}

void PluginProcessor::loadSampleFromFile (int slotIndex, const juce::File& file)
{
    if (slotIndex < 0 || slotIndex >= 2)
        return;
    if (! file.existsAsFile())
        return;

    loaderPool.addJob (new SampleLoadJob (formatManager, file, slotIndex, [this] (int slot, SampleData::Ptr sample)
    {
        loadedSampleForUI[(size_t) slot] = sample;
        {
            const juce::SpinLock::ScopedLockType lock (sharedDataLock);
            sampleForAudio[(size_t) slot] = sample;
            onsetCount[(size_t) slot] = 0;
            for (int i = 0; i < maxOnsets; ++i)
                onsetSamples[(size_t) slot][(size_t) i] = 0;
        }
        if (onSampleLoaded)
            onSampleLoaded (slot);
    }), true);
}

void PluginProcessor::analyzeOnsets (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2)
        return;

    auto sample = loadedSampleForUI[(size_t) slotIndex];
    if (sample == nullptr)
        return;

    loaderPool.addJob (new OnsetAnalyzeJob (sample, slotIndex, maxOnsets, [this] (int slot, std::array<int, 256> onsets, int count)
    {
        const int safeCount = juce::jlimit (0, maxOnsets, count);
        {
            const juce::SpinLock::ScopedLockType lock (sharedDataLock);
            for (int i = 0; i < maxOnsets; ++i)
                onsetSamples[(size_t) slot][(size_t) i] = (i < safeCount ? onsets[(size_t) i] : 0);
            onsetCount[(size_t) slot] = safeCount;
        }
        if (onOnsetsAnalyzed)
            onOnsetsAnalyzed (slot);
    }), true);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
