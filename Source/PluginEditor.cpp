#include "PluginEditor.h"
#include <map>

using namespace qv;

namespace ui
{
static juce::Font uiFont (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}
static juce::Font lcdFont (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold));
}

// ============================================================================ LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff34363b));
    setColour (juce::TextButton::buttonOnColourId, col::amber.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141518));
    setColour (juce::ListBox::outlineColourId, juce::Colour (0xff3a3c42));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff25272b));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber);
    setColour (juce::ScrollBar::thumbColourId, col::dim);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    // value arc
    juce::Path track; track.addCentredArc (c.x, c.y, r - 1, r - 1, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff0d0e10)); g.strokePath (track, juce::PathStrokeType (3.0f));
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zeroPos = bipolar ? (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : 0.0f;
    const float az = a0 + zeroPos * (a1 - a0);
    juce::Path val; val.addCentredArc (c.x, c.y, r - 1, r - 1, 0, juce::jmin (az, ang), juce::jmax (az, ang), true);
    g.setColour (col::amber); g.strokePath (val, juce::PathStrokeType (3.0f));

    // knob body
    const float kr = r * 0.74f;
    juce::ColourGradient grad (juce::Colour (0xff4a4c52), c.x - kr * 0.5f, c.y - kr, juce::Colour (0xff111214), c.x + kr * 0.4f, c.y + kr, false);
    g.setGradientFill (grad); g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colour (0xff050506)); g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.2f);
    // ribs
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = (float) i / 24.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * kr * 0.82f, c.y + std::sin (a) * kr * 0.82f, c.x + std::cos (a) * kr, c.y + std::sin (a) * kr, 1.0f);
    }
    // pointer
    juce::Path p; p.addRoundedRectangle (-1.5f, -kr * 0.92f, 3.0f, kr * 0.5f, 1.5f);
    g.setColour (col::text); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    juce::Colour base = on ? col::amber.withAlpha (0.9f) : juce::Colour (0xff303237);
    if (over) base = base.brighter (0.12f);
    if (down) base = base.darker (0.2f);
    g.setColour (base); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return uiFont (juce::jmin (13.0f, (float) h * 0.45f)); }

// ============================================================================ LCD
void Lcd::setLines (const juce::String& a, const juce::String& b, int cl, int cc)
{
    if (a == l1 && b == l2 && cl == cLine && cc == cCol) return;
    l1 = a; l2 = b; cLine = cl; cCol = cc; repaint();
}

void Lcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0c0d)); g.fillRoundedRectangle (r, 5.0f);
    auto glass = r.reduced (6.0f);
    juce::ColourGradient bg (col::lcdBg.brighter (0.15f), glass.getX(), glass.getY(), col::lcdBg.darker (0.15f), glass.getX(), glass.getBottom(), false);
    g.setGradientFill (bg); g.fillRoundedRectangle (glass, 3.0f);

    const int cols = 16;
    const float cw = (glass.getWidth() - 10.0f) / (float) cols;
    const float ch = (glass.getHeight() - 10.0f) / 2.0f;
    auto font = lcdFont (ch * 0.86f);
    {
        // keep every glyph inside its cell whatever monospaced font the OS provides
        juce::GlyphArrangement ga; ga.addLineOfText (font, "W", 0.0f, 0.0f);
        const float gw = ga.getBoundingBox (0, -1, true).getWidth();
        if (gw > cw * 0.92f) font.setHorizontalScale (cw * 0.92f / gw);
    }
    for (int line = 0; line < 2; ++line)
    {
        const juce::String& s = line == 0 ? l1 : l2;
        for (int i = 0; i < cols; ++i)
        {
            const float cx = glass.getX() + 5.0f + (float) i * cw, cy = glass.getY() + 5.0f + (float) line * ch;
            g.setColour (col::lcdInk.withAlpha (0.06f));
            g.fillRect (cx + 1.0f, cy + 2.0f, cw - 2.0f, ch - 4.0f);   // ghost character cell
            if (i < s.length() && s[i] != ' ')
            {
                // drawSingleLineText never drops a glyph that is wider than its cell
                // (drawText does, which left the LCD blank with wide monospaced fonts such as Consolas)
                g.setColour (col::lcdInk);
                g.setFont (font);
                const float baseline = cy + (ch + font.getAscent() - font.getDescent()) * 0.5f;
                g.drawSingleLineText (juce::String::charToString (s[i]), juce::roundToInt (cx + cw * 0.5f), juce::roundToInt (baseline),
                                      juce::Justification::horizontallyCentred);
            }
            if (line == cLine && i == cCol)
            {
                g.setColour (col::lcdInk);
                g.fillRect (cx + 2.0f, cy + ch - 4.0f, cw - 4.0f, 2.0f);
            }
        }
    }
}

