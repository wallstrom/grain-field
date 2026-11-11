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
    vt = load (BinaryData::VT323Regular_ttf, BinaryData::VT323Regular_ttfSize);
    pixel = load (BinaryData::SilkscreenRegular_ttf, BinaryData::SilkscreenRegular_ttfSize);
    pixelBold = load (BinaryData::SilkscreenBold_ttf, BinaryData::SilkscreenBold_ttfSize);
    instance = this;
}

Fonts::~Fonts()
{
    if (instance == this) instance = nullptr;
}

juce::Font mono (float h, bool)
{
    h *= 1.5f;   // VT323 is drawn small inside its em; this keeps the design's sizes readable
    if (auto* f = Fonts::instance)
        if (f->vt != nullptr)
            return juce::Font (juce::FontOptions (f->vt).withHeight (h));
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h * 0.72f, juce::Font::plain));
}

juce::Font pixelFont (float h, bool bold)
{
    if (auto* f = Fonts::instance)
        if (auto tf = bold ? f->pixelBold : f->pixel)
            return juce::Font (juce::FontOptions (tf).withHeight (h)).withExtraKerningFactor (0.08f);
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

void text (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size, juce::Justification j, float width)
{
    g.setColour (c);
    const auto f = mono (size);
    g.setFont (f);
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

void label (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size, juce::Justification j, float width, bool bold)
{
    g.setColour (c);
    g.setFont (pixelFont (size, bold));
    const float h = std::ceil (size * 1.4f);
    auto r = juce::Rectangle<float> (x, y, width, h);
    if (j.testFlags (juce::Justification::right)) r = r.withX (x - width);
    else if (j.testFlags (juce::Justification::horizontallyCentred)) r = r.withX (x - width * 0.5f);
    g.drawText (s.toUpperCase(), r, j, false);
}

float labelWidth (const juce::String& s, float size, bool bold)
{
    return juce::GlyphArrangement::getStringWidth (pixelFont (size, bold), s.toUpperCase());
}

void dot (juce::Graphics& g, float cx, float cy, float r, juce::Colour c)
{
    // Everything is pixels here: a dot is a small square.
    const float s = juce::jmax (1.0f, std::round (r * 1.6f));
    g.setColour (c);
    g.fillRect (std::round (cx - s * 0.5f), std::round (cy - s * 0.5f), s, s);
}

void arrow (juce::Graphics& g, float x, float cy, float w, juce::Colour c)
{
    // A pixel arrow: a shaft and a stepped head.
    g.setColour (c);
    const float y = std::round (cy);
    g.fillRect (x, y, w - 1.0f, 1.0f);
    g.fillRect (x + w - 3.0f, y - 2.0f, 1.0f, 1.0f);
    g.fillRect (x + w - 2.0f, y - 1.0f, 1.0f, 3.0f);
    g.fillRect (x + w - 3.0f, y + 2.0f, 1.0f, 1.0f);
}

void caret (juce::Graphics& g, float cx, float cy, juce::Colour c)
{
    g.setColour (c);
    g.fillRect (std::round (cx) - 3.0f, std::round (cy) - 1.0f, 6.0f, 1.0f);
    g.fillRect (std::round (cx) - 2.0f, std::round (cy), 4.0f, 1.0f);
    g.fillRect (std::round (cx) - 1.0f, std::round (cy) + 1.0f, 2.0f, 1.0f);
}

void chevronPx (juce::Graphics& g, float cx, float cy, float dir, juce::Colour c)
{
    // ‹ or › as four 2-pixel steps.
    g.setColour (c);
    const float x0 = std::round (cx), y0 = std::round (cy);
    for (int k = -3; k <= 3; ++k)
    {
        const float dx = dir * (float) (3 - std::abs (k)) - (dir > 0 ? 1.0f : -1.0f) * 1.5f;
        g.fillRect (x0 + dx - 1.0f, y0 + (float) k, 2.0f, 1.0f);
    }
}

/* ---------------- The building blocks ---------------- */

void bay (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::panel);
    g.fillRect (r);
    g.setColour (col::frame);
    g.drawRect (r, 1.0f);
}

void moduleFrame (juce::Graphics& g, juce::Rectangle<float> r, bool lit)
{
    g.setColour (col::panel);
    g.fillRect (r);
    g.setColour (lit ? col::ink : col::frame);
    g.drawRect (r, 1.0f);
    g.setColour (col::off);
    g.drawRect (r.reduced (3.0f), 1.0f);
}

float plateWidth (const juce::String& s)
{
    return labelWidth (s, 9.0f, true) + 12.0f;
}

void plate (juce::Graphics& g, float x, float y, const juce::String& s, bool inverse)
{
    const float w = plateWidth (s);
    g.setColour (inverse ? col::ink : col::bg);
    g.fillRect (x, y - 6.0f, w, 13.0f);
    label (g, s, x + 6.0f, y - 6.0f, inverse ? col::bg : col::ink, 9.0f, juce::Justification::centredLeft, w, true);
}

void raised (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over)
{
    if (on)
    {
        g.setColour (over ? col::bright : col::ink);
        g.fillRect (r);
        g.setColour (juce::Colour (0xff9A9A9A));
        g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
        g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
        return;
    }
    g.setColour (over ? col::raisedOver : col::raised);
    g.fillRect (r);
    // Lit from the top left: a light edge there, a dark one bottom right.
    g.setColour (juce::Colour (0xff2C2C2C));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
    g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
    g.setColour (juce::Colour (0xff070707));
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
}

void lcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff050505));
    g.fillRect (r);
    g.setColour (juce::Colour (0xff3A3A3A));
    g.drawRect (r, 1.0f);
    g.setColour (col::bg);
    g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, 1.0f);
    g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, 1.0f, r.getHeight() - 2.0f);
}

