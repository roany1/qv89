#include "../Source/emu/QvMachine.h"
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstring>
using namespace qvemu;
static void runFor (QvMachine& m, double secs) { int n=(int)(secs*QvMachine::kSampleRate); std::vector<float> x(n,0.f),l(n),r(n); m.process(x.data(),l.data(),r.data(),n); }
static void press (QvMachine& m, int c, int r, const char* tag) { m.setKeyMatrix(c,r,true); runFor(m,0.12); m.setKeyMatrix(c,r,false); runFor(m,0.3);
    std::printf("%-10s [%s] [%s]\n", tag, m.lcdLine(0).c_str(), m.lcdLine(1).c_str()); }
int main (int, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary); std::vector<uint8_t> rom ((std::istreambuf_iterator<char> (f)), {});
    QvMachine m; m.loadRom (rom.data(), rom.size()); m.powerOn (true); runFor (m, 3.2);
    press(m,0,0,"MIDI"); press(m,0,3,"c0r3"); press(m,0,1,"c0r1"); press(m,0,2,"c0r2"); press(m,0,2,"c0r2");
    press(m,3,2,"STORE"); press(m,0,3,"PAGE+"); press(m,0,3,"PAGE+"); press(m,0,3,"PAGE+"); press(m,3,2,"STORE");
    runFor (m, 1.0);
    std::printf("after: [%s] [%s]\n", m.lcdLine(0).c_str(), m.lcdLine(1).c_str());
    // find program names in SRAM
    const char* key = "EQ+CRS+DLY+RVB";
    for (size_t i = 0; i + 14 < m.sram.size(); ++i) if (std::memcmp (&m.sram[i], key, 14) == 0) std::printf ("found prog0 name at SRAM 0x%04zx (record base 0x%04zx)\n", i + 0x8000, i + 0x8000 - 0x6A);
    press(m,3,3,"PROGRAM"); press(m,1,3,"VALUE+"); press(m,1,3,"VALUE+");
    return 0;
}
