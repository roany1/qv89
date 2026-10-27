#include "../Source/Rom.h"
#include "../Source/Sysex.h"
#include <fstream>
#include <iterator>
#include <cstdio>
using namespace qv;
int main(int, char** argv){
  std::ifstream f(argv[1], std::ios::binary); std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)),{});
  std::string msg; auto progs = rom::extractPrograms(raw, msg);
  std::printf("%s\n", msg.c_str());
  int bad=0, rt=0;
  for (size_t i=0;i<progs.size();++i){ auto&p=progs[i];
    float all[NUM_PARAMS]; for(int k=0;k<NUM_PARAMS;++k) all[k]= k<NUM_PROGRAM_PARAMS? p.v[k] : info(k).def;
    std::printf("%02zu %-14s cfg%d %-14s rev %-10s dec %-3s dly %-15s %s\n", i, p.name.c_str(), (int)p.v[CONFIG]+1,
      formatValue(PITCH_MODE,p.v[PITCH_MODE],all).c_str(), formatValue(REV_TYPE,p.v[REV_TYPE],all).c_str(),
      formatValue(REV_DECAY,p.v[REV_DECAY],all).c_str(), formatValue(DLY_TIME,p.v[DLY_TIME],all).c_str(), formatValue(MOD1_SRC,p.v[MOD1_SRC],all).c_str());
    // roundtrip encode->decode
    Program q; std::string w; auto enc = sysex::encodeProgram(p); sysex::decodeProgram(enc.data(), q, w);
    for(int k=0;k<NUM_PROGRAM_PARAMS;++k) if (std::fabs(q.v[k]-p.v[k])>0.03f) { ++rt; std::printf("   roundtrip diff %s %g vs %g\n", info(k).id, p.v[k], q.v[k]); }
  }
  // write a 90-program bank .syx
  std::vector<uint8_t> all; for (auto&p:progs){ auto e=sysex::encodeProgram(p); all.insert(all.end(), e.begin(), e.end()); }
  std::vector<uint8_t> m={0xF0,0,0,0x0E,0x02,0x02,101}; auto pk=sysex::pack7(all); m.insert(m.end(),pk.begin(),pk.end()); m.push_back(0xF7);
  std::ofstream o(argv[2], std::ios::binary); o.write((const char*)m.data(), m.size());
  std::string log; auto back = sysex::parseFile(m, log); std::printf("bank syx: %zu bytes, reparsed %zu programs, roundtrip diffs %d\n", m.size(), back.size(), rt);
  return 0; }
