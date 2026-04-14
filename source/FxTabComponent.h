#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters.h"

class FxTabComponent : public juce::Component
{
public:
    explicit FxTabComponent (juce::AudioProcessorValueTreeState& apvts)
        : state (apvts)
    {
        addAndMakeVisible (drive);
        addAndMakeVisible (tone);
        addAndMakeVisible (delayMix);
        addAndMakeVisible (delayFeedback);
        addAndMakeVisible (delayTime);
        addAndMakeVisible (delaySync);
        addAndMakeVisible (delayDivision);
        addAndMakeVisible (reverbMix);
        addAndMakeVisible (reverbRoom);
        addAndMakeVisible (reverbDamp);

        drive.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        drive.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        tone.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        tone.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        delayMix.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        delayMix.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        delayFeedback.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        delayFeedback.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        delayTime.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        delayTime.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        delaySync.setButtonText ("Sync");
        delayDivision.addItem ("1/4", 1);
        delayDivision.addItem ("1/8", 2);
        delayDivision.addItem ("1/16", 3);
        delayDivision.addItem ("1/32", 4);

        reverbMix.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        reverbMix.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        reverbRoom.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        reverbRoom.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        reverbDamp.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        reverbDamp.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);

        driveAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::drive, drive);
        toneAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::tone, tone);
        delayMixAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::delayMix, delayMix);
        delayFeedbackAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::delayFeedback, delayFeedback);
        delayTimeAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::delayTimeMs, delayTime);
        delaySyncAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::delaySync, delaySync);
        delayDivisionAttachment = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::delayDivision, delayDivision);
        reverbMixAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::reverbMix, reverbMix);
        reverbRoomAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::reverbRoomSize, reverbRoom);
        reverbDampAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::reverbDamping, reverbDamp);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto row1 = area.removeFromTop (120);
        drive.setBounds (row1.removeFromLeft (120));
        tone.setBounds (row1.removeFromLeft (120));
        delayMix.setBounds (row1.removeFromLeft (120));
        delayFeedback.setBounds (row1.removeFromLeft (120));
        delayTime.setBounds (row1.removeFromLeft (120));

        area.removeFromTop (10);
        auto row2 = area.removeFromTop (30);
        delaySync.setBounds (row2.removeFromLeft (80));
        delayDivision.setBounds (row2.removeFromLeft (120));

        area.removeFromTop (10);
        auto row3 = area.removeFromTop (120);
        reverbMix.setBounds (row3.removeFromLeft (120));
        reverbRoom.setBounds (row3.removeFromLeft (120));
        reverbDamp.setBounds (row3.removeFromLeft (120));
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::AudioProcessorValueTreeState& state;

    juce::Slider drive;
    juce::Slider tone;
    juce::Slider delayMix;
    juce::Slider delayFeedback;
    juce::Slider delayTime;
    juce::ToggleButton delaySync;
    juce::ComboBox delayDivision;
    juce::Slider reverbMix;
    juce::Slider reverbRoom;
    juce::Slider reverbDamp;

    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> toneAttachment;
    std::unique_ptr<SliderAttachment> delayMixAttachment;
    std::unique_ptr<SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<SliderAttachment> delayTimeAttachment;
    std::unique_ptr<ButtonAttachment> delaySyncAttachment;
    std::unique_ptr<ComboBoxAttachment> delayDivisionAttachment;
    std::unique_ptr<SliderAttachment> reverbMixAttachment;
    std::unique_ptr<SliderAttachment> reverbRoomAttachment;
    std::unique_ptr<SliderAttachment> reverbDampAttachment;
};

