#pragma once
#include "Util.h"
#include "../Params.h"

namespace qv
{
// Reverb section: Plate / Room / Chamber / Hall / Reverse, with
// pre-delay + pre-delay mix, diffusion, density (first-reflection gap),
// separate low/high frequency decay and a noise gate on the output.
// "Type 2" (3-band EQ > Reverb config) doubles the tank to 16 lines.
// Mono in, stereo out (wet only).
class Reverb
{
public:
    static constexpr int kMaxN = 16;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        pre.setMaxSamples ((int) (0.26f * fs));
        for (int i = 0; i < 4; ++i) diff[i].init ((int) (0.01f * fs));
        for (int i = 0; i < kMaxN; ++i) tank[i].setMaxSamples ((int) (0.11f * fs));
        rev.setMaxSamples ((int) (1.3f * fs));
        gateAtk = 1.0f - std::exp (-1.0f / (0.001f * fs));
        detCoef = 1.0f - std::exp (-1.0f / (0.005f * fs));
        premix.setTime (fs, 30); predelay.setTime (fs, 80);
        lastType = -1; lastN = -1;
        reset();
    }

    void setExtraMod (float samples) { extraMod = samples; }

    void reset()
    {
        pre.clear(); rev.clear();
        for (auto& d : diff) d.d.clear();
        for (auto& t : tank) t.clear();
        for (int i = 0; i < kMaxN; ++i) { lpS[i] = hpS[i] = 0; }
        gateGain = 1; holdCount = 0; det = 0; bwState = 0; rvLp = rvHp = 0;
    }

    // chorusDepthMs: extra tank modulation (config 5 "reverb chorus"), 0 = off
    void setParams (const float* p, bool type2, float chorusSamples)
    {
        type = (int) p[REV_TYPE];
        N = type2 ? 16 : 8;
        extraMod = chorusSamples;
        const TypeDef& t = types[std::min (type, 3)];

        predelay.set (p[REV_PREDELAY] * 0.001f * fs);
        premix.set ((p[REV_PREMIX] + 99.0f) / 198.0f);

        const float dif = p[REV_DIFF] / 8.0f;
        static const float apMs[4] = { 1.31f, 2.07f, 3.71f, 5.29f };
        for (int i = 0; i < 4; ++i) { diff[i].len = std::max (1, (int) (apMs[i] * (0.6f + 0.5f * t.size) * 0.001f * fs)); diff[i].g = 0.72f * dif; }

        const float density = (type == 3) ? 6.0f : p[REV_DENSITY];
        erDelay = t.erMs * 0.001f * fs;
        gap = (8.0f - density) / 8.0f * t.gapMs * 0.001f * fs;
        erGain = 0.25f + 0.45f * (8.0f - density) / 8.0f;

        // Decay -> RT60
        const float v = p[REV_DECAY];
        rt = t.rtScale * 0.25f * std::pow (100.0f, v / 99.0f);
        const float rl = std::pow (10.0f, p[REV_LOWDEC] / 60.0f);
        const float rh = std::pow (10.0f, p[REV_HIGHDEC] / 60.0f);

        if (type != lastType || N != lastN)
        {
            for (int i = 0; i < N; ++i)
            {
                const float ms = (i < 8) ? t.lens[i] : t.lens[i - 8] * 0.873f + 0.7f;
                lens[i] = ms * 0.001f * fs;
            }
            lastType = type; lastN = N;
        }
        aLow = 1.0f - std::exp (-kTwoPi * 300.0f / fs);
        aHigh = 1.0f - std::exp (-kTwoPi * t.hfHz / fs);
        bwCoef = 1.0f - std::exp (-kTwoPi * t.bwHz / fs);
        for (int i = 0; i < N; ++i)
        {
            const float L = lens[i];
            gMid[i]  = std::min (0.9995f, std::pow (10.0f, -3.0f * L / (rt * fs)));
            gLow[i]  = std::min (0.9995f, std::pow (10.0f, -3.0f * L / (rt * rl * fs)));
            gHigh[i] = std::min (0.9995f, std::pow (10.0f, -3.0f * L / (rt * rh * fs)));
        }
        outScale = t.outGain / std::sqrt ((float) N);

        // Reverse: build tap table
        if (type == 4)
        {
            revLen = (0.05f + v * 0.012f) * fs;          // 50 ms .. ~1.24 s
            const int nTaps = 16 + (int) p[REV_DENSITY] * 4;
            if (nTaps != revTaps || std::fabs (revLen - revLenBuilt) > 1.0f)
            {
                revTaps = nTaps; revLenBuilt = revLen;
                Rng rng; float norm = 0;
                for (int k = 0; k < revTaps; ++k)
                {
                    const float pos = ((float) k + 0.5f + 0.8f * (rng.next() - 0.5f)) / (float) revTaps;
                    tapD[k] = std::max (2.0f, pos * revLen + 2.0f);   // louder taps arrive later, then the sound stops
                    const float g = std::pow (pos, 1.6f);
                    tapG[k] = g * (rng.next() < 0.5f ? -1.0f : 1.0f);
                    norm += g * g;
                }
                const float s = 0.9f / std::sqrt (std::max (1e-6f, norm));
                for (int k = 0; k < revTaps; ++k) tapG[k] *= s;
            }
            rvLpC = 1.0f - std::exp (-kTwoPi * (16000.0f * std::pow (10.0f, p[REV_HIGHDEC] / 40.0f)) / fs);
            rvHpC = 1.0f - std::exp (-kTwoPi * (20.0f * std::pow (10.0f, -p[REV_LOWDEC] / 40.0f)) / fs);
        }

        gateOn = p[GATE_ON] > 0.5f && type != 4;
        holdSamples = (int) (p[GATE_HOLD] * 0.02f * fs);
        relCoef = 1.0f - std::exp (-1.0f / ((0.005f * std::exp (p[GATE_REL] * 0.06f)) * fs));
        gatedLevel = p[GATE_LEVEL] / 99.0f;
    }

