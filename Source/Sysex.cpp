#include "Sysex.h"
#include <algorithm>
#include <cmath>

namespace qv::sysex
{
std::vector<uint8_t> unpack7 (const uint8_t* d, size_t n)
{
    std::vector<uint8_t> out;
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < n; ++i)
    {
        acc = (acc << 7) | (d[i] & 0x7f); bits += 7;
        if (bits >= 8) { bits -= 8; out.push_back ((uint8_t) ((acc >> bits) & 0xff)); }
    }
    return out;
}

std::vector<uint8_t> pack7 (const std::vector<uint8_t>& bytes)
{
    std::vector<uint8_t> out;
    uint32_t acc = 0; int bits = 0;
    for (uint8_t b : bytes)
    {
        acc = (acc << 8) | b; bits += 8;
        while (bits >= 7) { bits -= 7; out.push_back ((uint8_t) ((acc >> bits) & 0x7f)); }
    }
    if (bits > 0) out.push_back ((uint8_t) ((acc << (7 - bits)) & 0x7f));
    return out;
}

// ---- Mod-target mapping (service manual, "modulation target values") ----------------------
static int decodeTarget (int cfg, int revType, int pitchMode, bool post, int t)
{
    const int fn = (t >> 4) & 0x0f, ix = t & 0x0f;
    switch (fn)
    {
        case 0: // reverb
        {
            if (cfg == CFG_GRAPHIC || cfg == CFG_5BAND) return -1;
            if (revType == 3) { static const int h[] = { REV_INMIX, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF, REV_LOWDEC, REV_HIGHDEC }; return ix < 7 ? h[ix] : -1; }
            static const int r[] = { REV_INMIX, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF, REV_DENSITY, REV_LOWDEC, REV_HIGHDEC };
            return ix < 8 ? r[ix] : -1;
        }
        case 1: // delay
        {
            if (cfg == CFG_3BAND_REV) return -1;
            if (cfg == CFG_GRAPHIC) { static const int d[] = { DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R }; return ix < 4 ? d[ix] : -1; }
            static const int d[] = { DLY_INMIX, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R }; return ix < 5 ? d[ix] : -1;
        }
        case 2: // pitch
            if (cfg == CFG_ROTARY) { static const int r[] = { ROT_SEP, ROT_MOTOR, ROT_SPEED }; return ix < 3 ? r[ix] : -1; }
            if (cfg == CFG_3BAND_REV) { static const int r[] = { LFO_SPEED, LFO_DEPTH }; return ix < 2 ? r[ix] : -1; }
            if (cfg == CFG_GRAPHIC) return -1;
            if (pitchMode == 4) return ix == 0 ? DETUNE : -1;
            if (pitchMode == 5) { static const int r[] = { LFO_SPEED, LFO_DEPTH }; return ix < 2 ? r[ix] : -1; }
            { static const int r[] = { LFO_SPEED, LFO_DEPTH, PITCH_FB }; return ix < 3 ? r[ix] : -1; }
        case 3: // eq
            if (cfg == CFG_ROTARY) return ix == 0 ? ROT_HILEVEL : -1;
            if (cfg == CFG_GRAPHIC) return ix < 11 ? GEQ_16 + ix : -1;
            if (cfg == CFG_5BAND)
            {
                static const int r[] = { EQ_LOW_FREQ, EQ_LOW_AMP, EQ_LOMID_FREQ, EQ_LOMID_BW, EQ_LOMID_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP,
                                         EQ_HIMID_FREQ, EQ_HIMID_BW, EQ_HIMID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP };
                return ix < 13 ? r[ix] : -1;
            }
            { static const int r[] = { EQ_LOW_FREQ, EQ_LOW_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP }; return ix < 7 ? r[ix] : -1; }
        case 4: // mix
            switch (cfg)
            {
                case CFG_QUAD:
                    if (post) { static const int r[] = { MIX_MASTER, MIX_EQ, MIX_PITCH, MIX_DELAY, MIX_REVERB }; return ix < 5 ? r[ix] : -1; }
                    { static const int r[] = { MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }; return ix < 5 ? r[ix] : -1; }
                case CFG_ROTARY: { static const int r[] = { MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }; return ix < 4 ? r[ix] : -1; }
                case CFG_GRAPHIC: { static const int r[] = { MIX_MASTER, MIX_EQ, MIX_DELAY }; return ix < 3 ? r[ix] : -1; }
                case CFG_5BAND: { static const int r[] = { MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY }; return ix < 4 ? r[ix] : -1; }
                default: { static const int r[] = { MIX_DIRECT, MIX_MASTER, MIX_REVERB }; return ix < 3 ? r[ix] : -1; }
            }
        default: return -1; // multi-tap targets (QuadraVerb Plus) not supported
    }
}

