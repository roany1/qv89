#include "Cpu8051.h"

namespace qvemu
{
// machine cycles per opcode (standard 8051 timing)
static const uint8_t kCycles[256] = {
//  0 1 2 3 4 5 6 7 8 9 A B C D E F
    1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0
    2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1, // 1
    2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1, // 2
    2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1, // 3
    2,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1, // 4
    2,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1, // 5
    2,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1, // 6
    2,2,2,2,1,2,1,1,1,1,1,1,1,1,1,1, // 7
    2,2,2,2,4,2,2,2,2,2,2,2,2,2,2,2, // 8
    2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1, // 9
    2,2,1,2,4,1,2,2,2,2,2,2,2,2,2,2, // A
    2,2,1,1,2,2,2,2,2,2,2,2,2,2,2,2, // B
    2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // C
    2,2,1,1,1,2,1,1,2,2,2,2,2,2,2,2, // D
    2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1, // E
    2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1, // F
};

void Cpu8051::reset()
{
    for (auto& b : iram) b = 0;
    for (auto& b : ram80) b = 0;
    iram[SP] = 7; iram[P0] = iram[P1] = iram[P2] = iram[P3] = 0xFF;
    pc = 0; cycles = 0; inIsr[0] = inIsr[1] = false; rxPending = false; txCount = 0; serialDiv = 0;
}

uint8_t Cpu8051::rdDirect (uint8_t a)
{
    if (a < 0x80) return iram[a];
    if ((a == P0 || a == P1 || a == P2 || a == P3) && portRead)
        return portRead ((a - 0x80) >> 4, iram[a]);
    return iram[a];
}

void Cpu8051::wrDirect (uint8_t a, uint8_t v)
{
    iram[a] = v;
    if (a == SBUF) txCount = 320;            // transmit 10 bits at 31.25 kbaud
}

bool Cpu8051::getBit (uint8_t b, bool forRmw)
{
    const uint8_t addr = b < 0x80 ? (uint8_t) (0x20 + (b >> 3)) : (uint8_t) (b & 0xF8);
    const uint8_t v = (forRmw || addr < 0x80) ? iram[addr] : rdDirect (addr);
    return ((v >> (b & 7)) & 1) != 0;
}

void Cpu8051::setBit (uint8_t b, bool v)
{
    const uint8_t addr = b < 0x80 ? (uint8_t) (0x20 + (b >> 3)) : (uint8_t) (b & 0xF8);
    const uint8_t m = (uint8_t) (1u << (b & 7));
    iram[addr] = v ? (uint8_t) (iram[addr] | m) : (uint8_t) (iram[addr] & ~m);
}

void Cpu8051::parity()
{
    uint8_t a = iram[ACC]; a ^= a >> 4; a ^= a >> 2; a ^= a >> 1;
    iram[PSW] = (uint8_t) ((iram[PSW] & 0xFE) | (a & 1));
}

void Cpu8051::add (uint8_t v, int c)
{
    const int a = iram[ACC]; const int r = a + v + c;
    const bool ac = ((a & 0xF) + (v & 0xF) + c) > 0xF;
    const bool ov = (~(a ^ v) & (a ^ r) & 0x80) != 0;
    iram[ACC] = (uint8_t) r;
    iram[PSW] = (uint8_t) ((iram[PSW] & ~(0x80 | 0x40 | 0x04)) | (r > 0xFF ? 0x80 : 0) | (ac ? 0x40 : 0) | (ov ? 0x04 : 0));
}

void Cpu8051::subb (uint8_t v)
{
    const int a = iram[ACC]; const int c = cy() ? 1 : 0; const int r = a - v - c;
    const bool ac = ((a & 0xF) - (v & 0xF) - c) < 0;
    const bool ov = ((a ^ v) & (a ^ r) & 0x80) != 0;
    iram[ACC] = (uint8_t) r;
    iram[PSW] = (uint8_t) ((iram[PSW] & ~(0x80 | 0x40 | 0x04)) | (r < 0 ? 0x80 : 0) | (ac ? 0x40 : 0) | (ov ? 0x04 : 0));
}

void Cpu8051::tickTimers (int mc)
{
    const uint8_t tmod = iram[TMOD], tcon = iram[TCON];
    // Timer 0
    if (tcon & 0x10)
    {
        const int mode = tmod & 3;
        if (mode == 1)
        {
            uint32_t t = (uint32_t) ((iram[TH0] << 8) | iram[TL0]) + (uint32_t) mc;
            if (t > 0xFFFF) { iram[TCON] |= 0x20; t &= 0xFFFF; }
            iram[TH0] = (uint8_t) (t >> 8); iram[TL0] = (uint8_t) t;
        }
        else if (mode == 2)
        {
            int t = iram[TL0] + mc;
            while (t > 0xFF) { iram[TCON] |= 0x20; t = t - 0x100 + iram[TH0]; }
            iram[TL0] = (uint8_t) t;
        }
        else
        {
            uint32_t t = (uint32_t) (((iram[TH0] << 5) | (iram[TL0] & 0x1F))) + (uint32_t) mc;
            if (t > 0x1FFF) { iram[TCON] |= 0x20; t &= 0x1FFF; }
            iram[TH0] = (uint8_t) (t >> 5); iram[TL0] = (uint8_t) (t & 0x1F);
        }
    }
    // Timer 1 (baud rate generator, mode 2) – we only need it for serial timing, handled below.
    // Serial transmit completion
    if (txCount > 0) { txCount -= mc; if (txCount <= 0) iram[SCON] |= 0x02; }
    // Serial receive: one byte per 320 machine cycles (31250 baud), only if previous byte consumed (RI clear)
    serialDiv += mc;
    if (rxPending && serialDiv >= 320 && ! (iram[SCON] & 0x01) && (iram[SCON] & 0x10))
    {
        serialDiv = 0;
        iram[SBUF] = rxByte; iram[SCON] |= 0x01; rxPending = false;
    }
    if (serialDiv > 100000) serialDiv = 320;
}

void Cpu8051::checkInterrupts()
{
    const uint8_t ie = iram[IE];
    if (! (ie & 0x80)) return;
    const uint8_t ip = iram[IP], tcon = iram[TCON], scon = iram[SCON];
    struct Src { bool req; uint8_t enBit; uint8_t prBit; uint16_t vec; };
    const Src src[5] = {
        { (tcon & 0x02) != 0, 0x01, 0x01, 0x03 },
        { (tcon & 0x20) != 0, 0x02, 0x02, 0x0B },
        { (tcon & 0x08) != 0, 0x04, 0x04, 0x13 },
        { (tcon & 0x80) != 0, 0x08, 0x08, 0x1B },
        { (scon & 0x03) != 0, 0x10, 0x10, 0x23 } };
    for (int level = 1; level >= 0; --level)
    {
        if (inIsr[1] || (level == 0 && inIsr[0])) return;
        for (int i = 0; i < 5; ++i)
        {
            const auto& s = src[i];
            if (! s.req || ! (ie & s.enBit)) continue;
            if (((ip & s.prBit) ? 1 : 0) != level) continue;
            // hardware clears TF0/TF1 and edge-triggered IE0/IE1
            if (i == 1) iram[TCON] &= (uint8_t) ~0x20;
            if (i == 3) iram[TCON] &= (uint8_t) ~0x80;
            if (i == 0 && (tcon & 0x01)) iram[TCON] &= (uint8_t) ~0x02;
            if (i == 2 && (tcon & 0x04)) iram[TCON] &= (uint8_t) ~0x08;
            push ((uint8_t) pc); push ((uint8_t) (pc >> 8));
            pc = s.vec; inIsr[level] = true;
            cycles += 2; tickTimers (2);
            return;
        }
    }
}

int Cpu8051::step()
{
    checkInterrupts();
    const uint8_t op = fetch();
    const int mc = execute (op);
    cycles += (uint64_t) mc;
    tickTimers (mc);
    return mc;
}

int Cpu8051::execute (uint8_t op)
{
    const int lo = op & 0x0F, hi = op >> 4;
    if (lo == 1)
    {
        const uint8_t b1 = fetch();
        const uint16_t t = (uint16_t) ((pc & 0xF800) | ((op & 0xE0) << 3) | b1);
        if (op & 0x10) { push ((uint8_t) pc); push ((uint8_t) (pc >> 8)); }
        pc = t; return 2;
    }
    if (lo >= 6)
    {
        // register / indirect forms
        const bool ind = lo < 8; const int n = ind ? (lo - 6) : (lo - 8);
        auto get = [&]() -> uint8_t { return ind ? rdInd (R (n)) : R (n); };
        auto set = [&] (uint8_t v) { if (ind) wrInd (R (n), v); else R (n) = v; };
        switch (hi)
        {
            case 0x0: set ((uint8_t) (get() + 1)); break;
            case 0x1: set ((uint8_t) (get() - 1)); break;
            case 0x2: add (get(), 0); parity(); break;
            case 0x3: add (get(), cy() ? 1 : 0); parity(); break;
            case 0x4: A() |= get(); parity(); break;
            case 0x5: A() &= get(); parity(); break;
            case 0x6: A() ^= get(); parity(); break;
            case 0x7: set (fetch()); break;
            case 0x8: { const uint8_t d = fetch(); wrDirect (d, get()); break; }
            case 0x9: subb (get()); parity(); break;
            case 0xA: { const uint8_t d = fetch(); set (rdDirect (d)); break; }
            case 0xB: { const uint8_t imm = fetch(), off = fetch(); const uint8_t v = get(); setCy (v < imm); if (v != imm) pc = rel (off); break; }
            case 0xC: { const uint8_t v = get(); set (A()); A() = v; parity(); break; }
            case 0xD:
                if (! ind) { const uint8_t off = fetch(); const uint8_t v = (uint8_t) (get() - 1); set (v); if (v) pc = rel (off); }
                else { const uint8_t v = get(), a = A(); set ((uint8_t) ((v & 0xF0) | (a & 0x0F))); A() = (uint8_t) ((a & 0xF0) | (v & 0x0F)); parity(); }
                break;
            case 0xE: A() = get(); parity(); break;
            case 0xF: set (A()); break;
        }
        return kCycles[op];
    }
    switch (op)
    {
        case 0x00: break;
        case 0x02: { const uint8_t h = fetch(), l = fetch(); pc = (uint16_t) ((h << 8) | l); break; }
        case 0x03: { const uint8_t a = A(); A() = (uint8_t) ((a >> 1) | (a << 7)); break; }
        case 0x04: A()++; parity(); break;
        case 0x05: { const uint8_t d = fetch(); wrDirect (d, (uint8_t) (iram[d] + 1)); break; }
        case 0x10: { const uint8_t b = fetch(), off = fetch(); if (getBit (b, true)) { setBit (b, false); pc = rel (off); } break; }
        case 0x12: { const uint8_t h = fetch(), l = fetch(); push ((uint8_t) pc); push ((uint8_t) (pc >> 8)); pc = (uint16_t) ((h << 8) | l); break; }
        case 0x13: { const uint8_t a = A(); const bool c = cy(); setCy (a & 1); A() = (uint8_t) ((a >> 1) | (c ? 0x80 : 0)); parity(); break; }
        case 0x14: A()--; parity(); break;
        case 0x15: { const uint8_t d = fetch(); wrDirect (d, (uint8_t) (iram[d] - 1)); break; }
        case 0x20: { const uint8_t b = fetch(), off = fetch(); if (getBit (b)) pc = rel (off); break; }
        case 0x22: { const uint8_t h = pop(), l = pop(); pc = (uint16_t) ((h << 8) | l); break; }
        case 0x23: { const uint8_t a = A(); A() = (uint8_t) ((a << 1) | (a >> 7)); break; }
        case 0x24: add (fetch(), 0); parity(); break;
        case 0x25: add (rdDirect (fetch()), 0); parity(); break;
        case 0x30: { const uint8_t b = fetch(), off = fetch(); if (! getBit (b)) pc = rel (off); break; }
        case 0x32: { const uint8_t h = pop(), l = pop(); pc = (uint16_t) ((h << 8) | l); if (inIsr[1]) inIsr[1] = false; else inIsr[0] = false; break; }
        case 0x33: { const uint8_t a = A(); const bool c = cy(); setCy ((a & 0x80) != 0); A() = (uint8_t) ((a << 1) | (c ? 1 : 0)); parity(); break; }
        case 0x34: add (fetch(), cy() ? 1 : 0); parity(); break;
        case 0x35: add (rdDirect (fetch()), cy() ? 1 : 0); parity(); break;
        case 0x40: { const uint8_t off = fetch(); if (cy()) pc = rel (off); break; }
        case 0x42: { const uint8_t d = fetch(); wrDirect (d, (uint8_t) (iram[d] | A())); break; }
        case 0x43: { const uint8_t d = fetch(), i = fetch(); wrDirect (d, (uint8_t) (iram[d] | i)); break; }
        case 0x44: A() |= fetch(); parity(); break;
        case 0x45: A() |= rdDirect (fetch()); parity(); break;
        case 0x50: { const uint8_t off = fetch(); if (! cy()) pc = rel (off); break; }
        case 0x52: { const uint8_t d = fetch(); wrDirect (d, (uint8_t) (iram[d] & A())); break; }
        case 0x53: { const uint8_t d = fetch(), i = fetch(); wrDirect (d, (uint8_t) (iram[d] & i)); break; }
        case 0x54: A() &= fetch(); parity(); break;
        case 0x55: A() &= rdDirect (fetch()); parity(); break;
        case 0x60: { const uint8_t off = fetch(); if (A() == 0) pc = rel (off); break; }
        case 0x62: { const uint8_t d = fetch(); wrDirect (d, (uint8_t) (iram[d] ^ A())); break; }
        case 0x63: { const uint8_t d = fetch(), i = fetch(); wrDirect (d, (uint8_t) (iram[d] ^ i)); break; }
        case 0x64: A() ^= fetch(); parity(); break;
        case 0x65: A() ^= rdDirect (fetch()); parity(); break;
        case 0x70: { const uint8_t off = fetch(); if (A() != 0) pc = rel (off); break; }
        case 0x72: { const uint8_t b = fetch(); setCy (cy() || getBit (b)); break; }
        case 0x73: pc = (uint16_t) (A() + dptr()); break;
        case 0x74: A() = fetch(); parity(); break;
        case 0x75: { const uint8_t d = fetch(), i = fetch(); wrDirect (d, i); break; }
        case 0x80: { const uint8_t off = fetch(); pc = rel (off); break; }
        case 0x82: { const uint8_t b = fetch(); setCy (cy() && getBit (b)); break; }
        case 0x83: A() = code ((uint16_t) (A() + pc)); parity(); break;
        case 0x84: { const uint8_t a = A(), b = iram[B]; iram[PSW] &= (uint8_t) ~(0x80 | 0x04);
                     if (b == 0) iram[PSW] |= 0x04; else { A() = (uint8_t) (a / b); iram[B] = (uint8_t) (a % b); } parity(); break; }
        case 0x85: { const uint8_t s = fetch(), d = fetch(); wrDirect (d, rdDirect (s)); break; }
        case 0x90: { const uint8_t h = fetch(), l = fetch(); setDptr ((uint16_t) ((h << 8) | l)); break; }
        case 0x92: { const uint8_t b = fetch(); setBit (b, cy()); break; }
        case 0x93: A() = code ((uint16_t) (A() + dptr())); parity(); break;
        case 0x94: subb (fetch()); parity(); break;
        case 0x95: subb (rdDirect (fetch())); parity(); break;
        case 0xA0: { const uint8_t b = fetch(); setCy (cy() || ! getBit (b)); break; }
        case 0xA2: { const uint8_t b = fetch(); setCy (getBit (b)); break; }
        case 0xA3: setDptr ((uint16_t) (dptr() + 1)); break;
        case 0xA4: { const unsigned r = (unsigned) A() * iram[B]; A() = (uint8_t) r; iram[B] = (uint8_t) (r >> 8);
                     iram[PSW] = (uint8_t) ((iram[PSW] & ~(0x80 | 0x04)) | (r > 0xFF ? 0x04 : 0)); parity(); break; }
        case 0xA5: break; // reserved
        case 0xB0: { const uint8_t b = fetch(); setCy (cy() && ! getBit (b)); break; }
        case 0xB2: { const uint8_t b = fetch(); setBit (b, ! getBit (b, true)); break; }
        case 0xB3: setCy (! cy()); break;
        case 0xB4: { const uint8_t imm = fetch(), off = fetch(); setCy (A() < imm); if (A() != imm) pc = rel (off); break; }
        case 0xB5: { const uint8_t d = fetch(), off = fetch(); const uint8_t v = rdDirect (d); setCy (A() < v); if (A() != v) pc = rel (off); break; }
        case 0xC0: push (rdDirect (fetch())); break;
        case 0xC2: setBit (fetch(), false); break;
        case 0xC3: setCy (false); break;
        case 0xC4: { const uint8_t a = A(); A() = (uint8_t) ((a << 4) | (a >> 4)); break; }
        case 0xC5: { const uint8_t d = fetch(); const uint8_t v = rdDirect (d); wrDirect (d, A()); A() = v; parity(); break; }
        case 0xD0: { const uint8_t d = fetch(); wrDirect (d, pop()); break; }
        case 0xD2: setBit (fetch(), true); break;
        case 0xD3: setCy (true); break;
        case 0xD4: { int a = A(); bool c = cy();
                     if ((a & 0xF) > 9 || (iram[PSW] & 0x40)) a += 6;
                     if (a > 0xFF) c = true;
                     if (((a >> 4) & 0x1F) > 9 || c) a += 0x60;
                     if (a > 0xFF) c = true;
                     A() = (uint8_t) a; setCy (c); break; }
        case 0xD5: { const uint8_t d = fetch(), off = fetch(); const uint8_t v = (uint8_t) (iram[d] - 1); wrDirect (d, v); if (v) pc = rel (off); break; }
        case 0xE0: A() = xread (dptr()); parity(); break;
        case 0xE2: A() = xread ((uint16_t) ((iram[P2] << 8) | R (0))); parity(); break;
        case 0xE3: A() = xread ((uint16_t) ((iram[P2] << 8) | R (1))); parity(); break;
        case 0xE4: A() = 0; parity(); break;
        case 0xE5: A() = rdDirect (fetch()); parity(); break;
        case 0xF0: xwrite (dptr(), A()); break;
        case 0xF2: xwrite ((uint16_t) ((iram[P2] << 8) | R (0)), A()); break;
        case 0xF3: xwrite ((uint16_t) ((iram[P2] << 8) | R (1)), A()); break;
        case 0xF4: A() = (uint8_t) ~A(); parity(); break;
        case 0xF5: { const uint8_t d = fetch(); wrDirect (d, A()); break; }
        default: break;
    }
    return kCycles[op];
}
} // namespace qvemu
