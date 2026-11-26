#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LinearPhase.h"
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace EQ
{
constexpr int numBands = 24;
constexpr int maxStages = 12;      // resonance + up to 8 sections for a 96 dB/oct Butterworth (one more while a pole is sliding in); 1 + 10 for the ladder under 6 dB/oct
constexpr int fftMax = 1 << 15;
constexpr int fftHop = 2048;
inline constexpr float rangeDbs[6] = { 3.0f, 6.0f, 12.0f, 18.0f, 22.0f, 30.0f };

enum Type { Peak = 0, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass, TiltShelf, numTypes };
enum Channel { Stereo = 0, Left, Right, Mid, Side };
enum LfoShape { LfoOff = 0, LfoSine, LfoTri, LfoSquare, LfoSawUp, LfoSawDown, LfoRandom, LfoDrift, numLfoShapes };

// Drift: a smooth wander that never repeats. One random value per cycle (a hash of the cycle counter), joined by a Catmull-Rom curve:
// no flat spots and no jumps, unlike Random. Scaled by 0.8 because the spline overshoots its points by up to 25 %.
inline float driftPoint (uint32_t k)
{
    k ^= k >> 16; k *= 0x7feb352du; k ^= k >> 15; k *= 0x846ca68bu; k ^= k >> 16;
    return (float) (k >> 8) / 8388608.0f - 1.0f;
}
inline float driftAt (uint32_t k, float t)        // cycle k, position t (0..1) inside it
{
    const float a = driftPoint (k - 1u), b = driftPoint (k), c = driftPoint (k + 1u), d = driftPoint (k + 2u);
    const float v = 0.5f * (2.0f * b + (c - a) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t * t + (3.0f * b - a - 3.0f * c + d) * t * t * t);
    return 0.8f * v;
}

inline const juce::StringArray& typeNames()
{
    static const juce::StringArray s { "Peak", "Low Shelf", "High Shelf", "High Pass", "Low Pass",
                                       "Notch", "Band Pass", "Tilt" };
    return s;
}
inline const juce::StringArray& phaseNames()
{
    static const juce::StringArray s { "Zero Latency", "Linear Low", "Linear Medium", "Linear High" };
    return s;
}
inline const juce::StringArray& channelNames()
{
    static const juce::StringArray s { "Stereo", "Left", "Right", "Mid", "Side" };
    return s;
}
// Slope in dB per octave of Low / High Pass (and of a steepened bell): fixed steps, no values in between
inline constexpr float slopeSteps[] = { 3.0f, 6.0f, 12.0f, 18.0f, 24.0f, 30.0f, 36.0f, 42.0f, 48.0f, 54.0f, 60.0f, 66.0f, 72.0f, 78.0f, 84.0f, 90.0f, 96.0f };
inline constexpr int numSlopeSteps = (int) (sizeof (slopeSteps) / sizeof (slopeSteps[0]));
inline int slopeStepIndex (double v)      // the step nearest to v
{
    int best = 0;
    for (int i = 1; i < numSlopeSteps; ++i)
        if (std::abs ((double) slopeSteps[i] - v) < std::abs ((double) slopeSteps[best] - v)) best = i;
    return best;
}
// the steps from minV up, spread evenly over the control so that every step is equally wide
template <class T>
inline juce::NormalisableRange<T> slopeRange (double minV)
{
    const int first = slopeStepIndex (minV), n = numSlopeSteps - 1 - first;
    return juce::NormalisableRange<T> ((T) slopeSteps[first], (T) slopeSteps[numSlopeSteps - 1],
        [first, n] (T, T, T p) { return (T) slopeSteps[first + juce::jlimit (0, n, (int) std::lround ((double) p * n))]; },
        [first, n] (T, T, T v) { return (T) juce::jlimit (0.0, 1.0, (double) (slopeStepIndex ((double) v) - first) / (double) n); },
        [first] (T, T, T v) { return (T) slopeSteps[juce::jmax (first, slopeStepIndex ((double) v))]; });
}
inline juce::String fmtSlope (float v)
{
    return juce::String ((int) std::lround (v)) + " dB/oct";
}
inline const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray s { "Off", "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Random", "Drift" };
    return s;
}
inline const juce::StringArray& syncNames()
{
    static const juce::StringArray s { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T" };
    return s;
}
inline const juce::StringArray& detectorNames()
{
    static const juce::StringArray s { "Band", "Full", "Sidechain" };   // Band: the band's own range of the signal. Full: the whole signal. Sidechain: the external input
    return s;
}
inline const juce::StringArray& splitNames()
{
    static const juce::StringArray s { "Off", "Left | Right", "Mid | Side" };   // linked pair: 2nd side = mirrored (inverted) gain
    return s;
}

inline juce::String id (int band, const juce::String& param) { return "b" + juce::String (band) + "_" + param; }
inline bool hasGain (int t) { return t == LowShelf || t == HighShelf || t == TiltShelf || t == Peak; }
inline bool hasSlope (int t) { return t == LowPass || t == HighPass; }      // cut filters: the slope is their main control
inline bool usesSlope (int t) { return hasSlope (t) || t == Peak; }         // the Slope control also exists on the bell (12 ... 96 dB/oct, 12 = the ordinary bell)
inline bool hasQ (int) { return true; }

// 6.867 kHz (three decimals), 440 Hz
inline juce::String fmtFreq (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 3) + " kHz" : juce::String ((int) std::lround (v)) + " Hz";
}