// ============================================================================ Buttons
void FuncButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromTop (14.0f);
    g.setColour (dimmed ? col::dim.withAlpha (0.6f) : col::text);
    g.setFont (uiFont (10.5f));
    g.drawText (getName(), label, juce::Justification::centred, false);

    auto key = r.reduced (3.0f, 2.0f);
    juce::Colour k (0xff3a3c41);
    if (over) k = k.brighter (0.1f);
    if (down) k = k.darker (0.25f);
    juce::ColourGradient grad (k.brighter (0.15f), key.getX(), key.getY(), k.darker (0.3f), key.getX(), key.getBottom(), false);
    g.setGradientFill (grad); g.fillRoundedRectangle (key, 4.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (key, 4.0f, 1.0f);
    const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre (key.getCentre());
    if (lit)
    {
        g.setColour (col::ledRed.withAlpha (0.35f)); g.fillEllipse (led.expanded (4.0f));
        g.setColour (col::ledRed);
    }
    else g.setColour (juce::Colour (0xff4a1512));
    g.fillEllipse (led);
}

void ArrowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto key = getLocalBounds().toFloat().reduced (2.0f);
    juce::Colour k (0xff3a3c41);
    if (over) k = k.brighter (0.1f);
    if (down) k = k.darker (0.25f);
    g.setColour (k); g.fillRoundedRectangle (key, 4.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (key, 4.0f, 1.0f);
    juce::Path tri;
    const auto c = key.getCentre(); const float s = key.getHeight() * 0.22f;
    if (isLeft) tri.addTriangle (c.x - s, c.y, c.x + s, c.y - s, c.x + s, c.y + s);
    else tri.addTriangle (c.x + s, c.y, c.x - s, c.y - s, c.x - s, c.y + s);
    g.setColour (col::text); g.fillPath (tri);
}

// ============================================================================ ParamKnob
ParamKnob::ParamKnob (QV89Processor& p, int idx, std::function<void (int)> onTouch) : index (idx), proc (p)
{
    const bool vertical = idx >= GEQ_16 && idx <= GEQ_16K;
    slider.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::trackColourId, col::amber);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff0d0e10));
    slider.setColour (juce::Slider::thumbColourId, col::text);
    slider.setDoubleClickReturnValue (true, info (idx).def);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, info (idx).id, slider);
    slider.onDragStart = [onTouch, idx] { onTouch (idx); };
    slider.onValueChange = [this, onTouch, idx] { onTouch (idx); repaint(); };
    addAndMakeVisible (slider);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromTop (18); r.removeFromBottom (18);
    slider.setBounds (r);
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (focused ? juce::Colour (0xff2e2a22) : juce::Colour (0xff1a1b1e));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (focused ? col::amber : juce::Colour (0xff2c2e33));
    g.drawRoundedRectangle (r, 6.0f, focused ? 1.5f : 1.0f);

    float ctx[NUM_PARAMS]; proc.currentValues (ctx);
    g.setColour (col::dim); g.setFont (uiFont (10.0f));
    g.drawFittedText (info (index).lcd, getLocalBounds().reduced (4, 3).removeFromTop (16), juce::Justification::centred, 1, 0.7f);
    g.setColour (focused ? col::amber : col::text); g.setFont (uiFont (11.0f));
    g.drawFittedText (formatValue (index, ctx[index], ctx), getLocalBounds().reduced (4, 3).removeFromBottom (16), juce::Justification::centred, 1, 0.6f);
}

// ============================================================================ FlowStrip
void FlowStrip::layoutBlocks()
{
    blocks.clear();
    std::vector<std::pair<juce::String, Func>> names;
    switch (config)
    {
        case CFG_QUAD:      names = { { "3-BAND EQ", F_EQ }, { "PITCH", F_PITCH }, { "DELAY", F_DELAY }, { "REVERB", F_REVERB } }; break;
        case CFG_ROTARY:    names = { { "ROTARY SPKR", F_PITCH }, { "DELAY", F_DELAY }, { "REVERB", F_REVERB } }; break;
        case CFG_GRAPHIC:   names = { { "11-BAND GRAPHIC", F_EQ }, { "DELAY", F_DELAY } }; break;
        case CFG_5BAND:     names = { { "5-BAND EQ", F_EQ }, { "PITCH", F_PITCH }, { "DELAY", F_DELAY } }; break;
        default:            names = { { "3-BAND EQ", F_EQ }, { chorusTag ? "REVERB 2 + CHORUS" : "REVERB 2", F_REVERB } }; break;
    }
    names.push_back ({ "MIX", F_MIX });
    const float w = (float) getWidth(), h = (float) getHeight();
    const float left = 70.0f, right = 60.0f, gap = 34.0f;
    const float bw = (w - left - right - gap * (float) (names.size() - 1)) / (float) names.size();
    for (size_t i = 0; i < names.size(); ++i)
        blocks.push_back ({ names[i].first, names[i].second, { left + (float) i * (bw + gap), h * 0.22f, bw, h * 0.56f } });
}

