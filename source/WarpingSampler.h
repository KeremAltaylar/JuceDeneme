#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SampleData.h"

class WarpingSamplerSound final : public juce::SynthesiserSound
{
public:
    WarpingSamplerSound() = default;

    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpingSamplerSound)
};

class WarpingSamplerVoice final : public juce::SynthesiserVoice
{
public:
    void setSampleSource (std::atomic<SampleData*>* source)
    {
        sampleSource = source;
    }

    void prepare (double sampleRateIn, int maxBlockSize)
    {
        hostSampleRate = sampleRateIn;
        const int grainSize = defaultGrainSize;
        const int olaSize = grainSize * 4;

        olaBuffer.setSize (2, olaSize, false, false, true);
        grainBuffer.setSize (2, grainSize, false, false, true);
        window.resize ((size_t) grainSize);
        for (int i = 0; i < grainSize; ++i)
        {
            const float phase = (float) i / (float) (grainSize - 1);
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
        }

        juce::ignoreUnused (maxBlockSize);
    }

    void setSpeed (float speedIn) { speed = speedIn; }
    void setWarpEnabled (bool enabled) { warpEnabled = enabled; }

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<WarpingSamplerSound*> (s) != nullptr;
    }

    void startNote (int, float velocity, juce::SynthesiserSound* s, int) override
    {
        level = velocity;
        tailOff = 0.0f;

        if (dynamic_cast<WarpingSamplerSound*> (s) == nullptr)
            return;

        acquireSample();
        resetPlayback();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff <= 0.0f)
                tailOff = 1.0f;
        }
        else
        {
            clearCurrentNote();
            releaseSample();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (currentSampleRaw == nullptr)
            return;

        const auto& source = currentSampleRaw->getAudio();
        const int sourceNumSamples = source.getNumSamples();
        if (sourceNumSamples <= 1)
        {
            clearCurrentNote();
            releaseSample();
            return;
        }

        const float safeSpeed = juce::jlimit (0.25f, 4.0f, speed);
        if (warpEnabled)
            renderWarp (outputBuffer, startSample, numSamples, source, currentSampleRaw->getSourceSampleRate(), safeSpeed);
        else
            renderTimeStretch (outputBuffer, startSample, numSamples, source, currentSampleRaw->getSourceSampleRate(), safeSpeed);

        if (tailOff > 0.0f)
        {
            tailOff *= 0.995f;
            if (tailOff <= 0.005f)
            {
                clearCurrentNote();
                releaseSample();
                tailOff = 0.0f;
            }
        }
    }

