#include "Widgets.h"

namespace gf::ui
{
/* ---------------- ParamControl ---------------- */

ParamControl::ParamControl (GrainFieldProcessor& p, int sp) : proc (p), sceneParam (sp)
{
    param = proc.apvts.getParameter (spec (sp).id);
    setHint (*this, juce::String (spec (sp).name).toLowerCase(), spec (sp).hint);
    setRepaintsOnMouseActivity (false);
}

ParamControl::ParamControl (GrainFieldProcessor& p, const juce::String& paramId) : proc (p)
{
    param = proc.apvts.getParameter (paramId);
}

float ParamControl::getNorm() const
{
    if (editingNext()) return proc.getPartValue (proc.getNextPart(), sceneParam);
    return param->getValue();
}

float ParamControl::getDisplayNorm() const
{
    if (sceneParam < 0 || editingNext() || ! proc.engine.isRunning()) return getNorm();
    return proc.engine.getDisplayNorm (sceneParam);
}

void ParamControl::beginEdit()
{
    dragging = true;
    if (! editingNext()) param->beginChangeGesture();
}

void ParamControl::setNorm (float n)
{
    n = juce::jlimit (0.0f, 1.0f, n);
    if (editingNext()) proc.setPartValue (proc.getNextPart(), sceneParam, n);
    else if (std::abs (param->getValue() - n) > 1.0e-6f) param->setValueNotifyingHost (n);
    repaint();
}

void ParamControl::endEdit()
{
    if (! dragging) return;
    dragging = false;
    if (! editingNext()) param->endChangeGesture();
}

juce::String ParamControl::valueTextFor (float n) const
{
    if (sceneParam >= 0) return formatValue (sceneParam, param->convertFrom0to1 (n));
    return param->getText (n, 16).toLowerCase();
}

juce::String ParamControl::valueText() const { return valueTextFor (getNorm()); }

float ParamControl::dragToNorm (const juce::MouseEvent& e, float startNorm)
{
    const float range = e.mods.isShiftDown() ? 900.0f : 180.0f;
    return startNorm - (float) e.getDistanceFromDragStartY() / range;
}

void ParamControl::mouseDown (const juce::MouseEvent&)
{
    ignoreDrag = false;
    dragStartNorm = getNorm();
    if (! dragging) beginEdit();
}

void ParamControl::mouseDrag (const juce::MouseEvent& e)
{
    if (ignoreDrag || ! dragging) return;
    setNorm (dragToNorm (e, dragStartNorm));
}

void ParamControl::mouseUp (const juce::MouseEvent&)
{
    ignoreDrag = false;
    endEdit();
}

void ParamControl::mouseDoubleClick (const juce::MouseEvent&)
{
    // Back to where it starts. The second press already began the change, so this is part of it,
    // and the hand's small movements afterwards don't count as a drag.
    if (! dragging) beginEdit();
    setNorm (getDefaultNorm());
    ignoreDrag = true;
}

void ParamControl::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = (std::abs (w.deltaY) > 1.0e-6f ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    beginEdit();
    setNorm (getNorm() + d * 0.12f);
    endEdit();
}

void ParamControl::tick()
{
    const float n = getNorm(), d = getDisplayNorm();
    const bool ed = editingNext();
    if (std::abs (n - shownNorm) > 1.0e-4f || std::abs (d - shownDisplay) > 2.0e-3f || ed != shownEditing)
    {
        shownNorm = n;
        shownDisplay = d;
        shownEditing = ed;
        repaint();
    }
}

/* ---------------- Knob ---------------- */

Knob::Knob (GrainFieldProcessor& p, int sp, const juce::String& l) : ParamControl (p, sp), name (l) {}
Knob::Knob (GrainFieldProcessor& p, const juce::String& id, const juce::String& l) : ParamControl (p, id), name (l) {}

void Knob::paint (juce::Graphics& g)
{
    // A cell of its own: the name, a pixel knob, the value. Values Swell moves are drawn at full brightness,
    // and while the sound is somewhere else (Swell travelling, a blend) the tip blinks.
    const float w = (float) getWidth(), h = (float) getHeight(), n = getNorm(), d = getDisplayNorm();
    const bool ed = editingNext(), swells = sceneParam >= 0 && swellMoves (sceneParam);
    const bool moving = ! ed && std::abs (d - n) > 0.015f;
    const bool blink = (juce::Time::getMillisecondCounter() / 300) % 2 == 0;
    g.setColour (hover || dragging ? col::raised : col::cell);
    g.fillRect (getLocalBounds());
    ruleV (g, w - 1.0f, 0.0f, h);
    ruleH (g, 0.0f, w, h - 1.0f);

    const juce::Colour lit = ed ? col::dim (0.45f) : (swells ? col::bright : juce::Colour (0xffBDBDBD));
    const juce::Colour tip = moving && blink ? col::dim (0.35f) : (ed ? col::dim (0.6f) : col::bright);
    const float knobPx = 33.0f;
    if (horizontal)
    {
        const float ky = std::round ((h - knobPx) * 0.5f);
        pixelKnob (g, 8.0f, ky, moving ? d : n, lit, tip);
        label (g, name, 8.0f + knobPx + 8.0f, h * 0.5f - 13.0f, col::dim (0.55f), 7.0f);
        text (g, valueText(), 8.0f + knobPx + 8.0f, h * 0.5f - 2.0f, hover || dragging ? col::bright : col::ink, 11.0f);
        return;
    }
    label (g, name, w * 0.5f, 5.0f, col::dim (0.55f), 7.0f, juce::Justification::centred, w);
    pixelKnob (g, std::round ((w - knobPx) * 0.5f), 16.0f, moving ? d : n, lit, tip);
    text (g, valueText(), w * 0.5f, 49.0f, hover || dragging ? col::bright : (swells && ! ed ? col::bright : col::ink), 10.0f,
          juce::Justification::centred, w);
}

/* ---------------- HairSlider ---------------- */

HairSlider::HairSlider (GrainFieldProcessor& p, int sp, const juce::String& c) : ParamControl (p, sp), caption (c) {}

HairSlider::HairSlider (GrainFieldProcessor& p, const juce::String& id, const juce::String& c) : ParamControl (p, id), caption (c)
{
    setHint (*this, caption, "How loud Grain Field is. The last thing before the output");
}

juce::Rectangle<float> HairSlider::track() const
{
    const float x0 = caption.isEmpty() ? 2.0f : labelWidth (caption, 7.0f) + 10.0f;
    const float h = 8.0f;
    return { x0, std::round (((float) getHeight() - h) * 0.5f), (float) getWidth() - x0 - 2.0f, h };
}

float HairSlider::dragToNorm (const juce::MouseEvent& e, float)
{
    const auto t = track();
    return (e.position.x - t.getX()) / t.getWidth();
}

void HairSlider::paint (juce::Graphics& g)
{
    // A level as a row of square segments.
    const auto t = track();
    const float n = getNorm();
    const bool ed = editingNext(), active = hover || dragging;
    if (caption.isNotEmpty())
        label (g, caption, 0.0f, t.getCentreY() - 6.0f, col::dim (active ? 0.8f : 0.5f), 7.0f);
    const int segs = juce::jlimit (6, 16, (int) (t.getWidth() / 7.0f));
    segBar (g, t, n, segs, ed ? col::dim (0.5f) : (active ? col::bright : juce::Colour (0xffBDBDBD)));
}

/* ---------------- DragNumber ---------------- */

DragNumber::DragNumber (GrainFieldProcessor& p, int sp, const juce::String& w) : ParamControl (p, sp), word (w)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void DragNumber::paint (juce::Graphics& g)
{
    const bool active = hover || dragging;
    label (g, word, 0.0f, 0.0f, col::dim (0.5f), 7.0f);
    text (g, juce::String (juce::roundToInt (getNorm() * 100.0f)), labelWidth (word, 7.0f) + 6.0f, -3.0f,
          editingNext() ? col::dim (0.6f) : (active ? col::bright : col::ink), 11.0f);
}

/* ---------------- SwellLine ---------------- */

SwellLine::SwellLine (GrainFieldProcessor& p) : ParamControl (p, pSwell) {}

float SwellLine::dragToNorm (const juce::MouseEvent& e, float)
{
    return (e.position.x - lineX) / lineW();
}

SwellLine::Zone SwellLine::zoneAt (juce::Point<float> pt) const
{
    if (! getLocalBounds().toFloat().contains (pt)) return nowhere;
    if (pt.x < cellW) return onQuiet;
    if (wallRect().contains (pt)) return onWall;
    if (pt.x >= lineX - 2.0f && pt.x < lineX + lineW() + 2.0f) return onLine;
    return nowhere;
}

void SwellLine::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != hoverZone) { hoverZone = z; repaint(); }
    setMouseCursor (z == nowhere ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
}

