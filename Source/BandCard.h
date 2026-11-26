#pragma once
#include <map>
#include "PluginProcessor.h"
#include "PrismLook.h"
#include <functional>

// Floating band card: appears next to the selected band (EQ / DYNAMICS / LFO tabs).
namespace UI
{
inline const juce::Colour kCard (0xf2151515), kField (0xff1c1c1c), kLine (0xff343434), kText (0xffe8e8e8),
                          kAccent (0xff5eead4), kAmber (0xfffbbf24), kRed (0xfff87171);

// ---- icon paths (arbitrary box; they are scaled to fit the button)
inline juce::Path undoPath()      // bent arrow, pointing back
{
    juce::Path p;
    p.startNewSubPath (7.0f, 2.5f); p.lineTo (3.0f, 6.5f); p.lineTo (7.0f, 10.5f);
    p.startNewSubPath (3.0f, 6.5f); p.lineTo (11.0f, 6.5f);
    p.cubicTo (15.5f, 6.5f, 17.5f, 9.0f, 17.5f, 11.5f);
    p.cubicTo (17.5f, 14.0f, 15.5f, 15.5f, 12.0f, 15.5f);
    p.lineTo (8.0f, 15.5f);
    return p;
}
inline juce::Path redoPath()
{
    auto p = undoPath();
    p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (20.0f, 0.0f));
    return p;
}
inline juce::Path fullscreenPath()      // four corners
{
    juce::Path p;
    p.startNewSubPath (2.0f, 6.0f);   p.lineTo (2.0f, 2.0f);   p.lineTo (7.0f, 2.0f);
    p.startNewSubPath (13.0f, 2.0f);  p.lineTo (18.0f, 2.0f);  p.lineTo (18.0f, 6.0f);
    p.startNewSubPath (18.0f, 10.0f); p.lineTo (18.0f, 14.0f); p.lineTo (13.0f, 14.0f);
    p.startNewSubPath (7.0f, 14.0f);  p.lineTo (2.0f, 14.0f);  p.lineTo (2.0f, 10.0f);
    return p;
}
inline juce::Path windowedPath()        // the four corners turned inwards: back to a window
{
    juce::Path p;
    p.startNewSubPath (2.0f, 6.0f);   p.lineTo (7.0f, 6.0f);    p.lineTo (7.0f, 2.0f);
    p.startNewSubPath (13.0f, 2.0f);  p.lineTo (13.0f, 6.0f);   p.lineTo (18.0f, 6.0f);
    p.startNewSubPath (18.0f, 10.0f); p.lineTo (13.0f, 10.0f);  p.lineTo (13.0f, 14.0f);
    p.startNewSubPath (7.0f, 14.0f);  p.lineTo (7.0f, 10.0f);   p.lineTo (2.0f, 10.0f);
    return p;
}
inline juce::Path chevronPath (int dir)
{
    juce::Path p;
    if (dir < 0) { p.startNewSubPath (11.0f, 3.0f); p.lineTo (5.0f, 9.0f); p.lineTo (11.0f, 15.0f); }
    else         { p.startNewSubPath (5.0f, 3.0f);  p.lineTo (11.0f, 9.0f); p.lineTo (5.0f, 15.0f); }
    return p;
}
inline juce::Path crossPath()
{
    juce::Path p;
    p.startNewSubPath (3.0f, 3.0f); p.lineTo (13.0f, 13.0f);
    p.startNewSubPath (13.0f, 3.0f); p.lineTo (3.0f, 13.0f);
    return p;
}
inline juce::Path mutePath()
{
    juce::Path p;
    p.addEllipse (2.0f, 2.0f, 12.0f, 12.0f);
    p.startNewSubPath (4.5f, 11.5f); p.lineTo (11.5f, 4.5f);
    return p;
}
inline juce::Path headphonePath()
{
    juce::Path p;
    p.addCentredArc (8.0f, 8.0f, 5.5f, 5.5f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    p.addRoundedRectangle (0.5f, 7.5f, 3.2f, 6.0f, 1.2f);
    p.addRoundedRectangle (12.3f, 7.5f, 3.2f, 6.0f, 1.2f);
    return p;
}

inline juce::Path powerPath()
{
    juce::Path p;
    p.addCentredArc (8.0f, 9.0f, 6.0f, 6.0f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    p.startNewSubPath (8.0f, 1.5f); p.lineTo (8.0f, 8.0f);
    return p;
}
inline juce::Path spectrumPath()          // analyzer settings
{
    juce::Path p;
    p.startNewSubPath (3.0f, 14.0f);  p.lineTo (3.0f, 9.0f);
    p.startNewSubPath (8.0f, 14.0f);  p.lineTo (8.0f, 3.0f);
    p.startNewSubPath (13.0f, 14.0f); p.lineTo (13.0f, 6.5f);
    return p;
}
inline juce::Path matchPath()             // equal loudness
{
    juce::Path p;
    p.addEllipse (1.5f, 1.5f, 13.0f, 13.0f);
    p.startNewSubPath (5.0f, 6.0f);  p.lineTo (11.0f, 6.0f);
    p.startNewSubPath (5.0f, 10.0f); p.lineTo (11.0f, 10.0f);
    return p;
}
inline juce::Path deltaPath()
{
    juce::Path p;
    p.startNewSubPath (8.0f, 2.0f); p.lineTo (14.5f, 14.0f); p.lineTo (1.5f, 14.0f); p.closeSubPath();
    return p;
}
inline juce::Path eqTabPath()
{
    juce::Path p;
    p.startNewSubPath (1.0f, 12.0f); p.cubicTo (6.0f, 12.0f, 7.0f, 3.0f, 10.0f, 3.0f); p.cubicTo (13.0f, 3.0f, 14.0f, 12.0f, 19.0f, 12.0f);
    return p;
}
inline juce::Path dynTabPath()            // level in -> gain out
{
    juce::Path p;
    p.startNewSubPath (2.0f, 14.0f); p.lineTo (8.0f, 14.0f); p.cubicTo (11.0f, 14.0f, 12.0f, 5.0f, 18.0f, 4.0f);
    return p;
}
inline juce::Path lfoTabPath()
{
    juce::Path p;
    p.startNewSubPath (1.0f, 8.0f); p.cubicTo (4.0f, 0.0f, 7.0f, 0.0f, 10.0f, 8.0f); p.cubicTo (13.0f, 16.0f, 16.0f, 16.0f, 19.0f, 8.0f);
    return p;
}

// ---- fonts: nicer than the system default, with a safe fallback (JUCE falls back if a name is missing)
inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font monoFont (float h)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Menlo", h, juce::Font::plain));
   #elif JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
   #endif
}
inline float textWidth (const juce::Font& f, const juce::String& s)
{
    // JUCE 8 builds a fresh native font on every shaping call, so measuring text is expensive: remember the results
    static std::map<juce::String, float> cache;
    const auto key = juce::String (f.getHeight(), 2) + (f.isBold() ? "|b|" : "|p|") + f.getTypefaceName() + "|" + s;
    if (const auto it = cache.find (key); it != cache.end()) return it->second;
    if (cache.size() > 3000) cache.clear();
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return cache[key] = ga.getBoundingBox (0, -1, true).getWidth();
}

// ---- LFO shape value at phase x, -1..1. Random is drawn over several cycles (x counts cycles), with the same glide as the audio engine.
inline float lfoShape (int shape, float x, float smooth = 1.0f)
{
    if (shape == EQ::LfoDrift)
    {
        const float k = std::floor (x);
        return EQ::driftAt ((uint32_t) (int) k + 7u, x - k);       // a fixed stretch of the same wander the audio engine produces
    }
    if (shape == EQ::LfoRandom)
    {
        static const float pat[8] = { 0.3f, -0.7f, 0.8f, -0.2f, -0.9f, 0.5f, 0.1f, -0.5f };
        const int k = (int) std::floor (x);
        const float f = x - (float) k;
        const float a = pat[((k % 8) + 8) % 8], b = pat[((k + 1) % 8 + 8) % 8];
        const float t = smooth < 0.001f ? 1.0f : juce::jmin (1.0f, f / smooth);
        return a + (b - a) * (t * t * (3.0f - 2.0f * t));
    }
    x -= std::floor (x);
    switch (shape)
    {
        case EQ::LfoSine:    return std::sin (juce::MathConstants<float>::twoPi * x);
        case EQ::LfoTri:     return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
        case EQ::LfoSquare:  return x < 0.5f ? 1.0f : -1.0f;
        case EQ::LfoSawUp:   return 2.0f * x - 1.0f;
        case EQ::LfoSawDown: return 1.0f - 2.0f * x;
        default: return 0.0f;
    }
}

// same curve as the audio engine: band gain (dB) as a function of the detector level.
// Rest while quiet, Active while loud, a smooth step between them: Width dB wide, centred on the threshold.
inline float dynGain (float level, float thr, float wid, float rest, float active)
{
    const float t = juce::jlimit (0.0f, 1.0f, (level - (thr - wid * 0.5f)) / juce::jmax (0.1f, wid));
    return rest + (active - rest) * (t * t * (3.0f - 2.0f * t));
}

// ---- small icon button (optionally a toggle)
class IconButton : public juce::Button
{
public:
    explicit IconButton (const juce::String& name = {}) : juce::Button (name) { setWantsKeyboardFocus (false); }

    void setPath (const juce::Path& p) { path = p; repaint(); }
    void setColours (juce::Colour on, juce::Colour off) { onCol = on; offCol = off; repaint(); }
    void setFlat (bool f) { flat = f; repaint(); }     // no background at all: just the glyph, which lights up under the mouse

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        if (flat)
        {
            auto ic = path;
            if (ic.isEmpty()) return;
            ic.applyTransform (ic.getTransformToScaleToFit (r.reduced (r.getWidth() * 0.28f, r.getHeight() * 0.28f), true));
            g.setColour (kText.withAlpha (! isEnabled() ? 0.22f : (over || down) ? 1.0f : 0.6f));
            g.strokePath (ic, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }
        if (on || over || down)
        {
            g.setColour ((on ? onCol : kText).withAlpha (on ? 0.22f : 0.10f + (down ? 0.08f : 0.0f)));
            g.fillRoundedRectangle (r, 6.0f);
        }
        auto ic = path;
        if (! ic.isEmpty())
        {
            ic.applyTransform (ic.getTransformToScaleToFit (r.reduced (r.getWidth() * 0.24f, r.getHeight() * 0.24f), true));
            g.setColour ((on ? onCol : offCol).withAlpha (isEnabled() ? 1.0f : 0.3f));
            g.strokePath (ic, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

private:
    juce::Path path;
    juce::Colour onCol { kAccent }, offCol { kText.withAlpha (0.75f) };
    bool flat = false;
};

// ---- plain text that acts as a button: no box, an underline marks it (always, or when it is the active one)
class TextLink : public juce::Button
{
public:
    explicit TextLink (const juce::String& t = {}) : juce::Button (t)
    {
        setButtonText (t);
        setWantsKeyboardFocus (false);
        setRepaintsOnMouseActivity (true);
    }
    void setCentred (bool c) { centred = c; repaint(); }
    void setAlwaysUnderlined (bool u) { alwaysLine = u; repaint(); }
    void setFontHeight (float h) { fontH = h; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState(), lit = isEnabled() && (over || down);
        const float a = isEnabled() ? 1.0f : 0.35f;
        const auto font = uiFont (fontH);
        const auto text = getButtonText();
        const auto r = getLocalBounds().toFloat();
        const float tw = juce::jmin (r.getWidth(), textWidth (font, text) + 2.0f);
        const float x = centred ? r.getCentreX() - tw * 0.5f : r.getX();

        g.setFont (font);
        g.setColour ((on ? kAccent : kText.withAlpha (lit ? 1.0f : alwaysLine ? 0.85f : 0.5f)).withMultipliedAlpha (a));
        g.drawText (text, juce::Rectangle<float> (x, r.getY(), tw, r.getHeight() - 5.0f), juce::Justification::centredLeft, true);

        if (on || lit || alwaysLine)
        {
            g.setColour ((on ? kAccent : lit ? kText.withAlpha (0.8f) : kLine).withMultipliedAlpha (a));
            g.fillRect (juce::Rectangle<float> (x, r.getBottom() - (on ? 4.0f : 3.0f), tw, on ? 2.0f : 1.0f));
        }
    }

private:
    bool centred = false, alwaysLine = false;
    float fontH = 15.0f;
};

// ---- numeric readout: caption + big monospaced value. Drag up/down or scroll to change (Shift = fine),
//      double-click to type an exact value, Alt-click for the default.
class ValueField : public juce::Slider
{
public:
    explicit ValueField (const juce::String& cap) : caption (cap)
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setMouseDragSensitivity (260);
        setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // Shift + drag = fine

        addChildComponent (editor);
        editor.setFont (monoFont (17.0f));
        editor.setJustification (juce::Justification::centredLeft);
        editor.setSelectAllWhenFocused (true);
        editor.setIndents (2, 0);
        editor.onReturnKey = [this] { commit(); };
        editor.onEscapeKey = [this] { editor.setVisible (false); };
        editor.onFocusLost = [this] { editor.setVisible (false); repaint(); };
        setRepaintsOnMouseActivity (true);
    }

    void setCaption (const juce::String& c) { if (caption != c) { caption = c; repaint(); } }
    void setBandColour (juce::Colour c) { col = c; repaint(); }

    // one-line form for the header: caption and value side by side ("OUT  +1.5 dB"), underlined, no room above or below
    void setInline (bool b)
    {
        inlineMode = b;
        editor.setFont (monoFont (b ? 15.0f : 17.0f));
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float a = isEnabled() ? 1.0f : 0.4f;
        const bool lit = isEnabled() && (isMouseOverOrDragging() || editor.isVisible());

        g.setColour (kText.withAlpha (0.45f * a));
        g.setFont (uiFont (11.0f));
        if (inlineMode) g.drawText (caption, r.removeFromLeft (captionWidth()), juce::Justification::centredLeft);
        else            g.drawText (caption, r.removeFromTop (14.0f), juce::Justification::centredLeft);

        // split "1.800 kHz" into the number and its unit
        const auto full = getTextFromValue (getValue());
        int i = 0;
        while (i < full.length() && (juce::CharacterFunctions::isDigit (full[i]) || full[i] == '.' || full[i] == '-' || full[i] == '+')) ++i;
        const auto num = full.substring (0, i);
        const auto unit = full.substring (i).trim();

        if (! editor.isVisible())
        {
            // long exact numbers ("-12.345 dB", "12.346 kHz") shrink a little instead of being cut off
            float size = inlineMode ? 15.0f : 19.0f;
            auto nf = monoFont (size);
            const float unitW = unit.isNotEmpty() ? textWidth (uiFont (11.0f), unit) + 4.0f : 0.0f;
            while (size > 12.0f && textWidth (nf, num) + unitW > r.getWidth())
                nf = monoFont (size -= 1.0f);
            const auto va = r.withTrimmedBottom (inlineMode ? 2.0f : 4.0f);
            g.setColour ((lit ? col : kText).withAlpha (a));
            g.setFont (nf);
            g.drawText (num, va, juce::Justification::centredLeft, false);
            if (unit.isNotEmpty())
            {
                g.setColour (kText.withAlpha (0.4f * a));
                g.setFont (uiFont (11.0f));
                g.drawText (unit, va.withTrimmedLeft (textWidth (nf, num) + 4.0f), juce::Justification::centredLeft, false);
            }
        }

        g.setColour (lit ? col.withAlpha (0.9f) : kLine.withAlpha (a));
        g.fillRect (juce::Rectangle<float> (0.0f, (float) getHeight() - (lit ? 2.0f : 1.0f), (float) getWidth(), lit ? 2.0f : 1.0f));
    }

    void resized() override
    {
        juce::Slider::resized();
        editor.setBounds (inlineMode ? getLocalBounds().withTrimmedLeft ((int) captionWidth()).withTrimmedBottom (2)
                                     : getLocalBounds().withTrimmedTop (14).withTrimmedBottom (3));
    }

    std::function<void()> onContext;     // right-click (MIDI learn menu)
    std::function<void()> onClick;       // a plain click (no drag): used by Slope to open its list of steps

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick)         // list mode (Slope): a click opens the list, nothing else
        {
            if (isEnabled() && ! e.mods.isPopupMenu() && ! e.mods.isAltDown() && ! e.mouseWasDraggedSinceMouseDown())
                onClick();
            return;
        }
        juce::Slider::mouseUp (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! onClick) juce::Slider::mouseDrag (e);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! onClick) juce::Slider::mouseWheelMove (e, w);      // the list field ignores the wheel
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (isEnabled() && onContext) onContext();
            return;
        }
        if (e.mods.isAltDown() && isEnabled())      // Alt-click: back to default
            setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
        else if (! onClick)
            juce::Slider::mouseDown (e);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (! isEnabled() || onClick) return;
        editStart = getTextFromValue (getValue());
        editor.setText (editStart, false);
        editor.setVisible (true);
        editor.grabKeyboardFocus();
        editor.selectAll();
        repaint();
    }

