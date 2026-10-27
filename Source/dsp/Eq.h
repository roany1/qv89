#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "Util.h"
#include "../Params.h"

namespace qv
{
// RBJ biquad, transposed direct form II
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    static double safeF (double fs, double f) { return std::min (f, fs * 0.45); }

    void peak (double fs, double f, double db, double bwOct)
    {
        f = safeF (fs, f);
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * M_PI * f / fs;
        const double sn = std::sin (w), cs = std::cos (w);
        const double alpha = sn * std::sinh (std::log (2.0) / 2.0 * bwOct * w / sn);
        setNorm (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
    }
    void lowShelf (double fs, double f, double db)
    {
        f = safeF (fs, f);
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * M_PI * f / fs;
        const double sn = std::sin (w), cs = std::cos (w), alpha = sn / 2.0 * std::sqrt (2.0), sA = 2 * std::sqrt (A) * alpha;
        setNorm (A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA),
                 (A + 1) + (A - 1) * cs + sA, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
    }
    void highShelf (double fs, double f, double db)
    {
        f = safeF (fs, f);
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * M_PI * f / fs;
        const double sn = std::sin (w), cs = std::cos (w), alpha = sn / 2.0 * std::sqrt (2.0), sA = 2 * std::sqrt (A) * alpha;
        setNorm (A * ((A + 1) + (A - 1) * cs + sA), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sA),
                 (A + 1) - (A - 1) * cs + sA, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sA);
    }
    void lowpass (double fs, double f, double q = 0.70710678)
    {
        f = safeF (fs, f);
        const double w = 2.0 * M_PI * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2 * q);
        setNorm ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }
    void highpass (double fs, double f, double q = 0.70710678)
    {
        f = safeF (fs, f);
        const double w = 2.0 * M_PI * f / fs, cs = std::cos (w), alpha = std::sin (w) / (2 * q);
        setNorm ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }
};

// Stereo EQ that morphs between 3-band parametric, 5-band parametric and 11-band graphic.
class StereoEq
{
public:
    void prepare (double sampleRate) { fs = sampleRate; for (auto& c : ch) for (auto& b : c) b.reset(); lastKey = -1; }

    // Recompute coefficients when relevant params change (called once per block).
    void update (const float* p)
    {
        const int cfg = (int) p[CONFIG];
        mode = (cfg == CFG_GRAPHIC) ? 2 : (cfg == CFG_5BAND ? 1 : 0);
        bool same = (lastKey == mode);
        for (int i = EQ_LOW_FREQ; i <= GEQ_16K && same; ++i) same = (cache[(size_t) i] == p[i]);
        if (same) return;
        lastKey = mode;
        for (int i = EQ_LOW_FREQ; i <= GEQ_16K; ++i) cache[(size_t) i] = p[i];

        std::vector<Biquad> proto;
        if (mode == 2)
        {
            static const double fc[11] = { 16, 32, 62, 126, 250, 500, 1000, 2000, 4000, 8000, 16000 };
            nBands = 0;
            for (int i = 0; i < 11; ++i)
            {
                Biquad b; b.peak (fs, fc[i], p[GEQ_16 + i], 1.0); proto.push_back (b);
            }
        }
        else
        {
            Biquad lo; lo.lowShelf (fs, p[EQ_LOW_FREQ], p[EQ_LOW_AMP]); proto.push_back (lo);
            if (mode == 1) { Biquad b; b.peak (fs, p[EQ_LOMID_FREQ], p[EQ_LOMID_AMP], p[EQ_LOMID_BW]); proto.push_back (b); }
            Biquad m; m.peak (fs, p[EQ_MID_FREQ], p[EQ_MID_AMP], p[EQ_MID_BW]); proto.push_back (m);
            if (mode == 1) { Biquad b; b.peak (fs, p[EQ_HIMID_FREQ], p[EQ_HIMID_AMP], p[EQ_HIMID_BW]); proto.push_back (b); }
            Biquad hi; hi.highShelf (fs, p[EQ_HIGH_FREQ], p[EQ_HIGH_AMP]); proto.push_back (hi);
        }
        nBands = (int) proto.size();
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < nBands; ++i)
            {
                auto& b = ch[c][(size_t) i];
                b.b0 = proto[(size_t) i].b0; b.b1 = proto[(size_t) i].b1; b.b2 = proto[(size_t) i].b2;
                b.a1 = proto[(size_t) i].a1; b.a2 = proto[(size_t) i].a2;
            }
    }

    inline void process (float& l, float& r)
    {
        for (int i = 0; i < nBands; ++i) { l = ch[0][(size_t) i].process (l); r = ch[1][(size_t) i].process (r); }
    }

private:
    double fs = 44100;
    int mode = 0, nBands = 0, lastKey = -1;
    float cache[NUM_PROGRAM_PARAMS] {};
    Biquad ch[2][11];
};
} // namespace qv
