#include "PluginEditor.h"

namespace us
{
    const juce::Colour black { 0xff0a0a0b };
    const juce::Colour panel { 0xff141416 };
    const juce::Colour line  { 0xff2a2a2e };
    const juce::Colour bone  { 0xffe7e7e2 };
    const juce::Colour ash   { 0xff7d7d84 };
    const juce::Colour accent{ 0xffbfc6cc };
}

UncSpLookAndFeel::UncSpLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, us::bone);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, us::ash);
}

void UncSpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float cx = b.getCentreX(), cy = b.getCentreY(), ang = a0 + pos * (a1 - a0);

    juce::Path bg; bg.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a1, true);
    g.setColour (us::line); g.strokePath (bg, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path v; v.addCentredArc (cx, cy, rad, rad, 0.0f, a0, ang, true);
    g.setColour (us::accent); g.strokePath (v, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float hub = rad * 0.66f;
    g.setColour (us::panel); g.fillEllipse (cx - hub, cy - hub, hub * 2, hub * 2);
    g.setColour (us::line);  g.drawEllipse (cx - hub, cy - hub, hub * 2, hub * 2, 1.0f);
    juce::Path p; p.startNewSubPath (cx, cy);
    p.lineTo (cx + std::cos (ang - juce::MathConstants<float>::halfPi) * rad * 0.6f,
              cy + std::sin (ang - juce::MathConstants<float>::halfPi) * rad * 0.6f);
    g.setColour (us::accent); g.strokePath (p, juce::PathStrokeType (2.5f));
}

//==============================================================================
UncertainSpatialEditor::UncertainSpatialEditor (UncertainSpatialProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    setupKnob (image, imageLab, "IMAGE");
    setupKnob (dyn,   dynLab,   "DYNAMIQUE");

    imageAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "image", image);
    dynAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "dyn",   dyn);

    setSize (440, 380);
    startTimerHz (15);
}

UncertainSpatialEditor::~UncertainSpatialEditor() { stopTimer(); setLookAndFeel (nullptr); }

void UncertainSpatialEditor::paint (juce::Graphics& g)
{
    g.fillAll (us::black);
    g.setColour (us::bone); g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("UNCERTAIN", 0, 18, getWidth(), 26, juce::Justification::centred);
    g.setColour (us::ash); g.setFont (juce::Font (10.0f));
    g.drawText ("SPATIAL  -  STEREO & DYNAMIQUE PAR ZONES", 0, 44, getWidth(), 14, juce::Justification::centred);

    // vumetres par zone
    const char* names[4] = { "GRAVES", "BAS-MED", "MEDIUMS", "AIGUS" };
    const int n = 4, areaX = 24, areaY = 80, areaW = getWidth() - 48, barH = 120;
    const int bw = areaW / n;
    for (int b = 0; b < n; ++b)
    {
        const int x = areaX + b * bw + 8;
        const int w = bw - 16;
        g.setColour (us::panel);
        g.fillRect (x, areaY, w, barH);
        const float db = proc.getBandDb (b);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        const int hh = (int) (norm * barH);
        g.setColour (us::accent);
        g.fillRect (x, areaY + barH - hh, w, hh);
        g.setColour (us::line); g.drawRect (x, areaY, w, barH, 1);
        g.setColour (us::ash); g.setFont (juce::Font (9.0f, juce::Font::bold));
        g.drawText (names[b], x - 4, areaY + barH + 4, w + 8, 14, juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff3a3a40)); g.setFont (juce::Font (9.0f));
    g.drawText ("graves -> mono   |   aigus -> elargis   |   a placer avant le master",
                0, getHeight() - 22, getWidth(), 12, juce::Justification::centred);
}

void UncertainSpatialEditor::resized()
{
    const int ky = 236, kw = 150, kh = 120;
    image.setBounds (getWidth()/2 - kw - 10, ky, kw, kh);
    dyn.setBounds   (getWidth()/2 + 10,      ky, kw, kh);
    imageLab.setBounds (getWidth()/2 - kw - 10, ky - 16, kw, 14);
    dynLab.setBounds   (getWidth()/2 + 10,      ky - 16, kw, 14);
}
