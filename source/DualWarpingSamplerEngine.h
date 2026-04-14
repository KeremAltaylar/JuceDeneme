#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "SampleData.h"

class DualWarpingSamplerEngine
{
public:
    void prepare (double sampleRateIn, int samplesPerBlock, int numOutputChannels)
    {
        hostSampleRate = sampleRateIn;
        numChannels = juce::jlimit (1, 2, numOutputChannels);

        for (auto& v : voices)
            v.prepare (hostSampleRate, samplesPerBlock, numChannels);
    }

    void setParameters (float speedIn, bool warpEnabledIn)
    {
        speed = speedIn;
        warpEnabled = warpEnabledIn;
        for (auto& v : voices)
        {
            v.setSpeed (speed);
            v.setWarpEnabled (warpEnabled);
        }
    }

    void setSampleSource (int slotIndex, std::atomic<SampleData*>* source)
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return;
        sampleSources[(size_t) slotIndex] = source;
    }

    void render (juce::AudioBuffer<float>& output, int startSample, int numSamples)
    {
        for (auto& v : voices)
            v.render (output, startSample, numSamples);
    }

    void trigger (int slotIndex, int onsetSampleInSource, int durationSamplesOut, float velocity)
    {
        if (slotIndex < 0 || slotIndex >= 2)
            return;
        if (durationSamplesOut <= 0)
            return;

        auto* src = sampleSources[(size_t) slotIndex];
        if (src == nullptr)
            return;

        auto* sampleRaw = src->load (std::memory_order_acquire);
        if (sampleRaw == nullptr)
            return;

        WarpingVoice* voice = nullptr;
        for (auto& v : voices)
        {
            if (! v.isActive())
            {
                voice = &v;
                break;
            }
        }

        if (voice == nullptr)
            voice = &voices[0];

        voice->start (sampleRaw, onsetSampleInSource, durationSamplesOut, velocity);
    }

