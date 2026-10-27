// DASP-24 (Alesis QuadraVerb DSP ASIC) instruction-level model, reconstructed from the
// v1.07 firmware's microcode (see docs/REVERSE_ENGINEERING.md). 96 steps per sample,
// 64K-word circular delay memory, ~41.667 kHz sample rate.
//
// Word format (4 bytes, little endian address):  [addr lo][addr hi][cccccccc][ccc s oooo]
//   addr  : 16-bit offset added to the memory pointer (pointer decrements every sample)
//   c     : 11-bit unsigned coefficient, value = c / 2048
//   s     : sign of the coefficient
//   oooo  : operation (see execute())
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iterator>

namespace qvemu
{
class Dasp
{
public:
    static constexpr int kSteps = 96;
    uint8_t prog[0x180] {};

    void reset()
    {
        std::fill (std::begin (mem), std::end (mem), 0);
        acc = x = y = 0; ptr = 0; overflow = false;
    }

    // Process one sample. in = ADC value (-1..1). Returns DAC L/R.
    inline void tick (float in, float& outL, float& outR)
    {
        for (int i = 0; i < kSteps; ++i)
        {
            const uint8_t* w = prog + 4 * i;
            const uint16_t a = (uint16_t) (w[0] | (w[1] << 8));
            const int cv = ((w[2] << 8) | w[3]) >> 5;
            const int op = w[3] & 0x0F;
            const float c = (w[3] & 0x10) ? -(float) cv * (1.0f / 2048.0f) : (float) cv * (1.0f / 2048.0f);

            if (i == 94) acc = quantAdc (in);           // ADC sample enters the accumulator here
            const uint16_t p = (uint16_t) (ptr + a);
            const float r = toFloat (mem[p]);
            const float old = acc;
            float v;
            switch (op)
            {
                case 0x0: v = c * r + y; break;
                case 0x1: v = old + c * r; break;
                case 0x2: v = c * r + x; break;
                case 0x3: v = c * r + y; y = 0; break;
                case 0x4: v = c * r; x = r; break;
                case 0x5: v = c * r; y = r; break;
                case 0x6: v = old + c * r; x = r; break;
                case 0x7: v = old + c * r; y = r; break;
                case 0x8: mem[p] = toMem (old); v = c * old; break;
                case 0x9: mem[p] = toMem (old); v = c * old + old; break;
                case 0xA: mem[p] = toMem (old); v = c * old + x; break;
                case 0xB: mem[p] = toMem (old); v = c * old + y; y = 0; break;
                case 0xC: mem[p] = toMem (old); v = c * old; x = old; y = v; break;
                case 0xD: mem[p] = toMem (old); v = c * old; y = old; break;
                case 0xE: mem[p] = toMem (old); v = c * old + old; x = old; break;
                default:  mem[p] = toMem (old); v = c * old + old; break;
            }
            acc = sat (v);
            if (i == 0) outL = r;
            else if (i == 95) outR = r;
        }
        --ptr;
    }

    bool overflow = false;     // clip LED
    float accLimit = 1.0f;     // accumulator saturation (full scale)
    float peakAcc = 0.0f;      // diagnostics

private:
    // Delay memory: 20-bit words (5 x 64Kx4 DRAM), stored as int32
    static constexpr float kMemScale = 524288.0f;            // 2^19
    inline int32_t toMem (float v)
    {
        float s = v * kMemScale;
        if (s > 524287.0f) { s = 524287.0f; overflow = true; }
        else if (s < -524288.0f) { s = -524288.0f; overflow = true; }
        return (int32_t) std::lrintf (s);
    }
    static inline float toFloat (int32_t m) { return (float) m * (1.0f / kMemScale); }
    inline float sat (float v)
    {
        const float av = std::fabs (v);
        if (av > peakAcc) peakAcc = av;
        if (v > accLimit) { overflow = true; return accLimit; }
        if (v < -accLimit) { overflow = true; return -accLimit; }
        return v;
    }
    static inline float quantAdc (float v)
    {
        v = std::max (-1.0f, std::min (32767.0f / 32768.0f, v));
        return std::floor (v * 32768.0f) * (1.0f / 32768.0f);
    }

    int32_t mem[65536] {};
    float acc = 0, x = 0, y = 0;
    uint16_t ptr = 0;
};
} // namespace qvemu