void SwellLine::mouseDown (const juce::MouseEvent& e)
{
    ignoreDrag = false;
    pressZone = zoneAt (e.position);
    const float hx = lineX + lineW() * getNorm();
    const bool grabbed = pressZone == onLine && std::abs (e.position.x - hx) <= 10.0f;
    if (grabbed || (editingNext() && pressZone == onLine))
    {
        // Taking the handle: Swell is yours again.
        proc.cancelSwellFade();
        ParamControl::mouseDown (e);
        if (! grabbed) setNorm (dragToNorm (e, 0.0f));
    }
}

void SwellLine::mouseDrag (const juce::MouseEvent& e)
{
    if (ignoreDrag) return;
    if (! dragging)
    {
        if (pressZone != onLine || e.getDistanceFromDragStart() < 4) return;
        proc.cancelSwellFade();
        ParamControl::mouseDown (e);
    }
    setNorm (dragToNorm (e, 0.0f));
}

void SwellLine::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDragging = dragging, wasReset = ignoreDrag;
    ParamControl::mouseUp (e);
    if (wasDragging || wasReset || editingNext()) return;
    // A click: Swell travels there by itself.
    if (pressZone == onQuiet) proc.swellFadeTo (0.0f);
    else if (pressZone == onWall) proc.swellFadeTo (1.0f);
    else if (pressZone == onLine) proc.swellFadeTo (juce::jlimit (0.0f, 1.0f, dragToNorm (e, 0.0f)));
    repaint();
}

