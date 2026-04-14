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

    samplerEngine.setSampleSource (&sampleForAudio);

    speedParam = apvts.getRawParameterValue (ParameterIDs::speed);
    warpParam = apvts.getRawParameterValue (ParameterIDs::warp);
    sequenceLengthParam = apvts.getRawParameterValue (ParameterIDs::sequenceLength);
    for (int i = 0; i < 16; ++i)
        stepParams[(size_t) i] = apvts.getRawParameterValue (ParameterIDs::stepId (i));
}

PluginProcessor::~PluginProcessor()
{
    if (auto* old = sampleForAudio.exchange (nullptr, std::memory_order_acq_rel))
        old->decReferenceCount();
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
    samplerEngine.prepareToPlay (sampleRate, samplesPerBlock);
    sequencer.prepare (sampleRate);
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

    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::clear (buffer.getWritePointer (ch), numSamples);

    updateEngineParamsFromApvts();
    updateSequencerCacheFromApvts();

    const int seqLen = (int) (sequenceLengthParam != nullptr ? sequenceLengthParam->load() : 16.0f);

    std::array<StepSequencer::Event, 64> events {};
    const int eventCount = sequencer.createEventsForBlock (
        events,
        getPlayHead(),
        numSamples,
        seqLen,
        stepCache);

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

        if (e.noteOn)
            samplerEngine.noteOn (60, 1.0f);
        else
            samplerEngine.noteOff (60, 0.0f, true);
    }

    if (renderedUpTo < numSamples)
        samplerEngine.render (buffer, renderedUpTo, numSamples - renderedUpTo);

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
        SampleLoadJob (juce::AudioFormatManager& fm, juce::File f, std::function<void (SampleData::Ptr)> cb)
            : ThreadPoolJob ("SampleLoadJob"), formatManager (fm), file (std::move (f)), callback (std::move (cb))
        {
        }

        JobStatus runJob() override
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
            if (reader == nullptr)
            {
                juce::MessageManager::callAsync ([cb = callback]() { cb (nullptr); });
                return jobHasFinished;
            }

            const int numChannels = (int) reader->numChannels;
            const int64 numSamples64 = reader->lengthInSamples;
            if (numChannels <= 0 || numSamples64 <= 0 || numSamples64 > (int64) (30 * reader->sampleRate))
            {
                juce::MessageManager::callAsync ([cb = callback]() { cb (nullptr); });
                return jobHasFinished;
            }

            juce::AudioBuffer<float> audio (numChannels, (int) numSamples64);
            reader->read (&audio, 0, (int) numSamples64, 0, true, true);

            auto sample = SampleData::Ptr (new SampleData (std::move (audio), reader->sampleRate));
            juce::MessageManager::callAsync ([cb = callback, sample]() { cb (sample); });
            return jobHasFinished;
        }

    private:
        juce::AudioFormatManager& formatManager;
        juce::File file;
        std::function<void (SampleData::Ptr)> callback;
    };
}

void PluginProcessor::loadSampleFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    loaderPool.addJob (new SampleLoadJob (formatManager, file, [this] (SampleData::Ptr sample)
    {
        loadedSampleForUI = sample;
        auto* newRaw = sample.get();
        if (newRaw != nullptr)
            newRaw->incReferenceCount();

        if (auto* old = sampleForAudio.exchange (newRaw, std::memory_order_acq_rel))
            old->decReferenceCount();

        if (onSampleLoaded)
            onSampleLoaded();
    }), true);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
