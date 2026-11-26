#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;

EQ::BQ mk (double b0, double b1, double b2, double a0, double a1, double a2)
{
    EQ::BQ r;
    r.b0 = b0 / a0; r.b1 = b1 / a0; r.b2 = b2 / a0; r.a1 = a1 / a0; r.a2 = a2 / a0;
    return r;
}
EQ::BQ mkN (double b0, double b1, double b2, double a1, double a2)
{
    EQ::BQ r;
    r.b0 = b0; r.b1 = b1; r.b2 = b2; r.a1 = a1; r.a2 = a2;
    return r;
}

double lin (double db) { return std::pow (10.0, db / 20.0); }

EQ::BQ peakBQ (double sr, double f, double q, double g)
{
    const double A = std::sqrt (std::max (0.0, g));
    const double w = 2.0 * kPi * std::max (f, 2.0) / sr;
    const double alpha = std::sin (w) / (q * 2.0), c2 = -2.0 * std::cos (w);
    return mk (1.0 + alpha * A, c2, 1.0 - alpha * A, 1.0 + alpha / A, c2, 1.0 - alpha / A);
}

EQ::BQ lowShelfBQ (double sr, double f, double q, double g)
{
    const double A = std::sqrt (std::max (0.0, g)), am1 = A - 1.0, ap1 = A + 1.0;
    const double w = 2.0 * kPi * std::max (f, 2.0) / sr, co = std::cos (w);
    const double beta = std::sin (w) * std::sqrt (A) / q, am1co = am1 * co;
    return mk (A * (ap1 - am1co + beta), A * 2.0 * (am1 - ap1 * co), A * (ap1 - am1co - beta),
               ap1 + am1co + beta, -2.0 * (am1 + ap1 * co), ap1 + am1co - beta);
}

EQ::BQ highShelfBQ (double sr, double f, double q, double g)
{
    const double A = std::sqrt (std::max (0.0, g)), am1 = A - 1.0, ap1 = A + 1.0;
    const double w = 2.0 * kPi * std::max (f, 2.0) / sr, co = std::cos (w);
    const double beta = std::sin (w) * std::sqrt (A) / q, am1co = am1 * co;
    return mk (A * (ap1 + am1co + beta), A * -2.0 * (am1 + ap1 * co), A * (ap1 + am1co - beta),
               ap1 - am1co + beta, 2.0 * (am1 - ap1 * co), ap1 - am1co - beta);
}

EQ::BQ notchBQ (double sr, double f, double q)
{
    const double n = 1.0 / std::tan (kPi * f / sr), n2 = n * n, iq = 1.0 / q, c1 = 1.0 / (1.0 + iq * n + n2);
    return mkN (c1 * (1.0 + n2), c1 * 2.0 * (1.0 - n2), c1 * (1.0 + n2), c1 * 2.0 * (1.0 - n2), c1 * (1.0 - iq * n + n2));
}

double clampF (double sr, double f) { return juce::jlimit (20.0, sr * 0.49, f); }

// ---- analog-matched low / high pass (M. Vicanek, "Matched Second Order Digital Filters").
// The bilinear transform squeezes everything towards Nyquist: a low pass at 15 kHz is 17 dB too low at 20 kHz.
// These sections keep the analog magnitude at DC, at the corner and at Nyquist, so the tail keeps its natural shape.
struct MatchedPoles { double a1, a2, A0, A1, A2, p0, p1, p2; };

MatchedPoles matchedPoles (double sr, double f, double q)
{
    const double w0 = 2.0 * kPi * f / sr, zeta = 1.0 / (2.0 * q);
    MatchedPoles m {};
    const double e = std::exp (-zeta * w0);
    m.a1 = zeta <= 1.0 ? -2.0 * e * std::cos (std::sqrt (1.0 - zeta * zeta) * w0)
                       : -2.0 * e * std::cosh (std::sqrt (zeta * zeta - 1.0) * w0);
    m.a2 = e * e;
    m.A0 = (1.0 + m.a1 + m.a2) * (1.0 + m.a1 + m.a2);
    m.A1 = (1.0 - m.a1 + m.a2) * (1.0 - m.a1 + m.a2);
    m.A2 = -4.0 * m.a2;
    m.p1 = std::sin (w0 * 0.5) * std::sin (w0 * 0.5);
    m.p0 = 1.0 - m.p1;
    m.p2 = 4.0 * m.p0 * m.p1;
    return m;
}

EQ::BQ matchedLP2 (double sr, double f, double q)
{
    const auto m = matchedPoles (sr, f, q);
    const double R1 = (m.A0 * m.p0 + m.A1 * m.p1 + m.A2 * m.p2) * q * q;
    const double B0 = m.A0, B1 = std::max (0.0, (R1 - B0 * m.p0) / std::max (1.0e-30, m.p1));
    const double b0 = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
    return mkN (b0, std::sqrt (B0) - b0, 0.0, m.a1, m.a2);
}

EQ::BQ matchedHP2 (double sr, double f, double q)
{
    const auto m = matchedPoles (sr, f, q);
    const double b0 = q * std::sqrt (std::max (0.0, m.A0 * m.p0 + m.A1 * m.p1 + m.A2 * m.p2)) / std::max (1.0e-30, 4.0 * m.p1);
    return mkN (b0, -2.0 * b0, b0, m.a1, m.a2);
}

// first order, matched at DC and Nyquist
EQ::BQ matchedLP1 (double sr, double f)
{
    const double w0 = 2.0 * kPi * f / sr, a1 = -std::exp (-w0), r = kPi / w0, M = 1.0 / std::sqrt (1.0 + r * r);
    return mkN (0.5 * ((1.0 + a1) + M * (1.0 - a1)), 0.5 * ((1.0 + a1) - M * (1.0 - a1)), 0.0, a1, 0.0);
}

EQ::BQ matchedHP1 (double sr, double f)
{
    const double w0 = 2.0 * kPi * f / sr, a1 = -std::exp (-w0), r = kPi / w0, M = r / std::sqrt (1.0 + r * r);
    const double b0 = 0.5 * M * (1.0 - a1);
    return mkN (b0, -b0, 0.0, a1, 0.0);
}

// ---- continuous-slope low / high pass.
// Slope s (dB/oct) means order n = s / 6. Between two whole orders M and M+1 the Butterworth poles are morphed: every pole pair
// slides to its place in the next order and the one extra pole slides in from infinity (closed-form path, so the -3 dB point
// stays exactly on the cut-off, the passband stays flat and the measured slope follows the control). Sections are listed from
// the highest-Q pair downwards, so a section keeps its index (and its filter state) across every whole order.
EQ::BQ identityBQ() { return mkN (1.0, 0.0, 0.0, 0.0, 0.0); }

EQ::BQ lerpBQ (const EQ::BQ& a, const EQ::BQ& b, double w)
{
    const double v = 1.0 - w;
    return mkN (v * a.b0 + w * b.b0, v * a.b1 + w * b.b1, v * a.b2 + w * b.b2, v * a.a1 + w * b.a1, v * a.a2 + w * b.a2);
}

// product of two first-order sections (1 + a z^-1) as one biquad
EQ::BQ mulFirstOrder (const EQ::BQ& x, const EQ::BQ& y)
{
    return mkN (x.b0 * y.b0, x.b0 * y.b1 + x.b1 * y.b0, x.b1 * y.b1, x.a1 + y.a1, x.a1 * y.a1);
}

// one real pole at fc * ratio (low pass) or fc / ratio (high pass); a pole that is far away is simply "no filter"
EQ::BQ firstOrderSection (bool hp, double sr, double fc, double ratio)
{
    if (hp)
    {
        const double f = fc / ratio;
        return f < 0.5 ? identityBQ() : matchedHP1 (sr, f);
    }
    return matchedLP1 (sr, fc * ratio);
}

// a pole pair with corner fc * w0 (LP) / fc / w0 (HP) and quality q. Below q = 0.5 the poles are real: two first-order sections
double butterQ (int order, int k)
{
    return 1.0 / (2.0 * std::cos (((double) k - (order % 2 == 0 ? 0.5 : 0.0)) * kPi / (double) order));
}

EQ::BQ pairSection (bool hp, double sr, double fc, double w0, double q)
{
    auto realPair = [&]
    {
        const double d = std::sqrt (std::max (0.0, 1.0 - 4.0 * q * q));
        const double pp = w0 / (2.0 * q) * (1.0 + d), pm = w0 * w0 / pp;
        return mulFirstOrder (firstOrderSection (hp, sr, fc, pm), firstOrderSection (hp, sr, fc, pp));
    };
    if (q < 0.45)
        return realPair();
    const double f = hp ? std::max (fc / w0, 1.0) : std::min (fc * w0, 0.49 * sr);
    const auto c2 = hp ? matchedHP2 (sr, f, q) : matchedLP2 (sr, f, q);
    if (q >= 0.5)
        return c2;
    const double x = (q - 0.45) / 0.05, w = x * x * (3.0 - 2.0 * x);       // short cross-fade where the poles meet
    return lerpBQ (realPair(), c2, w);
}

// ---- between flat (0 dB/oct) and a single pole (6 dB/oct).
// |H| = (1 + x^2)^(-t/2) is a "fractional" pole: flat below the corner, falling t * 6 dB/oct above it. It is built as a ladder of
// octave-spaced poles with every zero sitting t of an octave above its pole. At t = 1 each zero lands on the next pole and the
// ladder collapses into one ordinary pole; at t = 0 every zero sits on its pole and nothing is left. A pole / zero pair is one section:
// first-order low pass at fc * rp divided by one at fc * rz (high pass: fc / rp and fc / rz).
constexpr int kLadder = 10;     // 2^10 = 1024 times the corner: more than the whole audible tail of a 20 Hz low pass

EQ::BQ poleZeroSection (bool hp, double sr, double fc, double rp, double rz)
{
    if (hp)
    {
        const auto P = matchedHP1 (sr, std::max (fc / rp, 0.5)), Z = matchedHP1 (sr, std::max (fc / rz, 0.5));
        const double g = P.b0 / Z.b0;
        return mkN (g, g * Z.a1, 0.0, P.a1, 0.0);
    }
    const auto P = matchedLP1 (sr, fc * rp), Z = matchedLP1 (sr, fc * rz);
    const double i = 1.0 / Z.b0, r = Z.b1 * i;
    return mkN (P.b0 * i, (P.b0 * Z.a1 + P.b1) * i, P.b1 * Z.a1 * i, P.a1 + r, P.a1 * r);
}