// Exact readout for the fields and the host: always three decimals, 62.060 Hz or 1.235 kHz
inline juce::String fmtFreqExact (float v)
{
    return v < 1000.0f ? juce::String (v, 3) + " Hz" : juce::String (v / 1000.0f, 3) + " kHz";
}

inline float parseNumber (const juce::String& t)
{
    return t.trim().replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-").replace (",", ".").getFloatValue();
}

// "D#3", "bb2", "A4" -> Hz, or -1 when the text is not a note. Same octave numbering as the note shown beside the frequency (C3 = middle C).
inline float noteToHz (const juce::String& txt)
{
    const auto s = txt.trim().toLowerCase();
    if (s.length() < 2 || s[0] < 'a' || s[0] > 'g') return -1.0f;
    static const int semi[7] = { 9, 11, 0, 2, 4, 5, 7 };           // a b c d e f g, counted from C
    int n = semi[s[0] - 'a'], i = 1;
    if (s[i] == '#') { ++n; ++i; }
    else if (s[i] == 'b' && s.length() > 2) { --n; ++i; }
    const auto oct = s.substring (i);
    if (oct.isEmpty() || ! oct.containsOnly ("-0123456789")) return -1.0f;
    const int midi = 12 * (oct.getIntValue() + 2) + n;
    return (float) (440.0 * std::pow (2.0, (midi - 69) / 12.0));
}

// "62.06", "1.2k", "1200 hz", "d#3"
inline float parseFreq (const juce::String& t)
{
    const float hz = noteToHz (t);
    if (hz > 0.0f) return hz;
    const auto s = t.trim().toLowerCase();
    const float v = parseNumber (s);
    return s.contains ("k") ? v * 1000.0f : v;
}

// Dynamics before 0.2.0 (Compress / Expand with Ratio and Knee) expressed in the Rest -> Active model.
//   Compress acts above the threshold: Rest = 0, Active = the old Gain.   Expand acts below it: Rest = the old Gain, Active = 0.
// The old curve starts knee/2 before the threshold and has used the whole Gain after run + knee/2; the new transition covers the same span.
struct OldDyn { float rest, active, thr, wid; };
inline OldDyn convertOldDyn (float gain, float thr, bool expand, float ratio, float knee)
{
    const float slope = 1.0f - 1.0f / juce::jmax (1.0f, ratio);
    const float run = slope > 0.01f ? std::abs (gain) / slope : 40.0f;       // dB of level over which the whole Gain is used
    const float wid = juce::jlimit (0.1f, 40.0f, run + knee);
    return expand ? OldDyn { gain, 0.0f, juce::jlimit (-60.0f, 0.0f, thr - run * 0.5f), wid }
                  : OldDyn { 0.0f, gain, juce::jlimit (-60.0f, 0.0f, thr + run * 0.5f), wid };
}

// ---- allocation-free biquads (same RBJ formulas JUCE uses, so GUI and audio agree)
struct BQ { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };

struct Biquad
{
    BQ c;
    double z1 = 0.0, z2 = 0.0;
    void set (const BQ& n) { c = n; }
    void reset() { z1 = z2 = 0.0; }
    inline double tick (double x)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void process (float* d, int n) { for (int i = 0; i < n; ++i) d[i] = (float) tick ((double) d[i]); }
};