static int listIndexOf (int param)
{
    const auto& l = modTargetList();
    for (size_t i = 0; i < l.size(); ++i) if (l[i] == param) return (int) i;
    return -1;
}

// ---- Program decode / encode --------------------------------------------------------------
bool decodeProgram (const uint8_t* b, Program& out, std::string& warn)
{
    out = defaultProgram();
    auto& v = out.v;
    auto u16 = [b] (int a) { return (int) ((b[a] << 8) | b[a + 1]); };
    auto sgn = [] (int x, int centre) { return (float) (x - centre); };
    auto clampP = [&v] (int i) { const auto& pi = info (i); v[(size_t) i] = std::min (pi.maxV, std::max (pi.minV, v[(size_t) i])); };

    int cfg = b[0x44];
    if (cfg > 4) { warn += "config " + std::to_string (cfg) + " (QuadraVerb Plus ring/resonator/sampling) not supported -> QuadMode; "; cfg = 0; }
    v[CONFIG] = (float) cfg;

    if (cfg == CFG_GRAPHIC)
        for (int i = 0; i < 11; ++i) v[(size_t) (GEQ_16 + i)] = sgn (b[0x01 + i], 14);
    else if (cfg != CFG_ROTARY)
    {
        v[EQ_LOW_FREQ] = (float) u16 (0x00);
        v[EQ_LOW_AMP]  = (float) (u16 (0x02) - 280) * 0.05f;
        v[EQ_MID_FREQ] = (float) u16 (0x04);
        v[EQ_MID_BW]   = (float) b[0x06] / 100.0f;
        v[EQ_MID_AMP]  = (float) (u16 (0x07) - 280) * 0.05f;
        v[EQ_HIGH_FREQ]= (float) u16 (0x09);
        v[EQ_HIGH_AMP] = (float) (u16 (0x0B) - 280) * 0.05f;
        if (cfg == CFG_5BAND)
        {
            v[EQ_LOMID_FREQ] = (float) u16 (0x0E);
            v[EQ_LOMID_BW]   = (float) b[0x10] / 100.0f;
            v[EQ_LOMID_AMP]  = (float) (u16 (0x11) - 280) * 0.05f;
            v[EQ_HIMID_FREQ] = (float) u16 (0x13);
            v[EQ_HIMID_BW]   = (float) b[0x15] / 100.0f;
            v[EQ_HIMID_AMP]  = (float) (u16 (0x16) - 280) * 0.05f;
        }
    }
    else v[ROT_HILEVEL] = sgn (b[0x0D], 20);

    if (cfg == CFG_3BAND_REV) v[REV_CHORUS] = (float) (b[0x1A] & 1);
    else if (cfg != CFG_ROTARY) v[PITCH_MODE] = (float) b[0x1A];
    v[PITCH_INPUT] = (float) (b[0x1B] & 1);
    v[LFO_WAVE] = (float) (b[0x1C] & 1);
    v[LFO_SPEED] = (float) b[0x1D];
    v[LFO_DEPTH] = (float) b[0x1E];
    v[PITCH_FB] = (float) b[0x20];
    v[DETUNE] = sgn (b[0x21], 99);
    v[TRIGGER_FLANGE] = (float) (b[0x25] & 1);
    if (cfg == CFG_ROTARY) { v[ROT_SEP] = (float) b[0x22]; v[ROT_MOTOR] = (float) (b[0x23] & 1); v[ROT_SPEED] = (float) (b[0x24] & 1); }

    v[DLY_MODE] = (float) b[0x27];
    if (v[DLY_MODE] > 2) { warn += "multi-tap delay (Plus) -> stereo; "; v[DLY_MODE] = 1; }
    v[DLY_INPUT] = (float) (b[0x28] & 1);
    v[DLY_INMIX] = sgn (b[0x29], 99);
    v[DLY_TIME] = (float) u16 (0x2A);
    v[DLY_FB] = (float) b[0x2C];
    v[DLY_TIME_R] = (float) u16 (0x2D);
    v[DLY_FB_R] = (float) b[0x2F];

    v[REV_TYPE] = (float) b[0x32];
    v[REV_IN1] = (float) b[0x34];
    v[REV_IN2] = (float) (b[0x35] & 1);
    v[REV_INMIX] = sgn (b[0x36], 99);
    v[REV_PREDELAY] = (float) b[0x37];
    v[REV_PREMIX] = sgn (b[0x38], 99);
    v[REV_DECAY] = (float) b[0x39];
    v[REV_DIFF] = (float) b[0x3A];
    v[REV_LOWDEC] = -(float) b[0x3B];       // stored as magnitude 0..60 (display -00..-60); confirmed against v1.07 ROM defaults
    v[REV_HIGHDEC] = -(float) b[0x3C];
    v[REV_DENSITY] = (float) b[0x3D];
    v[GATE_ON] = (float) (b[0x3E] & 1);
    v[GATE_HOLD] = (float) b[0x3F];
    v[GATE_REL] = (float) b[0x40];
    v[GATE_LEVEL] = (float) b[0x41];

    const bool post = (b[0x45] & 1) != 0;
    v[MIX_DIRECT_SEL] = post ? 1.0f : 0.0f;
    if (post || cfg == CFG_GRAPHIC) v[MIX_EQ] = (float) b[0x46]; else v[MIX_DIRECT] = (float) b[0x46];
    v[MIX_MASTER] = (float) b[0x47];
    v[MIX_PITCH] = (float) b[0x48];
    v[MIX_DELAY] = (float) b[0x49];
    v[MIX_REVERB] = (float) b[0x4A];

    for (int m = 0; m < 8; ++m)
    {
        const int src = b[0x50 + 3 * m], tgt = b[0x51 + 3 * m], amp = b[0x52 + 3 * m];
        v[(size_t) (MOD1_SRC + 3 * m)] = (float) std::min (src, kNumModSources - 1);
        const int param = decodeTarget (cfg, (int) v[REV_TYPE], (int) v[PITCH_MODE], post, tgt);
        const int li = param >= 0 ? listIndexOf (param) : -1;
        v[(size_t) (MOD1_TGT + 3 * m)] = (float) std::max (0, li);
        v[(size_t) (MOD1_AMP + 3 * m)] = li >= 0 ? sgn (amp, 99) : 0.0f;
    }

    std::string name;
    for (int i = 0; i < 14; ++i) { const char c = (char) b[0x6A + i]; name += (c >= 32 && c < 127) ? c : ' '; }
    while (! name.empty() && name.back() == ' ') name.pop_back();
    out.name = name.empty() ? "IMPORTED" : name;

    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) clampP (i);
    return true;
}