    inline void process (float x, float& outL, float& outR)
    {
        // Gate detector on reverb input
        const float a = std::fabs (x);
        det += detCoef * (a - det);
        float g = 1.0f;
        if (gateOn)
        {
            if (det > 0.1259f) holdCount = holdSamples + 1;     // -18 dBFS opens the gate
            if (holdCount > 0) { --holdCount; gateGain += gateAtk * (1.0f - gateGain); }
            else gateGain += relCoef * (gatedLevel - gateGain);
            g = gateGain;
        }

        pre.push (x);
        const float pd = std::max (0.0f, predelay.next());
        const float m = premix.next();
        auto sig = [&] (float extra)
        {
            const float d0 = 1.0f + extra;
            return (1.0f - m) * pre.readLinear (d0) + m * pre.readLinear (d0 + pd);
        };

        if (type == 4) { reverse (sig (0.0f), outL, outR); return; }

        const float erL = sig (erDelay) * erGain;
        const float erR = sig (erDelay * 1.27f + 3.0f) * erGain;
        float tin = sig (erDelay + gap);
        bwState += bwCoef * (tin - bwState);
        tin = bwState;
        for (int i = 0; i < 4; ++i) tin = diff[i].process (tin);

        // Tank
        float s[kMaxN];
        lfoPh += 0.37f / fs; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        for (int i = 0; i < N; ++i)
        {
            float d = lens[i];
            if (i < 4)
            {
                const float ph = kTwoPi * (lfoPh + 0.25f * (float) i);
                d += (0.00012f * fs) * std::sin (ph) + extraMod * ((i & 1) ? -1.0f : 1.0f);
                s[i] = tank[i].readHermite (d);
            }
            else s[i] = tank[i].read ((int) d);
            // 3-band decay filter
            lpS[i] += aLow * (s[i] - lpS[i]);
            hpS[i] += aHigh * (s[i] - hpS[i]);
            const float lo = lpS[i], hi = s[i] - hpS[i];
            s[i] = gMid[i] * s[i] + (gLow[i] - gMid[i]) * lo + (gHigh[i] - gMid[i]) * hi;
        }
        float l = 0, r = 0;
        for (int i = 0; i < N; ++i) { l += s[i] * oL[i]; r += s[i] * oR[i]; }

        hadamard (s, N);
        const float inj = tin * 0.6f;
        for (int i = 0; i < N; ++i) tank[i].push (s[i] + inj * inSign[i]);

        outL = (l * outScale + erL) * g;
        outR = (r * outScale + erR) * g;
    }

private:
    static void hadamard (float* x, int n)
    {
        for (int h = 1; h < n; h <<= 1)
            for (int i = 0; i < n; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b; x[j + h] = a - b;
                }
        const float sc = 1.0f / std::sqrt ((float) n);
        for (int i = 0; i < n; ++i) x[i] *= sc;
    }