void screen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::bg);
    g.fillRect (r);
    g.setColour (juce::Colour (0xff3A3A3A));
    g.drawRect (r, 1.0f);
    g.setColour (juce::Colour (0xff1C1C1C));
    g.drawRect (r.reduced (3.0f), 1.0f);
}

void ruleV (juce::Graphics& g, float x, float y0, float y1, juce::Colour c)
{
    g.setColour (c);
    g.fillRect (x, y0, 1.0f, y1 - y0);
}

void ruleH (juce::Graphics& g, float x0, float x1, float y, juce::Colour c)
{
    g.setColour (c);
    g.fillRect (x0, y, x1 - x0, 1.0f);
}

void pixelKnob (juce::Graphics& g, float x, float y, float norm, juce::Colour lit, juce::Colour tip, float s)
{
    constexpr int n = 11;
    constexpr float c0 = (n - 1) * 0.5f, sweep = 270.0f;
    const float at = juce::jlimit (0.0f, 1.0f, norm) * sweep;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
        {
            const float dx = (float) i - c0, dy = c0 - (float) j, d = std::hypot (dx, dy);
            if (d < 3.7f || d > 5.3f) continue;
            const float ang = std::atan2 (dy, dx) * 180.0f / juce::MathConstants<float>::pi;
            const float pos = std::fmod (std::fmod (225.0f - ang, 360.0f) + 360.0f, 360.0f);
            if (pos > sweep + 6.0f) continue;
            const bool on = pos <= at + 4.0f, isTip = std::abs (pos - at) < 22.0f;
            g.setColour (isTip ? tip : (on ? lit.withMultipliedAlpha (0.6f) : col::off));
            g.fillRect (x + (float) i * s, y + (float) j * s, s, s);
        }
    // The pointer: grid cells from the middle toward the tip.
    const float a = (225.0f - at) * juce::MathConstants<float>::pi / 180.0f;
    g.setColour (lit);
    int lastI = -1, lastJ = -1;
    for (float r = 0.6f; r <= 3.2f; r += 0.65f)
    {
        const int i = juce::roundToInt (c0 + r * std::cos (a)), j = juce::roundToInt (c0 - r * std::sin (a));
        if (i == lastI && j == lastJ) continue;
        lastI = i; lastJ = j;
        g.fillRect (x + (float) i * s, y + (float) j * s, s, s);
    }
}

void segBar (juce::Graphics& g, juce::Rectangle<float> r, float norm, int segments, juce::Colour lit, juce::Colour unlit)
{
    const float gap = 2.0f, w = std::floor ((r.getWidth() - gap * (float) (segments - 1)) / (float) segments);
    const int on = juce::roundToInt (juce::jlimit (0.0f, 1.0f, norm) * (float) segments);
    for (int i = 0; i < segments; ++i)
    {
        g.setColour (i < on ? lit : unlit);
        g.fillRect (r.getX() + (float) i * (w + gap), r.getY(), w, r.getHeight());
    }
}

// Dither tiles: white pixels on nothing, tiled across an area. Drawn without smoothing, so they stay square.
static const juce::Image& ditherTile (int kind)
{
    static juce::Image tiles[3];
    auto& im = tiles[kind];
    if (im.isNull())
    {
        const int n = kind == 2 ? 6 : 4;
        im = juce::Image (juce::Image::ARGB, n, n, true);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                bool on = false;
                if (kind == 0) on = ((x / 2) + (y / 2)) % 2 == 0;          // half: a 2-pixel checker
                else on = x < 2 && y < 2;                                    // a quarter, or sparser on a 6-pixel tile
                if (on) im.setPixelAt (x, y, juce::Colours::white);
            }
    }
    return im;
}

void dither (juce::Graphics& g, juce::Rectangle<float> r, float density, float alpha, float phase)
{
    const int kind = density >= 0.45f ? 0 : (density >= 0.2f ? 1 : 2);
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.setTiledImageFill (ditherTile (kind), juce::roundToInt (r.getX() + phase), juce::roundToInt (r.getY()), alpha);
    g.fillRect (r);
    g.restoreState();
}

void scanlines (juce::Graphics& g, juce::Rectangle<float> r, float alpha)
{
    g.setColour (juce::Colours::black.withAlpha (alpha));
    for (float y = r.getY() + 1.0f; y < r.getBottom(); y += 3.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
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
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0A0A0A));
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
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0A0A0A));
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff0A0A0A));
    setColour (juce::AlertWindow::textColourId, col::ink);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (fonts->vt != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return fonts->vt;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (col::frame);
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
        g.setColour (col::raisedOver);
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
    g.setColour (col::dim (0.45f));
    g.setFont (pixelFont (8.0f));
    g.drawText (name.toUpperCase(), area.toFloat().withTrimmedLeft (26.0f).withTrimmedTop (4.0f), juce::Justification::centredLeft, true);
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
    g.fillAll (col::panel);
    g.setColour (col::frame);
    g.drawRect (0, 0, w, h);
    text (g, t, 8.0f, 0.0f, col::ink, 11.0f, juce::Justification::centredLeft, (float) w);
}
void LookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (col::panel);
    g.fillPath (path);
    g.setColour (col::frame);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
} // namespace gf::ui
