#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <vector>

// Linear-phase engine for the static bands of the EQ.
//
// The magnitude of every static band is combined into one 2 x 2 matrix per frequency bin (left / right in, left / right out),
// which also covers Left, Right, Mid, Side and the split pairs. The matrix is turned into a symmetric (zero-phase) FIR, windowed
// to `taps` and delayed by taps / 2, and applied with FFT overlap-add on the audio thread.
// A background thread does the design, so dragging a band never blocks the audio.
//
// Total latency = hop + taps / 2 samples.
namespace EQ
{
// One band as the designer sees it: a plain copy of its parameters.
struct LinBand
{
    bool active = false;                // on, not muted, and not animated by an LFO / dynamics
    int type = 0;
    float slope = 12.0f;                // dB/oct, Low / High Pass only
    int channel = 0;                    // 0 Stereo, 1 Left, 2 Right, 3 Mid, 4 Side
    int split = 0;                      // 0 off, 1 Left | Right, 2 Mid | Side  (already 0 when the band cannot split)
    float freq = 1000.0f, gain = 0.0f, q = 1.0f;

    bool operator== (const LinBand& o) const
    {
        return active == o.active && type == o.type && slope == o.slope && channel == o.channel && split == o.split
               && freq == o.freq && gain == o.gain && q == o.q;
    }
    bool operator!= (const LinBand& o) const { return ! (*this == o); }
};

class LinearPhase : private juce::Thread
{
public:
    using Fetch = std::function<void (std::vector<LinBand>&)>;                       // worker thread: current bands
    using Magnitude = std::function<double (double)>;                                // linear magnitude at a frequency in Hz
    using MagFactory = std::function<Magnitude (const LinBand&, bool secondSide, double sampleRate)>;

    LinearPhase (int tapCount, int hopSize, Fetch f, MagFactory m)
        : juce::Thread ("Dypelv linear phase"),
          taps (tapCount), hop (hopSize), fetch (std::move (f)), makeMag (std::move (m))
    {
        order = 1;
        while ((1 << order) < taps + hop) ++order;
        size = 1 << order;
        bins = size / 2 + 1;

        fftAudio = std::make_unique<juce::dsp::FFT> (order);
        fftDesign = std::make_unique<juce::dsp::FFT> (order);

        const auto sz = (size_t) size;
        for (auto* v : { &bufA, &bufB, &tmpA, &tmpB, &work, &fir })
            v->assign (2 * sz, 0.0f);
        for (auto* v : { &olaA, &olaB })
            v->assign (sz, 0.0f);
        for (auto* v : { &inA, &inB, &outA, &outB })
            v->assign ((size_t) hop, 0.0f);
        for (size_t e = 0; e < 4; ++e)
        {
            local[e].assign (2 * sz, 0.0f);
            shared[e].assign (2 * sz, 0.0f);
            scratch[e].assign (2 * sz, 0.0f);
            t[e].assign ((size_t) bins, 0.0f);
        }
    }

    ~LinearPhase() override { stopThread (3000); }

    int latencySamples() const { return hop + taps / 2; }

    // message thread, from prepareToPlay
    void prepare (double sr, int numChannels)
    {
        stopThread (3000);                       // the designer must not run while the state is rebuilt
        sampleRate.store (sr);
        channels.store (juce::jlimit (1, 2, numChannels));
        cache.clear();
        resetStream();

        // identity design right away, so the engine is a clean delay until the worker has designed the real thing
        designNow (std::vector<LinBand>());
        localVersion = 0;
        refresh();

        forceDesign.store (true);
        if (! isThreadRunning())
            startThread (juce::Thread::Priority::low);
    }

    void setActive (bool a)
    {
        active.store (a);
        if (a) forceDesign.store (true);
        notify();
    }

    // audio thread: forget everything that was streaming (used when the engine is switched on)
    void resetStream()
    {
        pos = 0;
        for (auto* v : { &olaA, &olaB, &inA, &inB, &outA, &outB })
            std::fill (v->begin(), v->end(), 0.0f);
    }

    // audio thread. r may be null (mono). Output is the input delayed by latencySamples() and filtered.
    void process (float* l, float* r, int n)
    {
        refresh();
        const bool stereo = r != nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float oL = outA[(size_t) pos];
            inA[(size_t) pos] = l[i];
            l[i] = oL;
            if (stereo)
            {
                const float oR = outB[(size_t) pos];
                inB[(size_t) pos] = r[i];
                r[i] = oR;
            }
            if (++pos == hop)
            {
                runHop (stereo);
                pos = 0;
            }
        }
    }

private:
    // ------------------------------------------------------------ audio thread
    void refresh()
    {
        const auto v = version.load();
        if (v == localVersion) return;
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (! tl.isLocked()) return;                       // the designer is publishing: keep the old filter for one more hop
        for (size_t e = 0; e < 4; ++e)
        {
            std::copy (shared[e].begin(), shared[e].begin() + 2 * bins, local[e].begin());
            localNz[e] = sharedNz[e];
        }
        localVersion = version.load();
    }