    void reverse (float x, float& outL, float& outR)
    {
        float t = x;
        for (int i = 0; i < 4; ++i) t = diff[i].process (t);
        rvLp += rvLpC * (t - rvLp); t = rvLp;
        rvHp += rvHpC * (t - rvHp); t = t - rvHp;
        rev.push (t);
        float l = 0, r = 0;
        for (int k = 0; k < revTaps; ++k)
        {
            const float y = rev.read ((int) tapD[k]) * tapG[k];
            if (k & 1) r += y; else l += y;
            if ((k % 3) == 0) { l += 0.4f * y; r += 0.4f * y; }
        }
        outL = l * 1.3f; outR = r * 1.3f;
    }

    struct TypeDef { float lens[8]; float erMs, gapMs, hfHz, bwHz, rtScale, size, outGain; };
    static constexpr TypeDef types[4] = {
        { { 9.1f, 10.9f, 12.7f, 14.9f, 17.3f, 19.7f, 22.6f, 25.1f }, 1.5f, 25.f, 7000.f, 13000.f, 1.00f, 0.6f, 0.63f }, // plate
        { { 11.3f, 13.7f, 16.1f, 18.9f, 21.7f, 24.1f, 27.7f, 31.3f }, 4.5f, 30.f, 3500.f, 9000.f, 0.55f, 0.8f, 0.86f }, // room
        { { 19.1f, 22.9f, 26.3f, 29.9f, 34.1f, 38.3f, 42.7f, 47.9f }, 8.5f, 40.f, 4500.f, 10000.f, 0.80f, 1.0f, 1.16f }, // chamber
        { { 33.1f, 38.9f, 44.3f, 50.1f, 56.9f, 63.1f, 70.7f, 78.1f }, 14.f, 55.f, 3000.f, 8500.f, 1.25f, 1.3f, 1.74f }, // hall
    };
    static constexpr float oL[kMaxN]     = { 1, 1, 1, 1, -1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1 };
    static constexpr float oR[kMaxN]     = { 1, -1, 1, -1, 1, -1, 1, -1, -1, -1, 1, 1, -1, -1, 1, 1 };
    static constexpr float inSign[kMaxN] = { 1, 1, -1, -1, 1, 1, -1, -1, 1, -1, 1, -1, -1, 1, -1, 1 };

    float fs = 44100;
    int type = 0, N = 8, lastType = -1, lastN = -1;
    DelayLine pre, tank[kMaxN], rev;
    Allpass diff[4];
    Smoother predelay, premix;
    float lens[kMaxN] {}, gMid[kMaxN] {}, gLow[kMaxN] {}, gHigh[kMaxN] {}, lpS[kMaxN] {}, hpS[kMaxN] {};
    float aLow = 0.04f, aHigh = 0.4f, bwCoef = 0.8f, bwState = 0;
    float erDelay = 0, gap = 0, erGain = 0.3f, rt = 2, outScale = 0.3f, extraMod = 0, lfoPh = 0;
    // gate
    bool gateOn = false; int holdSamples = 0, holdCount = 0;
    float gateGain = 1, gateAtk = 0.02f, relCoef = 0.001f, gatedLevel = 0.1f, det = 0, detCoef = 0.01f;
    // reverse
    static constexpr int kMaxTaps = 64;
    float tapD[kMaxTaps] {}, tapG[kMaxTaps] {};
    int revTaps = 0; float revLen = 0, revLenBuilt = -1, rvLp = 0, rvHp = 0, rvLpC = 0.9f, rvHpC = 0.001f;
};
} // namespace qv
