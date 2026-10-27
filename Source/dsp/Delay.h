#pragma once
#include "Util.h"
#include "../Params.h"

namespace qv
{
// Mono / Stereo / Ping-pong delay. Mono in, stereo out (wet only).
// Delay-time changes glide (tape-like) instead of clicking.
class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxS = (int) (1.51f * fs) + 16;
        lineL.setMaxSamples (maxS); lineR.setMaxSamples (maxS);
        tL.setTime (fs, 60.0f); tR.setTime (fs, 60.0f);
        tL.reset (0.1f * fs); tR.reset (0.1f * fs);
        fbL.setTime (fs, 20.0f); fbR.setTime (fs, 20.0f);
        first = true;
    }
    void reset() { lineL.clear(); lineR.clear(); }

    void setParams (const float* p)
    {
        const int cfg = (int) p[CONFIG];
        mode = (int) p[DLY_MODE];
        const float lim = maxDelayMs (cfg, mode);
        const float limR = maxDelayMs (cfg, 1);
        tL.set (clampf (p[DLY_TIME], 1.0f, lim) * 0.001f * fs);
        tR.set (clampf (p[DLY_TIME_R], 1.0f, limR) * 0.001f * fs);
        fbL.set (fbGain (p[DLY_FB]));
        fbR.set (fbGain (p[DLY_FB_R]));
        if (first) { tL.reset (tL.target); tR.reset (tR.target); fbL.reset (fbL.target); fbR.reset (fbR.target); first = false; }
    }

    // 0..99 %; 99 = (almost) infinite repeat as on the hardware chart
    static float fbGain (float v) { return v >= 99.0f ? 0.997f : v / 100.0f; }

    inline void process (float x, float& outL, float& outR)
    {
        const float dl = std::max (2.0f, tL.next());
        const float dr = std::max (2.0f, tR.next());
        const float gl = fbL.next(), gr = fbR.next();
        switch (mode)
        {
            case 0: // mono
            {
                const float y = lineL.readHermite (dl);
                lineL.push (x + gl * y);
                outL = outR = y; break;
            }
            case 1: // stereo: two independent delays
            {
                const float yl = lineL.readHermite (dl);
                const float yr = lineR.readHermite (dr);
                lineL.push (x + gl * yl);
                lineR.push (x + gr * yr);
                outL = yl; outR = yr; break;
            }
            default: // ping-pong: L -> R -> L ...
            {
                const float yl = lineL.readHermite (dl);
                const float yr = lineR.readHermite (dl);
                lineL.push (x + gl * yr);
                lineR.push (yl);
                outL = yl; outR = yr; break;
            }
        }
    }

private:
    float fs = 44100;
    int mode = 1;
    bool first = true;
    DelayLine lineL, lineR;
    Smoother tL, tR, fbL, fbR;
};
} // namespace qv
