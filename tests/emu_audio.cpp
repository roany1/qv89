#include "../Source/emu/QvMachine.h"
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
#include <cmath>
using namespace qvemu;
static std::vector<float> L, R;
static void runFor (QvMachine& m, double secs, const std::vector<float>* in = nullptr)
{ int n=(int)(secs*QvMachine::kSampleRate); std::vector<float> x(n,0.f),l(n),r(n); if(in) for(int i=0;i<n&&i<(int)in->size();++i) x[i]=(*in)[i];
  m.process(x.data(),l.data(),r.data(),n); L=l; R=r; }
static void press (QvMachine& m, int c, int r) { m.setKeyMatrix(c,r,true); runFor(m,0.08); m.setKeyMatrix(c,r,false); runFor(m,0.12); }
static double rt60 (const std::vector<float>& a, const std::vector<float>& b)
{ int n=(int)a.size(); std::vector<double> e(n+1,0); for(int i=n-1;i>=0;--i) e[i]=e[i+1]+a[i]*a[i]+b[i]*b[i]; if(e[0]<=0) return -1;
  int i5=-1,i25=-1; for(int i=0;i<n;++i){ double db=10*log10(e[i]/e[0]+1e-30); if(i5<0&&db<=-5) i5=i; if(i25<0&&db<=-25){i25=i;break;} } return (i5<0||i25<0)?-1:3.0*(i25-i5)/QvMachine::kSampleRate; }
int main (int, char** argv)
{
    std::ifstream f (argv[1], std::ios::binary); std::vector<uint8_t> rom ((std::istreambuf_iterator<char> (f)), {});
    QvMachine m; m.loadRom (rom.data(), rom.size()); m.powerOn (true); m.dspCore().accLimit = 8.0f; runFor (m, 3.2);
    press(m,3,2); press(m,0,3); press(m,0,3); press(m,0,3); press(m,3,2); runFor(m,1.0);   // recall all factory programs
    press(m,3,3);  // PROGRAM
    for (int p = 0; p < 90; ++p)
    {
        if (p > 0) press(m,1,3);       // VALUE+
        runFor (m, 0.4);                // settle
        // test signal: 50 ms noise burst at -12 dBFS then silence
        int n = (int) (4.0 * QvMachine::kSampleRate); std::vector<float> in (n, 0.f); unsigned s = 1;
        for (int i = 0; i < (int) (0.05 * QvMachine::kSampleRate); ++i) { s = s * 1664525u + 1013904223u; in[i] = 0.25f * ((float) (s >> 8) / 8388608.0f - 1.0f); }
        runFor (m, 4.0, &in);
        double e = 0, pk = 0; for (size_t i = 0; i < L.size(); ++i) { e += L[i]*L[i] + R[i]*R[i]; pk = std::max (pk, (double) std::fabs (L[i])); }
        std::printf ("%02d [%s] E=%.3f peak=%.3f rt=%.2f maxacc=%.2f\n", p, m.lcdLine(1).c_str(), e, pk, rt60(L,R), m.dspCore().peakAcc); m.dspCore().peakAcc = 0;
        m.clearClip();
    }
    return 0;
}
