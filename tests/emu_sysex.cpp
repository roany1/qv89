#include "../Source/emu/QvMachine.h"
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
using namespace qvemu;
static void runFor (QvMachine& m, double secs) { int n=(int)(secs*QvMachine::kSampleRate); std::vector<float> x(n,0.f),l(n),r(n); m.process(x.data(),l.data(),r.data(),n); }
static void press (QvMachine& m, int c, int r, const char* tag=nullptr) { m.setKeyMatrix(c,r,true); runFor(m,0.08); m.setKeyMatrix(c,r,false); runFor(m,0.15);
  if (tag) std::printf("%-8s [%s] [%s]\n", tag, m.lcdLine(0).c_str(), m.lcdLine(1).c_str()); }
int main (int, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary); std::vector<uint8_t> rom ((std::istreambuf_iterator<char> (f)), {});
    QvMachine m; m.loadRom (rom.data(), rom.size()); m.powerOn (true); runFor (m, 3.2);
    press(m,0,0,"MIDI");
    for (int i = 0; i < 5; ++i) press(m,0,3,"PAGE+");
    auto before = m.sram;
    press(m,1,3,"VALUE+");
    for (size_t i = 0; i < m.sram.size(); ++i) if (before[i] != m.sram[i]) std::printf ("SRAM diff @%04zx: %02x -> %02x\n", i + 0x8000, before[i], m.sram[i]);
    // SysEx parameter edit: reverb (1) page 6 (decay) = 77 in QuadMode
    press(m,2,3,"REVERB");
    unsigned v = 77; uint8_t lsb = v & 0xFF, msb = (v >> 8) & 0xFF;
    uint8_t v1 = lsb >> 1, v2 = ((lsb & 1) << 6) | (msb >> 2), v3 = (msb & 3) << 5;
    std::vector<uint8_t> msg { 0xF0, 0x00, 0x00, 0x0E, 0x02, 0x01, 1, 6, v1, v2, v3, 0xF7 };
    std::printf("edit buf decay before: %d\n", m.sram[0x339]);
    m.midiIn (msg.data(), msg.size()); runFor (m, 0.3);
    std::printf("after sysex: [%s] [%s] decay=%d\n", m.lcdLine(0).c_str(), m.lcdLine(1).c_str(), m.sram[0x339]);
    // program change
    uint8_t pc[2] { 0xC0, 5 }; m.midiIn (pc, 2); runFor (m, 0.4);
    std::printf("after PC5: [%s] [%s]\n", m.lcdLine(0).c_str(), m.lcdLine(1).c_str());
    return 0;
}
