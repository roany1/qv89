// Runs the emulated QuadraVerb (original firmware + DSP) inside the plugin, at its native
// 41.667 kHz, with sample-rate conversion to/from the host rate.
#pragma once
#include <JuceHeader.h>
#include "emu/QvMachine.h"
#include "emu/Resampler.h"

class EmuEngine
{
public:
    struct Panel
    {
        char lcd[2][17] {};
        int cursor = -1;
        uint8_t led400 = 0xFF, led600 = 0xFF, led800 = 0xFF;
        bool clip = false;
        float dry = 0;
        std::array<uint8_t, 128> editBuf {};
        std::array<std::array<char, 15>, 100> names {};
        uint32_t serial = 0;              // increments every audio block
    };

    struct BootFrame { float t = 0; std::string line0, line1; };
    std::vector<BootFrame> bootLcd() const { const juce::SpinLock::ScopedLockType l (machineLock); return bootFrames; }

    bool init (const uint8_t* rom, size_t size, const std::vector<uint8_t>* savedSram);
    bool ready() const { return isReady; }
    void prepare (double hostRate, int maxBlock);
    int latencySamples() const { return latency; }

    // audio thread
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);
    void midiFromHost (const uint8_t* data, int n);           // audio thread only

    // any thread (lock-free queues, consumed on the audio thread)
    void postKey (int key, bool down);
    void postSysex (const std::vector<uint8_t>& bytes);

    Panel panel() const { const juce::SpinLock::ScopedLockType l (panelLock); return shownPanel; }
    // Copies of battery RAM (message thread). Uses the machine lock.
    std::vector<uint8_t> sramSnapshot();
    void restoreSram (const std::vector<uint8_t>& sram);     // warm boot with given RAM
    void runIdle (double seconds);                            // message thread, when no audio is running

private:
    void bootFastForward (bool cold);
    void runMachineFor (double seconds);
    void consumeCommands();
    void publishPanel (float dry);

    qvemu::QvMachine machine;
    qvemu::Resampler inRs, outRsL, outRsR;
    std::vector<float> machIn, machL, machR, tmpOutL, tmpOutR, fifoL, fifoR;
    double hostRate = 44100;
    int latency = 0;
    bool isReady = false;
    mutable juce::SpinLock machineLock;
    std::vector<BootFrame> bootFrames;
    juce::SpinLock sramLock;
    std::vector<uint8_t> sramCopy;
    int sramCopyCountdown = 0;
    std::atomic<bool> processing { false };
    mutable juce::SpinLock panelLock;
    Panel shownPanel;

    // command queues (UI -> audio)
    juce::AbstractFifo keyFifo { 256 };
    std::array<std::pair<int, bool>, 256> keyBuf {};
    juce::AbstractFifo midiFifo { 1 << 15 };
    std::vector<uint8_t> midiBuf = std::vector<uint8_t> (1 << 15);
};