void SwellLine::mouseDoubleClick (const juce::MouseEvent& e)
{
    proc.cancelSwellFade();
    ParamControl::mouseDoubleClick (e);
}

std::pair<juce::String, juce::String> SwellLine::hintAt (juce::Point<int> pt)
{
    const auto z = zoneAt (pt.toFloat());
    const auto over = juce::String (" over ") + timeText (proc.getSwellFadeSeconds());
    if (z == onQuiet) return { "fade to quiet", "Click and Swell travels all the way down" + over };
    if (z == onWall) return { "fade to the wall", "Click and Swell travels all the way up" + over };
    return { "swell", "From almost nothing to a full wall. Drag the handle to move it yourself, or click a point on the bar and it travels there" + over };
}

void SwellLine::tick()
{
    ParamControl::tick();
    const auto& sc = proc.getScenes();
    std::array<float, numScenes> m {};
    for (int i = 0; i < numScenes; ++i)
        m[(size_t) i] = sc.slots[(size_t) i].used ? sc.slots[(size_t) i].norm[(size_t) pSwell] : -1.0f;
    const int f = proc.isSwellFading() ? (int) (proc.getSwellFadeRemaining() * 8.0) : -1;
    if (m != marks || f != shownFade) { marks = m; shownFade = f; repaint(); }
}

