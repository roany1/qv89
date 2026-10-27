// Headless test of the plugin processor in ROM-emulation mode.
#include <JuceHeader.h>
#define private public
#include "PluginProcessor.h"
#undef private
#include <cstdio>
#include "Sysex.h"
#include "QvEdit.h"

static float rms (const juce::AudioBuffer<float>& b, int ch) { return b.getRMSLevel (ch, 0, b.getNumSamples()); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    QV89Processor p;
    std::printf ("rom ready=%d emu=%d bank=%d\n", (int) p.romAvailable(), (int) p.emulationActive(), p.numBankPrograms());
    p.setPlayConfigDetails (2, 2, 48000, 512);
    p.prepareToPlay (48000, 512);
    std::printf ("latency=%d\n", p.getLatencySamples());
    juce::MidiBuffer midi;
    auto run = [&] (int blocks, bool impulse)
    {
        float tail = 0;
        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buf (2, 512); buf.clear();
            if (impulse && b == 0) { buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f); }
            p.processBlock (buf, midi);
            tail += rms (buf, 0) + rms (buf, 1);
        }
        return tail;
    };
    auto pumpUi = [&] (int ms) { for (int i = 0; i < ms / 10; ++i) { p.timerCallback(); run (2, false); } };
    run (20, false); pumpUi (300);
    auto show = [&] (const char* tag)
    {
        auto pn = p.emuPanel();
        qv::Program hw; std::string w; qv::sysex::decodeProgram (pn.editBuf.data(), hw, w);
        std::printf ("   [hw edit buffer: '%s' decay=%.0f type=%.0f cfg=%.0f]\n", hw.name.c_str(), hw.v[qv::REV_DECAY], hw.v[qv::REV_TYPE], hw.v[qv::CONFIG]);
        std::printf ("%-14s LCD [%.16s] [%.16s]  prog='%s' decay=%.1f type=%.0f cfg=%.0f\n", tag, pn.lcd[0], pn.lcd[1], p.programName.toRawUTF8(), p.getParam (qv::REV_DECAY), p.getParam (qv::REV_TYPE), p.getParam (qv::CONFIG));
    };
    show ("boot");
    std::printf ("impulse energy prog0 = %.3f\n", run (200, true));
    p.selectBankProgram (7); pumpUi (400); show ("prog 7");
    std::printf ("impulse energy prog7 = %.3f\n", run (200, true));
    std::printf ("name 0..4: "); for (int i = 0; i < 5; ++i) std::printf ("'%s' ", p.bankProgramName (i).toRawUTF8()); std::printf ("\n");
    // host parameter edit -> SysEx edit into the emulated unit
    p.setParam (qv::REV_DECAY, 20.0f); pumpUi (400); show ("decay=20");
    // front-panel key: PAGE+ on REVERB
    p.emuKey (qvemu::QvMachine::K_REVERB, true); pumpUi (100); p.emuKey (qvemu::QvMachine::K_REVERB, false); pumpUi (300); show ("REVERB key");
    p.emuKey (qvemu::QvMachine::K_VALUE_UP, true); pumpUi (100); p.emuKey (qvemu::QvMachine::K_VALUE_UP, false); pumpUi (300); show ("VALUE+");
    // state round trip
    for (int k = 0; k < 6; ++k) { p.emuKey (qvemu::QvMachine::K_PAGE_UP, true); pumpUi (60); p.emuKey (qvemu::QvMachine::K_PAGE_UP, false); pumpUi (150); show ("PAGE+"); }
    p.setParam (qv::REV_DECAY, 41.0f); pumpUi (300); show ("host decay=41");
    { std::vector<float> v (qv::NUM_PARAMS); p.currentValues (v.data()); auto m = qv::edit::editFor (qv::REV_DECAY, v.data());
      std::printf ("sram505=%d sram397=%d\n", p.emu.machine.sram[0x505], p.emu.machine.sram[0x397]);
      std::printf ("lastSent=%.1f msg:", p.lastSent[qv::REV_DECAY]); if (m) for (auto b : *m) std::printf (" %02X", b); std::printf ("\n"); }
    p.setParam (qv::CONFIG, 2.0f); pumpUi (300); show ("host cfg=2");
    p.selectBankProgram (100); pumpUi (400); show ("orig #0 dump"); pumpUi (2000); show ("+2s");
    juce::MemoryBlock st; p.getStateInformation (st);
    std::printf ("state bytes=%d\n", (int) st.getSize());
    if (auto x = juce::AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize())) std::printf ("saved programName='%s'\n", x->getStringAttribute ("programName").toRawUTF8());
    QV89Processor q; q.setStateInformation (st.getData(), (int) st.getSize());
    std::printf ("after setState: decay=%.1f prog='%s'\n", q.getParam (qv::REV_DECAY), q.programName.toRawUTF8());
    q.setPlayConfigDetails (2, 2, 48000, 512); q.prepareToPlay (48000, 512);
    for (int b = 0; b < 30; ++b) { juce::AudioBuffer<float> buf (2, 512); buf.clear(); q.processBlock (buf, midi); q.timerCallback(); }
    std::printf ("restored decay=%.1f prog='%s'\n", q.getParam (qv::REV_DECAY), q.programName.toRawUTF8());
    // model engine switch
    p.setParam (qv::ENGINE, 1.0f); pumpUi (100);
    std::printf ("model mode emu=%d energy=%.3f\n", (int) p.emulationActive(), run (200, true));
    return 0;
}
