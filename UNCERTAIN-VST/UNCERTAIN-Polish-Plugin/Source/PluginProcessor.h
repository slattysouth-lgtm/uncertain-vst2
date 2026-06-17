#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "SpectralSuppressor.h"

//==============================================================================
//  UNCERTAIN // POLISH  -  pre-master cleanup + glue (one knob)
//  Spectral resonance/peak suppression  ->  parallel glue compression.
//  A placer AVANT BLOODMONEY Master.
//==============================================================================
class UncertainPolishProcessor : public juce::AudioProcessor
{
public:
    UncertainPolishProcessor();
    ~UncertainPolishProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UNCERTAIN Polish"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    SpectralSuppressor          suppressor;
    juce::dsp::Compressor<float> comp;          // parallel (wet) glue
    juce::dsp::Gain<float>       wetMakeup;
    juce::AudioBuffer<float>     glueBuffer;     // wet copy

    double currentSR = 44100.0;
    int    numCh     = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainPolishProcessor)
};