// ---- Peak (bell) with a Slope: a higher-order Butterworth bell (after Orfanidis).
// Slope / 12 is the order N (12 dB/oct = the ordinary bell, one biquad). Prototype, W = distance from the centre in band widths:
//     |H|^2 = (G^2 + G W^(2N)) / (1 + G W^(2N))
// Every order has the same peak gain G and the same half-gain band edges (W = 1), so Gain and Q keep their meaning; only the flanks get
// steeper. The poles sit at radius G^(-1/2N) and the zeros at G^(1/2N) on the Butterworth angles, and the band-pass map W = q (s^2 + 1) / s
// splits every prototype pair into two biquads with equal Q and reciprocal frequencies (an odd order has one real root: one biquad).
// Between two whole orders the shares slide: a real root fades away or is born, a pair opens from nothing, the pairs move to their new
// angles. The gain at the centre is the sum of the sections' log gains, and every share moves linearly, so the centre stays at G for every
// slope. Section 0 is the real root (identity at even orders), then the pairs from the most resonant down: a section keeps its index
// (and its filter state) across the whole range.
EQ::BQ bellBQ (double k, double wz, double qz, double wp, double qp)
{
    const double k2 = k * k, cz = wz / qz * k, cp = wp / qp * k;
    return mk (k2 + cz + wz * wz, 2.0 * (wz * wz - k2), k2 - cz + wz * wz,
               k2 + cp + wp * wp, 2.0 * (wp * wp - k2), k2 - cp + wp * wp);
}

// one analog quad factor p^2 + 2 zeta r p + r^2 (zeta < 1) through the band-pass map: frequency and Q of the section above the centre
void bellPairSection (double zeta, double r, double beta, double& w, double& qq)
{
    const std::complex<double> h = 0.5 * beta * r * std::complex<double> (-zeta, std::sqrt (1.0 - zeta * zeta));
    const auto d = std::sqrt (h * h - 1.0);
    auto s = h + d;
    if (std::abs (h - d) > std::abs (s)) s = h - d;            // the root outside the unit circle
    w = std::abs (s);
    qq = w / std::max (1.0e-12, -2.0 * s.real());
}

void bellStages (EQ::Stages& st, double sr, double f, double q, double gainDb, double slopeDb)
{
    const double order = juce::jlimit (1.0, 8.0, slopeDb / 12.0);
    int M = (int) std::floor (order);
    double t = order - (double) M;
    if (M >= 8) { M = 7; t = 1.0; }

    const bool cut = gainDb < 0.0;                              // a cut is the inverse of the boost with the opposite gain
    const double ell = std::abs (gainDb) * 0.11512925464970229;   // ln G
    const double beta = 1.0 / q, k = 1.0 / std::tan (kPi * f / sr);
    auto lerp = [] (double a, double b, double x) { return a + (b - a) * x; };

    auto emit = [&] (double lambda, double wz, double qz, double wp, double qp)
    {
        if (st.n >= EQ::maxStages) return;
        if (lambda < 1.0e-12) { st.c[(size_t) st.n++] = identityBQ(); return; }
        st.c[(size_t) st.n++] = cut ? bellBQ (k, wp, qp, wz, qz) : bellBQ (k, wz, qz, wp, qp);
    };

    // the real root: alive at odd orders, fades away towards the next one, and is born on the way from an even one
    const double lamR = (M % 2 == 1) ? (1.0 - t) * ell / (2.0 * M) : t * ell / (2.0 * (M + 1));
    emit (lamR, 1.0, q * std::exp (-lamR), 1.0, q * std::exp (lamR));

    for (int j = 1; j <= 4; ++j)
    {
        const bool inM = j <= M / 2, inM1 = j <= (M + 1) / 2;
        if (! inM1) break;
        auto zeta = [&] (int order_) { return std::sin ((2.0 * j - 1.0) * kPi / (2.0 * order_)); };
        const double zj = inM ? lerp (zeta (M), zeta (M + 1), t) : zeta (M + 1);
        const double lam = inM ? lerp (ell / (2.0 * M), ell / (2.0 * (M + 1)), t) : t * ell / (2.0 * (M + 1));
        double wzU, qzU, wpU, qpU;
        bellPairSection (zj, std::exp (lam), beta, wzU, qzU);
        bellPairSection (zj, std::exp (-lam), beta, wpU, qpU);
        emit (lam, wzU, qzU, wpU, qpU);                          // above the centre
        emit (lam, 1.0 / wzU, qzU, 1.0 / wpU, qpU);              // below it (reciprocal frequency, same Q)
    }
}
}

// ================================================================ filter design
EQ::BQ EQ::bandPassBQ (double sr, float fIn, float qIn)
{
    const double f = clampF (sr, fIn), q = std::max (0.05, (double) qIn);
    const double n = 1.0 / std::tan (kPi * f / sr), n2 = n * n, iq = 1.0 / q, c1 = 1.0 / (1.0 + iq * n + n2);
    return mkN (c1 * n * iq, 0.0, -c1 * n * iq, c1 * 2.0 * (1.0 - n2), c1 * (1.0 - iq * n + n2));
}

EQ::BQ EQ::lowPassBQ (double sr, float fIn, float qIn)
{
    const double f = clampF (sr, fIn), q = std::max (0.05, (double) qIn);
    const double n = 1.0 / std::tan (kPi * f / sr), n2 = n * n, iq = 1.0 / q, c1 = 1.0 / (1.0 + iq * n + n2);
    return mkN (c1, c1 * 2.0, c1, c1 * 2.0 * (1.0 - n2), c1 * (1.0 - iq * n + n2));
}

EQ::BQ EQ::highPassBQ (double sr, float fIn, float qIn)
{
    const double f = clampF (sr, fIn), q = std::max (0.05, (double) qIn);
    const double n = std::tan (kPi * f / sr), n2 = n * n, iq = 1.0 / q, c1 = 1.0 / (1.0 + iq * n + n2);
    return mkN (c1, c1 * -2.0, c1, c1 * 2.0 * (n2 - 1.0), c1 * (1.0 - iq * n + n2));
}

// ---- the height of the resonance does not depend on the Slope.
// Q sets how high the hump on the corner of a Low / High Pass is, and as the Slope moves it has to stay there. Built from Butterworth
// sections the hump comes out a little different at every slope: it rises by a tenth of a dB as the steeper poles squeeze it, and it
// dips by up to 0.7 dB at 10 kHz where an odd order brings in its extra pole and the digital sections are not quite the analog ones.
// Seen on a plot that is a tip that bobs up and down with the Slope, with a period of 12 dB/oct. So the resonance bell (section 0) is
// trimmed until the highest point of the response is the same as the highest point of the same filter at 12 dB/oct.
namespace
{
// |H|^2 of a cascade at the angular frequency w. Real and imaginary parts are formed first and squared last: a low corner with
// steep slopes puts poles within a millionth of z = 1, and the expanded form 1 + a1^2 + ... would lose all its digits there.
double cascadeMag2 (const EQ::Stages& s, double w)
{
    const double c1 = std::cos (w), s1 = std::sin (w), c2 = 1.0 - 2.0 * s1 * s1, s2 = 2.0 * s1 * c1;
    double m = 1.0;
    for (int i = 0; i < s.n; ++i)
    {
        const auto& c = s.c[(size_t) i];
        const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = c.b1 * s1 + c.b2 * s2;
        const double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = c.a1 * s1 + c.a2 * s2;
        m *= (nr * nr + ni * ni) / std::max (1.0e-300, dr * dr + di * di);
    }
    return m;
}

double responseDb (const EQ::Stages& s, double sr, double fr)
{
    return 10.0 * std::log10 (std::max (1.0e-300, cascadeMag2 (s, 2.0 * kPi * fr / sr)));
}

// the highest point of the response next to the corner, in dB: a Low Pass peaks below its corner, a High Pass above it
double peakDb (const EQ::Stages& s, bool hp, double sr, double f, double q)
{
    const double span = std::min (0.9, 2.0 / q);                   // half the width of the search, in ln f: a few band widths
    const double lo = hp ? -0.2 * span : -span, hi = hp ? span : 0.2 * span;
    constexpr int N = 24;
    const double step = (hi - lo) / (double) N, nyq = 0.499 * sr;
    auto at = [&] (double x)                                       // response at ln (frequency / corner) = x
    {
        const double fr = f * std::exp (x);
        return fr > nyq ? -1.0e9 : responseDb (s, sr, fr);
    };
    double best = -1.0e9;
    int bi = 0;
    for (int i = 0; i <= N; ++i)
    {
        const double v = at (lo + step * (double) i);
        if (v > best) { best = v; bi = i; }
    }
    // The top can be a sharp corner (a steep slope squeezes the resonance against the wall), where a parabola is no use: golden section
    // between the neighbours of the best point
    constexpr double phi = 0.6180339887498949;
    double a = lo + step * (double) std::max (0, bi - 1), b = lo + step * (double) std::min (N, bi + 1);
    double c = b - phi * (b - a), d = a + phi * (b - a), vc = at (c), vd = at (d);
    for (int it = 0; it < 18; ++it)
    {
        if (vc > vd) { b = d; d = c; vd = vc; c = b - phi * (b - a); vc = at (c); }
        else         { a = c; c = d; vc = vd; d = a + phi * (b - a); vd = at (d); }
    }
    return std::max (best, std::max (vc, vd));
}

// s is a finished Low / High Pass of more than 12 dB/oct whose section 0 is the resonance bell (gain g0, Q qTop * sqrt (g0))
void levelPeak (EQ::Stages& s, int type, double sr, double f, double q, double qTop, double g0)
{
    const double x = juce::jlimit (0.0, 1.0, (q - 0.8) / 0.15), wq = x * x * (3.0 - 2.0 * x);     // below Q 0.8 the hump is a hair of a dB, far from the corner
    if (wq <= 0.0) return;
    const bool hp = (type == EQ::HighPass);

    struct RefPeak { int type = -1; double sr = 0.0, f = 0.0, q = 0.0, peak = 0.0; };
    thread_local RefPeak ref;                                      // the same filter at 12 dB/oct; the Slope usually moves while f and Q stay
    if (ref.type != type || ref.sr != sr || ref.f != f || ref.q != q)
    {
        const auto r = EQ::makeStages (type, sr, (float) f, 0.0f, (float) q, 12.0f, false);
        ref.type = type; ref.sr = sr; ref.f = f; ref.q = q;
        ref.peak = peakDb (r, hp, sr, f, q);
    }

    const auto keep = s.c[0];
    double g = g0;
    for (int it = 0; it < 2; ++it)                                  // the hump follows the bell's gain almost one to one: two rounds are plenty
    {
        const double err = ref.peak - peakDb (s, hp, sr, f, q);
        if (! (std::abs (err) < 20.0)) { s.c[0] = keep; return; }   // the measurement is off the scale (a corner so low that the sections are numerically at their limit): leave the design alone
        if (std::abs (err) < 0.002) break;
        g *= std::pow (10.0, juce::jlimit (-1.5, 1.5, err) / 20.0);
        s.c[0] = peakBQ (sr, f, qTop * std::sqrt (g), g);
    }
    const double trim = std::pow (g / g0, wq);                      // fades in with Q
    if (! (trim > 0.8 && trim < 1.25)) { s.c[0] = keep; return; }   // never more than about 2 dB: more than that is not a level error
    s.c[0] = peakBQ (sr, f, qTop * std::sqrt (g0 * trim), g0 * trim);
}
}

