#include "Params.h"
#include <algorithm>
#include <cstdlib>

namespace qv
{
static std::vector<std::string> offOn() { return { "OFF", "ON" }; }

static std::vector<std::string> modSourceChoices()
{
    std::vector<std::string> s = { "PITCH BEND", "AFTER TOUCH", "NOTE NUMBER", "NOTE VELOCITY" };
    char b[32];
    for (int i = 0; i < 128; ++i) { std::snprintf (b, sizeof b, "CONTROLLER %03d", i); s.push_back (b); }
    return s;
}

static std::vector<ParamInfo> buildTable()
{
    std::vector<ParamInfo> t;
    auto add = [&t] (int idx, const char* id, const char* lcd, float mn, float mx, float st, float def,
                     std::vector<std::string> ch = {})
    {
        if ((int) t.size() != idx) throw "param table order";
        t.push_back ({ idx, id, lcd, mn, mx, st, def, std::move (ch) });
    };

    add (CONFIG, "config", "CONFIGURATION", 0, 4, 1, 0,
         { "EQ>PCH>DL>REVERB", "ROTARY>DL>REVERB", "GRAPHIC EQ>DELAY", "5BAND EQ>PCH>DLY", "3BAND EQ>REVERB" });

    add (EQ_LOW_FREQ,   "eqLowFreq",   "LOW EQ FREQUENCY", 20, 999, 1, 200);
    add (EQ_LOW_AMP,    "eqLowAmp",    "LOW EQ AMPLITUDE", -14, 14, 0.05f, 0);
    add (EQ_LOMID_FREQ, "eqLoMidFreq", "LOW MID EQ FREQ",  20, 500, 1, 100);
    add (EQ_LOMID_BW,   "eqLoMidBw",   "LOW MID EQ WIDTH", 0.2f, 2.55f, 0.01f, 1.0f);
    add (EQ_LOMID_AMP,  "eqLoMidAmp",  "LOW MID EQ AMP",   -14, 14, 0.05f, 0);
    add (EQ_MID_FREQ,   "eqMidFreq",   "MID EQ FREQUENCY", 200, 9999, 1, 2000);
    add (EQ_MID_BW,     "eqMidBw",     "MID EQ BANDWIDTH", 0.2f, 2.55f, 0.01f, 1.0f);
    add (EQ_MID_AMP,    "eqMidAmp",    "MID EQ AMPLITUDE", -14, 14, 0.05f, 0);
    add (EQ_HIMID_FREQ, "eqHiMidFreq", "HIGH MID EQ FREQ", 2000, 18000, 1, 5000);
    add (EQ_HIMID_BW,   "eqHiMidBw",   "HIGH MID EQ WIDTH",0.2f, 2.55f, 0.01f, 1.0f);
    add (EQ_HIMID_AMP,  "eqHiMidAmp",  "HIGH MID EQ AMP",  -14, 14, 0.05f, 0);
    add (EQ_HIGH_FREQ,  "eqHighFreq",  "HI EQ FREQUENCY",  2000, 18000, 1, 8000);
    add (EQ_HIGH_AMP,   "eqHighAmp",   "HI EQ AMPLITUDE",  -14, 14, 0.05f, 0);

    const char* gIds[]  = { "geq16", "geq32", "geq62", "geq126", "geq250", "geq500", "geq1k", "geq2k", "geq4k", "geq8k", "geq16k" };
    const char* gLcd[]  = { "16Hz BOOST/CUT", "32Hz BOOST/CUT", "62Hz BOOST/CUT", "126Hz BOOST/CUT", "250Hz BOOST/CUT",
                            "500Hz BOOST/CUT", "1KHz BOOST/CUT", "2KHz BOOST/CUT", "4KHz BOOST/CUT", "8KHz BOOST/CUT", "16KHz BOOST/CUT" };
    for (int i = 0; i < 11; ++i) add (GEQ_16 + i, gIds[i], gLcd[i], -14, 14, 1, 0);

    add (PITCH_MODE, "pitchMode", "PITCH MODE", 0, 5, 1, 1,
         { "MONO CHORUS", "STEREO CHORUS", "MONO FLANGE", "STEREO FLANGE", "PITCH DETUNE", "PHASE SHIFTER" });
    add (PITCH_INPUT, "pitchInput", "PITCH INPUT", 0, 1, 1, 1, { "PRE-EQ", "POST-EQ" });
    add (LFO_WAVE,  "lfoWave",  "LFO WAVESHAPE", 0, 1, 1, 0, { "TRIANGLE", "SQUARE" });
    add (LFO_SPEED, "lfoSpeed", "LFO SPEED", 0, 99, 1, 20);
    add (LFO_DEPTH, "lfoDepth", "LFO DEPTH", 0, 99, 1, 50);
    add (PITCH_FB,  "pitchFb",  "PITCH FEEDBACK", 0, 99, 1, 0);
    add (TRIGGER_FLANGE, "trigFlange", "TRIGGER FLANGE", 0, 1, 1, 0, offOn());
    add (DETUNE, "detune", "DETUNE AMOUNT", -99, 99, 1, 5);
    add (REV_CHORUS, "revChorus", "REVERB CHORUS", 0, 1, 1, 1, offOn());

    add (ROT_SEP,     "rotSep",     "ROTARY STEREO SEP", 0, 99, 1, 80);
    add (ROT_MOTOR,   "rotMotor",   "ROTARY MOTOR", 0, 1, 1, 1, offOn());
    add (ROT_SPEED,   "rotSpeed",   "ROTARY SPEED", 0, 1, 1, 0, { "SLOW", "FAST" });
    add (ROT_HILEVEL, "rotHiLevel", "HIGH ROTOR LEVEL", -20, 6, 1, 0);

    add (DLY_MODE,   "dlyMode",   "DELAY TYPE", 0, 2, 1, 1, { "MONO DELAY", "STEREO DELAY", "PING PONG DELAY" });
    add (DLY_INPUT,  "dlyInput",  "DELAY INPUT 1", 0, 1, 1, 1, { "PRE-EQ", "POST-EQ" });
    add (DLY_INMIX,  "dlyInMix",  "DELAY INPUT MIX", -99, 99, 1, 0);
    add (DLY_TIME,   "dlyTime",   "DELAY TIME", 1, 1500, 1, 100);
    add (DLY_FB,     "dlyFb",     "DELAY FEEDBACK", 0, 99, 1, 0);
    add (DLY_TIME_R, "dlyTimeR",  "RIGHT DELAY TIME", 1, 750, 1, 100);
    add (DLY_FB_R,   "dlyFbR",    "DELAY FEEDBACK R", 0, 99, 1, 0);

    add (REV_TYPE, "revType", "REVERB TYPE", 0, 4, 1, 0, { "PLATE", "ROOM", "CHAMBER", "HALL", "REVERSE" });
    add (REV_IN1,  "revIn1",  "REVERB INPUT 1", 0, 3, 1, 3);
    add (REV_IN2,  "revIn2",  "REVERB INPUT 2", 0, 1, 1, 1);
    add (REV_INMIX,    "revInMix",    "REVERB INPUT MIX", -99, 99, 1, 0);
    add (REV_PREDELAY, "revPreDelay", "REVERB PREDELAY", 1, 140, 1, 40);
    add (REV_PREMIX,   "revPreMix",   "PRE-DELAY MIX", -99, 99, 1, 99);
    add (REV_DECAY,    "revDecay",    "REVERB DECAY", 0, 99, 1, 50);
    add (REV_DIFF,     "revDiff",     "REVERB DIFFUSION", 0, 8, 1, 8);
    add (REV_DENSITY,  "revDensity",  "REVERB DENSITY", 0, 8, 1, 8);
    add (REV_LOWDEC,   "revLowDec",   "LOW FREQ DECAY", -60, 0, 1, 0);
    add (REV_HIGHDEC,  "revHighDec",  "HIGH FREQ DECAY", -60, 0, 1, -20);
    add (GATE_ON,    "gateOn",    "REVERB GATE", 0, 1, 1, 0, offOn());
    add (GATE_HOLD,  "gateHold",  "GATE HOLD TIME", 0, 99, 1, 0);
    add (GATE_REL,   "gateRel",   "GATE RELEASE", 0, 99, 1, 80);
    add (GATE_LEVEL, "gateLevel", "REVERB GATED LVL", 0, 99, 1, 10);

    add (MIX_DIRECT_SEL, "mixDirectSel", "DIRECT SIGNAL SEL", 0, 1, 1, 0, { "PRE-EQ", "POST-EQ" });
    add (MIX_DIRECT, "mixDirect", "DIRECT SIGNAL LVL", 0, 99, 1, 50);
    add (MIX_MASTER, "mixMaster", "MASTER EFFECTS", 0, 99, 1, 99);
    add (MIX_EQ,     "mixEq",     "EQ OUTPUT LEVEL", 0, 99, 1, 50);
    add (MIX_PITCH,  "mixPitch",  "PITCH OUTPUT LVL", 0, 99, 1, 50);
    add (MIX_DELAY,  "mixDelay",  "DELAY OUTPUT LVL", 0, 99, 1, 50);
    add (MIX_REVERB, "mixReverb", "REVERB OUTPUT LVL", 0, 99, 1, 99);

    const auto srcs = modSourceChoices();
    std::vector<std::string> tg;
    for (size_t i = 0; i < modTargetList().size(); ++i) tg.push_back (modTargetName ((int) i));
    static const char* srcIds[] = { "mod1Src", "mod2Src", "mod3Src", "mod4Src", "mod5Src", "mod6Src", "mod7Src", "mod8Src" };
    static const char* tgtIds[] = { "mod1Tgt", "mod2Tgt", "mod3Tgt", "mod4Tgt", "mod5Tgt", "mod6Tgt", "mod7Tgt", "mod8Tgt" };
    static const char* ampIds[] = { "mod1Amp", "mod2Amp", "mod3Amp", "mod4Amp", "mod5Amp", "mod6Amp", "mod7Amp", "mod8Amp" };
    static const char* srcLcd[] = { "MOD 1 SOURCE", "MOD 2 SOURCE", "MOD 3 SOURCE", "MOD 4 SOURCE", "MOD 5 SOURCE", "MOD 6 SOURCE", "MOD 7 SOURCE", "MOD 8 SOURCE" };
    static const char* tgtLcd[] = { "MOD 1 TARGET", "MOD 2 TARGET", "MOD 3 TARGET", "MOD 4 TARGET", "MOD 5 TARGET", "MOD 6 TARGET", "MOD 7 TARGET", "MOD 8 TARGET" };
    static const char* ampLcd[] = { "MOD 1 AMPLITUDE", "MOD 2 AMPLITUDE", "MOD 3 AMPLITUDE", "MOD 4 AMPLITUDE", "MOD 5 AMPLITUDE", "MOD 6 AMPLITUDE", "MOD 7 AMPLITUDE", "MOD 8 AMPLITUDE" };
    for (int m = 0; m < 8; ++m)
    {
        add (MOD1_SRC + m * 3, srcIds[m], srcLcd[m], 0, (float) (kNumModSources - 1), 1, 0, srcs);
        add (MOD1_TGT + m * 3, tgtIds[m], tgtLcd[m], 0, (float) (tg.size() - 1), 1, 0, tg);
        add (MOD1_AMP + m * 3, ampIds[m], ampLcd[m], -99, 99, 1, 0);
    }

    add (IN_GAIN,  "inGain",  "INPUT LEVEL", -24, 24, 0.1f, 0);
    add (OUT_GAIN, "outGain", "OUTPUT LEVEL", -24, 12, 0.1f, 0);
    add (BYPASS,   "bypass",  "BYPASS", 0, 1, 1, 0, offOn());
    add (VINTAGE,  "vintage", "16-BIT VINTAGE", 0, 1, 1, 1, offOn());
    add (ENGINE,   "engine",  "DSP ENGINE", 0, 1, 1, 0, { "ROM EMULATION", "MODELLED" });
    return t;
}

const std::vector<ParamInfo>& paramTable()
{
    static const std::vector<ParamInfo> t = buildTable();
    return t;
}

std::string modSourceName (int s)
{
    if (s == 0) return "PITCH BEND";
    if (s == 1) return "AFTER TOUCH";
    if (s == 2) return "NOTE NUMBER";
    if (s == 3) return "NOTE VELOCITY";
    char b[32]; std::snprintf (b, sizeof b, "CONTROLLER %03d", s - 4); return b;
}

std::string modTargetName (int li)
{
    const auto& l = modTargetList();
    if (li < 0 || li >= (int) l.size()) return "?";
    switch (l[(size_t) li])
    {
        case REV_INMIX: return "REVERB INPUT MIX";  case REV_PREDELAY: return "REVERB PREDELAY";
        case REV_PREMIX: return "REV PREDELAY MIX"; case REV_DECAY: return "REVERB DECAY";
        case REV_DIFF: return "REVERB DIFFUSION";   case REV_DENSITY: return "REVERB DENSITY";
        case REV_LOWDEC: return "REVERB LOW DECAY"; case REV_HIGHDEC: return "REVERB HI DECAY";
        case DLY_INMIX: return "DELAY INPUT MIX";   case DLY_TIME: return "DELAY TIME";
        case DLY_FB: return "DELAY FEEDBACK";       case DLY_TIME_R: return "RIGHT DELAY TIME";
        case DLY_FB_R: return "RIGHT DELAY FB";     case LFO_SPEED: return "LFO SPEED";
        case LFO_DEPTH: return "LFO DEPTH";         case PITCH_FB: return "PITCH FEEDBACK";
        case DETUNE: return "DETUNE AMOUNT";        case ROT_SEP: return "ROTARY STEREO";
        case ROT_MOTOR: return "ROTARY MOTOR";      case ROT_SPEED: return "ROTARY SPEED";
        case ROT_HILEVEL: return "ROTARY HI LEVEL";
        case EQ_LOW_FREQ: return "LO EQ FREQUENCY"; case EQ_LOW_AMP: return "LO EQ BOOST/CUT";
        case EQ_LOMID_FREQ: return "LOW MID EQ FREQ"; case EQ_LOMID_BW: return "LOW MID EQ WIDTH";
        case EQ_LOMID_AMP: return "LOW MID EQ AMP"; case EQ_MID_FREQ: return "MID EQ FREQUENCY";
        case EQ_MID_BW: return "MID EQ BANDWIDTH";  case EQ_MID_AMP: return "MID EQ BOOST/CUT";
        case EQ_HIMID_FREQ: return "HIGH MID EQ FREQ"; case EQ_HIMID_BW: return "HIGH MID EQ WIDTH";
        case EQ_HIMID_AMP: return "HIGH MID EQ AMP"; case EQ_HIGH_FREQ: return "HI EQ FREQUENCY";
        case EQ_HIGH_AMP: return "HI EQ BOOST/CUT";
        case GEQ_16: return "16Hz BOOST/CUT";  case GEQ_32: return "32Hz BOOST/CUT";
        case GEQ_62: return "62Hz BOOST/CUT";  case GEQ_126: return "126Hz BOOST/CUT";
        case GEQ_250: return "250Hz BOOST/CUT"; case GEQ_500: return "500Hz BOOST/CUT";
        case GEQ_1K: return "1KHz BOOST/CUT";  case GEQ_2K: return "2KHz BOOST/CUT";
        case GEQ_4K: return "4KHz BOOST/CUT";  case GEQ_8K: return "8KHz BOOST/CUT";
        case GEQ_16K: return "16KHz BOOST/CUT";
        case MIX_DIRECT: return "DIRECT MIX LEVEL"; case MIX_MASTER: return "EFFECT MIX LEVEL";
        case MIX_EQ: return "EQ MIX LEVEL";         case MIX_PITCH: return "PITCH MIX LEVEL";
        case MIX_DELAY: return "DELAY MIX LEVEL";   case MIX_REVERB: return "REVERB MIX LEVEL";
        default: return "?";
    }
}

static std::string signed2 (int v)
{
    char b[16]; std::snprintf (b, sizeof b, "%c%02d", v < 0 ? '-' : '+', std::abs (v)); return b;
}

std::string formatValue (int idx, float v, const float* all)
{
    const auto& pi = info (idx);
    const int cfg = all ? (int) all[CONFIG] : 0;
    const int iv = (int) std::lround (v);
    char b[48];

    switch (idx)
    {
        case REV_IN1:
            if (cfg == CFG_ROTARY) { static const char* n[] = { "PRE-ROTARY", "ROTARY OUTPUT", "DELAY MIX INPUT" }; return n[std::min (iv, 2)]; }
            if (cfg == CFG_3BAND_REV) return iv ? "POST-EQ" : "PRE-EQ";
            { static const char* n[] = { "PRE-EQ", "POST-EQ", "PITCH OUTPUT", "DELAY MIX INPUT" }; return n[iv & 3]; }
        case REV_IN2:
            if (cfg == CFG_ROTARY) return iv ? "DELAY OUTPUT" : "ROTARY OUTPUT";
            return iv ? "DELAY OUTPUT" : "PITCH OUTPUT";
        case REV_INMIX:
            if (iv == 0) return "1 <00> 2";
            std::snprintf (b, sizeof b, "1 %s%02d%s 2", iv < 0 ? "<" : " ", std::abs (iv), iv > 0 ? ">" : " ");
            return b;
        case DLY_INMIX:
        {
            const char* rhs = cfg == CFG_ROTARY ? "ROTARY" : "PITCH";
            const char* lhs = cfg == CFG_ROTARY ? "IN" : "1";
            if (iv == 0) { std::snprintf (b, sizeof b, "%s <00> %s", lhs, rhs); return b; }
            std::snprintf (b, sizeof b, "%s %s%02d%s %s", lhs, iv < 0 ? "<" : " ", std::abs (iv), iv > 0 ? ">" : " ", rhs);
            return b;
        }
        case REV_PREMIX:
            if (iv == 0) return "PRE <00> POST";
            std::snprintf (b, sizeof b, "PRE %s%02d%s POST", iv < 0 ? "<" : " ", std::abs (iv), iv > 0 ? ">" : " ");
            return b;
        case REV_PREDELAY: std::snprintf (b, sizeof b, "%03d milliseconds", iv); return b;
        case DLY_TIME: case DLY_TIME_R:
        {
            const int mode = all ? (int) all[DLY_MODE] : 0;
            const int lim = (int) maxDelayMs (cfg, idx == DLY_TIME_R ? 1 : mode);
            std::snprintf (b, sizeof b, "%03d milliseconds", std::min (iv, lim)); return b;
        }
        case DLY_FB: case DLY_FB_R: case PITCH_FB: case GATE_LEVEL:
            std::snprintf (b, sizeof b, "%02d%%", iv); return b;
        case REV_DECAY:
            if (all && (int) all[REV_TYPE] == 4) { std::snprintf (b, sizeof b, "REV TIME %02d", iv); return b; }
            std::snprintf (b, sizeof b, "%02d", iv); return b;
        case REV_LOWDEC: case REV_HIGHDEC:
            std::snprintf (b, sizeof b, "-%02d", std::abs (iv)); return b;
        case REV_TYPE:
        {
            const bool two = cfg == CFG_3BAND_REV;
            std::snprintf (b, sizeof b, "%s %d", pi.choices[(size_t) (iv % 5)].c_str(), two ? 2 : 1); return b;
        }
        case EQ_LOW_FREQ: case EQ_LOMID_FREQ: case EQ_MID_FREQ:
            std::snprintf (b, sizeof b, "%04dHz", iv); return b;
        case EQ_HIMID_FREQ: case EQ_HIGH_FREQ:
            std::snprintf (b, sizeof b, "%05dHz", iv); return b;
        case EQ_LOW_AMP: case EQ_LOMID_AMP: case EQ_MID_AMP: case EQ_HIMID_AMP: case EQ_HIGH_AMP:
            std::snprintf (b, sizeof b, "%c%05.2fdB", v < 0 ? '-' : '+', std::fabs (v)); return b;
        case EQ_LOMID_BW: case EQ_MID_BW: case EQ_HIMID_BW:
            std::snprintf (b, sizeof b, "%.2f OCTAVES", v); return b;
        case ROT_HILEVEL:
            std::snprintf (b, sizeof b, "%s%02ddB", iv < 0 ? "-" : "+", std::abs (iv)); return b;
        case DETUNE: case MOD1_AMP: case MOD2_AMP: case MOD3_AMP: case MOD4_AMP:
        case MOD5_AMP: case MOD6_AMP: case MOD7_AMP: case MOD8_AMP:
            return signed2 (iv);
        case MIX_DIRECT: case MIX_MASTER: case MIX_EQ: case MIX_PITCH: case MIX_DELAY: case MIX_REVERB:
            std::snprintf (b, sizeof b, "+%02d", iv); return b;
        case IN_GAIN: case OUT_GAIN:
            std::snprintf (b, sizeof b, "%+.1f dB", v); return b;
        default: break;
    }
    if (idx >= GEQ_16 && idx <= GEQ_16K) { std::snprintf (b, sizeof b, "%+03d dB", iv); return b; }
    if (! pi.choices.empty())
    {
        const int c = std::max (0, std::min ((int) pi.choices.size() - 1, iv));
        return pi.choices[(size_t) c];
    }
    std::snprintf (b, sizeof b, "%02d", iv);
    return b;
}

const char* funcName (Func f)
{
    static const char* n[] = { "REVERB", "DELAY", "PITCH", "EQ", "MIX", "MOD", "CONFIG", "UTIL" };
    return n[f];
}

bool functionActive (Func f, int cfg)
{
    switch (f)
    {
        case F_REVERB: return cfg == CFG_QUAD || cfg == CFG_ROTARY || cfg == CFG_3BAND_REV;
        case F_DELAY:  return cfg != CFG_3BAND_REV;
        case F_PITCH:  return cfg != CFG_GRAPHIC;
        default: return true;
    }
}

std::vector<int> pagesFor (Func f, const float* p)
{
    const int cfg = (int) p[CONFIG];
    std::vector<int> v;
    switch (f)
    {
        case F_CONFIG: v = { CONFIG }; break;
        case F_UTIL:   v = { ENGINE, IN_GAIN, OUT_GAIN, VINTAGE, BYPASS }; break;
        case F_REVERB:
        {
            if (! functionActive (f, cfg)) break;
            const int type = (int) p[REV_TYPE];
            v.push_back (REV_TYPE);
            if (cfg == CFG_3BAND_REV) v.push_back (REV_IN1);
            else { v.push_back (REV_IN1); v.push_back (REV_IN2); v.push_back (REV_INMIX); }
            v.push_back (REV_PREDELAY); v.push_back (REV_PREMIX); v.push_back (REV_DECAY); v.push_back (REV_DIFF);
            if (type != 3) v.push_back (REV_DENSITY);   // HALL has no density page
            v.push_back (REV_LOWDEC); v.push_back (REV_HIGHDEC);
            if (type != 4) { v.push_back (GATE_ON); v.push_back (GATE_HOLD); v.push_back (GATE_REL); v.push_back (GATE_LEVEL); }
            break;
        }
        case F_DELAY:
        {
            if (! functionActive (f, cfg)) break;
            const int mode = (int) p[DLY_MODE];
            v.push_back (DLY_MODE);
            if (cfg != CFG_ROTARY) v.push_back (DLY_INPUT);
            if (cfg != CFG_GRAPHIC) v.push_back (DLY_INMIX);
            v.push_back (DLY_TIME); v.push_back (DLY_FB);
            if (mode == 1) { v.push_back (DLY_TIME_R); v.push_back (DLY_FB_R); }
            break;
        }
        case F_PITCH:
        {
            if (cfg == CFG_ROTARY) { v = { ROT_SEP, ROT_MOTOR, ROT_SPEED }; break; }
            if (cfg == CFG_3BAND_REV) { v = { REV_CHORUS, LFO_WAVE, LFO_SPEED, LFO_DEPTH }; break; }
            if (cfg == CFG_GRAPHIC) break;
            const int mode = (int) p[PITCH_MODE];
            v = { PITCH_MODE, PITCH_INPUT };
            if (mode <= 1) { v.push_back (LFO_WAVE); v.push_back (LFO_SPEED); v.push_back (LFO_DEPTH); v.push_back (PITCH_FB); }
            else if (mode <= 3) { v.push_back (LFO_SPEED); v.push_back (LFO_DEPTH); v.push_back (PITCH_FB); v.push_back (TRIGGER_FLANGE); }
            else if (mode == 4) { v.push_back (DETUNE); }
            else { v.push_back (LFO_SPEED); v.push_back (LFO_DEPTH); }
            break;
        }
        case F_EQ:
            if (cfg == CFG_ROTARY) v = { ROT_HILEVEL };
            else if (cfg == CFG_GRAPHIC) for (int i = 0; i < 11; ++i) v.push_back (GEQ_16 + i);
            else if (cfg == CFG_5BAND)
                v = { EQ_LOW_FREQ, EQ_LOW_AMP, EQ_LOMID_FREQ, EQ_LOMID_BW, EQ_LOMID_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP,
                      EQ_HIMID_FREQ, EQ_HIMID_BW, EQ_HIMID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP };
            else v = { EQ_LOW_FREQ, EQ_LOW_AMP, EQ_MID_FREQ, EQ_MID_BW, EQ_MID_AMP, EQ_HIGH_FREQ, EQ_HIGH_AMP };
            break;
        case F_MIX:
        {
            const bool post = p[MIX_DIRECT_SEL] > 0.5f;
            switch (cfg)
            {
                case CFG_QUAD:  v = { MIX_DIRECT_SEL, post ? MIX_EQ : MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }; break;
                case CFG_ROTARY: v = { MIX_MASTER, MIX_PITCH, MIX_DELAY, MIX_REVERB }; break;
                case CFG_GRAPHIC: v = { MIX_MASTER, MIX_EQ, MIX_DELAY }; break;
                case CFG_5BAND: v = { MIX_DIRECT_SEL, post ? MIX_EQ : MIX_DIRECT, MIX_MASTER, MIX_PITCH, MIX_DELAY }; break;
                default: v = { MIX_DIRECT_SEL, post ? MIX_EQ : MIX_DIRECT, MIX_MASTER, MIX_REVERB }; break;
            }
            break;
        }
        case F_MOD:
            for (int m = 0; m < 8; ++m) { v.push_back (MOD1_SRC + 3 * m); v.push_back (MOD1_TGT + 3 * m); v.push_back (MOD1_AMP + 3 * m); }
            break;
        default: break;
    }
    return v;
}

Program defaultProgram()
{
    Program p;
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) p.v[(size_t) i] = info (i).def;
    return p;
}
} // namespace qv