private:
    float captionWidth() const { return textWidth (uiFont (11.0f), caption) + 8.0f; }

    void commit()
    {
        const auto t = editor.getText();
        editor.setVisible (false);
        if (t.isNotEmpty() && t != editStart)       // Enter on an untouched text keeps the exact stored value (the field shows it rounded)
            setValue (getValueFromText (t), juce::sendNotificationSync);
        repaint();
    }

    juce::String caption;
    juce::Colour col { kAccent };
    juce::TextEditor editor;
    juce::String editStart;
    bool inlineMode = false;
};

// ---- LFO shape icons (20 x 16 box)
inline juce::Path shapePath (int s)
{
    juce::Path p;
    switch (s)
    {
        case EQ::LfoOff:     p.startNewSubPath (2, 8); p.lineTo (18, 8); break;
        case EQ::LfoSine:    p.startNewSubPath (1, 8); p.cubicTo (4, 0, 7, 0, 10, 8); p.cubicTo (13, 16, 16, 16, 19, 8); break;
        case EQ::LfoTri:     p.startNewSubPath (1, 8); p.lineTo (5, 3); p.lineTo (10, 13); p.lineTo (15, 3); p.lineTo (19, 8); break;
        case EQ::LfoSquare:  p.startNewSubPath (1, 12); p.lineTo (1, 4); p.lineTo (7, 4); p.lineTo (7, 12); p.lineTo (13, 12); p.lineTo (13, 4); p.lineTo (19, 4); break;
        case EQ::LfoSawUp:   p.startNewSubPath (1, 13); p.lineTo (8, 3); p.lineTo (8, 13); p.lineTo (15, 3); p.lineTo (15, 13); break;
        case EQ::LfoSawDown: p.startNewSubPath (1, 3); p.lineTo (8, 13); p.lineTo (8, 3); p.lineTo (15, 13); p.lineTo (15, 3); break;
        case EQ::LfoDrift:   p.startNewSubPath (1, 10); p.cubicTo (4, 1, 6, 2, 9, 8); p.cubicTo (12, 14, 15, 13, 19, 5); break;
        default:             p.startNewSubPath (1, 9); p.lineTo (4, 9); p.lineTo (4, 4); p.lineTo (8, 4); p.lineTo (8, 12); p.lineTo (12, 12);
                             p.lineTo (12, 6); p.lineTo (16, 6); p.lineTo (16, 10); p.lineTo (19, 10); break;
    }
    return p;
}