std::vector<uint8_t> encodeProgram (const Program& p)
{
    std::vector<uint8_t> b (128, 0);
    const auto& v = p.v;
    auto r = [&v] (int i) { return (int) std::lround (v[(size_t) i]); };
    auto put16 = [&b] (int a, int x) { x = std::max (0, x); b[(size_t) a] = (uint8_t) ((x >> 8) & 0xff); b[(size_t) a + 1] = (uint8_t) (x & 0xff); };
    auto amp = [&v] (int i) { return (int) std::lround (v[(size_t) i] / 0.05f) + 280; };
    const int cfg = r (CONFIG);
    b[0x44] = (uint8_t) cfg;

    if (cfg == CFG_GRAPHIC) for (int i = 0; i < 11; ++i) b[(size_t) (0x01 + i)] = (uint8_t) (r (GEQ_16 + i) + 14);
    else if (cfg == CFG_ROTARY) b[0x0D] = (uint8_t) (r (ROT_HILEVEL) + 20);
    else
    {
        put16 (0x00, r (EQ_LOW_FREQ)); put16 (0x02, amp (EQ_LOW_AMP));
        put16 (0x04, r (EQ_MID_FREQ)); b[0x06] = (uint8_t) std::lround (v[EQ_MID_BW] * 100); put16 (0x07, amp (EQ_MID_AMP));
        put16 (0x09, r (EQ_HIGH_FREQ)); put16 (0x0B, amp (EQ_HIGH_AMP));
        if (cfg == CFG_5BAND)
        {
            put16 (0x0E, r (EQ_LOMID_FREQ)); b[0x10] = (uint8_t) std::lround (v[EQ_LOMID_BW] * 100); put16 (0x11, amp (EQ_LOMID_AMP));
            put16 (0x13, r (EQ_HIMID_FREQ)); b[0x15] = (uint8_t) std::lround (v[EQ_HIMID_BW] * 100); put16 (0x16, amp (EQ_HIMID_AMP));
        }
    }
    b[0x1A] = (uint8_t) (cfg == CFG_3BAND_REV ? r (REV_CHORUS) : r (PITCH_MODE));
    b[0x1B] = (uint8_t) r (PITCH_INPUT); b[0x1C] = (uint8_t) r (LFO_WAVE); b[0x1D] = (uint8_t) r (LFO_SPEED);
    b[0x1E] = (uint8_t) r (LFO_DEPTH); b[0x20] = (uint8_t) r (PITCH_FB); b[0x21] = (uint8_t) (r (DETUNE) + 99);
    if (cfg == CFG_ROTARY) { b[0x22] = (uint8_t) r (ROT_SEP); b[0x23] = (uint8_t) r (ROT_MOTOR); b[0x24] = (uint8_t) r (ROT_SPEED); }
    b[0x25] = (uint8_t) r (TRIGGER_FLANGE);

    b[0x27] = (uint8_t) r (DLY_MODE); b[0x28] = (uint8_t) r (DLY_INPUT); b[0x29] = (uint8_t) (r (DLY_INMIX) + 99);
    put16 (0x2A, r (DLY_TIME)); b[0x2C] = (uint8_t) r (DLY_FB); put16 (0x2D, r (DLY_TIME_R)); b[0x2F] = (uint8_t) r (DLY_FB_R);

    b[0x32] = (uint8_t) r (REV_TYPE); b[0x34] = (uint8_t) r (REV_IN1); b[0x35] = (uint8_t) r (REV_IN2);
    b[0x36] = (uint8_t) (r (REV_INMIX) + 99); b[0x37] = (uint8_t) r (REV_PREDELAY); b[0x38] = (uint8_t) (r (REV_PREMIX) + 99);
    b[0x39] = (uint8_t) r (REV_DECAY); b[0x3A] = (uint8_t) r (REV_DIFF); b[0x3B] = (uint8_t) (-r (REV_LOWDEC));
    b[0x3C] = (uint8_t) (-r (REV_HIGHDEC)); b[0x3D] = (uint8_t) r (REV_DENSITY); b[0x3E] = (uint8_t) r (GATE_ON);
    b[0x3F] = (uint8_t) r (GATE_HOLD); b[0x40] = (uint8_t) r (GATE_REL); b[0x41] = (uint8_t) r (GATE_LEVEL);

    const bool post = r (MIX_DIRECT_SEL) != 0;
    b[0x45] = (uint8_t) (post ? 1 : 0);
    b[0x46] = (uint8_t) ((post || cfg == CFG_GRAPHIC) ? r (MIX_EQ) : r (MIX_DIRECT));
    b[0x47] = (uint8_t) r (MIX_MASTER); b[0x48] = (uint8_t) r (MIX_PITCH); b[0x49] = (uint8_t) r (MIX_DELAY); b[0x4A] = (uint8_t) r (MIX_REVERB);

    for (int m = 0; m < 8; ++m)
    {
        b[(size_t) (0x50 + 3 * m)] = (uint8_t) std::min (126, r (MOD1_SRC + 3 * m));
        const int li = r (MOD1_TGT + 3 * m);
        const int param = modTargetList()[(size_t) std::max (0, std::min ((int) modTargetList().size() - 1, li))];
        int code = 0x40; // fallback: first mix target
        bool found = false;
        for (int fn = 0; fn < 5 && ! found; ++fn)
            for (int ix = 0; ix < 16 && ! found; ++ix)
                if (decodeTarget (cfg, r (REV_TYPE), r (PITCH_MODE), post, (fn << 4) | ix) == param) { code = (fn << 4) | ix; found = true; }
        b[(size_t) (0x51 + 3 * m)] = (uint8_t) code;
        b[(size_t) (0x52 + 3 * m)] = (uint8_t) (found ? r (MOD1_AMP + 3 * m) + 99 : 99);
    }
    for (int i = 0; i < 14; ++i)
    {
        const char c = i < (int) p.name.size() ? p.name[(size_t) i] : ' ';
        b[(size_t) (0x6A + i)] = (uint8_t) ((c >= 32 && c < 127) ? c : ' ');
    }
    return b;
}

