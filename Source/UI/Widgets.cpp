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

Knob::Knob (GrainFieldProcessor& p, int sp, const juce::String& l) : ParamControl (p, sp), label (l) {}
Knob::Knob (GrainFieldProcessor& p, const juce::String& id, const juce::String& l) : ParamControl (p, id), label (l) {}

static juce::Point<float> onRing (juce::Point<float> c, float r, float norm)
{
    const float a = juce::MathConstants<float>::pi * (-0.75f + 1.5f * norm);
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

static void arc (juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness, juce::Colour colour)
{
    if (to < from) std::swap (from, to);
    if (to - from < 1.0e-4f) return;
    const float pi = juce::MathConstants<float>::pi;
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, pi * (-0.75f + 1.5f * from), pi * (-0.75f + 1.5f * to), true);
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Knob::paint (juce::Graphics& g)
{
    const juce::Point<float> c ((float) getWidth() * 0.5f, 16.0f);
    const float r = 12.0f, n = getNorm(), d = getDisplayNorm();
    const bool ed = editingNext();
    const juce::Colour main = ed ? col::dim (0.6f) : (sceneParam >= 0 && swellMoves (sceneParam) ? col::accent : col::ink);

    arc (g, c, r, 0.0f, 1.0f, 1.0f, col::dim (hover || dragging ? 0.28f : 0.18f));
    arc (g, c, r, 0.0f, n, 1.5f, main);

    if (! ed && std::abs (d - n) > 0.015f)
    {
        // Where the sound really is.
        arc (g, c, r + 3.5f, n, d, 1.0f, main.withAlpha (0.35f));
        const auto pd = onRing (c, r + 3.5f, d);
        g.setColour (main.withAlpha (0.8f));
        g.drawEllipse (pd.x - 1.8f, pd.y - 1.8f, 3.6f, 3.6f, 1.0f);
    }

    const auto pn = onRing (c, r, n);
    if (ed)
    {
        g.setColour (main);
        g.drawEllipse (pn.x - 2.0f, pn.y - 2.0f, 4.0f, 4.0f, 1.0f);
    }
    else dot (g, pn.x, pn.y, 2.0f, main);

    const float w = (float) getWidth();
    text (g, label, w * 0.5f, 35.0f, ed ? col::dim (0.75f) : col::ink, 10.0f, juce::Justification::centred, w);
    text (g, valueText(), w * 0.5f, 49.0f, col::dim (hover || dragging ? 0.75f : 0.4f), 10.0f, juce::Justification::centred, w);
}

/* ---------------- HairSlider ---------------- */

HairSlider::HairSlider (GrainFieldProcessor& p, int sp, const juce::String& c) : ParamControl (p, sp), caption (c) {}

HairSlider::HairSlider (GrainFieldProcessor& p, const juce::String& id, const juce::String& c) : ParamControl (p, id), caption (c)
{
    setHint (*this, caption, "How loud Grain Field is. The last thing before the output");
}

juce::Rectangle<float> HairSlider::track() const
{
    const float x0 = caption.isEmpty() ? 4.0f : textWidth (caption, 11.0f) + 10.0f;
    return { x0, (float) getHeight() * 0.5f, (float) getWidth() - x0 - 4.0f, 1.0f };
}

float HairSlider::dragToNorm (const juce::MouseEvent& e, float)
{
    const auto t = track();
    return (e.position.x - t.getX()) / t.getWidth();
}

void HairSlider::paint (juce::Graphics& g)
{
    const auto t = track();
    const float n = getNorm();
    const bool ed = editingNext();
    if (caption.isNotEmpty())
        text (g, caption, 0.0f, t.getY() - 7.5f, col::dim (hover || dragging ? 0.7f : 0.4f), 11.0f);
    g.setColour (col::dim (0.2f));
    g.fillRect (t);
    const juce::Colour main = ed ? col::dim (0.6f) : col::ink;
    g.setColour (main);
    g.fillRect (t.withWidth (t.getWidth() * n));
    const float x = t.getX() + t.getWidth() * n;
    if (ed)
        g.drawEllipse (x - 3.0f, t.getY() - 3.0f, 6.0f, 6.0f, 1.0f);
    else
        dot (g, x, t.getY() + 0.5f, hover || dragging ? 4.0f : 3.5f, main);
    if (hover || dragging)
        text (g, valueText(), t.getRight(), t.getY() + 4.0f, col::dim (0.5f), 9.0f, juce::Justification::centredRight, 80.0f);
}

/* ---------------- DragNumber ---------------- */

DragNumber::DragNumber (GrainFieldProcessor& p, int sp, const juce::String& w) : ParamControl (p, sp), word (w)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void DragNumber::paint (juce::Graphics& g)
{
    const bool active = hover || dragging;
    const juce::String s = word + " " + juce::String (juce::roundToInt (getNorm() * 100.0f));
    text (g, s, (float) getWidth() * 0.5f, 0.0f, editingNext() ? col::dim (0.6f) : col::dim (active ? 0.9f : 0.45f), 10.0f,
          juce::Justification::centred, (float) getWidth());
    if (active)
    {
        g.setColour (col::dim (0.3f));
        g.fillRect ((float) getWidth() * 0.5f - 12.0f, (float) getHeight() - 2.0f, 24.0f, 1.0f);
    }
}

/* ---------------- SwellLine ---------------- */

SwellLine::SwellLine (GrainFieldProcessor& p) : ParamControl (p, pSwell) {}

float SwellLine::dragToNorm (const juce::MouseEvent& e, float)
{
    return (e.position.x - lineX) / lineW();
}

SwellLine::Zone SwellLine::zoneAt (juce::Point<float> pt) const
{
    const float y = lineYpos;
    if (pt.y >= y + 12.0f && pt.y < y + 32.0f)
    {
        if (pt.x >= lineX - 4.0f && pt.x < lineX + 44.0f) return onQuiet;
        if (pt.x >= lineX + lineW() - 40.0f && pt.x < lineX + lineW() + 6.0f) return onWall;
    }
    if (std::abs (pt.y - y) <= 12.0f && pt.x >= lineX - 8.0f && pt.x <= lineX + lineW() + 8.0f) return onLine;
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
    return { "swell", "From almost nothing to a full wall. Drag the handle to move it yourself, or click a point on the line and it travels there" + over };
}

void SwellLine::tick()
{
    ParamControl::tick();
    const auto& sc = proc.getScenes();
    std::array<float, numScenes> m {};
    for (int i = 0; i < numScenes; ++i)
        m[(size_t) i] = sc.slots[(size_t) i].used ? sc.slots[(size_t) i].norm[(size_t) pSwell] : -1.0f;
    const int f = proc.isSwellFading() ? (int) (proc.getSwellFadeRemaining() * 4.0) : -1;
    if (m != marks || f != shownFade) { marks = m; shownFade = f; repaint(); }
}

void SwellLine::paint (juce::Graphics& g)
{
    const float y = lineYpos, n = getNorm(), d = getDisplayNorm();
    const bool ed = editingNext();
    const juce::Colour main = ed ? col::dim (0.6f) : col::accent;

    text (g, "swell", 0.0f, y - 7.0f, main, 11.0f);
    text (g, "quiet", lineX, y + 15.0f, hoverZone == onQuiet && ! ed ? col::accent : col::dim (0.3f), 10.0f);
    text (g, "wall", lineX + lineW(), y + 15.0f, hoverZone == onWall && ! ed ? col::accent : col::dim (0.3f), 10.0f, juce::Justification::centredRight, 60.0f);

    if (! ed)
    {
        // A light on the line where the sound is: faint when quiet, warm at the wall.
        const float gx = lineX + lineW() * d, r = 24.0f;
        g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.05f + 0.2f * d), gx, y, col::accent.withAlpha (0.0f), gx + r, y, true));
        g.fillEllipse (gx - r, y - r, r * 2.0f, r * 2.0f);
    }
    g.setColour (col::dim (hover || dragging ? 0.22f : 0.15f));
    g.fillRect (lineX, y, lineW(), 1.0f);
    g.setColour (main);
    g.fillRect (lineX, y - 0.5f, lineW() * n, 2.0f);

    // Where Swell sits in the other scenes, so you can see where this one is.
    const int active = proc.getActivePart(), next = ed ? proc.getNextPart() : -1;
    for (int i = 0; i < numScenes; ++i)
    {
        const float m = marks[(size_t) i];
        if (m < 0.0f || i == (ed ? next : active)) continue;
        const float x = lineX + lineW() * m;
        dot (g, x, y + 0.5f, i == next ? 2.5f : 2.0f, col::accent.withAlpha (i == next ? 0.9f : 0.45f));
    }

    if (! ed && proc.isSwellFading())
    {
        // Travelling: where it's going, and how long is left.
        const float xt = lineX + lineW() * proc.getSwellFadeTarget(), xn = lineX + lineW() * n;
        const float dashes[] = { 2.0f, 3.0f };
        g.setColour (col::accent.withAlpha (0.55f));
        g.drawDashedLine ({ xn, y + 0.5f, xt, y + 0.5f }, dashes, 2, 1.0f);
        g.drawEllipse (xt - 4.0f, y - 3.5f, 8.0f, 8.0f, 1.0f);
        text (g, timeText (proc.getSwellFadeRemaining()), xt, y - 22.0f, col::accent, 9.0f, juce::Justification::centred, 60.0f);
    }
    else if (! ed && std::abs (d - n) > 0.01f)   // a blend on its way
    {
        const float xd = lineX + lineW() * d;
        g.setColour (col::accent.withAlpha (0.6f));
        g.drawEllipse (xd - 3.0f, y - 2.5f, 6.0f, 6.0f, 1.0f);
    }

    const float x = lineX + lineW() * n;
    g.setColour (juce::Colour (0xff101010));
    g.fillEllipse (x - 6.0f, y - 5.5f, 12.0f, 12.0f);
    g.setColour (main);
    g.drawEllipse (x - 6.0f, y - 5.5f, 12.0f, 12.0f, 2.0f);

    g.setColour (main);
    g.setFont (mono (13.0f));
    g.drawText (juce::String (juce::roundToInt (n * 100.0f)), juce::Rectangle<float> (lineX + lineW() + 20.0f, y - 9.0f, 40.0f, 18.0f),
                juce::Justification::centredLeft);
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
    const bool on = param->getValue() > 0.5f, over = isMouseOver();
    juce::Colour c = on ? (accentWhenOn ? col::accent : col::ink) : col::dim (over ? 0.75f : 0.45f);
    float x = 0.0f;
    if (on && accentWhenOn) { dot (g, 4.0f, (float) getHeight() * 0.5f, 3.0f, c); x = 14.0f; }
    text (g, word, x, (float) getHeight() * 0.5f - 7.5f, c, 11.0f);
    if (on && ! accentWhenOn)
    {
        g.setColour (c);
        g.fillRect (x, (float) getHeight() * 0.5f + 8.0f, textWidth (word), 1.0f);
    }
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
    const int m = param->getValue() > 0.5f ? 1 : 0;
    const float w0 = textWidth ("inside", 10.0f);
    const float xs[2] = { 0.0f, w0 + 14.0f };
    const char* words[2] = { "inside", "outside" };
    for (int i = 0; i < 2; ++i)
    {
        const bool on = i == m;
        text (g, words[i], xs[i], 0.0f, on ? col::ink : col::dim (0.35f), 10.0f);
        if (on)
        {
            g.setColour (col::ink);
            g.fillRect (xs[i], 16.0f, textWidth (words[i], 10.0f), 1.0f);
        }
    }
}

