// Complete QuadraVerb machine: 8031 running the original EPROM + DASP DSP + LCD/keys/latches.
#pragma once
#include "Cpu8051.h"
#include "Dasp.h"
#include <vector>
#include <deque>
#include <string>
#include <array>

namespace qvemu
{
class QvMachine
{
public:
    static constexpr double kSampleRate = 24000000.0 / 576.0;   // 41666.67 Hz
    static constexpr double kMachineCyclesPerSample = 1000000.0 / kSampleRate; // 12 MHz / 12

    bool loadRom (const uint8_t* data, size_t size);   // raw 27C512 image (scrambled as on the PCB)
    bool hasRom() const { return romOk; }
    void powerOn (bool coldStart);                     // coldStart clears battery RAM
    void process (const float* inMono, float* outL, float* outR, int n);   // at kSampleRate

    // MIDI input (raw bytes, incl. SysEx)
    void midiIn (const uint8_t* data, size_t n) { for (size_t i = 0; i < n; ++i) midiQueue.push_back (data[i]); }
    size_t midiPending() const { return midiQueue.size(); }

    // Front panel
    enum Key { K_PROGRAM, K_STORE, K_REVERB, K_DELAY, K_PITCH, K_EQ, K_MIDI, K_CONFIG, K_MIX, K_MOD, K_NAME, K_BYPASS,
               K_PAGE_UP, K_PAGE_DOWN, K_VALUE_UP, K_VALUE_DOWN, NUM_KEYS };
    void setKey (int key, bool down) { if (key >= 0 && key < NUM_KEYS) keyDown[key] = down; }
    void setKeyMatrix (int col, int row, bool down) { matrix[col & 3][row & 3] = down; }
    std::string lcdLine (int line) const;
    int lcdCursor() const { return lcdCursorOn ? lcdAddr : -1; }
    uint8_t ledLatch (int which) const { return latches[which & 3]; }   // 0x200..0x800
    float dryGain() const { return dryGainValue; }
    bool clip() const { return dsp.overflow; }
    void clearClip() { dsp.overflow = false; }
    Dasp& dspCore() { return dsp; }

    // Battery-backed RAM (32 KB) for state save/restore
    std::vector<uint8_t> sram = std::vector<uint8_t> (32768, 0);

    int keyMap[NUM_KEYS][2];            // column,row for each front panel key (auto-detected/defaults)

private:
    uint8_t xread (uint16_t a);
    void xwrite (uint16_t a, uint8_t v);
    uint8_t portRead (int port, uint8_t latch);
    void lcdCommand (uint8_t v);
    void lcdData (uint8_t v);

    std::vector<uint8_t> rom = std::vector<uint8_t> (65536, 0xFF);
    bool romOk = false;
    Cpu8051 cpu;
    Dasp dsp;
    double cycleDebt = 0;
    std::deque<uint8_t> midiQueue;
    bool keyDown[NUM_KEYS] {};
    bool matrix[4][4] {};
    uint8_t latches[4] { 0, 0, 0, 0 };
    // LCD (HD44780 compatible)
    uint8_t ddram[128] {};
    int lcdAddr = 0, lcdInc = 1; bool lcdCursorOn = false;
    // level detector (audio threshold comparator on P3.2)
    float envelope = 0;
    double capTime = 0; bool capHeld = true;
    float dryGainValue = 0.5f;
};
} // namespace qvemu
