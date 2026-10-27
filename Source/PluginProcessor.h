#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/Engine.h"
#include "EmuEngine.h"

class QV89Processor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
{
public:
    QV89Processor();
    ~QV89Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QV89"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return (int) factory.size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- used by the editor ---
    juce::AudioProcessorValueTreeState apvts;
    float getParam (int index) const { return rawParams[(size_t) index]->load(); }
    void setParam (int index, float value);              // hardware units, notifies host
    void currentValues (float* dest) const;               // full NUM_PARAMS array

    int numBankPrograms() const
    {
        if (emulationActive()) return 100 + ((int) factory.size() - numRomPrograms) + (int) userBank.size();
        return (int) factory.size() + (int) userBank.size();
    }
    juce::String bankProgramName (int i) const;
    void selectBankProgram (int i);
    int currentBankProgram() const { return currentBank; }

    qv::Program captureProgram() const;
    void loadProgram (const qv::Program& p);
    juce::String programName;

    juce::String importSysex (const juce::File& f);      // returns a status message
    juce::String importRom (const juce::File& f);        // QuadraVerb EPROM image (.bin)
    int romProgramCount() const { return numRomPrograms; }
    juce::String romDescription() const { return romInfo; }
    juce::String exportSysex (const juce::File& f);
    void clearUserBank() { userBank.clear(); }
    int userBankSize() const { return (int) userBank.size(); }

    float inputPeak() const { return engine.inPeak.load(); }
    float outputPeak() const { return engine.outPeak.load(); }
    std::atomic<int> nameChangeCounter { 0 };

    // --- original-firmware emulation ---
    bool emulationActive() const;
    bool romAvailable() const { return emu.ready(); }
    EmuEngine::Panel emuPanel() const { return emu.panel(); }
    void emuKey (int key, bool down) { emu.postKey (key, down); }
    juce::String loadRomImage (const juce::File& f);

private:
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void processEmulated (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void sendProgramToEmu (const qv::Program& p);
    void syncLastSent();

    EmuEngine emu;
    std::vector<uint8_t> romImage;
    std::vector<float> lastSent, emuTmpL, emuTmpR;
    std::array<uint8_t, 128> lastEditBuf {};
    uint32_t lastPanelSerial = 0;
    bool lastEmuActive = false;
    std::atomic<double> readbackHoldUntil { 0.0 }, lastProcessMs { 0.0 };   // ms; no panel readback while our own edits are in flight
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (QV89Processor* self);

    qv::Engine engine;
    std::vector<std::atomic<float>*> rawParams;
    std::vector<qv::Program> factory, userBank;
    int currentProgram = 0, currentBank = 0, numRomPrograms = 0;
    juce::String romInfo;
    std::atomic<int> pendingProgramChange { -1 };

    // MIDI mod sources
    float pitchBend = 0, afterTouch = 0, noteNumber = 0, velocity = 0;
    float cc[128] {};
    std::vector<float> work, monoTmp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QV89Processor)
};
