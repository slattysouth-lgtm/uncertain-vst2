/*
  ==============================================================================
   PluginEditor.cpp  —  Room Corrector (Uncertain)  v4
   Design : noir profond, touche de rouge, sobre.

   Disposition (signal flow de gauche a droite) :
   Row A (reparation) : CLEAN | REPAIR | TONE
   Row B (dynamique)  : COMP  | SIBILANCE | MIX
   Strip bas : OUTPUT + BYPASS
  ==============================================================================
*/
#include "PluginEditor.h"

namespace Col
{
    const juce::Colour bg      (0xff0A0A0D);
    const juce::Colour surface (0xff101013);
    const juce::Colour border  (0xff1C1C21);
    const juce::Colour body    (0xff131316);
    const juce::Colour text    (0xffECECEC);
    const juce::Colour sub     (0xff666670);
    const juce::Colour track   (0xff1E1E23);
    const juce::Colour red     (0xffC0203A);
    const juce::Colour redHi   (0xffE03050);
    const juce::Colour grpA    (0xff18181C); // fond Row A
    const juce::Colour grpB    (0xff141418); // fond Row B
}

//==============================================================================
CorrectorLookAndFeel::CorrectorLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,    Col::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId,            Col::text);
    setColour(juce::ToggleButton::textColourId,     Col::sub);
    setColour(juce::ToggleButton::tickColourId,     Col::redHi);
    setColour(juce::ToggleButton::tickDisabledColourId, Col::track);
}

void CorrectorLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                             int x,int y,int w,int h,
                                             float pos,
                                             float startAngle,float endAngle,
                                             juce::Slider&)
{
    const auto  b    = juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(7.0f);
    const float rad  = juce::jmin(b.getWidth(),b.getHeight())*0.5f;
    const auto  ctr  = b.getCentre();
    const float ang  = startAngle+pos*(endAngle-startAngle);
    const float lw   = rad*0.095f;
    const float arcR = rad-lw*0.5f;

    // Arc fond
    juce::Path back;
    back.addCentredArc(ctr.x,ctr.y,arcR,arcR,0.f,startAngle,endAngle,true);
    g.setColour(Col::track);
    g.strokePath(back,juce::PathStrokeType(lw,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));

    // Arc actif (rouge)
    juce::Path arc;
    arc.addCentredArc(ctr.x,ctr.y,arcR,arcR,0.f,startAngle,ang,true);
    g.setColour(Col::red);
    g.strokePath(arc,juce::PathStrokeType(lw,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));

    // Corps
    const auto kn=b.reduced(lw*2.3f);
    g.setColour(Col::body);
    g.fillEllipse(kn);
    g.setColour(Col::border);
    g.drawEllipse(kn,1.0f);

    // Index
    juce::Path ptr;
    const float pl=rad*0.40f;
    ptr.addRoundedRectangle(-lw*0.19f,-arcR+lw*1.4f,lw*0.38f,pl,lw*0.19f);
    ptr.applyTransform(juce::AffineTransform::rotation(ang).translated(ctr.x,ctr.y));
    g.setColour(juce::Colour(0xffE0E0E0));
    g.fillPath(ptr);

    // Point central rouge
    g.setColour(Col::redHi);
    g.fillEllipse(juce::Rectangle<float>(5.0f,5.0f).withCentre(ctr));
}

void CorrectorLookAndFeel::drawToggleButton (juce::Graphics& g,
                                             juce::ToggleButton& btn,
                                             bool, bool)
{
    const bool on=btn.getToggleState();
    const auto b=btn.getLocalBounds().toFloat();
    g.setColour(on?Col::red.withAlpha(0.18f):Col::surface);
    g.fillRoundedRectangle(b,4.0f);
    g.setColour(on?Col::red:Col::border);
    g.drawRoundedRectangle(b.reduced(0.5f),4.0f,1.0f);
    g.setColour(on?Col::redHi:Col::sub);
    g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText(btn.getButtonText(),b,juce::Justification::centred);
}

//==============================================================================
void RoomCorrectorAudioProcessorEditor::initKnob (juce::Slider& s, juce::Label& l,
                                                  const juce::String& name,
                                                  juce::Label* desc,
                                                  const juce::String& descTxt)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,17);
    s.setRotaryParameters(juce::MathConstants<float>::pi*1.20f,
                          juce::MathConstants<float>::pi*2.80f,true);
    addAndMakeVisible(s);

    l.setText(name,juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(12.0f,juce::Font::bold));
    l.setColour(juce::Label::textColourId,Col::text);
    addAndMakeVisible(l);

    if (desc)
    {
        desc->setText(descTxt,juce::dontSendNotification);
        desc->setJustificationType(juce::Justification::centredTop);
        desc->setFont(juce::Font(9.5f));
        desc->setColour(juce::Label::textColourId,Col::sub);
        addAndMakeVisible(*desc);
    }
}