// ---- one look for every non-numeric field: small caption, the value as text, underline (same language as ValueField)
inline void paintTextField (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& caption, const juce::String& value,
                            juce::Colour valueCol, juce::Colour accent, bool lit, bool enabled, bool chevron)
{
    const float a = enabled ? 1.0f : 0.4f;
    auto r = bounds.toFloat();
    g.setColour (kText.withAlpha (0.45f * a));
    g.setFont (uiFont (11.0f));
    g.drawText (caption, r.removeFromTop (14.0f), juce::Justification::centredLeft);

    g.setColour ((lit ? accent : valueCol).withMultipliedAlpha (a));
    g.setFont (uiFont (16.0f));
    g.drawText (value, r.withTrimmedBottom (4.0f).withTrimmedRight (chevron ? 14.0f : 0.0f), juce::Justification::centredLeft, true);

    if (chevron)
    {
        juce::Path chev;
        const float cx = (float) bounds.getWidth() - 6.0f, cy = r.getCentreY() - 1.0f;
        chev.startNewSubPath (cx - 3.5f, cy - 1.5f); chev.lineTo (cx, cy + 2.0f); chev.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (kText.withAlpha (0.45f * a));
        g.strokePath (chev, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (lit ? accent.withAlpha (0.9f) : kLine.withAlpha (a));
    g.fillRect (juce::Rectangle<float> (0.0f, (float) bounds.getHeight() - (lit ? 2.0f : 1.0f), (float) bounds.getWidth(), lit ? 2.0f : 1.0f));
}

struct FieldLook : PrismLook
{
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const juce::var cv = box.getProperties()["col"];
        const auto col = cv.isVoid() ? kAccent : juce::Colour ((juce::uint32) (int) cv);
        const bool lit = box.isEnabled() && (box.isMouseOver (true) || box.isPopupActive());
        paintTextField (g, { 0, 0, w, h }, box.getProperties()["cap"].toString(), box.getText(), kText, col, lit, box.isEnabled(), true);
    }
    void positionComboBoxText (juce::ComboBox&, juce::Label& l) override { l.setBounds (0, 0, 0, 0); }   // we paint the text ourselves
};

// Drop-down in the top bar: just text with an underline, like the preset name (no box, no arrow)
struct BarLook : PrismLook
{
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const bool lit = box.isEnabled() && (box.isMouseOver (true) || box.isPopupActive());
        const auto font = uiFont (15.0f);
        const auto text = box.getText();
        const float tw = juce::jmin ((float) w, textWidth (font, text) + 2.0f), x = ((float) w - tw) * 0.5f;
        g.setFont (font);
        g.setColour (kText.withAlpha (lit ? 1.0f : 0.85f));
        g.drawText (text, juce::Rectangle<float> (x, 0.0f, tw, (float) h - 5.0f), juce::Justification::centredLeft, true);
        g.setColour (lit ? kText.withAlpha (0.8f) : kLine);
        g.fillRect (juce::Rectangle<float> (x, (float) h - 3.0f, tw, 1.0f));
    }
    void positionComboBoxText (juce::ComboBox&, juce::Label& l) override { l.setBounds (0, 0, 0, 0); }
};

class ChoiceField : public juce::ComboBox
{
public:
    ChoiceField (const juce::String& cap, juce::LookAndFeel& lf)
    {
        setLookAndFeel (&lf);
        setWantsKeyboardFocus (false);
        getProperties().set ("cap", cap);
    }
    ~ChoiceField() override { setLookAndFeel (nullptr); }

    void setBandColour (juce::Colour c) { getProperties().set ("col", (int) c.getARGB()); repaint(); }
};

// ---- row of choices bound to a choice parameter (LFO shape): icons or text, the selected one lit with an underline
class SegRow : public juce::Component, public juce::TooltipClient
{
public:
    struct Item { juce::String text, tip; juce::Path icon; };

    explicit SegRow (std::vector<Item> its) : items (std::move (its))
    {
        int id = 1;
        for (auto& it : items) box.addItem (it.text, id++);
        box.onChange = [this] { repaint(); };
    }