EQ::Stages EQ::makeStages (int type, double sr, float fIn, float gainDb, float qIn, float slopeDb, bool level)
{
    Stages s;
    const double f = clampF (sr, fIn), q = std::max (0.05, (double) qIn);
    auto add = [&] (const BQ& b) { if (s.n < maxStages) s.c[(size_t) s.n++] = b; };

    switch (type)
    {
        case LowShelf:  add (lowShelfBQ (sr, f, q, lin (gainDb))); break;
        case HighShelf: add (highShelfBQ (sr, f, q, lin (gainDb))); break;
        case Peak:
            if (slopeDb <= 12.0f + 1.0e-4f) add (peakBQ (sr, f, q, lin (gainDb)));
            else bellStages (s, sr, f, q, gainDb, slopeDb);
            break;
        case Notch:     add (notchBQ (sr, f, q)); break;
        case BandPass:  add (bandPassBQ (sr, (float) f, (float) q)); break;

        case TiltShelf:  // lows up by g/2, highs down by g/2, crossing at f. Low Q = broad, flat slope
            add (lowShelfBQ (sr, f, q, lin (gainDb * 0.5f)));
            add (highShelfBQ (sr, f, q, lin (-gainDb * 0.5f)));
            break;

        case LowPass:
        case HighPass:
        {
            // Section 0 is always the resonance: a peak / dip on the corner that scales the Q of the sharpest pair.
            // Below 6 dB/oct there is nothing to resonate, from 6 to 12 it fades in early and smoothly (the hump of a high Q is
            // already clearly there by 7 dB/oct, as on the reference filter), from 12 up it is the plain Q.
            const bool hp = (type == HighPass);
            const double n = juce::jlimit (0.0, 16.0, (double) slopeDb / 6.0);       // filter order, continuous
            int M = (int) std::floor (n);
            double t = n - (double) M;
            if (M >= 16) { M = 15; t = 1.0; }
            const int M1 = M + 1;
            const double qs = q / 0.70710678;
            double bellQTop = 0.0, bellG = 1.0;                                      // what section 0 was built from (levelPeak below)
            auto addBell = [&] (double qTop)
            {
                const double w = (M == 1) ? std::pow (t, 0.9) : 1.0;
                const double g = std::pow (qs, w);
                bellQTop = qTop;
                bellG = g;
                add (std::abs (g - 1.0) < 1.0e-12 ? identityBQ() : peakBQ (sr, f, qTop * std::sqrt (g), g));
            };

            if (M == 0)
            {
                add (identityBQ());                                                  // no resonance below 6 dB/oct
                const double rho = std::pow (2.0, 0.5 * (1.0 - t));                  // keeps the corner on the target curve
                const double zTop = rho * std::exp2 ((double) (kLadder - 1) + t) * std::pow (4096.0, std::pow (t, 6.0));
                add (poleZeroSection (hp, sr, f, rho, zTop));
                for (int k = 1; k < kLadder; ++k)
                    add (poleZeroSection (hp, sr, f, rho * std::exp2 ((double) k), rho * std::exp2 ((double) (k - 1) + t)));
                break;
            }

            // how fast the new pole arrives: slow for a short tail, slower still when the tail is many octaves long
            const double reach = hp ? f / 20.0 : std::min (20000.0, 0.45 * sr) / f;
            const double xr = juce::jlimit (2.0, 16.0, std::sqrt (std::max (1.0, reach)));
            const double u = std::pow (t, 1.0 + 0.55 * std::log2 (xr));

            if (M % 2 == 0)
            {
                // M pole pairs -> M1 = M + 1: the pairs move, one real pole comes in from infinity
                const int pairs = M / 2;
                addBell (std::exp ((1.0 - u) * std::log (butterQ (M, pairs)) + u * std::log (butterQ (M1, pairs))));
                for (int j = pairs; j >= 1; --j)
                {
                    const double qj = std::exp ((1.0 - u) * std::log (butterQ (M, j)) + u * std::log (butterQ (M1, j)));
                    add (pairSection (hp, sr, f, 1.0, qj));
                }
                if (u > 1.0e-9)
                    add (firstOrderSection (hp, sr, f, 1.0 / std::sqrt (std::expm1 (u * 0.6931471805599453))));
            }
            else
            {
                // one real pole + (M-1)/2 pairs -> (M+1)/2 pairs: the real pole and a pole from infinity grow into a new pair
                const int pairs = M1 / 2;
                std::array<double, 8> qj {};
                double prod = 1.0;                                                    // product of the squared Qs of the other pairs
                for (int j = 2; j <= pairs; ++j)
                {
                    qj[(size_t) j - 2] = std::exp ((1.0 - u) * std::log (butterQ (M, j - 1)) + u * std::log (butterQ (M1, j)));
                    prod *= qj[(size_t) j - 2] * qj[(size_t) j - 2];
                }
                addBell (pairs >= 2 ? qj[(size_t) pairs - 2] : 0.70710678);
                for (int j = pairs; j >= 2; --j)
                    add (pairSection (hp, sr, f, 1.0, qj[(size_t) j - 2]));
                s.mergeAt = s.n;
                s.mergeKind = 1;
                if (u < 1.0e-9)
                {
                    add (firstOrderSection (hp, sr, f, 1.0));
                    add (identityBQ());
                }
                else
                {
                    // corner of the new pair slides in along the curve that keeps |H(fc)| at -3 dB
                    const double w0 = std::pow (std::expm1 (u * 0.6931471805599453), -0.25);
                    const double k = 1.0 - 1.0 / (w0 * w0);
                    const double qm = 1.0 / (w0 * std::sqrt (std::max (1.0e-6, 2.0 * prod - k * k)));
                    if (qm < 0.45)
                    {
                        // two real poles: two separate first-order sections. A product biquad with one pole near DC (high pass)
                        // would amplify the state mismatch when the section first appears into a loud thump
                        const double d = std::sqrt (std::max (0.0, 1.0 - 4.0 * qm * qm));
                        const double pp = w0 / (2.0 * qm) * (1.0 + d), pm = w0 * w0 / pp;
                        add (firstOrderSection (hp, sr, f, pm));
                        add (firstOrderSection (hp, sr, f, pp));
                    }
                    else
                    {
                        s.mergeKind = 2;
                        add (pairSection (hp, sr, f, w0, qm));
                        add (identityBQ());                                          // keeps the section count steady
                    }
                }
            }
            if (level && slopeDb > 12.0f + 1.0e-3f && M >= 2 && q > 0.8 && std::abs (bellG - 1.0) > 1.0e-9)
                levelPeak (s, type, sr, f, q, bellQTop, bellG);
            break;
        }
        default: add (peakBQ (sr, f, q, lin (gainDb))); break;
    }
    return s;
}

double EQ::magnitude (const Stages& s, double f, double sr)
{
    const double w = 2.0 * kPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    double m = 1.0;
    for (int i = 0; i < s.n; ++i)
    {
        const auto& c = s.c[(size_t) i];
        const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
        const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
        m *= std::abs (num) / std::max (1e-30, std::abs (den));
    }
    return m;
}

// ================================================================ analyzer tap
void EQ::AnalyzerTap::reset()
{
    ring.fill (0.0f);
    published.fill (0.0f);
    pos = since = 0;
    ready.store (false);
}

void EQ::AnalyzerTap::push (const float* l, const float* r, int n)
{
    constexpr int mask = fftMax - 1;
    for (int i = 0; i < n; ++i)
    {
        float v = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];
        if (! std::isfinite (v)) v = 0.0f;
        ring[(size_t) pos] = v;
        pos = (pos + 1) & mask;
        if (++since >= fftHop)
        {
            since = 0;
            seq.fetch_add (1, std::memory_order_acq_rel);      // odd: writing
            for (int k = 0; k < fftMax; ++k)
                published[(size_t) k] = ring[(size_t) ((pos + k) & mask)];
            seq.fetch_add (1, std::memory_order_acq_rel);      // even: stable
            ready.store (true);
        }
    }
}

bool EQ::AnalyzerTap::fetch (float* dst, int count)
{
    const auto s1 = seq.load (std::memory_order_acquire);
    if (s1 & 1u) return false;
    std::copy (published.data() + (fftMax - count), published.data() + fftMax, dst);
    std::atomic_thread_fence (std::memory_order_acquire);
    return seq.load (std::memory_order_relaxed) == s1;
}

// ================================================================ loudness
namespace
{
double lufsOf (double power) { return power > 1e-12 ? juce::jmax (-120.0, -0.691 + 10.0 * std::log10 (power)) : -120.0; }
}

void EQ::Loudness::prepare (double sr)
{
    // BS.1770 K-weighting: high shelf (+4 dB @ ~1.68 kHz) then 38 Hz high-pass
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        BQ c = mkN ((Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                    2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0);
        s1[0].set (c); s1[1].set (c);
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / sr), a0 = 1.0 + K / Q + K * K;
        BQ c = mkN (1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0);
        s2[0].set (c); s2[1].set (c);
    }
    blockLen = juce::jmax (1, (int) std::lround (0.1 * sr));
    clearAll();
}