//==============================================================================
RoomCorrectorAudioProcessorEditor::RoomCorrectorAudioProcessorEditor (RoomCorrectorAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);

    initKnob(cleanK,  cleanL,  "CLEAN",     &cleanD,  "Gate · Bruit · Reverb · Clics");
    initKnob(repairK, repairL, "REPAIR",    &repairD, "Proximity · Micro trop proche");
    initKnob(toneK,   toneL,   "TONE",      &toneD,   "Air adaptatif · Corps");
    initKnob(compK,   compL,   "COMP",      &compD,   "Compresseur · Limiteur");
    initKnob(sibK,    sibL,    "SIBILANCE", &sibD,    "De-esser adaptatif");
    initKnob(mixK,    mixL,    "MIX",       nullptr,  {});
    initKnob(outputK, outputL, "OUTPUT",    nullptr,  {});
    addAndMakeVisible(bypassBtn);

    using A=juce::AudioProcessorValueTreeState;
    cleanA  = std::make_unique<A::SliderAttachment>(proc.apvts,"clean",    cleanK);
    repairA = std::make_unique<A::SliderAttachment>(proc.apvts,"repair",   repairK);
    toneA   = std::make_unique<A::SliderAttachment>(proc.apvts,"tone",     toneK);
    compA   = std::make_unique<A::SliderAttachment>(proc.apvts,"comp",     compK);
    sibA    = std::make_unique<A::SliderAttachment>(proc.apvts,"sibilance",sibK);
    mixA    = std::make_unique<A::SliderAttachment>(proc.apvts,"mix",      mixK);
    outputA = std::make_unique<A::SliderAttachment>(proc.apvts,"output",   outputK);
    bypassA = std::make_unique<A::ButtonAttachment>(proc.apvts,"bypass",   bypassBtn);

    setSize(660,420);
}

RoomCorrectorAudioProcessorEditor::~RoomCorrectorAudioProcessorEditor()
{ setLookAndFeel(nullptr); }

//==============================================================================
void RoomCorrectorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(Col::bg);

    // Panneau principal
    g.setColour(Col::surface);
    g.fillRoundedRectangle(getLocalBounds().reduced(10).toFloat(),8.0f);

    // Separateur horizontal entre Row A et Row B
    const int sepY=74+185;
    g.setColour(Col::border);
    g.fillRect(juce::Rectangle<float>(22.0f,(float)sepY,(float)(getWidth()-44),1.0f));

    // Titre
    g.setFont(juce::Font(21.0f,juce::Font::bold));
    g.setColour(Col::text);
    g.drawText("ROOM CORRECTOR",28,18,280,26,juce::Justification::left);

    // Sous-titre
    g.setFont(juce::Font(10.0f));
    g.setColour(Col::sub);
    g.drawText("by Uncertain  ·  VST3 64-bit  ·  Voice",28,44,340,15,juce::Justification::left);

    // Ligne rouge sous le titre
    g.setColour(Col::red);
    g.fillRect(juce::Rectangle<float>(28.0f,62.0f,72.0f,1.5f));

    // Labels de groupe (A / B)
    g.setFont(juce::Font(8.5f));
    g.setColour(Col::sub.withAlpha(0.55f));
    g.drawText("RÉPARATION",28,68,120,14,juce::Justification::left);
    g.drawText("DYNAMIQUE", 28,(float)sepY+5,120,14,juce::Justification::left);

    // Chaine bas de fenetre
    g.setFont(juce::Font(8.5f));
    g.setColour(Col::sub.withAlpha(0.5f));
    g.drawText("GATE · DENOISE · DEREVERB · DE-CLICK · PROXIMITY · TONE[F0] · CORPS · AIR · COMP · DE-ESS",
               0,getHeight()-18,getWidth(),16,juce::Justification::centred);
}

void RoomCorrectorAudioProcessorEditor::resized()
{
    const int W=getWidth(), H=getHeight();
    const int topH=74;
    const int botH=26;
    const int midH=H-topH-botH;
    const int rowH=midH/2-4;

    const int kCols=3;
    const int kw=(W-44)/kCols;

    // ── Row A : CLEAN | REPAIR | TONE ─────────────────────────────────────
    {
        int cx=22;
        auto placeA=[&](juce::Slider& k, juce::Label& l, juce::Label& d)
        {
            l.setBounds(cx, topH, kw, 18);
            d.setBounds(cx, topH+18, kw, 14);
            k.setBounds(cx, topH+32, kw, rowH-32);
            cx+=kw;
        };
        placeA(cleanK,  cleanL,  cleanD);
        placeA(repairK, repairL, repairD);
        placeA(toneK,   toneL,   toneD);
    }

    // ── Row B : COMP | SIBILANCE | MIX ────────────────────────────────────
    {
        const int ry=topH+rowH+8;
        int cx=22;
        auto placeB=[&](juce::Slider& k, juce::Label& l, juce::Label* d)
        {
            l.setBounds(cx, ry, kw, 18);
            if (d) d->setBounds(cx, ry+18, kw, 14);
            k.setBounds(cx, ry+32, kw, rowH-32);
            cx+=kw;
        };
        placeB(compK,  compL, &compD);
        placeB(sibK,   sibL,  &sibD);
        placeB(mixK,   mixL,  nullptr);
    }

    // ── OUTPUT + BYPASS ───────────────────────────────────────────────────
    {
        const int bY=H-botH-32;
        outputL.setBounds(W-130, bY,     80, 16);
        outputK.setBounds(W-130, bY+16,  80, 38);
        bypassBtn.setBounds(W-46, 18, 34, 24);
    }
}
