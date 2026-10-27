#include "../Source/emu/QvMachine.h"
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
#include <cmath>
#include <chrono>
using namespace qvemu;
static void runFor (QvMachine& m, double secs, std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr, const std::vector<float>* in = nullptr)
{
    const int n = (int) (secs * QvMachine::kSampleRate);
    std::vector<float> x (n, 0.0f), l (n), r (n);
    if (in) for (int i = 0; i < n && i < (int) in->size(); ++i) x[i] = (*in)[i];
    m.process (x.data(), l.data(), r.data(), n);
    if (outL) *outL = l; if (outR) *outR = r;
}
int main (int argc, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary);
    std::vector<uint8_t> rom ((std::istreambuf_iterator<char> (f)), {});
    QvMachine m;
    if (! m.loadRom (rom.data(), rom.size())) { std::printf ("bad rom\n"); return 1; }
    m.powerOn (true);
    auto t0 = std::chrono::steady_clock::now();
    for (int s = 0; s < 6; ++s)
    {
        runFor (m, 0.5);
        std::printf ("t=%.1fs  [%s] [%s]\n", 0.5 * (s + 1), m.lcdLine (0).c_str(), m.lcdLine (1).c_str());
    }
    double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("3 s emulated in %.3f s (%.1fx realtime)\n", el, 3.0 / el);
    // key scan: press each matrix position and see LCD
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
        {
            m.setKeyMatrix (col, row, true); runFor (m, 0.15); m.setKeyMatrix (col, row, false); runFor (m, 0.25);
            std::printf ("key c%d r%d -> [%s] [%s]\n", col, row, m.lcdLine (0).c_str(), m.lcdLine (1).c_str());
        }
    return 0;
}
