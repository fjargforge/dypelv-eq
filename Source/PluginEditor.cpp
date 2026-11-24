#include "PluginEditor.h"
#include "Presets.h"
#include "Settings.h"

namespace
{
const juce::Colour kBg (0xff0d0d0d), kPanel (0xff131313), kAccent (0xff5eead4), kText (0xffe8e8e8);
const juce::Colour kIcon (0xffb2b2b2), kRed (0xfff87171), kAmber (0xfffbbf24), kMeter (0xffd2d2d2);
const juce::Colour kPalette[] = { juce::Colour (0xff5eead4), juce::Colour (0xff60a5fa), juce::Colour (0xffa78bfa),
                                  juce::Colour (0xfff0abfc), juce::Colour (0xfffbbf24), juce::Colour (0xffa3e635),
                                  juce::Colour (0xff38bdf8), juce::Colour (0xff34d399), juce::Colour (0xfffb923c),
                                  juce::Colour (0xff818cf8), juce::Colour (0xff22d3ee), juce::Colour (0xffe879f9) };
int gPalette = 0;      // 0 colour, 1 soft, 2 mono (a setting of the person, shared by all instances)
juce::Colour bandColour (int b)
{
    const auto c = kPalette[b % 12];
    if (gPalette == 1) return c.withMultipliedSaturation (0.42f).withMultipliedBrightness (0.97f);      // the same hues, quieter
    if (gPalette == 2) return juce::Colour (0xffc6c6c6);                                                // all bands one neutral light grey
    return c;
}

// stereo channel colours: one glance tells L, R, M and S apart
juce::Colour channelColour (int ch)
{
    switch (ch)
    {
        case EQ::Left:  return juce::Colour (0xff5aa9ff);     // blue
        case EQ::Right: return juce::Colour (0xffff7a6b);     // coral
        case EQ::Mid:   return juce::Colour (0xff4fe0a0);     // green
        case EQ::Side:  return juce::Colour (0xffc08bff);     // violet
        default:        return juce::Colour (0xffb2b2b2);
    }
}
const char* channelLetter (int ch)
{
    switch (ch) { case EQ::Left: return "L"; case EQ::Right: return "R"; case EQ::Mid: return "M"; case EQ::Side: return "S"; default: return ""; }
}

// Round badge: channel colour with its letter. `ring` (optional) is drawn around it, dashed for the second side of a split
// band, the same dashes as that side's curve on the graph.
void drawChannelBadge (juce::Graphics& g, juce::Point<float> c, float d, int ch, juce::Colour ring, bool dashedRing)
{
    const auto r = juce::Rectangle<float> (d, d).withCentre (c);
    if (! ring.isTransparent())
    {
        const auto rr = r.expanded (d * 0.15f);
        g.setColour (ring);
        if (dashedRing)
        {
            juce::Path circle, dashes;
            circle.addEllipse (rr);
            const float dl[] = { 2.0f, 3.0f };
            juce::PathStrokeType (1.3f).createDashedStroke (dashes, circle, dl, 2);
            g.fillPath (dashes);
        }
        else
            g.drawEllipse (rr, 1.3f);
    }
    g.setColour (channelColour (ch).withAlpha (0.96f));
    g.fillEllipse (r);
    g.setColour (juce::Colour (0xff131313));
    g.setFont (UI::uiFont (d * 0.66f, true));
    g.drawText (channelLetter (ch), r.translated (0.0f, -0.5f), juce::Justification::centred, false);
}

constexpr float anaRanges[] = { 60.0f, 90.0f, 120.0f };
constexpr float anaDecay[]  = { 0.6f, 1.6f, 4.0f };
constexpr float anaAvg[]    = { 0.0f, 5.0f, 15.0f, 60.0f };      // spectrum averaging: time constant in seconds, 0 = off (peak hold with fall instead)
constexpr float anaTilt[]   = { 0.0f, 3.0f, 4.5f };
constexpr float anaSmooth[] = { 0.0f, 1.0f / 12.0f, 1.0f / 6.0f, 1.0f / 3.0f };
constexpr float specStep = 3.0f;        // analyzer path point spacing in px (2 = finer, a bit heavier)
constexpr int curveStepPx = 2;          // EQ curve sampling: one point every this many px (1 = finest); extra points around every band's centre stay
constexpr double plotMinIntervalMs = 28.0;   // the plot repaints at most ~30 times a second unless a band is being dragged (0 = no limit)
constexpr int kIdleFrames = 15;         // timer ticks without a new FFT frame before the display starts to fall

// Note-name octave convention for the drag readout.
// -2 : C3 = middle C (A3 = 440 Hz), so 1.8 kHz shows as A5.
// -1 : scientific pitch notation (A4 = 440 Hz), 1.8 kHz would show as A6.
constexpr int kNoteOctaveShift = -2;

juce::String fmtFreqShort (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, 3) + "kHz";
    if (f >= 100.0f)  return juce::String ((int) std::lround (f)) + "Hz";
    return juce::String (f, 1) + "Hz";
}

juce::String noteName (float f)
{
    static const char* names[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int m = (int) std::lround (69.0 + 12.0 * std::log2 ((double) f / 440.0));
    return juce::String (names[((m % 12) + 12) % 12]) + juce::String ((int) std::floor ((double) m / 12.0) + kNoteOctaveShift);
}

juce::String lvlStr (float v) { return v <= -119.0f ? juce::String ("-inf") : juce::String (v, 1); }

juce::File presetDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    d = d.getChildFile ("Application Support");
   #endif
    const auto previous = d.getChildFile ("PrismEQ").getChildFile ("Presets");   // folder used before the plugin was renamed
    d = d.getChildFile ("Dypelv").getChildFile ("Presets");
    if (! d.isDirectory())
    {
        d.createDirectory();
        for (const auto& f : previous.findChildFiles (juce::File::findFiles, false, "*.prismpreset"))   // carry old presets over, once
            f.copyFileTo (d.getChildFile (f.getFileNameWithoutExtension() + ".dypelvpreset"));
    }
    return d;
}

float textWidth (const juce::Font& font, const juce::String& s) { return UI::textWidth (font, s); }     // cached, see BandCard.h

// ---------------------------------------------------------------- menu / button icons (20 x 16 box)
juce::Path typeIconPath (int t)
{
    juce::Path p;
    switch (t)
    {
        case EQ::LowShelf:  p.startNewSubPath (1, 4);  p.cubicTo (9, 4, 11, 11, 19, 11); break;
        case EQ::HighShelf: p.startNewSubPath (1, 11); p.cubicTo (9, 11, 11, 4, 19, 4); break;
        case EQ::Peak:      p.startNewSubPath (1, 12); p.cubicTo (6, 12, 7, 3, 10, 3); p.cubicTo (13, 3, 14, 12, 19, 12); break;
        case EQ::Notch:     p.startNewSubPath (1, 4);  p.lineTo (6, 4); p.cubicTo (8, 4, 9, 13, 10, 13); p.cubicTo (11, 13, 12, 4, 14, 4); p.lineTo (19, 4); break;
        case EQ::LowPass:   p.startNewSubPath (1, 4);  p.lineTo (9, 4); p.cubicTo (13, 4, 15, 11, 19, 14); break;
        case EQ::HighPass:  p.startNewSubPath (1, 14); p.cubicTo (5, 11, 7, 4, 11, 4); p.lineTo (19, 4); break;
        case EQ::BandPass:  p.startNewSubPath (1.5f, 15); p.cubicTo (5, 11.5f, 7.5f, 3, 10, 3); p.cubicTo (12.5f, 3, 15, 11.5f, 18.5f, 15); break;   // rounded top, sloping legs (a bell has flat shoulders)
        case EQ::TiltShelf: p.startNewSubPath (1, 3);  p.cubicTo (8, 3, 8, 4, 10, 8); p.cubicTo (12, 12, 12, 13, 19, 13); break;
        default:            p.startNewSubPath (1, 3);  p.lineTo (19, 13); break;   // flat tilt
    }
    return p;
}

// Menu icons that need real text are drawn into a small image (20 x 16 box, 3x resolution) instead of being stroked paths.
std::unique_ptr<juce::Drawable> imageIcon (const std::function<void (juce::Graphics&)>& draw)
{
    constexpr int sc = 3;
    juce::Image img (juce::Image::ARGB, 20 * sc, 16 * sc, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) sc));
        draw (g);
    }
    auto d = std::make_unique<juce::DrawableImage>();
    d->setImage (img);
    return d;
}

std::unique_ptr<juce::Drawable> channelMenuIcon (int c)
{
    return imageIcon ([c] (juce::Graphics& g)
    {
        if (c == EQ::Stereo)      // two overlapping round badges, blue and coral
        {
            g.setColour (channelColour (EQ::Left).withAlpha (0.65f));
            g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 7.0f, 8.0f }));
            g.setColour (channelColour (EQ::Right).withAlpha (0.65f));
            g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 13.0f, 8.0f }));
        }
        else
            drawChannelBadge (g, { 10.0f, 8.0f }, 14.0f, c, juce::Colours::transparentBlack, false);
    });
}

// The letters of the stereo icons are stroked outlines with one cap height (7.2), so L, R, M and S are always the same size.
juce::Path letterPath (int ch, float cx)
{
    juce::Path p;
    const float t = 4.4f, m = 8.0f, b = 11.6f;
    switch (ch)
    {
        case EQ::Left:
            p.startNewSubPath (cx - 2.3f, t);  p.lineTo (cx - 2.3f, b);  p.lineTo (cx + 2.6f, b);
            break;
        case EQ::Right:
            p.startNewSubPath (cx - 2.4f, b);  p.lineTo (cx - 2.4f, t);  p.lineTo (cx + 0.5f, t);
            p.quadraticTo (cx + 3.0f, t, cx + 3.0f, (t + m) * 0.5f);
            p.quadraticTo (cx + 3.0f, m, cx + 0.5f, m);  p.lineTo (cx - 2.4f, m);
            p.startNewSubPath (cx + 0.3f, m);  p.lineTo (cx + 3.0f, b);
            break;
        case EQ::Mid:
            p.startNewSubPath (cx - 3.0f, b);  p.lineTo (cx - 3.0f, t);  p.lineTo (cx, m + 1.2f);
            p.lineTo (cx + 3.0f, t);  p.lineTo (cx + 3.0f, b);
            break;
        default:   // Side
            p.startNewSubPath (cx + 2.6f, 5.9f);
            p.cubicTo (cx + 2.2f, t - 0.2f, cx - 2.6f, t - 0.2f, cx - 2.6f, 6.2f);
            p.cubicTo (cx - 2.6f, m + 0.2f, cx + 2.6f, m - 0.2f, cx + 2.6f, 9.9f);
            p.cubicTo (cx + 2.6f, b + 0.2f, cx - 2.2f, b + 0.2f, cx - 2.6f, 10.1f);
            break;
    }
    return p;
}