void SwellLine::paint (juce::Graphics& g)
{
    // Cells: quiet, the bar, wall, the value. The bar is solid white blocks up to the handle; while Swell
    // travels, a dithered stretch shows where it's heading.
    const float h = (float) getHeight(), n = getNorm(), d = getDisplayNorm();
    const bool ed = editingNext();
    const bool fading = ! ed && proc.isSwellFading();
    const juce::Colour main = ed ? col::dim (0.6f) : col::bright;

    raised (g, { 0.0f, 0.0f, cellW, h }, false, hoverZone == onQuiet && ! ed);
    label (g, "quiet", cellW * 0.5f, h * 0.5f - 6.0f, hoverZone == onQuiet && ! ed ? col::bright : col::dim (0.55f), 7.0f, juce::Justification::centred, cellW);
    const float wx = wallRect().getX();
    raised (g, wallRect(), false, hoverZone == onWall && ! ed);
    label (g, "wall", wx + cellW * 0.5f, h * 0.5f - 6.0f, hoverZone == onWall && ! ed ? col::bright : col::dim (0.55f), 7.0f, juce::Justification::centred, cellW);
    const float vx = wx + cellW;
    g.setColour (col::cell);
    g.fillRect (vx, 0.0f, (float) getWidth() - vx, h);
    ruleV (g, vx, 0.0f, h);
    text (g, juce::String (juce::roundToInt (n * 100.0f)), vx + ((float) getWidth() - vx) * 0.5f, fading ? 2.0f : h * 0.5f - 13.0f, main, fading ? 16.0f : 18.0f,
          juce::Justification::centred, (float) getWidth() - vx);
    if (fading)
    {
        const auto r = countdownRect();
        label (g, timeText (proc.getSwellFadeRemaining()), r.getCentreX(), r.getY(), col::bright, 7.0f, juce::Justification::centred, r.getWidth());
    }

    const auto bar = juce::Rectangle<float> (lineX - 4.0f, 8.0f, lineW() + 8.0f, h - 16.0f);
    lcd (g, bar);
    const float by = bar.getY() + 4.0f, bh = bar.getHeight() - 8.0f;
    const float filled = lineW() * n;
    g.setColour (main.withAlpha (0.88f));
    for (float x = 0.0f; x + 6.0f <= filled; x += 8.0f) g.fillRect (lineX + x, by, 6.0f, bh);

    // Where Swell sits in the other scenes: small grey pixels along the bottom.
    const int active = proc.getActivePart(), next = ed ? proc.getNextPart() : -1;
    for (int i = 0; i < numScenes; ++i)
    {
        const float m = marks[(size_t) i];
        if (m < 0.0f || i == (ed ? next : active)) continue;
        g.setColour (juce::Colour (0xff6A6A6A));
        g.fillRect (std::round (lineX + lineW() * m) - 2.0f, bar.getBottom() - 4.0f, 4.0f, 3.0f);
    }

    if (fading)
    {
        // Travelling: a dithered stretch from here to where it's going, and how long is left.
        const float xt = lineX + lineW() * proc.getSwellFadeTarget(), xn = lineX + filled;
        dither (g, { juce::jmin (xt, xn), by, std::abs (xt - xn), bh }, 0.5f, 0.6f, (float) (juce::Time::getMillisecondCounter() / 120 % 4));
        g.setColour (col::bright);
        g.drawRect (std::round (xt) - 3.0f, by - 1.0f, 6.0f, bh + 2.0f, 1.0f);
    }
    else if (! ed && std::abs (d - n) > 0.01f)   // a blend on its way
    {
        const float xd = lineX + lineW() * d;
        dither (g, { juce::jmin (xd, lineX + filled), by, std::abs (xd - lineX - filled), bh }, 0.25f, 0.5f);
    }

    // The handle: a bright block taller than the bar.
    const float x = std::round (lineX + filled);
    g.setColour (hoverZone == onLine || dragging ? col::bright : main);
    g.fillRect (x - 4.0f, bar.getY() + 1.0f, 8.0f, bar.getHeight() - 2.0f);
}

/* ---------------- TextToggle ---------------- */

TextToggle::TextToggle (GrainFieldProcessor& p, const juce::String& id, const juce::String& w, bool a)
    : param (p.apvts.getParameter (id)), word (w), accentWhenOn (a) {}

void TextToggle::tick()
{
    const bool on = param->getValue() > 0.5f;
    if (on != shown) { shown = on; repaint(); }
}

void TextToggle::paint (juce::Graphics& g)
{
    // A button cell: lit (inverse) when on.
    const bool on = param->getValue() > 0.5f, over = isMouseOver();
    raised (g, getLocalBounds().toFloat(), on, over);
    label (g, word, (float) getWidth() * 0.5f, (float) getHeight() * 0.5f - 6.0f, on ? col::bg : (over ? col::bright : col::dim (0.7f)), 8.0f,
           juce::Justification::centred, (float) getWidth());
    juce::ignoreUnused (accentWhenOn);
}

void TextToggle::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
    param->endChangeGesture();
    repaint();
}

/* ---------------- SpaceModeSwitch ---------------- */

SpaceModeSwitch::SpaceModeSwitch (GrainFieldProcessor& p) : param (p.apvts.getParameter (ids::spaceMode))
{
    setHint (*this, "inside / outside", "Inside: the space and echo are part of the sound. Outside: they go only to the Send output, for your own reverb");
}

void SpaceModeSwitch::tick()
{
    const int m = param->getValue() > 0.5f ? 1 : 0;
    if (m != shown) { shown = m; repaint(); }
}

