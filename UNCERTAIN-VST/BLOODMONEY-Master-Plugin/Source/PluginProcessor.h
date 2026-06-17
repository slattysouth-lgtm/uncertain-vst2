#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "LookaheadLimiter.h"
#include "MasterDSP.h"

//==============================================================================
//  BLOODMONEY // Master Engine
//  HPF -> EQ 4 bandes -> compression -> [Puissance] -> clipper 8x surechantillonne
//      -> limiteur look-ahead (securite plafond). Indicateur LUFS court-terme.
//==============================================================================
class BloodmoneyMasterProcessor : public juce::AudioProcessor
{
public:
    BloodmoneyMasterProcessor();
    ~BloodmoneyMasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BLOODMONEY Master"; }
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

    float getLufs() const { return meter.getLufs(); }   // pour l'affichage

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefs  = juce::dsp::IIR::Coefficients<float>;
    using Dup    = juce::dsp::ProcessorDuplicator<Filter, Coefs>;

    Dup hpf, subShelf, lowMid, presence, airShelf;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Gain<float> powerGain;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    LookaheadLimiter limiter;
    LoudnessMeter meter;

    double currentSR = 44100.0;
    int    numCh     = 2;

    void updateParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BloodmoneyMasterProcessor)
};