// Split menu icons. kind 0 = free line (off), 1 = L | R, 2 = M | S, 3 = L | R with the 2nd side inverted, 4 = M | S, 2nd inverted.
// The letters are always upright; the divider between them becomes an up / down arrow when the 2nd side is inverted.
std::unique_ptr<juce::Drawable> splitMenuIcon (int kind)
{
    return imageIcon ([kind] (juce::Graphics& g)
    {
        if (kind == 0)
        {
            g.setColour (kIcon);
            g.fillRect (juce::Rectangle<float> (2.0f, 7.4f, 16.0f, 1.4f));
            return;
        }
        const bool ms = kind == 2 || kind == 4, inverted = kind >= 3;
        const int a = ms ? EQ::Mid : EQ::Left, b = ms ? EQ::Side : EQ::Right;
        const juce::PathStrokeType st (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (channelColour (a));
        g.strokePath (letterPath (a, 4.6f), st);
        g.setColour (channelColour (b));
        g.strokePath (letterPath (b, 16.2f), st);
        g.setColour (kIcon.withAlpha (0.8f));
        if (! inverted)
            g.fillRect (juce::Rectangle<float> (9.9f, 2.5f, 1.0f, 11.0f));
        else
        {
            juce::Path arrow;
            arrow.startNewSubPath (10.8f, 3.0f);  arrow.lineTo (10.8f, 13.0f);
            arrow.startNewSubPath (9.8f, 4.3f);   arrow.lineTo (10.8f, 3.0f);   arrow.lineTo (11.8f, 4.3f);
            arrow.startNewSubPath (9.8f, 11.7f);  arrow.lineTo (10.8f, 13.0f);  arrow.lineTo (11.8f, 11.7f);
            g.strokePath (arrow, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    });
}

// Mirrored pair: two bells that mirror each other (upper = 1st side, lower = 2nd side). kind 0 = off, 1 = L | R, 2 = M | S.
// kind -1 = the same shape in neutral grey (for the sub-menu itself).
std::unique_ptr<juce::Drawable> pairMenuIcon (int kind)
{
    if (kind == 0) return splitMenuIcon (0);
    return imageIcon ([kind] (juce::Graphics& g)
    {
        const auto up = kind < 0 ? kIcon : channelColour (kind == 1 ? EQ::Left : EQ::Mid);
        const auto dn = kind < 0 ? kIcon : channelColour (kind == 1 ? EQ::Right : EQ::Side);
        auto bell = [&] (float dir, juce::Colour c)
        {
            juce::Path p;
            p.startNewSubPath (1.0f, 8.0f);
            p.cubicTo (5.0f, 8.0f, 7.0f, 8.0f - 5.8f * dir, 10.0f, 8.0f - 5.8f * dir);
            p.cubicTo (13.0f, 8.0f - 5.8f * dir, 15.0f, 8.0f, 19.0f, 8.0f);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        bell (1.0f, up);
        bell (-1.0f, dn);
    });
}

juce::Path forkIconPath()       // one band splits into two
{
    juce::Path p;
    p.startNewSubPath (1.0f, 8.0f);   p.lineTo (6.0f, 8.0f);
    p.cubicTo (10.0f, 8.0f, 10.0f, 3.5f, 14.0f, 3.5f);    p.lineTo (19.0f, 3.5f);
    p.startNewSubPath (6.0f, 8.0f);
    p.cubicTo (10.0f, 8.0f, 10.0f, 12.5f, 14.0f, 12.5f);  p.lineTo (19.0f, 12.5f);
    return p;
}

juce::Path arrowsIconPath (bool vertical)    // frequency (left / right) and gain (up / down)
{
    juce::Path p;
    if (! vertical)
    {
        p.startNewSubPath (2.0f, 8.0f);  p.lineTo (18.0f, 8.0f);
        p.startNewSubPath (5.0f, 4.8f);  p.lineTo (2.0f, 8.0f);   p.lineTo (5.0f, 11.2f);
        p.startNewSubPath (15.0f, 4.8f); p.lineTo (18.0f, 8.0f);  p.lineTo (15.0f, 11.2f);
    }
    else
    {
        p.startNewSubPath (10.0f, 1.5f); p.lineTo (10.0f, 14.5f);
        p.startNewSubPath (6.8f, 4.5f);  p.lineTo (10.0f, 1.5f);  p.lineTo (13.2f, 4.5f);
        p.startNewSubPath (6.8f, 11.5f); p.lineTo (10.0f, 14.5f); p.lineTo (13.2f, 11.5f);
    }
    return p;
}

juce::Path widthIconPath()      // Q = bandwidth: two bars with an arrow between them
{
    juce::Path p;
    p.startNewSubPath (2.5f, 2.5f);  p.lineTo (2.5f, 13.5f);
    p.startNewSubPath (17.5f, 2.5f); p.lineTo (17.5f, 13.5f);
    p.startNewSubPath (5.5f, 8.0f);  p.lineTo (14.5f, 8.0f);
    p.startNewSubPath (8.0f, 5.5f);  p.lineTo (5.5f, 8.0f);   p.lineTo (8.0f, 10.5f);
    p.startNewSubPath (12.0f, 5.5f); p.lineTo (14.5f, 8.0f);  p.lineTo (12.0f, 10.5f);
    return p;
}

// A piano keyboard (four white keys, two black ones): the MIDI learn icon
std::unique_ptr<juce::Drawable> keyboardIcon (juce::Colour c = kIcon)
{
    return imageIcon ([c] (juce::Graphics& g)
    {
        g.setColour (c);
        g.drawRoundedRectangle (juce::Rectangle<float> (1.5f, 2.5f, 17.0f, 11.0f), 1.8f, 1.2f);
        g.fillRect (juce::Rectangle<float> (4.8f, 2.9f, 2.0f, 5.6f));      // black keys
        g.fillRect (juce::Rectangle<float> (9.0f, 2.9f, 2.0f, 5.6f));
        g.drawLine (5.8f, 8.5f, 5.8f, 13.0f, 1.0f);                        // white key edges
        g.drawLine (10.0f, 8.5f, 10.0f, 13.0f, 1.0f);
        g.drawLine (14.2f, 3.0f, 14.2f, 13.0f, 1.0f);
    });
}

juce::Path shapeIconPath (int s)
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

juce::Path headphonePath()   // centred on (0,0), about 15 wide
{
    juce::Path p;
    p.addCentredArc (0.0f, 1.0f, 5.5f, 5.5f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    p.addRoundedRectangle (-7.5f, 0.5f, 3.2f, 6.0f, 1.2f);
    p.addRoundedRectangle (4.3f, 0.5f, 3.2f, 6.0f, 1.2f);
    return p;
}

juce::Path mutePath (bool muted)   // speaker, centred on (0,0), about 12 wide: a sound wave when on, a cross when muted
{
    juce::Path p;
    p.startNewSubPath (-6.0f, -2.0f);  p.lineTo (-3.5f, -2.0f);  p.lineTo (-0.5f, -4.8f);
    p.lineTo (-0.5f, 4.8f);  p.lineTo (-3.5f, 2.0f);  p.lineTo (-6.0f, 2.0f);  p.closeSubPath();
    if (muted) { p.startNewSubPath (2.2f, -2.4f); p.lineTo (6.0f, 2.4f); p.startNewSubPath (6.0f, -2.4f); p.lineTo (2.2f, 2.4f); }
    else       { p.startNewSubPath (2.4f, -3.0f); p.cubicTo (5.2f, -1.2f, 5.2f, 1.2f, 2.4f, 3.0f); }
    return p;
}

juce::Path actionIconPath (int which)   // 0 mute, 1 solo, 2 delete, 3 dynamic, 4 invert
{
    juce::Path p;
    if (which == 0) { p = mutePath (true); p.applyTransform (juce::AffineTransform::scale (1.2f).translated (10.0f, 8.0f)); }
    else if (which == 1) { auto h = headphonePath(); h.applyTransform (juce::AffineTransform::translation (10.0f, 8.0f)); p = h; }
    else if (which == 2) { p.startNewSubPath (4, 2.5f); p.lineTo (16, 13.5f); p.startNewSubPath (16, 2.5f); p.lineTo (4, 13.5f); }
    else if (which == 4)   // invert
    {
        p.startNewSubPath (6, 13); p.lineTo (6, 3); p.startNewSubPath (3, 6); p.lineTo (6, 3); p.lineTo (9, 6);
        p.startNewSubPath (14, 3); p.lineTo (14, 13); p.startNewSubPath (11, 10); p.lineTo (14, 13); p.lineTo (17, 10);
    }
    else { p.startNewSubPath (1, 13); p.lineTo (7, 13); p.lineTo (12, 3); p.lineTo (19, 3); }
    return p;
}

std::unique_ptr<juce::Drawable> makeIcon (const juce::Path& path, juce::Colour c)
{
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath (path);
    d->setFill (juce::FillType (juce::Colours::transparentBlack));
    d->setStrokeFill (juce::FillType (c));
    d->setStrokeType (juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return d;
}

void addDrawableItem (juce::PopupMenu& m, int id, const juce::String& text, std::unique_ptr<juce::Drawable> icon, bool current,
                      bool enabled = true)
{
    juce::PopupMenu::Item it (text);
    it.itemID = id;
    it.isEnabled = enabled;
    it.image = std::move (icon);
    if (current) it.colour = kAccent;
    m.addItem (std::move (it));
}

void addIconItem (juce::PopupMenu& m, int id, const juce::String& text, const juce::Path& icon, bool current,
                  juce::Colour tint = juce::Colours::transparentBlack)
{
    juce::PopupMenu::Item it (text);
    it.itemID = id;
    it.isEnabled = true;
    it.image = makeIcon (icon, tint.isTransparent() ? (current ? kAccent : kIcon) : tint);
    if (current) it.colour = kAccent;
    else if (! tint.isTransparent()) it.colour = tint;
    m.addItem (std::move (it));
}
}

// ================================================================ EQEditor
EQEditor::EQEditor (EQProcessor& p) : AudioProcessorEditor (&p), proc (p), card (p)
{
    setLookAndFeel (&look);

    for (size_t i = 0; i < 4; ++i)
    {
        ffts[i] = std::make_unique<juce::dsp::FFT> (12 + (int) i);
        windows[i] = std::make_unique<juce::dsp::WindowingFunction<float>> ((size_t) (4096 << i),
                                                                            juce::dsp::WindowingFunction<float>::blackmanHarris);
    }
    fftData.assign (2 * (size_t) EQProcessor::fftMax, 0.0f);

    addAndMakeVisible (bypassBtn);
    bypassBtn.setPath (UI::powerPath());
    bypassBtn.setColours (kAmber, kText.withAlpha (0.8f));
    bypassBtn.setTooltip ("Bypass");
    bypassBtn.setClickingTogglesState (true);
    bypassA = std::make_unique<BA> (proc.apvts, "bypass", bypassBtn);

    addAndMakeVisible (analyzerBtn);
    analyzerBtn.setPath (UI::spectrumPath());
    analyzerBtn.setColours (kAccent, kText.withAlpha (0.8f));
    analyzerBtn.setTooltip ("Display settings");
    analyzerBtn.onClick = [this] { if (! menuBlocked()) showSettingsMenu(); };

    addAndMakeVisible (phaseBtn);
    phaseBtn.setCentred (true);
    phaseBtn.setTooltip ("Phase mode: Zero Latency, or Linear Phase (adds latency, no phase shift)");
    phaseBtn.onClick = [this] { if (! menuBlocked()) showPhaseMenu(); };

    addAndMakeVisible (fullBtn);
    fullBtn.setPath (UI::fullscreenPath());
    fullBtn.setColours (kAccent, kText.withAlpha (0.8f));
    fullBtn.setTooltip ("Full screen");
    fullBtn.onClick = [this] { toggleFullscreen(); };

    addAndMakeVisible (presetBtn);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    presetBtn.setCentred (true);
    presetBtn.setAlwaysUnderlined (true);
    presetBtn.setTooltip ("Presets");
    prevBtn.setPath (UI::chevronPath (-1));  nextBtn.setPath (UI::chevronPath (1));
    prevBtn.setTooltip ("Previous preset");  nextBtn.setTooltip ("Next preset");
    for (auto* b : { &prevBtn, &nextBtn, &undoBtn, &redoBtn })
        b->setFlat (true);
    presetBtn.onClick = [this] { if (! menuBlocked()) showPresetMenu(); };
    prevBtn.onClick   = [this] { stepPreset (-1); };
    nextBtn.onClick   = [this] { stepPreset (1); };

    // undo / redo, A / B, delta listen, auto gain match
    for (auto* b : { &abA, &abB })
    {
        addAndMakeVisible (b);
        b->setCentred (true);
        b->setFontHeight (16.0f);
    }
    abA.setTooltip ("Setting A");
    abB.setTooltip ("Setting B");
    undoBtn.setTooltip ("Undo");
    redoBtn.setTooltip ("Redo");
    for (auto* b : { &undoBtn, &redoBtn })
        addAndMakeVisible (b);
    undoBtn.setPath (UI::undoPath());
    redoBtn.setPath (UI::redoPath());
    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };
    abA.onClick = [this] { switchAB (0); };
    abB.onClick = [this] { switchAB (1); };
    for (auto* b : { &deltaBtn, &matchBtn })
        addAndMakeVisible (b);
    deltaBtn.setPath (UI::deltaPath());
    matchBtn.setPath (UI::matchPath());
    deltaBtn.setColours (kAmber, kText.withAlpha (0.8f));
    matchBtn.setColours (kAccent, kText.withAlpha (0.8f));
    deltaBtn.setTooltip ("Delta: listen to what the EQ adds or removes");
    matchBtn.setTooltip ("Auto gain match: keeps the loudness equal for fair A/B");
    deltaBtn.setClickingTogglesState (true);
    matchBtn.setClickingTogglesState (true);
    deltaA = std::make_unique<BA> (proc.apvts, "delta", deltaBtn);
    matchA = std::make_unique<BA> (proc.apvts, "autogain", matchBtn);

    // output gain: a trim after the EQ and the auto gain
    outField.setInline (true);
    outField.setTooltip ("Output gain.  Drag, or double-click to type;  Alt-click resets");
    outField.setBandColour (kAccent);
    addAndMakeVisible (outField);
    outA = std::make_unique<SA> (proc.apvts, "out_gain", outField);
    outField.onContext = [this] { showParamMenu ("out_gain"); };

    // dB range drop-down
    rangeBox.addItemList (juce::StringArray { "+/- 3 dB", "+/- 6 dB", "+/- 12 dB", "+/- 18 dB", "+/- 22 dB", "+/- 30 dB" }, 1);
    rangeBox.setLookAndFeel (&barLook);
    addAndMakeVisible (rangeBox);
    addAndMakeVisible (faqBtn);
    faqBtn.setTooltip ("Quick guide");
    faqBtn.onClick = [this] { showFaq(); };
    rangeA = std::make_unique<CA> (proc.apvts, "range", rangeBox);

    // floating band card
    addChildComponent (card);

    // the header controls change only through their own repaint(): cache them, so the 60 Hz plot repaint does not re-shape their text
    for (juce::Component* hc : std::initializer_list<juce::Component*> { &bypassBtn, &analyzerBtn, &phaseBtn, &fullBtn, &presetBtn, &prevBtn, &nextBtn,
                                                                         &abA, &abB, &undoBtn, &redoBtn, &deltaBtn, &matchBtn, &outField, &rangeBox, &faqBtn })
        hc->setBufferedToImage (true);
    card.onMenu  = [this]
    {
        if (selected < 0) return;
        if (menuBlocked()) return;      // the click that closed the menu must not open it again
        showBandMenu (selected, card.getNameScreenArea());
    };
    card.onParamMenu = [this] (const juce::String& pid) { showParamMenu (pid); };
    card.onPrev  = [this] { stepBand (-1); };
    card.onNext  = [this] { stepBand (1); };
    card.onClose = [this] { selectBand (-1); };
    card.onSolo  = [this] { if (selected >= 0) toggleSolo (selected); };
    card.onSizeChanged = [this] { positionCard(); };
    card.onMoved = [this]
    {
        // remember the drop spot against the plot (distance from its top edge and from the nearer side edge): it survives resizing,
        // full screen and closing the window, and the card always comes back to exactly this place
        const auto r = card.getBounds();
        const bool right = (float) r.getCentreX() > plotRect.getCentreX();
        proc.cardRight.store (right);
        proc.cardDX.store (juce::jmax (0.0f, right ? plotRect.getRight() - (float) r.getRight() : (float) r.getX() - plotRect.getX()));
        proc.cardDY.store ((float) r.getY() - plotRect.getY());
    };

    // parameters covered by undo / redo / A-B: all band parameters
    for (auto* prm : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto pid = rp->paramID;
            if ((pid.length() > 2 && pid[0] == 'b' && juce::CharacterFunctions::isDigit (pid[1])))
                tracked.push_back (rp);
        }
    lastSeen = lastStable = snapshot();
    watcher.ticks = &inputTicks;
    addMouseListener (&watcher, true);

    // settings of the person (shared by all instances): film grain, band colours, interface scale, first-run hint
    Grain::enabled = Settings::getBool ("grain", true);
    gPalette = juce::jlimit (0, 2, Settings::getInt ("palette", 0));
    uiScalePct = Settings::getInt ("uiScale", 100);
    if (uiScalePct != 100 && uiScalePct != 125 && uiScalePct != 150 && uiScalePct != 200) uiScalePct = 100;
    hintVisible = ! Settings::getBool ("hintSeen", false);
    for (int b = 0; b < EQ::numBands && hintVisible; ++b)
        if (get (b, "on") > 0.5f) hintVisible = false;        // a session with bands needs no introduction
    if (! hintVisible) Settings::setBool ("hintSeen", true);

    setSize (1240, 700);
    setResizable (true, true);
    setResizeLimits (1120, 560, 4096, 2560);
    setWantsKeyboardFocus (true);
    if (uiScalePct != 100) applyUiScale (uiScalePct);
    startTimerHz (60);

    // The 60 Hz timer drifts against the screen refresh, so the curve used to stall for a frame and then take two steps at once
    // while the number on the card moved evenly. This callback runs exactly once per screen refresh and redraws the curve then.
    vblank = juce::VBlankAttachment (this, [this] { vblankTick(); });
}

EQEditor::~EQEditor()
{
    stopTimer();
    removeMouseListener (&watcher);
    endDrag();
    rangeBox.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------- helpers
// parameter lookup without building a String and searching by name on every call (they are read hundreds of times per frame)
std::atomic<float>* EQEditor::paramPtr (int band, const char* name) const
{
    uint64_t h = 1469598103934665603ull ^ (uint64_t) (band + 2);
    h *= 1099511628211ull;
    for (const char* p = name; *p != 0; ++p) { h ^= (uint8_t) *p; h *= 1099511628211ull; }
    auto it = paramCache.find (h);
    if (it == paramCache.end())
        it = paramCache.emplace (h, proc.apvts.getRawParameterValue (band < 0 ? juce::String (name) : EQ::id (band, name))).first;
    return it->second;
}

int EQEditor::geti (const char* id) const { return (int) std::lround (paramPtr (-1, id)->load()); }
float EQEditor::get (int b, const char* name) const { return paramPtr (b, name)->load(); }
float EQEditor::targetRange() const { return EQ::rangeDbs[juce::jlimit (0, 5, geti ("range"))]; }
float EQEditor::range() const { return rangeShown > 0.0f ? rangeShown : targetRange(); }   // what is drawn: glides to the chosen range
float EQEditor::anaRange() const { return anaRanges[juce::jlimit (0, 2, geti ("ana_range"))]; }

void EQEditor::setParam (const juce::String& pid, float value)
{
    if (auto* p = proc.apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EQEditor::setParamGesture (const juce::String& pid, float value)
{
    if (auto* p = proc.apvts.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

float EQEditor::xForFreq (float f) const
{
    return plotRect.getX() + plotRect.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}
float EQEditor::freqForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - plotRect.getX()) / plotRect.getWidth()));
}
float EQEditor::zeroY() const { return plotRect.getY() + plotRect.getHeight() * kZeroFrac; }
float EQEditor::lowRange() const { return juce::jmin (30.0f, range() * (1.0f - kZeroFrac) / kZeroFrac); }
float EQEditor::yForGain (float g) const { return zeroY() - g / range() * (plotRect.getHeight() * kZeroFrac); }
float EQEditor::gainForY (float y) const { return (zeroY() - y) / (plotRect.getHeight() * kZeroFrac) * range(); }
float EQEditor::yForLevel (float db) const
{
    const float aR = anaRange();
    return juce::jmap (juce::jlimit (-aR, 0.0f, db), -aR, 0.0f, plotRect.getBottom(), plotRect.getY());
}

juce::Point<float> EQEditor::nodePos (int b) const
{
    const int type = (int) std::lround (get (b, "type"));
    const float y = EQ::hasGain (type) ? yForGain (get (b, "gain")) : zeroY();
    return { xForFreq (get (b, "freq")), juce::jlimit (plotRect.getY(), plotRect.getBottom(), y) };
}

// the second dot of a mirrored pair: same frequency, the mirrored gain (on the dashed curve)
juce::Point<float> EQEditor::nodePos2 (int b) const
{
    return { xForFreq (get (b, "freq")), juce::jlimit (plotRect.getY(), plotRect.getBottom(), yForGain (-get (b, "gain"))) };
}

bool EQEditor::animated (int b) const
{
    const int type = (int) std::lround (get (b, "type"));
    return get (b, "lxs") > 0.5f || get (b, "lys") > 0.5f || (get (b, "dyn") > 0.5f && EQ::hasGain (type));
}
float EQEditor::effFreq (int b) const { return audioActive && animated (b) ? proc.effF[(size_t) b].load() : get (b, "freq"); }
float EQEditor::effGain (int b) const { return audioActive && animated (b) ? proc.effG[(size_t) b].load() : get (b, "gain"); }
float EQEditor::effQ (int b) const { return audioActive && animated (b) ? proc.effQ[(size_t) b].load() : get (b, "q"); }
float EQEditor::effGain2 (int b) const
{
    return audioActive && animated (b) ? proc.effG2[(size_t) b].load() : -get (b, "gain");   // linked pair: mirrored gain
}

// Nodes are drawn lowest index first and the selected band last (on top), so hit-testing
// walks from the top: on a tie the selected band wins, otherwise the highest index.
int EQEditor::hitBand (juce::Point<float> pt, int* side) const
{
    int best = -1, bestSide = 0;
    float bestD = 14.0f;
    for (int b = EQ::numBands - 1; b >= 0; --b)
    {
        if (get (b, "on") < 0.5f) continue;
        const bool pair = EQ::hasGain ((int) std::lround (get (b, "type"))) && get (b, "split") > 0.5f;
        for (int s = pair ? 1 : 0; s >= 0; --s)       // a mirrored pair has two dots; on a tie the first one wins
        {
            const float d = (s == 1 ? nodePos2 (b) : nodePos (b)).getDistanceFrom (pt);
            const bool better = best < 0 ? d < 14.0f
                                         : (d < bestD - 0.5f || (b == selected && d < bestD + 0.5f));
            if (better) { best = b; bestD = d; bestSide = s; }
        }
    }
    if (side != nullptr) *side = bestSide;
    return best;
}

// ---------------------------------------------------------------- solo
int EQEditor::soloedBand() const
{
    for (int b = 0; b < EQ::numBands; ++b)
        if (get (b, "on") > 0.5f && get (b, "solo") > 0.5f)
            return b;
    return -1;
}

void EQEditor::toggleSolo (int b)
{
    const bool now = get (b, "solo") < 0.5f;
    if (now)   // only one soloed band at a time
        for (int o = 0; o < EQ::numBands; ++o)
            if (o != b && get (o, "solo") > 0.5f)
                setParamGesture (EQ::id (o, "solo"), 0.0f);
    setParamGesture (EQ::id (b, "solo"), now ? 1.0f : 0.0f);
}

// ---------------------------------------------------------------- band management
void EQEditor::selectBand (int b)
{
    selected = b;
    cardHiddenByDrag = false;
    if (b < 0) cardOpen = false;       // the card is opened by double-clicking a band; deselecting closes it
    card.setBand (b, b >= 0 ? bandColour (b) : juce::Colours::transparentBlack);
    if (b >= 0) positionCard();
    repaint();
}

// Jump to the next / previous active band, ordered by frequency.
void EQEditor::stepBand (int dir)
{
    std::vector<std::pair<float, int>> act;
    for (int b = 0; b < EQ::numBands; ++b)
        if (get (b, "on") > 0.5f) act.push_back ({ get (b, "freq"), b });
    if (act.empty()) return;
    std::sort (act.begin(), act.end());

    int idx = -1;
    for (size_t i = 0; i < act.size(); ++i)
        if (act[i].second == selected) { idx = (int) i; break; }
    const int n = (int) act.size();
    idx = idx < 0 ? (dir > 0 ? 0 : n - 1) : (idx + dir + n) % n;
    selectBand (act[(size_t) idx].second);
}

// The card always opens in the same place: top right of the plot, or wherever the user last dropped it (kept in the processor).
void EQEditor::positionCard()
{
    if (selected < 0 || plotRect.isEmpty()) return;
    const int w = card.getWidth(), h = card.getHeight();
    const float dx = proc.cardDX.load();
    int x, y;
    if (dx < 0.0f)
    {
        x = (int) plotRect.getRight() - w - 14;
        y = juce::jmin ((int) plotRect.getY() + 46, (int) plotRect.getBottom() - h - 8);      // a tall card slides up instead of leaving the plot
    }
    else
    {
        x = proc.cardRight.load() ? (int) std::lround (plotRect.getRight() - dx) - w : (int) std::lround (plotRect.getX() + dx);
        y = (int) std::lround (plotRect.getY() + proc.cardDY.load());
    }
    x = juce::jlimit (0, juce::jmax (0, getWidth() - w), x);       // same limits as the card's own dragging
    y = juce::jlimit (0, juce::jmax (0, getHeight() - h), y);
    card.setTopLeftPosition (x, y);
    card.setVisible (cardOpen && ! cardHiddenByDrag);
}

void EQEditor::resetBand (int b)
{
    for (auto name : { "on", "solo", "mute", "gain", "type", "ch", "q", "slp", "freq", "split",
                       "dyn", "dthr", "dknee", "datk", "drel", "dmix", "dauto", "dmode", "dratio", "dsc",
                       "lxs", "lxd", "lxr", "lxa", "lxm", "lys", "lyd", "lyr", "lya", "lym" })
        if (auto* p = proc.apvts.getParameter (EQ::id (b, name)))
            p->setValueNotifyingHost (p->getDefaultValue());
}

int EQEditor::addBand (juce::Point<float> pos, int type)
{
    for (int b = 0; b < EQ::numBands; ++b)
    {
        if (get (b, "on") > 0.5f) continue;
        resetBand (b);
        setParam (EQ::id (b, "type"), (float) type);
        setParam (EQ::id (b, "freq"), freqForX (pos.x));
        if (EQ::hasGain (type))
            setParam (EQ::id (b, "gain"), juce::jlimit (-lowRange(), range(), gainForY (pos.y)));
        if (EQ::hasSlope (type))
            setParam (EQ::id (b, "q"), 0.7071f);     // 0.707 = plain Butterworth
        if (type == EQ::TiltShelf)
            setParam (EQ::id (b, "q"), 0.5f);        // low Q = broad, flat tilt
        setParam (EQ::id (b, "on"), 1.0f);
        cardOpen = true;
        selectBand (b);
        dismissHint();
        return b;
    }
    return -1;
}

void EQEditor::resetAllBands()
{
    endDrag();
    for (int b = 0; b < EQ::numBands; ++b)
        resetBand (b);
    selectBand (-1);
}

void EQEditor::deleteBand (int b)
{
    if (dragBand == b) endDrag();
    resetBand (b);
    if (selected == b) selectBand (-1);
}

// Invert = flip the band's response: boost <-> cut for bands with gain, notch <-> band pass, low pass <-> high pass.
void EQEditor::invertBand (int b)
{
    const int type = (int) std::lround (get (b, "type"));
    if (EQ::hasGain (type))
    {
        setParamGesture (EQ::id (b, "gain"), -get (b, "gain"));      // a linked pair mirrors it automatically
    }
    else if (type == EQ::Notch)    setParamGesture (EQ::id (b, "type"), (float) EQ::BandPass);
    else if (type == EQ::BandPass) setParamGesture (EQ::id (b, "type"), (float) EQ::Notch);
    else if (type == EQ::LowPass)  setParamGesture (EQ::id (b, "type"), (float) EQ::HighPass);
    else if (type == EQ::HighPass) setParamGesture (EQ::id (b, "type"), (float) EQ::LowPass);
}

// Split: the band becomes two independent bands, one per side (Left + Right, or Mid + Side).
// The copy starts identical (or with the gain inverted) and is selected, so you can drag it away.
void EQEditor::splitBand (int b, int mode, bool inverted)
{
    int nb = -1;
    for (int i = 0; i < EQ::numBands; ++i)
        if (i != b && get (i, "on") < 0.5f) { nb = i; break; }
    if (nb < 0) return;                                   // no free band left

    const int type = (int) std::lround (get (b, "type"));
    const bool gainType = EQ::hasGain (type);
    const bool wasPair = gainType && get (b, "split") > 0.5f;
    if (wasPair) mode = (int) std::lround (get (b, "split"));   // separating a linked pair keeps its own mode

    const float g1 = get (b, "gain");
    const float g2 = wasPair ? -g1 : (inverted && gainType ? -g1 : g1);

    for (auto name : { "type", "freq", "q", "slp", "mute", "dyn", "dthr", "dknee", "datk", "drel", "dmix", "dauto", "dmode", "dratio", "dsc",
                       "lxs", "lxd", "lxr", "lxa", "lxm", "lys", "lyd", "lyr", "lya", "lym" })
        if (auto* src = proc.apvts.getParameter (EQ::id (b, name)))
            if (auto* dst = proc.apvts.getParameter (EQ::id (nb, name)))
                dst->setValueNotifyingHost (src->getValue());

    setParam (EQ::id (nb, "gain"), g2);
    for (int band : { b, nb })
    {
        setParam (EQ::id (band, "split"), 0.0f);
        setParam (EQ::id (band, "solo"), 0.0f);
    }
    setParam (EQ::id (b, "ch"), (float) (mode == 2 ? EQ::Mid : EQ::Left));
    setParam (EQ::id (nb, "ch"), (float) (mode == 2 ? EQ::Side : EQ::Right));
    setParam (EQ::id (nb, "on"), 1.0f);
    selectBand (nb);
}

// ---------------------------------------------------------------- drag (balanced gestures)
void EQEditor::beginDrag (int band, juce::Point<float> mouse, int side)
{
    endDrag();
    dragBand = band;
    dragSide = side;
    dragMoved = false;
    dragStart = mouse;
    dragOffset = (side == 1 ? nodePos2 (band) : nodePos (band)) - mouse;    // grab the dot where it was clicked: no jump
    for (auto name : { "freq", "gain" })
        if (auto* p = proc.apvts.getParameter (EQ::id (band, name)))
            p->beginChangeGesture();
}

void EQEditor::endDrag()
{
    if (dragBand < 0) return;
    for (auto name : { "freq", "gain" })
        if (auto* p = proc.apvts.getParameter (EQ::id (dragBand, name)))
            p->endChangeGesture();
    dragBand = -1;
    if (cardHiddenByDrag)
    {
        cardHiddenByDrag = false;
        if (selected >= 0 && cardOpen) { card.setVisible (true); positionCard(); }
    }
}

// ---------------------------------------------------------------- menus
// One place for every drop-down: remembers when a menu was dismissed, so the same click cannot open it again
void EQEditor::runMenu (juce::PopupMenu& m, const juce::PopupMenu::Options& o, std::function<void (int)> cb)
{
    menuOpen = true;
    m.showMenuAsync (o, [safe = juce::Component::SafePointer<EQEditor> (this), cb] (int r)
    {
        if (safe == nullptr) return;
        safe->menuOpen = false;
        if (r == 0) safe->menuClosedAt = juce::Time::getMillisecondCounter();
        cb (r);
    });
}

bool EQEditor::menuBlocked() const
{
    return menuOpen || juce::Time::getMillisecondCounter() - menuClosedAt < 450;
}

void EQEditor::showBandMenu (int band, juce::Rectangle<int> anchor)
{
    const int curType = (int) std::lround (get (band, "type")), curCh = (int) std::lround (get (band, "ch"));
    const int shX = (int) std::lround (get (band, "lxs")), shY = (int) std::lround (get (band, "lys"));
    const int split = (int) std::lround (get (band, "split"));
    const bool muted = get (band, "mute") > 0.5f, soloed = get (band, "solo") > 0.5f;
    const bool gainType = EQ::hasGain (curType);

    juce::PopupMenu m;
    m.addSectionHeader ("BAND TYPE");
    for (int t = 0; t < EQ::numTypes; ++t)
        addIconItem (m, 100 + t, EQ::typeNames()[t], typeIconPath (t), t == curType);
    m.addSeparator();
    m.addSectionHeader ("STEREO");
    for (int c = 0; c < 5; ++c)
        addDrawableItem (m, 200 + c, EQ::channelNames()[c], channelMenuIcon (c), c == curCh && split == 0);
    m.addSeparator();
    m.addSectionHeader ("SPLIT / INVERT");
    {
        juce::PopupMenu two;     // one band -> two independent bands
        addDrawableItem (two, 700, "Left | Right", splitMenuIcon (1), false);
        addDrawableItem (two, 701, "Mid | Side", splitMenuIcon (2), false);
        addDrawableItem (two, 702, "Left | Right, 2nd inverted", splitMenuIcon (3), false, gainType);      // inversion needs a gain
        addDrawableItem (two, 703, "Mid | Side, 2nd inverted", splitMenuIcon (4), false, gainType);
        m.addSubMenu ("Two independent bands", two, true, makeIcon (forkIconPath(), kIcon));
    }
    {
        juce::PopupMenu pair;    // two dots, two sides, the 2nd side always mirrored (inverted). Always listed, greyed out without a gain.
        for (int k = 0; k < 3; ++k)
            addDrawableItem (pair, 600 + k, EQ::splitNames()[k], pairMenuIcon (k), k == split, gainType);
        m.addSubMenu (gainType ? "Linked pair (mirrored):  " + EQ::splitNames()[split] : juce::String ("Linked pair (mirrored)"),
                      pair, gainType, pairMenuIcon (-1));
    }
    addIconItem (m, 8, "Invert response", actionIconPath (4), false);
    m.addSeparator();
    addIconItem (m, 1, muted ? "Unmute" : "Mute", actionIconPath (0), muted);
    addIconItem (m, 2, soloed ? "Unsolo" : "Solo", actionIconPath (1), soloed);

    juce::PopupMenu lfo, shapesX, shapesY;
    for (int s = 0; s < EQ::numLfoShapes; ++s)
    {
        addIconItem (shapesX, 400 + s, EQ::lfoShapeNames()[s], shapeIconPath (s), s == shX);
        addIconItem (shapesY, 500 + s, EQ::lfoShapeNames()[s], shapeIconPath (s), s == shY);
    }
    lfo.addSubMenu ("X  shape  (frequency)", shapesX, true, makeIcon (arrowsIconPath (false), kIcon));
    lfo.addSubMenu (gainType ? "Y  shape  (gain)" : "Y  shape  (Q)", shapesY, true, makeIcon (arrowsIconPath (true), kIcon));
    m.addSubMenu ("LFO   X: " + EQ::lfoShapeNames()[shX] + "   Y: " + EQ::lfoShapeNames()[shY], lfo, true,
                  makeIcon (shapeIconPath (EQ::LfoSine), kIcon));

    {
        juce::PopupMenu midi;     // MIDI learn: arm one parameter, then move a controller
        auto item = [&] (int id, const char* label, const char* pn, const juce::Path& icon)
        {
            const int cc = proc.midiCCFor (EQ::id (band, pn));
            addIconItem (midi, id, juce::String (label) + (cc >= 0 ? "   (CC " + juce::String (cc) + ")" : juce::String()), icon, false);
        };
        item (800, "Learn Frequency", "freq", arrowsIconPath (false));
        if (gainType) item (801, "Learn Gain", "gain", arrowsIconPath (true));
        item (802, "Learn Q", "q", widthIconPath());
        midi.addSeparator();
        addIconItem (midi, 810, "Clear MIDI of this band", actionIconPath (2), false);
        m.addSubMenu ("MIDI learn", midi, true, keyboardIcon());
    }

    m.addSeparator();
    addIconItem (m, 3, "Delete band", actionIconPath (2), false, kRed);

    auto opts = juce::PopupMenu::Options().withMinimumWidth (210);
    if (! anchor.isEmpty()) opts = opts.withTargetScreenArea (anchor);
    runMenu (m, opts,
                     [safe = juce::Component::SafePointer<EQEditor> (this), band] (int r)
    {
        if (safe == nullptr) return;
        if (r == 0) return;
        auto& e = *safe;
        if (r >= 100 && r < 200)
        {
            const int oldType = (int) std::lround (e.get (band, "type")), newType = r - 100;
            e.setParamGesture (EQ::id (band, "type"), (float) newType);
            if (EQ::usesSlope (oldType) && EQ::usesSlope (newType) && (oldType == EQ::Peak) != (newType == EQ::Peak))
                e.setParamGesture (EQ::id (band, "slp"), 12.0f);       // a bell is not a 48 dB/oct cut filter and the other way round
        }
        else if (r >= 200 && r < 300)
        {
            e.setParamGesture (EQ::id (band, "ch"), (float) (r - 200));
            e.setParamGesture (EQ::id (band, "split"), 0.0f);
        }
        else if (r >= 400 && r < 500) e.setParamGesture (EQ::id (band, "lxs"), (float) (r - 400));
        else if (r >= 500 && r < 600) e.setParamGesture (EQ::id (band, "lys"), (float) (r - 500));
        else if (r >= 600 && r < 700) e.setParamGesture (EQ::id (band, "split"), (float) (r - 600));
        else if (r >= 700 && r < 704) e.splitBand (band, (r - 700) % 2 + 1, r >= 702);
        else if (r == 1) e.setParamGesture (EQ::id (band, "mute"), e.get (band, "mute") > 0.5f ? 0.0f : 1.0f);
        else if (r == 2) e.toggleSolo (band);
        else if (r == 8) e.invertBand (band);
        else if (r == 3) e.deleteBand (band);
        else if (r >= 800 && r <= 802)
        {
            static const char* pn[] = { "freq", "gain", "q" };
            e.proc.armMidiLearn (EQ::id (band, pn[r - 800]));
        }
        else if (r == 810) e.proc.clearMidiBand (band);
    });
}

void EQEditor::showParamMenu (const juce::String& pid)
{
    const int cc = proc.midiCCFor (pid);
    juce::PopupMenu m;
    addDrawableItem (m, 1, "MIDI learn", keyboardIcon(), false);
    addDrawableItem (m, 2, cc >= 0 ? "Clear MIDI  (CC " + juce::String (cc) + ")" : juce::String ("Clear MIDI"),
                     makeIcon (actionIconPath (2), kIcon), false, cc >= 0);
    runMenu (m, juce::PopupMenu::Options().withMinimumWidth (150),
                     [safe = juce::Component::SafePointer<EQEditor> (this), pid] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->proc.armMidiLearn (pid);
        else if (r == 2) safe->proc.clearMidi (pid);
    });
}

void EQEditor::showPhaseMenu()
{
    const int cur = geti ("phase");
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    juce::PopupMenu m;
    m.addSectionHeader ("PHASE MODE");
    for (int i = 0; i < EQ::phaseNames().size(); ++i)
    {
        const int lat = proc.phaseLatency (i);
        const auto label = EQ::phaseNames()[i] + (lat > 0 ? "   " + juce::String (lat * 1000.0 / sr, 0) + " ms" : juce::String ("   0 ms"));
        m.addItem (10 + i, label, true, i == cur);
    }
    m.addSeparator();
    m.addItem (99, "Bands with an LFO or dynamics stay zero-latency", false);
    runMenu (m, juce::PopupMenu::Options().withTargetComponent (phaseBtn),
                     [safe = juce::Component::SafePointer<EQEditor> (this)] (int r)
    {
        if (safe != nullptr && r >= 10 && r < 14) safe->setParamGesture ("phase", (float) (r - 10));
    });
}

void EQEditor::toggleFullscreen()
{
    if (auto* dw = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))      // standalone app
    {
        dw->setFullScreen (! dw->isFullScreen());
        syncFullIcon();
        return;
    }
    if (! maximised)             // plugin window: grow the editor to the screen, restore on the second press
    {
        restoreSize = { getWidth(), getHeight() };
        const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
        const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (0, 0, 1920, 1080);
        const float sc = juce::jmax (0.5f, appliedScale);       // the editor's own units are smaller than the screen's when scaled up
        maximised = true;
        setSize (juce::jlimit (1120, 4096, (int) ((float) (area.getWidth() - 40) / sc)), juce::jlimit (560, 2560, (int) ((float) (area.getHeight() - 120) / sc)));
    }
    else
    {
        maximised = false;
        setSize (restoreSize.x, restoreSize.y);
    }
    syncFullIcon();
}

bool EQEditor::isFullNow() const
{
    if (auto* dw = dynamic_cast<const juce::DocumentWindow*> (getTopLevelComponent()))
        return dw->isFullScreen();
    return maximised;
}

void EQEditor::syncFullIcon()
{
    const bool full = isFullNow();
    if (full == lastFull) return;
    lastFull = full;
    fullBtn.setPath (full ? UI::windowedPath() : UI::fullscreenPath());
    fullBtn.setTooltip (full ? "Window mode" : "Full screen");
}

void EQEditor::showAddMenu (juce::Point<float> pos)
{
    juce::PopupMenu m;
    m.addSectionHeader ("ADD BAND");
    for (int t = 0; t < EQ::numTypes; ++t)
        addIconItem (m, 100 + t, EQ::typeNames()[t], typeIconPath (t), false);
    runMenu (m, juce::PopupMenu::Options().withMinimumWidth (170),
                     [safe = juce::Component::SafePointer<EQEditor> (this), pos] (int r)
    {
        if (safe != nullptr && r >= 100 && r < 200)
            safe->addBand (pos, r - 100);
    });
}

bool EQEditor::scaleFits (int percent) const
{
    // the smallest the window can be, scaled up, must still fit on the screen the editor is on
    const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
    const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (0, 0, 1920, 1080);
    const float sc = hostScale * (float) percent * 0.01f;
    return 1120.0f * sc <= (float) area.getWidth() && 560.0f * sc <= (float) area.getHeight();
}

void EQEditor::applyUiScale (int percent)
{
    uiScalePct = percent;
    const float sc = hostScale * (float) percent * 0.01f;
    appliedScale = sc;

    // when the scaled window would be bigger than the screen, the editor gets smaller (in its own units) to fit
    const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
    const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (0, 0, 1920, 1080);
    const int maxW = juce::jmax (1120, (int) std::floor ((float) area.getWidth() / sc));
    const int maxH = juce::jmax (560, (int) std::floor ((float) area.getHeight() / sc));
    if (getWidth() > maxW || getHeight() > maxH)
        setSize (juce::jmin (getWidth(), maxW), juce::jmin (getHeight(), maxH));

    if (std::abs (getTransform().getScaleFactor() - sc) > 1.0e-4f)
        setScaleFactor (sc);
    repaint();
}

void EQEditor::setPalette (int mode)
{
    gPalette = juce::jlimit (0, 2, mode);
    Settings::setInt ("palette", gPalette);
    if (selected >= 0)
    {
        card.setBand (selected, bandColour (selected));
        positionCard();
    }
    repaint();
}

void EQEditor::dismissHint()
{
    if (! hintVisible) return;
    hintVisible = false;
    hintImg = {};
    Settings::setBool ("hintSeen", true);
    repaint (hintRect.expanded (2.0f).getSmallestIntegerContainer());
}

// the first-run card: static, so it is rendered once into a small image and blitted
void EQEditor::drawHint (juce::Graphics& g)
{
    if (! hintVisible || hintRect.isEmpty() || ! g.getClipBounds().intersects (hintRect.getSmallestIntegerContainer())) return;

    const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw = juce::jmax (1, juce::roundToInt (hintRect.getWidth() * sc)), ph = juce::jmax (1, juce::roundToInt (hintRect.getHeight() * sc));
    if (hintImg.isNull() || hintImg.getWidth() != pw || hintImg.getHeight() != ph || hintImgScale != sc)
    {
        hintImgScale = sc;
        hintImg = juce::Image (juce::Image::ARGB, pw, ph, true, juce::NativeImageType());
        juce::Graphics ig (hintImg);
        ig.addTransform (juce::AffineTransform::scale (sc));

        const auto r = juce::Rectangle<float> (hintRect.getWidth(), hintRect.getHeight()).reduced (0.5f);
        ig.setColour (juce::Colour (0xf2151515));
        ig.fillRoundedRectangle (r, 8.0f);
        ig.setColour (juce::Colour (0xff343434));
        ig.drawRoundedRectangle (r, 8.0f, 1.0f);

        struct Row { const char* key; const char* what; };
        const Row rows[] = { { "Double-click",   "add a band" },
                             { "Right-click",    "band menu: type, channel, LFO, MIDI" },
                             { "Wheel",          "Q of the band under the pointer" },
                             { "S  /  M",        "solo / mute the selected band" } };
        float y = 20.0f;
        for (const auto& row : rows)
        {
            ig.setColour (kText);
            ig.setFont (UI::uiFont (13.0f, true));
            ig.drawText (row.key, juce::Rectangle<float> (24.0f, y, 120.0f, 20.0f), juce::Justification::centredLeft, false);
            ig.setColour (kText.withAlpha (0.55f));
            ig.setFont (UI::uiFont (13.0f));
            ig.drawText (row.what, juce::Rectangle<float> (150.0f, y, r.getWidth() - 170.0f, 20.0f), juce::Justification::centredLeft, false);
            y += 27.0f;
        }
        ig.setColour (kText.withAlpha (0.35f));
        ig.setFont (UI::uiFont (11.0f));
        ig.drawText ("Click to dismiss", juce::Rectangle<float> (0.0f, r.getBottom() - 28.0f, r.getWidth(), 18.0f), juce::Justification::centred, false);
    }
    g.drawImageTransformed (hintImg, juce::AffineTransform::scale (1.0f / sc).translated (hintRect.getX(), hintRect.getY()));
}

void EQEditor::showSettingsMenu()
{
    static const char* ids[]    = { "ana_res", "ana_range", "ana_decay", "ana_smooth", "ana_tilt", "ana_avg" };
    static const char* titles[] = { "Resolution", "Range", "Speed", "Smoothing", "Slope compensation", "Averaging" };
    static const int scales[]   = { 100, 125, 150, 200 };

    auto choiceMenu = [this] (int k)
    {
        juce::PopupMenu sub;
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (ids[k])))
            for (int i = 0; i < cp->choices.size(); ++i)
                sub.addItem (100 + k * 10 + i, cp->choices[i], true, i == cp->getIndex());
        return sub;
    };

    juce::PopupMenu m, adv, scaleMenu, palMenu;
    m.addItem (1, "Show analyzer", true, geti ("ana_on") != 0);
    m.addItem (4, "Show original (input) spectrum", true, geti ("ana_in") != 0);
    m.addItem (2, "Show grid", true, geti ("grid") != 0);
    m.addItem (5, "Show LUFS meter", true, geti ("show_lufs") != 0);
    m.addSeparator();
    m.addSubMenu ("Spectrum averaging", choiceMenu (5));        // slow ambient material: the picture settles over seconds
    for (int k = 0; k < 5; ++k)      // the fine-tuning lives under "Advanced"
        adv.addSubMenu (titles[k], choiceMenu (k));
    m.addSubMenu ("Advanced", adv);
    m.addSeparator();
    for (int i = 0; i < 4; ++i)
        scaleMenu.addItem (300 + i, juce::String (scales[i]) + " %", scaleFits (scales[i]), scales[i] == uiScalePct);
    m.addSubMenu ("Interface scale", scaleMenu);
    static const char* palNames[] = { "Colour", "Soft", "Mono" };
    for (int i = 0; i < 3; ++i)
        palMenu.addItem (310 + i, palNames[i], true, i == gPalette);
    m.addSubMenu ("Band colours", palMenu);
    m.addItem (6, "Film grain", true, Grain::enabled);
    m.addItem (7, "Show first-run hints");
    runMenu (m, juce::PopupMenu::Options().withTargetComponent (analyzerBtn),
                     [safe = juce::Component::SafePointer<EQEditor> (this)] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->setParam ("ana_on", safe->geti ("ana_on") ? 0.0f : 1.0f);
        else if (r == 2) safe->setParam ("grid", safe->geti ("grid") ? 0.0f : 1.0f);
        else if (r == 4) safe->setParam ("ana_in", safe->geti ("ana_in") ? 0.0f : 1.0f);
        else if (r == 5) safe->setParam ("show_lufs", safe->geti ("show_lufs") ? 0.0f : 1.0f);
        else if (r == 6)
        {
            Grain::enabled = ! Grain::enabled;
            Settings::setBool ("grain", Grain::enabled);
            safe->repaint();
        }
        else if (r == 7)
        {
            Settings::setBool ("hintSeen", false);
            safe->hintVisible = true;
            safe->hintImg = {};
            safe->repaint();
        }
        else if (r >= 300 && r < 304)
        {
            Settings::setInt ("uiScale", scales[r - 300]);
            safe->applyUiScale (scales[r - 300]);
        }
        else if (r >= 310 && r < 313) safe->setPalette (r - 310);
        else if (r >= 100 && r < 200) safe->setParam (ids[(r - 100) / 10], (float) ((r - 100) % 10));
    });
}