void SpaceModeSwitch::paint (juce::Graphics& g)
{
    // Two cells, the chosen one lit.
    const int m = param->getValue() > 0.5f ? 1 : 0;
    const float half = (float) getWidth() * 0.5f, h = (float) getHeight();
    const char* words[2] = { "inside", "outside" };
    for (int i = 0; i < 2; ++i)
    {
        const bool on = i == m;
        raised (g, { half * (float) i, 0.0f, half, h }, on, false);
        label (g, words[i], half * (float) i + half * 0.5f, h * 0.5f - 6.0f, on ? col::bg : col::dim (0.6f), 7.0f, juce::Justification::centred, half);
    }
}

void SpaceModeSwitch::mouseUp (const juce::MouseEvent& e)
{
    const int m = e.position.x < (float) getWidth() * 0.5f ? 0 : 1;
    param->beginChangeGesture();
    param->setValueNotifyingHost ((float) m);
    param->endChangeGesture();
    repaint();
}

/* ---------------- PlayButton ---------------- */

PlayButton::PlayButton (GrainFieldProcessor& p) : proc (p), param (p.apvts.getParameter (ids::play))
{
    setHint (*this, "play", "Start or stop the piece. With sync on, it plays when your song plays");
}

void PlayButton::tick()
{
    const bool on = param->getValue() > 0.5f, running = proc.engine.isRunning();
    if (on != shownOn || running != shownRunning) { shownOn = on; shownRunning = running; repaint(); }
}

void PlayButton::paint (juce::Graphics& g)
{
    // Lit while playing, with pause bars; a dark cell with a pixel triangle while stopped.
    const auto b = getLocalBounds().toFloat();
    const bool over = isMouseOver(), on = param->getValue() > 0.5f, running = proc.engine.isRunning();
    raised (g, b, on, over);
    const auto c = b.getCentre();
    if (on)
    {
        g.setColour (running ? col::bg : juce::Colour (0xff6A6A6A));   // waiting for your song: greyed
        g.fillRect (std::round (c.x) - 6.0f, std::round (c.y) - 9.0f, 5.0f, 18.0f);
        g.fillRect (std::round (c.x) + 1.0f, std::round (c.y) - 9.0f, 5.0f, 18.0f);
    }
    else
    {
        g.setColour (over ? col::bright : col::ink);
        for (int k = 0; k < 8; ++k)
            g.fillRect (std::round (c.x) - 5.0f + (float) k * 1.5f, std::round (c.y) - 9.0f + (float) k, 2.0f, 18.0f - 2.0f * (float) k);
    }
}

void PlayButton::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
    param->endChangeGesture();
    repaint();
}
/* ---------------- TakeChip ---------------- */

void TakeChip::tick()
{
    juce::String st;
    st << (int) proc.isRecording() << (int) proc.isRecordingWaiting() << (int) proc.getRecordedSeconds() << proc.getLastTake().getFileName()
       << (proc.isRecording() ? (int) (juce::Time::getMillisecondCounter() / 400) : 0);
    if (st != shown) { shown = st; repaint(); }
}

void TakeChip::paint (juce::Graphics& g)
{
    const bool blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0, over = isMouseOverOrDragging();
    const float cy = (float) getHeight() * 0.5f;
    if (proc.isRecordingWaiting())
    {
        g.setColour (col::rec.withAlpha (blink ? 1.0f : 0.35f));
        g.drawRect (2.0f, cy - 4.0f, 8.0f, 8.0f, 2.0f);
        text (g, "waiting for your song", 18.0f, cy - 9.0f, col::rec, 12.0f);
    }
    else if (proc.isRecording())
    {
        g.setColour (col::rec.withAlpha (blink ? 1.0f : 0.45f));
        g.fillRect (2.0f, cy - 4.0f, 8.0f, 8.0f);
        text (g, "rec " + timeText (proc.getRecordedSeconds()), 18.0f, cy - 9.0f, col::rec, 12.0f);
    }
    else if (proc.getLastTake().existsAsFile())
    {
        // A grip of pixels, so it reads as something to pick up.
        dither (g, { 2.0f, cy - 6.0f, 8.0f, 12.0f }, 0.5f, over ? 1.0f : 0.6f);
        const auto t = "take " + timeText (proc.getLastTakeSeconds());
        text (g, t, 18.0f, cy - 9.0f, over ? col::bright : col::ink, 12.0f);
        text (g, "drag to a track", 18.0f + textWidth (t, 12.0f) + 12.0f, cy - 8.0f, col::dim (0.45f), 10.0f);
    }
}

