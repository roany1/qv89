// Streaming windowed-sinc sample-rate converter (arbitrary ratio), used to run the
// emulated QuadraVerb at its native 41.667 kHz inside any host sample rate.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace qvemu
{
class Resampler
{
public:
    // ratio = outRate / inRate
    void prepare (double inRate, double outRate, int halfTaps = 16)
    {
        step = inRate / outRate;
        taps = halfTaps;
        const double cutoff = std::min (1.0, outRate / inRate) * 0.92;   // anti-alias margin
        tableRes = 256;
        table.assign ((size_t) ((2 * taps) * tableRes + 1), 0.0f);
        for (int i = 0; i <= 2 * taps * tableRes; ++i)
        {
            const double x = (double) i / tableRes - taps;              // -taps..taps
            const double s = std::fabs (x) < 1e-9 ? 1.0 : std::sin (M_PI * x * cutoff) / (M_PI * x * cutoff);
            const double w = kaiser (x / taps, 8.0);
            table[(size_t) i] = (float) (s * w * cutoff);
        }
        hist.assign ((size_t) (4 * taps + 8), 0.0f);
        histLen = 0; pos = 0.0;
        buf.clear(); buf.reserve (1 << 16);
    }

    double latencyInputSamples() const { return taps; }

    // push input samples, pull as many output samples as available
    void push (const float* in, int n) { buf.insert (buf.end(), in, in + n); }

    int pull (float* out, int maxOut)
    {
        int produced = 0;
        while (produced < maxOut)
        {
            const int need = (int) std::floor (pos) + taps + 1;
            if (need >= (int) buf.size()) break;
            const int base = (int) std::floor (pos);
            const double frac = pos - base;
            double acc = 0;
            for (int k = -taps + 1; k <= taps; ++k)
            {
                const int idx = base + k;
                if (idx < 0) continue;
                const double x = (double) k - frac;                        // distance
                const double ti = (x + taps) * tableRes;
                const int i0 = (int) ti;
                if (i0 < 0 || i0 + 1 >= (int) table.size()) continue;
                const double f = ti - i0;
                const double h = table[(size_t) i0] + f * (table[(size_t) i0 + 1] - table[(size_t) i0]);
                acc += h * buf[(size_t) idx];
            }
            out[produced++] = (float) acc;
            pos += step;
        }
        // drop consumed input, keep history
        const int drop = std::max (0, (int) std::floor (pos) - taps);
        if (drop > 0)
        {
            buf.erase (buf.begin(), buf.begin() + drop);
            pos -= drop;
        }
        return produced;
    }

    int available() const { return std::max (0, (int) ((buf.size() - (size_t) taps - 1 - std::floor (pos)) / step)); }

private:
    static double bessel0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
        return s;
    }
    static double kaiser (double x, double beta)
    {
        if (std::fabs (x) > 1.0) return 0.0;
        return bessel0 (beta * std::sqrt (1.0 - x * x)) / bessel0 (beta);
    }
    double step = 1.0, pos = 0.0;
    int taps = 16, tableRes = 256, histLen = 0;
    std::vector<float> table, hist, buf;
};
} // namespace qvemu