// ---------------------------------------------------------------- presets
juce::String EQEditor::currentPresetName() const
{
    return proc.apvts.state.getProperty ("preset").toString();
}

void EQEditor::setPresetName (const juce::String& n)
{
    proc.apvts.state.setProperty ("preset", n, nullptr);
}

std::vector<PresetEntry> EQEditor::presetEntries() const
{
    std::vector<PresetEntry> list;
    const auto& fp = Presets::factory();
    for (size_t i = 0; i < fp.size(); ++i)
    {
        PresetEntry e;
        e.name = fp[i].name;
        e.factory = (int) i;
        list.push_back (e);
    }

    std::vector<juce::File> files;
    for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.dypelvpreset"))
        files.push_back (f);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    for (const auto& f : files)
    {
        PresetEntry e;
        e.name = f.getFileNameWithoutExtension();
        e.file = f;
        list.push_back (e);
    }
    return list;
}

void EQEditor::showPresetMenu()
{
    const auto entries = presetEntries();
    const auto cur = currentPresetName();

    juce::PopupMenu m, factory, user;
    m.addItem (1, "Init (all bands off)");
    m.addSeparator();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        auto& target = entries[i].factory >= 0 ? factory : user;
        target.addItem (1000 + (int) i, entries[i].name, true, entries[i].name == cur);
    }
    m.addSubMenu ("Factory", factory);
    if (user.getNumItems() > 0)
        m.addSubMenu ("User", user);
    m.addSeparator();
    m.addItem (2, "Save current as...");
    m.addItem (3, "Show presets folder");

    runMenu (m, juce::PopupMenu::Options().withTargetComponent (presetBtn),
                     [safe = juce::Component::SafePointer<EQEditor> (this), entries] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1)      { safe->resetAllBands(); safe->setPresetName ("Init"); }
        else if (r == 2) safe->promptSavePreset();
        else if (r == 3) presetDir().revealToUser();
        else if (r >= 1000 && (size_t) (r - 1000) < entries.size())
            safe->loadEntry (entries[(size_t) (r - 1000)]);
    });
}

