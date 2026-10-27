#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Sysex.h"
#include "Rom.h"
#include "QvEdit.h"
#if QV89_HAS_ROM
 #include "BinaryData.h"
#endif

using namespace qv;

juce::AudioProcessorValueTreeState::ParameterLayout QV89Processor::createLayout (QV89Processor* self)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const auto& pi = info (i);
        juce::NormalisableRange<float> range (pi.minV, pi.maxV, pi.step);
        if (i == EQ_LOW_FREQ || i == EQ_LOMID_FREQ || i == EQ_MID_FREQ || i == EQ_HIMID_FREQ || i == EQ_HIGH_FREQ || i == DLY_TIME || i == DLY_TIME_R)
            range.setSkewForCentre (i == DLY_TIME || i == DLY_TIME_R ? 250.0f : std::sqrt (pi.minV * pi.maxV));

        auto toText = [i, self] (float v, int)
        {
            float ctx[NUM_PARAMS];
            if (self != nullptr && self->rawParams.size() == (size_t) NUM_PARAMS) self->currentValues (ctx);
            else for (int k = 0; k < NUM_PARAMS; ++k) ctx[k] = info (k).def;
            ctx[i] = v;
            return juce::String (formatValue (i, v, ctx));
        };
        auto fromText = [i] (const juce::String& t)
        {
            const auto& p = info (i);
            for (size_t c = 0; c < p.choices.size(); ++c)
                if (t.trim().equalsIgnoreCase (juce::String (p.choices[c]))) return (float) c;
            return t.retainCharacters ("-0123456789.").getFloatValue();
        };
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { pi.id, 1 }, juce::String (pi.lcd), range, pi.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
    }
    return layout;
}

QV89Processor::QV89Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "QV89", createLayout (this))
{
    for (int i = 0; i < NUM_PARAMS; ++i) rawParams.push_back (apvts.getRawParameterValue (info (i).id));
#if QV89_HAS_ROM
    {
        // Original QuadraVerb factory programs, extracted from the EPROM image embedded at build time
        std::vector<uint8_t> raw ((const uint8_t*) BinaryData::qv_rom_bin, (const uint8_t*) BinaryData::qv_rom_bin + BinaryData::qv_rom_binSize);
        std::string info;
        factory = rom::extractPrograms (raw, info);
        romInfo = info;
    }
#endif
    numRomPrograms = (int) factory.size();
    for (auto& p : factoryPrograms()) factory.push_back (p);
    work.resize (NUM_PARAMS);
    lastSent.resize (NUM_PROGRAM_PARAMS);
#if QV89_HAS_ROM
    romImage.assign ((const uint8_t*) BinaryData::qv_rom_bin, (const uint8_t*) BinaryData::qv_rom_bin + BinaryData::qv_rom_binSize);
#endif
    if (! romImage.empty()) emu.init (romImage.data(), romImage.size(), nullptr);
    loadProgram (factory[0]);
    syncLastSent();
    lastEmuActive = emulationActive();
    startTimerHz (15);
}

void QV89Processor::syncLastSent()
{
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) lastSent[(size_t) i] = getParam (i);
}

bool QV89Processor::emulationActive() const
{
    return emu.ready() && getParam (ENGINE) < 0.5f;
}

juce::String QV89Processor::loadRomImage (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() != 65536) return "NEED 64KB ROM";
    std::vector<uint8_t> raw ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    std::string info;
    if (rom::extractPrograms (raw, info).empty()) return "ROM NOT RECOGNISED";
    romImage = raw;
    suspendProcessing (true);
    const bool ok = emu.init (romImage.data(), romImage.size(), nullptr);
    emu.prepare (getSampleRate() > 0 ? getSampleRate() : 44100.0, juce::jmax (512, getBlockSize()));
    lastEditBuf.fill (0);
    syncLastSent();
    suspendProcessing (false);
    updateHostDisplay();
    return ok ? "ROM EMULATION READY" : "ROM BOOT FAILED";
}

void QV89Processor::sendProgramToEmu (const Program& p)
{
    auto msg = sysex::makeDump (p, 100);
    readbackHoldUntil = juce::Time::getMillisecondCounterHiRes() + 1200.0;
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) lastSent[(size_t) i] = getParam (i);
    emu.postSysex (msg);
}

