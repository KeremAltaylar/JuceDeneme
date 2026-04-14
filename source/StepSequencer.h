#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class StepSequencer
{
public:
    struct Event
    {
        int sampleOffset = 0;
        int stepIndex = 0;
    };

    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        reset();
    }

    void reset()
    {
        lastStepIndex = -1;
    }

    int createStepStartEvents (
        std::array<Event, 64>& events,
        int numSamples,
        int sequenceLength,
        bool isPlaying,
        double bpm,
        double ppqStart)
    {
        int eventCount = 0;

        if (! isPlaying || sampleRate <= 0.0 || bpm <= 0.0)
        {
            reset();
            return 0;
        }

        const double ppqPerSample = bpm / (60.0 * sampleRate);
        const int length = juce::jlimit (1, 16, sequenceLength);
        const double stepPpq = 4.0 / (double) length;

        auto stepIndexAtPpq = [length, stepPpq] (double ppq)
        {
            const auto stepCount = static_cast<int> (std::floor (ppq / stepPpq));
            int idx = stepCount % length;
            if (idx < 0)
                idx += length;
            return idx;
        };

        const int initialStep = stepIndexAtPpq (ppqStart);
        if (lastStepIndex == -1)
        {
            lastStepIndex = initialStep;
            events[(size_t) eventCount++] = Event { 0, lastStepIndex };
        }

        double ppqNow = ppqStart;
        int sampleOffset = 0;

        while (sampleOffset < numSamples)
        {
            const double currentStepIndexF = std::floor (ppqNow / stepPpq);
            const double nextBoundaryPpq = (currentStepIndexF + 1.0) * stepPpq;
            const double ppqUntilBoundary = nextBoundaryPpq - ppqNow;
            const int samplesUntilBoundary = ppqPerSample > 0.0
                ? juce::jlimit (0, numSamples - sampleOffset, (int) std::ceil (ppqUntilBoundary / ppqPerSample))
                : numSamples - sampleOffset;

            if (samplesUntilBoundary <= 0)
                break;

            sampleOffset += samplesUntilBoundary;
            ppqNow += ppqPerSample * samplesUntilBoundary;

            if (sampleOffset >= numSamples)
                break;

            const int newStep = stepIndexAtPpq (ppqNow);
            if (newStep != lastStepIndex)
            {
                lastStepIndex = newStep;
                events[(size_t) eventCount++] = Event { sampleOffset, lastStepIndex };
            }
        }

        return eventCount;
    }

private:
    double sampleRate = 44100.0;
    int lastStepIndex = -1;
};