void EQEditor::promptSavePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    const auto cur = currentPresetName();
    w->addTextEditor ("name", cur.isEmpty() ? juce::String ("My preset") : cur);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true,
                        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<EQEditor> (this), w] (int result)
                        {
                            if (result == 1 && safe != nullptr)
                                safe->savePreset (w->getTextEditorContents ("name").trim());
                        }),
                        true);
}

void EQEditor::savePreset (const juce::String& name)
{
    if (name.isEmpty()) return;

    juce::XmlElement xml ("DypelvPreset");
    xml.setAttribute ("name", name);
    for (auto* prm : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto pid = rp->paramID;   // band parameters look like "b12_freq"
            if (pid.length() > 2 && pid[0] == 'b' && juce::CharacterFunctions::isDigit (pid[1]))
            {
                auto* e = xml.createNewChildElement ("P");
                e->setAttribute ("id", pid);
                e->setAttribute ("v", (double) rp->convertFrom0to1 (rp->getValue()));
            }
        }

    xml.writeTo (presetDir().getChildFile (juce::File::createLegalFileName (name) + ".dypelvpreset"));
    setPresetName (name);
}

void EQEditor::loadEntry (const PresetEntry& e)
{
    if (e.factory >= 0)
    {
        const auto& list = Presets::factory();
        if ((size_t) e.factory >= list.size()) return;
        resetAllBands();
        int b = 0;
        for (const auto& pb : list[(size_t) e.factory].bands)
        {
            if (b >= EQ::numBands) break;
            setParam (EQ::id (b, "type"), (float) pb.type);
            setParam (EQ::id (b, "ch"), (float) pb.channel);
            setParam (EQ::id (b, "freq"), pb.freq);
            setParam (EQ::id (b, "gain"), pb.gain);
            setParam (EQ::id (b, "q"), pb.q);
            setParam (EQ::id (b, "slp"), pb.slope);
            setParam (EQ::id (b, "on"), 1.0f);
            ++b;
        }
        setPresetName (e.name);
    }
    else if (auto xml = juce::parseXML (e.file))
    {
        if (! xml->hasTagName ("DypelvPreset") && ! xml->hasTagName ("PrismPreset")) return;   // old tag = presets made before the rename
        resetAllBands();
        for (auto* p : xml->getChildWithTagNameIterator ("P"))
            setParam (p->getStringAttribute ("id"), (float) p->getDoubleAttribute ("v"));
        setPresetName (e.name);
    }
    repaint();
}

