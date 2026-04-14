#pragma once

#include <juce_dsp/juce_dsp.h>

class FxChain
{
public:
    void prepare (double sampleRateIn, int maxBlockSize, int numChannelsIn)
    {
        sampleRate = sampleRateIn;
        numChannels = juce::jlimit (1, 2, numChannelsIn);
        reverbMaxBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maxBlockSize;
        spec.numChannels = (juce::uint32) numChannels;

        for (auto& f : toneFilters)
        {
            f.reset();
            f.prepare (spec);
            f.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }

        const int maxDelaySamples = (int) std::ceil (sampleRate * maxDelaySeconds);
        for (auto& d : delayLines)
        {
            d.reset();
            d.setMaximumDelayInSamples (maxDelaySamples);
            juce::dsp::ProcessSpec delaySpec;
            delaySpec.sampleRate = sampleRate;
            delaySpec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);
            delaySpec.numChannels = 1;
            d.prepare (delaySpec);
        }

        reverb.reset();
        reverb.prepare (spec);
    }

    void setParameters (
        float driveDbIn,
        float toneHzIn,
        float delayMixIn,
        float delayFeedbackIn,
        float delayTimeMsIn,
        bool delaySyncIn,
        int delayDivisionIndexIn,
        float reverbMixIn,
        float reverbRoomSizeIn,
        float reverbDampingIn,
        double bpmIn)
    {
        driveDb = driveDbIn;
        toneHz = toneHzIn;
        delayMix = delayMixIn;
        delayFeedback = delayFeedbackIn;
        delayTimeMs = delayTimeMsIn;
        delaySync = delaySyncIn;
        delayDivisionIndex = delayDivisionIndexIn;
        reverbMix = reverbMixIn;
        reverbRoomSize = reverbRoomSizeIn;
        reverbDamping = reverbDampingIn;
        bpm = bpmIn;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int ns = buffer.getNumSamples();
        const int nc = buffer.getNumChannels();

        const float driveGain = juce::Decibels::decibelsToGain (driveDb);
        const float cutoff = juce::jlimit (20.0f, 20000.0f, toneHz);
        for (int ch = 0; ch < juce::jmin (2, nc); ++ch)
            toneFilters[(size_t) ch].setCutoffFrequency (cutoff);

        for (int ch = 0; ch < nc; ++ch)
        {
            float* x = buffer.getWritePointer (ch);
            auto& filter = toneFilters[(size_t) juce::jmin (ch, 1)];
            for (int i = 0; i < ns; ++i)
            {
                float v = x[i] * driveGain;
                v = std::tanh (v);
                v = filter.processSample (0, v);
                x[i] = v;
            }
        }

        const float mix = juce::jlimit (0.0f, 1.0f, delayMix);
        const float fb = juce::jlimit (0.0f, 0.95f, delayFeedback);
        const int delaySamples = juce::jlimit (1, (int) std::ceil (sampleRate * maxDelaySeconds), computeDelaySamples());

        for (auto& d : delayLines)
            d.setDelay ((float) delaySamples);

        for (int ch = 0; ch < nc; ++ch)
        {
            float* x = buffer.getWritePointer (ch);
            auto& dl = delayLines[(size_t) juce::jmin (ch, 1)];
            for (int i = 0; i < ns; ++i)
            {
                const float dry = x[i];
                const float wet = dl.popSample (0);
                dl.pushSample (0, dry + wet * fb);
                x[i] = dry * (1.0f - mix) + wet * mix;
            }
        }

        if (reverbMix > 0.0001f)
        {
            juce::dsp::Reverb::Parameters p;
            p.roomSize = juce::jlimit (0.0f, 1.0f, reverbRoomSize);
            p.damping = juce::jlimit (0.0f, 1.0f, reverbDamping);
            p.wetLevel = juce::jlimit (0.0f, 1.0f, reverbMix);
            p.dryLevel = 1.0f - p.wetLevel;
            p.width = 1.0f;
            p.freezeMode = 0.0f;
            reverb.setParameters (p);

            juce::dsp::AudioBlock<float> block (buffer);
            int offset = 0;
            const int maxChunk = (int) juce::jmax ((juce::uint32) 1, reverbMaxBlockSize);
            while (offset < ns)
            {
                const int chunk = juce::jmin (maxChunk, ns - offset);
                auto subBlock = block.getSubBlock ((size_t) offset, (size_t) chunk);
                juce::dsp::ProcessContextReplacing<float> ctx (subBlock);
                reverb.process (ctx);
                offset += chunk;
            }
        }
    }

private:
    int computeDelaySamples() const
    {
        if (! delaySync || bpm <= 0.0)
            return (int) std::round ((double) delayTimeMs * 0.001 * sampleRate);

        const double quarterSeconds = 60.0 / bpm;
        double factor = 0.25;
        switch (delayDivisionIndex)
        {
            case 0: factor = 1.0; break;
            case 1: factor = 0.5; break;
            case 2: factor = 0.25; break;
            case 3: factor = 0.125; break;
            default: break;
        }

        return (int) std::round (quarterSeconds * factor * sampleRate);
    }

    static constexpr double maxDelaySeconds = 2.0;

    double sampleRate = 44100.0;
    int numChannels = 2;
    juce::uint32 reverbMaxBlockSize = 0;

    float driveDb = 0.0f;
    float toneHz = 4000.0f;

    float delayMix = 0.0f;
    float delayFeedback = 0.0f;
    float delayTimeMs = 250.0f;
    bool delaySync = true;
    int delayDivisionIndex = 2;
    double bpm = 120.0;

    float reverbMix = 0.0f;
    float reverbRoomSize = 0.5f;
    float reverbDamping = 0.5f;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> toneFilters;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLines;
    juce::dsp::Reverb reverb;
};
