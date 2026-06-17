#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BloodmoneyMasterProcessor::BloodmoneyMasterProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BloodmoneyMasterProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<P>(juce::ParameterID{ "sub",      1 }, "Sub / 808", R(-2.0f,  6.0f, 0.1f),  2.5f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "presence", 1 }, "Clarte",    R(-2.0f,  6.0f, 0.1f),  2.5f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "air",      1 }, "Air",       R( 0.0f,  6.0f, 0.1f),  3.0f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "punch",    1 }, "Punch",     R( 0.0f,100.0f, 1.0f), 50.0f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "clip",     1 }, "Clip",      R( 0.0f,100.0f, 1.0f), 10.0f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "power",    1 }, "Puissance", R( 0.0f, 18.0f, 0.1f),  7.0f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "ceiling",  1 }, "Plafond",   R(-2.0f, -0.1f, 0.1f), -1.0f));

    return { p.begin(), p.end() };
}

//==============================================================================
bool BloodmoneyMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void BloodmoneyMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    numCh     = juce::jmax (1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    for (auto* d : { &hpf, &subShelf, &lowMid, &presence, &airShelf })
        d->prepare (spec);
    comp.prepare (spec);
    powerGain.prepare (spec);
    powerGain.setRampDurationSeconds (0.05);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR); // 8x
    oversampling->initProcessing ((size_t) samplesPerBlock);

    limiter.prepare (sampleRate, numCh, 2.0f, 90.0f);
    meter.prepare (sampleRate, numCh);

    const int osLat = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (osLat + limiter.getLatencySamples());
}

void BloodmoneyMasterProcessor::updateParams()
{
    auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
    const float subDb  = apvts.getRawParameterValue ("sub")->load();
    const float presDb = apvts.getRawParameterValue ("presence")->load();
    const float airDb  = apvts.getRawParameterValue ("air")->load();
    const float punch  = apvts.getRawParameterValue ("punch")->load();

    *hpf.state      = *Coefs::makeHighPass  (currentSR,    30.0f, 0.707f);
    *subShelf.state = *Coefs::makeLowShelf  (currentSR,    90.0f, 0.707f, g (subDb));
    *lowMid.state   = *Coefs::makePeakFilter(currentSR,   300.0f, 1.0f,   g (-(subDb * 0.6f + 1.0f)));
    *presence.state = *Coefs::makePeakFilter(currentSR,  3600.0f, 0.9f,   g (presDb));
    *airShelf.state = *Coefs::makeHighShelf (currentSR, 10500.0f, 0.707f, g (airDb));

    comp.setThreshold (-12.0f - (punch / 100.0f) * 16.0f);
    comp.setRatio     (  1.5f + (punch / 100.0f) *  4.5f);
    comp.setAttack    (  5.0f);
    comp.setRelease   (130.0f);
}

void BloodmoneyMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int c = totalIn; c < totalOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    updateParams();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // EQ + glue
    hpf.process (ctx); subShelf.process (ctx); lowMid.process (ctx);
    presence.process (ctx); airShelf.process (ctx);
    comp.process (ctx);

    // Puissance : gain qui pousse dans le clipper (= ce qui monte le LUFS)
    powerGain.setGainDecibels (apvts.getRawParameterValue ("power")->load());
    powerGain.process (ctx);

    // Clipper "propre", 8x surechantillonne (anti-aliasing = clarte)
    const float ceil = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("ceiling")->load());
    const float knee = juce::jmap (apvts.getRawParameterValue ("clip")->load(), 0.0f, 100.0f, 0.0f, 0.6f);
    auto up = oversampling->processSamplesUp (block);
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        auto* s = up.getChannelPointer (ch);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            s[i] = cleanClip (s[i], ceil, knee);
    }
    oversampling->processSamplesDown (block);

    // Filet de securite : limiteur look-ahead (cretes inter-echantillons)
    limiter.setCeiling (ceil);
    limiter.process (buffer.getArrayOfWritePointers(), buffer.getNumSamples());

    // mesure LUFS de la sortie
    meter.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* BloodmoneyMasterProcessor::createEditor()
{
    return new BloodmoneyMasterEditor (*this);
}

void BloodmoneyMasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void BloodmoneyMasterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BloodmoneyMasterProcessor();
}