void EQEditor::stepPreset (int dir)
{
    const auto list = presetEntries();
    if (list.empty()) return;
    const int n = (int) list.size();
    const auto cur = currentPresetName();
    int idx = -1;
    for (int i = 0; i < n; ++i)
        if (list[(size_t) i].name == cur) { idx = i; break; }
    idx = idx < 0 ? (dir > 0 ? 0 : n - 1) : (idx + dir + n) % n;
    loadEntry (list[(size_t) idx]);
}

// ---------------------------------------------------------------- layout
void EQEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (50).reduced (16, 9);

    // right side, from the edge inwards
    bypassBtn.setBounds (top.removeFromRight (32));   top.removeFromRight (6);
    analyzerBtn.setBounds (top.removeFromRight (32)); top.removeFromRight (4);
    fullBtn.setBounds (top.removeFromRight (32));     top.removeFromRight (12);
    rangeBox.setBounds (top.removeFromRight (96));    top.removeFromRight (14);
    phaseBtn.setBounds (top.removeFromRight (112));   top.removeFromRight (14);
    deltaBtn.setBounds (top.removeFromRight (32));    top.removeFromRight (4);
    matchBtn.setBounds (top.removeFromRight (32));    top.removeFromRight (14);
    outField.setBounds (top.removeFromRight (100));    top.removeFromRight (14);
    faqBtn.setBounds (top.removeFromRight (26));

    // left side
    top.removeFromLeft (116);   // "DYPELV EQ" logo is painted here
    prevBtn.setBounds (top.removeFromLeft (24));      top.removeFromLeft (2);
    presetBtn.setBounds (top.removeFromLeft (150));   top.removeFromLeft (2);
    nextBtn.setBounds (top.removeFromLeft (24));      top.removeFromLeft (22);
    undoBtn.setBounds (top.removeFromLeft (28));      top.removeFromLeft (2);
    redoBtn.setBounds (top.removeFromLeft (28));      top.removeFromLeft (22);
    abA.setBounds (top.removeFromLeft (24));          top.removeFromLeft (4);
    abB.setBounds (top.removeFromLeft (24));

    statusRect = area.removeFromBottom (26).reduced (16, 0);
    // right gutter (64 px): dBFS scale + slim L / R (+ optional LUFS) meters. The EQ dB scale lives inside the plot.
    // 22 px on the left: a band at 20 Hz sits on the plot's left edge and its 19 px glow must fit inside the window
    plotRect = area.toFloat().withTrimmedLeft (22).withTrimmedRight (64).withTrimmedTop (18).withTrimmedBottom (22);

    hintRect = juce::Rectangle<float> (420.0f, 168.0f).withCentre (plotRect.getCentre());
    hintImg = {};
    rebuildCurves();   // keep the response curve in step with the new size straight away
    positionCard();
}

// ---------------------------------------------------------------- cached EQ response
// Everything the plot area can touch when it is redrawn: the plot, the dBFS gutter on its right, and 36 px all round for the parts
// of a node that are drawn outside the plot (glow 19 px, stereo badges 27 px, LFO arrows 25 px). A smaller rectangle leaves a
// sliver of the old glow behind, which showed up as a clipped or doubled node at the edges of the plot.
juce::Rectangle<int> EQEditor::plotDirtyRect() const
{
    return plotRect.withRight (plotRect.getRight() + 64.0f).expanded (36.0f).getSmallestIntegerContainer();
}

void EQEditor::vblankTick()
{
    // the dB range glides to the chosen one (a quarter of a second, in log steps because a scale is a ratio) instead of snapping
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float target = targetRange();
    bool gliding = false;
    if (rangeShown <= 0.0f)
        rangeShown = rangeFrom = rangeGoal = target;
    else
    {
        if (target != rangeGoal) { rangeGoal = target; rangeFrom = rangeShown; rangeT0 = now; }
        if (rangeShown != target)
        {
            const float x = (float) juce::jlimit (0.0, 1.0, (now - rangeT0) / 260.0);
            rangeShown = x >= 1.0f ? target : rangeFrom * std::pow (target / rangeFrom, x * x * (3.0f - 2.0f * x));
            gliding = true;
        }
    }
    // LFO / dynamics motion needs about 30 fps, not the display rate; the full rate stays for dragging and while the range glides
    const bool interacting = dragBand >= 0 || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
    if (! gliding && ! interacting && now - lastPlotRepaintMs < plotMinIntervalMs) return;
    if (rebuildCurves() || gliding)
    {
        repaint (plotDirtyRect());
        lastPlotRepaintMs = now;
    }
}

bool EQEditor::rebuildCurves()
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const int base = (int) plotRect.getWidth() + 1;
    if (base < 2) return false;

    bool changed = false;
    const bool sizeChanged = (base != curveBase || sr != curveSr);

    std::array<BandSig, EQ::numBands> ns;
    bool sigChanged = false;
    for (int b = 0; b < EQ::numBands; ++b)
    {
        const int type = (int) std::lround (get (b, "type"));
        auto& s = ns[(size_t) b];
        s.on = get (b, "on") > 0.5f && get (b, "mute") < 0.5f;
        s.type = type;
        s.slope = get (b, "slp");
        s.f = effFreq (b);
        s.g = effGain (b);
        s.q = effQ (b);
        s.split = EQ::hasGain (type) ? (int) std::lround (get (b, "split")) : 0;
        s.g2 = s.split > 0 ? effGain2 (b) : 0.0f;
        s.dyn = get (b, "dyn") > 0.5f && EQ::hasGain (type);
        s.tg = s.dyn ? get (b, "gain") : 0.0f;

        if (! (s == sigs[(size_t) b])) sigChanged = true;
    }
    if (! sizeChanged && ! sigChanged) return false;

    // sample positions: one per pixel column, plus extra ones packed around every band's centre. A narrow band (Q up to 72) is
    // thinner than a pixel at its tip, so without them the sampling steps over the bottom of the notch and the tip looks cut off
    // and flickers while the band is dragged.
    std::vector<float> xs;
    xs.reserve ((size_t) base + 20 * (size_t) EQ::numBands);
    for (int i = 0; i < base; i += curveStepPx) xs.push_back ((float) i / (float) (base - 1));
    xs.push_back (1.0f);
    static const float kStep[] = { 0.0f, 0.03f, 0.08f, 0.16f, 0.3f, 0.5f, 0.8f, 1.3f, 2.2f, 4.0f };
    for (int b = 0; b < EQ::numBands; ++b)
    {
        const auto& s = ns[(size_t) b];
        if (! s.on) continue;
        const float q = juce::jmax (0.1f, s.q);
        for (float k : kStep)
            for (int sgn = (k == 0.0f ? 1 : -1); sgn <= 1; sgn += 2)
            {
                const float f = s.f * std::exp ((float) sgn * k / q);
                if (f < 20.0f || f > 20000.0f) continue;
                xs.push_back (juce::jlimit (0.0f, 1.0f, (xForFreq (f) - plotRect.getX()) / plotRect.getWidth()));
            }
    }
    std::sort (xs.begin(), xs.end());
    xs.erase (std::unique (xs.begin(), xs.end(), [] (float a, float b) { return b - a < 2.0e-5f; }), xs.end());

    const bool gridChanged = sizeChanged || xs != curveX;
    if (gridChanged)
    {
        curveBase = base;
        curveSr = sr;
        curveX = std::move (xs);
        curveCols = (int) curveX.size();
        for (auto& v : bandCurve) v.clear();
        for (auto& v : bandCurveB) v.clear();
        for (auto& v : targetCurve) v.clear();
        changed = true;
    }
    const int cols = curveCols;
    const float pw = plotRect.getWidth(), px0 = plotRect.getX();

    for (int b = 0; b < EQ::numBands; ++b)
    {
        const auto& s = ns[(size_t) b];
        auto& old = sigs[(size_t) b];
        if (! gridChanged && s == old) continue;
        old = s;
        changed = true;

        auto& v = bandCurve[(size_t) b];
        auto& vB = bandCurveB[(size_t) b];
        auto& vT = targetCurve[(size_t) b];
        if (! s.on) { v.clear(); vB.clear(); vT.clear(); continue; }

        auto fill = [&] (std::vector<float>& dst, float gain)
        {
            dst.resize ((size_t) cols);
            const auto st = EQ::makeStages (s.type, sr, s.f, gain, s.q, s.slope);
            for (int i = 0; i < cols; ++i)
                dst[(size_t) i] = juce::Decibels::gainToDecibels ((float) EQ::magnitude (st, (double) freqForX (px0 + pw * curveX[(size_t) i]), sr));
        };
        fill (v, s.g);
        if (s.split > 0) fill (vB, s.g2); else vB.clear();
        if (s.dyn)       fill (vT, s.tg);  else vT.clear();   // where the band would sit at full effect
    }

    if (! changed) return false;
    curveTotal.assign ((size_t) cols, 0.0f);
    for (int b = 0; b < EQ::numBands; ++b)
    {
        const auto& v = bandCurve[(size_t) b];
        const auto& vB = bandCurveB[(size_t) b];
        if (! sigs[(size_t) b].on || v.size() != (size_t) cols) continue;
        const bool both = sigs[(size_t) b].split > 0 && vB.size() == (size_t) cols;
        for (size_t i = 0; i < v.size(); ++i)
            curveTotal[i] += both ? 0.5f * (v[i] + vB[i]) : v[i];   // split: the total shows the average of both sides
    }
    return true;
}

// ---------------------------------------------------------------- analyzer
// Takes the newest frame from one tap, FFTs it and updates that layer's smoothed bins.
void EQEditor::analyzeTap (EQ::AnalyzerTap& tap, SpecLayer& L, int res, bool& rebuild)
{
    const int N = 4096 << res;
    if (L.smoothed.size() != (size_t) N / 2) return;   // layer not (re)initialised yet

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float fall = anaDecay[juce::jlimit (0, 2, geti ("ana_decay"))];
    const float avgTau = anaAvg[juce::jlimit (0, 3, geti ("ana_avg"))];
    bool changed = false;

    if (tap.ready.exchange (false) && tap.fetch (fftData.data(), N))
    {
        std::fill (fftData.begin() + N, fftData.begin() + 2 * N, 0.0f);
        windows[(size_t) res]->multiplyWithWindowingTable (fftData.data(), (size_t) N);
        ffts[(size_t) res]->performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 0.18f * (float) N;   // Blackman-Harris coherent gain ~0.36
        const float tilt = anaTilt[juce::jlimit (0, 2, geti ("ana_tilt"))];

        // averaging: a running mean of the POWER per bin with a time constant of seconds (one frame arrives every fftHop samples)
        const bool avg = avgTau > 0.0f;
        const bool seed = avg && L.avgPow.size() != L.smoothed.size();
        if (seed) L.avgPow.assign (L.smoothed.size(), 0.0f);
        else if (! avg && ! L.avgPow.empty()) L.avgPow.clear();
        const float aCoef = avg ? 1.0f - std::exp (-(float) EQ::fftHop / ((float) sr * avgTau)) : 1.0f;

        for (size_t i = 1; i < L.smoothed.size(); ++i)
        {
            float db;
            if (avg)
            {
                const float amp = fftData[i] / norm, pw = amp * amp;
                float& m = L.avgPow[i];
                m = seed ? pw : m + (pw - m) * aCoef;
                db = juce::jmax (-120.0f, 10.0f * std::log10 (m + 1.0e-12f));
            }
            else
                db = juce::Decibels::gainToDecibels (fftData[i] / norm, -120.0f);
            if (db > -119.5f)     // slope compensation must never lift the silence floor (it drew a ramp when nothing played)
                db = juce::jmax (-120.0f, db + tilt * std::log2 ((float) i * (float) sr / (float) N / 1000.0f));
            if (! std::isfinite (db)) db = -120.0f;
            L.smoothed[i] = avg ? db : (db > L.smoothed[i] ? db : juce::jmax (db, L.smoothed[i] - fall));
        }
        L.idle = 0;
        changed = true;
    }
    else if (++L.idle > kIdleFrames)
    {
        // no audio: let the display fall away instead of freezing on the last frame
        L.idle = kIdleFrames + 1;
        L.avgPow.clear();                      // averaging starts afresh when the sound returns
        for (auto& v : L.smoothed)
            if (v > -119.9f) { v = juce::jmax (-120.0f, v - 3.0f); changed = true; }
    }

    if (changed)
    {
        L.prefix.assign (L.smoothed.size() + 1, 0.0);
        for (size_t i = 0; i < L.smoothed.size(); ++i)
            L.prefix[i + 1] = L.prefix[i] + L.smoothed[i];
        rebuild = true;
    }
}

