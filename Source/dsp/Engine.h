#pragma once
#include "Eq.h"
#include "Pitch.h"
#include "Rotary.h"
#include "Delay.h"
#include "Reverb.h"
#include <atomic>

namespace qv
{
// Complete QV89 signal processor. Feed it the (already modulated) parameter array
// once per block with setParams(), then call process().
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        eq.prepare (sampleRate); pitch.prepare (sampleRate); rotary.prepare (sampleRate);
        delay.prepare (sampleRate); reverb.prepare (sampleRate);
        for (auto* s : { &sDirect, &sEq, &sMaster, &sPitch, &sDelay, &sReverb, &sIn, &sOut, &sBypass })
            s->setTime (fs, 15.0f);
        sBypass.setTime (fs, 8.0f);
        first = true;
    }

    void reset() { pitch.reset(); delay.reset(); reverb.reset(); }

    void setParams (const float* p)
    {
        std::copy (p, p + NUM_PARAMS, par);
        cfg = std::max (0, std::min (4, (int) p[CONFIG]));
        eq.update (p);
        pitch.setParams (p);
        rotary.setParams (p);
        delay.setParams (p);
        reverb.setParams (p, cfg == CFG_3BAND_REV, 0.0f);
        revChorus = (cfg == CFG_3BAND_REV) && p[REV_CHORUS] > 0.5f;
        vintage = p[VINTAGE] > 0.5f;

        auto lv = [] (float v) { return clampf (v, 0.0f, 99.0f) / 99.0f; };
        const bool post = p[MIX_DIRECT_SEL] > 0.5f;
        float direct = 0, eqL = 0;
        switch (cfg)
        {
            case CFG_ROTARY:  direct = 0; eqL = 0; break;
            case CFG_GRAPHIC: direct = 0; eqL = lv (p[MIX_EQ]); break;
            default: direct = post ? 0 : lv (p[MIX_DIRECT]); eqL = post ? lv (p[MIX_EQ]) : 0; break;
        }
        sDirect.set (direct); sEq.set (eqL);
        sMaster.set (lv (p[MIX_MASTER]));
        sPitch.set ((cfg == CFG_QUAD || cfg == CFG_ROTARY || cfg == CFG_5BAND) ? lv (p[MIX_PITCH]) : 0.0f);
        sDelay.set (cfg != CFG_3BAND_REV ? lv (p[MIX_DELAY]) : 0.0f);
        sReverb.set ((cfg == CFG_QUAD || cfg == CFG_ROTARY || cfg == CFG_3BAND_REV) ? lv (p[MIX_REVERB]) : 0.0f);
        sIn.set (dbToGain (p[IN_GAIN]));
        sOut.set (dbToGain (p[OUT_GAIN]));
        sBypass.set (p[BYPASS] > 0.5f ? 1.0f : 0.0f);
        if (first) { for (auto* s : { &sDirect, &sEq, &sMaster, &sPitch, &sDelay, &sReverb, &sIn, &sOut, &sBypass }) s->reset (s->target); first = false; }
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        float pkIn = 0, pkOut = 0;
        const int pin = (int) par[PITCH_INPUT], din = (int) par[DLY_INPUT];
        const int rin1 = (int) par[REV_IN1], rin2 = (int) par[REV_IN2];
        const float tDly = (par[DLY_INMIX] + 99.0f) / 198.0f;
        const float tRev = (par[REV_INMIX] + 99.0f) / 198.0f;

        for (int i = 0; i < n; ++i)
        {
            const float rawL = inL[i], rawR = inR[i];
            const float gIn = sIn.next();
            float xl = rawL * gIn, xr = rawR * gIn;
            float mono = 0.5f * (xl + xr);              // the hardware sums to mono before the converter
            pkIn = std::max (pkIn, std::fabs (mono));
            if (vintage) mono = quant16 (mono);

            float el = xl, er = xr;
            if (cfg != CFG_ROTARY) eq.process (el, er);
            float eqMono = 0.5f * (el + er);
            if (vintage) eqMono = quant16 (eqMono);

            float pl = 0, pr = 0, dl = 0, dr = 0, rl = 0, rr = 0;
            const float pLvl = sPitch.next(), dLvl = sDelay.next(), rLvl = sReverb.next();
            const float master = sMaster.next(), dirL = sDirect.next(), eqLv = sEq.next();

            switch (cfg)
            {
                case CFG_QUAD:
                case CFG_5BAND:
                {
                    pitch.process (pin ? eqMono : mono, pl, pr);
                    const float pm = 0.5f * (pl + pr);
                    const float d1 = din ? eqMono : mono;
                    const float dIn = d1 + tDly * (pm - d1);
                    delay.process (dIn, dl, dr);
                    if (cfg == CFG_QUAD)
                    {
                        const float dm = 0.5f * (dl + dr);
                        const float r1 = rin1 == 0 ? mono : rin1 == 1 ? eqMono : rin1 == 2 ? pm : dIn;
                        const float r2 = rin2 ? dm : pm;
                        reverb.process (r1 + tRev * (r2 - r1), rl, rr);
                    }
                    break;
                }
                case CFG_ROTARY:
                {
                    rotary.process (mono, pl, pr);
                    const float om = 0.5f * (pl + pr);
                    const float dIn = mono + tDly * (om - mono);
                    delay.process (dIn, dl, dr);
                    const float dm = 0.5f * (dl + dr);
                    const int r1i = std::min (rin1, 2);
                    const float r1 = r1i == 0 ? mono : r1i == 1 ? om : dIn;
                    const float r2 = rin2 ? dm : om;
                    reverb.process (r1 + tRev * (r2 - r1), rl, rr);
                    break;
                }
                case CFG_GRAPHIC:
                    delay.process (din ? eqMono : mono, dl, dr);
                    break;
                default: // 3-band EQ > Reverb (type 2, optional reverb chorus)
                {
                    if (revChorus) reverb.setExtraMod (pitch.chorusModSamples());
                    else reverb.setExtraMod (0.0f);
                    reverb.process ((rin1 & 1) ? eqMono : mono, rl, rr);
                    break;
                }
            }

            float oL = dirL * xl + eqLv * el + master * (pLvl * pl + dLvl * dl + rLvl * rl);
            float oR = dirL * xr + eqLv * er + master * (pLvl * pr + dLvl * dr + rLvl * rr);
            const float gOut = sOut.next();
            oL *= gOut; oR *= gOut;
            const float b = sBypass.next();
            oL += b * (rawL - oL); oR += b * (rawR - oR);
            if (! std::isfinite (oL) || ! std::isfinite (oR)) { reset(); oL = oR = 0; }
            outL[i] = oL; outR[i] = oR;
            pkOut = std::max (pkOut, std::max (std::fabs (oL), std::fabs (oR)));
        }
        inPeak.store (pkIn);
        outPeak.store (pkOut);
    }

    std::atomic<float> inPeak { 0 }, outPeak { 0 };

private:
    static inline float quant16 (float x)
    {
        x = clampf (x, -1.0f, 32767.0f / 32768.0f);              // converter overflow clips
        return std::floor (x * 32768.0f + 0.5f) / 32768.0f;
    }

    float fs = 44100;
    float par[NUM_PARAMS] {};
    int cfg = 0;
    bool revChorus = false, vintage = true, first = true;
    StereoEq eq; PitchFx pitch; Rotary rotary; StereoDelay delay; Reverb reverb;
    Smoother sDirect, sEq, sMaster, sPitch, sDelay, sReverb, sIn, sOut, sBypass;
};
} // namespace qv