void EQ::Loudness::clearAll()
{
    for (auto& f : s1) f.reset();
    for (auto& f : s2) f.reset();
    acc = 0.0; accN = 0; head = 0; count = 0;
    blocks.fill (0.0);
    hsum.fill (0.0);
    hcnt.fill (0.0);
    momentary.store (-120.0f); shortTerm.store (-120.0f); integrated.store (-120.0f);
}

void EQ::Loudness::process (const float* l, const float* r, int n)
{
    if (resetRequest.exchange (false))
    {
        hsum.fill (0.0); hcnt.fill (0.0);
        integrated.store (-120.0f);
    }

    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const double x0 = std::isfinite (l[i]) ? (double) l[i] : 0.0;
        double z = s2[0].tick (s1[0].tick (x0));
        z *= z;
        pl = std::max (pl, (float) std::abs (x0));
        if (r != nullptr)
        {
            const double x1 = std::isfinite (r[i]) ? (double) r[i] : 0.0;
            double y = s2[1].tick (s1[1].tick (x1));
            z += y * y;
            pr = std::max (pr, (float) std::abs (x1));
        }
        acc += z;
        if (++accN >= blockLen)
        {
            finishBlock (acc / (double) accN);
            acc = 0.0; accN = 0;
        }
    }
    if (r == nullptr) pr = pl;
    if (pl > peakL.load (std::memory_order_relaxed)) peakL.store (pl);
    if (pr > peakR.load (std::memory_order_relaxed)) peakR.store (pr);
}

void EQ::Loudness::finishBlock (double power)
{
    blocks[(size_t) head] = power;
    head = (head + 1) % 30;
    count = juce::jmin (count + 1, 30);

    auto meanLast = [&] (int k)
    {
        k = juce::jmin (k, count);
        double s = 0.0;
        for (int i = 1; i <= k; ++i)
            s += blocks[(size_t) ((head - i + 60) % 30)];
        return s / (double) juce::jmax (1, k);
    };

    momentary.store ((float) lufsOf (meanLast (4)));
    shortTerm.store ((float) lufsOf (meanLast (30)));

    if (count >= 4)   // 400 ms gating blocks, 75 % overlap
    {
        const double p400 = meanLast (4);
        const double l = lufsOf (p400);
        if (l > -70.0)
        {
            const int bin = juce::jlimit (0, 1000, (int) std::floor ((l + 70.0) * 10.0));
            hsum[(size_t) bin] += p400;
            hcnt[(size_t) bin] += 1.0;
        }
        double sumA = 0.0, nA = 0.0;
        for (size_t i = 0; i < hsum.size(); ++i) { sumA += hsum[i]; nA += hcnt[i]; }
        if (nA > 0.0)
        {
            const double gate = lufsOf (sumA / nA) - 10.0;
            double sumR = 0.0, nR = 0.0;
            for (size_t i = 0; i < hsum.size(); ++i)
                if (-70.0 + 0.1 * (double) i >= gate) { sumR += hsum[i]; nR += hcnt[i]; }
            integrated.store (nR > 0.0 ? (float) lufsOf (sumR / nR) : -120.0f);
        }
    }
}

// ================================================================ parameters
juce::AudioProcessorValueTreeState::ParameterLayout EQProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto freqAttr = AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return EQ::fmtFreqExact (v); })
                        .withValueFromStringFunction ([] (const String& t) { return EQ::parseFreq (t); });
    auto gainAttr = AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return String (v > 0 ? "+" : "") + String (v, 2) + " dB"; })
                        .withValueFromStringFunction ([] (const String& t) { return EQ::parseNumber (t); });
    auto slopeAttr = AudioParameterFloatAttributes()
                         .withStringFromValueFunction ([] (float v, int) { return EQ::fmtSlope (v); })
                         .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto qAttr = AudioParameterFloatAttributes()
                     .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })
                     .withValueFromStringFunction ([] (const String& t) { return EQ::parseNumber (t); });
    auto dbAttr = AudioParameterFloatAttributes()
                      .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " dB"; })
                      .withValueFromStringFunction ([] (const String& t) { return EQ::parseNumber (t); });
    auto msAttr = AudioParameterFloatAttributes()
                      .withStringFromValueFunction ([] (float v, int) { return v >= 100.0f ? String ((int) std::lround (v)) + " ms" : String (v, 1) + " ms"; });
    auto pctAttr = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String ((int) std::lround (v)) + " %"; });
    auto hzAttr = AudioParameterFloatAttributes()
                      .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; });
    auto octAttr = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " oct"; });

    // display settings and mode switches are not worth automating: a jump of the grid, the analyzer or a detector mode only confuses the host's lane
    const auto fixedBool   = AudioParameterBoolAttributes().withAutomatable (false);
    const auto fixedChoice = AudioParameterChoiceAttributes().withAutomatable (false);

    for (int b = 0; b < EQ::numBands; ++b)
    {
        const auto n = "Band " + String (b + 1) + " ";
        auto pid = [&] (const char* name) { return ParameterID { EQ::id (b, name), 1 }; };
        auto pid2 = [&] (const char* name) { return ParameterID { EQ::id (b, name), 2 }; };    // added in 0.2.0

        NormalisableRange<float> fr (20.0f, 20000.0f, 0.0f);
        fr.setSkewForCentre (1000.0f);
        NormalisableRange<float> qr (0.1f, 72.0f, 0.0f);
        qr.setSkewForCentre (1.0f);
        NormalisableRange<float> atk (0.1f, 500.0f, 0.0f);   atk.setSkewForCentre (20.0f);
        NormalisableRange<float> rel (1.0f, 2000.0f, 0.0f);  rel.setSkewForCentre (150.0f);
        NormalisableRange<float> rate (0.02f, 20.0f, 0.0f);  rate.setSkewForCentre (1.0f);
        NormalisableRange<float> detF (20.0f, 20000.0f, 0.0f);  detF.setSkewForCentre (1000.0f);
        NormalisableRange<float> detQ (0.1f, 10.0f, 0.0f);      detQ.setSkewForCentre (1.0f);

        layout.add (std::make_unique<AudioParameterBool> (pid ("on"), n + "On", false));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("type"), n + "Type", EQ::typeNames(), (int) EQ::Peak));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("ch"), n + "Channel", EQ::channelNames(), 0));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("freq"), n + "Freq", fr, 1000.0f, freqAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("gain"), n + "Gain", NormalisableRange<float> (-30.0f, 30.0f, 0.001f), 0.0f, gainAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("q"), n + "Q", qr, 1.0f, qAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("slp"), n + "Slope", EQ::slopeRange<float> (3.0), 12.0f, slopeAttr));
        layout.add (std::make_unique<AudioParameterBool> (pid ("solo"), n + "Solo", false));
        layout.add (std::make_unique<AudioParameterBool> (pid ("mute"), n + "Mute", false));

        // linked pair: first side uses Gain, the second side (Right or Side) gets minus Gain (mirrored)
        layout.add (std::make_unique<AudioParameterChoice> (pid ("split"), n + "Split", EQ::splitNames(), 0));

        // dynamics
        layout.add (std::make_unique<AudioParameterBool> (pid ("dyn"), n + "Dynamic", false));
        // Dynamics: the band lives between two gains. Rest (the band's Gain) when the detector is quiet, Active when it is loud;
        // the level moves the gain from one to the other around the Threshold over a transition Width.
        layout.add (std::make_unique<AudioParameterFloat> (pid ("dthr"), n + "Dyn Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.01f), -24.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid2 ("dwid"), n + "Dyn Width", NormalisableRange<float> (0.1f, 40.0f, 0.01f), 6.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid2 ("dgain"), n + "Dyn Active Gain", NormalisableRange<float> (-30.0f, 30.0f, 0.001f), -3.0f, gainAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("datk"), n + "Dyn Attack", atk, 10.0f, msAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("drel"), n + "Dyn Release", rel, 120.0f, msAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("dmix"), n + "Dyn Mix", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
        layout.add (std::make_unique<AudioParameterBool> (pid ("dauto"), n + "Dyn Auto", false, fixedBool));
        layout.add (std::make_unique<AudioParameterChoice> (pid2 ("ddet"), n + "Dyn Detector", EQ::detectorNames(), 0, fixedChoice));
        layout.add (std::make_unique<AudioParameterBool> (pid2 ("dfol"), n + "Dyn Follow Band", true, fixedBool));
        layout.add (std::make_unique<AudioParameterFloat> (pid2 ("dfreq"), n + "Dyn Detector Freq", detF, 1000.0f, freqAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid2 ("dq"), n + "Dyn Detector Q", detQ, 1.0f, qAttr));

        // two independent LFOs: X modulates frequency, Y modulates gain (or Q on bands without gain). Shape "Off" disables one.
        layout.add (std::make_unique<AudioParameterChoice> (pid ("lxs"), n + "LFO X Shape", EQ::lfoShapeNames(), 0, fixedChoice));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("lxd"), n + "LFO X Sync", EQ::syncNames(), 0, fixedChoice));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lxr"), n + "LFO X Rate", rate, 1.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lxa"), n + "LFO X Amount", NormalisableRange<float> (0.0f, 3.0f, 0.01f), 0.5f, octAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lxm"), n + "LFO X Smooth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));   // Random shape: 0 = stepped, 100 = glides all the way
        layout.add (std::make_unique<AudioParameterChoice> (pid ("lys"), n + "LFO Y Shape", EQ::lfoShapeNames(), 0, fixedChoice));
        layout.add (std::make_unique<AudioParameterChoice> (pid ("lyd"), n + "LFO Y Sync", EQ::syncNames(), 0, fixedChoice));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lyr"), n + "LFO Y Rate", rate, 1.0f, hzAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lya"), n + "LFO Y Amount", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, dbAttr));
        layout.add (std::make_unique<AudioParameterFloat> (pid ("lym"), n + "LFO Y Smooth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "delta", 1 }, "Delta Listen", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto Gain Match", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "out_gain", 1 }, "Output Gain", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, gainAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "phase", 1 }, "Phase Mode", EQ::phaseNames(), 0, fixedChoice));   // changes the latency
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Display Range",
                                                        StringArray { "3 dB", "6 dB", "12 dB", "18 dB", "22 dB", "30 dB" }, 2, fixedChoice));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "grid", 1 }, "Show Grid", true, fixedBool));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "ana_on", 1 }, "Analyzer On", true, fixedBool));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "ana_in", 1 }, "Show Input Spectrum", true, fixedBool));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "show_lufs", 1 }, "Show LUFS Meter", false, fixedBool));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_res", 1 }, "Analyzer Resolution",
                                                        StringArray { "Low (4096)", "Medium (8192)", "High (16384)", "Max (32768)" }, 3, fixedChoice));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_range", 1 }, "Analyzer Range",
                                                        StringArray { "60 dB", "90 dB", "120 dB" }, 2, fixedChoice));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_decay", 1 }, "Analyzer Speed",
                                                        StringArray { "Slow", "Medium", "Fast" }, 1, fixedChoice));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_smooth", 1 }, "Analyzer Smoothing",
                                                        StringArray { "Off", "1/12 oct", "1/6 oct", "1/3 oct" }, 0, fixedChoice));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_tilt", 1 }, "Analyzer Tilt",
                                                        StringArray { "0 dB/oct", "3 dB/oct", "4.5 dB/oct" }, 1, fixedChoice));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ana_avg", 1 }, "Analyzer Averaging",
                                                        StringArray { "Off", "5 s", "15 s", "60 s" }, 0, fixedChoice));
    return layout;
}