private:
    class WarpingVoice
    {
    public:
        void prepare (double hostRate, int, int numOutputChannels)
        {
            hostSampleRate = hostRate;
            numChannels = juce::jlimit (1, 2, numOutputChannels);

            const int grainSize = defaultGrainSize;
            const int olaSize = grainSize * 4;

            olaBuffer.setSize (numChannels, olaSize, false, false, true);
            grainBuffer.setSize (numChannels, grainSize, false, false, true);
            window.resize ((size_t) grainSize);
            for (int i = 0; i < grainSize; ++i)
            {
                const float phase = (float) i / (float) (grainSize - 1);
                window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
            }
        }

        void setSpeed (float s) { speed = s; }
        void setWarpEnabled (bool w) { warpEnabled = w; }

        bool isActive() const { return active; }

        void start (SampleData* sampleIn, int onsetInSource, int durationOutSamples, float velocity)
        {
            stop();

            if (sampleIn == nullptr)
                return;

            const auto& src = sampleIn->getAudio();
            if (src.getNumSamples() <= 1)
                return;

            onsetInSource = juce::jlimit (0, src.getNumSamples() - 2, onsetInSource);
            samplesRemaining = durationOutSamples;
            level = velocity;
            sampleIn->incReferenceCount();
            currentSampleRaw = sampleIn;
            sourceSampleRate = sampleIn->getSourceSampleRate();

            resetPlayback (onsetInSource);
            active = true;
        }

        void stop()
        {
            active = false;
            samplesRemaining = 0;
            if (currentSampleRaw != nullptr)
            {
                currentSampleRaw->decReferenceCount();
                currentSampleRaw = nullptr;
            }
        }

        void render (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
        {
            if (! active || currentSampleRaw == nullptr)
                return;

            const auto& source = currentSampleRaw->getAudio();
            if (source.getNumSamples() <= 1)
            {
                stop();
                return;
            }

            int toDo = juce::jmin (numSamples, samplesRemaining);
            if (toDo <= 0)
            {
                stop();
                return;
            }

            const float safeSpeed = juce::jlimit (0.25f, 4.0f, speed);
            if (warpEnabled)
                renderWarp (outputBuffer, startSample, toDo, source, sourceSampleRate, safeSpeed);
            else
                renderTimeStretch (outputBuffer, startSample, toDo, source, sourceSampleRate, safeSpeed);

            samplesRemaining -= toDo;
            if (samplesRemaining <= 0)
                stop();
        }

    private:
        void resetPlayback (int onsetInSource)
        {
            sourceReadPos = (double) onsetInSource;
            olaBuffer.clear();
            olaReadIndex = 0;
            olaWriteIndex = 0;
            samplesUntilNextGrain = 0;
            inputGrainStart = (double) onsetInSource;
            seeded = false;
        }

        void renderWarp (
            juce::AudioBuffer<float>& outputBuffer,
            int startSample,
            int numSamples,
            const juce::AudioBuffer<float>& source,
            double sourceRate,
            float speedFactor)
        {
            const int outChannels = outputBuffer.getNumChannels();
            const int srcChannels = source.getNumChannels();
            const double baseInc = (sourceRate / juce::jmax (1.0, hostSampleRate));
            const double inc = baseInc * (double) speedFactor;

            for (int i = 0; i < numSamples; ++i)
            {
                const int idx0 = (int) sourceReadPos;
                if (idx0 >= source.getNumSamples() - 1)
                {
                    stop();
                    return;
                }

                const float frac = (float) (sourceReadPos - (double) idx0);
                for (int ch = 0; ch < outChannels; ++ch)
                {
                    const int srcCh = juce::jmin (ch, srcChannels - 1);
                    const float* src = source.getReadPointer (srcCh);
                    const float v0 = src[idx0];
                    const float v1 = src[idx0 + 1];
                    const float v = (v0 + frac * (v1 - v0)) * level;
                    outputBuffer.addSample (ch, startSample + i, v);
                }

                sourceReadPos += inc;
            }
        }

        void seedGrainsIfNeeded (const juce::AudioBuffer<float>& source, double sourceRate, float speedFactor)
        {
            if (seeded)
                return;

            const int hop = defaultHopSize;
            scheduleGrain (source, sourceRate, speedFactor);
            olaWriteIndex = (olaWriteIndex + hop) % olaBuffer.getNumSamples();
            scheduleGrain (source, sourceRate, speedFactor);
            samplesUntilNextGrain = hop;
            seeded = true;
        }

        void scheduleGrain (const juce::AudioBuffer<float>& source, double sourceRate, float speedFactor)
        {
            const int srcChannels = source.getNumChannels();
            const int grainSize = grainBuffer.getNumSamples();
            const int olaSize = olaBuffer.getNumSamples();
            const double baseInc = (sourceRate / juce::jmax (1.0, hostSampleRate));

            const double maxSourceIndexNeeded = inputGrainStart + (double) (grainSize + 1) * baseInc;
            if (maxSourceIndexNeeded >= (double) (source.getNumSamples() - 1))
            {
                stop();
                return;
            }

            for (int ch = 0; ch < numChannels; ++ch)
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

            for (int ch = 0; ch < numChannels; ++ch)
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
            double sourceRate,
            float speedFactor)
        {
            seedGrainsIfNeeded (source, sourceRate, speedFactor);
            if (! active)
                return;

            const int outChannels = outputBuffer.getNumChannels();
            const int olaSize = olaBuffer.getNumSamples();
            const int hop = defaultHopSize;

            for (int i = 0; i < numSamples; ++i)
            {
                if (samplesUntilNextGrain <= 0)
                {
                    scheduleGrain (source, sourceRate, speedFactor);
                    if (! active)
                        return;
                    olaWriteIndex = (olaWriteIndex + hop) % olaSize;
                    samplesUntilNextGrain = hop;
                }

                for (int ch = 0; ch < outChannels; ++ch)
                {
                    const int olaCh = juce::jmin (ch, numChannels - 1);
                    float* ola = olaBuffer.getWritePointer (olaCh);
                    float v = ola[olaReadIndex];
                    ola[olaReadIndex] = 0.0f;
                    outputBuffer.addSample (ch, startSample + i, v * level);
                }

                olaReadIndex = (olaReadIndex + 1) % olaSize;
                --samplesUntilNextGrain;
            }
        }

        static constexpr int defaultGrainSize = 2048;
        static constexpr int defaultHopSize = 512;

        bool active = false;
        int samplesRemaining = 0;

        SampleData* currentSampleRaw = nullptr;
        double sourceSampleRate = 44100.0;
        double hostSampleRate = 44100.0;
        int numChannels = 2;

        float speed = 1.0f;
        bool warpEnabled = true;
        float level = 1.0f;

        double sourceReadPos = 0.0;

        juce::AudioBuffer<float> olaBuffer;
        juce::AudioBuffer<float> grainBuffer;
        std::vector<float> window;
        int olaReadIndex = 0;
        int olaWriteIndex = 0;
        int samplesUntilNextGrain = 0;
        double inputGrainStart = 0.0;
        bool seeded = false;
    };

    double hostSampleRate = 44100.0;
    int numChannels = 2;
    float speed = 1.0f;
    bool warpEnabled = true;

    std::array<std::atomic<SampleData*>*, 2> sampleSources { nullptr, nullptr };
    std::array<WarpingVoice, 8> voices;
};