    juce::ComboBox box;    // hidden: attached to the parameter; the row just shows and sets its selection
    void setBandColour (juce::Colour c) { col = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float a = isEnabled() ? 1.0f : 0.4f;
        const int n = (int) items.size();
        const float cw = r.getWidth() / (float) juce::jmax (1, n);
        const int sel = box.getSelectedItemIndex();
        const auto body = r.withTrimmedBottom (4.0f);

        for (int i = 0; i < n; ++i)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + (float) i * cw, body.getY(), cw, body.getHeight());
            const bool on = i == sel, hov = i == hoverIdx && isEnabled();
            g.setColour ((on ? col : kText.withAlpha (hov ? 0.9f : 0.5f)).withMultipliedAlpha (a));
            const auto& it = items[(size_t) i];
            if (! it.icon.isEmpty())
            {
                auto ic = it.icon;      // drawn in a 20 x 16 box: one scale for every icon, so they all match
                const float sc = juce::jmin (cell.getWidth() * 0.62f / 20.0f, cell.getHeight() * 0.62f / 16.0f);
                ic.applyTransform (juce::AffineTransform::translation (-10.0f, -8.0f).scaled (sc).translated (cell.getCentreX(), cell.getCentreY()));
                g.strokePath (ic, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                g.setFont (uiFont (14.0f));
                g.drawText (it.text, cell, juce::Justification::centred);
            }
            if (on)
                g.fillRect (juce::Rectangle<float> (cell.getX() + 6.0f, r.getBottom() - 2.0f, cw - 12.0f, 2.0f));
        }
        g.setColour (kLine.withAlpha (a));
        g.fillRect (juce::Rectangle<float> (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f));
    }

    void mouseMove (const juce::MouseEvent& e) override { const int i = indexAt (e.x); if (i != hoverIdx) { hoverIdx = i; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hoverIdx = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.x);
        if (isEnabled() && i >= 0) box.setSelectedItemIndex (i, juce::sendNotificationSync);
    }
    juce::String getTooltip() override
    {
        const int i = indexAt (getMouseXYRelative().x);
        return i >= 0 ? items[(size_t) i].tip : juce::String();
    }

private:
    int indexAt (int x) const
    {
        const int n = (int) items.size();
        return n > 0 && x >= 0 && x < getWidth() ? juce::jlimit (0, n - 1, x * n / juce::jmax (1, getWidth())) : -1;
    }
    std::vector<Item> items;
    juce::Colour col { kAccent };
    int hoverIdx = -1;
};

inline std::vector<SegRow::Item> shapeItems()
{
    std::vector<SegRow::Item> v;
    for (int s = 0; s < EQ::numLfoShapes; ++s)
        v.push_back ({ EQ::lfoShapeNames()[s], EQ::lfoShapeNames()[s], shapePath (s) });
    return v;
}
// ---- On / Off as text with an underline (bool parameter)
class ToggleField : public juce::Button
{
public:
    explicit ToggleField (const juce::String& cap) : juce::Button (cap), caption (cap)
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (false);
        setRepaintsOnMouseActivity (true);
    }
    void setBandColour (juce::Colour c) { col = c; repaint(); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        paintTextField (g, getLocalBounds(), caption, on ? "On" : "Off", on ? col : kText.withAlpha (0.55f), col,
                        isEnabled() && (over || down), isEnabled(), false);
    }
private:
    juce::String caption;
    juce::Colour col { kAccent };
};

// ---- two or more named choices as text with an underline; a click steps to the next one (choice parameter)
class CycleField : public juce::Component, public juce::SettableTooltipClient
{
public:
    CycleField (const juce::String& cap, const juce::StringArray& names) : caption (cap)
    {
        box.addItemList (names, 1);
        box.onChange = [this] { repaint(); };
        setRepaintsOnMouseActivity (true);
    }

    juce::ComboBox box;    // hidden: attached to the parameter
    void setBandColour (juce::Colour c) { col = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        paintTextField (g, getLocalBounds(), caption, box.getText(), kText, col, isEnabled() && isMouseOver(), isEnabled(), false);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        const int n = box.getNumItems();
        if (isEnabled() && n > 1) box.setSelectedItemIndex ((box.getSelectedItemIndex() + 1) % n, juce::sendNotificationSync);
    }

private:
    juce::String caption;
    juce::Colour col { kAccent };
};

// ---- the card
class BandCard : public juce::Component
{
public:
    static constexpr int W = 324;

    std::function<void()> onMenu, onPrev, onNext, onClose, onSolo, onSizeChanged, onMoved;
    std::function<void (const juce::String&)> onParamMenu;      // right-click on a value: parameter id

    // screen area of the band name in the header (the band-type menu opens right under it)
    juce::Rectangle<int> getNameScreenArea() const
    {
        return nameRect.withWidth (juce::jmin (nameRect.getWidth(), 150)).translated (getScreenX(), getScreenY());
    }

    explicit BandCard (EQProcessor& p) : proc (p),
        freqBar ("Frequency"), gainBar ("Gain"), qBar ("Q"),
        thrBar ("Threshold"), widBar ("Width"), restBar ("Rest"), actBar ("Active"), dFreqBar ("Detector freq"), dQBar ("Detector Q"),
        atkBar ("Attack"), relBar ("Release"), mixBar ("Wet / dry"),
        xRateBar ("Rate"), xAmtBar ("Amount"), yRateBar ("Rate"), yAmtBar ("Amount"), xSmBar ("Smooth"), ySmBar ("Smooth"),
        slopeBar ("Slope")
    {
        const char* tn[] = { "EQ", "Dynamics", "LFO" };
        const juce::Path tp[] = { eqTabPath(), dynTabPath(), lfoTabPath() };
        for (size_t i = 0; i < tabs.size(); ++i)
        {
            tabs[i].setPath (tp[i]);
            tabs[i].setTooltip (tn[i]);
            tabs[i].setClickingTogglesState (true);
            tabs[i].setRadioGroupId (41);
            tabs[i].onClick = [this, i] { tab = (int) i; sig = -1; tick(); };
            addAndMakeVisible (tabs[i]);
        }
        tabs[0].setToggleState (true, juce::dontSendNotification);

        prevBtn.setPath (chevronPath (-1));  nextBtn.setPath (chevronPath (1));
        soloBtn.setPath (headphonePath());   muteBtn.setPath (mutePath());
        closeBtn.setPath (crossPath());
        soloBtn.setClickingTogglesState (false);
        muteBtn.setClickingTogglesState (false);
        soloBtn.setTooltip ("Solo");  muteBtn.setTooltip ("Mute");  closeBtn.setTooltip ("Close");
        prevBtn.setTooltip ("Previous band");  nextBtn.setTooltip ("Next band");
        soloBtn.setColours (kAmber, kText.withAlpha (0.75f));
        muteBtn.setColours (kRed, kText.withAlpha (0.75f));
        prevBtn.onClick  = [this] { if (onPrev)  onPrev(); };
        nextBtn.onClick  = [this] { if (onNext)  onNext(); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        soloBtn.onClick  = [this] { if (onSolo)  onSolo(); };
        muteBtn.onClick  = [this]
        {
            if (band < 0) return;
            if (auto* pr = proc.apvts.getParameter (EQ::id (band, "mute")))
            {
                pr->beginChangeGesture();
                pr->setValueNotifyingHost (pr->getValue() > 0.5f ? 0.0f : 1.0f);
                pr->endChangeGesture();
            }
        };
        for (auto* b : { &prevBtn, &nextBtn, &soloBtn, &muteBtn, &closeBtn })
            addAndMakeVisible (*b);

        xSyncFld.addItemList (EQ::syncNames(), 1);
        ySyncFld.addItemList (EQ::syncNames(), 1);
        dynBtn.setTooltip ("Dynamic: the level moves the band's gain from Rest (quiet) to Active (loud)");
        autoBtn.setTooltip ("Auto: the threshold follows the average level");
        followBtn.onClick = [this]
        {
            // switching Follow off: the detector starts where the band is, instead of jumping to its own stored frequency
            if (! followBtn.getToggleState() && band >= 0)
            {
                gcSetParam ("dfreq", val ("freq"));
                gcSetParam ("dq", juce::jlimit (0.1f, 10.0f, val ("q")));
            }
        };
        followBtn.setTooltip ("Follow: the detector filter moves and narrows with the band. Off: it has its own frequency and Q");
        detFld.setTooltip ("Detector. Band: listens to the band's own range. Full: the whole signal. Sidechain: the sidechain input. Click to switch");
        restBar.setTooltip ("Rest: the band's gain while the signal is quiet (same as Gain on the EQ tab)");
        actBar.setTooltip ("Active: the band's gain while the signal is loud");
        widBar.setTooltip ("Width: how many dB the level needs to travel from Rest to Active");

        for (auto* c : bodyComps())
            addChildComponent (*c);

        setSize (W, 160);
        setVisible (false);
    }

    int getBand() const { return band; }
    void setLive (bool l) { live = l; }

    void setBand (int b, juce::Colour c)
    {
        att.clear();
        band = b;
        col = c;
        if (b < 0) { setVisible (false); return; }

        auto& ap = proc.apvts;
        auto sa = [&] (const char* n, juce::Slider& s)   { att.push_back (std::make_shared<SA> (ap, EQ::id (b, n), s)); };
        auto ca = [&] (const char* n, juce::ComboBox& cb) { att.push_back (std::make_shared<CA> (ap, EQ::id (b, n), cb)); };
        auto ba = [&] (const char* n, juce::Button& bt)   { att.push_back (std::make_shared<BA> (ap, EQ::id (b, n), bt)); };
        sa ("freq", freqBar);  sa ("gain", gainBar);  sa ("q", qBar);  sa ("slp", slopeBar);
        // Slope moves in fixed steps (EQ::slopeSteps), each as wide as the others
        slopeBar.setNormalisableRange (EQ::slopeRange<double> (3.0));
        slopeBar.onClick = [this]               // click: the list of steps (no drag, no wheel)
        {
            juce::PopupMenu m;
            const int cur = EQ::slopeStepIndex (slopeBar.getValue()), first = EQ::slopeStepIndex (slopeBar.getMinimum());
            for (int i = first; i < EQ::numSlopeSteps; ++i)
                m.addItem (i + 1, EQ::fmtSlope (EQ::slopeSteps[i]), true, i == cur);
            juce::Component::SafePointer<ValueField> sp (&slopeBar);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slopeBar), [sp] (int id)
            {
                if (id > 0 && sp != nullptr) sp->setValue ((double) EQ::slopeSteps[id - 1], juce::sendNotificationSync);
            });
        };
        ba ("dyn", dynBtn);  ba ("dauto", autoBtn);  ba ("dfol", followBtn);  ca ("ddet", detFld.box);
        sa ("dthr", thrBar);  sa ("dwid", widBar);  sa ("gain", restBar);  sa ("dgain", actBar);
        sa ("dfreq", dFreqBar);  sa ("dq", dQBar);
        sa ("datk", atkBar);  sa ("drel", relBar);  sa ("dmix", mixBar);
        ca ("lxs", xShapeRow.box);  ca ("lxd", xSyncFld);  sa ("lxr", xRateBar);  sa ("lxa", xAmtBar);  sa ("lxm", xSmBar);
        ca ("lys", yShapeRow.box);  ca ("lyd", ySyncFld);  sa ("lyr", yRateBar);  sa ("lya", yAmtBar);  sa ("lym", ySmBar);

