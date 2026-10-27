#include "../Source/emu/QvMachine.h"
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
using namespace qvemu;
static void runFor (QvMachine& m, double secs) { int n=(int)(secs*QvMachine::kSampleRate); std::vector<float> x(n,0.f),l(n),r(n); m.process(x.data(),l.data(),r.data(),n); }
int main (int, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary); std::vector<uint8_t> rom ((std::istreambuf_iterator<char> (f)), {});
    QvMachine m; m.loadRom (rom.data(), rom.size()); m.powerOn (true); runFor (m, 3.2);
    const char* names[16]={"MIDI","BYPASS","PAGE-","PAGE+","EQ","NAME","VAL-","VAL+","DELAY","MIX","CONFIG","REVERB","PITCH","MOD","STORE","PROGRAM"};
    for (int k = 0; k < 16; ++k)
    {
        int c = k / 4, r = k % 4;
        m.setKeyMatrix(c,r,true); runFor(m,0.08); m.setKeyMatrix(c,r,false); runFor(m,0.2);
        std::printf("%-8s L400=%02x L600=%02x L800=%02x  [%s]\n", names[k], m.ledLatch(1), m.ledLatch(2), m.ledLatch(3), m.lcdLine(0).c_str());
    }
    m.setKeyMatrix(0,1,true); runFor(m,0.08); m.setKeyMatrix(0,1,false); runFor(m,0.2);
    std::printf("BYPASS2  L400=%02x L600=%02x L800=%02x dry=%.2f\n", m.ledLatch(1), m.ledLatch(2), m.ledLatch(3), m.dryGain());
    return 0;
}
