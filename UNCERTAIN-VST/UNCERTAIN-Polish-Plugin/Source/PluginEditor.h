#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class UncLookAndFeel : public juce::LookAndFeel_V4
{
public:
    UncLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
class UncertainPolishEditor : public juce::AudioProcessorEditor
{
public:
    explicit UncertainPolishEditor (UncertainPolishProcessor&);
    ~UncertainPolishEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UncertainPolishProcessor& proc;
    UncLookAndFeel lnf;

    juce::Slider polish;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainPolishEditor)
};
