#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class UncSpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    UncSpLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

class UncertainSpatialEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit UncertainSpatialEditor (UncertainSpatialProcessor&);
    ~UncertainSpatialEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { repaint(); }

    UncertainSpatialProcessor& proc;
    UncSpLookAndFeel lnf;

    juce::Slider image, dyn;
    juce::Label  imageLab, dynLab;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> imageAtt, dynAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainSpatialEditor)
};