void TakeChip::mouseDrag (const juce::MouseEvent& e)
{
    const auto take = proc.getLastTake();
    if (draggingOut || proc.isRecording() || ! take.existsAsFile() || e.getDistanceFromDragStart() < 5) return;
    draggingOut = true;
    // Hands the file to whatever it's dropped on: an audio track in your song, the desktop, a folder.
    juce::Component::SafePointer<TakeChip> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ take.getFullPathName() }, false, this,
                                                                 [safe] { if (safe != nullptr) safe->draggingOut = false; });
}

void TakeChip::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = draggingOut || e.getDistanceFromDragStart() >= 5;
    draggingOut = false;
    if (wasDrag || proc.isRecording()) return;
    const auto take = proc.getLastTake();
    if (! take.existsAsFile()) return;
    juce::PopupMenu m;
    m.addSectionHeader (take.getFileNameWithoutExtension().toLowerCase());
    m.addItem ("show the file", [take] { take.revealToUser(); });
    m.addItem ("make it the root sound", [this, take] { proc.setRootFile (take); });
    m.addItem ("open the recordings folder", [] { auto f = GrainFieldProcessor::getRecordingsFolder(); f.createDirectory(); f.revealToUser(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

std::pair<juce::String, juce::String> TakeChip::hintAt (juce::Point<int>)
{
    if (proc.isRecordingWaiting()) return { "ready to record", "Sync is on, so the recording starts when your song does" };
    if (proc.isRecording()) return { "recording", "Everything you hear is being written to a file. Click the record button again to finish" };
    return { "take " + timeText (proc.getLastTakeSeconds()), "Your last recording. Drag it onto an audio track in your song, or click for more. It's kept in Music/Grain Field/Recordings" };
}

/* ---------------- StageLabel ---------------- */

StageLabel::StageLabel (GrainFieldProcessor& p, int st, const juce::String& w, bool r) : proc (p), stage (st), word (w), right (r)
{
    static const char* const what[3] = { "the grains", "the drones", "the bed underneath" };
    setHint (*this, word, juce::String ("Solo to hear only ") + what[st] + ", mute to take them out. What's left is what the other parts make");
}

void StageLabel::layout (float& xw, float& xs, float& xm) const
{
    // Two cells, solo and mute, filling the component.
    xw = 0.0f;
    xs = 0.0f;
    xm = (float) getWidth() * 0.5f;
}

int StageLabel::zoneAt (float x) const
{
    return x < (float) getWidth() * 0.5f ? onSolo : onMute;
}

bool StageLabel::hitTest (int, int)
{
    return true;
}

void StageLabel::tick()
{
    const int st = proc.engine.getSolo() * 4 + (proc.engine.isMuted (stage) ? 1 : 0);
    if (st != shown) { shown = st; repaint(); }
}

void StageLabel::paint (juce::Graphics& g)
{
    const int solo = proc.engine.getSolo();
    const bool muted = proc.engine.isMuted (stage), soloed = solo == stage;
    const float half = (float) getWidth() * 0.5f, h = (float) getHeight();
    raised (g, { 0.0f, 0.0f, half, h }, soloed, hover == onSolo);
    raised (g, { half, 0.0f, half, h }, muted, hover == onMute);
    label (g, "solo", half * 0.5f, h * 0.5f - 6.0f, soloed ? col::bg : col::dim (hover == onSolo ? 1.0f : 0.6f), 7.0f, juce::Justification::centred, half);
    label (g, "mute", half * 1.5f, h * 0.5f - 6.0f, muted ? col::bg : col::dim (hover == onMute ? 1.0f : 0.6f), 7.0f, juce::Justification::centred, half);
    juce::ignoreUnused (right);
}

void StageLabel::mouseMove (const juce::MouseEvent& e)
{
    const int z = zoneAt (e.position.x);
    if (z != hover) { hover = z; repaint(); }
    setMouseCursor (z == onSolo || z == onMute ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void StageLabel::mouseUp (const juce::MouseEvent& e)
{
    const int z = zoneAt (e.position.x);
    if (z == onSolo) proc.engine.setSolo (proc.engine.getSolo() == stage ? -1 : stage);
    else if (z == onMute) proc.engine.setMuted (stage, ! proc.engine.isMuted (stage));
    if (auto* top = getTopLevelComponent()) top->repaint();
}
} // namespace gf::ui
