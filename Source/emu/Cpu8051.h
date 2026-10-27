// Intel 8031/8051 core used to run the original QuadraVerb firmware.
// Machine-cycle accurate enough for timers / serial; no JUCE dependencies.
#pragma once
#include <cstdint>
#include <functional>

namespace qvemu
{
class Cpu8051
{
public:
    // Bus callbacks
    std::function<uint8_t (uint16_t)> code;            // MOVC / fetch
    std::function<uint8_t (uint16_t)> xread;           // MOVX read
    std::function<void (uint16_t, uint8_t)> xwrite;    // MOVX write
    std::function<uint8_t (int port, uint8_t latch)> portRead; // P0..P3 pin read

    void reset();
    int step();                    // executes one instruction (+ pending interrupt), returns machine cycles
    void serialReceive (uint8_t b) { rxPending = true; rxByte = b; }
    bool serialBusy() const { return rxPending; }
    uint64_t cycles = 0;

    uint8_t iram[256] {};          // 0x00-0x7F RAM, 0x80-0xFF SFR
    uint8_t ram80[128] {};          // indirect 0x80-0xFF (8032 only; harmless)
    uint16_t pc = 0;

private:
    enum { SP = 0x81, DPL = 0x82, DPH = 0x83, PCON = 0x87, TCON = 0x88, TMOD = 0x89, TL0 = 0x8A, TL1 = 0x8B,
           TH0 = 0x8C, TH1 = 0x8D, P1 = 0x90, SCON = 0x98, SBUF = 0x99, P2 = 0xA0, IE = 0xA8, P3 = 0xB0,
           IP = 0xB8, PSW = 0xD0, ACC = 0xE0, B = 0xF0, P0 = 0x80 };

    uint8_t fetch() { return code (pc++); }
    uint8_t& A() { return iram[ACC]; }
    uint16_t dptr() const { return (uint16_t) ((iram[DPH] << 8) | iram[DPL]); }
    void setDptr (uint16_t v) { iram[DPH] = (uint8_t) (v >> 8); iram[DPL] = (uint8_t) v; }
    int bank() const { return ((iram[PSW] >> 3) & 3) * 8; }
    uint8_t& R (int n) { return iram[bank() + n]; }
    bool cy() const { return (iram[PSW] & 0x80) != 0; }
    void setCy (bool v) { iram[PSW] = (uint8_t) ((iram[PSW] & 0x7F) | (v ? 0x80 : 0)); }

    uint8_t rdDirect (uint8_t a);
    void wrDirect (uint8_t a, uint8_t v);
    uint8_t rdInd (uint8_t a) { return a < 0x80 ? iram[a] : ram80[a - 0x80]; }
    void wrInd (uint8_t a, uint8_t v) { if (a < 0x80) iram[a] = v; else ram80[a - 0x80] = v; }
    bool getBit (uint8_t b, bool forRmw = false);
    void setBit (uint8_t b, bool v);
    void push (uint8_t v) { iram[SP]++; wrInd (iram[SP], v); }
    uint8_t pop() { uint8_t v = rdInd (iram[SP]); iram[SP]--; return v; }
    void parity();
    void add (uint8_t v, int c);
    void subb (uint8_t v);
    uint16_t rel (uint8_t off) { return (uint16_t) (pc + (int8_t) off); }
    int execute (uint8_t op);
    void tickTimers (int mc);
    void checkInterrupts();

    bool inIsr[2] { false, false };      // priority levels in service
    bool rxPending = false; uint8_t rxByte = 0;
    int txCount = 0;
    int serialDiv = 0;
};
} // namespace qvemu