void QV89Processor::timerCallback()
{
    if (emulationActive() != lastEmuActive)
    {
        lastEmuActive = emulationActive();
        setLatencySamples (lastEmuActive ? emu.latencySamples() : 0);
        lastEditBuf.fill (0);          // force a readback of the hardware edit buffer
        syncLastSent();
    }
    // Pull edits made on the emulated front panel back into the plugin parameters
    if (! emulationActive()) return;
    if (juce::Time::getMillisecondCounterHiRes() - lastProcessMs.load() > 250.0)
        emu.runIdle (1.0 / 15.0);      // host not processing (stopped / no device): keep the panel alive
    if (juce::Time::getMillisecondCounterHiRes() < readbackHoldUntil.load()) return;
    const auto panel = emu.panel();
    if (panel.serial == lastPanelSerial) return;
    lastPanelSerial = panel.serial;
    if (std::equal (panel.editBuf.begin(), panel.editBuf.end(), lastEditBuf.begin())) return;
    lastEditBuf = panel.editBuf;
    Program p; std::string w;
    sysex::decodeProgram (panel.editBuf.data(), p, w);
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i)
    {
        const float nv = p.v[(size_t) i];
        if (std::fabs (getParam (i) - nv) > 1e-3f)
        {
            lastSent[(size_t) i] = nv;
            setParam (i, nv);
        }
        lastSent[(size_t) i] = getParam (i);     // the value as the host parameter quantised it
    }
    if (programName.toStdString() != p.name) { programName = p.name; ++nameChangeCounter; }
}

bool QV89Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void QV89Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    if (emu.ready()) emu.prepare (sampleRate, juce::jmax (samplesPerBlock, 512));
    setLatencySamples (emulationActive() ? emu.latencySamples() : 0);
    emuTmpL.assign ((size_t) juce::jmax (samplesPerBlock, 512) * 2, 0.0f);
    emuTmpR.assign ((size_t) juce::jmax (samplesPerBlock, 512) * 2, 0.0f);
    monoTmp.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    currentValues (work.data());
    engine.setParams (work.data());
}

void QV89Processor::currentValues (float* dest) const
{
    for (int i = 0; i < NUM_PARAMS; ++i) dest[i] = rawParams[(size_t) i]->load();
}

void QV89Processor::setParam (int index, float value)
{
    if (auto* p = apvts.getParameter (info (index).id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void QV89Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (emulationActive()) { processEmulated (buffer, midi); return; }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isChannelPressure()) afterTouch = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch()) afterTouch = (float) m.getAfterTouchValue() / 127.0f;
        else if (m.isNoteOn()) { noteNumber = (float) m.getNoteNumber() / 127.0f; velocity = m.getFloatVelocity(); }
        else if (m.isController()) cc[m.getControllerNumber() & 127] = (float) m.getControllerValue() / 127.0f;
        else if (m.isProgramChange()) { pendingProgramChange = m.getProgramChangeNumber(); triggerAsyncUpdate(); }
    }

    currentValues (work.data());
    // --- MIDI modulation matrix ---
    const auto& targets = modTargetList();
    for (int s = 0; s < 8; ++s)
    {
        const float amp = work[(size_t) (MOD1_AMP + 3 * s)];
        if (amp == 0.0f) continue;
        const int src = (int) work[(size_t) (MOD1_SRC + 3 * s)];
        const int li = juce::jlimit (0, (int) targets.size() - 1, (int) work[(size_t) (MOD1_TGT + 3 * s)]);
        float sv = 0;
        switch (src)
        {
            case 0: sv = pitchBend; break;
            case 1: sv = afterTouch; break;
            case 2: sv = noteNumber; break;
            case 3: sv = velocity; break;
            default: sv = cc[(src - 4) & 127]; break;
        }
        const int t = targets[(size_t) li];
        const auto& pi = info (t);
        work[(size_t) t] = juce::jlimit (pi.minV, pi.maxV, work[(size_t) t] + amp / 99.0f * sv * (pi.maxV - pi.minV));
    }
    engine.setParams (work.data());

    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = nIn > 1 ? buffer.getReadPointer (1) : inL;
    // Engine reads inputs sample by sample before writing, so in-place processing is safe.
    float* outL = buffer.getWritePointer (0);
    float* outR = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    if (outR != nullptr) engine.process (inL, inR, outL, outR, n);
    else
    {
        if ((int) monoTmp.size() < n) monoTmp.resize ((size_t) n); // only if host exceeds the announced block size
        engine.process (inL, inR, outL, monoTmp.data(), n);
        for (int i = 0; i < n; ++i) outL[i] = 0.5f * (outL[i] + monoTmp[(size_t) i]);
    }
}

