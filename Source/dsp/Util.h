#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace qv
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

// One-pole parameter smoother
struct Smoother
{
    float cur = 0, target = 0, coeff = 0.01f;
    void setTime (float sr, float ms) { coeff = 1.0f - std::exp (-1.0f / std::max (1.0f, 0.001f * ms * sr)); }
    void reset (float v) { cur = target = v; }
    void set (float v) { target = v; }
    float next() { cur += coeff * (target - cur); return cur; }
};

// Power-of-two circular delay line with fractional (Hermite) reads.
// read(d): value written d samples before the *next* write (d >= 1).
class DelayLine
{
public:
    void setMaxSamples (int n)
    {
        int s = 1; while (s < n + 8) s <<= 1;
        buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int size() const { return mask + 1; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    inline float readLinear (float d) const
    {
        const int i = (int) d; const float f = d - (float) i;
        const float a = read (i), b = read (i + 1);
        return a + f * (b - a);
    }
    inline float readHermite (float d) const
    {
        d = std::max (1.0f, d);
        const int i = (int) d; const float f = d - (float) i;
        const float xm1 = read (i - 1 < 1 ? 1 : i - 1), x0 = read (i), x1 = read (i + 1), x2 = read (i + 2);
        const float c0 = x0, c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
private:
    std::vector<float> buf { 0.0f };
    int mask = 0, w = 0;
};

// Schroeder allpass on its own delay line
struct Allpass
{
    DelayLine d; int len = 1; float g = 0.5f;
    void init (int maxLen) { d.setMaxSamples (maxLen); }
    inline float process (float x)
    {
        const float z = d.read (len);
        const float v = x + g * z;
        d.push (v);
        return z - g * v;
    }
};

// Simple LFO: triangle or (slew-limited) square, output -1..1
struct LFO
{
    float phase = 0, inc = 0, sqState = 0, sqCoeff = 0.01f;
    void setRate (float sr, float hz) { inc = hz / sr; sqCoeff = 1.0f - std::exp (-1.0f / (0.004f * sr)); }
    inline float next (int wave)
    {
        phase += inc; if (phase >= 1.0f) phase -= 1.0f;
        const float tri = phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
        if (wave == 0) return tri;
        const float sq = phase < 0.5f ? 1.0f : -1.0f;
        sqState += sqCoeff * (sq - sqState);
        return sqState;
    }
};

// Cheap deterministic RNG
struct Rng { uint32_t s = 0x12345678u; float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; } };
} // namespace qv
