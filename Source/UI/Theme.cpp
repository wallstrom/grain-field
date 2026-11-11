#include "Theme.h"
#include "BinaryData.h"

namespace gf::ui
{
static juce::Typeface::Ptr load (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

Fonts::Fonts()
{
    mono = load (BinaryData::DMMonoRegular_ttf, BinaryData::DMMonoRegular_ttfSize);
    monoMedium = load (BinaryData::DMMonoMedium_ttf, BinaryData::DMMonoMedium_ttfSize);
    serif = load (BinaryData::InstrumentSerifItalic_ttf, BinaryData::InstrumentSerifItalic_ttfSize);
    instance = this;
}

Fonts::~Fonts()
{
    if (instance == this) instance = nullptr;
}

juce::Font mono (float h, bool medium)
{
    h *= 1.08f;   // DM Mono runs small; this keeps the design's sizes readable
    if (auto* f = Fonts::instance)
        if (auto tf = medium ? f->monoMedium : f->mono)
            return juce::Font (juce::FontOptions (tf).withHeight (h)).withExtraKerningFactor (0.02f);
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
}

juce::Font serif (float h)
{
    if (auto* f = Fonts::instance)
        if (f->serif != nullptr)
            return juce::Font (juce::FontOptions (f->serif).withHeight (h));
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), h, juce::Font::italic));
}

void text (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size, juce::Justification j, float width)
{
    g.setColour (c);
    g.setFont (mono (size));
    const float h = std::ceil (size * 1.35f);
    auto r = juce::Rectangle<float> (x, y, width, h);
    if (j.testFlags (juce::Justification::right)) r = r.withX (x - width);
    else if (j.testFlags (juce::Justification::horizontallyCentred)) r = r.withX (x - width * 0.5f);
    g.drawText (s, r, j, false);
}

float textWidth (const juce::String& s, float size)
{
    return juce::GlyphArrangement::getStringWidth (mono (size), s);
}

void dot (juce::Graphics& g, float cx, float cy, float r, juce::Colour c)
{
    g.setColour (c);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
}

void arrow (juce::Graphics& g, float x, float cy, float w, juce::Colour c)
{
    g.setColour (c);
    g.drawLine (x, cy, x + w, cy, 1.0f);
    juce::Path p;
    p.startNewSubPath (x + w - 3.0f, cy - 3.0f);
    p.lineTo (x + w, cy);
    p.lineTo (x + w - 3.0f, cy + 3.0f);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void caret (juce::Graphics& g, float cx, float cy, juce::Colour c)
{
    juce::Path p;
    p.addTriangle (cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f, cx, cy + 2.0f);
    g.setColour (c);
    g.fillPath (p);
}

void pencil (juce::Graphics& g, float x, float y, float s, juce::Colour c)
{
    // A thin diagonal pencil: body, tip and a cap line.
    juce::Path p;
    p.startNewSubPath (x + s * 0.15f, y + s * 0.85f);
    p.lineTo (x + s * 0.3f, y + s * 0.55f);
    p.lineTo (x + s * 0.8f, y + s * 0.05f);
    p.lineTo (x + s * 0.95f, y + s * 0.2f);
    p.lineTo (x + s * 0.45f, y + s * 0.7f);
    p.closeSubPath();
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    g.drawLine (x + s * 0.68f, y + s * 0.17f, x + s * 0.83f, y + s * 0.32f, 1.0f);
}

void setHint (juce::Component& c, const juce::String& label, const juce::String& t)
{
    c.getProperties().set ("hintLabel", label);
    c.getProperties().set ("hintText", t);
}

/* ---------------- Look and feel ---------------- */

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141414));
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::headerTextColourId, col::dim (0.4f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::dim (0.08f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
    setColour (juce::TextEditor::textColourId, col::ink);
    setColour (juce::TextEditor::backgroundColourId, col::bg);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, col::ink);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, col::accent);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff141414));
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff141414));
    setColour (juce::AlertWindow::textColourId, col::ink);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (fonts->mono != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return fonts->mono;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (col::dim (0.12f));
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& t, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 50; idealHeight = 11; return; }
    idealHeight = 24;
    idealWidth = (int) textWidth (t, 12.0f) + 56;
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                     bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& t,
                                     const juce::String&, const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (col::dim (0.1f));
        g.fillRect (area.getX() + 12, area.getCentreY(), area.getWidth() - 24, 1);
        return;
    }
    auto r = area.toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour (col::dim (0.07f));
        g.fillRect (r);
    }
    if (isTicked) dot (g, r.getX() + 13.0f, r.getCentreY(), 2.5f, col::accent);

    juce::Colour c = textColour != nullptr ? *textColour : col::ink;
    if (! isActive) c = col::dim (0.3f);
    else if (! isHighlighted && ! isTicked && textColour == nullptr) c = col::dim (0.75f);
    g.setColour (c);
    g.setFont (mono (12.0f));
    g.drawText (t, r.withTrimmedLeft (26.0f).withTrimmedRight (20.0f), juce::Justification::centredLeft, true);

    if (hasSubMenu)
    {
        juce::Path p;
        const float x = r.getRight() - 12.0f, y = r.getCentreY();
        p.startNewSubPath (x - 2.0f, y - 3.5f);
        p.lineTo (x + 1.5f, y);
        p.lineTo (x - 2.0f, y + 3.5f);
        g.setColour (col::dim (0.4f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setColour (col::dim (0.35f));
    g.setFont (mono (10.0f));
    g.drawText (name.toLowerCase(), area.toFloat().withTrimmedLeft (26.0f).withTrimmedTop (4.0f), juce::Justification::centredLeft, true);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int, int, juce::TextEditor&)
{
    g.fillAll (col::bg);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (col::accent);
    g.fillRect (0, h - 1, w, 1);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
    g.setColour (col::dim (0.12f));
    g.drawRect (0, 0, w, h);
    text (g, t, 8.0f, 0.0f, col::ink, 11.0f, juce::Justification::centredLeft, (float) w);
}
void LookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colour (0xff141414));
    g.fillPath (path);
    g.setColour (col::dim (0.18f));
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
} // namespace gf::ui
