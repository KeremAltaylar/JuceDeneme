#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class StepSequencer
{
public:
    struct Event
    {
        int sampleOffset = 0;
        bool noteOn = false;
    };

    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        reset();
    }

    void reset()
    {
        lastStepIndex = -1;
        lastStepWasActive = false;
    }

    int createEventsForBlock (
        std::array<Event, 64>& events,
        juce::AudioPlayHead* playHead,
        int numSamples,
        int sequenceLength,
        const std::array<bool, 16>& steps)
    {
        int eventCount = 0;

        if (playHead == nullptr || sampleRate <= 0.0)
            return ensureNoteOff (events, eventCount);

#if JUCE_MAJOR_VERSION >= 8
        const auto position = playHead->getPosition();
        if (! position.hasValue() || ! position->getIsPlaying())
            return ensureNoteOff (events, eventCount);

        const auto bpmOpt = position->getBpm();
        const auto ppqOpt = position->getPpqPosition();
        if (! bpmOpt.hasValue() || ! ppqOpt.hasValue() || *bpmOpt <= 0.0)
            return ensureNoteOff (events, eventCount);

        const double bpm = *bpmOpt;
        const double ppqStart = *ppqOpt;
#else
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (! playHead->getCurrentPosition (pos))
            return ensureNoteOff (events, eventCount);

        if (! pos.isPlaying || pos.bpm <= 0.0)
            return ensureNoteOff (events, eventCount);

        const double bpm = pos.bpm;
        const double ppqStart = pos.ppqPosition;
#endif

        const double ppqPerSample = bpm / (60.0 * sampleRate);
        const double stepPpq = 0.25;
        const int length = juce::jlimit (1, 16, sequenceLength);

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
            lastStepWasActive = steps[(size_t) lastStepIndex];
            if (lastStepWasActive)
                events[(size_t) eventCount++] = Event { 0, true };
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
                if (lastStepWasActive)
                    events[(size_t) eventCount++] = Event { sampleOffset, false };

                lastStepIndex = newStep;
                lastStepWasActive = steps[(size_t) lastStepIndex];

                if (lastStepWasActive)
                    events[(size_t) eventCount++] = Event { sampleOffset, true };
            }
        }

        return eventCount;
    }

private:
    int ensureNoteOff (std::array<Event, 64>& events, int eventCount)
    {
        if (lastStepWasActive)
        {
            events[(size_t) eventCount++] = Event { 0, false };
            lastStepWasActive = false;
        }
        lastStepIndex = -1;
        return eventCount;
    }

    double sampleRate = 44100.0;
    int lastStepIndex = -1;
    bool lastStepWasActive = false;
};