        auto ctx = [this, b] (ValueField& f, const char* n)
        {
            const auto pid = EQ::id (b, n);
            f.onContext = [this, pid] { if (onParamMenu) onParamMenu (pid); };
        };
        ctx (freqBar, "freq");  ctx (gainBar, "gain");  ctx (qBar, "q");  ctx (slopeBar, "slp");
        ctx (thrBar, "dthr");   ctx (widBar, "dwid");   ctx (restBar, "gain");  ctx (actBar, "dgain");
        ctx (dFreqBar, "dfreq"); ctx (dQBar, "dq");
        ctx (atkBar, "datk");   ctx (relBar, "drel");   ctx (mixBar, "dmix");
        ctx (xRateBar, "lxr");  ctx (xAmtBar, "lxa");   ctx (xSmBar, "lxm");
        ctx (yRateBar, "lyr");  ctx (yAmtBar, "lya");   ctx (ySmBar, "lym");

        for (auto* v : bars())
            v->setBandColour (c);
        for (auto* f : { &xSyncFld, &ySyncFld })
            f->setBandColour (c);
        for (auto* sg : { &xShapeRow, &yShapeRow })
            sg->setBandColour (c);
        for (auto* t : { &dynBtn, &autoBtn, &followBtn })
            t->setBandColour (c);
        detFld.setBandColour (c);

