#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleData.h"
#include "WarpingSampler.h"

class WarpingSamplerEngine
{
public:
    WarpingSamplerEngine()
    {
        for (int i = 0; i < 8; ++i)
            synth.addVoice (new WarpingSamplerVoice());

        synth.addSound (new WarpingSamplerSound());
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<WarpingSamplerVoice*> (synth.getVoice (i)))
                v->prepare (sampleRate, samplesPerBlock);
        }
    }

    void setSampleSource (std::atomic<SampleData*>* source)
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<WarpingSamplerVoice*> (synth.getVoice (i)))
                v->setSampleSource (source);
        }
    }

    void setParameters (float speed, bool warpEnabled)
    {
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* v = dynamic_cast<WarpingSamplerVoice*> (synth.getVoice (i)))
            {
                v->setSpeed (speed);
                v->setWarpEnabled (warpEnabled);
            }
        }
    }

    void render (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi, int startSample, int numSamples)
    {
        synth.renderNextBlock (output, midi, startSample, numSamples);
    }

    void render (juce::AudioBuffer<float>& output, int startSample, int numSamples)
    {
        emptyMidi.clear();
        synth.renderNextBlock (output, emptyMidi, startSample, numSamples);
    }

    void noteOn (int midiNoteNumber, float velocity)
    {
        synth.noteOn (1, midiNoteNumber, velocity);
    }

    void noteOff (int midiNoteNumber, float velocity, bool allowTailOff)
    {
        synth.noteOff (1, midiNoteNumber, velocity, allowTailOff);
    }

private:
    juce::Synthesiser synth;
    juce::MidiBuffer emptyMidi;
};
