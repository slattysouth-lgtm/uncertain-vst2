/*
  ==============================================================================
   PluginProcessor.h  —  Room Corrector (Uncertain)  v4
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "DSP.h"

class RoomCorrectorAudioProcessor : public juce::AudioProcessor
{
public:
    RoomCorrectorAudioProcessor();
    ~RoomCorrectorAudioProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor()      const override { return true; }

    const juce::String getName() const override { return "Room Corrector"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                        override { return 1; }
    int  getCurrentProgram()                     override { return 0; }
    void setCurrentProgram (int)                 override {}
    const juce::String getProgramName (int)      override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData)       override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    SpectralCleaner cleaner[2];
    Enhancer        enhancer[2];
    AdaptiveGate    gate[2];
    MixReady        mixReady;
    DelayLine       dryDelay;

    juce::AudioBuffer<float> dryBuf;

    juce::SmoothedValue<float> cleanSm, repairSm, toneSm, compSm, sibSm, mixSm, outSm;
    float tpGain=1.0f; // etat du plafond de securite true-peak

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomCorrectorAudioProcessor)
};
