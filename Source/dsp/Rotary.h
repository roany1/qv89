#pragma once
#include "Eq.h"

namespace qv
{
// Rotating-speaker simulator (config "ROTARY>DL>REVERB").
// Horn (treble) + drum (bass) split at 800 Hz with 4th-order Linkwitz-Riley crossover,
// each rotor with its own inertia, doppler (modulated delay) and amplitude modulation,
// picked up by two virtual microphones whose angle sets the stereo separation.
class Rotary
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (int i = 0; i < 2; ++i) { lp[i].lowpass (fs, 800); hp[i].highpass (fs, 800); lp[i].reset(); hp[i].reset(); }
        hornLine.setMaxSamples ((int) (0.01f * fs));
        drumLine.setMaxSamples ((int) (0.01f * fs));
        hornSpeed = slowHorn; drumSpeed = slowDrum;
        hornPhase = 0; drumPhase = 0.3f;
    }

    void setParams (const float* p)
    {
        separation = p[ROT_SEP] / 99.0f;
        motorOn = p[ROT_MOTOR] > 0.5f;
        fast = p[ROT_SPEED] > 0.5f;
        hiGain = dbToGain (p[ROT_HILEVEL]);
    }

    inline void process (float x, float& outL, float& outR)
    {
        // Rotor inertia: horn ~0.7 s, drum ~4 s time constants
        const float tH = motorOn ? (fast ? fastHorn : slowHorn) : 0.0f;
        const float tD = motorOn ? (fast ? fastDrum : slowDrum) : 0.0f;
        hornSpeed += (tH - hornSpeed) * (tH > hornSpeed ? 1.0f / (0.6f * fs) : 1.0f / (0.9f * fs));
        drumSpeed += (tD - drumSpeed) * (tD > drumSpeed ? 1.0f / (3.5f * fs) : 1.0f / (4.5f * fs));
        hornPhase += hornSpeed / fs; hornPhase -= std::floor (hornPhase);
        drumPhase += drumSpeed / fs; drumPhase -= std::floor (drumPhase);

        const float lo = lp[1].process (lp[0].process (x));
        const float hi = hp[1].process (hp[0].process (x));    // LR4 sums flat (allpass) in phase

        hornLine.push (hi);
        drumLine.push (lo);

        const float micAngle = separation * 0.5f * kPi;       // each mic +/- up to 90 deg
        float l = 0, r = 0;
        for (int m = 0; m < 2; ++m)
        {
            const float off = (m == 0 ? -micAngle : micAngle);
            const float ah = kTwoPi * hornPhase + off;
            const float ad = kTwoPi * drumPhase + off;
            // horn: doppler +/-0.3 ms, strong AM (directional horn)
            const float hd = 0.0012f * fs + 0.0003f * fs * std::sin (ah);
            const float hAmp = 0.55f + 0.45f * std::cos (ah);
            const float h = hornLine.readLinear (hd) * hAmp * hiGain;
            // drum: little doppler, milder AM
            const float dd = 0.0012f * fs + 0.0001f * fs * std::sin (ad);
            const float dAmp = 0.8f + 0.2f * std::cos (ad);
            const float d = drumLine.readLinear (dd) * dAmp;
            (m == 0 ? l : r) = h + d;
        }
        outL = l; outR = r;
    }

private:
    float fs = 44100;
    Biquad lp[2], hp[2];
    DelayLine hornLine, drumLine;
    float hornPhase = 0, drumPhase = 0, hornSpeed = 0, drumSpeed = 0;
    float separation = 0.8f, hiGain = 1;
    bool motorOn = true, fast = false;
    static constexpr float slowHorn = 0.83f, fastHorn = 6.7f, slowDrum = 0.67f, fastDrum = 5.8f;
};
} // namespace qv