void SpaceModeSwitch::mouseUp (const juce::MouseEvent& e)
{
    const int m = e.position.x < textWidth ("inside", 10.0f) + 7.0f ? 0 : 1;
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
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver(), on = param->getValue() > 0.5f, running = proc.engine.isRunning();
    if (over)
    {
        g.setColour (col::dim (0.08f));
        g.fillEllipse (b);
    }
    g.setColour (over ? col::ink : col::dim (0.5f));
    g.drawEllipse (b, 1.0f);
    const auto c = b.getCentre();
    if (on)
    {
        // Pause bars; dim while waiting for your song to start.
        g.setColour (running ? col::ink : col::dim (0.45f));
        g.fillRect (c.x - 4.0f, c.y - 5.5f, 2.0f, 11.0f);
        g.fillRect (c.x + 2.0f, c.y - 5.5f, 2.0f, 11.0f);
    }
    else
    {
        juce::Path p;
        p.addTriangle (c.x - 3.0f, c.y - 6.0f, c.x + 7.0f, c.y, c.x - 3.0f, c.y + 6.0f);
        g.setColour (col::ink);
        g.fillPath (p);
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
    if (proc.isRecordingWaiting())
    {
        g.setColour (col::accent.withAlpha (blink ? 1.0f : 0.35f));
        g.drawEllipse (1.0f, 5.0f, 7.0f, 7.0f, 1.0f);
        text (g, "waiting for your song", 15.0f, 1.0f, col::accent, 11.0f);
    }
    else if (proc.isRecording())
    {
        dot (g, 4.5f, 8.5f, 3.5f, col::accent.withAlpha (blink ? 1.0f : 0.45f));
        text (g, "recording " + timeText (proc.getRecordedSeconds()), 15.0f, 1.0f, col::accent, 11.0f);
    }
    else if (proc.getLastTake().existsAsFile())
    {
        // A grip, so it reads as something to pick up.
        g.setColour (col::dim (over ? 0.9f : 0.5f));
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 2; ++c) g.fillRect (1.0f + 4.0f * (float) c, 4.0f + 4.0f * (float) r, 1.5f, 1.5f);
        const auto label = "take " + timeText (proc.getLastTakeSeconds());
        text (g, label, 15.0f, 1.0f, over ? col::accent : col::ink, 11.0f);
        text (g, "drag to a track", 15.0f + textWidth (label) + 12.0f, 1.5f, col::dim (0.32f), 10.0f);
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
    const float ww = textWidth (word), sw = textWidth ("solo", 10.0f), mw = textWidth ("mute", 10.0f);
    if (right)
    {
        const float r = (float) getWidth();
        xw = r - ww;
        xm = xw - 14.0f - mw;
        xs = xm - 10.0f - sw;
    }
    else
    {
        xw = 0.0f;
        xs = ww + 14.0f;
        xm = xs + sw + 10.0f;
    }
}

int StageLabel::zoneAt (float x) const
{
    float xw, xs, xm;
    layout (xw, xs, xm);
    if (x >= xs - 4.0f && x < xs + textWidth ("solo", 10.0f) + 5.0f) return onSolo;
    if (x >= xm - 4.0f && x < xm + textWidth ("mute", 10.0f) + 5.0f) return onMute;
    if (x >= xw && x < xw + textWidth (word)) return onWord;
    return -1;
}

bool StageLabel::hitTest (int x, int)
{
    float xw, xs, xm;
    layout (xw, xs, xm);
    return right ? (float) x >= xs - 4.0f : (float) x < xm + textWidth ("mute", 10.0f) + 5.0f;
}

void StageLabel::tick()
{
    const int st = proc.engine.getSolo() * 4 + (proc.engine.isMuted (stage) ? 1 : 0);
    if (st != shown) { shown = st; repaint(); }
}

void StageLabel::paint (juce::Graphics& g)
{
    float xw, xs, xm;
    layout (xw, xs, xm);
    const int solo = proc.engine.getSolo();
    const bool muted = proc.engine.isMuted (stage), soloed = solo == stage, audible = proc.engine.isAudible (stage);
    const float y = 4.0f;

    text (g, word, xw, y, audible ? col::dim (soloed ? 0.9f : 0.4f) : col::dim (0.18f), 11.0f);
    if (! audible)
    {
        g.setColour (col::dim (0.3f));
        g.fillRect (xw, y + 8.0f, textWidth (word), 1.0f);
    }
    text (g, "solo", xs, y + 1.0f, soloed ? col::accent : col::dim (hover == onSolo ? 0.8f : 0.22f), 10.0f);
    text (g, "mute", xm, y + 1.0f, muted ? col::accent : col::dim (hover == onMute ? 0.8f : 0.22f), 10.0f);
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