        sig = -1;
        toFront (false);       // the editor decides whether the card is shown (double-click on a band)
        tick();
        repaint();
    }

    // called from the editor timer: keeps visibility / enabled state in step with the parameters, refreshes live previews
    void tick()
    {
        if (band < 0) return;
        const int type = (int) std::lround (val ("type"));
        const bool vGain = EQ::hasGain (type);
        const bool dynOn = val ("dyn") > 0.5f, autoOn = val ("dauto") > 0.5f;
        const bool xs = val ("lxd") > 0.5f, ys = val ("lyd") > 0.5f;
        const bool xOn = val ("lxs") > 0.5f, yOn = val ("lys") > 0.5f;
        const bool xRnd = (int) std::lround (val ("lxs")) == EQ::LfoRandom, yRnd = (int) std::lround (val ("lys")) == EQ::LfoRandom;
        const int split = vGain ? (int) std::lround (val ("split")) : 0;
        const bool onePole = EQ::hasSlope (type) && val ("slp") < 6.05f;     // 6 dB/oct and below is a single pole or less: no resonance to control
        const int det = juce::jlimit (0, 2, (int) std::lround (val ("ddet")));             // 0 Band, 1 Full, 2 Sidechain
        const bool follow = val ("dfol") > 0.5f;

        const int s = tab | (type << 2) | (dynOn ? 1 << 6 : 0) | (autoOn ? 1 << 7 : 0) | (xs ? 1 << 8 : 0) | (ys ? 1 << 9 : 0)
                      | (xOn ? 1 << 10 : 0) | (yOn ? 1 << 11 : 0) | (split << 12) | (band << 16) | (xRnd ? 1 << 24 : 0) | (yRnd ? 1 << 25 : 0)
                      | (onePole ? 1 << 26 : 0) | (follow ? 1 << 27 : 0) | (det << 28);
        if (s != sig)
        {
            sig = s;
            gainBar.setCaption (split == 1 ? "Gain  L" : split == 2 ? "Gain  M" : "Gain");
            restBar.setCaption (split == 1 ? "Rest  L" : split == 2 ? "Rest  M" : "Rest");
            // the cut filters start at 3 dB/oct; the bell starts at its ordinary shape, 12 dB/oct
            const double slopeMin = type == EQ::Peak ? 12.0 : 3.0;
            if (slopeBar.getMinimum() != slopeMin)
                slopeBar.setNormalisableRange (EQ::slopeRange<double> (slopeMin));
            yAmtBar.setCaption (vGain ? "Amount" : "Amount (Q)");
            for (size_t i = 0; i < tabs.size(); ++i)
                tabs[i].setToggleState ((int) i == tab, juce::dontSendNotification);

            qBar.setEnabled (! onePole);
            thrBar.setEnabled (dynOn && ! autoOn);
            for (auto* v : { &widBar, &restBar, &actBar, &atkBar, &relBar, &mixBar, &dFreqBar, &dQBar })
                v->setEnabled (dynOn);
            detFld.setEnabled (dynOn);
            autoBtn.setEnabled (dynOn);
            followBtn.setEnabled (dynOn && det == 0);       // Full and Sidechain listen to an unfiltered signal: nothing to follow
            dQBar.setEnabled (dynOn && type == EQ::Peak);   // shelves use a fixed detector slope, only the bell has a detector Q
            xRateBar.setEnabled (xOn && ! xs);  xAmtBar.setEnabled (xOn);  xSmBar.setEnabled (xOn);  xSyncFld.setEnabled (xOn);
            yRateBar.setEnabled (yOn && ! ys);  yAmtBar.setEnabled (yOn);  ySmBar.setEnabled (yOn);  ySyncFld.setEnabled (yOn);
            relayout();
        }

        soloBtn.setToggleState (val ("solo") > 0.5f, juce::dontSendNotification);
        muteBtn.setToggleState (val ("mute") > 0.5f, juce::dontSendNotification);

        if (tab == 1 && gcRect.getWidth() > 0)       // the curve follows the fields, the automation and the host even without audio
        {
            const std::array<float, 5> now { val ("gain"), val ("dgain"), val ("dthr"), val ("dwid"), dynOn ? 1.0f : 0.0f };
            if (now != gcLast) { gcLast = now; repaint (gcRect.expanded (2)); }
        }

        if (live || liveWas)
        {
            if (tab == 1 && gcRect.getWidth() > 0) repaint (gcRect.expanded (2));
            if (tab == 2) { repaint (stripX.expanded (2)); repaint (stripY.expanded (2)); }
        }
        liveWas = live;
    }

    // keep the card inside its parent
    void keepInside()
    {
        if (auto* par = getParentComponent())
            setTopLeftPosition (juce::jlimit (0, juce::jmax (0, par->getWidth() - getWidth()), getX()),
                                juce::jlimit (0, juce::jmax (0, par->getHeight() - getHeight()), getY()));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 12.0f);
        g.setColour (kCard);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col.withAlpha (0.75f));
        g.drawRoundedRectangle (r, 12.0f, 1.2f);

        if (band < 0) return;

        // header: numbered chip + band type (click to open the full band menu)
        const auto chip = Rectangle<float> (22.0f, 22.0f).withCentre ({ 24.0f, 20.0f });
        g.setColour (col.withAlpha (0.25f));
        g.fillEllipse (chip.expanded (3.0f));
        g.setColour (Colour (0xff121212));
        g.fillEllipse (chip);
        g.setColour (col);
        g.drawEllipse (chip, 1.7f);
        g.setColour (kText);
        g.setFont (uiFont (11.0f));
        g.drawText (String (band + 1), chip, Justification::centred);

        const int type = (int) std::lround (val ("type"));
        const auto nameFont = uiFont (14.0f, true);
        g.setFont (nameFont);
        g.setColour (kText);
        g.drawText (EQ::typeNames()[type], nameRect, Justification::centredLeft);
        g.setColour (kText.withAlpha (0.4f));
        g.setFont (uiFont (11.0f));
        const int split = EQ::hasGain (type) ? (int) std::lround (val ("split")) : 0;
        const auto sub = split == 1 ? String ("L | R mirrored") : split == 2 ? String ("M | S mirrored")
                                                                              : EQ::channelNames()[(int) std::lround (val ("ch"))];
        g.drawText (sub, nameRect.withTrimmedLeft (juce::jmin (nameRect.getWidth() - 40, (int) textWidth (nameFont, EQ::typeNames()[type]) + 8)),
                    Justification::centredLeft);

        g.setColour (kLine);
        g.drawHorizontalLine (headerH - 2, 10.0f, (float) getWidth() - 10.0f);

        // captions
        g.setFont (uiFont (11.0f));
        g.setColour (kText.withAlpha (0.45f));
        for (auto& c : caps)
            g.drawText (c.text, c.r, Justification::centredLeft);

        if (tab == 1) paintGainComputer (g);
        if (tab == 2)
        {
            paintStrip (g, stripX, (int) std::lround (val ("lxs")), proc.lfoPhX[(size_t) band].load(), val ("lxs") > 0.5f, val ("lxm") * 0.01f);
            paintStrip (g, stripY, (int) std::lround (val ("lys")), proc.lfoPhY[(size_t) band].load(), val ("lys") > 0.5f, val ("lym") * 0.01f);
        }
        if (! msg.isEmpty())
        {
            g.setColour (kText.withAlpha (0.5f));
            g.setFont (12.0f);
            g.drawFittedText (msg, msgRect, Justification::centred, 3);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = false;
        if (gcActiveNow() && ! e.mods.isPopupMenu() && gcRect.contains (e.getPosition()))
        {
            gcMouseDown (e);                       // a handle of the Rest -> Active curve
            if (gcDrag != gcNone) return;
        }
        dragging = e.y < headerH;
        if (dragging) dragger.startDraggingComponent (this, e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (gcDrag != gcNone) { gcMouseDrag (e); return; }
        if (! dragging) return;
        dragger.dragComponent (this, e, nullptr);
        keepInside();
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto h = gcActiveNow() && gcRect.contains (e.getPosition()) ? gcHit (e.position) : gcNone;
        if (h != gcHover)
        {
            gcHover = h;
            setMouseCursor (h == gcNone ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
            repaint (gcRect.expanded (2));
        }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (gcHover != gcNone) { gcHover = gcNone; setMouseCursor (juce::MouseCursor::NormalCursor); repaint (gcRect.expanded (2)); }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (gcDrag != gcNone) { gcMouseUp(); return; }
        if (dragging && e.mouseWasDraggedSinceMouseDown())
        {
            if (onMoved) onMoved();
        }
        else if (dragging && nameRect.contains (e.getPosition()) && onMenu)
            onMenu();
        dragging = false;
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}   // never pass the wheel on to the graph

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Cap { juce::String text; juce::Rectangle<int> r; };

    static constexpr int headerH = 42, fieldH = 44, colGap = 12;

    float val (const char* n) const { return proc.apvts.getRawParameterValue (EQ::id (band, n))->load(); }

    std::vector<ValueField*> bars()
    {
        return { &freqBar, &gainBar, &qBar, &thrBar, &widBar, &restBar, &actBar, &dFreqBar, &dQBar, &atkBar, &relBar, &mixBar,
                 &xRateBar, &xAmtBar, &yRateBar, &yAmtBar, &xSmBar, &ySmBar, &slopeBar };
    }
    std::vector<juce::Component*> bodyComps()
    {
        std::vector<juce::Component*> v;
        for (auto* b : bars()) v.push_back (b);
        for (auto* c : std::initializer_list<juce::Component*> { &detFld, &xShapeRow, &xSyncFld, &yShapeRow, &ySyncFld,
                                                                  &dynBtn, &autoBtn, &followBtn })
            v.push_back (c);
        return v;
    }

    void relayout()
    {
        for (auto* c : bodyComps()) c->setVisible (false);
        caps.clear();
        msg = {};
        gcRect = stripX = stripY = {};

        const int pad = 14, fw = W - 2 * pad;
        const int type = (int) std::lround (val ("type"));
        const bool vGain = EQ::hasGain (type), vSlope = EQ::usesSlope (type);

        // header
        {
            auto h = getLocalBounds().withHeight (headerH).reduced (pad - 2, 0).withTrimmedTop (9).withHeight (24);
            closeBtn.setBounds (h.removeFromRight (24));  h.removeFromRight (2);
            muteBtn.setBounds (h.removeFromRight (24));   h.removeFromRight (2);
            soloBtn.setBounds (h.removeFromRight (24));   h.removeFromRight (6);
            nextBtn.setBounds (h.removeFromRight (22));   h.removeFromRight (1);
            prevBtn.setBounds (h.removeFromRight (22));   h.removeFromRight (6);
            h.removeFromLeft (36);
            nameRect = h.withHeight (24);
        }

        int y = headerH;
        {
            const int tw = (fw - 8) / 3;       // segmented icon tabs
            for (size_t i = 0; i < tabs.size(); ++i)
                tabs[i].setBounds (pad + (int) i * (tw + 4), y, tw, 28);
            y += 28 + 12;
        }

        auto put = [&] (juce::Component& c, juce::Rectangle<int> r) { c.setBounds (r); c.setVisible (true); };
        auto caption = [&] (const juce::String& t, juce::Rectangle<int> r) { caps.push_back ({ t, r }); };

        const int c3 = (fw - 2 * colGap) / 3;                 // three readouts per row
        auto col3 = [&] (int i) { return pad + i * (c3 + colGap); };

        if (tab == 0)
        {
            int c = 0;
            put (freqBar, { col3 (c++), y, c3, fieldH });
            if (vGain) put (gainBar, { col3 (c++), y, c3, fieldH });
            put (qBar, { col3 (c++), y, c3, fieldH });
            if (vSlope && c < 3) put (slopeBar, { col3 (c++), y, c3, fieldH });
            y += fieldH + 8;
            if (vSlope && ! slopeBar.isVisible()) { put (slopeBar, { col3 (0), y, c3, fieldH }); y += fieldH + 8; }
        }
        else if (tab == 1)
        {
            if (! vGain)
            {
                msg = "Dynamics works on bands that have gain:\nPeak, Shelves and Tilt.";
                msgRect = { pad, y, fw, 70 };
                y += 80;
            }
            else
            {
                // Dynamic | Detector | Auto | Follow  (Detector is wider: "Sidechain")
                const int g4 = 8, wd = 98, wn = (fw - wd - 3 * g4) / 3;
                put (dynBtn,    { pad,                              y, wn, fieldH });
                put (detFld,    { pad + wn + g4,                    y, wd, fieldH });
                put (autoBtn,   { pad + wn + g4 + wd + g4,          y, wn, fieldH });
                put (followBtn, { pad + wn + g4 + wd + g4 + wn + g4, y, wn, fieldH });
                y += fieldH + 10;
                gcRect = { pad, y, fw, 92 };
                y += 92 + 12;
                put (restBar, { col3 (0), y, c3, fieldH });
                put (actBar,  { col3 (1), y, c3, fieldH });
                put (thrBar,  { col3 (2), y, c3, fieldH });
                y += fieldH + 8;
                put (widBar, { col3 (0), y, c3, fieldH });
                put (atkBar, { col3 (1), y, c3, fieldH });
                put (relBar, { col3 (2), y, c3, fieldH });
                y += fieldH + 8;
                put (mixBar, { col3 (0), y, c3, fieldH });
                if (val ("ddet") < 0.5f && val ("dfol") < 0.5f)       // Band detector with its own filter
                {
                    put (dFreqBar, { col3 (1), y, c3, fieldH });
                    if (type == EQ::Peak) put (dQBar, { col3 (2), y, c3, fieldH });
                }
                y += fieldH + 4;
            }
        }
        else
        {
            auto section = [&] (const char* title, SegRow& shape, ChoiceField& sync, juce::Rectangle<int>& strip,
                                ValueField& rate, ValueField& amt, ValueField& smooth, bool random)
            {
                caption (title, { pad, y, fw, 14 });
                y += 16;
                put (shape, { pad, y, fw, 30 });
                y += 30 + 6;
                strip = { pad, y, fw, 30 };
                y += 30 + 8;
                if (random)      // Random adds Smooth: 0 = stepped, 100 = glides the whole way
                {
                    const int g4 = 10, c4 = (fw - 3 * g4) / 4;
                    put (sync,   { pad + 0 * (c4 + g4), y, c4, fieldH });
                    put (rate,   { pad + 1 * (c4 + g4), y, c4, fieldH });
                    put (amt,    { pad + 2 * (c4 + g4), y, c4, fieldH });
                    put (smooth, { pad + 3 * (c4 + g4), y, c4, fieldH });
                }
                else
                {
                    put (sync, { col3 (0), y, c3, fieldH });
                    put (rate, { col3 (1), y, c3, fieldH });
                    put (amt,  { col3 (2), y, c3, fieldH });
                }
                y += fieldH + 12;
            };
            section ("X  -  frequency", xShapeRow, xSyncFld, stripX, xRateBar, xAmtBar, xSmBar, (int) std::lround (val ("lxs")) == EQ::LfoRandom);
            section (vGain ? "Y  -  gain" : "Y  -  Q", yShapeRow, ySyncFld, stripY, yRateBar, yAmtBar, ySmBar, (int) std::lround (val ("lys")) == EQ::LfoRandom);
            y -= 4;
        }

        y += 4;
        if (getHeight() != y)
        {
            setSize (W, y);
            keepInside();
            if (onSizeChanged) onSizeChanged();
        }
        repaint();
    }

    // ---- the Rest -> Active curve: level (x, -60 ... 0 dB) against the band's gain (y). Handles can be dragged.
    enum GcHandle { gcNone = 0, gcRest, gcActive, gcThr, gcWidL, gcWidR };

    struct GcGeom
    {
        juce::Rectangle<float> plot;
        float span = 6.0f, thr = -24.0f, wid = 6.0f, rest = 0.0f, active = -3.0f;
        static constexpr float lo = -60.0f, hi = 0.0f;
        float X (float lv) const { return plot.getX() + plot.getWidth() * (lv - lo) / (hi - lo); }
        float Y (float gd) const { return plot.getCentreY() - plot.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, gd / span); }
        float levelAt (float x) const { return lo + (hi - lo) * (x - plot.getX()) / plot.getWidth(); }
        float gainAt (float y) const { return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * span; }
        juce::Point<float> pos (GcHandle h) const
        {
            const float xl = juce::jlimit (plot.getX(), plot.getRight(), X (thr - wid * 0.5f));
            const float xr = juce::jlimit (plot.getX(), plot.getRight(), X (thr + wid * 0.5f));
            switch (h)
            {
                case gcRest:   return { plot.getX() + 9.0f, Y (rest) };
                case gcActive: return { plot.getRight() - 9.0f, Y (active) };
                case gcThr:    return { X (thr), Y (0.5f * (rest + active)) };
                case gcWidL:   return { xl, Y (rest) };
                case gcWidR:   return { xr, Y (active) };
                default:       return {};
            }
        }
    };

    GcGeom gcGeom() const
    {
        GcGeom q;
        q.plot = gcRect.toFloat().reduced (8.0f, 8.0f);
        q.rest = val ("gain");  q.active = val ("dgain");  q.thr = val ("dthr");  q.wid = val ("dwid");
        if (val ("dauto") > 0.5f && live) q.thr = proc.effThr[(size_t) band].load();
        // the vertical scale: +-span dB, in steps of 3. Frozen while a handle is being dragged, so it does not slide under the mouse.
        q.span = gcSpanFrozen > 0.0f ? gcSpanFrozen
                                     : juce::jlimit (6.0f, 30.0f, std::ceil (juce::jmax (std::abs (q.rest), std::abs (q.active)) * 1.3f / 3.0f) * 3.0f);
        return q;
    }

    bool gcActiveNow() const { return tab == 1 && band >= 0 && ! gcRect.isEmpty() && val ("dyn") > 0.5f && EQ::hasGain ((int) std::lround (val ("type"))); }

    GcHandle gcHit (juce::Point<float> p) const
    {
        const auto q = gcGeom();
        const bool autoOn = val ("dauto") > 0.5f;
        GcHandle best = gcNone;
        float bestD = 10.0f;                                    // pick the nearest handle within 10 px
        for (auto h : { gcRest, gcActive, gcWidL, gcWidR, gcThr })     // feet first: when they sit on the threshold, they win and the width can grow
        {
            if (h == gcThr && autoOn) continue;                 // the Auto threshold is not draggable
            const float d = p.getDistanceFrom (q.pos (h));
            if (d < bestD) { bestD = d; best = h; }
        }
        return best;
    }

    void gcSetParam (const char* n, float v)
    {
        if (auto* pr = proc.apvts.getParameter (EQ::id (band, n)))
            pr->setValueNotifyingHost (pr->convertTo0to1 (pr->getNormalisableRange().snapToLegalValue (v)));
    }

    static const char* gcParam (GcHandle h)
    {
        switch (h) { case gcRest: return "gain"; case gcActive: return "dgain"; case gcThr: return "dthr"; default: return "dwid"; }
    }

    void gcMouseDown (const juce::MouseEvent& e)
    {
        gcDrag = gcHit (e.position);
        if (gcDrag == gcNone) return;
        const auto q = gcGeom();
        gcSpanFrozen = q.span;
        gcLastPos = e.position;
        gcValue = gcDrag == gcRest ? q.rest : gcDrag == gcActive ? q.active : gcDrag == gcThr ? q.thr : q.wid;
        if (auto* pr = proc.apvts.getParameter (EQ::id (band, gcParam (gcDrag)))) pr->beginChangeGesture();
        repaint (gcRect.expanded (2));
    }

    void gcMouseDrag (const juce::MouseEvent& e)
    {
        if (gcDrag == gcNone) return;
        const auto q = gcGeom();
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;      // Shift = fine
        const auto d = (e.position - gcLastPos) * fine;
        gcLastPos = e.position;
        const float dxDb = d.x / q.plot.getWidth() * (GcGeom::hi - GcGeom::lo);
        const float dyDb = -d.y / (q.plot.getHeight() * 0.5f) * q.span;
        switch (gcDrag)
        {
            case gcRest:   gcValue = juce::jlimit (-q.span, q.span, gcValue + dyDb);  gcSetParam ("gain",  gcValue); break;
            case gcActive: gcValue = juce::jlimit (-q.span, q.span, gcValue + dyDb);  gcSetParam ("dgain", gcValue); break;
            case gcThr:    gcValue = juce::jlimit (-60.0f, 0.0f, gcValue + dxDb);     gcSetParam ("dthr",  gcValue); break;
            case gcWidL:   gcValue = juce::jlimit (0.1f, 40.0f, gcValue - 2.0f * dxDb); gcSetParam ("dwid", gcValue); break;
            case gcWidR:   gcValue = juce::jlimit (0.1f, 40.0f, gcValue + 2.0f * dxDb); gcSetParam ("dwid", gcValue); break;
            default: break;
        }
        repaint (gcRect.expanded (2));
    }

    void gcMouseUp()
    {
        if (gcDrag == gcNone) return;
        if (auto* pr = proc.apvts.getParameter (EQ::id (band, gcParam (gcDrag)))) pr->endChangeGesture();
        gcDrag = gcNone;
        gcSpanFrozen = 0.0f;
        repaint (gcRect.expanded (2));
    }

    void paintGainComputer (juce::Graphics& g) const
    {
        using namespace juce;
        const auto r = gcRect.toFloat();
        if (r.isEmpty()) return;
        const bool dynOn = val ("dyn") > 0.5f, autoOn = val ("dauto") > 0.5f;
        const auto q = gcGeom();
        const auto& plot = q.plot;

        g.setColour (kField);
        g.fillRoundedRectangle (r, 6.0f);

        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) q.Y (0.0f), plot.getX(), plot.getRight());
        g.setColour (kAmber.withAlpha (dynOn ? 0.55f : 0.25f));
        g.drawVerticalLine ((int) q.X (q.thr), plot.getY(), plot.getBottom());

        // the transition zone, lightly shaded
        if (dynOn)
        {
            const float xl = jlimit (plot.getX(), plot.getRight(), q.X (q.thr - q.wid * 0.5f));
            const float xr = jlimit (plot.getX(), plot.getRight(), q.X (q.thr + q.wid * 0.5f));
            g.setColour (kAmber.withAlpha (0.07f));
            g.fillRect (Rectangle<float> (xl, plot.getY(), juce::jmax (1.0f, xr - xl), plot.getHeight()));
        }

        Path curve;
        for (int i = 0; i <= 120; ++i)
        {
            const float lv = GcGeom::lo + (GcGeom::hi - GcGeom::lo) * (float) i / 120.0f;
            const float gd = dynOn ? dynGain (lv, q.thr, q.wid, q.rest, q.active) : q.rest;
            const auto pt = Point<float> (q.X (lv), q.Y (gd));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        g.setColour (col.withAlpha (dynOn ? 0.95f : 0.35f));
        g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour (kText.withAlpha (0.4f));
        g.setFont (9.0f);
        g.drawText ("level " + String (GcGeom::lo, 0) + " ... 0 dB", r.reduced (8.0f, 2.0f), Justification::bottomRight);
        g.drawText (juce::String::fromUTF8 ("\xc2\xb1") + String (q.span, 0) + " dB", r.reduced (8.0f, 2.0f), Justification::topLeft);

        if (dynOn)
        {
            // handles: Rest and Active (up / down), threshold (left / right), width (the two feet of the slope)
            auto dot = [&] (GcHandle h, Colour c, bool ring)
            {
                if (h == gcThr && autoOn) return;
                const auto p = q.pos (h);
                const bool on = h == gcDrag || (gcDrag == gcNone && h == gcHover);
                const float rad = (ring ? 4.0f : 4.5f) + (on ? 1.5f : 0.0f);
                if (h == gcWidL || h == gcWidR)
                {
                    g.setColour (on ? c : c.withAlpha (0.7f));
                    g.drawRect (Rectangle<float> (rad * 1.4f, rad * 1.4f).withCentre (p), 1.4f);
                    return;
                }
                g.setColour (Colour (0xff121212));
                g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p));
                g.setColour (on ? Colours::white : c);
                if (ring) g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p), 1.6f);
                else      g.fillEllipse (Rectangle<float> (rad * 2.0f - 3.0f, rad * 2.0f - 3.0f).withCentre (p));
            };
            dot (gcWidL, kAmber, true);  dot (gcWidR, kAmber, true);
            dot (gcThr, kAmber, true);
            dot (gcRest, col, false);    dot (gcActive, col, false);

            // while a handle is dragged (or under the mouse) its exact value is shown in the corner
            const GcHandle h = gcDrag != gcNone ? gcDrag : gcHover;
            if (h != gcNone)
            {
                const char* pid = gcParam (h);
                if (auto* pr = proc.apvts.getParameter (EQ::id (band, pid)))
                {
                    const String name = h == gcRest ? "Rest" : h == gcActive ? "Active" : h == gcThr ? "Threshold" : "Width";
                    g.setColour (kText.withAlpha (0.85f));
                    g.setFont (monoFont (11.0f));
                    g.drawText (name + "  " + pr->getCurrentValueAsText(), r.reduced (8.0f, 2.0f), Justification::topRight);
                }
            }

            if (live)    // live dot: where the signal is on the curve right now
            {
                const float lv = jlimit (GcGeom::lo, GcGeom::hi, proc.effLevel[(size_t) band].load());
                g.setColour (Colours::white);
                g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ q.X (lv), q.Y (dynGain (lv, q.thr, q.wid, q.rest, q.active)) }));
            }
        }
    }

    void paintStrip (juce::Graphics& g, juce::Rectangle<int> rc, int shape, float phase, bool on, float smooth) const
    {
        using namespace juce;
        const auto r = rc.toFloat();
        if (r.isEmpty()) return;
        g.setColour (kField);
        g.fillRoundedRectangle (r, 6.0f);
        if (! on) return;
        const auto plot = r.reduced (8.0f, 5.0f);

        const float periods = shape == EQ::LfoRandom || shape == EQ::LfoDrift ? 4.0f : 2.0f;       // Random shows a few cycles so the glide is visible
        Path p;
        const int n = 240;
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n * periods;
            const float v = lfoShape (shape, x, smooth);
            const auto pt = Point<float> (plot.getX() + plot.getWidth() * x / periods, plot.getCentreY() - v * plot.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (col.withAlpha (0.9f));
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

        if (live)
        {
            const float px = plot.getX() + plot.getWidth() * phase / periods;     // first period shows the live position
            g.setColour (Colours::white.withAlpha (0.35f));
            g.drawVerticalLine ((int) px, plot.getY(), plot.getBottom());
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ px, plot.getCentreY() - lfoShape (shape, phase, smooth) * plot.getHeight() * 0.5f }));
        }
    }

    EQProcessor& proc;
    FieldLook fieldLook;
    int band = -1, tab = 0, sig = -1;
    bool live = false, liveWas = false, dragging = false;
    juce::Colour col { kAccent };
    juce::ComponentDragger dragger;

    std::array<IconButton, 3> tabs;
    IconButton prevBtn, nextBtn, soloBtn, muteBtn, closeBtn;
    ValueField freqBar, gainBar, qBar, thrBar, widBar, restBar, actBar, dFreqBar, dQBar, atkBar, relBar, mixBar,
               xRateBar, xAmtBar, yRateBar, yAmtBar, xSmBar, ySmBar, slopeBar;
    ChoiceField xSyncFld { "Sync", fieldLook }, ySyncFld { "Sync", fieldLook };
    SegRow xShapeRow { shapeItems() }, yShapeRow { shapeItems() };
    CycleField detFld { "Detector", EQ::detectorNames() };
    ToggleField dynBtn { "Dynamic" }, autoBtn { "Auto" }, followBtn { "Follow" };

    // the Rest -> Active curve: handle under the mouse / being dragged, and the state of the drag
    GcHandle gcHover = gcNone, gcDrag = gcNone;
    juce::Point<float> gcLastPos;
    float gcValue = 0.0f, gcSpanFrozen = 0.0f;
    std::array<float, 5> gcLast {};
    std::vector<std::shared_ptr<void>> att;

    std::vector<Cap> caps;
    juce::String msg;
    juce::Rectangle<int> nameRect, msgRect, gcRect, stripX, stripY;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandCard)
};
}   // namespace UI
