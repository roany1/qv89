#pragma once
#include "Util.h"
#include "../Params.h"

namespace qv
{
// Pitch section: mono/stereo chorus, mono/stereo flange, pitch detune, phase shifter.
// Mono in, stereo out, wet signal only (dry is supplied by the Mix section, as on the hardware).
class PitchFx
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxD = (int) (0.06f * fs) + 8;
        for (auto& d : line) d.setMaxSamples (maxD);
        detLine.setMaxSamples ((int) (0.12f * fs));
        reset();
        envCoef = 1.0f - std::exp (-1.0f / (0.002f * fs));
        relCoef = 1.0f - std::exp (-1.0f / (0.08f * fs));
    }
    void reset()
    {
        for (auto& d : line) d.clear();
        detLine.clear();
        fbState[0] = fbState[1] = 0; lfo.phase = 0; detPhase = 0; env = 0; armed = true;
        for (auto& s : apState) s = 0;
    }

    // speed 0..99 -> Hz (exponential, ~0.04 .. ~12 Hz)
    static float speedHz (float v) { return 0.04f * std::exp (v * 0.058f); }

    void setParams (const float* p)
    {
        mode   = (int) p[PITCH_MODE];
        wave   = (int) p[LFO_WAVE];
        speed  = p[LFO_SPEED];
        depth  = p[LFO_DEPTH];
        fbAmt  = p[PITCH_FB];
        trig   = p[TRIGGER_FLANGE] > 0.5f;
        cents  = p[DETUNE];
        lfo.setRate (fs, speedHz (speed));
    }

    inline void process (float x, float& outL, float& outR)
    {
        switch (mode)
        {
            case 0: case 1: chorus (x, outL, outR, mode == 1); break;
            case 2: case 3: flange (x, outL, outR, mode == 3); break;
            case 4: { const float y = detune (x); outL = outR = y; break; }
            default: { const float y = phaser (x); outL = outR = y; break; }
        }
    }

    // Used by config 5 ("reverb chorus"): returns a delay-modulation value in samples
    inline float chorusModSamples()
    {
        return lfo.next (wave) * depth / 99.0f * 0.0015f * fs;
    }

private:
    void chorus (float x, float& l, float& r, bool stereo)
    {
        const float m = lfo.next (wave);
        const float base = 0.012f * fs;                  // 12 ms centre delay
        const float sweep = depth / 99.0f * 0.008f * fs;  // up to +/- 8 ms
        const float fb = fbAmt / 99.0f * 0.85f;

        const float dL = base + sweep * m;
        const float yL = line[0].readHermite (dL);
        line[0].push (x + fb * yL);
        if (! stereo) { l = r = yL; return; }
        const float dR = base - sweep * m;                // opposite LFO polarity: L sharp while R flat
        const float yR = line[1].readHermite (dR);
        line[1].push (x + fb * yR);
        l = yL; r = yR;
    }

    void flange (float x, float& l, float& r, bool stereo)
    {
        if (trig)
        {
            // Envelope follower; restart LFO at top of cycle when input crosses threshold.
            const float a = std::fabs (x);
            env += (a > env ? envCoef : relCoef) * (a - env);
            const float thr = 0.0631f;                      // -24 dBFS
            if (armed && env > thr) { lfo.phase = 0.5f; armed = false; }   // phase 0.5 = triangle top
            else if (env < thr * 0.5f) armed = true;
        }
        const float m = 0.5f + 0.5f * lfo.next (0);       // flange: triangle 0..1
        const float minD = 0.00025f * fs;                    // 0.25 ms
        const float span = (0.0005f + depth / 99.0f * 0.0075f) * fs;
        const float fb = fbAmt / 99.0f * 0.95f;

        const float dL = minD + span * m;
        const float yL = line[0].readHermite (dL + 1.0f);
        line[0].push (x + fb * yL);
        if (! stereo) { l = r = yL; return; }
        const float dR = minD + span * (1.0f - m);          // one side flanges up while the other flanges down
        const float yR = line[1].readHermite (dR + 1.0f);
        line[1].push (x + fb * yR);
        l = yL; r = yR;
    }

    float detune (float x)
    {
        // Dual-tap rotating delay pitch shifter, 50 ms window, sin^2 crossfade.
        const float ratio = std::pow (2.0f, cents / 1200.0f);
        const float W = 0.05f * fs;
        detPhase += (1.0f - ratio) / W;
        detPhase -= std::floor (detPhase);
        const float p2 = detPhase + 0.5f - std::floor (detPhase + 0.5f);
        detLine.push (x);
        const float y1 = detLine.readHermite (2.0f + detPhase * W);
        const float y2 = detLine.readHermite (2.0f + p2 * W);
        const float g1 = std::sin (kPi * detPhase), g2 = std::sin (kPi * p2);
        return y1 * g1 * g1 + y2 * g2 * g2;
    }

    float phaser (float x)
    {
        const float m = 0.5f + 0.5f * lfo.next (0);
        const float octaves = 0.5f + depth / 99.0f * 5.0f;
        const float fc = clampf (180.0f * std::pow (2.0f, m * octaves), 20.0f, 0.45f * fs);
        const float t = std::tan (kPi * fc / fs);
        const float a = (t - 1.0f) / (t + 1.0f);
        float y = x;
        for (int i = 0; i < 6; ++i)
        {
            const float out = a * y + apState[i];
            apState[i] = y - a * out;
            y = out;
        }
        return y;
    }

    float fs = 44100;
    int mode = 1, wave = 0;
    float speed = 20, depth = 50, fbAmt = 0, cents = 5;
    bool trig = false, armed = true;
    float env = 0, envCoef = 0.01f, relCoef = 0.001f;
    DelayLine line[2], detLine;
    float fbState[2] {};
    float detPhase = 0;
    float apState[6] {};
    LFO lfo;
};
} // namespace qv
