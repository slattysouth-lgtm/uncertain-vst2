#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class BmLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BmLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
class BloodmoneyMasterEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit BloodmoneyMasterEditor (BloodmoneyMasterProcessor&);
    ~BloodmoneyMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void makeKnob (Knob&, const juce::String& paramID, const juce::String& name);
    void applyPreset (float sub, float pres, float air, float punch,
                      float clip, float pow, float ceil);

    BloodmoneyMasterProcessor& proc;
    BmLookAndFeel lnf;

    static constexpr int kNumKnobs = 7;
    Knob knobs[kNumKnobs];
    juce::Label lufsLabel;

    juce::TextButton pClean { "PROPRE" }, pStream { "STREAM" }, pLoud { "PUISSANCE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BloodmoneyMasterEditor)
};