void EQEditor::updateSpectrumTarget (SpecLayer& L)
{
    const int cols = specCols;
    if (cols < 2 || lastRes < 0 || L.prefix.size() != L.smoothed.size() + 1) return;

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const int N = 4096 << lastRes, half = N / 2;
    const float smoothOct = anaSmooth[juce::jlimit (0, 3, geti ("ana_smooth"))];
    const float binsPerHz = (float) ((double) N / sr);

    // integral of the (piecewise-constant) bin values; bin i covers u in [i, i+1)
    auto P = [&] (float u) -> double
    {
        u = juce::jlimit (0.0f, (float) half, u);
        const int i = juce::jmin ((int) u, half - 1);
        return L.prefix[(size_t) i] + (double) (u - (float) i) * (double) L.smoothed[(size_t) i];
    };

    L.target.resize ((size_t) cols);
    for (int c = 0; c < cols; ++c)
    {
        const float colW = plotRect.getWidth() / (float) (cols - 1);
        const float xc = (float) c * colW;
        float fl = freqForX (plotRect.getX() + xc - 0.5f * colW);
        float fh = freqForX (plotRect.getX() + xc + 0.5f * colW);
        if (smoothOct > 0.0f)
        {
            const float fc = freqForX (plotRect.getX() + xc), k = std::pow (2.0f, smoothOct * 0.5f);
            fl = fc / k; fh = fc * k;
        }

        float bl = fl * binsPerHz, bh = fh * binsPerHz;
        float db;
        if (smoothOct <= 0.0f && bh - bl >= 1.0f)
        {
            // "Off": keep peaks (max of the bins under this column)
            const int lo = juce::jlimit (1, half - 1, (int) std::floor (bl));
            const int hi = juce::jlimit (lo + 1, half, (int) std::ceil (bh));
            db = -120.0f;
            for (int i = lo; i < hi; ++i) db = juce::jmax (db, L.smoothed[(size_t) i]);
        }
        else
        {
            // At least one bin wide: a box average over fractional edges = continuous linear
            // interpolation at low frequencies (no stair-stepping), true smoothing at high ones.
            if (bh - bl < 1.0f) { const float mid = 0.5f * (bl + bh); bl = mid - 0.5f; bh = mid + 0.5f; }
            bl = juce::jmax (bl, 0.5f);
            bh = juce::jmin (bh, (float) half - 0.5f);
            if (bh - bl < 0.5f) bl = bh - 1.0f;
            db = (float) ((P (bh + 0.5f) - P (bl + 0.5f)) / (double) (bh - bl));
        }
        L.target[(size_t) c] = db;
    }

    // light 1-2-1 pass across columns
    const auto tmp = L.target;
    for (int c = 1; c < cols - 1; ++c)
        L.target[(size_t) c] = 0.25f * tmp[(size_t) c - 1] + 0.5f * tmp[(size_t) c] + 0.25f * tmp[(size_t) c + 1];
}

void EQEditor::timerCallback()
{
    // a host that sets its own scale (Windows) replaces ours: take its value as the base and put ours back on top
    if (const float cur = getTransform().getScaleFactor(); std::abs (cur - appliedScale) > 1.0e-3f && cur > 0.1f)
    {
        hostScale = cur;
        applyUiScale (uiScalePct);
    }

    const int ri = geti ("range");
    bool fullRepaint = false;
    if (ri != lastRange) { lastRange = ri; fullRepaint = true; }

    // is the audio thread running?
    const auto bc = proc.blockCount.load (std::memory_order_relaxed);
    idleTicks = bc != lastBlockCount ? 0 : juce::jmin (idleTicks + 1, 1000);
    lastBlockCount = bc;
    audioActive = idleTicks < 20;
    trackHistory();

    if (selected >= 0 && get (selected, "on") < 0.5f) selectBand (-1);
    card.setLive (audioActive);
    card.tick();

    const auto pn = currentPresetName();
    presetBtn.setButtonText (pn.isEmpty() ? juce::String ("Presets") : pn);

    {
        const int pm = juce::jlimit (0, 3, geti ("phase"));
        phaseBtn.setButtonText (EQ::phaseNames()[pm]);
        phaseBtn.setToggleState (pm > 0, juce::dontSendNotification);
        syncFullIcon();      // the OS can leave full screen on its own (Esc, the green button)
        const bool armed = proc.learnParam.load() >= 0;
        if (armed != lastArmed) { lastArmed = armed; repaint (statusRect); }
        const int lt = proc.learnTick.load();
        if (lt != lastLearnTick)
        {
            lastLearnTick = lt;
            flashMsg = "MIDI assigned: CC " + juce::String (proc.lastLearnedCC.load());
            flashTicks = 150;
            repaint (statusRect);
        }
        if (flashTicks > 0 && --flashTicks == 0) repaint (statusRect);
    }

    // ---- meters
    for (int c = 0; c < 2; ++c)
    {
        const float lin = (c == 0 ? proc.loudness.peakL : proc.loudness.peakR).exchange (0.0f);
        const float db = juce::Decibels::gainToDecibels (lin, -120.0f);
        peakDb[c] = juce::jmax (db, peakDb[c] - 1.0f);
        if (db >= peakHold[c]) { peakHold[c] = db; holdTimer[c] = 90; }
        else if (--holdTimer[c] < 0) peakHold[c] = juce::jmax (-120.0f, peakHold[c] - 0.6f);
    }
    if (audioActive)
    {
        lufsM += (proc.loudness.momentary.load() - lufsM) * 0.35f;
        lufsS = proc.loudness.shortTerm.load();
        lufsI = proc.loudness.integrated.load();
    }
    else
        lufsM = juce::jmax (-120.0f, lufsM - 1.5f);

    // ---- spectrum layers
    if (geti ("ana_on"))
    {
        bool rbIn = false, rbOut = false;
        const int res = geti ("ana_res");
        const int N = 4096 << res;
        if (res != lastRes)
        {
            lastRes = res;
            for (auto* L : { &layIn, &layOut })
            {
                L->smoothed.assign ((size_t) N / 2, -120.0f);
                L->prefix.assign (L->smoothed.size() + 1, 0.0);
            }
            rbIn = rbOut = true;
        }

        if (geti ("ana_in")) analyzeTap (proc.tapIn, layIn, res, rbIn);
        analyzeTap (proc.tapOut, layOut, res, rbOut);

        const int sm = geti ("ana_smooth");
        if (sm != lastSmooth) { lastSmooth = sm; rbIn = rbOut = true; }

        const int cols = (int) (plotRect.getWidth() / specStep) + 1;
        if (cols != specCols)
        {
            // window resized: resample what is on screen instead of resetting it, so the scope doesn't blank or glitch
            auto resample = [] (std::vector<float>& v, int newN)
            {
                if (v.size() < 2) { v.assign ((size_t) newN, -120.0f); return; }
                std::vector<float> o ((size_t) newN);
                for (int i = 0; i < newN; ++i)
                {
                    const float pos = (float) i * (float) (v.size() - 1) / (float) (newN - 1);
                    const size_t i0 = (size_t) pos, i1 = juce::jmin (i0 + 1, v.size() - 1);
                    const float fr = pos - (float) i0;
                    o[(size_t) i] = v[i0] + (v[i1] - v[i0]) * fr;
                }
                v.swap (o);
            };
            specCols = cols;
            for (auto* L : { &layIn, &layOut })
            {
                resample (L->disp, cols);
                resample (L->target, cols);
            }
            rbIn = rbOut = true;
        }

        if (rbIn)  updateSpectrumTarget (layIn);
        if (rbOut) updateSpectrumTarget (layOut);

        // ease the drawn spectra toward the newest FFT frame at the full display rate
        for (auto* L : { &layIn, &layOut })
        {
            const size_t n = juce::jmin (L->disp.size(), L->target.size());
            for (size_t i = 0; i < n; ++i)
            {
                const float t = L->target[i];
                float& d = L->disp[i];
                d += (t - d) * (t > d ? 0.55f : 0.30f);
            }
        }
    }

    // the plot repaints at most ~30 times a second while nothing is being dragged (one budget shared with the vblank callback):
    // the spectrum looks the same and the load is about halved
    const double now = juce::Time::getMillisecondCounterHiRes();
    const bool interacting = dragBand >= 0 || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
    if (fullRepaint || interacting || now - lastPlotRepaintMs >= plotMinIntervalMs)
    {
        rebuildCurves();
        lastPlotRepaintMs = now;
        if (fullRepaint) repaint();
        else repaint (plotDirtyRect());
    }
}

// ---------------------------------------------------------------- painting
void EQEditor::drawLayer (juce::Graphics& g, const SpecLayer& L, juce::Colour col, float fillA, float lineA, float lineW) const
{
    using namespace juce;
    if (specCols < 2 || (int) L.disp.size() != specCols) return;
    const auto r = plotRect;

    bool any = false;                      // nothing playing: draw nothing instead of a line along the bottom edge
    for (float v : L.disp) if (v > -118.0f) { any = true; break; }
    if (! any) return;

    auto P = [&] (int i) { return Point<float> (r.getX() + r.getWidth() * (float) i / (float) (specCols - 1), yForLevel (L.disp[(size_t) i])); };
    Path line;
    line.startNewSubPath (P (0));
    for (int i = 0; i < specCols - 1; ++i)
    {
        const auto a = P (i), b = P (i + 1);
        const auto mid = (a + b) * 0.5f;
        if (i == 0) line.lineTo (mid);
        else        line.quadraticTo (a, mid);
    }
    line.lineTo (P (specCols - 1));

    Path fill (line);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (fillA), 0, r.getY(), col.withAlpha (fillA * 0.08f), 0, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col.withAlpha (lineA));
    g.strokePath (line, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
}

