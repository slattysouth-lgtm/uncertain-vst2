/*
  ==============================================================================
   PluginEditor.h  —  Room Corrector (Uncertain)  v4
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CorrectorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CorrectorLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool, bool) override;
};

class RoomCorrectorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomCorrectorAudioProcessorEditor (RoomCorrectorAudioProcessor&);
    ~RoomCorrectorAudioProcessorEditor() override;
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    using Att = juce::AudioProcessorValueTreeState;

    RoomCorrectorAudioProcessor& proc;
    CorrectorLookAndFeel lnf;

    // 6 potards principaux + output
    juce::Slider   cleanK, repairK, toneK, compK, sibK, mixK, outputK;
    juce::Label    cleanL, repairL, toneL, compL, sibL, mixL, outputL;
    juce::Label    cleanD, repairD, toneD, compD, sibD; // descriptions courtes
    juce::ToggleButton bypassBtn { "BYPASS" };

    std::unique_ptr<Att::SliderAttachment> cleanA, repairA, toneA, compA, sibA, mixA, outputA;
    std::unique_ptr<Att::ButtonAttachment> bypassA;

    void initKnob (juce::Slider&, juce::Label&, const juce::String& name,
                   juce::Label* desc, const juce::String& descTxt);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomCorrectorAudioProcessorEditor)
};
