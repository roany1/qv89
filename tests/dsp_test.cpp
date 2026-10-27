// Offline DSP test: renders impulses/tones through every configuration and mode,
// checks stability and measures reverb decay / delay timing.
#define _USE_MATH_DEFINES
#include "../Source/dsp/Engine.h"
#include "../Source/Params.h"
#include <cstdio>
#include <chrono>
#if defined(__SSE__) || defined(_M_X64)
 #include <xmmintrin.h>
 #include <pmmintrin.h>
 #define QV_FTZ 1
#endif
using namespace qv;

static std::vector<float> baseParams()
{
    std::vector<float> p (NUM_PARAMS);
    for (int i = 0; i < NUM_PARAMS; ++i) p[i] = info (i).def;
    p[VINTAGE] = 0;
    return p;
}

struct Result { std::vector<float> L, R; bool finite = true; float peak = 0; };

static Result render (std::vector<float> p, double fs, int len, int sig /*0 impulse,1 noise burst,2 sine*/)
{
    Engine e; e.prepare (fs); e.setParams (p.data());
    Result r; r.L.resize (len); r.R.resize (len);
    std::vector<float> in (len, 0.0f);
    Rng rng;
    if (sig == 0) in[0] = 1.0f;
    else if (sig == 1) for (int i = 0; i < (int) (0.05 * fs); ++i) in[i] = 0.5f * (rng.next() * 2 - 1);
    else for (int i = 0; i < len; ++i) in[i] = 0.5f * std::sin (2 * M_PI * 440 * i / fs);
    const int B = 256;
    for (int o = 0; o < len; o += B)
    {
        const int n = std::min (B, len - o);
        e.setParams (p.data());
        e.process (in.data() + o, in.data() + o, r.L.data() + o, r.R.data() + o, n);
    }
    for (int i = 0; i < len; ++i)
    {
        if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) r.finite = false;
        r.peak = std::max (r.peak, std::max (std::fabs (r.L[i]), std::fabs (r.R[i])));
    }
    return r;
}

// Schroeder backward integration RT60 (T20 extrapolated: -5..-25 dB)
static float rt60 (const std::vector<float>& L, const std::vector<float>& R, double fs, int start)
{
    const int n = (int) L.size();
    std::vector<double> e (n + 1, 0.0);
    for (int i = n - 1; i >= start; --i) e[i] = e[i + 1] + L[i] * L[i] + R[i] * R[i];
    const double e0 = e[start];
    if (e0 <= 0) return 0;
    int i5 = -1, i25 = -1;
    for (int i = start; i < n; ++i)
    {
        const double db = 10 * std::log10 (e[i] / e0 + 1e-30);
        if (i5 < 0 && db <= -5) i5 = i;
        if (i25 < 0 && db <= -25) { i25 = i; break; }
    }
    if (i5 < 0 || i25 < 0) return -1;
    return (float) (3.0 * (i25 - i5) / fs);
}

static int firstAbove (const std::vector<float>& x, float thr, int from = 0)
{
    for (int i = from; i < (int) x.size(); ++i) if (std::fabs (x[i]) > thr) return i;
    return -1;
}

