#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
UncertainSpatialProcessor::UncertainSpatialProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout UncertainSpatialProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<P>(juce::ParameterID{ "image", 1 }, "Image",     R(0.0f, 100.0f, 1.0f), 50.0f));
    p.push_back (std::make_unique<P>(juce::ParameterID{ "dyn",   1 }, "Dynamique", R(0.0f, 100.0f, 1.0f), 35.0f));
    return { p.begin(), p.end() };
}

bool UncertainSpatialProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void UncertainSpatialProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    numCh     = juce::jmax (1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };

    for (auto* x : { &xover1, &xover2, &xover3 })
    {
        x->prepare (spec);
        x->setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    }
    xover1.setCutoffFrequency (120.0f);
    xover2.setCutoffFrequency (500.0f);
    xover3.setCutoffFrequency (5000.0f);

    for (auto& c : comp) c.prepare (spec);

    // timing par zone : percussifs rapides, tonal plus lent
    const float atk[4] = { 15.0f, 12.0f, 10.0f, 5.0f };
    const float rel[4] = { 160.0f, 140.0f, 120.0f, 90.0f };
    for (int b = 0; b < 4; ++b) { comp[(size_t) b].setAttack (atk[b]); comp[(size_t) b].setRelease (rel[b]); }

    envCoef = std::exp (-1.0 / (sampleRate * 0.3)); // ~300 ms
    env = { 0, 0, 0, 0 };
}

void UncertainSpatialProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int c = totalIn; c < totalOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const float image = apvts.getRawParameterValue ("image")->load() / 100.0f;
    const float dyn   = apvts.getRawParameterValue ("dyn")->load()   / 100.0f;

    // largeur stereo par zone
    const float width[4] = {
        juce::jmap (image, 0.0f, 1.0f, 1.0f, 0.0f),   // graves -> mono
        juce::jmap (image, 0.0f, 1.0f, 1.0f, 0.7f),   // bas-med
        juce::jmap (image, 0.0f, 1.0f, 1.0f, 1.05f),  // mediums (voix)
        juce::jmap (image, 0.0f, 1.0f, 1.0f, 1.6f)    // aigus -> elargis
    };

    // dynamique par zone
    const float thr   = juce::jmap (dyn, 0.0f, 1.0f, 0.0f, -16.0f);
    const float ratio = juce::jmap (dyn, 0.0f, 1.0f, 1.0f,  3.5f);
    for (auto& c : comp) { c.setThreshold (thr); c.setRatio (ratio); }

    const int ns = buffer.getNumSamples();

    if (numCh >= 2)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        for (int i = 0; i < ns; ++i)
        {
            // --- split en 4 bandes (par canal) ---
            float bL[4], bR[4], hpL, hpR, t2L, t2R;
            xover1.processSample (0, L[i], bL[0], hpL);
            xover1.processSample (1, R[i], bR[0], hpR);
            xover2.processSample (0, hpL, bL[1], t2L);
            xover2.processSample (1, hpR, bR[1], t2R);
            xover3.processSample (0, t2L, bL[2], bL[3]);
            xover3.processSample (1, t2R, bR[2], bR[3]);

            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < 4; ++b)
            {
                // largeur M/S
                float m = 0.5f * (bL[b] + bR[b]);
                float s = 0.5f * (bL[b] - bR[b]) * width[b];
                float l = m + s, r = m - s;

                // dynamique
                l = comp[(size_t) b].processSample (0, l);
                r = comp[(size_t) b].processSample (1, r);

                // vumetre (energie mid)
                env[(size_t) b] = envCoef * env[(size_t) b] + (1.0 - envCoef) * (double) (m * m);

                outL += l; outR += r;
            }
            L[i] = outL; R[i] = outR;
        }
    }
    else
    {
        auto* M = buffer.getWritePointer (0);
        for (int i = 0; i < ns; ++i)
        {
            float b[4], hp, t2;
            xover1.processSample (0, M[i], b[0], hp);
            xover2.processSample (0, hp,   b[1], t2);
            xover3.processSample (0, t2,   b[2], b[3]);
            float out = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                float v = comp[(size_t) k].processSample (0, b[k]);
                env[(size_t) k] = envCoef * env[(size_t) k] + (1.0 - envCoef) * (double) (b[k] * b[k]);
                out += v;
            }
            M[i] = out;
        }
    }

    for (int b = 0; b < 4; ++b)
        bandDb[(size_t) b].store (juce::jlimit (-60.0f, 6.0f,
            (float) (10.0 * std::log10 (env[(size_t) b] + 1.0e-9))));
}

//==============================================================================
juce::AudioProcessorEditor* UncertainSpatialProcessor::createEditor()
{
    return new UncertainSpatialEditor (*this);
}

void UncertainSpatialProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void UncertainSpatialProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UncertainSpatialProcessor();
}
