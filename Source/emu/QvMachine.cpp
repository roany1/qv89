#include "QvMachine.h"
#include <cmath>

namespace qvemu
{
static uint16_t reverseLow8 (uint16_t a)
{
    uint16_t lo = 0;
    for (int i = 0; i < 8; ++i) if (a & (1u << i)) lo |= (uint16_t) (1u << (7 - i));
    return (uint16_t) ((a & 0xFF00) | lo);
}

bool QvMachine::loadRom (const uint8_t* data, size_t size)
{
    if (size != 65536) return false;
    for (uint32_t a = 0; a < 65536; ++a) rom[a] = data[reverseLow8 ((uint16_t) a)];   // A0..A7 reversed on the PCB
    // sanity: reset vector must be an LJMP
    romOk = rom[0] == 0x02;
    // default key matrix (column = P1.7..P1.4 strobe index, row = P1.0..P1.3)
    for (int k = 0; k < NUM_KEYS; ++k) { keyMap[k][0] = k / 4; keyMap[k][1] = k % 4; }
    return romOk;
}

void QvMachine::powerOn (bool coldStart)
{
    if (coldStart) std::fill (sram.begin(), sram.end(), 0);
    cpu.code = [this] (uint16_t a) { return rom[a]; };
    cpu.xread = [this] (uint16_t a) { return xread (a); };
    cpu.xwrite = [this] (uint16_t a, uint8_t v) { xwrite (a, v); };
    cpu.portRead = [this] (int port, uint8_t latch) { return portRead (port, latch); };
    cpu.reset();
    dsp.reset();
    std::fill (std::begin (ddram), std::end (ddram), (uint8_t) ' ');
    cycleDebt = 0;
}

uint8_t QvMachine::xread (uint16_t a)
{
    if (a >= 0x8000) return sram[a - 0x8000];
    return 0xFF;
}

void QvMachine::xwrite (uint16_t a, uint8_t v)
{
    if (a >= 0x8000) { sram[a - 0x8000] = v; return; }
    if (a < 0x180) { dsp.prog[a] = v; return; }
    switch (a & 0xFE01)
    {
        case 0x0200: lcdCommand (v); return;
        case 0x0201: lcdData (v); return;
        default: break;
    }
    if (a == 0x0400) latches[1] = v;
    else if (a == 0x0600)
    {
        // bits 0..4: dry VCA level (bit-swapped on the PCB), bit 6: audio threshold cap discharge
        const bool hold = (v & 0x40) != 0;
        if (hold) { capHeld = true; capTime = 0; }
        else if (capHeld) { capHeld = false; capTime = 0; }
        int code = v & 0x1F;
        // undo the PCB bit swap done in firmware routine 7803h (0<->4, 1<->3)
        int lv = ((code & 0x01) << 4) | ((code & 0x10) >> 4) | ((code & 0x02) << 2) | ((code & 0x08) >> 2) | (code & 0x04);
        dryGainValue = (float) lv / 31.0f;
        latches[2] = v;
    }
    else if (a == 0x0800) latches[3] = v;
    else if (a == 0x0180) {} // DSP update strobe
}

uint8_t QvMachine::portRead (int port, uint8_t latch)
{
    if (port == 1)
    {
        // keypad: firmware drives one column low on P1.7..P1.4 and reads rows on P1.0..P1.3 (active low)
        uint8_t rows = 0x0F;
        for (int col = 0; col < 4; ++col)
        {
            if (latch & (0x80 >> col)) continue;          // column not strobed
            for (int row = 0; row < 4; ++row)
            {
                bool down = matrix[col][row];
                for (int k = 0; k < NUM_KEYS; ++k) if (keyDown[k] && keyMap[k][0] == col && keyMap[k][1] == row) down = true;
                if (down) rows &= (uint8_t) ~(1 << row);
            }
        }
        return (uint8_t) ((latch & 0xF0) | (rows & latch & 0x0F));
    }
    if (port == 3)
    {
        uint8_t v = latch;
        // P3.2: audio threshold comparator (high while the timing cap is below the input peak)
        bool high = false;
        if (! capHeld)
        {
            const double tau = 11.0e-3;                 // calibrated: -18 dBFS ~ 6 timer ticks
            const double e = std::min (0.999, (double) envelope);
            const double tReach = e > 1e-6 ? -tau * std::log (1.0 - e) : 0.0;
            high = capTime < tReach;
        }
        v = high ? (uint8_t) (v | 0x04) : (uint8_t) (v & ~0x04);
        v &= (uint8_t) ~0x08;                            // piezo comparator: no velocity
        v |= 0x30;                                       // footswitches open
        return v;
    }
    return latch;
}

void QvMachine::lcdCommand (uint8_t v)
{
    if (v & 0x80) { lcdAddr = v & 0x7F; return; }
    if (v & 0x40) return;                                // CGRAM address (custom chars) ignored
    if (v & 0x20) return;                                // function set
    if (v & 0x10) { if (v & 0x08) {} else lcdAddr = (lcdAddr + ((v & 0x04) ? 1 : -1)) & 0x7F; return; }
    if (v & 0x08) { lcdCursorOn = (v & 0x03) != 0; return; }
    if (v & 0x04) { lcdInc = (v & 0x02) ? 1 : -1; return; }
    if (v & 0x02) { lcdAddr = 0; return; }
    if (v & 0x01) { std::fill (std::begin (ddram), std::end (ddram), (uint8_t) ' '); lcdAddr = 0; }
}

void QvMachine::lcdData (uint8_t v)
{
    ddram[lcdAddr & 0x7F] = v;
    lcdAddr = (lcdAddr + lcdInc) & 0x7F;
}

std::string QvMachine::lcdLine (int line) const
{
    std::string s;
    const int base = line == 0 ? 0x00 : 0x40;
    for (int i = 0; i < 16; ++i)
    {
        const uint8_t c = ddram[base + i];
        s += (c >= 32 && c < 127) ? (char) c : ' ';
    }
    return s;
}

void QvMachine::process (const float* in, float* outL, float* outR, int n)
{
    for (int i = 0; i < n; ++i)
    {
        // run the 8031 for one sample period
        cycleDebt += kMachineCyclesPerSample;
        while (cycleDebt > 0)
        {
            if (! midiQueue.empty() && ! cpu.serialBusy()) { cpu.serialReceive (midiQueue.front()); midiQueue.pop_front(); }
            cycleDebt -= cpu.step();
        }
        // level detector envelope (peak rectifier with ~50 ms release)
        const float a = std::fabs (in[i]);
        envelope = a > envelope ? a : envelope * 0.99952f;
        if (! capHeld) capTime += 1.0 / kSampleRate;

        float l = 0, r = 0;
        dsp.tick (in[i], l, r);
        // 16-bit DAC
        outL[i] = std::round (std::max (-1.0f, std::min (1.0f, l)) * 32767.0f) / 32767.0f;
        outR[i] = std::round (std::max (-1.0f, std::min (1.0f, r)) * 32767.0f) / 32767.0f;
    }
}
} // namespace qvemu