int main()
{
#ifdef QV_FTZ
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE (_MM_DENORMALS_ZERO_ON);
#endif
    const double fs = 48000;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what); if (! ok) ++failures; };

    std::printf ("== Reverb types (config 1, wet only) ==\n");
    const char* tn[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "REVERSE" };
    for (int cfg : { CFG_QUAD, CFG_3BAND_REV })
        for (int t = 0; t < 5; ++t)
            for (int dec : { 20, 50, 80 })
            {
                auto p = baseParams();
                p[CONFIG] = cfg; p[REV_TYPE] = t; p[REV_DECAY] = dec;
                p[REV_IN1] = 0; p[REV_INMIX] = -99; p[REV_PREDELAY] = 1;
                p[MIX_DIRECT] = 0; p[MIX_PITCH] = 0; p[MIX_DELAY] = 0; p[MIX_REVERB] = 99;
                auto r = render (p, fs, (int) (fs * 12), 0);
                const float t60 = rt60 (r.L, r.R, fs, 0);
                double eL = 0, eR = 0, c = 0;
                for (size_t i = 0; i < r.L.size(); ++i) { eL += r.L[i] * r.L[i]; eR += r.R[i] * r.R[i]; c += r.L[i] * r.R[i]; }
                std::printf ("cfg%d %-8s decay %2d: RT60~%6.2fs peak %.3f energy %.3f L/R corr %.2f %s\n", cfg + 1, tn[t], dec, t60, r.peak,
                             eL + eR, c / std::sqrt (eL * eR + 1e-20), r.finite ? "" : "NaN!");
                if (! r.finite || r.peak > 4.0f) { ++failures; std::printf ("   ^^^ FAIL\n"); }
            }

    std::printf ("== Low/high decay effect (hall, decay 60) ==\n");
    for (int hd : { 0, -20, -60 })
    {
        auto p = baseParams(); p[REV_TYPE] = 3; p[REV_DECAY] = 60; p[REV_HIGHDEC] = hd;
        p[REV_IN1] = 0; p[REV_INMIX] = -99; p[MIX_DIRECT] = 0; p[MIX_PITCH] = 0; p[MIX_DELAY] = 0;
        auto r = render (p, fs, (int) (fs * 8), 0);
        // crude HF energy in tail 1..2 s via first difference
        double hf = 0, all = 0;
        for (int i = (int) fs; i < (int) (2 * fs); ++i) { const float d = r.L[i] - r.L[i - 1]; hf += d * d; all += r.L[i] * r.L[i]; }
        std::printf ("  high decay %3d: HF/total ratio in 1-2 s tail = %.4f\n", hd, hf / (all + 1e-20));
    }

    std::printf ("== Gate ==\n");
    {
        auto p = baseParams(); p[REV_TYPE] = 0; p[REV_DECAY] = 80; p[GATE_ON] = 1; p[GATE_HOLD] = 10; p[GATE_REL] = 10; p[GATE_LEVEL] = 0;
        p[REV_IN1] = 0; p[REV_INMIX] = -99; p[MIX_DIRECT] = 0; p[MIX_PITCH] = 0; p[MIX_DELAY] = 0;
        auto r = render (p, fs, (int) (fs * 3), 1);
        double e1 = 0, e2 = 0;
        for (int i = 0; i < (int) (0.2 * fs); ++i) e1 += r.L[i] * r.L[i];
        for (int i = (int) (0.6 * fs); i < (int) (1.0 * fs); ++i) e2 += r.L[i] * r.L[i];
        std::printf ("  energy 0-200ms %.4f, 600-1000ms %.8f\n", e1, e2);
        check (e2 < e1 * 1e-4, "gate closes after hold+release");
    }

    std::printf ("== Delay timing ==\n");
    for (int cfg : { CFG_QUAD, CFG_GRAPHIC })
        for (int mode = 0; mode < 3; ++mode)
        {
            auto p = baseParams(); p[CONFIG] = cfg; p[DLY_MODE] = mode; p[DLY_TIME] = 300; p[DLY_TIME_R] = 150; p[DLY_FB] = 50;
            p[DLY_INMIX] = -99; p[MIX_DIRECT] = 0; p[MIX_EQ] = 0; p[MIX_PITCH] = 0; p[MIX_REVERB] = 0; p[MIX_DELAY] = 99;
            auto r = render (p, fs, (int) (fs * 2), 0);
            const int l1 = firstAbove (r.L, 0.05f), r1 = firstAbove (r.R, 0.05f);
            std::printf ("  cfg%d mode %d: first echo L %.1f ms, R %.1f ms %s\n", cfg + 1, mode, l1 * 1000.0 / fs, r1 * 1000.0 / fs, r.finite ? "" : "NaN!");
            if (! r.finite) ++failures;
        }
    {
        auto p = baseParams(); p[CONFIG] = CFG_QUAD; p[DLY_MODE] = 0; p[DLY_TIME] = 1500; p[DLY_INMIX] = -99;
        p[MIX_DIRECT] = 0; p[MIX_PITCH] = 0; p[MIX_REVERB] = 0;
        auto r = render (p, fs, (int) (fs * 2), 0);
        const int l1 = firstAbove (r.L, 0.05f);
        std::printf ("  QuadMode mono 1500ms request -> %.1f ms (limit 800)\n", l1 * 1000.0 / fs);
        check (std::abs (l1 * 1000.0 / fs - 800) < 2, "QuadMode clamps mono delay to 800 ms");
    }

    std::printf ("== Pitch modes ==\n");
    const char* pm[] = { "M CHORUS", "S CHORUS", "M FLANGE", "S FLANGE", "DETUNE", "PHASER" };
    for (int m = 0; m < 6; ++m)
    {
        auto p = baseParams(); p[PITCH_MODE] = m; p[LFO_DEPTH] = 60; p[LFO_SPEED] = 40; p[PITCH_FB] = 60; p[DETUNE] = 20;
        p[MIX_DIRECT] = 50; p[MIX_PITCH] = 50; p[MIX_DELAY] = 0; p[MIX_REVERB] = 0;
        auto r = render (p, fs, (int) (fs * 2), 2);
        double rms = 0; for (float v : r.L) rms += v * v; rms = std::sqrt (rms / r.L.size());
        std::printf ("  %-9s rms %.3f peak %.3f %s\n", pm[m], rms, r.peak, r.finite ? "" : "NaN!");
        if (! r.finite || r.peak > 2.0f) ++failures;
    }
    {
        // Detune check: +20 cents on 440 Hz -> measure zero-crossing frequency of wet only
        auto p = baseParams(); p[PITCH_MODE] = 4; p[DETUNE] = 99; p[MIX_DIRECT] = 0; p[MIX_PITCH] = 99; p[MIX_DELAY] = 0; p[MIX_REVERB] = 0;
        auto r = render (p, fs, (int) (fs * 2), 2);
        int zc = 0; for (int i = (int) fs / 2 + 1; i < (int) (1.5 * fs); ++i) if (r.L[i - 1] < 0 && r.L[i] >= 0) ++zc;
        const double expect = 440 * std::pow (2.0, 99 / 1200.0);
        std::printf ("  detune +99c: measured %d Hz, expected %.1f Hz\n", zc, expect);
        check (std::abs (zc - expect) < 4, "pitch detune ratio");
    }

    std::printf ("== Rotary ==\n");
    for (int sp = 0; sp < 2; ++sp)
    {
        auto p = baseParams(); p[CONFIG] = CFG_ROTARY; p[ROT_SPEED] = sp; p[MIX_PITCH] = 99; p[MIX_DELAY] = 0; p[MIX_REVERB] = 0;
        auto r = render (p, fs, (int) (fs * 4), 2);
        double c = 0, eL = 0, eR = 0; for (size_t i = fs; i < r.L.size(); ++i) { c += r.L[i] * r.R[i]; eL += r.L[i] * r.L[i]; eR += r.R[i] * r.R[i]; }
        std::printf ("  %s: peak %.3f L/R corr %.2f %s\n", sp ? "FAST" : "SLOW", r.peak, c / std::sqrt (eL * eR + 1e-20), r.finite ? "" : "NaN!");
        if (! r.finite) ++failures;
    }

    std::printf ("== EQ ==\n");
    {
        auto p = baseParams(); p[CONFIG] = CFG_QUAD; p[MIX_DIRECT_SEL] = 1; p[MIX_EQ] = 99; p[MIX_MASTER] = 0;
        p[EQ_MID_FREQ] = 440; p[EQ_MID_AMP] = 14; p[EQ_MID_BW] = 1;
        auto r = render (p, fs, (int) fs, 2);
        double rms = 0; for (int i = (int) fs / 2; i < (int) fs; ++i) rms += r.L[i] * r.L[i]; rms = std::sqrt (rms / (fs / 2));
        const double db = 20 * std::log10 (rms / (0.5 / std::sqrt (2.0)));
        std::printf ("  mid +14 dB @440 on 440 Hz tone: %.2f dB\n", db);
        check (std::abs (db - 14) < 0.3, "parametric mid gain");
        auto q = baseParams(); q[CONFIG] = CFG_GRAPHIC; q[MIX_EQ] = 99; q[MIX_MASTER] = 0; q[GEQ_500] = -14;
        auto r2 = render (q, fs, (int) fs, 2);
        rms = 0; for (int i = (int) fs / 2; i < (int) fs; ++i) rms += r2.L[i] * r2.L[i]; rms = std::sqrt (rms / (fs / 2));
        std::printf ("  graphic 500Hz -14 dB on 440 Hz tone: %.2f dB\n", 20 * std::log10 (rms / (0.5 / std::sqrt (2.0))));
    }

    std::printf ("== CPU (worst case configs, 48 kHz, 10 s) ==\n");
    for (int cfg = 0; cfg < 5; ++cfg)
    {
        auto p = baseParams(); p[CONFIG] = cfg; p[PITCH_MODE] = 4; p[REV_TYPE] = 3;
        Engine e; e.prepare (fs); e.setParams (p.data());
        std::vector<float> in (256), oL (256), oR (256); Rng rng;
        auto t0 = std::chrono::high_resolution_clock::now();
        const int blocks = (int) (10 * fs / 256);
        for (int b = 0; b < blocks; ++b) { for (auto& v : in) v = rng.next() - 0.5f; e.setParams (p.data()); e.process (in.data(), in.data(), oL.data(), oR.data(), 256); }
        const double sec = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        std::printf ("  cfg%d: %.2f%% of one core\n", cfg + 1, 100.0 * sec / 10.0);
    }

    std::printf ("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