EQProcessor::EQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int b = 0; b < EQ::numBands; ++b)
    {
        auto& q = p[(size_t) b];
        auto g = [&] (const char* name) { return apvts.getRawParameterValue (EQ::id (b, name)); };
        q.on = g ("on");     q.type = g ("type"); q.ch = g ("ch");     q.freq = g ("freq"); q.gain = g ("gain");
        q.q = g ("q");       q.slope = g ("slp"); q.solo = g ("solo"); q.mute = g ("mute");
        q.dyn = g ("dyn");   q.dthr = g ("dthr"); q.datk = g ("datk"); q.drel = g ("drel");
        q.dmix = g ("dmix"); q.dauto = g ("dauto");
        q.split = g ("split");
        q.dgain = g ("dgain"); q.dwid = g ("dwid"); q.ddet = g ("ddet"); q.dfol = g ("dfol"); q.dfreq = g ("dfreq"); q.dq = g ("dq");
        q.lxd = g ("lxd"); q.lyd = g ("lyd");
        q.lxs = g ("lxs");   q.lxr = g ("lxr");   q.lxa = g ("lxa");   q.lxm = g ("lxm");
        q.lys = g ("lys");   q.lyr = g ("lyr");   q.lya = g ("lya");   q.lym = g ("lym");
        effF[(size_t) b].store (q.freq->load());
        effG[(size_t) b].store (q.gain->load());
        effQ[(size_t) b].store (q.q->load());
        effG2[(size_t) b].store (-q.gain->load());
        rts[(size_t) b].rngX = 0x9e3779b9u * (uint32_t) (b + 1);
        rts[(size_t) b].rngY = 0x85ebca6bu * (uint32_t) (b + 3);
    }
    pBypass = apvts.getRawParameterValue ("bypass");
    pAnaOn  = apvts.getRawParameterValue ("ana_on");
    pDelta = apvts.getRawParameterValue ("delta");
    pAutoGain = apvts.getRawParameterValue ("autogain");
    pOutGain = apvts.getRawParameterValue ("out_gain");
    pPhase = apvts.getRawParameterValue ("phase");

    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            allParams.push_back (rp);

    // linear-phase engines (FIR length, hop): the design thread of each one reads the band parameters through `fetch`
    auto fetch = [this] (std::vector<EQ::LinBand>& out)
    {
        out.resize ((size_t) EQ::numBands);
        const bool stereo = numChCfg.load() > 1;
        for (int b = 0; b < EQ::numBands; ++b)
        {
            const auto& q = p[(size_t) b];
            auto& d = out[(size_t) b];
            const int type = (int) std::lround (q.type->load());
            d.active = q.on->load() > 0.5f && q.mute->load() < 0.5f && ! isAnimated (b);
            d.type = type;
            d.slope = q.slope->load();
            d.channel = (int) std::lround (q.ch->load());
            d.split = (EQ::hasGain (type) && stereo) ? (int) std::lround (q.split->load()) : 0;
            d.freq = q.freq->load();
            d.gain = q.gain->load();
            d.q = q.q->load();
        }
    };
    auto magnitudeOf = [] (const EQ::LinBand& b, bool second, double sr) -> EQ::LinearPhase::Magnitude
    {
        const auto st = EQ::makeStages (b.type, sr, b.freq, second ? -b.gain : b.gain, b.q, b.slope);
        return [st, sr] (double f) { return EQ::magnitude (st, f, sr); };
    };
    lin[0] = std::make_unique<EQ::LinearPhase> (2048, 256, fetch, magnitudeOf);
    lin[1] = std::make_unique<EQ::LinearPhase> (8192, 1024, fetch, magnitudeOf);
    lin[2] = std::make_unique<EQ::LinearPhase> (16384, 2048, fetch, magnitudeOf);
}

bool EQProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (out != l.getMainInputChannelSet()) return false;
    if (l.inputBuses.size() > 1)      // optional sidechain: off, mono or stereo
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void EQProcessor::prepareToPlay (double sr, int blockSize)
{
    sampleRate = sr;
    for (auto& r : rts)
    {
        const auto rx = r.rngX, ry = r.rngY;
        r = BandRT();
        r.rngX = rx; r.rngY = ry;
    }
    for (auto& f : soloFilt) f.reset();
    soloF = soloQ = -1.0f;
    soloMode = -1;

    mBuf.assign ((size_t) juce::jmax (blockSize, 64), 0.0f);
    sBuf.assign ((size_t) juce::jmax (blockSize, 64), 0.0f);
    dry.setSize (2, juce::jmax (blockSize, 64));
    bypassMix = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    autoGain = 1.0f; deltaMix = 0.0f; pIn = pOut = 0.0;
    trimGain = juce::Decibels::decibelsToGain (pOutGain->load());
    tapIn.reset();
    tapOut.reset();
    loudness.prepare (sr);

    // phase mode: engines, dry-signal delay and the latency we report to the host
    numChCfg.store (juce::jmax (1, juce::jmin (getMainBusNumInputChannels(), 2)));
    for (auto& e : lin) e->prepare (sr, numChCfg.load());
    const int mode = juce::jlimit (0, 3, (int) std::lround (pPhase->load()));
    phaseActive.store (mode);
    for (size_t i = 0; i < lin.size(); ++i) lin[i]->setActive ((int) i + 1 == mode);
    for (auto& r : dryRing) r.assign ((size_t) lin[2]->latencySamples() + 8, 0.0f);
    dryPos = 0;
    setLatencySamples (phaseLatency (mode));
}

// ================================================================ DSP
namespace
{
// one control-rate LFO step (called every `len` samples)
float lfoTick (double& ph, float& rnd, float& rndTarget, uint32_t& rng, int shape, float smooth, double advance, bool absolute, double absPhase)
{
    bool wrapped = false;
    if (absolute)     // locked to the host's beat position
    {
        const double np = absPhase - std::floor (absPhase);
        wrapped = np < ph;
        ph = np;
    }
    else
    {
        ph += advance;
        if (ph >= 1.0) { ph -= std::floor (ph); wrapped = true; }
    }
    const float x = (float) ph;
    switch (shape)
    {
        case EQ::LfoSine:    return std::sin (2.0f * (float) kPi * x);
        case EQ::LfoTri:     return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
        case EQ::LfoSquare:  return x < 0.5f ? 1.0f : -1.0f;
        case EQ::LfoSawUp:   return 2.0f * x - 1.0f;
        case EQ::LfoSawDown: return 1.0f - 2.0f * x;
        case EQ::LfoDrift:
            if (wrapped) ++rng;                 // the cycle counter: every cycle brings a new point of the wander
            return EQ::driftAt (rng, x);
        default:
            // Random: a new value every cycle. rnd = where the glide starts, rndTarget = where it goes.
            // smooth = share of the cycle spent gliding: 0 steps straight to the new value, 1 glides the whole way (no jumps at all).
            if (wrapped)
            {
                rng = rng * 1664525u + 1013904223u;
                rnd = rndTarget;
                rndTarget = (float) (rng >> 8) / 8388608.0f - 1.0f;
            }
            {
                const float t = smooth < 0.001f ? 1.0f : juce::jmin (1.0f, x / smooth);
                return rnd + (rndTarget - rnd) * (t * t * (3.0f - 2.0f * t));
            }
    }
}
}

