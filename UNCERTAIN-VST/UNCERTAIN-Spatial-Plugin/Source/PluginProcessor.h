#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

//==============================================================================
//  UNCERTAIN // SPATIAL
//  Traitement multibande "par zones d'instruments" :
//  4 bandes (graves / bas-med / mediums / aigus), chacune avec sa largeur
//  stereo (M/S) et sa dynamique. Graves -> mono, aigus -> elargis.
//  Ce n'est PAS de la separation de sources : c'est un traitement par zone
//  de frequences (la ou vivent les familles d'instruments).
//==============================================================================
class UncertainSpatialProcessor : public juce::AudioProcessor
{
public:
    UncertainSpatialProcessor();
    ~UncertainSpatialProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UNCERTAIN Spatial"; }
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

    float getBandDb (int b) const { return bandDb[(size_t) juce::jlimit (0, 3, b)].load(); }

private:
    using LR = juce::dsp::LinkwitzRileyFilter<float>;

    LR xover1, xover2, xover3;                     // 120 / 500 / 5000 Hz
    std::array<juce::dsp::Compressor<float>, 4> comp;

    double currentSR = 44100.0;
    int    numCh     = 2;

    std::array<double, 4> env { { 0, 0, 0, 0 } };
    double envCoef = 0.0;
    std::array<std::atomic<float>, 4> bandDb { { {-60.0f}, {-60.0f}, {-60.0f}, {-60.0f} } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UncertainSpatialProcessor)
};
