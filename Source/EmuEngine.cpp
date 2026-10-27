#include "EmuEngine.h"

using namespace qvemu;

void EmuEngine::runMachineFor (double seconds)
{
    const int n = (int) (seconds * QvMachine::kSampleRate);
    std::vector<float> x ((size_t) n, 0.0f), l ((size_t) n), r ((size_t) n);
    machine.process (x.data(), l.data(), r.data(), n);
}

void EmuEngine::bootFastForward (bool cold)
{
    machine.powerOn (cold);
    runMachineFor (3.4);                                  // boot banner
    auto press = [this] (int key) { machine.setKey (key, true); runMachineFor (0.08); machine.setKey (key, false); runMachineFor (0.15); };
    if (cold)
    {
        // STORE > PAGE+ x3 > STORE  = "RECALL ALL 90 ALESIS PROGRAMS"
        press (QvMachine::K_STORE); press (QvMachine::K_PAGE_UP); press (QvMachine::K_PAGE_UP); press (QvMachine::K_PAGE_UP); press (QvMachine::K_STORE);
        runMachineFor (1.0);
    }
    // SYSEX ENABLE must be switched on through the front panel: the firmware keeps a working
    // copy outside battery RAM, so poking the stored flag alone is not enough.
    press (QvMachine::K_PROGRAM);                         // leave any message screen first
    press (QvMachine::K_MIDI);
    for (int i = 0; i < 10 && machine.lcdLine (0).find ("SYSEX ENABLE") == std::string::npos; ++i) press (QvMachine::K_PAGE_UP);
    if (machine.lcdLine (0).find ("SYSEX ENABLE") != std::string::npos && machine.lcdLine (1).find ("OFF") != std::string::npos)
        press (QvMachine::K_VALUE_UP);
    press (QvMachine::K_PROGRAM);
    if (cold)
    {
        const uint8_t pc[2] = { 0xC0, 0x00 };
        machine.midiIn (pc, 2);
        runMachineFor (0.5);
    }
}

bool EmuEngine::init (const uint8_t* rom, size_t size, const std::vector<uint8_t>* savedSram)
{
    const juce::SpinLock::ScopedLockType l (machineLock);
    if (! machine.loadRom (rom, size)) { isReady = false; return false; }
    // key matrix (column, row) measured on the v1.07 firmware
    const int km[QvMachine::NUM_KEYS][2] = {
        { 3, 3 }, { 3, 2 }, { 2, 3 }, { 2, 0 }, { 3, 0 }, { 1, 0 }, { 0, 0 }, { 2, 2 }, { 2, 1 }, { 3, 1 }, { 1, 1 }, { 0, 1 },
        { 0, 3 }, { 0, 2 }, { 1, 3 }, { 1, 2 } };
    for (int k = 0; k < QvMachine::NUM_KEYS; ++k) { machine.keyMap[k][0] = km[k][0]; machine.keyMap[k][1] = km[k][1]; }
    if (savedSram != nullptr && savedSram->size() == machine.sram.size())
    {
        machine.sram = *savedSram;
        bootFastForward (false);
    }
    else bootFastForward (true);
    isReady = true;
    return true;
}

void EmuEngine::restoreSram (const std::vector<uint8_t>& sram)
{
    const juce::SpinLock::ScopedLockType l (machineLock);
    if (! isReady || sram.size() != machine.sram.size()) return;
    machine.sram = sram;
    bootFastForward (false);
}

void EmuEngine::prepare (double rate, int maxBlock)
{
    const juce::SpinLock::ScopedLockType l (machineLock);
    hostRate = rate;
    inRs.prepare (rate, QvMachine::kSampleRate, 16);
    outRsL.prepare (QvMachine::kSampleRate, rate, 16);
    outRsR.prepare (QvMachine::kSampleRate, rate, 16);
    const size_t cap = (size_t) (maxBlock * 4 + 4096);
    machIn.assign (cap, 0); machL.assign (cap, 0); machR.assign (cap, 0);
    tmpOutL.assign (cap, 0); tmpOutR.assign (cap, 0);
    // output FIFO primed with silence to absorb the +-1 sample jitter of the converters
    const int prime = 64;
    fifoL.assign ((size_t) prime, 0.0f); fifoR.assign ((size_t) prime, 0.0f);
    fifoL.reserve (cap * 2); fifoR.reserve (cap * 2);
    latency = prime + 16 + (int) std::ceil (16.0 * rate / QvMachine::kSampleRate);
}

void EmuEngine::postKey (int key, bool down)
{
    int s1, n1, s2, n2;
    keyFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { keyBuf[(size_t) s1] = { key, down }; keyFifo.finishedWrite (1); }
}

void EmuEngine::postSysex (const std::vector<uint8_t>& bytes)
{
    int s1, n1, s2, n2;
    midiFifo.prepareToWrite ((int) bytes.size(), s1, n1, s2, n2);
    if (n1 + n2 < (int) bytes.size()) return;
    std::copy (bytes.begin(), bytes.begin() + n1, midiBuf.begin() + s1);
    std::copy (bytes.begin() + n1, bytes.end(), midiBuf.begin() + s2);
    midiFifo.finishedWrite (n1 + n2);
}

void EmuEngine::midiFromHost (const uint8_t* data, int n)
{
    machine.midiIn (data, (size_t) n);
}