void EQEditor::paint (juce::Graphics& g)
{
    using namespace juce;

    const auto r = plotRect;
    const float R = range(), aR = anaRange();
    const bool grid = geti ("grid") != 0;
    const float lowLim = R * (1.0f - kZeroFrac) / kZeroFrac;     // the 0 dB line sits above the middle: more room below, for cuts
    const float Rt = targetRange();
    const float eqStep = Rt <= 3 ? 1.0f : Rt <= 6 ? 2.0f : Rt <= 12 ? 3.0f : Rt <= 18 ? 6.0f : Rt <= 22 ? 5.0f : 10.0f;

    // ---- background, logo, plot panel, grid and the frequency labels: static, so they are drawn once into an image
    //      (JUCE 8 shapes text slowly, and this part used to be redrawn 60 times a second)
    drawCached (g, layerBg, { 0, 0, 0, r.getX(), r.getY(), r.getWidth(), r.getHeight(), R, Rt, grid ? 1.0f : 0.0f, 0, 0 }, [&] (Graphics& sg)
    {
        sg.setGradientFill (ColourGradient (Colour (0xff181818), 0, 0, kBg, 0, (float) getHeight(), false));
        sg.fillAll();

        sg.setColour (kText);
        sg.setFont (19.0f);
        const int logoW = (int) std::ceil (textWidth (sg.getCurrentFont(), "DYPELV"));
        sg.drawText ("DYPELV", 18, 10, logoW + 4, 30, Justification::centredLeft);
        sg.setColour (kAccent);
        sg.drawText ("EQ", 18 + logoW + 6, 10, 40, 30, Justification::centredLeft);

        sg.setColour (kPanel);
        sg.fillRoundedRectangle (r, 8.0f);
        sg.setColour (Colours::white.withAlpha (0.10f));
        sg.drawRoundedRectangle (r, 8.0f, 1.0f);

        if (grid)
        {
            const float freqs[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900,
                                    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
            sg.setFont (10.5f);
            for (float f : freqs)
            {
                const bool major = f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000
                                   || f == 2000 || f == 5000 || f == 10000 || f == 20000;
                const float x = xForFreq (f);
                sg.setColour (Colours::white.withAlpha (major ? 0.17f : 0.075f));
                sg.drawVerticalLine ((int) x, r.getY() + 1, r.getBottom() - 1);
                if (major)
                {
                    sg.setColour (kText.withAlpha (0.75f));
                    sg.drawText (f >= 1000 ? String (f / 1000.0f, 0) + "k" : String ((int) f),
                                 (int) x - 20, (int) r.getBottom() + 4, 40, 14, Justification::centred);
                }
            }
            for (float v = 0; v <= lowLim + 0.01f; v += eqStep)
                for (int sgn : { 1, -1 })
                {
                    if ((v == 0 && sgn < 0) || (sgn > 0 && v > R + 0.01f)) continue;
                    sg.setColour (Colours::white.withAlpha (v == 0 ? 0.28f : 0.11f));
                    sg.drawHorizontalLine ((int) yForGain (v * (float) sgn), r.getX() + 1, r.getRight() - 1);
                }
        }
    });

    g.saveState();
    g.reduceClipRegion (r.toNearestInt());

    // ---- spectrum: original (input) underneath, changed (after bands) on top
    if (geti ("ana_on") != 0 && lastRes >= 0)
    {
        if (geti ("ana_in") != 0)
            drawLayer (g, layIn, Colour (0xff9a9a9a), 0.20f, 0.55f, 1.1f);
        drawLayer (g, layOut, kAccent, 0.30f, 0.85f, 1.5f);
    }

    const bool curvesOk = curveCols > 1 && curveTotal.size() == (size_t) curveCols && curveX.size() == (size_t) curveCols;
    auto cx = [&] (int i) { return r.getX() + r.getWidth() * curveX[(size_t) i]; };
    auto dashed = [&] (const std::vector<float>& v, Colour col, float a, float on, float off)
    {
        Path pth;
        for (int i = 0; i < curveCols; ++i)
        {
            if (i == 0) pth.startNewSubPath (cx (i), yForGain (v[(size_t) i]));
            else        pth.lineTo (cx (i), yForGain (v[(size_t) i]));
        }
        Path d;
        const float dl[] = { on, off };
        PathStrokeType (1.3f).createDashedStroke (d, pth, dl, 2);
        g.setColour (col.withAlpha (a));
        g.fillPath (d);
    };

    // ---- band curves: a thin line and a faint shade in the band's colour, always visible; the selected band is stronger
    if (curvesOk)
    {
        auto drawBand = [&] (int b)
        {
            const auto& v = bandCurve[(size_t) b];
            if (! sigs[(size_t) b].on || v.size() != (size_t) curveCols) return;
            const bool sel = (b == selected);
            const int spl = sigs[(size_t) b].split;
            // a pair draws its two sides in the channel colours (Left / Right or Mid / Side), anything else in the band's colour
            const auto col = spl > 0 ? channelColour (spl == 1 ? EQ::Left : EQ::Mid) : bandColour (b);
            const auto colB = spl > 0 ? channelColour (spl == 1 ? EQ::Right : EQ::Side) : col;

            Path line;
            for (int i = 0; i < curveCols; ++i)
            {
                if (i == 0) line.startNewSubPath (cx (i), yForGain (v[(size_t) i]));
                else        line.lineTo (cx (i), yForGain (v[(size_t) i]));
            }

            // the shade: boost / cut types fill between the curve and the 0 dB line; Low Pass, High Pass and Band Pass shade what passes,
            // i.e. everything under the curve down to the bottom of the plot (as in TBTECH). Every band has one, the selected one is stronger.
            {
                const int tp = sigs[(size_t) b].type;
                const bool passType = tp == EQ::LowPass || tp == EQ::HighPass || tp == EQ::BandPass;
                const float yEnd = passType ? r.getBottom() : zeroY();
                Path bp (line);
                bp.lineTo (cx (curveCols - 1), yEnd);
                bp.lineTo (cx (0), yEnd);
                bp.closeSubPath();
                g.setColour (col.withAlpha (sel ? 0.11f : 0.06f));
                g.fillPath (bp);
            }
            g.setColour (col.withAlpha (sel ? 0.95f : 0.55f));
            g.strokePath (line, PathStrokeType (sel ? 1.5f : 1.1f, PathStrokeType::curved, PathStrokeType::rounded));

            // dynamic band: dashed outline = where the band would sit at full effect; the solid line is the live response
            const auto& vT = targetCurve[(size_t) b];
            if (vT.size() == (size_t) curveCols) dashed (vT, col, sel ? 0.8f : 0.5f, 5.0f, 4.0f);
            // split band: dashed outline = the second side (Right / Side)
            const auto& vB = bandCurveB[(size_t) b];
            if (sigs[(size_t) b].split > 0 && vB.size() == (size_t) curveCols) dashed (vB, colB, sel ? 0.95f : 0.6f, 2.0f, 3.0f);
        };
        for (int b = 0; b < EQ::numBands; ++b)
            if (b != selected) drawBand (b);
        if (selected >= 0 && selected < EQ::numBands) drawBand (selected);
    }

    // ---- solo: the band's Q is the solo range. The band's range stays exactly as it is, everything outside is darkened
    //      (flat, only a very short fade at the edges, no lines); drawn over the spectrum and band shades, under the EQ curve
    const int sb = soloedBand();
    if (sb >= 0)
    {
        const float f0 = effFreq (sb), q = jmax (0.1f, effQ (sb));
        const float k = 1.0f / (2.0f * q), s = std::sqrt (1.0f + k * k);
        const float x1 = jlimit (r.getX(), r.getRight(), xForFreq (jmax (20.0f, f0 * (s - k))));
        const float x2 = jlimit (r.getX(), r.getRight(), xForFreq (jmin (20000.0f, f0 * (s + k))));
        const auto dim = Colours::black.withAlpha (0.5f);   // raise / lower this to dim the outside more / less
        const float fade = 14.0f;                            // width of the soft edge, inside the dimmed side; 0 = hard edge
        const float lw = jmax (0.0f, x1 - r.getX()), rw = jmax (0.0f, r.getRight() - x2);
        g.setColour (dim);
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), jmax (0.0f, lw - fade), r.getHeight()));
        g.fillRect (Rectangle<float> (x2 + jmin (fade, rw), r.getY(), jmax (0.0f, rw - fade), r.getHeight()));
        if (lw > 0.0f)
        {
            const float fw = jmin (fade, lw);
            g.setGradientFill (ColourGradient (dim, x1 - fw, 0.0f, dim.withAlpha (0.0f), x1, 0.0f, false));
            g.fillRect (Rectangle<float> (x1 - fw, r.getY(), fw, r.getHeight()));
        }
        if (rw > 0.0f)
        {
            const float fw = jmin (fade, rw);
            g.setGradientFill (ColourGradient (dim.withAlpha (0.0f), x2, 0.0f, dim, x2 + fw, 0.0f, false));
            g.fillRect (Rectangle<float> (x2, r.getY(), fw, r.getHeight()));
        }
    }

    // ---- EQ response (from cache)
    if (curvesOk)
    {
        Path curve;
        for (int i = 0; i < curveCols; ++i)
        {
            const float x = cx (i), y = yForGain (curveTotal[(size_t) i]);
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }
        g.setColour (Colours::white.withAlpha (0.10f));
        g.strokePath (curve, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xfff6f6f6));
        g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    else
    {
        g.setColour (Colour (0xfff6f6f6));
        g.drawHorizontalLine ((int) zeroY(), r.getX(), r.getRight());
    }

    g.restoreState();

    // ---- scale labels, static: the EQ dB scale inside the plot (right edge, dim) and the dBFS scale in the gutter
    drawCached (g, layerScale, { 0, 0, 0, r.getX(), r.getY(), r.getWidth(), r.getHeight(), R, Rt, aR, 0, 0 }, [&] (Graphics& sg)
    {
        sg.setFont (10.0f);
        for (float v = 0; v <= lowLim + 0.01f; v += eqStep)
            for (int sgn : { 1, -1 })
            {
                if ((v == 0 && sgn < 0) || (sgn > 0 && v > R + 0.01f)) continue;
                const float val = v * (float) sgn;
                sg.setColour (kText.withAlpha (0.72f));
                sg.drawText ((val > 0 ? "+" : "") + String ((int) std::lround (val)),
                             (int) r.getRight() - 40, (int) yForGain (val) - 7, 34, 14, Justification::centredRight);
            }

        const float gx = r.getRight();
        sg.setFont (9.5f);
        sg.setColour (kAccent.withAlpha (0.8f));
        sg.drawText ("dBFS", (int) gx + 4, (int) r.getY() - 15, 28, 11, Justification::centredLeft);
        for (float db = 0; db >= -aR + 0.1f; db -= 10.0f)
        {
            sg.setFont (10.0f);
            sg.setColour (kAccent.withAlpha (0.85f));
            sg.drawText (String ((int) db), (int) gx + 4, (int) yForLevel (db) - 7, 28, 14, Justification::centredLeft);
        }
    });

    // ---- right gutter: slim L / R peak bars (+ optional LUFS-M bar) with clip lights
    {
        const float gx = r.getRight();
        const bool lufsOn = geti ("show_lufs") != 0;

        auto bar = [&] (float x, float db, float hold, bool isLufs)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (Rectangle<float> (x, r.getY(), 7.0f, r.getHeight()), 2.0f);
            if (db > -aR + 0.1f)
            {
                const float y = yForLevel (db);
                if (isLufs) g.setColour (kMeter.withAlpha (0.9f));
                else
                {
                    ColourGradient grad (Colour (0xffef4444), 0, yForLevel (0.0f), Colour (0xff34d399), 0, yForLevel (-24.0f), false);
                    grad.addColour (0.3, Colour (0xfffbbf24));
                    g.setGradientFill (grad);
                }
                g.fillRect (Rectangle<float> (x, y, 7.0f, r.getBottom() - y));
            }
            if (hold > -aR + 0.1f)
            {
                g.setColour (isLufs ? kAmber : kText);
                g.fillRect (Rectangle<float> (x, yForLevel (hold) - 1.0f, 7.0f, 2.0f));
            }
            if (! isLufs)    // clip light above the bar: lit when the peak touched 0 dBFS (click the gutter to clear)
            {
                const bool clip = hold > -0.3f;
                g.setColour (clip ? kRed : kText.withAlpha (0.12f));
                g.fillRoundedRectangle (Rectangle<float> (x, r.getY() - 6.0f, 7.0f, 4.0f), 1.5f);
            }
        };
        bar (gx + 34.0f, peakDb[0], peakHold[0], false);
        bar (gx + 43.0f, peakDb[1], peakHold[1], false);
        if (lufsOn) bar (gx + 52.0f, lufsM, lufsI, true);
    }

    // ---- nodes (selected band last so it is on top, matching hitBand)
    auto drawDot = [&] (int b, int side)       // side 1 = the second dot of a mirrored pair (on the dashed curve)
    {
        const bool muted = get (b, "mute") > 0.5f;
        const auto col = muted ? bandColour (b).withAlpha (0.35f) : bandColour (b);
        const auto p = side == 1 ? nodePos2 (b) : nodePos (b);
        const bool sel = b == selected, hov = b == hovered, soloed = get (b, "solo") > 0.5f;
        const float rad = sel ? 10.5f : 9.0f;
        const int type = (int) std::lround (get (b, "type"));
        const bool lfoX = get (b, "lxs") > 0.5f, lfoY = get (b, "lys") > 0.5f, lfo = lfoX || lfoY;
        const bool dynOn = get (b, "dyn") > 0.5f && EQ::hasGain (type);
        const int ch = (int) std::lround (get (b, "ch"));
        const int split = EQ::hasGain (type) ? (int) std::lround (get (b, "split")) : 0;
        // the channel this dot stands for: a pair has Left + Right (or Mid + Side), one dot each; otherwise the band's own channel
        const int dch = split > 0 ? (side == 0 ? (split == 1 ? (int) EQ::Left : (int) EQ::Mid) : (split == 1 ? (int) EQ::Right : (int) EQ::Side)) : ch;
        const auto dark = Colour (0xff121212);

        if (! muted && audioActive && animated (b))     // ghost dot: where the LFO / dynamics put the band right now
        {
            const Point<float> gp (xForFreq (effFreq (b)),
                                   jlimit (r.getY(), r.getBottom(), EQ::hasGain (type) ? yForGain (side == 1 ? effGain2 (b) : effGain (b)) : zeroY()));
            g.setColour (col.withAlpha (0.45f));
            g.drawLine (p.x, p.y, gp.x, gp.y, 1.0f);
            g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (gp));
            if (dynOn && side == 0 && std::abs (effGain (b) - get (b, "gain")) > 0.1f)   // live gain change from the compressor
            {
                g.setFont (10.5f);
                g.setColour (kText.withAlpha (0.85f));
                const float d = effGain (b) - get (b, "gain");
                g.drawText (String (d > 0 ? "+" : "") + String (d, 1), Rectangle<float> (gp.x + 6.0f, gp.y - 7.0f, 44.0f, 14.0f), Justification::centredLeft);
            }
        }

        g.setColour (col.withAlpha (sel || hov ? 0.28f : 0.12f));
        g.fillEllipse (Rectangle<float> (rad * 3.6f, rad * 3.6f).withCentre (p));

        const Rectangle<float> body = Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p);
        // the dot: bands with an LFO are rounded squares, with arrows for the axes that move. A one-channel band is filled with its
        // channel colour (a mirrored pair has two dots, one per side), like the badges in the channel menu; a stereo band is dark. The ring keeps the
        // band's colour. Solo: a stereo dot is filled amber, a channel dot gets a thicker amber ring.
        const bool chanDot = dch != EQ::Stereo;
        Path shape;
        if (lfo) shape.addRoundedRectangle (body, rad * 0.5f);
        else     shape.addEllipse (body);
        g.setColour (soloed && ! chanDot ? kAmber : dark);
        g.fillPath (shape);
        if (chanDot)
        {
            const float fa = muted ? 0.35f : 0.96f;
            g.saveState();
            g.reduceClipRegion (shape);
            g.setColour (channelColour (dch).withAlpha (fa));
            g.fillPath (shape);
            g.restoreState();
        }
        g.setColour (soloed ? kAmber : col);
        g.strokePath (shape, PathStrokeType ((sel ? 2.4f : 1.7f) + (soloed && chanDot ? 0.9f : 0.0f)));
        if (dynOn)   // dynamic bands get an outer ring
        {
            g.setColour (col.withAlpha (0.75f));
            g.drawEllipse (body.expanded (3.5f), 1.0f);
        }
        // what the dot says: the band number for a stereo band; for a one-channel band the channel letter in the channel colour
        // a soloed dot is amber with dark ink, a muted one is crossed out
        {
            const Colour ink (0xff131313);
            auto letter = [&] (int chan, float dx, float size)
            {
                g.setFont (UI::uiFont (size, true));
                g.setColour (muted ? kText.withAlpha (0.6f) : ink);      // dark letters on the channel colour, as in the menu
                g.drawText (channelLetter (chan), Rectangle<float> (10.0f, body.getHeight()).withCentre ({ body.getCentreX() + dx, body.getCentreY() }),
                            Justification::centred, false);
            };
            if (dch != EQ::Stereo) letter (dch, 0.0f, 10.5f);
            else
            {
                g.setColour (soloed ? ink : muted ? kText.withAlpha (0.4f) : kText);
                g.setFont (10.5f);
                g.drawText (String (b + 1), body, Justification::centred);
            }
            if (muted)
            {
                g.setColour (kText.withAlpha (0.85f));
                const auto mc = body.getCentre();
                const float h = rad * 0.68f;
                g.drawLine (mc.x - h, mc.y + h, mc.x + h, mc.y - h, 1.5f);
            }
        }

        auto arrow = [&] (float ax, float ay, float dx, float dy)
        {
            Path t;
            t.addTriangle (ax + dx * 4.0f, ay + dy * 4.0f,
                           ax - dx * 2.0f - dy * 3.5f, ay - dy * 2.0f + dx * 3.5f,
                           ax - dx * 2.0f + dy * 3.5f, ay - dy * 2.0f - dx * 3.5f);
            g.setColour (col.withAlpha (0.95f));
            g.fillPath (t);
        };
        if (lfoX) { arrow (p.x - rad - 8.0f, p.y, -1.0f, 0.0f); arrow (p.x + rad + 8.0f, p.y, 1.0f, 0.0f); }
        if (lfoY) { arrow (p.x, p.y - rad - 8.0f, 0.0f, -1.0f); arrow (p.x, p.y + rad + 8.0f, 0.0f, 1.0f); }


    };
    auto drawNode = [&] (int b)
    {
        if (EQ::hasGain ((int) std::lround (get (b, "type"))) && get (b, "split") > 0.5f)
            drawDot (b, 1);      // a mirrored pair: the second dot first, so the first one lies on top when both sit at 0 dB
        drawDot (b, 0);
    };
    for (int b = 0; b < EQ::numBands; ++b)
        if (b != selected && get (b, "on") > 0.5f)
            drawNode (b);
    if (selected >= 0 && get (selected, "on") > 0.5f)
        drawNode (selected);

    // ---- value bubble next to the dot while it is being dragged:  -5.0dB / 6.867kHz F#7
    if (dragBand >= 0 && get (dragBand, "on") > 0.5f)
    {
        const int type = (int) std::lround (get (dragBand, "type"));
        const float f = get (dragBand, "freq");
        const auto p = dragSide == 1 ? nodePos2 (dragBand) : nodePos (dragBand);

        String l1;
        const String l2 = fmtFreqShort (f) + " " + noteName (f);
        if (EQ::hasGain (type))
        {
            float gdb = (dragSide == 1 ? -1.0f : 1.0f) * get (dragBand, "gain");
            if (std::abs (gdb) < 0.05f) gdb = 0.0f;
            l1 = String (gdb > 0.0f ? "+" : "") + String (gdb, 1) + "dB";
        }

        g.setFont (14.0f);
        const Font f1 = g.getCurrentFont().boldened();
        g.setFont (13.0f);
        const Font f2 = g.getCurrentFont();

        const bool two = l1.isNotEmpty();
        const float w = jmax (two ? textWidth (f1, l1) : 0.0f, textWidth (two ? f2 : f1, l2)) + 22.0f;
        const float h = two ? 42.0f : 26.0f;

        float bx = p.x + 20.0f, by = p.y - h - 12.0f;
        if (bx + w > r.getRight() - 4.0f) bx = p.x - 20.0f - w;
        if (by < r.getY() + 4.0f) by = p.y + 16.0f;
        by = jlimit (r.getY() + 4.0f, jmax (r.getY() + 4.0f, r.getBottom() - h - 4.0f), by);
        bx = jmax (r.getX() + 4.0f, bx);

        const Rectangle<float> box (bx, by, w, h);
        g.setColour (Colour (0xf2151515));
        g.fillRoundedRectangle (box, 7.0f);
        g.setColour (bandColour (dragBand).withAlpha (0.9f));
        g.drawRoundedRectangle (box, 7.0f, 1.2f);

        auto inner = box.reduced (11.0f, 4.0f);
        if (two)
        {
            g.setFont (f1);
            g.setColour (Colours::white);
            g.drawText (l1, inner.removeFromTop (17.0f), Justification::centredLeft);
            g.setFont (f2);
            g.setColour (kText.withAlpha (0.8f));
            g.drawText (l2, inner, Justification::centredLeft);
        }
        else
        {
            g.setFont (f1);
            g.setColour (Colours::white);
            g.drawText (l2, inner, Justification::centredLeft);
        }
    }

    // ---- readout pill (hovered / selected band), top left
    const int rb = hovered >= 0 ? hovered : selected;
    if (rb >= 0 && get (rb, "on") > 0.5f)
    {
        const int type = (int) std::lround (get (rb, "type"));
        String t = String (rb + 1) + "   " + EQ::typeNames()[type] + "   " + EQ::fmtFreq (get (rb, "freq")) + " " + noteName (get (rb, "freq"));
        if (EQ::hasGain (type)) t << "   " << (get (rb, "gain") > 0 ? "+" : "") << String (get (rb, "gain"), 1) << " dB";
        if (EQ::hasSlope (type)) t << "   " << EQ::fmtSlope (get (rb, "slp"));
        else if (EQ::hasQ (type)) t << "   Q " << String (get (rb, "q"), 2);
        if (type == EQ::Peak && get (rb, "slp") > 12.05f) t << "   " << EQ::fmtSlope (get (rb, "slp"));     // a steepened bell: Q and Slope
        const int split = EQ::hasGain (type) ? (int) std::lround (get (rb, "split")) : 0;
        t << "   " << (split > 0 ? "Split " + EQ::splitNames()[split] : EQ::channelNames()[(int) std::lround (get (rb, "ch"))]);
        if (get (rb, "dyn") > 0.5f && EQ::hasGain (type)) t << "   Dyn";
        if (get (rb, "lxs") > 0.5f) t << "   LFO X " << EQ::lfoShapeNames()[(int) std::lround (get (rb, "lxs"))];
        if (get (rb, "lys") > 0.5f) t << "   LFO Y " << EQ::lfoShapeNames()[(int) std::lround (get (rb, "lys"))];
        if (get (rb, "mute") > 0.5f) t << "   (muted)";
        g.setFont (12.0f);
        // the width moves in steps of 8 px: a digit that is a hair wider no longer makes the right edge of the pill tremble while a value is dragged
        const Rectangle<float> pill (r.getX() + 12, r.getY() + 10, std::ceil ((textWidth (g.getCurrentFont(), t) + 44.0f) / 8.0f) * 8.0f, 26.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (pill, 13.0f);
        g.setColour (bandColour (rb));
        g.fillEllipse (Rectangle<float> (8, 8).withCentre ({ pill.getX() + 14, pill.getCentreY() }));
        g.setColour (kText);
        g.drawText (t, pill.withTrimmedLeft (26), Justification::centredLeft);
    }

    // ---- level readout, top right (click to reset peak / integrated loudness)
    {
        g.setFont (12.0f);
        String t = "Peak " + lvlStr (jmax (peakHold[0], peakHold[1])) + " dB";
        if (geti ("show_lufs") != 0)
            t << "      LUFS   M " << lvlStr (lufsM) << "   S " << lvlStr (lufsS) << "   I " << lvlStr (lufsI);
        if (geti ("autogain") != 0)
            t << "      Match " << String (proc.autoGainDb.load(), 1) << " dB";
        const float w = textWidth (g.getCurrentFont(), t) + 24.0f;
        lufsPill = Rectangle<float> (r.getRight() - 12.0f - w, r.getY() + 10.0f, w, 26.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (lufsPill, 13.0f);
        g.setColour (kText.withAlpha (0.9f));
        g.drawText (t, lufsPill, Justification::centred);
    }

    // ---- status strip: only messages (MIDI learn, notices); the hints live behind the ? button
    const bool armed = proc.learnParam.load() >= 0;
    if (armed || flashTicks > 0)
    {
        g.setFont (11.0f);
        g.setColour (kAmber);
        keyboardIcon (kAmber)->drawWithin (g, Rectangle<float> (statusRect.getX(), statusRect.getCentreY() - 8.0f, 20.0f, 16.0f),
                                           RectanglePlacement::centred, 1.0f);
        g.drawText (armed ? juce::String ("MIDI learn: move a controller to assign it   |   Esc: cancel") : flashMsg,
                    statusRect.withTrimmedLeft (26), Justification::centredLeft);
    }
}

void EQEditor::showFaq()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Quick guide");
    for (auto t : { "Drag a node: frequency and gain",
                    "Double-click the graph: add a band",
                    "Double-click a band: open / close its editor",
                    "Wheel on a band: Q  (the solo range while soloed)",
                    "Click Slope in the editor: pick a step",
                    "Double-click a value: type it,  Alt-click: reset",
                    "Tab: next band,  Esc: deselect",
                    "OUT (top): output gain,  drag or double-click to type",
                    "Band menu: Linked pair (mirrored gain) or Two independent bands",
                    "Band dot: its number, or L / R / M / S for one channel;  amber = solo,  crossed = muted",
                    "S: solo,  M: mute  (selected band)",
                    "Display menu (spectrum icon): analyzer, averaging, interface scale, band colours, film grain",
                    "Right-click: add a band by type, or the band menu" })
        m.addItem (t, false, false, nullptr);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&faqBtn));
}