    void runHop (bool stereo)
    {
        const size_t sz = (size_t) size;
        std::copy (inA.begin(), inA.end(), bufA.begin());
        std::fill (bufA.begin() + hop, bufA.end(), 0.0f);
        fftAudio->performRealOnlyForwardTransform (bufA.data(), true);
        if (stereo)
        {
            std::copy (inB.begin(), inB.end(), bufB.begin());
            std::fill (bufB.begin() + hop, bufB.end(), 0.0f);
            fftAudio->performRealOnlyForwardTransform (bufB.data(), true);
        }

        const float* h00 = local[0].data();
        const float* h01 = local[1].data();
        const float* h10 = local[2].data();
        const float* h11 = local[3].data();
        const bool n00 = localNz[0], n01 = localNz[1] && stereo, n10 = localNz[2] && stereo, n11 = localNz[3] && stereo;

        for (int k = 0; k < bins; ++k)
        {
            const size_t i = (size_t) (2 * k);
            const float ar = bufA[i], ai = bufA[i + 1];
            const float br = stereo ? bufB[i] : 0.0f, bi = stereo ? bufB[i + 1] : 0.0f;

            float yr = 0.0f, yi = 0.0f;                                   // left out = H00 * left + H01 * right
            if (n00) { yr += h00[i] * ar - h00[i + 1] * ai;  yi += h00[i] * ai + h00[i + 1] * ar; }
            if (n01) { yr += h01[i] * br - h01[i + 1] * bi;  yi += h01[i] * bi + h01[i + 1] * br; }
            tmpA[i] = yr;  tmpA[i + 1] = yi;

            if (stereo)
            {
                float zr = 0.0f, zi = 0.0f;                               // right out = H10 * left + H11 * right
                if (n10) { zr += h10[i] * ar - h10[i + 1] * ai;  zi += h10[i] * ai + h10[i + 1] * ar; }
                if (n11) { zr += h11[i] * br - h11[i + 1] * bi;  zi += h11[i] * bi + h11[i + 1] * br; }
                tmpB[i] = zr;  tmpB[i + 1] = zi;
            }
        }

        finishChannel (tmpA, olaA, outA, sz);
        if (stereo) finishChannel (tmpB, olaB, outB, sz);
    }

    void finishChannel (std::vector<float>& spec, std::vector<float>& ola, std::vector<float>& out, size_t sz)
    {
        fftAudio->performRealOnlyInverseTransform (spec.data());          // time domain in the first `size` floats
        for (size_t j = 0; j < sz; ++j)
            ola[j] += spec[j];
        std::copy (ola.begin(), ola.begin() + hop, out.begin());          // finished samples
        std::copy (ola.begin() + hop, ola.end(), ola.begin());            // slide the tail down
        std::fill (ola.end() - hop, ola.end(), 0.0f);
    }

    // ------------------------------------------------------------ design thread
    void run() override
    {
        std::vector<LinBand> now, last;
        while (! threadShouldExit())
        {
            if (active.load())
            {
                fetch (now);
                if (forceDesign.exchange (false) || now != last)
                {
                    designNow (now);
                    last = now;
                }
            }
            wait (25);
        }
    }

    struct BandCache
    {
        bool valid = false, pair = false;
        LinBand key;
        double sr = 0.0;
        std::vector<float> hA, hB;
    };