void EmuEngine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const juce::SpinLock::ScopedTryLockType l (machineLock);
    if (! l.isLocked() || ! isReady)
    {
        for (int i = 0; i < n; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; }
        return;
    }
    consumeCommands();

    // host -> 41.667 kHz (mono sum, as the hardware does before its converter)
    for (int i = 0; i < n; ++i) tmpOutL[(size_t) i] = 0.5f * (inL[i] + inR[i]);
    inRs.push (tmpOutL.data(), n);
    const int m = inRs.pull (machIn.data(), (int) machIn.size());
    if (m > 0)
    {
        machine.process (machIn.data(), machL.data(), machR.data(), m);
        outRsL.push (machL.data(), m);
        outRsR.push (machR.data(), m);
    }
    const int got = outRsL.pull (tmpOutL.data(), (int) tmpOutL.size());
    outRsR.pull (tmpOutR.data(), got);
    fifoL.insert (fifoL.end(), tmpOutL.begin(), tmpOutL.begin() + got);
    fifoR.insert (fifoR.end(), tmpOutR.begin(), tmpOutR.begin() + got);

    const int avail = (int) fifoL.size();
    const float dry = machine.dryGain();
    for (int i = 0; i < n; ++i)
    {
        const float wl = i < avail ? fifoL[(size_t) i] : 0.0f;
        const float wr = i < avail ? fifoR[(size_t) i] : 0.0f;
        outL[i] = wl + dry * inL[i];
        outR[i] = wr + dry * inR[i];
    }
    const int used = std::min (avail, n);
    fifoL.erase (fifoL.begin(), fifoL.begin() + used);
    fifoR.erase (fifoR.begin(), fifoR.begin() + used);
    // keep the FIFO from drifting (should never trigger)
    if (fifoL.size() > 4096) { fifoL.erase (fifoL.begin(), fifoL.end() - 64); fifoR.erase (fifoR.begin(), fifoR.end() - 64); }

    publishPanel (dry);
}

void EmuEngine::consumeCommands()
{
    int s1, n1, s2, n2;
    keyFifo.prepareToRead (keyFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) machine.setKey (keyBuf[(size_t) (s1 + i)].first, keyBuf[(size_t) (s1 + i)].second);
    for (int i = 0; i < n2; ++i) machine.setKey (keyBuf[(size_t) (s2 + i)].first, keyBuf[(size_t) (s2 + i)].second);
    keyFifo.finishedRead (n1 + n2);
    midiFifo.prepareToRead (midiFifo.getNumReady(), s1, n1, s2, n2);
    if (n1 > 0) machine.midiIn (midiBuf.data() + s1, (size_t) n1);
    if (n2 > 0) machine.midiIn (midiBuf.data() + s2, (size_t) n2);
    midiFifo.finishedRead (n1 + n2);
}

void EmuEngine::publishPanel (float dry)
{
    Panel p;
    for (int line = 0; line < 2; ++line)
    {
        const auto s = machine.lcdLine (line);
        for (int i = 0; i < 16; ++i) p.lcd[line][i] = s[(size_t) i];
    }
    p.cursor = machine.lcdCursor();
    p.led400 = machine.ledLatch (1); p.led600 = machine.ledLatch (2); p.led800 = machine.ledLatch (3);
    p.clip = machine.clip(); machine.clearClip();
    p.dry = dry;
    std::copy (machine.sram.begin() + 0x300, machine.sram.begin() + 0x380, p.editBuf.begin());
    for (int k = 0; k < 100; ++k)
    {
        const size_t base = (size_t) (0x4E00 + 128 * k + 0x6A);
        for (int i = 0; i < 14; ++i) { const char ch = (char) machine.sram[base + (size_t) i]; p.names[(size_t) k][(size_t) i] = (ch >= 32 && ch < 127) ? ch : ' '; }
        p.names[(size_t) k][14] = 0;
    }
    p.serial = shownPanel.serial + 1;
    if (--sramCopyCountdown <= 0)
    {
        sramCopyCountdown = 8;
        const juce::SpinLock::ScopedTryLockType sl (sramLock);
        if (sl.isLocked()) { sramCopy = machine.sram; processing = true; }
    }
    {
        const juce::SpinLock::ScopedTryLockType pl (panelLock);
        if (pl.isLocked()) { const bool c = shownPanel.clip; shownPanel = p; shownPanel.clip = p.clip || c; }
    }
}

void EmuEngine::runIdle (double seconds)
{
    // keeps the front panel alive when the host is not calling processBlock
    const juce::SpinLock::ScopedLockType l (machineLock);
    if (! isReady) return;
    consumeCommands();
    const int n = (int) (seconds * QvMachine::kSampleRate);
    if ((int) machIn.size() < n) { machIn.resize ((size_t) n); machL.resize ((size_t) n); machR.resize ((size_t) n); }
    std::fill (machIn.begin(), machIn.begin() + n, 0.0f);
    machine.process (machIn.data(), machL.data(), machR.data(), n);
    publishPanel (machine.dryGain());
}

std::vector<uint8_t> EmuEngine::sramSnapshot()
{
    {
        const juce::SpinLock::ScopedLockType l (sramLock);
        if (processing.load() && ! sramCopy.empty()) return sramCopy;
    }
    const juce::SpinLock::ScopedLockType l (machineLock);
    return machine.sram;
}