private:
    void resetPlayback()
    {
        sourceReadPos = 0.0;
        olaBuffer.clear();
        olaReadIndex = 0;
        olaWriteIndex = 0;
        samplesUntilNextGrain = 0;
        inputGrainStart = 0.0;
        seeded = false;
    }

    void renderWarp (
        juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples,
        const juce::AudioBuffer<float>& source,
        double sourceSampleRate,
        float speedFactor)
    {
        const int outChannels = outputBuffer.getNumChannels();
        const int srcChannels = source.getNumChannels();
        const double baseInc = (sourceSampleRate / juce::jmax (1.0, hostSampleRate));
        const double inc = baseInc * (double) speedFactor;

        for (int i = 0; i < numSamples; ++i)
        {
            const int idx0 = (int) sourceReadPos;
            if (idx0 >= source.getNumSamples() - 1)
            {
                clearCurrentNote();
                releaseSample();
                return;
            }

            const float frac = (float) (sourceReadPos - (double) idx0);
            for (int ch = 0; ch < outChannels; ++ch)
            {
                const int srcCh = juce::jmin (ch, srcChannels - 1);
                const float* src = source.getReadPointer (srcCh);
                const float v0 = src[idx0];
                const float v1 = src[idx0 + 1];
                float v = v0 + frac * (v1 - v0);
                v *= level;
                if (tailOff > 0.0f)
                    v *= tailOff;
                outputBuffer.addSample (ch, startSample + i, v);
            }

            sourceReadPos += inc;
        }
    }

    void seedGrainsIfNeeded (const juce::AudioBuffer<float>& source, double sourceSampleRate, float speedFactor)
    {
        if (seeded)
            return;

        const int hop = defaultHopSize;
        scheduleGrain (source, sourceSampleRate, speedFactor);
        olaWriteIndex = (olaWriteIndex + hop) % olaBuffer.getNumSamples();
        scheduleGrain (source, sourceSampleRate, speedFactor);
        samplesUntilNextGrain = hop;
        seeded = true;
    }

    void scheduleGrain (const juce::AudioBuffer<float>& source, double sourceSampleRate, float speedFactor)
    {
        const int srcChannels = source.getNumChannels();
        const int grainSize = grainBuffer.getNumSamples();
        const int olaSize = olaBuffer.getNumSamples();
        const double baseInc = (sourceSampleRate / juce::jmax (1.0, hostSampleRate));

        const double maxSourceIndexNeeded = inputGrainStart + (double) (grainSize + 1) * baseInc;
        if (maxSourceIndexNeeded >= (double) (source.getNumSamples() - 1))
        {
            clearCurrentNote();
            releaseSample();
            return;
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = juce::jmin (ch, srcChannels - 1);
            const float* src = source.getReadPointer (srcCh);
            float* dst = grainBuffer.getWritePointer (ch);

            for (int i = 0; i < grainSize; ++i)
            {
                const double readPos = inputGrainStart + (double) i * baseInc;
                const int idx0 = (int) readPos;
                const float frac = (float) (readPos - (double) idx0);
                const float v0 = src[idx0];
                const float v1 = src[idx0 + 1];
                dst[i] = (v0 + frac * (v1 - v0)) * window[(size_t) i];
            }
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* grain = grainBuffer.getReadPointer (ch);
            float* ola = olaBuffer.getWritePointer (ch);
            int write = olaWriteIndex;
            for (int i = 0; i < grainSize; ++i)
            {
                ola[write] += grain[i];
                write = (write + 1) % olaSize;
            }
        }

        const int hop = defaultHopSize;
        inputGrainStart += (double) hop * (double) speedFactor * baseInc;
    }

    void renderTimeStretch (
        juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples,
        const juce::AudioBuffer<float>& source,
        double sourceSampleRate,
        float speedFactor)
    {
        seedGrainsIfNeeded (source, sourceSampleRate, speedFactor);
        if (currentSampleRaw == nullptr)
            return;

        const int outChannels = outputBuffer.getNumChannels();
        const int olaSize = olaBuffer.getNumSamples();
        const int hop = defaultHopSize;

        for (int i = 0; i < numSamples; ++i)
        {
            if (samplesUntilNextGrain <= 0)
            {
                scheduleGrain (source, sourceSampleRate, speedFactor);
                if (currentSampleRaw == nullptr)
                    return;

                olaWriteIndex = (olaWriteIndex + hop) % olaSize;
                samplesUntilNextGrain = hop;
            }

            for (int ch = 0; ch < outChannels; ++ch)
            {
                const int olaCh = juce::jmin (ch, 1);
                float* ola = olaBuffer.getWritePointer (olaCh);
                float v = ola[olaReadIndex];
                ola[olaReadIndex] = 0.0f;
                v *= level;
                if (tailOff > 0.0f)
                    v *= tailOff;
                outputBuffer.addSample (ch, startSample + i, v);
            }

            olaReadIndex = (olaReadIndex + 1) % olaSize;
            --samplesUntilNextGrain;
        }
    }

    static constexpr int defaultGrainSize = 2048;
    static constexpr int defaultHopSize = 512;

    std::atomic<SampleData*>* sampleSource = nullptr;
    SampleData* currentSampleRaw = nullptr;
    double hostSampleRate = 44100.0;
    float speed = 1.0f;
    bool warpEnabled = true;
    float level = 0.8f;
    float tailOff = 0.0f;

    double sourceReadPos = 0.0;

    juce::AudioBuffer<float> olaBuffer;
    juce::AudioBuffer<float> grainBuffer;
    std::vector<float> window;
    int olaReadIndex = 0;
    int olaWriteIndex = 0;
    int samplesUntilNextGrain = 0;
    double inputGrainStart = 0.0;
    bool seeded = false;

    void acquireSample()
    {
        releaseSample();
        if (sampleSource == nullptr)
            return;
        if (auto* s = sampleSource->load (std::memory_order_acquire))
        {
            s->incReferenceCount();
            currentSampleRaw = s;
        }
    }

    void releaseSample()
    {
        if (currentSampleRaw != nullptr)
        {
            currentSampleRaw->decReferenceCount();
            currentSampleRaw = nullptr;
        }
    }
};