void EQProcessor::runBand (int b, float* L, float* R, int n)
{
    auto& q = p[(size_t) b];
    auto& rt = rts[(size_t) b];

    if (! (q.on->load() > 0.5f && q.mute->load() < 0.5f))
    {
        rt.curType = -1;
        rt.n = 0;
        return;
    }

    const int type = (int) std::lround (q.type->load());
    const int ch = (int) std::lround (q.ch->load());
    const float slopeT = q.slope->load(), baseFT = q.freq->load();
    const float baseG = q.gain->load(), baseQ = q.q->load();
    const bool gainType = EQ::hasGain (type);

    // Slope and Frequency glide to their new value (one-pole, about 80 ms) and the coefficients are renewed every 32 samples while they do.
    // One big coefficient jump per audio block throws a low-frequency thump out of a high-order filter (measured: up to +40 dB over its
    // steady response); many small steps do not, and the slower the glide the smaller the thump of a sudden jump of the knob.
    // A new band or another type starts at the knob values at once.
    if (type != rt.curType) { rt.smSlope = slopeT; rt.smF = baseFT; }
    const bool slopeUsed = EQ::usesSlope (type);
    float slope = slopeUsed ? rt.smSlope : slopeT, baseF = rt.smF;
    bool glide = (slopeUsed && slope != slopeT) || baseF != baseFT;

    // two independent LFOs. X -> frequency. Y -> gain, or Q on bands that have no gain (notch, band pass, low/high pass)
    const int shX = (int) std::lround (q.lxs->load()), shY = (int) std::lround (q.lys->load());
    const int dvX = (int) std::lround (q.lxd->load()), dvY = (int) std::lround (q.lyd->load());
    const bool lfoX = shX > 0, lfoY = shY > 0;
    const bool dyn = q.dyn->load() > 0.5f && gainType;

    // linked pair: first side = Gain, second side = minus Gain (mirrored)
    const int splitMode = gainType && R != nullptr ? (int) std::lround (q.split->load()) : 0;
    const bool pair = splitMode > 0;

    const bool animated = lfoX || lfoY || dyn;
    const int chunk = animated || glide ? 32 : n;

    const float rateX = q.lxr->load(), rateY = q.lyr->load(), amtX = q.lxa->load(), amtY = q.lya->load();
    const float smX = q.lxm->load() * 0.01f, smY = q.lym->load() * 0.01f;
    const float thr = q.dthr->load(), wid = juce::jmax (0.1f, q.dwid->load());
    const float actShift = q.dgain->load() - baseG;     // Active minus Rest: how far the dynamics can move the gain
    const float mix = q.dmix->load() * 0.01f;
    const bool autoThr = q.dauto->load() > 0.5f;
    const int detMode = (int) std::lround (q.ddet->load());          // 0 Band, 1 Full, 2 Sidechain
    const bool follow = q.dfol->load() > 0.5f;
    const float detFreqP = q.dfreq->load(), detQP = q.dq->load();
    const bool useSC = dyn && detMode == 2 && scL != nullptr;
    const float atkS = std::max (0.0001f, q.datk->load() * 0.001f), relS = std::max (0.001f, q.drel->load() * 0.001f);

    // one LFO step: free-running in Hz, or locked to the host tempo / beat position
    auto lfoStep = [&] (double& ph, float& rnd, float& rndT, uint32_t& rngS, int shape, float smooth, int div, float rateHz, int offs, int len2) -> float
    {
        static const double beatsTbl[] = { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
        const double beats = beatsTbl[juce::jlimit (0, 11, div)];
        if (beats > 0.0 && hostBpm > 0.0)
        {
            const double hz = hostBpm / 60.0 / beats;
            const bool lock = hostPlaying && hostPpq >= 0.0;
            const double absPh = lock ? (hostPpq + (double) offs * hostBpm / 60.0 / sampleRate) / beats : 0.0;
            return lfoTick (ph, rnd, rndT, rngS, shape, smooth, hz * (double) len2 / sampleRate, lock, absPh);
        }
        return lfoTick (ph, rnd, rndT, rngS, shape, smooth, (double) rateHz * (double) len2 / sampleRate, false, 0.0);
    };

    float* m = mBuf.data();
    float* s = sBuf.data();
    float fEff = baseF, gEff = baseG, gB = baseG, qEff = baseQ;
    float lastLv = -120.0f, lastThr = thr, lastDyn = 0.0f;

    for (int off = 0; off < n; off += chunk)
    {
        const int len = juce::jmin (chunk, n - off);
        if (glide)
        {
            const float k = 1.0f - std::exp (-(float) len / (0.08f * (float) sampleRate));
            slope += (slopeT - slope) * k;
            baseF *= std::exp2 (std::log2 (baseFT / baseF) * k);
            if (std::abs (slopeT - slope) < 0.005f) slope = slopeT;
            if (std::abs (baseFT / baseF - 1.0f) < 2.0e-4f) baseF = baseFT;
            glide = (slopeUsed && slope != slopeT) || baseF != baseFT;
        }
        fEff = baseF;
        qEff = baseQ;
        float gLfo = baseG;
        if (lfoX) fEff = baseF * std::exp2 (amtX * lfoStep (rt.phX, rt.rndX, rt.rndTX, rt.rngX, shX, smX, dvX, rateX, off, len));
        if (lfoY)
        {
            const float v = lfoStep (rt.phY, rt.rndY, rt.rndTY, rt.rngY, shY, smY, dvY, rateY, off, len);
            if (gainType) gLfo = baseG + amtY * v;
            else          qEff = juce::jlimit (0.1f, 72.0f, baseQ * std::pow (10.0f, amtY * v / 20.0f));   // amount in dB of Q
        }
        fEff = juce::jlimit (20.0f, 20000.0f, fEff);

        gEff = gLfo;
        if (dyn)
        {
            const float lv = juce::Decibels::gainToDecibels (rt.env, -120.0f);
            rt.avgDb += (lv - rt.avgDb) * (1.0f - std::exp (-(float) len / (1.5f * (float) sampleRate)));
            lastLv = lv;
            lastThr = autoThr ? rt.avgDb + 5.0f : thr;
            // Rest -> Active: a smooth step centred on the threshold, Width dB across
            const float t = juce::jlimit (0.0f, 1.0f, (lv - (lastThr - wid * 0.5f)) / wid);
            lastDyn = actShift * (t * t * (3.0f - 2.0f * t));
            gEff = gLfo + lastDyn;

            const float dF = follow ? fEff : juce::jlimit (20.0f, 20000.0f, detFreqP);
            const float dQ = follow ? baseQ : detQP;
            if (dF != rt.detF || dQ != rt.detQ || type != rt.detType)
            {
                rt.detF = dF; rt.detQ = dQ; rt.detType = type;
                EQ::BQ d;
                if (type == EQ::LowShelf)       d = EQ::lowPassBQ (sampleRate, dF, 0.7071f);
                else if (type == EQ::HighShelf) d = EQ::highPassBQ (sampleRate, dF, 0.7071f);
                else if (type == EQ::Peak)      d = EQ::bandPassBQ (sampleRate, dF, juce::jmax (0.3f, dQ));
                else                            d = EQ::bandPassBQ (sampleRate, dF, 0.5f);
                rt.det[0].set (d); rt.det[1].set (d);
            }
        }

        gB = pair ? -gEff : gEff;

        if (type != rt.curType || slope != rt.curSlope || fEff != rt.curF || gEff != rt.curG || gB != rt.curG2 || qEff != rt.curQ)
        {
            const bool typeChanged = type != rt.curType;
            const int oldN = rt.n;
            const auto stA = EQ::makeStages (type, sampleRate, fEff, gEff, qEff, slope);
            const auto stB = pair ? EQ::makeStages (type, sampleRate, fEff, gB, qEff, slope) : stA;

            // the growing pole of an odd-order slope changes from two first-order sections to one biquad (or back): carry the state over
            if (! typeChanged && stA.mergeAt >= 0 && stA.mergeAt == rt.mergeAt && stA.mergeKind != rt.mergeKind
                && stA.mergeAt + 1 < oldN && stA.mergeAt + 1 < stA.n)
            {
                const auto ia = (size_t) stA.mergeAt, ib = ia + 1;
                for (size_t ch = 0; ch < 2; ++ch)
                {
                    auto& fa = rt.flt[ch][ia];
                    auto& fb = rt.flt[ch][ib];
                    if (rt.mergeKind == 1)               // A then B  ->  one biquad
                    {
                        const double sA = fa.z1, sB = fb.z1;
                        fa.z1 = fb.c.b0 * sA + sB;
                        fa.z2 = fb.c.b1 * sA + fa.c.a1 * sB;
                        fb.reset();
                    }
                    else                                 // one biquad  ->  A then B (the new coefficients)
                    {
                        const auto& nA = (ch == 0 ? stA : stB).c[ia];
                        const auto& nB = (ch == 0 ? stA : stB).c[ib];
                        const double den = nB.b1 - nA.a1 * nB.b0;
                        const double sA = std::abs (den) > 1.0e-12 ? (fa.z2 - nA.a1 * fa.z1) / den : 0.0;
                        const double sB = fa.z1 - nB.b0 * sA;
                        fa.z1 = sA; fa.z2 = 0.0;
                        fb.z1 = sB; fb.z2 = 0.0;
                    }
                }
            }
            rt.mergeAt = stA.mergeAt; rt.mergeKind = stA.mergeKind;
            rt.n = stA.n;
            for (int i = 0; i < stA.n; ++i)
            {
                rt.flt[0][(size_t) i].set (stA.c[(size_t) i]);
                rt.flt[1][(size_t) i].set (stB.c[(size_t) i]);
                if (typeChanged || i >= oldN) { rt.flt[0][(size_t) i].reset(); rt.flt[1][(size_t) i].reset(); }   // a sliding slope keeps the running sections' state
            }
            rt.curType = type; rt.curSlope = slope; rt.curF = fEff; rt.curG = gEff; rt.curG2 = gB; rt.curQ = qEff;
        }

        float* w0 = nullptr;
        float* w1 = nullptr;
        size_t p0 = 0;
        bool ms = false;
        auto fillMS = [&]
        {
            for (int i = 0; i < len; ++i)
            {
                m[i] = 0.5f * (L[off + i] + R[off + i]);
                s[i] = 0.5f * (L[off + i] - R[off + i]);
            }
        };

        if (pair)
        {
            if (splitMode == 1) { w0 = L + off; w1 = R + off; }
            else { fillMS(); w0 = m; w1 = s; ms = true; }
        }
        else
        {
            switch (ch)
            {
                case EQ::Stereo: w0 = L + off; w1 = R != nullptr ? R + off : nullptr; break;
                case EQ::Left:   w0 = L + off; break;
                case EQ::Right:  if (R != nullptr) { w0 = R + off; p0 = 1; } break;
                default:
                    if (R == nullptr) { if (ch == EQ::Mid) w0 = L + off; }
                    else { fillMS(); w0 = ch == EQ::Mid ? m : s; ms = true; }
                    break;
            }
        }

        if (w0 != nullptr)
        {
            float t0[32], t1[32];
            const bool doMix = dyn && mix < 0.999f;
            if (dyn)
            {
                // detector input: the band's own signal, or the external sidechain
                const float* d0 = useSC ? scL + off : w0;
                const float* d1 = useSC ? (scR != nullptr ? scR + off : nullptr) : w1;
                float pk = 0.0f;
                for (int i = 0; i < len; ++i)
                    pk = juce::jmax (pk, detMode == 1 ? std::abs (d0[i]) : (float) std::abs (rt.det[p0].tick ((double) d0[i])));
                if (d1 != nullptr)
                    for (int i = 0; i < len; ++i)
                        pk = juce::jmax (pk, detMode == 1 ? std::abs (d1[i]) : (float) std::abs (rt.det[1].tick ((double) d1[i])));
                const float cf = std::exp (-(float) len / ((pk > rt.env ? atkS : relS) * (float) sampleRate));
                rt.env = cf * rt.env + (1.0f - cf) * pk;
                if (doMix)
                {
                    std::copy (w0, w0 + len, t0);
                    if (w1 != nullptr) std::copy (w1, w1 + len, t1);
                }
            }

            for (int i = 0; i < rt.n; ++i)
            {
                rt.flt[p0][(size_t) i].process (w0, len);
                if (w1 != nullptr) rt.flt[1][(size_t) i].process (w1, len);
            }

            // Safety net. A resonant filter whose frequency jumps (LFO Square / Saw on a narrow high pass, say) can pump itself up
            // chunk after chunk. Anything past +60 dBFS (or not a number) is a fault, not music: clear the band and let it start again.
            {
                bool fault = false;
                for (int i = 0; i < len; ++i) fault |= ! (std::abs (w0[i]) < 1000.0f);
                if (w1 != nullptr)
                    for (int i = 0; i < len; ++i) fault |= ! (std::abs (w1[i]) < 1000.0f);
                if (fault)
                {
                    for (auto& chain : rt.flt)
                        for (auto& f : chain) f.reset();
                    std::fill (w0, w0 + len, 0.0f);
                    if (w1 != nullptr) std::fill (w1, w1 + len, 0.0f);
                }
            }

            if (doMix)
            {
                for (int i = 0; i < len; ++i) w0[i] = t0[i] + mix * (w0[i] - t0[i]);
                if (w1 != nullptr)
                    for (int i = 0; i < len; ++i) w1[i] = t1[i] + mix * (w1[i] - t1[i]);
            }

            if (ms)
                for (int i = 0; i < len; ++i)
                {
                    L[off + i] = m[i] + s[i];
                    R[off + i] = m[i] - s[i];
                }
        }
    }

    if (slopeUsed) rt.smSlope = slope;
    rt.smF = baseF;
    effF[(size_t) b].store (fEff);
    effG[(size_t) b].store (gEff);
    effQ[(size_t) b].store (qEff);
    effG2[(size_t) b].store (gB);
    effLevel[(size_t) b].store (lastLv);
    effThr[(size_t) b].store (lastThr);
    effDyn[(size_t) b].store (lastDyn);
    lfoPhX[(size_t) b].store ((float) rt.phX);
    lfoPhY[(size_t) b].store ((float) rt.phY);
}

bool EQProcessor::isAnimated (int b) const
{
    const auto& q = p[(size_t) b];
    return q.lxs->load() > 0.5f || q.lys->load() > 0.5f
           || (q.dyn->load() > 0.5f && EQ::hasGain ((int) std::lround (q.type->load())));
}

void EQProcessor::publishStatic (int b)
{
    const auto& q = p[(size_t) b];
    effF[(size_t) b].store (q.freq->load());
    effG[(size_t) b].store (q.gain->load());
    effQ[(size_t) b].store (q.q->load());
    effG2[(size_t) b].store (-q.gain->load());
}

void EQProcessor::runEQ (juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    if ((int) mBuf.size() < n) { mBuf.resize ((size_t) n); sBuf.resize ((size_t) n); }

    const int mode = phaseActive.load();
    if (mode == 0)
    {
        for (int b = 0; b < EQ::numBands; ++b)
            runBand (b, L, R, n);
        return;
    }

    // linear phase: static bands go through the FIR engine, bands with an LFO or dynamics stay minimum-phase and run first,
    // so they see the sidechain without the engine's delay
    for (int b = 0; b < EQ::numBands; ++b)
    {
        if (isAnimated (b)) runBand (b, L, R, n);
        else                publishStatic (b);
    }
    lin[(size_t) mode - 1]->process (L, R, n);
}

// dry signal delayed by the linear-phase latency, so delta / bypass / auto gain / solo line up with the wet signal
void EQProcessor::delayDry (int numCh, int n, int delay)
{
    for (int c = 0; c < numCh; ++c)
    {
        auto& ring = dryRing[(size_t) c];
        const int len = (int) ring.size();
        if (len < 2) continue;
        const int d = juce::jlimit (0, len - 1, delay);
        float* x = dry.getWritePointer (c);
        int w = dryPos;
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) w] = x[i];
            int r = w - d;
            if (r < 0) r += len;
            x[i] = ring[(size_t) r];
            if (++w >= len) w = 0;
        }
    }
    dryPos = (dryPos + n) % juce::jmax (1, (int) dryRing[0].size());
}

