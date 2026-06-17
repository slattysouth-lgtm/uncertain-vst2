#include "PluginEditor.h"

namespace bm
{
    const juce::Colour kBlack  { 0xff070707 };
    const juce::Colour kPanel  { 0xff111110 };
    const juce::Colour kLine    { 0xff26211f };
    const juce::Colour kBone    { 0xffe9e4dc };
    const juce::Colour kAsh      { 0xff8a847d };
    const juce::Colour kBlood    { 0xffff2b1f };
    const juce::Colour kGold      { 0xffb8862f };
}

//==============================================================================
BmLookAndFeel::BmLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,      bm::kBone);
    setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,              bm::kAsh);
    setColour (juce::TextButton::buttonColourId,       bm::kPanel);
    setColour (juce::TextButton::textColourOffId,      bm::kAsh);
    setColour (juce::TextButton::textColourOnId,       bm::kBone);
}

void BmLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path bg;  bg.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (bm::kLine);
    g.strokePath (bg, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val; val.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (bm::kBlood);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float hub = radius * 0.62f;
    g.setColour (bm::kPanel); g.fillEllipse (cx - hub, cy - hub, hub * 2.0f, hub * 2.0f);
    g.setColour (bm::kLine);  g.drawEllipse (cx - hub, cy - hub, hub * 2.0f, hub * 2.0f, 1.0f);

    juce::Path ptr; const float pl = radius * 0.55f;
    ptr.startNewSubPath (cx, cy);
    ptr.lineTo (cx + std::cos (angle - juce::MathConstants<float>::halfPi) * pl,
                cy + std::sin (angle - juce::MathConstants<float>::halfPi) * pl);
    g.setColour (bm::kBlood);
    g.strokePath (ptr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
BloodmoneyMasterEditor::BloodmoneyMasterEditor (BloodmoneyMasterProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    const char* ids[kNumKnobs]   = { "sub", "presence", "air", "punch", "clip", "power", "ceiling" };
    const char* names[kNumKnobs] = { "SUB / 808", "CLARTE", "AIR", "PUNCH", "CLIP", "PUISSANCE", "PLAFOND" };
    for (int i = 0; i < kNumKnobs; ++i)
        makeKnob (knobs[i], ids[i], names[i]);

    auto styleBtn = [this] (juce::TextButton& b) { b.setColour (juce::TextButton::buttonColourId, bm::kPanel); addAndMakeVisible (b); };
    styleBtn (pClean); styleBtn (pStream); styleBtn (pLoud);
    pClean .onClick = [this] { applyPreset (1.5f, 1.5f, 2.0f, 25.0f,  5.0f,  3.0f, -1.0f); };
    pStream.onClick = [this] { applyPreset (2.5f, 2.5f, 3.0f, 50.0f, 10.0f,  7.0f, -1.0f); };
    pLoud  .onClick = [this] { applyPreset (3.5f, 3.0f, 3.5f, 75.0f, 15.0f, 11.0f, -0.5f); };

    lufsLabel.setJustificationType (juce::Justification::centred);
    lufsLabel.setColour (juce::Label::textColourId, bm::kGold);
    lufsLabel.setFont (juce::Font (15.0f, juce::Font::bold));
    addAndMakeVisible (lufsLabel);

    setSize (600, 430);
    startTimerHz (12);
}

BloodmoneyMasterEditor::~BloodmoneyMasterEditor() { stopTimer(); setLookAndFeel (nullptr); }

void BloodmoneyMasterEditor::timerCallback()
{
    const float l = proc.getLufs();
    lufsLabel.setText (l <= -69.0f ? juce::String ("-- LUFS")
                                   : juce::String (l, 1) + " LUFS  (court terme)",
                       juce::dontSendNotification);
    lufsLabel.setColour (juce::Label::textColourId,
                         (l > -11.5f && l < -9.0f) ? bm::kBlood : bm::kGold); // cible -10/-11
}

void BloodmoneyMasterEditor::makeKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void BloodmoneyMasterEditor::applyPreset (float sub, float pres, float air, float punch,
                                          float clip, float pow, float ceil)
{
    auto setP = [this] (const char* id, float v)
    {
        if (auto* prm = proc.apvts.getParameter (id))
            prm->setValueNotifyingHost (proc.apvts.getParameterRange (id).convertTo0to1 (v));
    };
    setP ("sub", sub); setP ("presence", pres); setP ("air", air); setP ("punch", punch);
    setP ("clip", clip); setP ("power", pow); setP ("ceiling", ceil);
}

//==============================================================================
void BloodmoneyMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (bm::kBlack);
    g.setColour (bm::kBone); g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("BLOOD", 18, 14, 110, 32, juce::Justification::left);
    g.setColour (bm::kBlood); g.drawText ("MONEY", 108, 14, 130, 32, juce::Justification::left);
    g.setColour (bm::kAsh); g.setFont (juce::Font (10.0f));
    g.drawText ("MASTER ENGINE", 230, 22, 160, 18, juce::Justification::left);

    g.setColour (bm::kLine); g.fillRect (18, 90, getWidth() - 36, 1);

    g.setColour (juce::Colour (0xff3a322e)); g.setFont (juce::Font (9.0f));
    g.drawText ("CLIPPER 8x SURECHANTILLONNE  -  PUISSANT ET CLAIR  -  vise -10/-11 LUFS",
                18, getHeight() - 20, getWidth() - 36, 14, juce::Justification::left);
}

void BloodmoneyMasterEditor::resized()
{
    const int by = 56, bh = 24, bw = 110, gap = 8;
    pClean .setBounds (getWidth() - (bw * 3 + gap * 2) - 18, by, bw, bh);
    pStream.setBounds (getWidth() - (bw * 2 + gap)      - 18, by, bw, bh);
    pLoud  .setBounds (getWidth() -  bw                 - 18, by, bw, bh);

    const int top = 104, kw = 138, khRow = 134;
    auto place = [&] (int idx, int col, int row)
    {
        const int x = 18 + col * kw, y = top + row * khRow;
        knobs[idx].label.setBounds (x, y, kw, 16);
        knobs[idx].slider.setBounds (x, y + 16, kw, khRow - 28);
    };
    place (0, 0, 0); place (1, 1, 0); place (2, 2, 0); place (3, 3, 0);
    place (4, 0, 1); place (5, 1, 1); place (6, 2, 1);

    lufsLabel.setBounds (18 + 3 * kw, top + khRow + 30, kw, 40);
}
