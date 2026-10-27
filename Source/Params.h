// QV89 — parameter table shared by DSP, UI, preset and SysEx code.
// Ranges and defaults follow the Alesis QuadraVerb service-manual parameter map
// (section 11) and the owner's-manual default chart. Pure C++, no JUCE.
#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <cmath>

namespace qv
{
enum Config { CFG_QUAD = 0, CFG_ROTARY, CFG_GRAPHIC, CFG_5BAND, CFG_3BAND_REV, NUM_CONFIGS };

enum P : int
{
    CONFIG = 0,
    // 3/5 band parametric EQ
    EQ_LOW_FREQ, EQ_LOW_AMP,
    EQ_LOMID_FREQ, EQ_LOMID_BW, EQ_LOMID_AMP,
    EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP,
    EQ_HIMID_FREQ, EQ_HIMID_BW, EQ_HIMID_AMP,
    EQ_HIGH_FREQ, EQ_HIGH_AMP,
    // 11 band graphic
    GEQ_16, GEQ_32, GEQ_62, GEQ_126, GEQ_250, GEQ_500, GEQ_1K, GEQ_2K, GEQ_4K, GEQ_8K, GEQ_16K,
    // Pitch section
    PITCH_MODE, PITCH_INPUT, LFO_WAVE, LFO_SPEED, LFO_DEPTH, PITCH_FB, TRIGGER_FLANGE, DETUNE, REV_CHORUS,
    // Rotary speaker (config 2)
    ROT_SEP, ROT_MOTOR, ROT_SPEED, ROT_HILEVEL,
    // Delay
    DLY_MODE, DLY_INPUT, DLY_INMIX, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R,
    // Reverb
    REV_TYPE, REV_IN1, REV_IN2, REV_INMIX, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF, REV_DENSITY,
    REV_LOWDEC, REV_HIGHDEC, GATE_ON, GATE_HOLD, GATE_REL, GATE_LEVEL,
    // Mix
    MIX_DIRECT_SEL, MIX_DIRECT, MIX_MASTER, MIX_EQ, MIX_PITCH, MIX_DELAY, MIX_REVERB,
    // Mod matrix (8 slots x source/target/amplitude)
    MOD1_SRC, MOD1_TGT, MOD1_AMP,
    MOD2_SRC, MOD2_TGT, MOD2_AMP,
    MOD3_SRC, MOD3_TGT, MOD3_AMP,
    MOD4_SRC, MOD4_TGT, MOD4_AMP,
    MOD5_SRC, MOD5_TGT, MOD5_AMP,
    MOD6_SRC, MOD6_TGT, MOD6_AMP,
    MOD7_SRC, MOD7_TGT, MOD7_AMP,
    MOD8_SRC, MOD8_TGT, MOD8_AMP,
    NUM_PROGRAM_PARAMS,
    // Global (not stored in programs)
    IN_GAIN = NUM_PROGRAM_PARAMS, OUT_GAIN, BYPASS, VINTAGE, ENGINE,
    NUM_PARAMS
};

struct ParamInfo
{
    int index;
    const char* id;
    const char* lcd;     // LCD page title (max 16 chars)
    float minV, maxV, step, def;
    std::vector<std::string> choices; // non-empty => enumerated
};

// Mod sources: 0 pitch bend, 1 aftertouch, 2 note number, 3 velocity, 4..131 = CC 0..127
constexpr int kNumModSources = 132;

// Mod targets: a flat list of program parameters that can be modulated.
inline const std::vector<int>& modTargetList()
{
    static const std::vector<int> t = {
        REV_INMIX, REV_PREDELAY, REV_PREMIX, REV_DECAY, REV_DIFF, REV_DENSITY, REV_LOWDEC, REV_HIGHDEC,
        DLY_INMIX, DLY_TIME, DLY_FB, DLY_TIME_R, DLY_FB_R,
        LFO_SPEED, LFO_DEPTH, PITCH_FB, DETUNE,
        ROT_SEP, ROT_MOTOR, ROT_SPEED, ROT_HILEVEL,
        EQ_LOW_FREQ, EQ_LOW_AMP, EQ_LOMID_FREQ, EQ_LOMID_BW, EQ_LOMID_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP,
        EQ_HIMID_FREQ, EQ_HIMID_BW, EQ_HIMID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP,
        GEQ_16, GEQ_32, GEQ_62, GEQ_126, GEQ_250, GEQ_500, GEQ_1K, GEQ_2K, GEQ_4K, GEQ_8K, GEQ_16K,
        MIX_DIRECT, MIX_MASTER, MIX_EQ, MIX_PITCH, MIX_DELAY, MIX_REVERB };
    return t;
}

const std::vector<ParamInfo>& paramTable();
inline const ParamInfo& info (int i) { return paramTable()[(size_t) i]; }

// Hardware-style value text for the LCD / host. `all` = full parameter array (context).
std::string formatValue (int index, float v, const float* all);
std::string modSourceName (int s);
std::string modTargetName (int listIndex);

// Function buttons / pages
enum Func { F_REVERB = 0, F_DELAY, F_PITCH, F_EQ, F_MIX, F_MOD, F_CONFIG, F_UTIL, NUM_FUNCS };
const char* funcName (Func f);
std::vector<int> pagesFor (Func f, const float* all);
bool functionActive (Func f, int config);

// Delay-time limits for a configuration/mode (ms)
inline float maxDelayMs (int config, int mode)
{
    const bool longCfg = (config == CFG_GRAPHIC || config == CFG_5BAND);
    if (mode == 0) return longCfg ? 1500.f : 800.f;
    return longCfg ? 750.f : 400.f;
}

struct Program
{
    std::string name = "INIT PROGRAM";
    std::array<float, NUM_PROGRAM_PARAMS> v {};
};
Program defaultProgram();
} // namespace qv
