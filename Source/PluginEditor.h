#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour panelTop   { 0xff2b2d31 };
    const juce::Colour panelBot   { 0xff17181b };
    const juce::Colour lower      { 0xff202226 };
    const juce::Colour text       { 0xffdcd7c9 };
    const juce::Colour dim        { 0xff8b877c };
    const juce::Colour amber      { 0xffe9a23b };
    const juce::Colour ledRed     { 0xffff3b2f };
    const juce::Colour ledGreen   { 0xff5cff6a };
    const juce::Colour lcdBg      { 0xff9fbf3e };
    const juce::Colour lcdInk     { 0xff18260a };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
};

class Lcd : public juce::Component
{
public:
    void setLines (const juce::String& a, const juce::String& b, int cursorLine = -1, int cursorCol = -1);
    void paint (juce::Graphics&) override;
private:
    juce::String l1, l2; int cLine = -1, cCol = -1;
};

class FuncButton : public juce::Button
{
public:
    explicit FuncButton (const juce::String& n) : juce::Button (n) {}
    bool lit = false, dimmed = false;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class ArrowButton : public juce::Button
{
public:
    ArrowButton (const juce::String& n, bool left) : juce::Button (n), isLeft (left) { setRepeatSpeed (350, 70, 25); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    bool isLeft;
};

class ParamKnob : public juce::Component
{
public:
    ParamKnob (QV89Processor& p, int index, std::function<void (int)> onTouch);
    void paint (juce::Graphics&) override;
    void resized() override;
    const int index;
    bool focused = false;
    juce::Slider slider;
private:
    QV89Processor& proc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class FlowStrip : public juce::Component
{
public:
    std::function<void (qv::Func)> onSelect;
    int config = 0; qv::Func selected = qv::F_REVERB; bool chorusTag = false;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    struct Block { juce::String name; qv::Func f; juce::Rectangle<float> r; };
    std::vector<Block> blocks;
    void layoutBlocks();
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (QV89Processor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1000, H = 650;

private:
    enum Mode { M_PROGRAM, M_FUNC, M_NAME };
    void timerCallback() override;
    void selectFunction (qv::Func f);
    void rebuildKnobs();
    void updateLcd();
    void page (int dir);
    void value (int dir, int repeatCount);
    void showStoreMenu();
    void fileAction (int id);
    void focusParam (int index);
    juce::String pagesSignature();
    void emuKeyState (int key, bool down);
    void updateEmuPanel();
    bool emu() const { return proc.emulationActive(); }

    QV89Processor& proc;
    LookAndFeel lnf;
    Lcd lcd;
    FlowStrip flow;
    juce::Slider inKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt;
    ArrowButton pageL { "pageL", true }, pageR { "pageR", false }, valL { "valL", true }, valR { "valR", false };
    juce::OwnedArray<FuncButton> funcButtons;
    juce::OwnedArray<ParamKnob> knobs;
    juce::OwnedArray<juce::TextButton> panelButtons;   // config chips / util actions
    juce::Label hint;
    juce::TextButton engineBtn, fileBtn { "FILE..." };
    bool keyDown[16] {};
    bool wasEmu = false;
    std::vector<EmuEngine::BootFrame> boot;
    double bootStartMs = 0;
    juce::Viewport programView;
    juce::Component programHolder;
    std::unique_ptr<juce::FileChooser> chooser;

    Mode mode = M_FUNC;
    qv::Func func = qv::F_REVERB;
    int focus = -1;          // param index focused on LCD
    int nameCursor = 0;
    juce::String sig, status; int statusTimer = 0;
    float ledLevel = 0; int clipHold = 0;
    int lastNameCounter = -1, repeatCount = 0;
    juce::Rectangle<int> ledArea, panelArea;
};
} // namespace ui

class QV89Editor : public juce::AudioProcessorEditor
{
public:
    explicit QV89Editor (QV89Processor&);
    void resized() override;
private:
    ui::Content content;
};
