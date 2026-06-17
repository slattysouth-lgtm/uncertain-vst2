#include "PluginEditor.h"

namespace unc
{
    const juce::Colour black { 0xff0a0a0b };
    const juce::Colour panel { 0xff141416 };
    const juce::Colour line  { 0xff2a2a2e };
    const juce::Colour bone  { 0xffe7e7e2 };
    const juce::Colour ash   { 0xff7d7d84 };
    const juce::Colour accent{ 0xffbfc6cc }; // cold steel / "uncertain"
}

//==============================================================================
UncLookAndFeel::UncLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,      unc::bone);
    setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void UncLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (14.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path bg;
    bg.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (unc::line);
    g.strokePath (bg, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (unc::accent);
    g.strokePath (val, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float hub = radius * 0.70f;
    g.setColour (unc::panel);
    g.fillEllipse (cx - hub, cy - hub, hub * 2.0f, hub * 2.0f);
    g.setColour (unc::line);
    g.drawEllipse (cx - hub, cy - hub, hub * 2.0f, hub * 2.0f, 1.0f);

    juce::Path ptr;
    const float pl = radius * 0.66f;
    ptr.startNewSubPath (cx, cy);
    ptr.lineTo (cx + std::cos (angle - juce::MathConstants<float>::halfPi) * pl,
                cy + std::sin (angle - juce::MathConstants<float>::halfPi) * pl);
    g.setColour (unc::accent);
    g.strokePath (ptr, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
UncertainPolishEditor::UncertainPolishEditor (UncertainPolishProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    polish.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    polish.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
    polish.setColour (juce::Slider::textBoxTextColourId, unc::bone);
    addAndMakeVisible (polish);

    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "polish", polish);

    setSize (360, 380);
}

UncertainPolishEditor::~UncertainPolishEditor()
{
    setLookAndFeel (nullptr);
}

void UncertainPolishEditor::paint (juce::Graphics& g)
{
    g.fillAll (unc::black);

    g.setColour (unc::bone);
    g.setFont (juce::Font (24.0f, juce::Font::bold));
    g.drawText ("UNCERTAIN", 0, 22, getWidth(), 28, juce::Justification::centred);

    g.setColour (unc::ash);
    g.setFont (juce::Font (10.0f));
    g.drawText ("POLISH  -  PRE-MASTER", 0, 50, getWidth(), 16, juce::Justification::centred);

    g.setColour (unc::ash);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("CORRECTION  +  GLUE", 0, getHeight() - 64, getWidth(), 16, juce::Justification::centred);

    g.setColour (juce::Colour (0xff3a3a40));
    g.setFont (juce::Font (9.0f));
    g.drawText ("a placer AVANT le master  -  1 = leger   100 = maximal",
                0, getHeight() - 26, getWidth(), 14, juce::Justification::centred);
}

void UncertainPolishEditor::resized()
{
    polish.setBounds (getWidth() / 2 - 110, 78, 220, 220);
}