void QV89Processor::processEmulated (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // 1) host MIDI goes straight into the emulated MIDI IN port
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        std::vector<uint8_t> bytes (m.getRawData(), m.getRawData() + m.getRawDataSize());
        if (! bytes.empty()) emu.postSysex (bytes);
    }
    // 2) parameter changes -> QuadraVerb SysEx edits (like a hardware editor would send)
    currentValues (work.data());
    int changed = 0;
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i)
        if (! (work[(size_t) i] == lastSent[(size_t) i])) ++changed;
    if (changed > 8)
    {
        Program p; p.name = programName.toStdString();
        for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) p.v[(size_t) i] = work[(size_t) i];
        sendProgramToEmu (p);
    }
    else if (changed > 0)
    {
        // config first, so the following pages refer to the right configuration
        std::vector<int> order;
        if (! (work[CONFIG] == lastSent[CONFIG])) order.push_back (CONFIG);
        for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) if (i != CONFIG && ! (work[(size_t) i] == lastSent[(size_t) i])) order.push_back (i);
        for (int i : order)
        {
            if (auto msg = edit::editFor (i, work.data())) emu.postSysex (*msg);
            readbackHoldUntil = juce::Time::getMillisecondCounterHiRes() + 400.0;
            lastSent[(size_t) i] = work[(size_t) i];
        }
    }

    lastProcessMs = juce::Time::getMillisecondCounterHiRes();
    // 3) run the machine
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    const float gIn = juce::Decibels::decibelsToGain (work[IN_GAIN]);
    const float gOut = juce::Decibels::decibelsToGain (work[OUT_GAIN]);
    if ((int) emuTmpL.size() < n) { emuTmpL.resize ((size_t) n); emuTmpR.resize ((size_t) n); }
    float* l = buffer.getWritePointer (0);
    float* r = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inR = nIn > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0);
    float pkIn = 0;
    for (int i = 0; i < n; ++i)
    {
        emuTmpL[(size_t) i] = l[i] * gIn; emuTmpR[(size_t) i] = inR[i] * gIn;
        pkIn = juce::jmax (pkIn, std::fabs (0.5f * (emuTmpL[(size_t) i] + emuTmpR[(size_t) i])));
    }
    engine.inPeak.store (pkIn);
    emu.process (emuTmpL.data(), emuTmpR.data(), emuTmpL.data(), emuTmpR.data(), n);
    const bool byp = work[BYPASS] > 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float ol = byp ? l[i] : emuTmpL[(size_t) i] * gOut;
        const float orr = byp ? inR[i] : emuTmpR[(size_t) i] * gOut;
        if (r != nullptr) { l[i] = ol; r[i] = orr; } else l[i] = 0.5f * (ol + orr);
    }
}

void QV89Processor::handleAsyncUpdate()
{
    const int pc = pendingProgramChange.exchange (-1);
    if (pc >= 0 && pc < numBankPrograms()) selectBankProgram (pc);
}

void QV89Processor::setCurrentProgram (int index)
{
    if (index < 0 || index >= (int) factory.size()) return;
    currentProgram = index;
    selectBankProgram (index);
}

const juce::String QV89Processor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) factory.size()) ? juce::String (factory[(size_t) index].name) : juce::String();
}

juce::String QV89Processor::bankProgramName (int i) const
{
    if (emu.ready() && getParam (ENGINE) < 0.5f)
    {
        if (i < 100) { const auto p = emu.panel(); return juce::String (p.names[(size_t) i].data()).trimEnd(); }
        i -= 100;
        if (i < (int) factory.size() - numRomPrograms) return factory[(size_t) (numRomPrograms + i)].name;
        i -= (int) factory.size() - numRomPrograms;
        return juce::isPositiveAndBelow (i, (int) userBank.size()) ? juce::String (userBank[(size_t) i].name) : juce::String();
    }
    if (i < (int) factory.size()) return factory[(size_t) i].name;
    i -= (int) factory.size();
    return juce::isPositiveAndBelow (i, (int) userBank.size()) ? juce::String (userBank[(size_t) i].name) : juce::String();
}

void QV89Processor::selectBankProgram (int i)
{
    if (i < 0 || i >= numBankPrograms()) return;
    if (emulationActive() && i < 100)
    {
        // internal QuadraVerb memory: plain MIDI program change, exactly like the hardware
        currentBank = i;
        emu.postSysex ({ 0xC0, (uint8_t) i });
        return;
    }
    currentBank = i;
    if (emulationActive())
    {
        // originals / imported programs: sent to the emulated unit as a SysEx edit-buffer dump
        int j = i - 100;
        const int nOrig = (int) factory.size() - numRomPrograms;
        const Program& p = j < nOrig ? factory[(size_t) (numRomPrograms + j)] : userBank[(size_t) (j - nOrig)];
        loadProgram (p);
        sendProgramToEmu (p);
        return;
    }
    if (i < (int) factory.size()) { currentProgram = i; loadProgram (factory[(size_t) i]); }
    else loadProgram (userBank[(size_t) (i - (int) factory.size())]);
}

