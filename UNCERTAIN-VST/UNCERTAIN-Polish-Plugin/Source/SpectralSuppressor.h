#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <memory>

//==============================================================================
//  UNCERTAIN - Spectral resonance / harsh-peak suppressor
//
//  STFT (Hann, 75% overlap). For each frame we estimate the SMOOTH spectral
//  envelope (a box average of the magnitude spectrum). Bins that stick out
//  above that envelope = resonances / harsh peaks; we pull them back down.
//  Everything that sits at or below the envelope is left untouched.
//
//  setAmount(0..1) scales how aggressively peaks are tamed (threshold + depth
//  + max attenuation move together).
//==============================================================================
class SpectralSuppressor
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr  = sampleRate;
        nCh = juce::jmax (1, numChannels);

        makeWindow();
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        chans.clear();
        chans.resize ((size_t) nCh);
        for (auto& c : chans) c.prepare (fftSize);

        mag.assign    ((size_t) (fftSize / 2 + 1), 0.0f);
        prefix.assign ((size_t) (fftSize / 2 + 2), 0.0f);
    }

    int  getLatencySamples() const noexcept { return fftSize; }
    void setAmount (float a) noexcept        { amount = juce::jlimit (0.0f, 1.0f, a); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int ns = buffer.getNumSamples();
        const int chToDo = juce::jmin (nCh, buffer.getNumChannels());
        for (int ch = 0; ch < chToDo; ++ch)
            processChannel (chans[(size_t) ch], buffer.getWritePointer (ch), ns);
    }

    void reset() { for (auto& c : chans) c.clear(); }

private:
    static constexpr int fftOrder = 11;            // 2048-point FFT
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hop      = fftSize / 4;   // 75% overlap

    struct ChannelState
    {
        std::vector<float> inFifo, outFifo, work;  // work = 2*fftSize (real-only FFT)
        int idx = 0, hopCount = 0;

        void prepare (int N)
        {
            inFifo.assign  ((size_t) N, 0.0f);
            outFifo.assign ((size_t) N, 0.0f);
            work.assign    ((size_t) (2 * N), 0.0f);
            idx = 0; hopCount = 0;
        }
        void clear()
        {
            std::fill (inFifo.begin(),  inFifo.end(),  0.0f);
            std::fill (outFifo.begin(), outFifo.end(), 0.0f);
            idx = 0; hopCount = 0;
        }
    };

    void makeWindow()
    {
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                          * (float) i / (float) fftSize);
        olaNorm = 1.0f / 1.5f; // Hann analysis+synthesis @ 75% overlap sums to 1.5
    }

    void processChannel (ChannelState& s, float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            s.inFifo[(size_t) s.idx] = data[i];
            data[i] = s.outFifo[(size_t) s.idx];   // delayed (by fftSize) processed output
            s.outFifo[(size_t) s.idx] = 0.0f;

            s.idx = (s.idx + 1) % fftSize;
            if (++s.hopCount >= hop) { s.hopCount = 0; processFrame (s); }
        }
    }

    void processFrame (ChannelState& s)
    {
        auto& w = s.work;
        std::fill (w.begin(), w.end(), 0.0f);

        // windowed analysis frame (oldest sample first)
        for (int k = 0; k < fftSize; ++k)
            w[(size_t) k] = s.inFifo[(size_t) ((s.idx + k) % fftSize)] * window[(size_t) k];

        fft->performRealOnlyForwardTransform (w.data());
        processSpectrum (w);
        fft->performRealOnlyInverseTransform (w.data());

        // windowed synthesis + overlap-add
        for (int k = 0; k < fftSize; ++k)
            s.outFifo[(size_t) ((s.idx + k) % fftSize)] += w[(size_t) k] * window[(size_t) k] * olaNorm;
    }

    void processSpectrum (std::vector<float>& w)
    {
        const int half = fftSize / 2;

        // magnitudes 0..half
        for (int b = 0; b <= half; ++b)
        {
            const float re = w[(size_t) (2 * b)];
            const float im = w[(size_t) (2 * b + 1)];
            mag[(size_t) b] = std::sqrt (re * re + im * im);
        }

        // prefix sum for fast box-average envelope
        prefix[0] = 0.0f;
        for (int b = 0; b <= half; ++b)
            prefix[(size_t) (b + 1)] = prefix[(size_t) b] + mag[(size_t) b];

        // amount -> processing strength
        const float threshFactor = juce::Decibels::decibelsToGain (juce::jmap (amount, 0.0f, 1.0f, 6.0f, 1.5f));
        const float depth        = juce::jmap (amount, 0.0f, 1.0f, 0.30f, 0.90f);
        const float minGain      = juce::Decibels::decibelsToGain (juce::jmap (amount, 0.0f, 1.0f, -4.0f, -16.0f));
        const int   R            = 12;   // envelope smoothing radius (bins)

        for (int b = 0; b <= half; ++b)
        {
            const int lo = juce::jmax (0, b - R);
            const int hi = juce::jmin (half, b + R);
            const float env = (prefix[(size_t) (hi + 1)] - prefix[(size_t) lo]) / (float) (hi - lo + 1);

            const float m = mag[(size_t) b];
            float g = 1.0f;
            const float thresh = env * threshFactor;
            if (m > thresh && thresh > 1.0e-9f)
            {
                const float over    = m / thresh;                       // > 1
                const float reduced = thresh * std::pow (over, 1.0f - depth);
                g = juce::jmax (reduced / m, minGain);
            }

            // apply real gain to bin b and its conjugate mirror (keeps output real)
            w[(size_t) (2 * b)]     *= g;
            w[(size_t) (2 * b + 1)] *= g;
            if (b > 0 && b < half)
            {
                const int m2 = fftSize - b;
                w[(size_t) (2 * m2)]     *= g;
                w[(size_t) (2 * m2 + 1)] *= g;
            }
        }
    }

    double sr = 44100.0;
    int    nCh = 2;
    float  amount = 0.5f, olaNorm = 1.0f / 1.5f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, mag, prefix;
    std::vector<ChannelState> chans;
};
