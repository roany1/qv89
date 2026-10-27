// Maps QV89 parameters to QuadraVerb SysEx "MIDI editing" messages (opcode 01),
// using the function/page tables of the QuadraVerb service manual (section 11).
#pragma once
#include "Params.h"
#include "Sysex.h"
#include <optional>

namespace qv::edit
{
struct Field { int offset; int width; };    // location of the stored value in the 128-byte program

// Stored-value location for a parameter (independent of config)
inline std::optional<Field> fieldFor (int p)
{
    switch (p)
    {
        case CONFIG: return Field { 0x44, 1 };
        case EQ_LOW_FREQ: return Field { 0x00, 2 };   case EQ_LOW_AMP: return Field { 0x02, 2 };
        case EQ_MID_FREQ: return Field { 0x04, 2 };   case EQ_MID_BW: return Field { 0x06, 1 };
        case EQ_MID_AMP: return Field { 0x07, 2 };    case EQ_HIGH_FREQ: return Field { 0x09, 2 };
        case EQ_HIGH_AMP: return Field { 0x0B, 2 };   case EQ_LOMID_FREQ: return Field { 0x0E, 2 };
        case EQ_LOMID_BW: return Field { 0x10, 1 };   case EQ_LOMID_AMP: return Field { 0x11, 2 };
        case EQ_HIMID_FREQ: return Field { 0x13, 2 }; case EQ_HIMID_BW: return Field { 0x15, 1 };
        case EQ_HIMID_AMP: return Field { 0x16, 2 };
        case ROT_HILEVEL: return Field { 0x0D, 1 };
        case PITCH_MODE: case REV_CHORUS: return Field { 0x1A, 1 };
        case PITCH_INPUT: return Field { 0x1B, 1 };   case LFO_WAVE: return Field { 0x1C, 1 };
        case LFO_SPEED: return Field { 0x1D, 1 };     case LFO_DEPTH: return Field { 0x1E, 1 };
        case PITCH_FB: return Field { 0x20, 1 };      case DETUNE: return Field { 0x21, 1 };
        case ROT_SEP: return Field { 0x22, 1 };       case ROT_MOTOR: return Field { 0x23, 1 };
        case ROT_SPEED: return Field { 0x24, 1 };     case TRIGGER_FLANGE: return Field { 0x25, 1 };
        case DLY_MODE: return Field { 0x27, 1 };      case DLY_INPUT: return Field { 0x28, 1 };
        case DLY_INMIX: return Field { 0x29, 1 };     case DLY_TIME: return Field { 0x2A, 2 };
        case DLY_FB: return Field { 0x2C, 1 };        case DLY_TIME_R: return Field { 0x2D, 2 };
        case DLY_FB_R: return Field { 0x2F, 1 };
        case REV_TYPE: return Field { 0x32, 1 };      case REV_IN1: return Field { 0x34, 1 };
        case REV_IN2: return Field { 0x35, 1 };       case REV_INMIX: return Field { 0x36, 1 };
        case REV_PREDELAY: return Field { 0x37, 1 };  case REV_PREMIX: return Field { 0x38, 1 };
        case REV_DECAY: return Field { 0x39, 1 };     case REV_DIFF: return Field { 0x3A, 1 };
        case REV_LOWDEC: return Field { 0x3B, 1 };    case REV_HIGHDEC: return Field { 0x3C, 1 };
        case REV_DENSITY: return Field { 0x3D, 1 };   case GATE_ON: return Field { 0x3E, 1 };
        case GATE_HOLD: return Field { 0x3F, 1 };     case GATE_REL: return Field { 0x40, 1 };
        case GATE_LEVEL: return Field { 0x41, 1 };
        case MIX_DIRECT_SEL: return Field { 0x45, 1 };
        case MIX_DIRECT: case MIX_EQ: return Field { 0x46, 1 };
        case MIX_MASTER: return Field { 0x47, 1 };    case MIX_PITCH: return Field { 0x48, 1 };
        case MIX_DELAY: return Field { 0x49, 1 };     case MIX_REVERB: return Field { 0x4A, 1 };
        default: break;
    }
    if (p >= GEQ_16 && p <= GEQ_16K) return Field { 0x01 + (p - GEQ_16), 1 };
    if (p >= MOD1_SRC && p <= MOD8_AMP) { const int m = (p - MOD1_SRC) / 3, k = (p - MOD1_SRC) % 3; return Field { 0x50 + 3 * m + k, 1 }; }
    return std::nullopt;
}

// function (1..10) and page for a parameter in the current configuration / modes
inline bool functionPage (int p, const float* v, int& func, int& page)
{
    const int cfg = (int) v[CONFIG];
    const int type = (int) v[REV_TYPE];
    const int pmode = (int) v[PITCH_MODE];
    const bool post = v[MIX_DIRECT_SEL] > 0.5f;
    func = -1; page = -1;
    auto revPage = [&] (int p2) -> int
    {
        // configs 0/1: type,in1,in2,inmix,predelay,premix,decay,diff,density,low,high,gate,hold,rel,level
        // config 4   : type,in1,predelay,premix,decay,diff,density,low,high,gate,hold,rel,level
        std::vector<int> order;
        if (cfg == CFG_3BAND_REV) order = { REV_TYPE, REV_IN1, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF };
        else order = { REV_TYPE, REV_IN1, REV_IN2, REV_INMIX, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF };
        if (type != 3) order.push_back (REV_DENSITY);
        order.push_back (REV_LOWDEC); order.push_back (REV_HIGHDEC);
        if (type != 4) { order.push_back (GATE_ON); order.push_back (GATE_HOLD); order.push_back (GATE_REL); order.push_back (GATE_LEVEL); }
        for (size_t i = 0; i < order.size(); ++i) if (order[i] == p2) return (int) i;
        return -1;
    };
    auto idx = [] (std::initializer_list<int> l, int p2) { int i = 0; for (int q : l) { if (q == p2) return i; ++i; } return -1; };

    if (p == CONFIG) { func = 7; page = 0; return true; }
    if (p >= REV_TYPE && p <= GATE_LEVEL)
    {
        if (cfg == CFG_GRAPHIC || cfg == CFG_5BAND) return false;
        func = 1; page = revPage (p); return page >= 0;
    }
    if (p >= DLY_MODE && p <= DLY_FB_R)
    {
        func = 2;
        switch (cfg)
        {
            case CFG_QUAD: case CFG_5BAND: page = idx ({ DLY_MODE, DLY_INPUT, DLY_INMIX, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R }, p); break;
            case CFG_ROTARY: page = idx ({ DLY_MODE, DLY_INMIX, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R }, p); break;
            case CFG_GRAPHIC: page = idx ({ DLY_MODE, DLY_INPUT, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R }, p); break;
            default: return false;
        }
        return page >= 0;
    }
    if (p == ROT_SEP || p == ROT_MOTOR || p == ROT_SPEED)
    {
        if (cfg != CFG_ROTARY) return false;
        func = 3; page = idx ({ ROT_SEP, ROT_MOTOR, ROT_SPEED }, p); return page >= 0;
    }
    if (p == ROT_HILEVEL) { if (cfg != CFG_ROTARY) return false; func = 4; page = 0; return true; }
    if ((p >= PITCH_MODE && p <= DETUNE) || p == REV_CHORUS)
    {
        func = 3;
        if (cfg == CFG_3BAND_REV) { page = idx ({ REV_CHORUS, LFO_WAVE, LFO_SPEED, LFO_DEPTH }, p); return page >= 0; }
        if (cfg != CFG_QUAD && cfg != CFG_5BAND) return false;
        if (p == PITCH_MODE) { page = 0; return true; }
        if (p == PITCH_INPUT) { page = 1; return true; }
        if (pmode <= 1) page = idx ({ -1, -1, LFO_WAVE, LFO_SPEED, LFO_DEPTH, PITCH_FB }, p);
        else if (pmode <= 3) page = idx ({ -1, -1, LFO_SPEED, LFO_DEPTH, PITCH_FB, TRIGGER_FLANGE }, p);
        else if (pmode == 4) page = idx ({ -1, -1, DETUNE }, p);
        else page = idx ({ -1, -1, LFO_SPEED, LFO_DEPTH }, p);
        return page >= 0;
    }
    if ((p >= EQ_LOW_FREQ && p <= EQ_HIGH_AMP) || (p >= GEQ_16 && p <= GEQ_16K))
    {
        func = 4;
        if (cfg == CFG_GRAPHIC) { page = (p >= GEQ_16 && p <= GEQ_16K) ? p - GEQ_16 : -1; return page >= 0; }
        if (cfg == CFG_5BAND)
        {
            page = idx ({ EQ_LOW_FREQ, EQ_LOW_AMP, EQ_LOMID_FREQ, EQ_LOMID_BW, EQ_LOMID_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP,
                          EQ_HIMID_FREQ, EQ_HIMID_BW, EQ_HIMID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP }, p);
            return page >= 0;
        }
        if (cfg == CFG_QUAD || cfg == CFG_3BAND_REV)
        {
            page = idx ({ EQ_LOW_FREQ, EQ_LOW_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP }, p);
            return page >= 0;
        }
        return false;
    }
    if (p >= MIX_DIRECT_SEL && p <= MIX_REVERB)
    {
        func = 8;
        switch (cfg)
        {
            case CFG_QUAD:
                page = post ? idx ({ MIX_DIRECT_SEL, MIX_MASTER, MIX_EQ, MIX_PITCH, MIX_DELAY, MIX_REVERB }, p)
                            : idx ({ MIX_DIRECT_SEL, MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }, p);
                break;
            case CFG_ROTARY: page = idx ({ MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }, p); break;
            case CFG_GRAPHIC: page = idx ({ MIX_MASTER, MIX_EQ, MIX_DELAY }, p); break;
            case CFG_5BAND:
                page = post ? idx ({ MIX_DIRECT_SEL, MIX_MASTER, MIX_EQ, MIX_PITCH, MIX_DELAY }, p)
                            : idx ({ MIX_DIRECT_SEL, MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY }, p);
                break;
            default:
                page = post ? idx ({ MIX_DIRECT_SEL, MIX_MASTER, MIX_EQ, -1, -1, MIX_REVERB }, p)
                            : idx ({ MIX_DIRECT_SEL, MIX_DIRECT, MIX_MASTER, -1, -1, MIX_REVERB }, p);
                break;
        }
        return page >= 0;
    }
    if (p >= MOD1_SRC && p <= MOD8_AMP) { func = 9; page = p - MOD1_SRC; return true; }
    return false;
}

inline std::vector<uint8_t> editMessage (int func, int page, int value)
{
    const uint8_t lsb = (uint8_t) (value & 0xFF), msb = (uint8_t) ((value >> 8) & 0xFF);
    return { 0xF0, 0x00, 0x00, 0x0E, 0x02, 0x01, (uint8_t) func, (uint8_t) page,
             (uint8_t) (lsb >> 1), (uint8_t) (((lsb & 1) << 6) | (msb >> 2)), (uint8_t) ((msb & 3) << 5), 0xF7 };
}

// Build the SysEx edit for parameter p given the full parameter array (program part)
inline std::optional<std::vector<uint8_t>> editFor (int p, const float* v)
{
    int func, page;
    if (! functionPage (p, v, func, page)) return std::nullopt;
    auto f = fieldFor (p);
    if (! f) return std::nullopt;
    Program prog;
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) prog.v[(size_t) i] = v[i];
    const auto bytes = sysex::encodeProgram (prog);
    int value = bytes[(size_t) f->offset];
    if (f->width == 2) value = (bytes[(size_t) f->offset] << 8) | bytes[(size_t) f->offset + 1];
    return editMessage (func, page, value);
}

inline std::vector<uint8_t> nameMessages (const std::string& name)
{
    std::vector<uint8_t> all;
    for (int i = 0; i < 14; ++i)
    {
        const char c = i < (int) name.size() ? name[(size_t) i] : ' ';
        auto m = editMessage (10, i, (uint8_t) ((c >= 32 && c < 127) ? c : ' '));
        all.insert (all.end(), m.begin(), m.end());
    }
    return all;
}
} // namespace qv::edit
