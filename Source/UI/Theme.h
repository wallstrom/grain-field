#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours, type and shared drawing for Grain Field.
// A black screen lit like an old display: white light in a few strengths, four steps of dark grey for depth,
// square pixels and pixel type. Swell is simply the brightest thing; red is only for recording.
namespace gf::ui
{
namespace col
{
    const juce::Colour bg { 0xff000000 };        // the page
    const juce::Colour panel { 0xff0A0A0A };     // bays and modules, one step up
    const juce::Colour cell { 0xff0E0E0E };      // a control's own cell
    const juce::Colour raised { 0xff141414 };    // buttons, a step higher
    const juce::Colour raisedOver { 0xff1C1C1C };
    const juce::Colour frame { 0xff5A5A5A };     // a bay's border
    const juce::Colour rule { 0xff2E2E2E };      // between cells
    const juce::Colour faint { 0xff1E1E1E };
    const juce::Colour off { 0xff262626 };       // unlit pixels and segments
    const juce::Colour ink { 0xffE6E6E6 };
    const juce::Colour bright { 0xffFFFFFF };
    const juce::Colour accent = bright;          // Swell, and whatever you're touching: the brightest thing
    const juce::Colour rec { 0xffE5806F };
    inline juce::Colour dim (float alpha) { return ink.withAlpha (alpha); }
} // namespace col

// Embedded fonts: VT323 for words and numbers, Silkscreen for the small capital labels.
// One copy is shared by every open editor and freed when the last one closes.
struct Fonts
{
    Fonts();
    ~Fonts();
    juce::Typeface::Ptr vt, pixel, pixelBold;
    static inline Fonts* instance = nullptr;
};

juce::Font mono (float height, bool medium = false);    // VT323, sized to read like the old 11 pt text at 11
juce::Font pixelFont (float height, bool bold = false); // Silkscreen

// Text placed the way the design places it: x and y are the top left of the line.
void text (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = 11.0f,
           juce::Justification j = juce::Justification::centredLeft, float width = 400.0f);
float textWidth (const juce::String& s, float size = 11.0f);
// A small capital label in the pixel font, such as a knob's name.
void label (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = 8.0f,
            juce::Justification j = juce::Justification::centredLeft, float width = 300.0f, bool bold = false);
float labelWidth (const juce::String& s, float size = 8.0f, bool bold = false);

// Small drawn glyphs the fonts don't have.
void dot (juce::Graphics& g, float cx, float cy, float r, juce::Colour c);   // drawn as a square pixel
void arrow (juce::Graphics& g, float x, float cy, float w, juce::Colour c);   // →
void caret (juce::Graphics& g, float cx, float cy, juce::Colour c);           // ▾
void pencil (juce::Graphics& g, float x, float y, float size, juce::Colour c);
void chevronPx (juce::Graphics& g, float cx, float cy, float dir, juce::Colour c);   // ‹ › in pixels

// The building blocks of the panel.
void bay (juce::Graphics& g, juce::Rectangle<float> r);                      // a group of controls: border, one step up
void moduleFrame (juce::Graphics& g, juce::Rectangle<float> r, bool lit);    // a module: a double rule like an old game's menu
void plate (juce::Graphics& g, float x, float y, const juce::String& s, bool inverse);   // a name set into a top border
float plateWidth (const juce::String& s);
void raised (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over);          // a button's cell
void lcd (juce::Graphics& g, juce::Rectangle<float> r);                      // a recessed readout
void screen (juce::Graphics& g, juce::Rectangle<float> r);                   // a picture, set into the panel
void ruleV (juce::Graphics& g, float x, float y0, float y1, juce::Colour c = col::rule);
void ruleH (juce::Graphics& g, float x0, float x1, float y, juce::Colour c = col::rule);

// A knob drawn as pixel art on an 11 x 11 grid: a ring with a gap at the bottom, lit up to the value,
// the cells at the tip brightest, and a short pointer from the middle.
void pixelKnob (juce::Graphics& g, float x, float y, float norm, juce::Colour lit, juce::Colour tip, float cellSize = 3.0f);
// A bar of square segments.
void segBar (juce::Graphics& g, juce::Rectangle<float> r, float norm, int segments, juce::Colour lit, juce::Colour unlit = col::off);
// Grey made the way old screens made it: a pattern of lit and dark pixels. density 0.5, 0.25 or 0.12.
void dither (juce::Graphics& g, juce::Rectangle<float> r, float density, float alpha, float phase = 0.0f);
// Faint dark lines across a screen, every third pixel.
void scanlines (juce::Graphics& g, juce::Rectangle<float> r, float alpha = 0.35f);

// Every control carries a name and a sentence; the editor shows them in the footer.
void setHint (juce::Component& c, const juce::String& label, const juce::String& text);

// Components that show different hints over different parts of themselves.
struct HintSource
{
    virtual ~HintSource() = default;
    virtual std::pair<juce::String, juce::String> hintAt (juce::Point<int> local) = 0;
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getPopupMenuFont() override { return mono (12.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    int getPopupMenuBorderSize() override { return 6; }
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 16; }
    float getCallOutBoxCornerSize (const juce::CallOutBox&) override { return 0.0f; }

private:
    juce::SharedResourcePointer<Fonts> fonts;
};
} // namespace gf::ui
