// Original QV89 factory programs (not copies of the Alesis ROM programs).
#pragma once
#include "Params.h"
#include <functional>

namespace qv
{
inline std::vector<Program> factoryPrograms()
{
    std::vector<Program> list;
    auto make = [&list] (const char* name, std::function<void (std::array<float, NUM_PROGRAM_PARAMS>&)> f)
    {
        Program p = defaultProgram();
        p.name = name;
        f (p.v);
        list.push_back (p);
    };
    // helper to silence an effect in the QuadMode mix
    auto quadDefaults = [] (auto& v) { v[CONFIG] = CFG_QUAD; v[MIX_PITCH] = 0; v[MIX_DELAY] = 0; v[MIX_REVERB] = 99; v[MIX_DIRECT] = 99; };

    make ("BRIGHT PLATE", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 0; v[REV_DECAY] = 45; v[REV_PREDELAY] = 20; v[REV_IN1] = 1; v[REV_INMIX] = -99;
                                         v[REV_HIGHDEC] = -6; v[MIX_REVERB] = 70; v[EQ_HIGH_AMP] = 3; });
    make ("VOCAL PLATE", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 0; v[REV_DECAY] = 52; v[REV_PREDELAY] = 60; v[REV_PREMIX] = 60; v[REV_IN1] = 1; v[REV_INMIX] = -99;
                                        v[EQ_LOW_FREQ] = 180; v[EQ_LOW_AMP] = -6; v[REV_LOWDEC] = -20; v[MIX_REVERB] = 65; });
    make ("DRUM ROOM", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 1; v[REV_DECAY] = 30; v[REV_PREDELAY] = 8; v[REV_DIFF] = 6; v[REV_DENSITY] = 7;
                                      v[REV_IN1] = 0; v[REV_INMIX] = -99; v[MIX_REVERB] = 75; });
    make ("WOOD CHAMBER", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 2; v[REV_DECAY] = 48; v[REV_PREDELAY] = 25; v[REV_HIGHDEC] = -30;
                                         v[REV_IN1] = 0; v[REV_INMIX] = -99; v[MIX_REVERB] = 70; });
    make ("CONCERT HALL", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 3; v[REV_DECAY] = 62; v[REV_PREDELAY] = 35; v[REV_HIGHDEC] = -24; v[REV_LOWDEC] = -4;
                                         v[REV_IN1] = 0; v[REV_INMIX] = -99; v[MIX_REVERB] = 80; });
    make ("REVERSE SNARE", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 4; v[REV_DECAY] = 30; v[REV_PREDELAY] = 1; v[REV_DENSITY] = 6;
                                          v[REV_IN1] = 0; v[REV_INMIX] = -99; v[MIX_REVERB] = 99; });
    make ("GATED DRUMS", [&] (auto& v) { quadDefaults (v); v[REV_TYPE] = 1; v[REV_DECAY] = 75; v[REV_DIFF] = 8; v[REV_DENSITY] = 8; v[GATE_ON] = 1;
                                        v[GATE_HOLD] = 12; v[GATE_REL] = 8; v[GATE_LEVEL] = 0; v[REV_IN1] = 0; v[REV_INMIX] = -99; v[MIX_REVERB] = 99; });
    make ("CHORUS PLATE", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 1; v[LFO_SPEED] = 25; v[LFO_DEPTH] = 40; v[REV_TYPE] = 0; v[REV_DECAY] = 50;
                                         v[REV_IN1] = 2; v[REV_INMIX] = -99; v[MIX_DIRECT] = 99; v[MIX_PITCH] = 60; v[MIX_DELAY] = 0; v[MIX_REVERB] = 60; });
    make ("JET FLANGE", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 3; v[LFO_SPEED] = 18; v[LFO_DEPTH] = 80; v[PITCH_FB] = 75;
                                       v[MIX_DIRECT] = 50; v[MIX_PITCH] = 50; v[MIX_DELAY] = 0; v[MIX_REVERB] = 20; v[REV_IN1] = 2; v[REV_INMIX] = -99; });
    make ("TRIGGER FLANGE", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 2; v[LFO_SPEED] = 30; v[LFO_DEPTH] = 90; v[PITCH_FB] = 70; v[TRIGGER_FLANGE] = 1;
                                           v[MIX_DIRECT] = 50; v[MIX_PITCH] = 50; v[MIX_DELAY] = 0; v[MIX_REVERB] = 0; });
    make ("12 STRING", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 4; v[DETUNE] = 9; v[DLY_MODE] = 1; v[DLY_TIME] = 18; v[DLY_TIME_R] = 27; v[DLY_INMIX] = 99;
                                      v[MIX_DIRECT] = 70; v[MIX_PITCH] = 55; v[MIX_DELAY] = 40; v[MIX_REVERB] = 35; v[REV_TYPE] = 1; v[REV_DECAY] = 30; });
    make ("PHASE SPACE", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 5; v[LFO_SPEED] = 22; v[LFO_DEPTH] = 70; v[REV_TYPE] = 3; v[REV_DECAY] = 50;
                                        v[REV_IN1] = 2; v[REV_INMIX] = -99; v[MIX_DIRECT] = 60; v[MIX_PITCH] = 60; v[MIX_DELAY] = 0; v[MIX_REVERB] = 55; });
    make ("SLAPBACK", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[DLY_MODE] = 0; v[DLY_TIME] = 110; v[DLY_FB] = 8; v[DLY_INMIX] = -99; v[REV_TYPE] = 1; v[REV_DECAY] = 25;
                                     v[MIX_DIRECT] = 99; v[MIX_PITCH] = 0; v[MIX_DELAY] = 60; v[MIX_REVERB] = 30; });
    make ("PING PONG HALL", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[DLY_MODE] = 2; v[DLY_TIME] = 375; v[DLY_FB] = 45; v[DLY_INMIX] = -99; v[REV_TYPE] = 3; v[REV_DECAY] = 55;
                                           v[REV_IN1] = 3; v[REV_IN2] = 1; v[REV_INMIX] = 40; v[MIX_DIRECT] = 99; v[MIX_PITCH] = 0; v[MIX_DELAY] = 55; v[MIX_REVERB] = 60; });
    make ("STEREO ECHOES", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[DLY_MODE] = 1; v[DLY_TIME] = 330; v[DLY_TIME_R] = 250; v[DLY_FB] = 35; v[DLY_FB_R] = 35;
                                          v[PITCH_MODE] = 1; v[LFO_DEPTH] = 25; v[DLY_INMIX] = 50; v[MIX_DIRECT] = 99; v[MIX_PITCH] = 30; v[MIX_DELAY] = 55; v[MIX_REVERB] = 30; });
    make ("ROTARY SLOW", [&] (auto& v) { v[CONFIG] = CFG_ROTARY; v[ROT_SPEED] = 0; v[ROT_SEP] = 85; v[REV_TYPE] = 1; v[REV_DECAY] = 25; v[REV_IN1] = 1;
                                        v[MIX_PITCH] = 99; v[MIX_DELAY] = 0; v[MIX_REVERB] = 25;
                                        v[MOD1_SRC] = 4 + 1; v[MOD1_TGT] = 19; v[MOD1_AMP] = 99; }); // mod wheel (CC1) -> fast
    make ("ROTARY FAST", [&] (auto& v) { v[CONFIG] = CFG_ROTARY; v[ROT_SPEED] = 1; v[ROT_SEP] = 85; v[ROT_HILEVEL] = 2; v[REV_TYPE] = 1; v[REV_DECAY] = 25; v[REV_IN1] = 1;
                                        v[MIX_PITCH] = 99; v[MIX_DELAY] = 0; v[MIX_REVERB] = 25;
                                        v[MOD1_SRC] = 4 + 1; v[MOD1_TGT] = 19; v[MOD1_AMP] = -99; }); // mod wheel (CC1) -> slow
    make ("GRAPHIC ECHO", [&] (auto& v) { v[CONFIG] = CFG_GRAPHIC; v[GEQ_16] = -14; v[GEQ_32] = -10; v[GEQ_62] = -4; v[GEQ_2K] = 3; v[GEQ_4K] = 4;
                                         v[DLY_MODE] = 0; v[DLY_TIME] = 480; v[DLY_FB] = 40; v[MIX_EQ] = 99; v[MIX_DELAY] = 50; });
    make ("TELEPHONE", [&] (auto& v) { v[CONFIG] = CFG_GRAPHIC; for (int i = 0; i < 11; ++i) v[GEQ_16 + i] = -14; v[GEQ_1K] = 6; v[GEQ_2K] = 8; v[GEQ_500] = 0;
                                      v[MIX_EQ] = 99; v[MIX_DELAY] = 0; });
    make ("5BAND DOUBLER", [&] (auto& v) { v[CONFIG] = CFG_5BAND; v[EQ_LOMID_FREQ] = 250; v[EQ_LOMID_AMP] = -3; v[EQ_HIMID_FREQ] = 3500; v[EQ_HIMID_AMP] = 2.5f;
                                          v[PITCH_MODE] = 4; v[DETUNE] = -7; v[DLY_MODE] = 1; v[DLY_TIME] = 22; v[DLY_TIME_R] = 31; v[DLY_INMIX] = 99;
                                          v[MIX_DIRECT_SEL] = 1; v[MIX_EQ] = 99; v[MIX_PITCH] = 0; v[MIX_DELAY] = 60; });
    make ("TAPE ECHO 5BAND", [&] (auto& v) { v[CONFIG] = CFG_5BAND; v[EQ_LOW_AMP] = -8; v[EQ_HIGH_FREQ] = 4000; v[EQ_HIGH_AMP] = -10; v[DLY_INPUT] = 1;
                                            v[DLY_MODE] = 0; v[DLY_TIME] = 360; v[DLY_FB] = 55; v[PITCH_MODE] = 0; v[LFO_SPEED] = 10; v[LFO_DEPTH] = 8; v[DLY_INMIX] = 99; v[PITCH_INPUT] = 1;
                                            v[MIX_DIRECT_SEL] = 0; v[MIX_DIRECT] = 99; v[MIX_PITCH] = 0; v[MIX_DELAY] = 60; });
    make ("BIG HALL 2", [&] (auto& v) { v[CONFIG] = CFG_3BAND_REV; v[REV_TYPE] = 3; v[REV_DECAY] = 70; v[REV_PREDELAY] = 45; v[REV_IN1] = 1; v[REV_CHORUS] = 1;
                                       v[LFO_SPEED] = 15; v[LFO_DEPTH] = 30; v[REV_HIGHDEC] = -18; v[MIX_DIRECT] = 99; v[MIX_REVERB] = 75; });
    make ("SILK PLATE 2", [&] (auto& v) { v[CONFIG] = CFG_3BAND_REV; v[REV_TYPE] = 0; v[REV_DECAY] = 58; v[REV_PREDELAY] = 30; v[REV_IN1] = 1; v[REV_CHORUS] = 1;
                                         v[LFO_SPEED] = 20; v[LFO_DEPTH] = 45; v[EQ_LOW_AMP] = -5; v[EQ_HIGH_AMP] = 2; v[MIX_DIRECT] = 99; v[MIX_REVERB] = 70; });
    make ("AMBIENT WASH", [&] (auto& v) { v[CONFIG] = CFG_QUAD; v[PITCH_MODE] = 1; v[LFO_SPEED] = 12; v[LFO_DEPTH] = 55; v[DLY_MODE] = 2; v[DLY_TIME] = 400; v[DLY_FB] = 60;
                                         v[DLY_INMIX] = 99; v[REV_TYPE] = 3; v[REV_DECAY] = 85; v[REV_IN1] = 3; v[REV_IN2] = 1; v[REV_INMIX] = 0; v[REV_HIGHDEC] = -30;
                                         v[MIX_DIRECT] = 80; v[MIX_PITCH] = 30; v[MIX_DELAY] = 45; v[MIX_REVERB] = 90; });
    make ("INIT PROGRAM", [&] (auto&) {});
    return list;
}
} // namespace qv