// Solo is audio-only: a band-pass of the input around the soloed band, with the band's own Q as the
// solo range. It is applied AFTER the analyzer taps, so the spectrum display never changes.
// A one-channel band solos that channel: L and R each on their own side, M and S as the mono centre / side signal.
void EQProcessor::applySolo (juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    int sb = -1;
    for (int b = 0; b < EQ::numBands; ++b)
        if (p[(size_t) b].on->load() > 0.5f && p[(size_t) b].solo->load() > 0.5f) { sb = b; break; }

    if (sb < 0) { soloF = soloQ = -1.0f; soloMode = -1; soloMono.store (false); monoPm = monoPs = 0.0f; return; }

    const int type = (int) std::lround (p[(size_t) sb].type->load());
    const int split = EQ::hasGain (type) ? (int) std::lround (p[(size_t) sb].split->load()) : 0;
    const int mode = (numCh == 2 && dry.getNumChannels() >= 2 && split == 0) ? (int) std::lround (p[(size_t) sb].ch->load()) : (int) EQ::Stereo;

    const float f = effF[(size_t) sb].load();
    const float qv = juce::jmax (0.1f, effQ[(size_t) sb].load());
    if (f != soloF || qv != soloQ)
    {
        soloF = f; soloQ = qv;
        const auto c = EQ::bandPassBQ (sampleRate, f, qv);
        soloFilt[0].set (c);
        soloFilt[1].set (c);
    }
    if (mode != EQ::Mid && mode != EQ::Side) { soloMono.store (false); monoPm = monoPs = 0.0f; }
    if (mode != soloMode)
    {
        soloMode = mode;
        monoPm = monoPs = 0.0f;
        for (auto& sf : soloFilt) sf.reset();      // another signal goes through the filter now
    }

    if (mode == EQ::Left || mode == EQ::Right)
    {
        const int keep = mode == EQ::Left ? 0 : 1;
        buffer.copyFrom (keep, 0, dry, keep, 0, n);
        soloFilt[(size_t) keep].process (buffer.getWritePointer (keep), n);
        buffer.clear (1 - keep, 0, n);
    }
    else if (mode == EQ::Mid || mode == EQ::Side)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        const auto* dl = dry.getReadPointer (0);
        const auto* dr = dry.getReadPointer (1);
        const float sgn = mode == EQ::Mid ? 1.0f : -1.0f;
        float pm = 0.0f, ps = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            pm = std::max (pm, std::abs (0.5f * (dl[i] + dr[i])));
            ps = std::max (ps, std::abs (0.5f * (dl[i] - dr[i])));
            l[i] = 0.5f * (dl[i] + sgn * dr[i]);
        }
        // is there anything in Side at all? Peaks with a 0.3 s memory, so a quiet stretch does not make the notice flicker
        const float keep = (float) std::exp (-(double) n / (0.3 * sampleRate));
        monoPm = std::max (pm, monoPm * keep);
        monoPs = std::max (ps, monoPs * keep);
        soloMono.store (mode == EQ::Side && monoPm > 1.0e-3f && monoPs < 1.0e-5f);
        soloFilt[0].process (l, n);
        std::copy (l, l + n, r);
    }
    else
    {
        for (int c = 0; c < numCh; ++c)
        {
            buffer.copyFrom (c, 0, dry, c, 0, n);
            soloFilt[(size_t) c].process (buffer.getWritePointer (c), n);
        }
    }
}

void EQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (getMainBusNumInputChannels(), 2);
    if (numCh == 0 || n == 0) return;

    blockCount.fetch_add (1, std::memory_order_relaxed);
    handleMidi (midi);

    // phase mode changed: switch engines (latency changes, so the host is told from the message thread)
    {
        const int want = juce::jlimit (0, 3, (int) std::lround (pPhase->load()));
        if (want != phaseActive.load())
        {
            phaseActive.store (want);
            for (size_t i = 0; i < lin.size(); ++i) lin[i]->setActive ((int) i + 1 == want);
            if (want > 0) lin[(size_t) want - 1]->resetStream();
            for (auto& r : dryRing) std::fill (r.begin(), r.end(), 0.0f);
            dryPos = 0;
            triggerAsyncUpdate();
        }
    }

    // optional external sidechain (used by dynamic bands that have "Sidechain" on)
    scL = scR = nullptr;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1))
            if (bus->isEnabled())
            {
                auto sc = getBusBuffer (buffer, true, 1);
                if (sc.getNumChannels() > 0)
                {
                    scL = sc.getReadPointer (0);
                    scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : nullptr;
                }
            }

    // host tempo / position for the synced LFOs
    hostBpm = 0.0; hostPpq = -1.0; hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) hostPpq = *ppq;
            hostPlaying = pos->getIsPlaying();
        }

    const bool anaOn = pAnaOn->load() > 0.5f;
    auto push = [&] (EQ::AnalyzerTap& t)
    {
        if (anaOn) t.push (buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr, n);
    };
    auto meter = [&] { loudness.process (buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr, n); };

    const float target = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    if (bypassMix >= 1.0f && target >= 1.0f && phaseActive.load() == 0)
    {
        push (tapIn);
        push (tapOut);
        meter();
        return;
    }

    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < numCh; ++c)
        dry.copyFrom (c, 0, buffer, c, 0, n);
    if (const int mode = phaseActive.load(); mode > 0)
        delayDry (numCh, n, lin[(size_t) mode - 1]->latencySamples());

    push (tapIn);                       // original
    runEQ (buffer, numCh, n);

    // auto gain match: slowly follow the level difference between output and input
    {
        double sIn = 0.0, sOut = 0.0;
        for (int c = 0; c < numCh; ++c)
        {
            const float* di = dry.getReadPointer (c);
            const float* wo = buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i) { sIn += (double) di[i] * di[i]; sOut += (double) wo[i] * wo[i]; }
        }
        const double norm = 1.0 / (double) (n * numCh), a = 1.0 - std::exp (-(double) n / (1.5 * sampleRate));
        pIn += (sIn * norm - pIn) * a;
        pOut += (sOut * norm - pOut) * a;

        float tgt = 1.0f;
        if (pAutoGain->load() > 0.5f)
            tgt = (pIn > 1.0e-9 && pOut > 1.0e-9) ? juce::jlimit (0.25f, 4.0f, (float) std::sqrt (pIn / pOut)) : autoGain;
        const float prev = autoGain;
        autoGain += (tgt - autoGain) * (float) (1.0 - std::exp (-(double) n / (0.4 * sampleRate)));
        if (autoGain != 1.0f || prev != 1.0f)
            buffer.applyGainRamp (0, n, prev, autoGain);
        autoGainDb.store (juce::Decibels::gainToDecibels (autoGain));
    }

    // output gain: a plain trim after the EQ, glided over the block so turning it does not click; bypass skips it (the dry signal is mixed in below)
    {
        const float tgt = juce::Decibels::decibelsToGain (pOutGain->load());
        const float prev = trimGain;
        trimGain += (tgt - trimGain) * (float) (1.0 - std::exp (-(double) n / (0.02 * sampleRate)));
        if (std::abs (trimGain - tgt) < 1.0e-5f) trimGain = tgt;
        if (trimGain != 1.0f || prev != 1.0f)
            buffer.applyGainRamp (0, n, prev, trimGain);
    }

    if (bypassMix != 0.0f || target != 0.0f)   // 5 ms crossfade to/from bypass
    {
        const float step = (float) (1.0 / (0.005 * sampleRate));
        float* w[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
        const float* d[2] = { dry.getReadPointer (0), numCh > 1 ? dry.getReadPointer (1) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            bypassMix = target > bypassMix ? juce::jmin (target, bypassMix + step) : juce::jmax (target, bypassMix - step);
            for (int c = 0; c < numCh; ++c)
                w[c][i] += (d[c][i] - w[c][i]) * bypassMix;
        }
    }

    push (tapOut);                      // changed by the bands (not by solo / delta)

    // delta listen: output = what the EQ added or removed (ramped so toggling doesn't click)
    {
        const float dT = pDelta->load() > 0.5f ? 1.0f : 0.0f;
        if (deltaMix != 0.0f || dT != 0.0f)
        {
            const float step = (float) (1.0 / (0.005 * sampleRate));
            float* w[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
            const float* d[2] = { dry.getReadPointer (0), numCh > 1 ? dry.getReadPointer (1) : nullptr };
            for (int i = 0; i < n; ++i)
            {
                deltaMix = dT > deltaMix ? juce::jmin (dT, deltaMix + step) : juce::jmax (dT, deltaMix - step);
                for (int c = 0; c < numCh; ++c)
                    w[c][i] -= deltaMix * d[c][i];
            }
        }
    }

    applySolo (buffer, numCh, n);
    meter();
}

juce::AudioProcessorEditor* EQProcessor::createEditor() { return new EQEditor (*this); }


// Projects saved before 0.2.0 had Compress/Expand with Ratio and Knee. Translate each dynamic band to Rest / Active / Width:
//   Compress (acts above the threshold):  Rest = 0, Active = old Gain, the change runs from the threshold up to the level where the ratio has used up the Gain
//   Expand   (acts below the threshold):  Rest = old Gain, Active = 0, the same run downwards
// The transition is centred on a new threshold (EQ::convertOldDyn); Sidechain becomes a detector choice.
static void migrateDynamics (juce::ValueTree& st)
{
    if (! st.isValid() || st.getProperty ("dynModel").toString().isNotEmpty()) return;
    auto find = [&] (const juce::String& id) -> juce::ValueTree
    {
        for (int i = 0; i < st.getNumChildren(); ++i)
        {
            auto c = st.getChild (i);
            if (c.getProperty ("id").toString() == id) return c;
        }
        return {};
    };
    auto get = [&] (int b, const char* n, float def)
    {
        auto c = find (EQ::id (b, n));
        return c.isValid() ? (float) (double) c.getProperty ("value", (double) def) : def;
    };
    auto set = [&] (int b, const char* n, float v)
    {
        const auto id = EQ::id (b, n);
        auto c = find (id);
        if (! c.isValid()) { c = juce::ValueTree ("PARAM"); c.setProperty ("id", id, nullptr); st.appendChild (c, nullptr); }
        c.setProperty ("value", (double) v, nullptr);
    };
    for (int b = 0; b < EQ::numBands; ++b)
    {
        const bool hadOld = find (EQ::id (b, "dratio")).isValid() || find (EQ::id (b, "dmode")).isValid() || find (EQ::id (b, "dknee")).isValid();
        if (! hadOld) continue;
        if (get (b, "dyn", 0.0f) > 0.5f)
        {
            const auto m = EQ::convertOldDyn (get (b, "gain", 0.0f), get (b, "dthr", -24.0f), get (b, "dmode", 0.0f) > 0.5f,
                                              get (b, "dratio", 4.0f), get (b, "dknee", 6.0f));
            set (b, "gain", m.rest);  set (b, "dgain", m.active);  set (b, "dthr", m.thr);  set (b, "dwid", m.wid);
        }
        set (b, "ddet", get (b, "dsc", 0.0f) > 0.5f ? 2.0f : 0.0f);
    }
    st.setProperty ("dynModel", 2, nullptr);
}

void EQProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto map = state.getOrCreateChildWithName ("MIDIMAP", nullptr);
    map.removeAllChildren (nullptr);
    for (auto& s : midiSlots)
    {
        const int cc = s.cc.load(), pi = s.param.load();
        if (cc < 0 || pi < 0 || pi >= (int) allParams.size()) continue;
        juce::ValueTree m ("MAP");
        m.setProperty ("cc", cc, nullptr);
        m.setProperty ("id", allParams[(size_t) pi]->paramID, nullptr);
        map.appendChild (m, nullptr);
    }
    state.setProperty ("dynModel", 2, nullptr);
    auto ui = state.getOrCreateChildWithName ("UI", nullptr);
    ui.setProperty ("cardDX", cardDX.load(), nullptr);
    ui.setProperty ("cardDY", cardDY.load(), nullptr);
    ui.setProperty ("cardRight", cardRight.load() ? 1 : 0, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void EQProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto loaded = juce::ValueTree::fromXml (*xml);
            migrateDynamics (loaded);
            apvts.replaceState (loaded);

            const auto ui = apvts.state.getChildWithName ("UI");
            if (ui.isValid() && ui.hasProperty ("cardDX") && (double) ui["cardDX"] >= 0.0)
            {
                cardDX.store ((float) (double) ui["cardDX"]);
                cardDY.store ((float) (double) ui["cardDY"]);
                cardRight.store ((int) ui["cardRight"] != 0);
            }

            for (auto& s : midiSlots) { s.cc.store (-1); s.param.store (-1); s.pending.store (-1.0f); }
            const auto map = apvts.state.getChildWithName ("MIDIMAP");
            int slot = 0;
            for (int i = 0; i < map.getNumChildren() && slot < maxMidiSlots; ++i)
            {
                const auto m = map.getChild (i);
                const auto id = m["id"].toString();
                for (size_t k = 0; k < allParams.size(); ++k)
                    if (allParams[k]->paramID == id)
                    {
                        midiSlots[(size_t) slot].param.store ((int) k);
                        midiSlots[(size_t) slot].cc.store ((int) m["cc"]);
                        ++slot;
                        break;
                    }
            }
        }
}

// ================================================================ MIDI learn
void EQProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    bool any = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isController()) continue;
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;

        const int learn = learnParam.load();
        if (learn >= 0)             // the next controller that moves is assigned to the armed parameter
        {
            int slot = -1;
            for (int i = 0; i < maxMidiSlots && slot < 0; ++i)
                if (midiSlots[(size_t) i].param.load() == learn) slot = i;
            for (int i = 0; i < maxMidiSlots && slot < 0; ++i)
                if (midiSlots[(size_t) i].cc.load() < 0) slot = i;
            if (slot >= 0)
            {
                midiSlots[(size_t) slot].param.store (learn);
                midiSlots[(size_t) slot].cc.store (cc);
            }
            lastLearnedCC.store (cc);
            learnParam.store (-1);
            learnTick.fetch_add (1);
        }

        for (auto& s : midiSlots)
            if (s.cc.load() == cc) { s.pending.store (v); any = true; }
    }
    if (any) triggerAsyncUpdate();
}

void EQProcessor::handleAsyncUpdate()
{
    setLatencySamples (phaseLatency (phaseActive.load()));

    for (auto& s : midiSlots)
    {
        const float v = s.pending.exchange (-1.0f);
        const int pi = s.param.load();
        if (v < 0.0f || pi < 0 || pi >= (int) allParams.size()) continue;
        auto* prm = allParams[(size_t) pi];
        if (std::abs (prm->getValue() - v) < 1.0e-4f) continue;
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (v);
        prm->endChangeGesture();
    }
}

void EQProcessor::armMidiLearn (const juce::String& paramId)
{
    for (size_t k = 0; k < allParams.size(); ++k)
        if (allParams[k]->paramID == paramId) { learnParam.store ((int) k); return; }
}

int EQProcessor::midiCCFor (const juce::String& paramId) const
{
    for (auto& s : midiSlots)
    {
        const int pi = s.param.load();
        if (pi >= 0 && pi < (int) allParams.size() && allParams[(size_t) pi]->paramID == paramId)
            return s.cc.load();
    }
    return -1;
}

void EQProcessor::clearMidi (const juce::String& paramId)
{
    for (auto& s : midiSlots)
    {
        const int pi = s.param.load();
        if (pi >= 0 && pi < (int) allParams.size() && allParams[(size_t) pi]->paramID == paramId)
        {
            s.cc.store (-1);
            s.param.store (-1);
        }
    }
}

void EQProcessor::clearMidiBand (int band)
{
    const auto prefix = "b" + juce::String (band) + "_";
    for (auto& s : midiSlots)
    {
        const int pi = s.param.load();
        if (pi >= 0 && pi < (int) allParams.size() && allParams[(size_t) pi]->paramID.startsWith (prefix))
        {
            s.cc.store (-1);
            s.param.store (-1);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQProcessor(); }
