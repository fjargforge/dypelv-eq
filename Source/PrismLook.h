#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Film grain: one small pre-rendered tile of faint white specks, tiled over the clip area. Costs a plain blit, nothing is generated per frame.
// The specks lift the colours underneath by ~3 levels on average, so the palette is set ~3 levels darker to compensate.
namespace Grain
{
    inline constexpr float mean = 0.012f, spread = 0.010f;      // speck opacity: raise both for more grain, 0 / 0 = off

    inline bool enabled = true;                                  // the display menu switches it (a setting of the person, Settings.h)

    inline void paint (juce::Graphics& g)
    {
        if (! enabled || (mean <= 0.0f && spread <= 0.0f)) return;
        static juce::Image tile;
        static float tileScale = 0.0f;
        const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (tile.isNull() || std::abs (sc - tileScale) > 0.01f)
        {
            tileScale = sc;
            const int n = juce::roundToInt (256.0f * sc);
            tile = juce::Image (juce::Image::ARGB, n, n, true);
            juce::Image::BitmapData bd (tile, juce::Image::BitmapData::writeOnly);
            juce::Random rnd (0x5eed1234);
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x)
                {
                    const float gauss = (rnd.nextFloat() + rnd.nextFloat() + rnd.nextFloat() - 1.5f) * 2.0f;     // ~N(0,1)
                    const float a = juce::jlimit (0.0f, 0.06f, mean + spread * gauss);
                    bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, a));
                }
        }
        g.setFillType (juce::FillType (tile, juce::AffineTransform::scale (1.0f / tileScale)));
        g.fillRect (g.getClipBounds());
    }
}

struct PrismLook : juce::LookAndFeel_V4
{
    PrismLook()
    {
        using namespace juce;
        const Colour text (0xffe8e8e8), field (0xff1c1c1c), line (0xff343434), accent (0xff5eead4);

       #if JUCE_MAC
        setDefaultSansSerifTypefaceName ("Avenir Next");     // missing fonts fall back to the system default
       #elif JUCE_WINDOWS
        setDefaultSansSerifTypefaceName ("Segoe UI");
       #endif

        setColour (ComboBox::backgroundColourId, field);
        setColour (ComboBox::outlineColourId, line);
        setColour (ComboBox::textColourId, text);
        setColour (ComboBox::arrowColourId, text.withAlpha (0.6f));
        setColour (PopupMenu::backgroundColourId, Colour (0xff181818));
        setColour (PopupMenu::textColourId, text);
        setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (PopupMenu::highlightedTextColourId, text);

        setColour (Slider::textBoxTextColourId, text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, accent.withAlpha (0.3f));
        setColour (TextEditor::backgroundColourId, field);
        setColour (TextEditor::textColourId, text);
        setColour (TextEditor::outlineColourId, line);
        setColour (TextEditor::focusedOutlineColourId, accent);

        setColour (TextButton::buttonColourId, field);
        setColour (TextButton::buttonOnColourId, accent);
        setColour (TextButton::textColourOffId, text.withAlpha (0.85f));
        setColour (TextButton::textColourOnId, Colour (0xff131313));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        LookAndFeel_V4::drawPopupMenuBackground (g, w, h);
        Grain::paint (g);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float d = jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float rad = d * 0.5f;
        const float angle = a0 + pos * (a1 - a0);
        const auto col = s.findColour (Slider::rotarySliderFillColourId);

        g.setColour (Colour (0xff1c1c1c));
        g.fillEllipse (Rectangle<float> (d - 10.0f, d - 10.0f).withCentre (c));

        Path track;
        track.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);
        g.setColour (Colours::white.withAlpha (0.09f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        Path val;
        val.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, angle, true);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

        const float pr = rad - 11.0f;
        const Point<float> p (c.x + pr * std::sin (angle), c.y - pr * std::cos (angle));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (p));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bg;
        if (down) c = c.brighter (0.15f);
        else if (highlighted) c = c.brighter (0.08f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
};