    void designNow (const std::vector<LinBand>& bands)
    {
        const double sr = sampleRate.load();
        const bool mono = channels.load() < 2;
        const double df = sr / (double) size;

        // 2 x 2 matrix per bin, starts as identity: [t00 t01; t10 t11]
        std::fill (t[0].begin(), t[0].end(), 1.0f);
        std::fill (t[1].begin(), t[1].end(), 0.0f);
        std::fill (t[2].begin(), t[2].end(), 0.0f);
        std::fill (t[3].begin(), t[3].end(), 1.0f);

        if (cache.size() != bands.size()) cache.assign (bands.size(), BandCache());

        for (size_t bi = 0; bi < bands.size(); ++bi)
        {
            const auto& b = bands[bi];
            auto& c = cache[bi];
            if (! b.active) { c.valid = false; continue; }
            if (mono && b.channel != 0 && b.channel != 1 && b.channel != 3) { c.valid = false; continue; }   // Right / Side do nothing on a mono bus

            if (! c.valid || c.key != b || c.sr != sr)
            {
                c.pair = b.split > 0;
                c.hA.assign ((size_t) bins, 1.0f);
                c.hB.assign ((size_t) bins, 1.0f);
                const auto magA = makeMag (b, false, sr);
                const auto magB = c.pair ? makeMag (b, true, sr) : Magnitude();
                for (int k = 0; k < bins; ++k)
                {
                    const double f = juce::jmax (0.01, (double) k * df);
                    const float a = (float) magA (f);
                    c.hA[(size_t) k] = a;
                    c.hB[(size_t) k] = c.pair ? (float) magB (f) : a;
                }
                c.key = b;
                c.sr = sr;
                c.valid = true;
            }

            for (int k = 0; k < bins; ++k)
            {
                const size_t ki = (size_t) k;
                const float hA = c.hA[ki], hB = c.hB[ki];
                float m00 = 1.0f, m01 = 0.0f, m10 = 0.0f, m11 = 1.0f;

                if (mono)                                   // a single channel: only the left path exists
                    m00 = hA;
                else if (b.split == 1)                      // Left | Right: two independent filters
                {
                    m00 = hA;  m11 = hB;
                }
                else if (b.split == 2)                      // Mid | Side
                {
                    m00 = m11 = 0.5f * (hA + hB);
                    m01 = m10 = 0.5f * (hA - hB);
                }
                else
                {
                    switch (b.channel)
                    {
                        case 1:  m00 = hA; break;                                    // Left
                        case 2:  m11 = hA; break;                                    // Right
                        case 3:  m00 = m11 = 0.5f * (hA + 1.0f);  m01 = m10 = 0.5f * (hA - 1.0f); break;   // Mid
                        case 4:  m00 = m11 = 0.5f * (1.0f + hA);  m01 = m10 = 0.5f * (1.0f - hA); break;   // Side
                        default: m00 = m11 = hA; break;                              // Stereo
                    }
                }

                const float a00 = t[0][ki], a01 = t[1][ki], a10 = t[2][ki], a11 = t[3][ki];
                t[0][ki] = m00 * a00 + m01 * a10;
                t[1][ki] = m00 * a01 + m01 * a11;
                t[2][ki] = m10 * a00 + m11 * a10;
                t[3][ki] = m10 * a01 + m11 * a11;
            }
        }

        bool nz[4];
        for (size_t e = 0; e < 4; ++e)
            nz[e] = toSpectrum (t[e], scratch[e], e == 0 || e == 3);

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            for (size_t e = 0; e < 4; ++e)
            {
                std::copy (scratch[e].begin(), scratch[e].begin() + 2 * bins, shared[e].begin());
                sharedNz[e] = nz[e];
            }
            version.fetch_add (1);
        }
    }

    // zero-phase magnitude -> windowed linear-phase FIR -> complex spectrum for the convolution. false = the entry is all zero.
    bool toSpectrum (const std::vector<float>& mag, std::vector<float>& dst, bool alwaysUsed)
    {
        float mx = 0.0f;
        for (float v : mag) mx = juce::jmax (mx, std::abs (v));
        if (! alwaysUsed && mx < 1.0e-5f) return false;

        std::fill (work.begin(), work.end(), 0.0f);
        for (int k = 0; k < bins; ++k)
            work[(size_t) (2 * k)] = mag[(size_t) k];
        fftDesign->performRealOnlyInverseTransform (work.data());          // real, even impulse response around n = 0

        std::fill (fir.begin(), fir.end(), 0.0f);
        const int half = taps / 2;
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (int j = 0; j < taps; ++j)
        {
            const int src = (j - half + size) % size;
            const float w = 0.5f + 0.5f * std::cos (twoPi * (float) (j - half) / (float) taps);       // Hann, centred on the FIR midpoint
            fir[(size_t) j] = work[(size_t) src] * w;
        }
        fftDesign->performRealOnlyForwardTransform (fir.data(), true);
        std::copy (fir.begin(), fir.begin() + 2 * bins, dst.begin());
        return true;
    }

    // ------------------------------------------------------------ state
    const int taps, hop;
    int order = 0, size = 0, bins = 0;
    Fetch fetch;
    MagFactory makeMag;

    std::unique_ptr<juce::dsp::FFT> fftAudio, fftDesign;      // one per thread: the FFT object keeps scratch memory

    // audio thread
    std::vector<float> bufA, bufB, tmpA, tmpB, olaA, olaB, inA, inB, outA, outB;
    std::vector<float> local[4];
    bool localNz[4] { true, false, false, true };
    uint32_t localVersion = 0;
    int pos = 0;

    // shared between the two threads (guarded by `lock`)
    juce::SpinLock lock;
    std::vector<float> shared[4];
    bool sharedNz[4] { true, false, false, true };
    std::atomic<uint32_t> version { 0 };

    // design thread
    std::vector<float> work, fir, scratch[4], t[4];
    std::vector<BandCache> cache;

    std::atomic<bool> active { false }, forceDesign { true };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> channels { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinearPhase)
};
}
