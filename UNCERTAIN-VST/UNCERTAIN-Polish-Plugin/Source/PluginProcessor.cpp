#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
UncertainPolishProcessor::UncertainPolishProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout UncertainPolishProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "polish", 1 }, "Polish",
        juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 50.0f));
    return { p.begin(), p.end() };
}

//==============================================================================
bool UncertainPolishProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void UncertainPolishProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    numCh     = juce::jmax (1, getTotalNumOutputChannels());

    suppressor.prepare (sampleRate, numCh);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    comp.prepare (spec);
    wetMakeup.prepare (spec);
    wetMakeup.setRampDurationSeconds (0.05);

    glueBuffer.setSize (numCh, samplesPerBlock, false, false, true);

    setLatencySamples (suppressor.getLatencySamples());
}

void UncertainPolishProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int c = totalIn; c < totalOut; ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int ns = buffer.getNumSamples();
    const int ch = juce::jmin (numCh, buffer.getNumChannels());

    // one macro knob -> 0..1
    const float polish = apvts.getRawParameterValue ("polish")->load();
    const float amount = juce::jlimit (0.0f, 1.0f, (polish - 1.0f) / 99.0f);

    // ---- 1. spectral polish (resonance / harsh-peak suppression) ----
    suppressor.setAmount (amount);
    suppressor.process (buffer);

    // ---- 2. parallel glue compression ----
    const float thr   = juce::jmap (amount, 0.0f, 1.0f, -14.0f, -28.0f);
    const float ratio = juce::jmap (amount, 0.0f, 1.0f,   2.5f,   6.0f);
    const float mix   = juce::jmap (amount, 0.0f, 1.0f,  0.05f,  0.45f);
    comp.setThreshold (thr);
    comp.setRatio     (ratio);
    comp.setAttack    (20.0f);    // slow attack lets transients through = glue, not pump
    comp.setRelease   (180.0f);

    // wet copy
    for (int c = 0; c < ch; ++c)
        glueBuffer.copyFrom (c, 0, buffer, c, 0, ns);

    juce::dsp::AudioBlock<float> wetBlock (glueBuffer.getArrayOfWritePointers(),
                                           (size_t) ch, (size_t) ns);
    juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
    comp.process (wetCtx);

    // approximate makeup so the compressed path sits at dry level before blending
    const float makeupDb = -thr * (1.0f - 1.0f / ratio) * 0.6f;
    wetMakeup.setGainDecibels (makeupDb);
    wetMakeup.process (wetCtx);

    // blend : (1-mix)*dry + mix*wet
    for (int c = 0; c < ch; ++c)
    {
        auto* dry = buffer.getWritePointer (c);
        const auto* wet = glueBuffer.getReadPointer (c);
        for (int i = 0; i < ns; ++i)
            dry[i] = (1.0f - mix) * dry[i] + mix * wet[i];
    }
}

//==============================================================================
juce::AudioProcessorEditor* UncertainPolishProcessor::createEditor()
{
    return new UncertainPolishEditor (*this);
}

void UncertainPolishProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void UncertainPolishProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UncertainPolishProcessor();
}