void FlowStrip::paint (juce::Graphics& g)
{
    layoutBlocks();
    const float h = (float) getHeight(), w = (float) getWidth();
    g.setColour (col::dim); g.setFont (uiFont (11.0f));
    g.drawText ("IN", juce::Rectangle<float> (10, 0, 40, h), juce::Justification::centred);
    g.drawText ("OUT", juce::Rectangle<float> (w - 50, 0, 44, h), juce::Justification::centred);
    g.setColour (juce::Colour (0xff55575d));
    g.drawLine (44, h * 0.5f, w - 50, h * 0.5f, 2.0f);
    // direct path
    if (config != CFG_ROTARY)
    {
        g.setColour (juce::Colour (0xff3c3e44));
        const float y = h * 0.9f;
        g.drawLine (44, h * 0.5f, 44, y, 1.0f); g.drawLine (44, y, blocks.back().r.getX() + 10, y, 1.0f);
        g.drawLine (blocks.back().r.getX() + 10, y, blocks.back().r.getX() + 10, blocks.back().r.getBottom(), 1.0f);
        g.setColour (col::dim); g.setFont (uiFont (9.0f));
        g.drawText ("DIRECT", juce::Rectangle<float> (52, y - 11, 60, 10), juce::Justification::left);
    }
    for (auto& b : blocks)
    {
        const bool sel = b.f == selected;
        g.setColour (sel ? juce::Colour (0xff3a3122) : juce::Colour (0xff26282c));
        g.fillRoundedRectangle (b.r, 5.0f);
        g.setColour (sel ? col::amber : juce::Colour (0xff4a4c52));
        g.drawRoundedRectangle (b.r, 5.0f, sel ? 2.0f : 1.0f);
        g.setColour (sel ? col::amber : col::text); g.setFont (uiFont (12.0f));
        g.drawFittedText (b.name, b.r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void FlowStrip::mouseUp (const juce::MouseEvent& e)
{
    for (auto& b : blocks)
        if (b.r.contains (e.position) && onSelect) { onSelect (b.f); return; }
}

// ============================================================================ Content
static const char* kFuncLabels[12] = { "PROG", "STORE", "REVERB", "DELAY", "PITCH", "EQ",
                                       "UTIL", "CONFIG", "MIX", "MOD", "NAME", "BYPASS" };

Content::Content (QV89Processor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (lcd);
    addAndMakeVisible (flow);
    flow.onSelect = [this] (Func f) { selectFunction (f); };

    for (auto* k : { &inKnob, &outKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (*k);
    }
    inKnob.setDoubleClickReturnValue (true, 0.0); outKnob.setDoubleClickReturnValue (true, 0.0);
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, info (IN_GAIN).id, inKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, info (OUT_GAIN).id, outKnob);
    inKnob.onValueChange = [this] { focusParam (IN_GAIN); };
    outKnob.onValueChange = [this] { focusParam (OUT_GAIN); };

    for (auto* b : { &pageL, &pageR, &valL, &valR }) addAndMakeVisible (*b);
    programView.setViewedComponent (&programHolder, false);
    programView.setScrollBarsShown (true, false);
    addChildComponent (programView);
    pageL.onClick = [this] { if (! emu()) page (-1); };
    pageR.onClick = [this] { if (! emu()) page (+1); };
    valL.onClick = [this] { if (! emu()) value (-1, ++repeatCount); };
    valR.onClick = [this] { if (! emu()) value (+1, ++repeatCount); };
    auto resetRepeat = [this] (juce::Button& b) { if (b.getState() == juce::Button::buttonNormal || b.getState() == juce::Button::buttonOver) repeatCount = 0; };
    // in ROM-emulation mode the arrows are the real keys: the original firmware does its own auto-repeat
    using QK = qvemu::QvMachine;
    pageL.onStateChange = [this] { emuKeyState (QK::K_PAGE_DOWN, pageL.getState() == juce::Button::buttonDown); };
    pageR.onStateChange = [this] { emuKeyState (QK::K_PAGE_UP, pageR.getState() == juce::Button::buttonDown); };
    valL.onStateChange = [this, resetRepeat] { resetRepeat (valL); emuKeyState (QK::K_VALUE_DOWN, valL.getState() == juce::Button::buttonDown); };
    valR.onStateChange = [this, resetRepeat] { resetRepeat (valR); emuKeyState (QK::K_VALUE_UP, valR.getState() == juce::Button::buttonDown); };

    engineBtn.onClick = [this] { proc.setParam (ENGINE, proc.getParam (ENGINE) > 0.5f ? 0.0f : 1.0f); };
    fileBtn.onClick = [this] { showStoreMenu(); };
    engineBtn.setTooltip ("ROM = original QuadraVerb v1.07 firmware + DSP emulation; MODEL = QV89 modelled engine");
    addAndMakeVisible (engineBtn); addAndMakeVisible (fileBtn);

    for (int i = 0; i < 12; ++i)
    {
        auto* b = funcButtons.add (new FuncButton (kFuncLabels[i]));
        addAndMakeVisible (b);
        // key index == button index (PROG, STORE, REVERB, DELAY, PITCH, EQ, MIDI, CONFIG, MIX, MOD, NAME, BYPASS)
        b->onStateChange = [this, i, b] { emuKeyState (i, b->getState() == juce::Button::buttonDown); };
        b->onClick = [this, i]
        {
            if (emu())
            {
                // knobs follow the function pressed on the emulated panel
                const Func fm[12] = { (Func) -1, (Func) -1, F_REVERB, F_DELAY, F_PITCH, F_EQ, (Func) -1, F_CONFIG, F_MIX, F_MOD, (Func) -1, (Func) -1 };
                if (i == 0) { mode = M_PROGRAM; rebuildKnobs(); }
                else if (fm[i] != (Func) -1) selectFunction (fm[i]);
                return;
            }
            switch (i)
            {
                case 0: mode = M_PROGRAM; rebuildKnobs(); break;
                case 1: showStoreMenu(); break;
                case 2: selectFunction (F_REVERB); break;
                case 3: selectFunction (F_DELAY); break;
                case 4: selectFunction (F_PITCH); break;
                case 5: selectFunction (F_EQ); break;
                case 6: selectFunction (F_UTIL); break;
                case 7: selectFunction (F_CONFIG); break;
                case 8: selectFunction (F_MIX); break;
                case 9: selectFunction (F_MOD); break;
                case 10: mode = M_NAME; nameCursor = 0; rebuildKnobs(); break;
                case 11: proc.setParam (BYPASS, proc.getParam (BYPASS) > 0.5f ? 0.0f : 1.0f); break;
                default: break;
            }
            updateLcd();
        };
    }

    hint.setColour (juce::Label::textColourId, col::dim);
    hint.setFont (uiFont (13.0f, false));
    hint.setJustificationType (juce::Justification::centred);
    addChildComponent (hint);

    setSize (W, H);          // after all children exist: triggers resized()
    setSize (W, H);          // after all children exist: triggers resized()
    selectFunction (F_REVERB);
    if (proc.emulationActive() && ! proc.bootSplashShown)
    {
        // replay the firmware's power-on screen at its real speed
        proc.bootSplashShown = true;
        boot = proc.emuBootLcd();
        bootStartMs = juce::Time::getMillisecondCounterHiRes();
    }
    startTimerHz (30);
}

Content::~Content() { stopTimer(); programView.setViewedComponent (nullptr, false); setLookAndFeel (nullptr); }

void Content::paint (juce::Graphics& g)
{
    // faceplate
    auto face = getLocalBounds().removeFromTop (190).toFloat();
    juce::ColourGradient grad (col::panelTop, 0, 0, col::panelBot, 0, face.getBottom(), false);
    g.setGradientFill (grad); g.fillRect (face);
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (int y = 2; y < 190; y += 3) g.drawHorizontalLine (y, 30.0f, (float) W - 30.0f);   // brushed metal
    // rack ears
    for (float x : { 0.0f, (float) W - 28.0f })
    {
        g.setColour (juce::Colour (0xff141517)); g.fillRect (x, 0.0f, 28.0f, 190.0f);
        g.setColour (juce::Colour (0xff050505));
        for (float y : { 34.0f, 156.0f }) g.fillRoundedRectangle (x + 6.0f, y - 6.0f, 16.0f, 12.0f, 6.0f);
    }
    // brand
    g.setColour (col::text); g.setFont (uiFont (17.0f));
    g.drawText ("ALESIS", juce::Rectangle<int> (46, 20, 190, 20), juce::Justification::left);
    g.setFont (uiFont (31.0f).italicised());
    g.drawText ("QUADRAVERB", juce::Rectangle<int> (44, 40, 200, 36), juce::Justification::left);
    g.setColour (col::amber); g.setFont (uiFont (10.5f));
    g.drawText ("16 BIT DIGITAL EFFECTS PROCESSOR", juce::Rectangle<int> (46, 76, 220, 14), juce::Justification::left);
    g.setColour (col::dim); g.setFont (uiFont (9.0f, false));
    g.drawText ("QV-89 EMULATION / FIRMWARE V1.07", juce::Rectangle<int> (46, 90, 200, 12), juce::Justification::left);

    // headroom LEDs
    const char* lbl[4] = { "-30", "-18", "-6", "CLIP" };
    const float thr[4] = { 0.0316f, 0.126f, 0.501f, 0.99f };
    for (int i = 0; i < 4; ++i)
    {
        auto r = juce::Rectangle<float> (50.0f + (float) i * 44.0f, 122.0f, 12.0f, 12.0f);
        const bool on = (i == 3) ? clipHold > 0 : ledLevel >= thr[i];
        const auto c = i == 3 ? col::ledRed : col::ledGreen;
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (r.expanded (4.0f)); }
        g.setColour (on ? c : c.darker (2.6f)); g.fillEllipse (r);
        g.setColour (col::dim); g.setFont (uiFont (9.5f));
        g.drawText (lbl[i], juce::Rectangle<float> (r.getX() - 14.0f, 140.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    g.setColour (col::dim); g.setFont (uiFont (9.0f));
    g.drawText ("HEADROOM", juce::Rectangle<int> (50, 156, 170, 12), juce::Justification::left);

    // knob labels
    g.setColour (col::text); g.setFont (uiFont (10.5f));
    g.drawText ("INPUT", juce::Rectangle<int> (240, 142, 80, 14), juce::Justification::centred);
    g.drawText ("OUTPUT", juce::Rectangle<int> (326, 142, 80, 14), juce::Justification::centred);
    g.drawText ("PAGE", juce::Rectangle<int> (430, 168, 110, 14), juce::Justification::centred);
    g.drawText ("VALUE", juce::Rectangle<int> (556, 168, 110, 14), juce::Justification::centred);

    // lower section
    auto lower = getLocalBounds().withTrimmedTop (190).toFloat();
    g.setColour (col::lower); g.fillRect (lower);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (190, 0.0f, (float) W);
    g.setColour (juce::Colour (0xff2a2c31)); g.drawHorizontalLine (268, 20.0f, (float) W - 20.0f);
}

void Content::resized()
{
    inKnob.setBounds (245, 64, 70, 76);
    outKnob.setBounds (331, 64, 70, 76);
    lcd.setBounds (425, 42, 245, 82);
    engineBtn.setBounds (425, 12, 150, 24);
    fileBtn.setBounds (580, 12, 90, 24);
    pageL.setBounds (432, 134, 52, 32); pageR.setBounds (486, 134, 52, 32);
    valL.setBounds (558, 134, 52, 32);  valR.setBounds (612, 134, 52, 32);
    for (int i = 0; i < 12; ++i)
    {
        const int c = i % 6, r = i / 6;
        funcButtons[i]->setBounds (688 + c * 46, 36 + r * 66, 46, 52);
    }
    flow.setBounds (20, 196, W - 40, 68);
    panelArea = { 20, 276, W - 40, H - 286 };
    rebuildKnobs();
}

juce::String Content::pagesSignature()
{
    float v[NUM_PARAMS]; proc.currentValues (v);
    juce::String s; s << (int) mode << ":" << (int) func << ":" << (int) v[CONFIG] << ":" << proc.userBankSize() << ":"
                      << (mode == M_PROGRAM ? proc.currentBankProgram() : -1) << ":";
    for (int i : pagesFor (func, v)) s << i << ",";
    return s;
}

void Content::selectFunction (Func f)
{
    mode = M_FUNC;
    func = f;
    float v[NUM_PARAMS]; proc.currentValues (v);
    auto pages = pagesFor (f, v);
    focus = pages.empty() ? -1 : pages.front();
    rebuildKnobs();
    updateLcd();
}

void Content::focusParam (int index)
{
    if (mode == M_NAME) mode = M_FUNC;
    focus = index;
    for (auto* k : knobs) { const bool f = k->index == index; if (k->focused != f) { k->focused = f; k->repaint(); } }
    updateLcd();
}

void Content::rebuildKnobs()
{
    // keep existing knobs (e.g. one being dragged) so a page rebuild does not interrupt the gesture
    std::map<int, std::unique_ptr<ParamKnob>> old;
    while (! knobs.isEmpty()) { std::unique_ptr<ParamKnob> k (knobs.removeAndReturn (knobs.size() - 1)); const int idx = k->index; old[idx] = std::move (k); }
    panelButtons.clear();
    removeChildComponent (&hint);
    hint.setVisible (false);
    programView.setVisible (false);
    sig = pagesSignature();

    float v[NUM_PARAMS]; proc.currentValues (v);
    const int cfg = (int) v[CONFIG];
    flow.config = cfg;
    flow.chorusTag = v[REV_CHORUS] > 0.5f;
    flow.selected = mode == M_FUNC ? func : (Func) -1;
    flow.repaint();

    auto area = panelArea;
    auto touch = [this] (int idx) { focusParam (idx); };

    if (mode == M_PROGRAM)
    {
        // program list (scrollable): ROM factory programs, QV89 originals, imported SysEx/ROM programs
        const int n = proc.numBankPrograms(), nRom = proc.romProgramCount(), nFac = proc.getNumPrograms();
        const int cols = 5, bw = (area.getWidth() - 14) / cols, bh = 28;
        programHolder.setSize (area.getWidth() - 14, ((n + cols - 1) / cols) * (bh + 4));
        for (int i = 0; i < n; ++i)
        {
            auto* b = panelButtons.add (new juce::TextButton());
            juce::String tag = i < nRom ? "" : (i < nFac ? " *" : " +");
            if (emu()) tag = i < 100 ? "" : (i < 100 + nFac - nRom ? " *" : " +");
            b->setButtonText (juce::String (i).paddedLeft ('0', 2) + "  " + proc.bankProgramName (i) + tag);
            b->setToggleState (i == proc.currentBankProgram(), juce::dontSendNotification);
            b->onClick = [this, i] { proc.selectBankProgram (i); updateLcd(); };   // timer rebuilds the list
            b->setBounds ((i % cols) * bw + 2, (i / cols) * (bh + 4), bw - 4, bh);
            programHolder.addAndMakeVisible (b);
        }
        programView.setBounds (area.withTrimmedBottom (22));
        addAndMakeVisible (programView);
        programView.setViewPosition (programView.getViewPositionX(), juce::jmin (programView.getViewPositionY(), juce::jmax (0, programHolder.getHeight() - programView.getHeight())));
        if (emu()) hint.setText ("00-99: memory of the emulated unit (STORE saves here)  |  * QV89 originals and + imported are sent as SysEx", juce::dontSendNotification);
        else hint.setText (juce::String (nRom > 0 ? "00-" + juce::String (nRom - 1) + ": QuadraVerb ROM  |  " : "") + "* QV89 originals  |  + imported  |  MIDI Program Change selects by number",
                      juce::dontSendNotification);
        hint.setBounds (area.removeFromBottom (20));
        addAndMakeVisible (hint);
        return;
    }
    if (mode == M_NAME)
    {
        hint.setText ("NAME:  PAGE \xe2\x97\x80 \xe2\x96\xb6 moves the cursor,  VALUE \xe2\x97\x80 \xe2\x96\xb6 changes the character", juce::dontSendNotification);
        hint.setBounds (area.withHeight (40));
        addAndMakeVisible (hint);
        return;
    }

    if (func == F_CONFIG)
    {
        static const char* desc[5] = {
            "QuadMode: 3-band EQ > Pitch > Delay > Reverb, all four effects at once",
            "Rotary speaker simulator > Delay > Reverb",
            "11-band graphic EQ (+/-14 dB) > Delay up to 1.5 s",
            "5-band parametric EQ > Pitch > Delay up to 1.5 s",
            "3-band EQ > high-density Reverb 2 with optional reverb chorus" };
        const int bw = (area.getWidth() - 4 * 10) / 5;
        for (int c = 0; c < 5; ++c)
        {
            auto* b = panelButtons.add (new juce::TextButton (info (CONFIG).choices[(size_t) c]));
            b->setToggleState (c == cfg, juce::dontSendNotification);
            b->onClick = [this, c] { proc.setParam (CONFIG, (float) c); focus = CONFIG; updateLcd(); };   // timer rebuilds
            b->setBounds (area.getX() + c * (bw + 10), area.getY() + 20, bw, 70);
            addAndMakeVisible (b);
        }
        hint.setText (juce::String ("CONFIG ") + juce::String (cfg + 1) + ":  " + desc[cfg], juce::dontSendNotification);
        hint.setBounds (area.getX(), area.getY() + 110, area.getWidth(), 30);
        addAndMakeVisible (hint);
        return;
    }

    auto pages = pagesFor (func, v);
    if (pages.empty())
    {
        hint.setText (juce::String (funcName (func)) + " is not part of configuration " + juce::String (cfg + 1) +
                      "  (" + info (CONFIG).choices[(size_t) cfg] + ")", juce::dontSendNotification);
        hint.setBounds (area.withHeight (60));
        addAndMakeVisible (hint);
        return;
    }
    if (std::find (pages.begin(), pages.end(), focus) == pages.end()) focus = pages.front();

    const bool graphic = func == F_EQ && cfg == CFG_GRAPHIC;
    const int cols = graphic ? 11 : 8;
    const int cw = area.getWidth() / cols, chh = graphic ? area.getHeight() - 70 : 118;
    for (size_t i = 0; i < pages.size(); ++i)
    {
        auto it = old.find (pages[i]);
        auto* k = knobs.add (it != old.end() ? it->second.release() : new ParamKnob (proc, pages[i], touch));
        int c = (int) i % cols, r = (int) i / cols;
        if (func == F_MOD) { c = (int) i / 3; r = (int) i % 3; }
        k->setBounds (area.getX() + c * cw, area.getY() + r * chh, cw, chh);
        k->focused = pages[i] == focus;
        addAndMakeVisible (k);
    }

    if (func == F_UTIL)
    {
        const char* labels[6] = { "LOAD PROGRAM...", "SAVE PROGRAM...", "IMPORT SYSEX...", "EXPORT SYSEX...", "CLEAR IMPORTED", "LOAD QV ROM (.bin)..." };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = panelButtons.add (new juce::TextButton (labels[i]));
            b->setBounds (area.getX() + i * (area.getWidth() / 6) + 4, area.getY() + 150, area.getWidth() / 6 - 8, 34);
            b->onClick = [this, i]
            {
                if (i == 4) { proc.clearUserBank(); status = "IMPORTED CLEARED"; statusTimer = 60; updateLcd(); return; }
                if (i == 5) { fileAction (5); return; }
                const int ids[4] = { 3, 1, 4, 2 };
                fileAction (ids[i]);
            };
            addAndMakeVisible (b);
        }
        hint.setText ((proc.romProgramCount() > 0 ? proc.romDescription() + " embedded.  " : juce::String()) +
                      "SysEx/ROM import reads real QuadraVerb dumps and EPROM images; imported programs appear under PROG.",
                      juce::dontSendNotification);
        hint.setBounds (area.getX(), area.getY() + 196, area.getWidth(), 24);
        addAndMakeVisible (hint);
    }
}

void Content::fileAction (int id)
{
    // 1 save .qv89, 2 export .syx, 3 load .qv89, 4 import .syx, 5 load EPROM image
    const bool save = (id == 1 || id == 2);
    const juce::String pattern = id == 5 ? "*.bin;*.rom" : (id == 2 || id == 4) ? "*.syx" : "*.qv89";
    chooser = std::make_unique<juce::FileChooser> (save ? "Save" : "Open", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), pattern);
    const int fcFlags = (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                            : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (fcFlags, [this, id] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (id == 1)
        {
            auto p = proc.captureProgram();
            juce::XmlElement x ("QV89Program"); x.setAttribute ("name", juce::String (p.name));
            for (int k = 0; k < NUM_PROGRAM_PARAMS; ++k) x.setAttribute (info (k).id, p.v[(size_t) k]);
            status = x.writeTo (f.withFileExtension ("qv89")) ? "PROGRAM SAVED" : "WRITE FAILED";
        }
        else if (id == 2) status = proc.exportSysex (f.withFileExtension ("syx"));
        else if (id == 3)
        {
            if (auto x = juce::XmlDocument::parse (f); x != nullptr && x->hasTagName ("QV89Program"))
            {
                auto p = defaultProgram(); p.name = x->getStringAttribute ("name", "LOADED").toStdString();
                for (int k = 0; k < NUM_PROGRAM_PARAMS; ++k) p.v[(size_t) k] = (float) x->getDoubleAttribute (info (k).id, info (k).def);
                proc.loadProgram (p); status = "PROGRAM LOADED";
            }
            else status = "NOT A QV89 FILE";
        }
        else if (id == 5) status = proc.importRom (f);
        else status = proc.importSysex (f);
        statusTimer = 75;
        updateLcd();
    });
}

void Content::emuKeyState (int key, bool down)
{
    if (! emu() || ! juce::isPositiveAndBelow (key, 16)) { if (juce::isPositiveAndBelow (key, 16)) keyDown[key] = false; return; }
    if (keyDown[key] == down) return;
    keyDown[key] = down;
    proc.emuKey (key, down);
}

void Content::updateEmuPanel()
{
    if (! boot.empty())
    {
        const float t = (float) ((juce::Time::getMillisecondCounterHiRes() - bootStartMs) * 0.001);
        if (t < boot.back().t)
        {
            const EmuEngine::BootFrame* fr = &boot.front();
            for (auto& b : boot) if (b.t <= t) fr = &b;
            lcd.setLines (juce::String (fr->line0), juce::String (fr->line1));
            for (auto* b : funcButtons) b->lit = false;
            return;
        }
        boot.clear();
    }
    const auto p = proc.emuPanel();
    auto conv = [] (const char* s)
    {
        juce::String out;
        for (int i = 0; i < 16; ++i)
        {
            const auto c = (unsigned char) s[i];
            out += c == 0x7E ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) : c == 0x7F ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x90"))
                 : (c >= 32 && c < 127) ? juce::String::charToString ((juce::juce_wchar) c) : juce::String (" ");
        }
        return out;
    };
    int cl = -1, cc = -1;
    if (p.cursor >= 0) { cl = p.cursor >= 0x40 ? 1 : 0; cc = p.cursor & 0x3F; if (cc > 15) cl = cc = -1; }
    juce::String b = conv (p.lcd[1]);
    if (statusTimer > 0) b = status.substring (0, 16);
    lcd.setLines (conv (p.lcd[0]), b, cl, cc);

    // LED latches are active low
    auto on = [] (uint8_t latch, int bit) { return ((latch >> bit) & 1) == 0; };
    funcButtons[0]->lit = (p.led600 >> 7 & 1) == 0;    // PROGRAM
    funcButtons[1]->lit = on (p.led400, 5);             // STORE
    funcButtons[2]->lit = on (p.led800, 1);             // REVERB
    funcButtons[3]->lit = on (p.led800, 3);             // DELAY
    funcButtons[4]->lit = on (p.led800, 5);             // PITCH
    funcButtons[5]->lit = on (p.led800, 7);             // EQ
    funcButtons[6]->lit = on (p.led400, 7);             // MIDI
    funcButtons[7]->lit = on (p.led800, 0);             // CONFIG
    funcButtons[8]->lit = on (p.led800, 2);             // MIX
    funcButtons[9]->lit = on (p.led800, 4);             // MOD
    funcButtons[10]->lit = on (p.led800, 6);            // NAME
    funcButtons[11]->lit = on (p.led400, 6);            // BYPASS
    for (auto* b : funcButtons) b->dimmed = false;
    if (p.clip) clipHold = 20;
}

void Content::showStoreMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("STORE / LOAD");
    m.addItem (1, "Save program to file (.qv89)...");
    m.addItem (3, "Load program file (.qv89)...");
    m.addSeparator();
    m.addItem (2, "Export program as QuadraVerb SysEx (.syx)...");
    m.addItem (4, "Import QuadraVerb SysEx (.syx)...");
    m.addItem (5, "Load QuadraVerb EPROM image (.bin)...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (funcButtons[1]), [this] (int id)
    {
        if (id > 0) fileAction (id);
    });
}

void Content::page (int dir)
{
    if (mode == M_NAME) { nameCursor = juce::jlimit (0, 13, nameCursor + dir); updateLcd(); return; }
    if (mode == M_PROGRAM) return;
    float v[NUM_PARAMS]; proc.currentValues (v);
    auto pages = pagesFor (func, v);
    if (pages.empty()) return;
    auto it = std::find (pages.begin(), pages.end(), focus);
    int idx = it == pages.end() ? 0 : (int) (it - pages.begin());
    idx = juce::jlimit (0, (int) pages.size() - 1, idx + dir);
    focusParam (pages[(size_t) idx]);
}

void Content::value (int dir, int rc)
{
    if (mode == M_PROGRAM)
    {
        const int n = proc.numBankPrograms();
        proc.selectBankProgram ((proc.currentBankProgram() + dir + n) % n);
        updateLcd(); return;
    }
    if (mode == M_NAME)
    {
        auto name = proc.programName.paddedRight (' ', 14).substring (0, 14);
        juce::juce_wchar c = name[nameCursor];
        c = (juce::juce_wchar) (c + dir);
        if (c < 32) c = 126;
        if (c > 126) c = 32;
        name = name.substring (0, nameCursor) + juce::String::charToString (c) + name.substring (nameCursor + 1);
        proc.programName = name.trimEnd();
        updateLcd(); return;
    }
    if (focus < 0) return;
    const auto& pi = info (focus);
    const float steps = (pi.maxV - pi.minV) / pi.step;
    float mult = 1.0f;
    if (steps > 150) mult = rc > 30 ? 50.0f : rc > 12 ? 10.0f : 1.0f;
    const float nv = juce::jlimit (pi.minV, pi.maxV, proc.getParam (focus) + (float) dir * pi.step * mult);
    proc.setParam (focus, nv);
    updateLcd();
}

void Content::updateLcd()
{
    if (emu()) { updateEmuPanel(); return; }
    float v[NUM_PARAMS]; proc.currentValues (v);
    juce::String a, b; int cl = -1, cc = -1;
    if (mode == M_PROGRAM)
    {
        a = proc.programName.toUpperCase();
        const int i = proc.currentBankProgram();
        b = "PROGRAM " + juce::String (i).paddedLeft ('0', 2) + (i < proc.romProgramCount() ? " ROM" : i < proc.getNumPrograms() ? " QV89" : " IMP");
    }
    else if (mode == M_NAME)
    {
        a = "EDIT NAME:";
        b = "\"" + proc.programName.paddedRight (' ', 14).substring (0, 14) + "\"";
        cl = 1; cc = nameCursor + 1;
    }
    else if (func == F_UTIL && focus < 0) { a = "UTILITIES"; b = ""; }
    else if (focus >= 0)
    {
        if (pagesFor (func, v).empty() && func != F_CONFIG) { a = "NOT AVAILABLE IN"; b = "THIS CONFIG"; }
        else { a = info (focus).lcd; b = formatValue (focus, v[focus], v); }
    }
    else { a = "NOT AVAILABLE IN"; b = "THIS CONFIG"; }
    if (statusTimer > 0) b = status;
    lcd.setLines (a.toUpperCase().substring (0, 16), b.substring (0, 16), cl, cc);
}

void Content::timerCallback()
{
    if (emu() != wasEmu)
    {
        wasEmu = emu();
        funcButtons[6]->setName (wasEmu ? "MIDI" : kFuncLabels[6]);
        for (auto& k : keyDown) k = false;
        mode = M_FUNC; rebuildKnobs();
    }
    engineBtn.setButtonText (proc.romAvailable() ? (wasEmu ? "ENGINE: ROM" : "ENGINE: MODEL") : "ENGINE: MODEL (NO ROM)");
    engineBtn.setEnabled (proc.romAvailable());
    if (wasEmu)
    {
        const float pk = proc.inputPeak();
        ledLevel = juce::jmax (pk, ledLevel * 0.85f);
        if (pk >= 0.99f) clipHold = 20; else if (clipHold > 0) --clipHold;
        repaint (ledArea.isEmpty() ? juce::Rectangle<int> (40, 110, 200, 50) : ledArea);
        updateEmuPanel();
        for (auto* b : funcButtons) b->repaint();
        if (pagesSignature() != sig) rebuildKnobs();
        if (proc.nameChangeCounter.load() != lastNameCounter) { lastNameCounter = proc.nameChangeCounter.load(); if (mode == M_PROGRAM) rebuildKnobs(); }
        for (auto* k : knobs) k->repaint();
        if (statusTimer > 0) --statusTimer;
        return;
    }
    // meters
    const float pk = proc.inputPeak();
    ledLevel = juce::jmax (pk, ledLevel * 0.85f);
    if (pk >= 0.99f) clipHold = 20; else if (clipHold > 0) --clipHold;
    repaint (ledArea.isEmpty() ? juce::Rectangle<int> (40, 110, 200, 50) : ledArea);

    float v[NUM_PARAMS]; proc.currentValues (v);
    funcButtons[11]->lit = v[BYPASS] > 0.5f;
    funcButtons[0]->lit = mode == M_PROGRAM;
    funcButtons[10]->lit = mode == M_NAME;
    const Func map[12] = { (Func) -1, (Func) -1, F_REVERB, F_DELAY, F_PITCH, F_EQ, F_UTIL, F_CONFIG, F_MIX, F_MOD, (Func) -1, (Func) -1 };
    for (int i = 2; i < 10; ++i)
    {
        funcButtons[i]->lit = mode == M_FUNC && map[i] == func;
        funcButtons[i]->dimmed = ! functionActive (map[i], (int) v[CONFIG]);
    }
    for (auto* b : funcButtons) b->repaint();

    if (pagesSignature() != sig || (flow.chorusTag != (v[REV_CHORUS] > 0.5f))) rebuildKnobs();
    if (proc.nameChangeCounter.load() != lastNameCounter) { lastNameCounter = proc.nameChangeCounter.load(); if (mode == M_PROGRAM) rebuildKnobs(); }
    for (auto* k : knobs) k->repaint();
    if (statusTimer > 0 && --statusTimer == 0) updateLcd();
    updateLcd();
}
} // namespace ui

// ============================================================================ Editor
QV89Editor::QV89Editor (QV89Processor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (ui::Content::W / 2, ui::Content::H / 2, ui::Content::W * 2, ui::Content::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) ui::Content::W / (double) ui::Content::H);
    setSize (ui::Content::W, ui::Content::H);
}

void QV89Editor::resized()
{
    const float s = (float) getWidth() / (float) ui::Content::W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, ui::Content::W, ui::Content::H);
}