Program QV89Processor::captureProgram() const
{
    Program p;
    p.name = programName.toStdString();
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) p.v[(size_t) i] = getParam (i);
    return p;
}

void QV89Processor::loadProgram (const Program& p)
{
    // Config first so value-to-text contexts are right
    setParam (CONFIG, p.v[CONFIG]);
    for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) if (i != CONFIG) setParam (i, p.v[(size_t) i]);
    programName = p.name;
    ++nameChangeCounter;
}

juce::String QV89Processor::importRom (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return "CANNOT READ FILE";
    std::vector<uint8_t> raw ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    std::string info;
    auto progs = rom::extractPrograms (raw, info);
    if (progs.empty()) return "ROM NOT RECOGNISED";
    if (! emu.ready() && mb.getSize() == 65536) return loadRomImage (f);
    const int firstNew = numBankPrograms();
    for (auto& p : progs) userBank.push_back (p);
    selectBankProgram (firstNew);
    return juce::String ((int) progs.size()) + " ROM PROGRAMS";
}

juce::String QV89Processor::importSysex (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return "CANNOT READ FILE";
    std::vector<uint8_t> data ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    std::string log;
    auto progs = sysex::parseFile (data, log);
    if (progs.empty()) return juce::String ("NO QV PROGRAMS: ") + log;
    const int firstNew = numBankPrograms();
    for (auto& p : progs) userBank.push_back (p);
    selectBankProgram (firstNew);
    return juce::String ((int) progs.size()) + " PROGRAM(S) LOADED";
}

juce::String QV89Processor::exportSysex (const juce::File& f)
{
    auto msg = sysex::makeDump (captureProgram(), 100);
    return f.replaceWithData (msg.data(), msg.size()) ? "SYSEX SAVED" : "WRITE FAILED";
}

void QV89Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("programName", programName, nullptr);
    state.setProperty ("bankIndex", currentBank, nullptr);
    juce::ValueTree bank ("UserBank");
    for (auto& p : userBank)
    {
        juce::ValueTree t ("Program");
        t.setProperty ("name", juce::String (p.name), nullptr);
        juce::String vals;
        for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) vals << juce::String (p.v[(size_t) i]) << (i + 1 < NUM_PROGRAM_PARAMS ? "," : "");
        t.setProperty ("values", vals, nullptr);
        bank.appendChild (t, nullptr);
    }
    state.removeChild (state.getChildWithName ("UserBank"), nullptr);
    state.appendChild (bank, nullptr);
    if (emu.ready())
    {
        auto ram = emu.sramSnapshot();
        const auto pn = emu.panel();                       // freshest copy of the edit buffer
        if (ram.size() >= 0x380) std::copy (pn.editBuf.begin(), pn.editBuf.end(), ram.begin() + 0x300);
        juce::MemoryBlock mb (ram.data(), ram.size());
        state.setProperty ("qvSram", mb.toBase64Encoding(), nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void QV89Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;
    userBank.clear();
    auto bank = state.getChildWithName ("UserBank");
    for (auto t : bank)
    {
        Program p = defaultProgram();
        p.name = t["name"].toString().toStdString();
        juce::StringArray vals; vals.addTokens (t["values"].toString(), ",", "");
        for (int i = 0; i < juce::jmin (vals.size(), (int) NUM_PROGRAM_PARAMS); ++i) p.v[(size_t) i] = vals[i].getFloatValue();
        userBank.push_back (p);
    }
    programName = state["programName"].toString();
    currentBank = (int) state.getProperty ("bankIndex", 0);
    state.removeChild (bank, nullptr);
    const juce::String ramB64 = state["qvSram"].toString();
    state.removeProperty ("qvSram", nullptr);
    apvts.replaceState (state);
    if (emu.ready() && ramB64.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (ramB64))
        {
            std::vector<uint8_t> ram ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
            suspendProcessing (true);
            emu.restoreSram (ram);
            suspendProcessing (false);
        }
    }
    if (emu.ready())
    {
        // the firmware recalls the stored program at power-up: re-send the session's edit buffer
        Program p; p.name = programName.toStdString();
        for (int i = 0; i < NUM_PROGRAM_PARAMS; ++i) p.v[(size_t) i] = getParam (i);
        lastEditBuf.fill (0);
        sendProgramToEmu (p);
    }
    syncLastSent();
    ++nameChangeCounter;
}

juce::AudioProcessorEditor* QV89Processor::createEditor() { return new QV89Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QV89Processor(); }