std::vector<Program> parseFile (const std::vector<uint8_t>& f, std::string& log)
{
    std::vector<Program> out;
    size_t i = 0;
    while (i < f.size())
    {
        if (f[i] != 0xF0) { ++i; continue; }
        size_t e = i + 1;
        while (e < f.size() && f[e] != 0xF7) ++e;
        if (e >= f.size()) break;
        const uint8_t* m = f.data() + i;
        const size_t len = e - i + 1;
        if (len > 8 && m[1] == 0x00 && m[2] == 0x00 && m[3] == 0x0E && m[4] == 0x02 && m[5] == 0x02)
        {
            const int progNum = m[6];
            auto data = unpack7 (m + 7, len - 8);
            const size_t nProg = data.size() / 128;
            if (nProg == 0) log += "short dump ignored; ";
            for (size_t k = 0; k < nProg; ++k)
            {
                Program p; std::string w;
                decodeProgram (data.data() + k * 128, p, w);
                if (! w.empty()) log += p.name + ": " + w;
                out.push_back (p);
            }
            (void) progNum;
        }
        else if (len > 5 && m[1] == 0x00 && m[2] == 0x00 && m[3] == 0x0E)
            log += "Alesis SysEx for another device/opcode skipped; ";
        i = e + 1;
    }
    if (out.empty()) log += "no QuadraVerb program dumps found";
    return out;
}

std::vector<uint8_t> makeDump (const Program& p, int programNumber)
{
    std::vector<uint8_t> m = { 0xF0, 0x00, 0x00, 0x0E, 0x02, 0x02, (uint8_t) programNumber };
    auto packed = pack7 (encodeProgram (p));
    m.insert (m.end(), packed.begin(), packed.end());
    m.push_back (0xF7);
    return m;
}
} // namespace qv::sysex
