#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colours, type and shared drawing for Grain Field.
// A black field, one warm ink in a few strengths, one accent for whatever Swell touches
// and for what is happening now. Small lowercase mono type, a serif for the name.
namespace gf::ui
{
namespace col
{
    const juce::Colour bg { 0xff0C0C0C };
    const juce::Colour ink { 0xffDCD6C8 };
    const juce::Colour accent { 0xffE0784A };
    inline juce::Colour dim (float alpha) { return ink.withAlpha (alpha); }
} // namespace col

// Embedded fonts: DM Mono for everything, Instrument Serif for the name.
// One copy is shared by every open editor and freed when the last one closes.
struct Fonts
{
    Fonts();
    ~Fonts();
    juce::Typeface::Ptr mono, monoMedium, serif;
    static inline Fonts* instance = nullptr;
};

juce::Font mono (float height, bool medium = false);
juce::Font serif (float height);

// Text placed the way the design places it: x and y are the top left of the line.
void text (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = 11.0f,
           juce::Justification j = juce::Justification::centredLeft, float width = 400.0f);
float textWidth (const juce::String& s, float size = 11.0f);

// Small drawn glyphs the fonts don't have.
void dot (juce::Graphics& g, float cx, float cy, float r, juce::Colour c);
void arrow (juce::Graphics& g, float x, float cy, float w, juce::Colour c);   // →
void caret (juce::Graphics& g, float cx, float cy, juce::Colour c);           // ▾
void pencil (juce::Graphics& g, float x, float y, float size, juce::Colour c);

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
    float getCallOutBoxCornerSize (const juce::CallOutBox&) override { return 2.0f; }

private:
    juce::SharedResourcePointer<Fonts> fonts;
};
} // namespace gf::ui
