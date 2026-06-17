#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

//==============================================================================
//  Clipper "propre" : clip dur avec petit genou optionnel.
//  A utiliser DANS le domaine suréchantillonné (8x) pour eviter l'aliasing.
//  Garantit |sortie| <= ceiling.
//==============================================================================
static inline float cleanClip (float x, float ceiling, float knee) noexcept
{
    const float ax = std::fabs (x);
    const float k  = ceiling * (1.0f - knee);
    if (ax <= k) return x;

    const float s = (x < 0.0f) ? -1.0f : 1.0f;
    if (knee < 1.0e-4f)                       // clip dur (le plus net / le plus fort)
        return s * juce::jmin (ax, ceiling);

    const float t      = (ax - k) / (ceiling - k);          // > 0
    const float shaped = k + (ceiling - k) * (1.0f - std::exp (-t)); // -> ceiling sans depasser
    return s * juce::jmin (shaped, ceiling);
}

//==============================================================================
//  Mesure de loudness court-terme approchee (ponderation K facon BS.1770).
//  Filtre K = high-shelf (+4 dB) puis high-pass, puis moyenne glissante ~3 s.
//==============================================================================
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int channels)
    {
        sr = sampleRate;
        nCh = juce::jmax (1, channels);
        shelf = makeHighShelf (sr, 1681.97, 0.7071, 3.999);
        hp    = makeHighPass  (sr, 38.135, 0.5);
        st.assign ((size_t) nCh, {});
        msAvg = 1.0e-7f;
        oneMinus = 1.0f - std::exp (-1.0f / (float) (sr * 3.0)); // ~3 s
    }

    void process (const juce::AudioBuffer<float>& buffer)
    {
        const int ns = buffer.getNumSamples();
        const int ch = juce::jmin (nCh, buffer.getNumChannels());
        for (int i = 0; i < ns; ++i)
        {
            float inst = 0.0f;
            for (int c = 0; c < ch; ++c)
            {
                float v = buffer.getSample (c, i);
                v = st[(size_t) c].s1.process (v, shelf);
                v = st[(size_t) c].s2.process (v, hp);
                inst += v * v;
            }
            msAvg += oneMinus * (inst - msAvg);
        }
        const float lufs = -0.691f + 10.0f * std::log10 (juce::jmax (msAvg, 1.0e-7f));
        current.store (juce::jlimit (-70.0f, 0.0f, lufs));
    }

    float getLufs() const { return current.load(); }

private:
    struct Coeffs { float b0, b1, b2, a1, a2; };
    struct Biquad
    {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process (float x, const Coeffs& c) noexcept
        {
            const float y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };
    struct ChState { Biquad s1, s2; };

    static Coeffs makeHighShelf (double fs, double f0, double Q, double dB)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cw = std::cos (w0), sw = std::sin (w0), alpha = sw / (2.0 * Q);
        const double A = std::pow (10.0, dB / 40.0), sa = 2.0 * std::sqrt (A) * alpha;
        const double b0 =      A * ((A + 1) + (A - 1) * cw + sa);
        const double b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        const double b2 =      A * ((A + 1) + (A - 1) * cw - sa);
        const double a0 =          (A + 1) - (A - 1) * cw + sa;
        const double a1 =      2 * ((A - 1) - (A + 1) * cw);
        const double a2 =          (A + 1) - (A - 1) * cw - sa;
        return { (float)(b0/a0), (float)(b1/a0), (float)(b2/a0), (float)(a1/a0), (float)(a2/a0) };
    }
    static Coeffs makeHighPass (double fs, double f0, double Q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cw = std::cos (w0), sw = std::sin (w0), alpha = sw / (2.0 * Q);
        const double b0 = (1 + cw) / 2, b1 = -(1 + cw), b2 = (1 + cw) / 2;
        const double a0 = 1 + alpha, a1 = -2 * cw, a2 = 1 - alpha;
        return { (float)(b0/a0), (float)(b1/a0), (float)(b2/a0), (float)(a1/a0), (float)(a2/a0) };
    }

    double sr = 44100.0;
    int nCh = 2;
    Coeffs shelf {}, hp {};
    std::vector<ChState> st;
    float msAvg = 1.0e-7f, oneMinus = 0.0f;
    std::atomic<float> current { -70.0f };
};
