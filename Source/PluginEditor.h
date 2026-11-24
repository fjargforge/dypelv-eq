#pragma once
#include "PluginProcessor.h"
#include "PrismLook.h"
#include "BandCard.h"

struct PresetEntry
{
    juce::String name;
    int factory = -1;      // index into Presets::factory(), or -1 for a user file
    juce::File file;
};

class EQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EQEditor (EQProcessor&);
    ~EQEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct SpecLayer
    {
        std::vector<float> smoothed, target, disp;
        std::vector<double> prefix;
        std::vector<float> avgPow;          // long-term average of the power per bin (spectrum averaging)
        int idle = 0;
    };
    struct BandSig
    {
        bool on = false, dyn = false;
        int type = -1, split = 0;
        float slope = -1.0f, f = -1.0f, g = -1.0f, q = -1.0f, g2 = 0.0f, tg = 0.0f;
        bool operator== (const BandSig& o) const
        {
            return on == o.on && dyn == o.dyn && type == o.type && slope == o.slope && split == o.split
                   && f == o.f && g == o.g && q == o.q && g2 == o.g2 && tg == o.tg;
        }
    };

    void timerCallback() override;

    int geti (const char* id) const;
    float get (int band, const char* name) const;
    float range() const;                           // the dB range as drawn (glides to the chosen one)
    float targetRange() const;                     // the chosen dB range
    float zeroY() const;                           // where 0 dB is drawn: above the middle, there is more room below for cuts
    float lowRange() const;                        // how far below 0 dB a node can be dragged
    void showFaq();
    static constexpr float kZeroFrac = 0.40f;
    float anaRange() const;
    void setParam (const juce::String& pid, float value);          // plain set
    void setParamGesture (const juce::String& pid, float value);   // set wrapped in begin/end gesture

    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForGain (float g) const;
    float gainForY (float y) const;
    float yForLevel (float db) const;       // analyzer / meter scale (dBFS)
    juce::Point<float> nodePos (int band) const;
    juce::Point<float> nodePos2 (int band) const;      // second dot of a mirrored pair
    bool animated (int band) const;         // LFO or dynamics active
    float effFreq (int band) const;
    float effGain (int band) const;
    float effQ (int band) const;
    float effGain2 (int band) const;
    int hitBand (juce::Point<float>, int* side = nullptr) const;      // side: 1 = the mirror dot of a pair

    // solo
    int soloedBand() const;
    void toggleSolo (int band);

    // band management
    void selectBand (int b);
    int addBand (juce::Point<float>, int type);       // returns the new band, or -1 when all 24 are in use
    void resetBand (int b);
    void resetAllBands();
    void deleteBand (int b);
    void invertBand (int b);                                   // flip the band's response
    void splitBand (int b, int mode, bool inverted);           // one band -> two bands (mode 1 = L|R, 2 = M|S)
    void stepBand (int dir);                                   // jump to the next / previous band (sorted by frequency)
    void positionCard();

    // menus
    void showSettingsMenu();
    void applyUiScale (int percent);             // interface scale on top of whatever scale the host gives
    bool scaleFits (int percent) const;
    void setPalette (int mode);
    void dismissHint();
    void drawHint (juce::Graphics&);
    void showBandMenu (int band, juce::Rectangle<int> anchor = {});
    void runMenu (juce::PopupMenu&, const juce::PopupMenu::Options&, std::function<void (int)>);
    bool menuBlocked() const;
    void showPhaseMenu();
    void showParamMenu (const juce::String& pid);       // MIDI learn for one parameter
    void toggleFullscreen();
    bool isFullNow() const;
    void syncFullIcon();       // the button shows "enter full screen" or "back to a window"
    void showAddMenu (juce::Point<float>);

    // drag handling (gestures are always balanced)
    void beginDrag (int band, juce::Point<float> mouse, int side = 0);
    void endDrag();

    // presets
    std::vector<PresetEntry> presetEntries() const;
    juce::String currentPresetName() const;
    void setPresetName (const juce::String&);
    void showPresetMenu();
    void promptSavePreset();
    void savePreset (const juce::String& name);
    void loadEntry (const PresetEntry&);
    void stepPreset (int dir);

    // history: undo / redo and A / B
    std::vector<float> snapshot() const;
    void applySnapshot (const std::vector<float>&);
    void commitHistory();
    void trackHistory();
    void undo();
    void redo();
    void switchAB (int slot);

    // cached drawing data
    bool rebuildCurves();                          // true when the response curves changed
    void vblankTick();                             // once per screen refresh: keeps the curve in step with the display
    juce::Rectangle<int> plotDirtyRect() const;    // plot + right gutter + room for a node's glow, badges and arrows
    void analyzeTap (EQ::AnalyzerTap&, SpecLayer&, int res, bool& rebuild);
    void updateSpectrumTarget (SpecLayer&);
    void drawLayer (juce::Graphics&, const SpecLayer&, juce::Colour, float fillA, float lineA, float lineW) const;

    EQProcessor& proc;
    PrismLook look;

    UI::TextLink presetBtn { "Presets" }, abA { "A" }, abB { "B" }, phaseBtn { "Zero Latency" };
    UI::IconButton bypassBtn, analyzerBtn, deltaBtn, matchBtn, undoBtn, redoBtn, prevBtn, nextBtn, fullBtn;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<BA> deltaA, matchA;
    UI::ValueField outField { "OUT" };
    std::unique_ptr<SA> outA;
    UI::BarLook barLook;
    juce::ComboBox rangeBox;
    std::unique_ptr<BA> bypassA;
    std::unique_ptr<CA> rangeA;

    // floating band card (always in one place: top right of the plot, or where the user dropped it)
    UI::BandCard card;
    bool cardOpen = false, cardHiddenByDrag = false;
    float rangeShown = 0.0f;
    float rangeFrom = 0.0f, rangeGoal = 0.0f;
    double rangeT0 = 0.0;
    juce::TextButton faqBtn { "?" };
    juce::Rectangle<int> statusRect;

    // analyzer: input ("original") and output (changed by bands) layers
    std::array<std::unique_ptr<juce::dsp::FFT>, 4> ffts;
    std::array<std::unique_ptr<juce::dsp::WindowingFunction<float>>, 4> windows;
    std::vector<float> fftData;
    SpecLayer layIn, layOut;
    int lastRes = -1, lastSmooth = -1, specCols = 0;

    // cached EQ response (dB per pixel column), rebuilt only when a band changes
    std::array<BandSig, EQ::numBands> sigs;
    std::array<std::vector<float>, EQ::numBands> bandCurve, bandCurveB, targetCurve;   // A side, B side (split), dynamic target
    std::vector<float> curveTotal, curveX;   // curveX: where each sample sits across the plot (0..1); denser around every band's centre
    int curveCols = 0, curveBase = 0;
    double curveSr = 0.0;

    // meters
    float peakDb[2] { -120.0f, -120.0f }, peakHold[2] { -120.0f, -120.0f };
    int holdTimer[2] { 0, 0 };
    float lufsM = -120.0f, lufsS = -120.0f, lufsI = -120.0f;
    uint32_t lastBlockCount = 0;
    int idleTicks = 100;
    bool audioActive = false;
    juce::Rectangle<float> lufsPill;

    // history tracking (user edits only: changes count when the mouse was used recently)
    struct InputWatcher : juce::MouseListener
    {
        int* ticks = nullptr;
        void mouseDown (const juce::MouseEvent&) override { if (ticks != nullptr) *ticks = 0; }
        void mouseDrag (const juce::MouseEvent&) override { if (ticks != nullptr) *ticks = 0; }
        void mouseUp (const juce::MouseEvent&) override { if (ticks != nullptr) *ticks = 0; }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { if (ticks != nullptr) *ticks = 0; }
    };
    InputWatcher watcher;
    std::vector<juce::RangedAudioParameter*> tracked;
    std::vector<float> lastSeen, lastStable;
    int settleTicks = 0, inputTicks = 100000;
    bool userChange = false;

    juce::Rectangle<float> plotRect;
    int selected = -1, hovered = -1, dragBand = -1, dragSide = 0, lastRange = -1;
    juce::Point<float> dragOffset, dragStart;
    float dragGain0 = 0.0f, dragGainOff = 0.0f, dragY0 = 0.0f;
    bool dragMoved = false;


    // card toggling and menu guard
    bool cardOpenBeforeClick = false;
    juce::uint32 menuClosedAt = 0;
    bool menuOpen = false;

    // status flash (MIDI learn feedback), full screen
    juce::String flashMsg;
    int flashTicks = 0, lastLearnTick = 0;
    bool maximised = false, lastArmed = false, lastFull = false, lastMonoHint = false;

    // interface scale: the host may set its own scale (Windows) and overwrite ours, so the timer notices and puts ours back on top
    int uiScalePct = 100;
    float hostScale = 1.0f, appliedScale = 1.0f;

    // first-run hint: a small card over the plot until the first band is added or it is clicked
    bool hintVisible = false;
    juce::Rectangle<float> hintRect;
    juce::Image hintImg;
    float hintImgScale = 0.0f;

    // layers that only change when their key does (size, plot, range ...): drawn once into an image, then blitted every frame
    struct LayerCache { juce::Image img; std::array<float, 12> key {}; };
    LayerCache layerBg, layerScale;
    double lastPlotRepaintMs = 0.0;
    mutable std::unordered_map<uint64_t, std::atomic<float>*> paramCache;
    std::atomic<float>* paramPtr (int band, const char* name) const;
    void drawCached (juce::Graphics&, LayerCache&, std::array<float, 12> key, const std::function<void (juce::Graphics&)>& draw);
    juce::Point<int> restoreSize;       // a press only becomes a drag after a few pixels, so clicks and double-clicks never nudge a band

    juce::VBlankAttachment vblank;      // declared last: it is destroyed first, before anything its callback touches

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQEditor)
};