// ---------------------------------------------------------------- mouse / keys
void EQEditor::mouseMove (const juce::MouseEvent& e) { hovered = hitBand (e.position); }

void EQEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    endDrag();            // never leave a gesture open from an earlier click

    if (hintVisible && hintRect.contains (e.position)) { dismissHint(); return; }

    int hitSide = 0;
    const int hit = hitBand (e.position, &hitSide);       // dots win over everything else, also those sitting on the plot edge

    if (hit < 0 && lufsPill.contains (e.position))
    {
        proc.loudness.resetRequest.store (true);
        lufsI = -120.0f;
        return;
    }

    if (hit < 0 && e.position.x > plotRect.getRight() && plotRect.getY() - 16.0f < e.position.y && e.position.y < plotRect.getBottom())
    {
        peakHold[0] = peakHold[1] = -120.0f;     // click in the meter gutter clears the clip lights
        return;
    }

    if (e.mods.isPopupMenu())     // right-click: everything is edited here
    {
        if (hit != selected) selectBand (hit);
        if (hit >= 0) showBandMenu (hit);
        else if (plotRect.contains (e.position)) showAddMenu (e.position);
        return;
    }

    if (e.getNumberOfClicks() <= 1)       // remembered for the double-click that may follow: it toggles the card
        cardOpenBeforeClick = hit >= 0 && hit == selected && cardOpen;

    if (hit != selected)
        selectBand (hit);
    if (hit < 0) return;
    beginDrag (hit, e.position, hitSide);
}

void EQEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const int hit = hitBand (e.position);
    if (hit >= 0)      // double-click a band: open its card, or close it when it was already open
    {
        endDrag();
        if (cardOpenBeforeClick)
        {
            cardOpenBeforeClick = false;
            selectBand (-1);
            return;
        }
        cardOpen = true;
        if (hit != selected) selectBand (hit);
        positionCard();
        return;
    }
    if (! plotRect.contains (e.position)) return;
    if (lufsPill.contains (e.position)) return;
    endDrag();
    addBand (e.position, EQ::Peak);      // double-click on empty graph = new Peak band
}

void EQEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    if (! dragMoved)
    {
        if (e.position.getDistanceFrom (dragStart) < 4.0f) return;     // still a click
        dragMoved = true;
        dragOffset = (dragSide == 1 ? nodePos2 (dragBand) : nodePos (dragBand)) - e.position;
        // A node whose gain is outside the drawn range (the range was made smaller) sits on the plot's edge. Grabbing it must not snap
        // the gain to the edge: the gain stays where it is, and the difference fades out as the pointer travels up or down.
        dragGain0 = (dragSide == 1 ? -1.0f : 1.0f) * get (dragBand, "gain");      // the dot's own gain (the mirror dot: minus the band's)
        dragGainOff = dragGain0 - gainForY (e.position.y + dragOffset.y);
        dragY0 = e.position.y;
        if (card.isVisible()) { cardHiddenByDrag = true; card.setVisible (false); }   // the card gets out of the way while dragging
    }
    const auto pt = e.position + dragOffset;
    setParam (EQ::id (dragBand, "freq"), freqForX (pt.x));
    if (EQ::hasGain ((int) std::lround (get (dragBand, "type"))))
    {
        const float fade = std::exp (-std::abs (e.position.y - dragY0) / 120.0f);
        const float g = gainForY (pt.y) + dragGainOff * fade;
        setParam (EQ::id (dragBand, "gain"), (dragSide == 1 ? -1.0f : 1.0f) * juce::jlimit (juce::jmin (-lowRange(), dragGain0), juce::jmax (range(), dragGain0), g));
    }
}

void EQEditor::mouseUp (const juce::MouseEvent&) { endDrag(); }

void EQEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = soloedBand();      // while a band is soloed, the wheel acts on that band (Q = the solo range)
    if (b < 0) b = hitBand (e.position) >= 0 ? hitBand (e.position) : selected;
    if (b < 0) return;
    if (b != selected) selectBand (b);

    // the wheel is always Q (the slope of Low / High Pass is picked from the list in the band editor)
    // gentle: a swipe with momentum used to run Q from 18 to 72 in under a second; now one event moves it by 2.5% at most
    setParamGesture (EQ::id (b, "q"), juce::jlimit (0.1f, 72.0f, get (b, "q") * std::exp (juce::jlimit (-0.025f, 0.025f, w.deltaY * 0.25f))));
}

void EQEditor::drawCached (juce::Graphics& g, LayerCache& lc, std::array<float, 12> key, const std::function<void (juce::Graphics&)>& draw)
{
    const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw = juce::jmax (1, juce::roundToInt ((float) getWidth() * sc)), ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * sc));
    key[0] = (float) getWidth(); key[1] = (float) getHeight(); key[2] = sc;
    if (lc.img.isNull() || lc.img.getWidth() != pw || lc.img.getHeight() != ph || lc.key != key)
    {
        lc.key = key;
        lc.img = juce::Image (juce::Image::ARGB, pw, ph, true, juce::NativeImageType());      // native: the text looks exactly as when drawn directly
        juce::Graphics lg (lc.img);
        lg.addTransform (juce::AffineTransform::scale (sc));
        draw (lg);
    }
    g.drawImageTransformed (lc.img, juce::AffineTransform::scale (1.0f / sc));
}

void EQEditor::paintOverChildren (juce::Graphics& g)
{
    drawHint (g);          // first run only
    Grain::paint (g);      // film grain over everything: the background, the plot, the band card
}

bool EQEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int kc = k.getKeyCode();
    inputTicks = 0;      // keyboard edits count as user edits for undo
    if (mods.isCommandDown() && (kc == 'z' || kc == 'Z'))
    {
        if (mods.isShiftDown()) redo(); else undo();
        return true;
    }
    if (mods.isCommandDown() && (kc == 'y' || kc == 'Y'))
    {
        redo();
        return true;
    }
    if (kc == juce::KeyPress::tabKey)
    {
        stepBand (mods.isShiftDown() ? -1 : 1);
        return true;
    }
    if (kc == juce::KeyPress::escapeKey && proc.learnParam.load() >= 0)
    {
        proc.cancelMidiLearn();
        repaint (statusRect);
        return true;
    }
    if (kc == juce::KeyPress::escapeKey && selected >= 0)
    {
        selectBand (-1);
        return true;
    }
    if (selected >= 0 && (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey))
    {
        deleteBand (selected);
        return true;
    }
    if (selected >= 0 && ! mods.isCommandDown() && ! mods.isCtrlDown() && ! mods.isAltDown())   // S = solo, M = mute (selected band)
    {
        if (kc == 's' || kc == 'S') { toggleSolo (selected); return true; }
        if (kc == 'm' || kc == 'M')
        {
            setParamGesture (EQ::id (selected, "mute"), get (selected, "mute") > 0.5f ? 0.0f : 1.0f);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- history: undo / redo / A-B
std::vector<float> EQEditor::snapshot() const
{
    std::vector<float> v;
    v.reserve (tracked.size());
    for (auto* p : tracked) v.push_back (p->getValue());
    return v;
}

void EQEditor::applySnapshot (const std::vector<float>& s)
{
    endDrag();
    for (size_t i = 0; i < tracked.size() && i < s.size(); ++i)
        tracked[i]->setValueNotifyingHost (s[i]);
    lastSeen = lastStable = snapshot();     // applying a snapshot is not a new edit
    settleTicks = 0;
    userChange = false;
}

// store the state from before the current burst of edits as one undo step
void EQEditor::commitHistory()
{
    const auto cur = snapshot();
    if (cur != lastSeen)
    {
        lastSeen = cur;
        if (inputTicks < 900) userChange = true;
    }
    if (userChange && cur != lastStable)
    {
        proc.undoStack.push_back (lastStable);
        if (proc.undoStack.size() > 100) proc.undoStack.erase (proc.undoStack.begin());
        proc.redoStack.clear();
    }
    lastStable = cur;
    userChange = false;
    settleTicks = 0;
}

void EQEditor::trackHistory()
{
    if (inputTicks < 100000) ++inputTicks;
    const auto cur = snapshot();
    if (cur != lastSeen)       // something changed; an edit counts as the user's only if the mouse was used lately
    {
        lastSeen = cur;
        settleTicks = 18;
        if (inputTicks < 900) userChange = true;
    }
    else if (settleTicks > 0 && --settleTicks == 0)
        commitHistory();

    undoBtn.setEnabled (! proc.undoStack.empty());
    redoBtn.setEnabled (! proc.redoStack.empty());
    abA.setToggleState (proc.abActive == 0, juce::dontSendNotification);
    abB.setToggleState (proc.abActive == 1, juce::dontSendNotification);
}

void EQEditor::undo()
{
    commitHistory();
    if (proc.undoStack.empty()) return;
    proc.redoStack.push_back (snapshot());
    const auto s = proc.undoStack.back();
    proc.undoStack.pop_back();
    applySnapshot (s);
}

void EQEditor::redo()
{
    commitHistory();
    if (proc.redoStack.empty()) return;
    proc.undoStack.push_back (snapshot());
    const auto s = proc.redoStack.back();
    proc.redoStack.pop_back();
    applySnapshot (s);
}

void EQEditor::switchAB (int slot)
{
    if (slot == proc.abActive) return;
    commitHistory();
    proc.abSlot[proc.abActive] = snapshot();      // remember the slot we are leaving
    proc.abActive = slot;
    if (proc.abSlot[slot].size() == tracked.size()) applySnapshot (proc.abSlot[slot]);
    else proc.abSlot[slot] = snapshot();          // first visit: slot starts as a copy of the current settings
}