struct Stages
{
    std::array<BQ, maxStages> c;
    int n = 0;
    // Low / High Pass with an odd order: the pole that is growing into a pair sits in slots mergeAt and mergeAt + 1. While its two
    // poles are real they are two first-order sections in a row (mergeKind 1); once they are a complex pair it is one biquad (2).
    // The processor converts the filter state when it flips, so the output does not jump.
    int mergeAt = -1, mergeKind = 0;
};

Stages makeStages (int type, double sr, float freq, float gainDb, float q, float slopeDb, bool level = true);   // level = false: the plain design, without the resonance trim (see levelPeak)
double magnitude (const Stages&, double freq, double sr);
BQ bandPassBQ (double sr, float f, float q);
BQ lowPassBQ (double sr, float f, float q);
BQ highPassBQ (double sr, float f, float q);

// ---- analyzer tap: audio thread pushes, GUI thread fetches (seqlock so the GUI never reads a torn frame)
struct AnalyzerTap
{
    std::array<float, fftMax> published {};
    std::atomic<uint32_t> seq { 0 };
    std::atomic<bool> ready { false };

    void reset();
    void push (const float* l, const float* r, int n);     // audio thread
    bool fetch (float* dst, int count);                    // GUI thread: newest `count` samples; false if torn

private:
    std::array<float, fftMax> ring {};
    int pos = 0, since = 0;
};

// ---- BS.1770 loudness (K-weighted) + sample peak, measured on the final output
struct Loudness
{
    std::atomic<float> momentary { -120.0f }, shortTerm { -120.0f }, integrated { -120.0f };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };       // linear, GUI resets by exchange(0)
    std::atomic<bool> resetRequest { false };

    void prepare (double sr);
    void process (const float* l, const float* r, int n);    // audio thread

private:
    void finishBlock (double power);
    void clearAll();

    Biquad s1[2], s2[2];
    double acc = 0.0;
    int accN = 0, blockLen = 4800;
    std::array<double, 30> blocks {};
    int head = 0, count = 0;
    std::array<double, 1001> hsum {};
    std::array<double, 1001> hcnt {};
};
}

class EQProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    EQProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    const juce::String getName() const override { return "Dypelv EQ"; }
    bool acceptsMidi() const override { return true; }      // MIDI learn: controllers can move band parameters
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

    static constexpr int fftMax = EQ::fftMax;

    // spectrum taps: input = "original", output = after the bands (solo does NOT touch these)
    EQ::AnalyzerTap tapIn, tapOut;
    EQ::Loudness loudness;

    // effective (LFO + dynamics applied) frequency / gain per band, for the GUI
    std::array<std::atomic<float>, EQ::numBands> effF {}, effG {}, effQ {}, effG2 {};
    // live detector level / effective threshold (dB) and LFO phases (0..1), for the band card previews
    std::array<std::atomic<float>, EQ::numBands> effLevel {}, effThr {}, effDyn {}, lfoPhX {}, lfoPhY {};   // effDyn: dB the dynamics add to the band right now
    std::atomic<uint32_t> blockCount { 0 };
    std::atomic<float> autoGainDb { 0.0f };
    std::atomic<bool> soloMono { false };      // Side is soloed but the signal is mono (left = right), so there is nothing to hear

    // ---- MIDI learn: any parameter can follow a controller (CC). Mappings are saved with the session.
    void armMidiLearn (const juce::String& paramId);
    void cancelMidiLearn() { learnParam.store (-1); }
    int midiCCFor (const juce::String& paramId) const;       // -1 = not mapped
    void clearMidi (const juce::String& paramId);
    void clearMidiBand (int band);
    std::atomic<int> learnParam { -1 }, learnTick { 0 }, lastLearnedCC { -1 };

    // phase mode: 0 = zero latency, 1..3 = linear phase (Low / Medium / High). Latency in samples for a mode:
    int phaseLatency (int mode) const { return mode <= 0 ? 0 : lin[(size_t) juce::jmin (mode, 3) - 1]->latencySamples(); }

    // editor history (message thread only): undo / redo snapshots and the A / B slots
    std::vector<std::vector<float>> undoStack, redoStack;
    std::vector<float> abSlot[2];
    int abActive = 0;

    // where the user last dropped the band card: distance from the plot's top edge and from its nearer side edge.
    // Interface state: it is saved with the session (not with presets) and survives closing / re-opening the editor.
    std::atomic<float> cardDX { -1.0f }, cardDY { 0.0f };       // cardDX < 0: never moved, the card sits in its default place
    std::atomic<bool> cardRight { true };                        // measured from the right edge of the plot (false: from the left edge)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void runEQ (juce::AudioBuffer<float>&, int numCh, int n);
    void runBand (int band, float* L, float* R, int n);
    void applySolo (juce::AudioBuffer<float>&, int numCh, int n);
    void handleAsyncUpdate() override;                   // message thread: latency changes and MIDI-driven parameter moves
    void handleMidi (const juce::MidiBuffer&);
    bool isAnimated (int b) const;                       // LFO or dynamics on this band
    void publishStatic (int b);                          // GUI values for a band that the linear-phase engine handles
    void delayDry (int numCh, int n, int delay);

    struct Ptrs
    {
        std::atomic<float> *on = nullptr, *type = nullptr, *ch = nullptr, *freq = nullptr, *gain = nullptr,
                           *q = nullptr, *slope = nullptr, *solo = nullptr, *mute = nullptr, *split = nullptr,
                           *dyn = nullptr, *dthr = nullptr, *datk = nullptr, *drel = nullptr,
                           *dmix = nullptr, *dauto = nullptr, *dgain = nullptr, *dwid = nullptr, *ddet = nullptr,
                           *dfol = nullptr, *dfreq = nullptr, *dq = nullptr,
                           *lxd = nullptr, *lyd = nullptr,
                           *lxs = nullptr, *lxr = nullptr, *lxa = nullptr, *lxm = nullptr, *lym = nullptr,
                           *lys = nullptr, *lyr = nullptr, *lya = nullptr;
    };
    std::array<Ptrs, EQ::numBands> p;
    std::atomic<float> *pBypass = nullptr, *pAnaOn = nullptr, *pDelta = nullptr, *pAutoGain = nullptr, *pOutGain = nullptr;
    float deltaMix = 0.0f, autoGain = 1.0f, trimGain = 1.0f;
    float monoPm = 0.0f, monoPs = 0.0f;        // running peaks of Mid and Side while Side is soloed
    double pIn = 0.0, pOut = 0.0;
    double hostBpm = 0.0, hostPpq = -1.0;
    bool hostPlaying = false;
    const float* scL = nullptr;
    const float* scR = nullptr;

    struct BandRT
    {
        std::array<std::array<EQ::Biquad, EQ::maxStages>, 2> flt;
        std::array<EQ::Biquad, 2> det;
        int curType = -1, n = 0, detType = -1;
        int mergeAt = -1, mergeKind = 0;
        float curSlope = -1.0f, curF = -1.0f, curG = -1.0f, curG2 = -1.0f, curQ = -1.0f, detF = -1.0f, detQ = -1.0f;
        float smSlope = 0.0f, smF = 1000.0f;       // Slope and Frequency as the filter uses them: they glide to the knob values (see runBand)
        double phX = 0.0, phY = 0.0;
        float rndX = 0.0f, rndTX = 0.0f, rndY = 0.0f, rndTY = 0.0f;
        uint32_t rngX = 12345u, rngY = 67890u;
        float env = 0.0f, avgDb = -60.0f;
    };
    std::array<BandRT, EQ::numBands> rts;

    std::array<EQ::Biquad, 2> soloFilt;
    float soloF = -1.0f, soloQ = -1.0f;
    int soloMode = -1;

    std::vector<float> mBuf, sBuf;
    juce::AudioBuffer<float> dry;
    float bypassMix = 0.0f;
    double sampleRate = 44100.0;

    static constexpr int maxMidiSlots = 64;
    struct MidiSlot { std::atomic<int> cc { -1 }, param { -1 }; std::atomic<float> pending { -1.0f }; };
    std::array<MidiSlot, maxMidiSlots> midiSlots;
    std::vector<juce::RangedAudioParameter*> allParams;

    std::atomic<float>* pPhase = nullptr;
    std::atomic<int> phaseActive { 0 }, numChCfg { 2 };
    std::array<std::vector<float>, 2> dryRing;           // delays the dry signal by the linear-phase latency
    int dryPos = 0;
    std::array<std::unique_ptr<EQ::LinearPhase>, 3> lin; // declared last: its design threads stop before anything they read is destroyed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQProcessor)
};
