#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

//==============================================================================
//  BLOODMONEY look-ahead brick-wall limiter
//
//  Real-time port of the offline algorithm: a sliding-window maximum of the
//  inter-channel peak (over the look-ahead window) drives the gain, applied to
//  the audio delayed by the same window. Because the detector "sees" the peak
//  before it reaches the output, the gain is already pulled down when it lands
//  -> no hard clipping. A soft-clip (tanh knee) backstops any residual
//  overshoot smoothly. The whole point: loud, never harsh.
//
//  One gain is computed across all channels so the stereo image is preserved.
//==============================================================================
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int channels, float lookaheadMs, float releaseMs)
    {
        sr     = sampleRate;
        numCh  = std::max (1, channels);
        L      = std::max (1, (int) std::lround (sampleRate * lookaheadMs / 1000.0));
        cap    = L + 2;

        delay.assign ((size_t) numCh * (size_t) cap, 0.0f);
        dqVal.assign ((size_t) cap, 0.0f);
        dqPos.assign ((size_t) cap, 0LL);
        dqHead = dqTail = 0;
        n = 0;
        currentGain = 1.0f;

        // attack reaches the target within ~ the look-ahead window
        const float attackMs = lookaheadMs;
        attackCoeff  = std::exp (-1.0f / (float) (sr * (attackMs  / 1000.0)));
        releaseCoeff = std::exp (-1.0f / (float) (sr * (releaseMs / 1000.0)));
    }

    int  getLatencySamples() const noexcept { return L; }
    void setCeiling (float linearCeiling) noexcept { ceiling = std::max (1.0e-4f, linearCeiling); }

    // Processes the buffer in place (one shared gain across all channels).
    void process (float* const* channelData, int numSamples) noexcept
    {
        const float knee = 0.88f * ceiling;

        for (int i = 0; i < numSamples; ++i)
        {
            // ---- detector: inter-channel peak ----
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const float a = std::fabs (channelData[c][i]);
                if (a > peak) peak = a;
            }

            // ---- sliding-window maximum of the peak over [n-L, n] ----
            while (dqHead < dqTail && dqPos[(size_t) (dqHead % cap)] < n - L) ++dqHead;
            while (dqHead < dqTail && dqVal[(size_t) ((dqTail - 1) % cap)] <= peak) --dqTail;
            dqVal[(size_t) (dqTail % cap)] = peak;
            dqPos[(size_t) (dqTail % cap)] = n;
            ++dqTail;
            const float windowMax = dqVal[(size_t) (dqHead % cap)];

            // ---- target gain + attack/release smoothing ----
            const float target = (windowMax > ceiling) ? (ceiling / windowMax) : 1.0f;
            currentGain = (target < currentGain)
                            ? target + (currentGain - target) * attackCoeff
                            : target + (currentGain - target) * releaseCoeff;

            // ---- delay audio by L, apply gain, soft-clip safety ----
            const int wIdx = (int) (n % cap);
            int rIdx = (int) ((n - L) % cap); if (rIdx < 0) rIdx += cap;

            for (int c = 0; c < numCh; ++c)
            {
                float* d = &delay[(size_t) c * (size_t) cap];
                d[wIdx] = channelData[c][i];

                float out = d[rIdx] * currentGain;

                const float ax = std::fabs (out);
                if (ax > knee)
                {
                    const float sgn  = (out < 0.0f) ? -1.0f : 1.0f;
                    const float over = (ax - knee) / (ceiling - knee);
                    out = sgn * (knee + (ceiling - knee) * std::tanh (over));
                }
                channelData[c][i] = out;
            }

            ++n;
        }
    }

    void reset() noexcept
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        dqHead = dqTail = 0; n = 0; currentGain = 1.0f;
    }

private:
    double sr = 44100.0;
    int    numCh = 2, L = 64, cap = 66;
    long long n = 0, dqHead = 0, dqTail = 0;
    float  ceiling = 0.89f, currentGain = 1.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f;

    std::vector<float>     delay, dqVal;
    std::vector<long long> dqPos;
};
