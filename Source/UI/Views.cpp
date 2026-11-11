#include "Views.h"

namespace gf::ui
{
static juce::String lower (const juce::String& s) { return s.toLowerCase(); }

/* ---------------- InlineEditor ---------------- */

InlineEditor::InlineEditor()
{
    setFont (mono (11.0f));
    setIndents (0, 2);
    setBorder ({});
    setJustification (juce::Justification::centredLeft);
    setInputRestrictions (32);
    onReturnKey = [this] { finish (true); };
    onEscapeKey = [this] { finish (false); };
    onFocusLost = [this] { finish (true); };
}

void InlineEditor::start (juce::Component& parent, juce::Rectangle<int> area, const juce::String& initial,
                          std::function<void (const juce::String&)> done)
{
    onDone = std::move (done);
    if (getParentComponent() != &parent) parent.addChildComponent (this);
    setBounds (area);
    setText (initial, false);
    selectAll();
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void InlineEditor::finish (bool commit)
{
    if (! isVisible()) return;
    setVisible (false);
    auto cb = std::move (onDone);
    onDone = nullptr;
    const auto t = getText().trim();
    if (commit && cb && t.isNotEmpty()) cb (t);
}

/* ---------------- ScenesView ---------------- */

ScenesView::ScenesView (GrainFieldProcessor& p) : proc (p) {}

juce::String ScenesView::sceneName (int part) const
{
    const auto& s = proc.getScenes().slots[(size_t) part];
    return s.name.isNotEmpty() ? lower (s.name) : "scene " + juce::String (part + 1);
}

void ScenesView::layout()
{
    // The label, then one chip per scene, then the chip that keeps what you hear as a new one.
    chips.clear();
    float x = textWidth ("scenes") + 22.0f;
    for (int i = 0; i < numScenes; ++i)
    {
        if (! proc.getScenes().slots[(size_t) i].used) continue;
        const float w = textWidth (sceneName (i)) + 26.0f;
        chips.push_back ({ i, { x, 0.0f, w, chipH } });
        x += w + 8.0f;
    }
    addRect = proc.getScenes().firstFree() >= 0 ? juce::Rectangle<float> (x, 0.0f, textWidth ("+ keep this as a scene") + 26.0f, chipH)
                                                : juce::Rectangle<float>();
}

juce::Rectangle<float> ScenesView::chipRect (int part) const
{
    for (auto& c : chips) if (c.part == part) return c.r;
    return {};
}

int ScenesView::itemAt (juce::Point<float> p) const
{
    for (auto& c : chips) if (c.r.contains (p)) return c.part;
    if (! addRect.isEmpty() && addRect.contains (p)) return onAdd;
    return onNothing;
}

void ScenesView::tick()
{
    // Redraw when anything shown changes: which scenes there are, their names, which one you're in, a blend.
    juce::String now;
    for (int i = 0; i < numScenes; ++i)
        if (proc.getScenes().slots[(size_t) i].used) now << i << sceneName (i) << ";";
    now << proc.getActivePart() << ":" << juce::roundToInt (proc.engine.getBlendProgress() * 100.0f);
    if (now != shown) { shown = now; layout(); repaint(); }
}

void ScenesView::paint (juce::Graphics& g)
{
    if (chips.empty() && addRect.isEmpty()) layout();
    text (g, "scenes", 0.0f, chipH * 0.5f - 8.0f, col::dim (0.4f), 11.0f);
    const int active = proc.getActivePart();
    const float pr = proc.engine.getBlendProgress();
    for (auto& c : chips)
    {
        const bool on = c.part == active, over = hover == c.part;
        auto r = c.r.reduced (0.5f);
        if (on)
        {
            g.setColour (col::accent.withAlpha (0.08f));
            g.fillRect (r);
            // While it blends in, the chip fills from the left.
            if (pr >= 0.0f && pr < 1.0f)
            {
                g.setColour (col::accent.withAlpha (0.18f));
                g.fillRect (r.withWidth (r.getWidth() * pr));
            }
        }
        g.setColour (on ? col::accent : col::dim (over ? 0.5f : 0.18f));
        g.drawRect (r, 1.0f);
        text (g, sceneName (c.part), c.r.getX() + 13.0f, c.r.getCentreY() - 8.0f, on ? col::accent : (over ? col::ink : col::dim (0.75f)), 11.0f);
    }
    if (! addRect.isEmpty())
    {
        const bool over = hover == onAdd;
        g.setColour (col::dim (over ? 0.5f : 0.2f));
        const float dash[] = { 2.0f, 3.0f };
        juce::Path outline, dashed;
        outline.addRectangle (addRect.reduced (0.5f));
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dash, 2);
        g.fillPath (dashed);
        text (g, "+ keep this as a scene", addRect.getX() + 13.0f, addRect.getCentreY() - 8.0f, over ? col::ink : col::dim (0.5f), 11.0f);
    }
}

void ScenesView::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void ScenesView::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown()) return;
    const int i = itemAt (e.position);
    if (i == onAdd) { proc.newPart(); return; }
    if (i < 0) return;
    if (e.mods.isPopupMenu()) { showMenu (i); return; }
    if (i != proc.getActivePart()) proc.goToPart (i);
}

void ScenesView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = itemAt (e.position);
    if (i >= 0) rename (i);
}

std::pair<juce::String, juce::String> ScenesView::hintAt (juce::Point<int> p)
{
    const int i = itemAt (p.toFloat());
    if (i == onAdd) return { "keep this as a scene", "Keeps everything you hear (sounds, notes, chords, every knob) as a new scene, and puts you in it" };
    if (i >= 0)
        return { sceneName (i), i == proc.getActivePart()
                                    ? "The scene you're in. What you change is kept with it. Double-click to rename; right-click to rename or remove"
                                    : "Click to blend into it over the blend time. Your DAW can automate the Scene parameter to do the same. Double-click to rename; right-click to remove" };
    return { "scenes", "Whole sounds you keep and blend between. Nothing moves on by itself: you click, or your DAW decides when" };
}

void ScenesView::rename (int part)
{
    const auto r = chipRect (part);
    if (r.isEmpty()) return;
    const juce::Rectangle<int> area ((int) r.getX(), (int) r.getY(), (int) juce::jmax (110.0f, r.getWidth()), (int) chipH);
    renamer.start (*this, area, proc.getScenes().slots[(size_t) part].name, [this, part] (const juce::String& name) { proc.renamePart (part, name); });
}

void ScenesView::showMenu (int part)
{
    juce::PopupMenu m;
    const int active = proc.getActivePart();
    m.addSectionHeader (sceneName (part));
    if (part != active) m.addItem ("blend into it", [this, part] { proc.goToPart (part); });
    m.addItem (juce::String::fromUTF8 ("rename…"), [this, part] { rename (part); });
    if (part != active)
    {
        m.addSeparator();
        m.addItem ("remove", [this, part] { proc.deletePart (part); });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (chipRect (part).toNearestInt())));
}

/* ---------------- HarmonyView ---------------- */

static juce::String noteLower (int pc) { return juce::String (keyNames[((pc % 12) + 12) % 12]).toLowerCase(); }

// The notes a chord is made of, root first.
static juce::String chordNotes (int key, const ChordStep& c)
{
    const auto& v = chordVoicing (c.type);
    std::array<bool, 12> has {};
    for (int iv : v.drone) has[(size_t) (((iv % 12) + 12) % 12)] = true;
    for (int iv : v.pad) has[(size_t) (((iv % 12) + 12) % 12)] = true;
    juce::String out;
    for (int iv = 0; iv < 12; ++iv)
        if (has[(size_t) iv]) out << (out.isEmpty() ? "" : " ") << noteLower (key + c.root + iv);
    return out;
}

// The same, as it will sound: the lowest note with its octave first.
static juce::String chordNotesFromBass (int key, const ChordStep& c)
{
    const int bass = chordBassMidi (key, c);
    const auto& v = chordVoicing (c.type);
    std::array<bool, 12> has {};
    for (int iv : v.drone) has[(size_t) (((iv % 12) + 12) % 12)] = true;
    for (int iv : v.pad) has[(size_t) (((iv % 12) + 12) % 12)] = true;
    juce::String out = noteNameWithOctave (bass);
    for (int iv = 1; iv < 12; ++iv)
        if (has[(size_t) iv]) out << " " << noteLower (bass + iv);
    return out;
}

static juce::String octText (int oct) { return oct > 0 ? "+" + juce::String (oct) : juce::String (oct); }

// The small editor for one slot: pick its note, its kind, how many bars it lasts and how high it sits.
class ChordEditor : public juce::Component
{
public:
    ChordEditor (HarmonyView& h, int idx) : owner (h), index (idx) { setSize (300, 192); }

    void paint (juce::Graphics& g) override
    {
        const auto p = owner.shownProg();
        const int key = owner.keyShown();
        const bool exists = index >= 0 && index < p.count;
        g.fillAll (juce::Colour (0xff141414));
        const ChordStep st = exists ? p.steps[(size_t) index] : ChordStep { -1, 1, 4 };
        text (g, exists ? chordName (key, st) : juce::String ("new chord: pick its note"), 8.0f, 4.0f, exists ? col::ink : col::accent, 12.0f);
        row (g, "note", 30.0f);
        for (int n = 0; n < 12; ++n)
        {
            const bool on = exists && (key + st.root) % 12 == n;
            cell (g, noteCell (n), noteLower (n), on, true);
        }
        row (g, "kind", 66.0f);
        for (int t = 0; t < 6; ++t)
            cell (g, kindCell (t), juce::String (chordNames[t]).toLowerCase(), exists && st.type == t, exists);
        row (g, "bars", 102.0f);
        for (int b = 0; b < 5; ++b)
            cell (g, barsCell (b), juce::String (barChoices[b]), exists && st.bars == barChoices[b], exists);
        // Where the chord sits: lower is darker, higher is brighter.
        row (g, "octave", 138.0f);
        for (int o = minChordOct; o <= maxChordOct; ++o)
            cell (g, octCell (o), octText (o), exists && st.oct == o, exists);
        if (exists)
        {
            text (g, st.oct < 0 ? "darker" : (st.oct == 0 ? "as it is" : (st.oct == 1 ? "brighter" : "bright, thin")), 222.0f, 140.0f, col::dim (0.4f), 9.5f);
            text (g, chordNotesFromBass (key, st), 8.0f, 170.0f, col::dim (0.5f), 10.0f);
            if (p.count > 1) text (g, "remove", 292.0f, 170.0f, col::dim (over == removeZone ? 0.9f : 0.4f), 10.0f, juce::Justification::centredRight, 80.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { over = removeRect().contains (e.position) ? removeZone : 0; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto p = owner.shownProg();
        const int key = owner.keyShown();
        const int playing = owner.playingIndex();
        const bool exists = index >= 0 && index < p.count;
        for (int n = 0; n < 12; ++n)
            if (noteCell (n).contains (e.position))
            {
                const int root = ((n - key) % 12 + 12) % 12;
                if (! exists)
                {
                    if (p.count >= Progression::maxSteps) return;
                    index = p.count;
                    p.steps[(size_t) p.count++] = { root, 1, 4 };   // now it's on
                }
                else p.steps[(size_t) index].root = root;
                owner.apply (p, playing);
                repaint();
                return;
            }
        if (! exists) return;
        for (int t = 0; t < 6; ++t)
            if (kindCell (t).contains (e.position)) { p.steps[(size_t) index].type = t; owner.apply (p, playing); repaint(); return; }
        for (int b = 0; b < 5; ++b)
            if (barsCell (b).contains (e.position)) { p.steps[(size_t) index].bars = barChoices[b]; owner.apply (p, playing); repaint(); return; }
        for (int o = minChordOct; o <= maxChordOct; ++o)
            if (octCell (o).contains (e.position)) { p.steps[(size_t) index].oct = o; owner.apply (p, playing); repaint(); return; }
        if (p.count > 1 && removeRect().contains (e.position))
        {
            for (int k = index; k < p.count - 1; ++k) p.steps[(size_t) k] = p.steps[(size_t) k + 1];
            --p.count;
            int keep = playing;
            if (playing > index) keep = playing - 1;
            else if (playing == index) keep = juce::jmin (index, p.count - 1);
            owner.apply (p, keep);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
        }
    }

private:
    static constexpr int barChoices[5] = { 1, 2, 4, 8, 16 };
    enum { removeZone = 1 };
    static void row (juce::Graphics& g, const char* label, float y) { text (g, label, 6.0f, y + 4.0f, col::dim (0.35f), 9.0f); }
    void cell (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, bool enabled)
    {
        if (on) { g.setColour (col::accent); g.fillRect (r); }
        else { g.setColour (col::dim (enabled ? 0.18f : 0.07f)); g.drawRect (r, 1.0f); }
        text (g, t, r.getCentreX(), r.getY() + 3.0f, on ? col::bg : col::dim (enabled ? 0.85f : 0.25f), 9.5f, juce::Justification::centred, r.getWidth());
    }
    juce::Rectangle<float> noteCell (int n) const { return { 44.0f + 21.0f * (float) n, 28.0f, 19.0f, 20.0f }; }
    juce::Rectangle<float> kindCell (int t) const { return { 44.0f + 42.0f * (float) t, 64.0f, 40.0f, 20.0f }; }
    juce::Rectangle<float> barsCell (int b) const { return { 44.0f + 42.0f * (float) b, 100.0f, 40.0f, 20.0f }; }
    juce::Rectangle<float> octCell (int o) const { return { 44.0f + 42.0f * (float) (o - minChordOct), 136.0f, 40.0f, 20.0f }; }
    juce::Rectangle<float> removeRect() const { return { 230.0f, 166.0f, 66.0f, 20.0f }; }

    HarmonyView& owner;
    int index;
    int over = 0;
};

std::unique_ptr<juce::Component> makeChordEditor (HarmonyView& owner, int index) { return std::make_unique<ChordEditor> (owner, index); }

HarmonyView::HarmonyView (GrainFieldProcessor& p) : proc (p) {}

Progression HarmonyView::shownProg() const
{
    if (proc.isEditingNext()) return proc.getScenes().slots[(size_t) proc.getNextPart()].prog;
    return proc.getProgression();
}

int HarmonyView::keyShown() const
{
    auto* param = proc.apvts.getParameter (spec (hmKey).id);
    const float n = proc.isEditingNext() ? proc.getPartValue (proc.getNextPart(), hmKey) : param->getValue();
    return juce::jlimit (0, 11, juce::roundToInt (param->convertFrom0to1 (n)));
}

int HarmonyView::playingIndex() const
{
    if (proc.isEditingNext()) return -1;
    return juce::jlimit (0, juce::jmax (0, shownProg().count - 1), proc.engine.getStepIndex());
}

void HarmonyView::apply (const Progression& p, int playingNowAt)
{
    if (proc.isEditingNext()) proc.setPartProgression (proc.getNextPart(), p);
    else proc.setProgression (p, playingNowAt);
    repaint();
}

bool HarmonyView::staying() const { return proc.apvts.getRawParameterValue (ids::hold)->load() > 0.5f; }

void HarmonyView::setStaying (bool on)
{
    auto* param = proc.apvts.getParameter (ids::hold);
    param->beginChangeGesture();
    param->setValueNotifyingHost (on ? 1.0f : 0.0f);
    param->endChangeGesture();
    repaint();
}

void HarmonyView::tick()
{
    juce::String st;
    st << shownProg().toString() << "|" << proc.engine.getStepIndex() << "|" << keyShown() << "|" << proc.engine.getCue() << "|"
       << proc.engine.getCueWhen() << "|" << (int) staying() << "|" << (int) proc.isEditingNext() << "|"
       << proc.engine.getKeysChord() << "|"
       << juce::roundToInt (proc.engine.getStepProgress() * 200.0f) << "|" << (int) ((juce::Time::getMillisecondCounter() / 350) % 2);
    if (st != shownState) { shownState = st; repaint(); }
}

void HarmonyView::paint (juce::Graphics& g)
{
    const auto p = shownProg();
    const int key = keyShown();
    const bool ed = proc.isEditingNext();
    const int playing = playingIndex();
    const int cue = ed ? -1 : proc.engine.getCue();
    const int when = proc.engine.getCueWhen();

    // Header: the chords, and the root they're counted from. Whether they stay or loop is the loop button beside them.
    const juce::String title = ed ? "chords of " + lower (proc.getScenes().slots[(size_t) proc.getNextPart()].name) : juce::String ("chords");
    text (g, title, 0.0f, 0.0f, ed ? col::ink : col::dim (0.4f), 11.0f);
    // The chords are counted from the root, which sits at the top of the window.
    const float kx = keyNameX();
    text (g, "on", kx, 0.0f, col::dim (0.4f), 11.0f);
    text (g, noteLower (key), kx + 22.0f, 0.0f, hover == onKey ? col::accent : col::ink, 11.0f);

    // A chord played on a MIDI keyboard: its name, and what to do with it.
    juce::String keysName;
    float xKeysName = 0.0f, xKeep = 0.0f, xLetGo = 0.0f;
    const bool keysHold = keysLayout (keysName, xKeysName, xKeep, xLetGo);
    if (keysHold)
    {
        text (g, "keys", xKeysName - textWidth ("keys") - 10.0f, 0.0f, col::dim (0.4f), 11.0f);
        text (g, keysName, xKeysName, 0.0f, col::accent, 11.0f);
        text (g, "keep", xKeep, 0.0f, hover == onKeep ? col::accent : col::dim (0.75f), 11.0f);
        text (g, "let go", xLetGo, 0.0f, hover == onLetGo ? col::accent : col::dim (0.45f), 11.0f);
    }

    const bool blink = (juce::Time::getMillisecondCounter() / 350) % 2 == 0;
    const int firstEmpty = p.count;
    for (int i = 0; i < Progression::maxSteps; ++i)
    {
        auto r = slotRect (i);
        const bool used = i < p.count;
        const bool isDragged = dragging && i == pressed;
        if (! used)
        {
            // Off until you set it up.
            g.setColour (col::dim (i == firstEmpty && hover == i ? 0.4f : 0.12f));
            const float dash[] = { 2.0f, 3.0f };
            juce::Path outline;
            outline.addRectangle (r.reduced (0.5f));
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dash, 2);
            g.fillPath (dashed);
            if (i == firstEmpty)
                text (g, "+", r.getCentreX(), r.getCentreY() - 8.0f, col::dim (hover == i ? 0.9f : 0.35f), 12.0f, juce::Justification::centred, 20.0f);
            continue;
        }
        if (isDragged) { g.setColour (col::dim (0.06f)); g.fillRect (r); continue; }

        const auto& st = p.steps[(size_t) i];
        const bool on = i == playing, queued = i == cue;
        const bool over = hover == i || hover == onEditBase + i || hover == onNowBase + i || hover == onLoopBase + i;
        const bool looping = ! ed && ! staying(), out = looping && ! st.loop;   // out of the loop: it waits until you queue it
        juce::Colour border = on ? (keysHold ? col::accent.withAlpha (0.4f) : col::accent) : (queued ? col::accent.withAlpha (blink ? 0.9f : 0.4f) : col::dim (over ? 0.45f : (out ? 0.1f : 0.2f)));
        if (on && ! keysHold)
        {
            g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.10f), 0.0f, r.getBottom(), col::accent.withAlpha (0.0f), 0.0f, r.getY(), false));
            g.fillRect (r);
        }
        g.setColour (border);
        g.drawRect (r, on ? 1.5f : 1.0f);
        const auto cname = chordName (key, st);
        text (g, cname, r.getX() + 6.0f, r.getY() + 5.0f, on ? col::ink : col::dim (over || queued ? 0.9f : (out ? 0.38f : 0.7f)), 10.5f,
              juce::Justification::centredLeft, r.getWidth() - 22.0f);
        // Moved up or down from where it would sit: say so beside the name.
        if (st.oct != 0 && textWidth (cname, 10.5f) + 32.0f < r.getWidth() - 14.0f)
            text (g, octText (st.oct), r.getX() + 6.0f + textWidth (cname, 10.5f) + 6.0f, r.getY() + 5.5f, col::accent.withAlpha (out ? 0.4f : 0.85f), 9.5f);
        text (g, chordNotesFromBass (key, st), r.getX() + 6.0f, r.getY() + 20.0f, col::dim (out ? 0.25f : 0.45f), 9.0f, juce::Justification::centredLeft, r.getWidth() - 10.0f);
        if (looping)
        {
            // In the loop: a small dot. Out of it: a ring.
            const float lx = r.getRight() - 9.0f, ly = r.getY() + 10.0f;
            const juce::Colour lc = hover == onLoopBase + i ? col::accent : col::dim (st.loop ? 0.6f : 0.3f);
            if (st.loop) dot (g, lx, ly, 2.2f, lc);
            else { g.setColour (lc); g.drawEllipse (lx - 2.5f, ly - 2.5f, 5.0f, 5.0f, 1.0f); }
        }
        const juce::String bars = juce::String (st.bars) + (st.bars == 1 ? " bar" : " bars");
        juce::String tag;
        if (queued) tag = when == 0 ? "next" : (when == 1 ? "at bar" : "now");
        text (g, tag.isNotEmpty() ? tag : bars, r.getX() + 6.0f, r.getY() + 34.0f, queued ? col::accent : col::dim (0.3f), 9.0f);
        if (over && ! dragging)
        {
            pencil (g, r.getRight() - 15.0f, r.getY() + 36.0f, 10.0f, hover == onEditBase + i ? col::accent : col::dim (0.5f));
            if (! ed && ! on)
                text (g, "now", r.getRight() - 24.0f, r.getY() + 34.0f, hover == onNowBase + i ? col::accent : col::dim (0.5f), 9.0f,
                      juce::Justification::centredRight, 30.0f);
        }
        if (on)
        {
            const float pr = juce::jlimit (0.0f, 1.0f, proc.engine.getStepProgress());
            g.setColour (col::accent);
            g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth() * pr, 2.0f);
        }
    }

    if (dragging && pressed >= 0 && pressed < p.count)
    {
        // The slot you're moving, and where it will land.
        const int target = juce::jlimit (0, p.count - 1, slotAt (dragX));
        const auto tr = slotRect (target);
        g.setColour (col::accent.withAlpha (0.6f));
        g.fillRect (target <= pressed ? tr.getX() - 3.5f : tr.getRight() + 2.5f, tr.getY(), 1.0f, tr.getHeight());
        auto r = slotRect (pressed).withX (dragX - grabOffset);
        g.setColour (col::bg.withAlpha (0.9f));
        g.fillRect (r);
        g.setColour (col::ink);
        g.drawRect (r, 1.0f);
        text (g, chordName (key, p.steps[(size_t) pressed]), r.getX() + 6.0f, r.getY() + 5.0f, col::ink, 10.5f, juce::Justification::centredLeft, r.getWidth() - 10.0f);
    }
}

int HarmonyView::slotAt (float x) const
{
    return juce::jlimit (0, Progression::maxSteps - 1, (int) std::floor (x / (slotW() + gap)));
}

int HarmonyView::itemAt (juce::Point<float> pt) const
{
    const bool ed = proc.isEditingNext();
    if (pt.y < 18.0f)
    {
        const float kx = keyNameX();
        if (pt.x >= kx - 4.0f && pt.x < kx + 46.0f) return onKey;
        if (! ed)
        {
            juce::String kn;
            float xn = 0.0f, xk = 0.0f, xg = 0.0f;
            if (keysLayout (kn, xn, xk, xg))
            {
                if (pt.x >= xk - 5.0f && pt.x < xk + textWidth ("keep") + 6.0f) return onKeep;
                if (pt.x >= xg - 5.0f && pt.x < xg + textWidth ("let go") + 6.0f) return onLetGo;
            }
        }
        return onNothing;
    }
    for (int i = 0; i < Progression::maxSteps; ++i)
    {
        const auto r = slotRect (i);
        if (r.contains (pt))
        {
            const auto p = shownProg();
            if (i < p.count)
            {
                if (pt.y > r.getY() + 30.0f)
                {
                    if (pt.x > r.getRight() - 20.0f) return onEditBase + i;
                    if (! ed && i != playingIndex() && pt.x > r.getRight() - 52.0f) return onNowBase + i;
                }
                if (! ed && ! staying() && pt.x > r.getRight() - 20.0f && pt.y < r.getY() + 20.0f) return onLoopBase + i;
            }
            return i;
        }
    }
    return onNothing;
}

void HarmonyView::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    const auto p = shownProg();
    const bool clickable = h != onNothing && (h >= onEditBase || h < 0 || h <= p.count);
    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

std::pair<juce::String, juce::String> HarmonyView::hintAt (juce::Point<int> pt)
{
    const int i = itemAt (pt.toFloat());
    const auto p = shownProg();
    if (i == onKey) return { "on the root", "Every chord is counted from the root, set at the top beside the root sound. Click to pick another root here" };
    if (i == onKeep) return { "keep", "Keeps the chord you played as a slot of its own, so it's part of the piece" };
    if (i == onLetGo) return { "let go", "Back to the chord in the lit slot" };
    if (i == onStay) return { "stay", "When a chord's bars are done it stays, until you queue the next one" };
    if (i == onPlayOn) return { "loop", "When a chord's bars are done, the next one in the loop plays, round and round. The dot on each slot puts it in or out" };
    if (i >= onLoopBase)
        return p.steps[(size_t) (i - onLoopBase)].loop ? std::pair<juce::String, juce::String> { "in the loop", "Click to take this chord out of the loop. It then only plays when you queue it" }
                                                       : std::pair<juce::String, juce::String> { "out of the loop", "This chord only plays when you queue it. Click to put it back in the loop" };
    if (i >= onNowBase) return { "now", "Go to this chord right away, without waiting for the bar" };
    if (i >= onEditBase) return { "edit", "Change this chord's note, kind, length or octave, or remove it" };
    if (i >= 0 && i < p.count)
    {
        const auto name = chordName (keyShown(), p.steps[(size_t) i]);
        if (proc.isEditingNext()) return { name, "Click to change it. Drag to move it" };
        if (i == playingIndex()) return { name + " · playing", "Click to change it. Drag to move it" };
        return { name, "Click to queue it: it plays when this chord's bars are done. Click again for the next bar line. Drag to move it" };
    }
    if (i == p.count) return { "new chord", "Set up a chord in this slot: its note, kind and length" };
    if (i >= 0) return { "empty", "Off. Fill the slots from the left" };
    return { "harmony", "The chords your piece moves through. Texture and drones are tuned to the one playing. With a MIDI keyboard, play a note or a chord and the harmony goes there" };
}

void HarmonyView::openEditor (int index)
{
    auto* parent = getParentComponent();
    if (parent == nullptr) return;
    const auto area = slotRect (juce::jlimit (0, Progression::maxSteps - 1, index < 0 ? shownProg().count : index)).toNearestInt() + getPosition();
    juce::CallOutBox::launchAsynchronously (std::make_unique<ChordEditor> (*this, index), area, parent);
}

bool HarmonyView::keysLayout (juce::String& name, float& xName, float& xKeep, float& xLetGo) const
{
    ChordStep c;
    if (proc.isEditingNext() || ! proc.getKeysChord (c)) return false;
    name = chordName (keyShown(), c) + (c.oct != 0 ? " " + octText (c.oct) : juce::String());
    xName = keyNameX() + 60.0f + textWidth ("keys") + 10.0f;
    xKeep = xName + textWidth (name) + 18.0f;
    xLetGo = xKeep + textWidth ("keep") + 14.0f;
    return true;
}

float HarmonyView::keyNameX() const
{
    if (! proc.isEditingNext()) return textWidth ("chords") + 18.0f;
    return textWidth ("chords of " + lower (proc.getScenes().slots[(size_t) proc.getNextPart()].name)) + 24.0f;
}

void HarmonyView::setKey (int key)
{
    key = ((key % 12) + 12) % 12;
    auto* param = proc.apvts.getParameter (spec (hmKey).id);
    const float n = param->convertTo0to1 ((float) key);
    if (proc.isEditingNext()) proc.setPartValue (proc.getNextPart(), hmKey, n);
    else { param->beginChangeGesture(); param->setValueNotifyingHost (n); param->endChangeGesture(); }
    repaint();
}

void HarmonyView::keyMenu (juce::Component& target, juce::Rectangle<int> areaInTarget)
{
    juce::PopupMenu m;
    m.addSectionHeader ("root");
    const int cur = keyShown();
    for (int k = 0; k < 12; ++k)
    {
        // The two keys that sit closest to this one share most of its notes: the gentlest places to move to.
        const int up = ((k - cur) % 12 + 12) % 12;
        const juce::String near = up == 7 ? "   a fifth up, close" : (up == 5 ? "   a fifth down, close" : "");
        m.addItem (noteLower (k) + near, true, k == cur, [this, k] { setKey (k); });
    }
    auto opts = juce::PopupMenu::Options().withTargetComponent (&target);
    if (! areaInTarget.isEmpty()) opts = opts.withTargetScreenArea (target.localAreaToGlobal (areaInTarget));
    m.showMenuAsync (opts);
}

void HarmonyView::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    pressed = -1;
    const int i = itemAt (e.position);
    if (i >= 0 && i < onEditBase && i < shownProg().count)
    {
        pressed = i;
        grabOffset = e.position.x - slotRect (i).getX();
    }
}

void HarmonyView::mouseDrag (const juce::MouseEvent& e)
{
    if (pressed < 0) return;
    if (! dragging && e.getDistanceFromDragStart() > 6) dragging = true;
    if (dragging) { dragX = e.position.x; repaint(); }
}

void HarmonyView::mouseUp (const juce::MouseEvent& e)
{
    auto p = shownProg();
    if (dragging)
    {
        dragging = false;
        const int from = pressed, to = juce::jlimit (0, p.count - 1, slotAt (dragX));
        pressed = -1;
        if (from == to) { repaint(); return; }
        // Move the slot and keep track of where the playing and queued chords end up.
        std::vector<int> order;
        for (int k = 0; k < p.count; ++k) order.push_back (k);
        order.erase (order.begin() + from);
        order.insert (order.begin() + to, from);
        Progression q = p;
        int newPlaying = -1, newCue = -1;
        const int playing = playingIndex(), cue = proc.engine.getCue();
        for (int k = 0; k < p.count; ++k)
        {
            q.steps[(size_t) k] = p.steps[(size_t) order[(size_t) k]];
            if (order[(size_t) k] == playing) newPlaying = k;
            if (order[(size_t) k] == cue) newCue = k;
        }
        const int when = proc.engine.getCueWhen();
        apply (q, newPlaying);
        if (newCue >= 0) proc.engine.cueStep (newCue, when);
        return;
    }
    pressed = -1;

    const int i = itemAt (e.position);
    if (i == onKey) { keyMenu (*this, juce::Rectangle<int> ((int) keyNameX() + 16, 0, 30, 16)); return; }
    if (i == onKeep) { proc.keepKeysChord(); return; }
    if (i == onLetGo) { proc.releaseKeysChord(); repaint(); return; }
    if (i == onStay) { setStaying (true); return; }
    if (i == onPlayOn) { setStaying (false); return; }
    if (i >= onLoopBase)
    {
        const int k = i - onLoopBase;
        p.steps[(size_t) k].loop = ! p.steps[(size_t) k].loop;
        apply (p, playingIndex());
        return;
    }
    if (i >= onNowBase) { proc.engine.cueStep (i - onNowBase, 2); repaint(); return; }
    if (i >= onEditBase) { openEditor (i - onEditBase); return; }
    if (i < 0) return;
    if (i >= p.count)
    {
        if (i == p.count) openEditor (-1);   // set up a new chord
        return;
    }
    if (proc.isEditingNext() || e.mods.isPopupMenu() || i == playingIndex()) { openEditor (i); return; }
    // Queue it: when this chord is done; click again for the next bar line; shift-click for now.
    const int cue = proc.engine.getCue(), when = proc.engine.getCueWhen();
    if (e.mods.isShiftDown()) proc.engine.cueStep (i, 2);
    else if (cue == i && when == 0) proc.engine.cueStep (i, 1);
    else if (cue == i) proc.engine.cueStep (-1);
    else proc.engine.cueStep (i, 0);
    repaint();
}

/* ---------------- SourcesView ---------------- */

SourcesView::SourcesView (GrainFieldProcessor& p, Mode m) : proc (p), mode (m)
{
    if (mode == Mode::texture)
        for (int k = 0; k < 3; ++k)
        {
            sliders[k] = std::make_unique<HairSlider> (p, txMix1 + k, juce::String());
            setHint (*sliders[k], "amount", "How much of the texture comes from this sound. All the way down: none of it");
            addAndMakeVisible (*sliders[k]);
        }
    else
    {
        sliders[1] = std::make_unique<HairSlider> (p, drBed, juce::String ("bed"));
        setHint (*sliders[1], "bed level", "How loud the bed recording underneath everything is");
        addAndMakeVisible (*sliders[1]);
    }
    flashes.reserve (256);
}

void SourcesView::resized()
{
    if (mode == Mode::texture)
    {
        for (int k = 0; k < 3; ++k) sliders[k]->setBounds ((int) sliderX(), k * rowH + 3, 84, 16);
    }
    else sliders[1]->setBounds ((int) nameX(), 2 * rowH + 3, 150, 16);
}

juce::Rectangle<float> SourcesView::stripRect (int row) const
{
    const float right = mode == Mode::texture ? sliderX() - 14.0f : (float) getWidth();
    return { stripX(), (float) (row * rowH) + 2.0f, juce::jmax (20.0f, right - stripX()), 18.0f };
}

juce::String SourcesView::nameOf (int slot) const
{
    const auto id = proc.getSlotSourceId (slot);
    if (id.isEmpty() || id == "none") return "empty";
    if (id == "live") return "the track sent in";
    if (auto* s = proc.engine.getPlayingSource (slot)) if (s->id == id) return lower (s->name);
    for (auto& b : SourceLibrary::builtins()) if (b.id == id) return lower (b.name);
    return lower (id.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false));
}

juce::String SourcesView::rootLabel (bool& isSet) const
{
    const auto id = proc.getSlotSourceId (0);
    isSet = id != "none";
    for (int k = 1; k < numSlots; ++k) isSet = isSet && proc.getSlotSourceId (k) == id;
    return isSet ? "root: " + nameOf (0) : juce::String ("root sound");
}

void SourcesView::addEvents (const VisEvent* ev, int n)
{
    if (mode != Mode::texture) return;
    for (int i = 0; i < n; ++i)
        if (ev[i].type == evCloud && ev[i].slot >= 0 && ev[i].slot < 3)
        {
            if (flashes.size() >= 240) flashes.erase (flashes.begin(), flashes.begin() + 16);
            flashes.push_back ({ ev[i].slot, ev[i].x, ev[i].span, 0.0f });
        }
}

void SourcesView::tick (float dt)
{
    for (auto& f : flashes) f.age += dt;
    flashes.erase (std::remove_if (flashes.begin(), flashes.end(), [] (const Flash& f) { return f.age >= 0.7f; }), flashes.end());
    for (int r = 0; r < numRows(); ++r)
    {
        const int i = slotOf (r);
        const auto id = proc.getSlotSourceId (i);
        const bool own = id.startsWith ("file:") || id.startsWith ("capture:");
        cores[(size_t) i] = own ? proc.getSlotCoreNote (i) : -1.0f;
        canSetNote[(size_t) i] = own && proc.slotRootCanBeSet (i);
        noteChoice[(size_t) i] = own ? proc.getSlotRootChoice (i) : (int) GrainFieldProcessor::rootAsFound;
    }
    for (int r = 0; r < numRows(); ++r)
    {
        // Where each sound plays: light it up for a moment when it changes, whichever level moved it.
        const int np = notePOf (r), idx = shownChoice (np);
        const auto now = playsNote (np, idx) + juce::String (idx);
        if (playsShown[(size_t) r].isNotEmpty() && now != playsShown[(size_t) r]) playsFlash[(size_t) r] = 1.0f;
        playsShown[(size_t) r] = now;
        playsFlash[(size_t) r] = juce::jmax (0.0f, playsFlash[(size_t) r] - dt / 1.4f);
    }
    for (auto& sl : sliders) if (sl != nullptr) sl->tick();
    repaint();
}

int SourcesView::shownChoice (int p) const
{
    auto* param = proc.apvts.getParameter (spec (p).id);
    const float n = proc.isEditingNext() ? proc.getPartValue (proc.getNextPart(), p) : param->getValue();
    return juce::roundToInt (param->convertFrom0to1 (n));
}

void SourcesView::setChoice (int p, int index)
{
    auto* param = proc.apvts.getParameter (spec (p).id);
    const float n = param->convertTo0to1 ((float) index);
    if (proc.isEditingNext()) proc.setPartValue (proc.getNextPart(), p, n);
    else { param->beginChangeGesture(); param->setValueNotifyingHost (n); param->endChangeGesture(); }
    repaint();
}

juce::String SourcesView::playsNote (int p, int index) const
{
    // The root, then the module's step, then (for a texture sound) its own.
    int pc = shownChoice (hmKey) + noteStep (index).pc;
    if (p == txNote1 || p == txNote2 || p == txNote3) pc += noteStep (shownChoice (txNote)).pc;
    return noteLower (pc);
}

static juce::String stepWords (int p, int index)
{
    switch (index)
    {
        case 0: return (p == txNote1 || p == txNote2 || p == txNote3) ? "follows the texture" : "on the root";
        case 1: return "a fifth above";
        case 2: return "a fourth above";
        case 3: return "an octave up";
        case 4: return "an octave down";
        default: return "its own pitch, as recorded";
    }
}

void SourcesView::noteStepMenu (int p, juce::Component& target, juce::Rectangle<int> area)
{
    const int cur = shownChoice (p);
    juce::String title = "where the texture plays";
    if (p == drNote) title = "where the drones play";
    else if (p == bedNote) title = "where the bed plays";
    else if (p != txNote) title = "where " + nameOf (p - txNote1) + " plays";
    juce::PopupMenu m;
    m.addSectionHeader (title);
    for (int i = 0; i < numNoteSteps (p); ++i)
    {
        const auto st = noteStep (i);
        if (st.own) m.addSeparator();
        const juce::String label = st.own ? stepWords (p, i) : stepWords (p, i) + ":  " + playsNote (p, i);
        m.addItem (label, true, i == cur, [this, p, i] { setChoice (p, i); });
    }
    auto opts = juce::PopupMenu::Options().withTargetComponent (&target);
    if (! area.isEmpty()) opts = opts.withTargetScreenArea (target.localAreaToGlobal (area));
    m.showMenuAsync (opts);
}

void SourcesView::playsMenu (int row)
{
    noteStepMenu (notePOf (row), *this, playsRect (row).toNearestInt());
}

// How much of a sound the grains are cut from, either side of the window's middle. Smear widens it.
float SourcesView::windowHalf() const
{
    const float smear = proc.isEditingNext() ? proc.getPartValue (proc.getNextPart(), txSmear) : proc.engine.getDisplayNorm (txSmear);
    return juce::jmax (0.012f, 1.5f * (0.004f + 0.2f * smear * smear) + 0.008f + 0.05f * smear);
}

void SourcesView::paint (juce::Graphics& g)
{
    const int capturing = proc.getPendingCaptureSlot();
    const bool running = proc.engine.isRunning(), ed = proc.isEditingNext();
    for (int r = 0; r < numRows(); ++r)
    {
        const int i = slotOf (r);
        const float y = (float) (r * rowH) + 3.0f;
        const bool over = r == hover && ! hoverStrip;
        juce::String name = nameOf (i);
        juce::Colour c = col::ink;
        float x = nameX();
        if (mode == Mode::drones) text (g, r == 0 ? "from" : "under", 0.0f, y, col::dim (0.3f), 11.0f);

        const auto id = proc.getSlotSourceId (i);
        bool quiet = false;
        if (i == capturing)
        {
            const bool on = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
            dot (g, x + 4.0f, y + 7.5f, 3.0f, col::accent.withAlpha (on ? 1.0f : 0.35f));
            name = "listening";
            c = col::accent;
            x += 14.0f;
        }
        else if (name.startsWith ("resample") || name.startsWith ("locked"))
        {
            dot (g, x + 4.0f, y + 7.5f, 3.0f, col::accent);
            c = col::accent;
            x += 14.0f;
        }
        else if (name == "empty") c = col::dim (0.3f);
        else if (id == "live")
        {
            // A small level light: is anything coming in?
            const float lv = juce::jlimit (0.0f, 1.0f, proc.engine.getInputLevel() * 3.0f);
            dot (g, x + 4.0f, y + 7.5f, 2.5f, lv > 0.01f ? col::accent.withAlpha (0.35f + 0.65f * lv) : col::dim (0.25f));
            x += 14.0f;
            if (! proc.isSideChainConnected()) name = "no track connected";
            else if (lv <= 0.01f) name = "connected, silent";
        }
        else if (i == slotBed && ! proc.engine.isAudible (Engine::stBed)) { c = col::dim (0.3f); quiet = true; }
        if (over) c = c == col::accent ? col::ink : col::accent;

        float room = nameX() + nameW() - x;
        chipRects[(size_t) r] = {};
        if (canSetNote[(size_t) i] && i != capturing)
        {
            // Your own sound's note: found by listening when it was loaded, or what you've said it is. Click to change it.
            static const char* const nn[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
            const bool has = cores[(size_t) i] > 0.0f, chosen = noteChoice[(size_t) i] != GrainFieldProcessor::rootAsFound;
            const juce::String chip = has ? juce::String (nn[((int) cores[(size_t) i]) % 12]) : juce::String (chosen ? "no note" : "note");
            const float cw = textWidth (chip, 10.0f);
            const float cx = juce::jmin (x + textWidth (name) + 7.0f, nameX() + nameW() - cw - 2.0f);
            room = cx - x - 5.0f;
            const bool overChip = hover == r && hoverNote;
            text (g, chip, cx, y + 0.5f, overChip ? col::accent : (has ? col::accent.withAlpha (chosen ? 0.95f : 0.6f) : col::dim (0.32f)), 10.0f);
            if (chosen || overChip)
            {
                g.setColour (col::accent.withAlpha (overChip ? 0.9f : 0.45f));
                g.fillRect (cx, y + 14.5f, cw, 1.0f);
            }
            chipRects[(size_t) r] = { cx - 4.0f, y - 2.0f, cw + 8.0f, 19.0f };
        }
        text (g, name, x, y, hover == r && hoverNote ? (name == "empty" ? col::dim (0.3f) : col::ink) : c, 11.0f, juce::Justification::centredLeft, room);

        {
            // Where it plays: the note it is tuned to, and the step that put it there. Click to change it.
            const int np = notePOf (r), idx = shownChoice (np);
            const bool overP = hover == r && hoverPlays;
            const float fl = playsFlash[(size_t) r];
            float px = playsX();
            arrow (g, px, y + 7.5f, 7.0f, col::dim (overP ? 0.8f : 0.3f));
            px += 13.0f;
            if (noteStep (idx).own)
                text (g, "own", px, y, overP ? col::accent : col::dim (0.5f), 10.5f);
            else
            {
                const auto note = playsNote (np, idx);
                const juce::Colour base = idx != 0 ? col::ink : col::dim (0.6f);
                text (g, note, px, y, overP ? col::accent : base.interpolatedWith (col::accent, fl), 11.0f);
                if (idx != 0) text (g, noteStepName (np, idx), px + textWidth (note) + 5.0f, y + 0.5f, col::dim (0.45f), 9.5f);
            }
            if (overP)
            {
                g.setColour (col::accent.withAlpha (0.5f));
                g.fillRect (playsX() + 13.0f, y + 15.0f, playsW - 15.0f, 1.0f);
            }
        }

        // The sound itself, and where it is being read.
        const auto sr = stripRect (r);
        const float mid = sr.getCentreY();
        g.setColour (col::dim (0.09f));
        g.fillRect (sr.getX(), mid, sr.getWidth(), 1.0f);
        auto* src = proc.engine.getPlayingSource (i);
        if (id == "none" || id.isEmpty() || src == nullptr || src->id != id) continue;

        const bool isTexture = mode == Mode::texture;
        float centre = -1.0f, half = 0.0f;
        if (isTexture)
        {
            const float set = ed ? proc.getPartValue (proc.getNextPart(), txPos1 + r) : proc.apvts.getParameter (spec (txPos1 + r).id)->getValue();
            centre = running && ! ed && dragRow < 0 ? proc.engine.getReadPos (i) : set;
            half = windowHalf();
        }
        const float wa = juce::jlimit (0.0f, 1.0f, centre - half), wb = juce::jlimit (0.0f, 1.0f, centre + half);
        const float amount = isTexture ? proc.apvts.getParameter (spec (txMix1 + r).id)->getValue() : 1.0f;
        const float fade = quiet ? 0.4f : (isTexture ? 0.45f + 0.55f * juce::jmin (1.0f, amount * 2.0f) : 1.0f);

        if (src->live || src->peaks.empty())
        {
            // The track sent in has no fixed shape: a row of marks that light with its level.
            const float lv = juce::jlimit (0.0f, 1.0f, proc.engine.getInputLevel() * 3.0f);
            g.setColour (col::dim ((0.12f + 0.4f * lv) * fade));
            for (float px = sr.getX(); px < sr.getRight(); px += 4.0f) g.fillRect (px, mid - 1.0f - 3.0f * lv, 1.0f, 3.0f + 6.0f * lv);
        }
        else
        {
            float mx = 1.0e-4f;
            for (float pk : src->peaks) mx = juce::jmax (mx, pk);
            const int cols = juce::jmax (1, (int) (sr.getWidth() / 2.0f)), bins = (int) src->peaks.size();
            for (int cI = 0; cI < cols; ++cI)
            {
                const float f0 = (float) cI / (float) cols, f1 = (float) (cI + 1) / (float) cols;
                float pk = 0.0f;
                for (int b = (int) (f0 * (float) bins); b <= juce::jmin (bins - 1, (int) (f1 * (float) bins)); ++b) pk = juce::jmax (pk, src->peaks[(size_t) b]);
                const float h = juce::jmax (1.0f, std::sqrt (pk / mx) * sr.getHeight());
                const bool inside = isTexture && f1 >= wa && f0 <= wb;
                g.setColour (col::ink.withAlpha ((inside ? 0.62f : 0.2f) * fade));
                g.fillRect (sr.getX() + (float) cI * 2.0f, mid - h * 0.5f, 1.0f, h);
            }
        }

        if (isTexture)
        {
            // The window the grains are cut from. Drag it.
            const bool live = (hover == r && hoverStrip) || dragRow == r || (dragAll && dragRow >= 0);
            const float ax = sr.getX() + wa * sr.getWidth(), bx = sr.getX() + wb * sr.getWidth();
            g.setColour (col::accent.withAlpha (live ? 0.16f : 0.09f));
            g.fillRect (ax, sr.getY(), bx - ax, sr.getHeight());
            g.setColour (col::accent.withAlpha ((live ? 0.95f : 0.55f) * fade));
            g.fillRect (ax, sr.getY(), 1.0f, sr.getHeight());
            g.fillRect (bx - 1.0f, sr.getY(), 1.0f, sr.getHeight());
            // Each grain as it is cut.
            for (auto& f : flashes)
            {
                if (f.slot != i) continue;
                const float t = 1.0f - f.age / 0.7f;
                const float fx = sr.getX() + juce::jlimit (0.0f, 1.0f, f.x) * sr.getWidth();
                const float fw = juce::jlimit (1.5f, sr.getRight() - fx, f.span * sr.getWidth());
                g.setColour (col::ink.withAlpha (0.22f * t * t));
                g.fillRect (fx, sr.getY(), fw, sr.getHeight());
                g.setColour (col::ink.withAlpha (0.85f * t * t));
                g.fillRect (fx, sr.getY(), 1.0f, sr.getHeight());
            }
        }
        else if (running && ! quiet)
        {
            // Where the drones are listening, or the bed is playing, right now.
            const float px = sr.getX() + juce::jlimit (0.0f, 1.0f, proc.engine.getReadPos (i)) * sr.getWidth();
            g.setGradientFill (juce::ColourGradient (col::accent.withAlpha (0.0f), px - 26.0f, 0.0f, col::accent.withAlpha (0.16f), px, 0.0f, false));
            g.fillRect (juce::jmax (sr.getX(), px - 26.0f), sr.getY(), px - juce::jmax (sr.getX(), px - 26.0f), sr.getHeight());
            g.setColour (col::accent.withAlpha (0.85f));
            g.fillRect (px, sr.getY(), 1.0f, sr.getHeight());
        }
    }
}

int SourcesView::rowAt (juce::Point<float> p, bool& onStrip) const
{
    onStrip = false;
    for (int r = 0; r < numRows(); ++r)
        if (p.y >= (float) (r * rowH) && p.y < (float) ((r + 1) * rowH))
        {
            if (stripRect (r).expanded (3.0f, 3.0f).contains (p)) { onStrip = true; return r; }
            return p.x < stripX() - 4.0f ? r : -1;
        }
    return -1;
}

void SourcesView::mouseMove (const juce::MouseEvent& e)
{
    bool strip = false;
    const int h = rowAt (e.position, strip);
    const bool note = onNoteChip (e.position, h);
    const bool plays = h >= 0 && ! strip && playsRect (h).contains (e.position);
    if (h != hover || strip != hoverStrip || note != hoverNote || plays != hoverPlays) { hover = h; hoverStrip = strip; hoverNote = note; hoverPlays = plays; repaint(); }
    const bool movable = strip && mode == Mode::texture && h >= 0 && proc.getSlotSourceId (slotOf (h)) != "none";
    setMouseCursor (movable ? juce::MouseCursor::LeftRightResizeCursor
                            : (h >= 0 && ! strip ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
}

std::pair<juce::String, juce::String> SourcesView::hintAt (juce::Point<int> p)
{
    bool strip = false;
    const int r = rowAt (p.toFloat(), strip);
    if (r >= 0 && strip)
    {
        if (mode == Mode::texture)
            return { "where the grains come from", "The lit window is the stretch of this sound the grains are cut from; each flash is a grain. Drag to move it, shift-drag to move all three. Smear widens it" };
        return r == 0 ? std::pair<juce::String, juce::String> { "the drones' sound", "The line shows where in this sound the drones are listening right now" }
                      : std::pair<juce::String, juce::String> { "the bed", "The line shows where the bed is playing. With a clear note of its own, it is shifted to sit on the root, unless you leave it at its own pitch" };
    }
    if (r < 0)
        return mode == Mode::texture ? std::pair<juce::String, juce::String> { "texture sounds", "The three sounds the texture's grains are cut from, where in each, and how much of each" }
                                     : std::pair<juce::String, juce::String> { "drone sounds", "The sound the drones grow from, and the bed that plays underneath everything" };
    static const char* const what[numSlots] = { "texture", "texture", "texture", "the drones", "underneath everything" };
    const int i = slotOf (r);
    if (playsRect (r).contains (p.toFloat()))
    {
        if (mode == Mode::texture)
            return { "where " + nameOf (i) + " plays", "Counted from the texture: follows it, a fifth or a fourth above, an octave up or down, or its own pitch, left as recorded. Only this sound moves" };
        if (r == 0)
            return { "where the drones play", "Counted from the root: on it, a fifth or a fourth above, or an octave up or down. Every drone voice moves, melting over the dissolve time" };
        return { "where the bed plays", "Counted from the root: on it, a fifth or a fourth above, an octave up or down, or its own pitch, left as recorded" };
    }
    if (onNoteChip (p.toFloat(), r))
        return { "this sound's own note", "The note the sound is tuned from: found by listening, unless you've said otherwise. Click to say what it really is; the sound is then shifted from that note to fit each chord" };
    if (proc.getSlotSourceId (i) == "live")
    {
        // The side chain: say which of the three things is going on.
        if (! proc.isSideChainConnected())
            return { "side chain: nothing connected", "In Logic, pick the track in the Side Chain menu at the top right of this window. In Live, choose it under Audio From in the device's sidechain section" };
        if (proc.engine.getInputLevel() <= 0.004f)
            return { "side chain: connected, but silent", "A track is connected and nothing is coming from it. It only arrives while your song plays and that track has sound" };
        return { "side chain: receiving", "Grain Field is being made from the last few seconds of the track sent in. Click to pick another sound" };
    }
    return { nameOf (i) + " to " + what[i], "Click to pick another sound, load your own, or record what's playing. Whatever you pick follows where this layer plays" };
}

void SourcesView::setWindow (int row, float pos, bool all, float delta)
{
    for (int k = 0; k < 3; ++k)
    {
        if (! all && k != row) continue;
        const float n = juce::jlimit (0.0f, 1.0f, all ? dragStart[(size_t) k] + delta : pos);
        if (proc.isEditingNext()) proc.setPartValue (proc.getNextPart(), txPos1 + k, n);
        else proc.apvts.getParameter (spec (txPos1 + k).id)->setValueNotifyingHost (n);
    }
    repaint();
}

void SourcesView::mouseDown (const juce::MouseEvent& e)
{
    dragRow = -1;
    bool strip = false;
    const int r = rowAt (e.position, strip);
    if (r < 0 || ! strip || mode != Mode::texture || e.mods.isPopupMenu() || proc.getSlotSourceId (slotOf (r)) == "none") return;
    dragRow = r;
    dragAll = e.mods.isShiftDown();
    dragStartX = e.position.x;
    for (int k = 0; k < 3; ++k)
    {
        auto* param = proc.apvts.getParameter (spec (txPos1 + k).id);
        dragStart[(size_t) k] = proc.isEditingNext() ? proc.getPartValue (proc.getNextPart(), txPos1 + k) : param->getValue();
        if (! proc.isEditingNext() && (dragAll || k == r)) param->beginChangeGesture();
    }
    const auto sr = stripRect (r);
    if (! dragAll) setWindow (r, (e.position.x - sr.getX()) / sr.getWidth(), false, 0.0f);
}

void SourcesView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    const auto sr = stripRect (dragRow);
    if (dragAll) setWindow (dragRow, 0.0f, true, (e.position.x - dragStartX) / sr.getWidth());
    else if (e.getDistanceFromDragStart() > 1) setWindow (dragRow, (e.position.x - sr.getX()) / sr.getWidth(), false, 0.0f);
}

void SourcesView::mouseUp (const juce::MouseEvent& e)
{
    if (dragRow >= 0)
    {
        if (! proc.isEditingNext())
            for (int k = 0; k < 3; ++k)
                if (dragAll || k == dragRow) proc.apvts.getParameter (spec (txPos1 + k).id)->endChangeGesture();
        dragRow = -1;
        dragAll = false;
        repaint();
        return;
    }
    bool strip = false;
    const int r = rowAt (e.position, strip);
    if (onNoteChip (e.position, r)) { noteMenu (r); return; }
    if (r >= 0 && ! strip && playsRect (r).contains (e.position)) { playsMenu (r); return; }
    if (r >= 0 && ! strip) showMenu (r);
}

void SourcesView::noteMenu (int row)
{
    static const char* const nn[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    const int slot = slotOf (row);
    const int found = proc.getSlotFoundNote (slot), choice = proc.getSlotRootChoice (slot);
    juce::PopupMenu m;
    m.addSectionHeader ("this sound's own note");
    m.addItem ("as found by listening: " + (found >= 0 ? juce::String (nn[found]) : juce::String ("nothing clear")), true,
               choice == GrainFieldProcessor::rootAsFound, [this, slot] { proc.setSlotRoot (slot, GrainFieldProcessor::rootAsFound); });
    m.addSeparator();
    for (int k = 0; k < 12; ++k)
        m.addItem (nn[k], true, choice == k, [this, slot, k] { proc.setSlotRoot (slot, k); });
    m.addSeparator();
    m.addItem ("no note: leave it at its own pitch", true, choice == GrainFieldProcessor::rootNoNote,
               [this, slot] { proc.setSlotRoot (slot, GrainFieldProcessor::rootNoNote); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                         .withTargetScreenArea (localAreaToGlobal (chipRects[(size_t) juce::jlimit (0, 2, row)].toNearestInt())));
}

void SourcesView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Back to where the window starts out.
    bool strip = false;
    const int r = rowAt (e.position, strip);
    if (r < 0 || ! strip || mode != Mode::texture || dragRow != r) return;
    setWindow (r, proc.apvts.getParameter (spec (txPos1 + r).id)->getDefaultValue(), false, 0.0f);
    // The rest of this press doesn't move it.
    if (! proc.isEditingNext())
        for (int k = 0; k < 3; ++k)
            if (dragAll || k == dragRow) proc.apvts.getParameter (spec (txPos1 + k).id)->endChangeGesture();
    dragRow = -1;
    dragAll = false;
}

void SourcesView::showMenu (int row)
{
    const int slot = slotOf (row);
    juce::PopupMenu m;
    const auto current = proc.getSlotSourceId (slot);
    auto pick = [this, slot] (const juce::String& id) { proc.setSlotSourceId (slot, id); repaint(); };

    const auto all = SourceLibrary::builtins();
    struct Group { BuiltinKind kind; const char* title; };
    const Group groups[] = { { BuiltinKind::played, "textures" }, { BuiltinKind::sustained, "sustained" }, { BuiltinKind::place, "places" } };
    // The most fitting group first.
    std::vector<Group> order (std::begin (groups), std::end (groups));
    if (slot == slotTone) std::swap (order[0], order[1]);
    if (slot == slotBed) { std::swap (order[0], order[2]); }
    for (auto& grp : order)
    {
        m.addSectionHeader (grp.title);
        for (auto& b : all)
            if (b.kind == grp.kind)
                m.addItem (lower (b.name), true, b.id == current, [pick, id = b.id] { pick (id); });
    }

    const auto files = SourceLibrary::folderSounds();
    const auto loaded = proc.library.getLoaded();
    juce::PopupMenu yours;
    for (auto& f : files)
    {
        const auto id = "file:" + f.getFullPathName();
        yours.addItem (lower (f.getFileNameWithoutExtension()), true, id == current, [this, slot, f] { proc.loadFileIntoSlot (slot, f); });
    }
    bool sep = false;
    for (auto& s : loaded)
    {
        bool inFolder = false;
        for (auto& f : files) if (s->id == "file:" + f.getFullPathName()) inFolder = true;
        if (inFolder) continue;
        if (! sep && files.size() > 0) { yours.addSeparator(); sep = true; }
        yours.addItem (lower (s->name), true, s->id == current, [pick, id = s->id] { pick (id); });
    }
    m.addSeparator();
    m.addItem ("the track sent in (side chain)", true, current == "live", [pick] { pick ("live"); });
    if (yours.getNumItems() > 0) m.addSubMenu ("yours", yours);
    m.addItem (juce::String::fromUTF8 ("load a sound…"), [this, slot]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a sound", SourceLibrary::getSoundsFolder(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, slot] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadFileIntoSlot (slot, f);
                              });
    });
    m.addItem ("record the last 8 seconds", [this, slot] { proc.captureIntoSlot (slot); });
    const float core = (current.startsWith ("file:") || current.startsWith ("capture:")) ? proc.getSlotCoreNote (slot) : -1.0f;
    if (core > 0.0f)
    {
        static const char* const nn[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
        const juce::String note = nn[((int) core) % 12];
        m.addSeparator();
        m.addSectionHeader ("its core note is " + note);
        m.addItem ("tune to this sound: root " + note, [this, slot] { proc.tuneToSlot (slot); });
        m.addItem ("make a piece from this sound", [this, slot] { proc.pieceFromSlot (slot); });
    }
    else if (current.startsWith ("file:") || current.startsWith ("capture:"))
    {
        m.addSeparator();
        m.addSectionHeader ("no clear note in this sound");
    }
    if (proc.slotRootCanBeSet (slot))
        m.addItem (juce::String::fromUTF8 ("change its own note…"), [this, row] { noteMenu (row); });
    m.addItem ("empty", true, current == "none", [this, slot] { proc.clearSlot (slot); });
    m.addItem ("open the sounds folder", []
    {
        auto f = SourceLibrary::getSoundsFolder();
        f.createDirectory();
        f.revealToUser();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                         .withTargetScreenArea (localAreaToGlobal (juce::Rectangle<float> (nameX(), (float) (row * rowH), 160.0f, 18.0f).toNearestInt())));
}

void SourcesView::rootMenu (juce::Component& target)
{
    juce::PopupMenu m;
    m.addSectionHeader ("one sound for everything");
    m.addItem ("the track sent in (side chain)", [this] { proc.setRootSound ("live"); });
    m.addItem (juce::String::fromUTF8 ("load a recording…"), [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a recording", SourceLibrary::getSoundsFolder(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.setRootFile (f);
                              });
    });
    const auto files = SourceLibrary::folderSounds();
    if (files.size() > 0)
    {
        m.addSectionHeader ("your sounds folder");
        for (auto& f : files) m.addItem (lower (f.getFileNameWithoutExtension()), [this, f] { proc.setRootFile (f); });
    }
    juce::PopupMenu captured;
    for (auto& s : proc.library.getLoaded())
        if (s->id.startsWith ("capture:") || s->id.contains ("/Captures/"))
            captured.addItem (lower (s->name), [this, id = s->id] { proc.setRootSound (id); });
    if (captured.getNumItems() > 0) m.addSubMenu ("recorded here", captured);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

/* ---------------- The modules' pictures ---------------- */

void Picture::panel (juce::Graphics& g) const
{
    g.setColour (juce::Colour (0xff0A0A0A));
    g.fillRect (getLocalBounds());
    g.setColour (col::dim (0.05f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

static juce::String noteName (float hz)
{
    static const char* const n[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    const int m = juce::roundToInt (69.0f + 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f));
    return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
}

TexturePicture::TexturePicture (GrainFieldProcessor& p) : Picture (p)
{
    dots.reserve (640);
    setHint (*this, "texture", "Every grain is a soft light: left to right where you hear it, low to high by its note. Its tape makes the cloud sway and drop out; its space widens each light");
}

void TexturePicture::addEvents (const VisEvent* evs, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const auto& ev = evs[i];
        Dot d;
        d.type = ev.type;
        d.rev = ev.rev;
        d.u = juce::jlimit (0.0f, 1.0f, 0.5f + 0.46f * ev.pan + (rng.nextFloat() - 0.5f) * 0.05f);
        d.v = ev.midi > 0.0f ? juce::jlimit (0.0f, 1.0f, pitchToV (ev.midi) + (rng.nextFloat() - 0.5f) * 0.035f)
                             : 0.25f + 0.5f * rng.nextFloat();
        d.r = 1.0f + 3.0f * juce::jlimit (0.0f, 1.0f, ev.dur / 5.0f);
        d.life = juce::jlimit (0.35f, 6.0f, ev.dur) + 0.3f;
        d.alpha = 0.25f + 0.6f * juce::jlimit (0.0f, 1.0f, ev.near);
        d.age = 0.0f;
        if (ev.type != evCloud)
        {
            // Detail: small, quick, anywhere.
            d.v = 0.1f + 0.8f * rng.nextFloat();
            d.r = ev.type == evDust ? 0.6f : 0.9f;
            d.alpha *= 0.6f;
            d.life = 0.5f;
        }
        if (dots.size() >= 400) dots.erase (dots.begin(), dots.begin() + 8);
        dots.push_back (d);
    }
}

void TexturePicture::advance (float dt)
{
    for (auto& d : dots) d.age += dt;
    dots.erase (std::remove_if (dots.begin(), dots.end(), [] (const Dot& d) { return d.age >= d.life; }), dots.end());
}

void TexturePicture::paint (juce::Graphics& g)
{
    panel (g);
    const auto& eng = proc.engine;
    const bool running = eng.isRunning(), on = eng.isAudible (Engine::stTexture);
    const float w = (float) getWidth() - 28.0f, left = 14.0f, top = 12.0f, floor = (float) getHeight() - 12.0f;
    bool anySound = false;
    for (int k = 0; k < 3; ++k) anySound = anySound || proc.getSlotSourceId (k) != "none";
    if (! anySound)
    {
        text (g, "pick a sound above", (float) getWidth() * 0.5f, (float) getHeight() * 0.5f - 8.0f, col::dim (0.22f), 10.0f, juce::Justification::centred, 200.0f);
        return;
    }

    // The texture's own tape: the whole cloud sways with its wow and dims when it drops out.
    const float sway = eng.getTapeWow (0) * 9.0f, drop = juce::jlimit (0.0f, 1.0f, eng.getTapeDrop (0));
    const float smear = disp (txSmear), space = disp (txSpace), tape = disp (txTape);
    const float layer = (running ? 1.0f : 0.3f) * (on ? 1.0f : 0.25f) * (1.0f - 0.65f * drop);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));
    if (layer < 0.999f) g.beginTransparencyLayer (layer);
    for (auto& d : dots)
    {
        const float t = d.age / d.life;
        // Forward grains arrive and fade; reversed ones swell in and vanish.
        float env = t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f;
        if (d.rev) env = t < 0.8f ? t / 0.8f : 1.0f - (t - 0.8f) / 0.2f;
        env = env * env * (3.0f - 2.0f * env);
        const float dx = left + d.u * w + sway, dy = floor - d.v * (floor - top);
        const float a = juce::jlimit (0.0f, 1.0f, d.alpha * env);
        if (d.type == evGlitch) { g.setColour (col::ink.withAlpha (a)); g.fillRect (dx - 3.0f, dy, 6.0f, 1.0f); continue; }
        if (d.type == evCloud)
        {
            // A soft light: wider the more the cloud is smeared, and the more of it goes into the room.
            const float hr = d.r * (3.0f + 4.0f * smear + 6.0f * space);
            g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (a * (0.16f + 0.1f * space)), dx, dy, col::ink.withAlpha (0.0f), dx + hr, dy, true));
            g.fillEllipse (dx - hr, dy - hr, hr * 2.0f, hr * 2.0f);
        }
        const float cr = d.type == evCloud ? d.r * (0.45f + 0.35f * env) : d.r;
        g.setColour (col::ink.withAlpha (a * 0.9f));
        g.fillEllipse (dx - cr, dy - cr, cr * 2.0f, cr * 2.0f);
    }
    if (running && tape > 0.2f)
    {
        // Worn tape: a little dust across the picture.
        juce::Random r ((juce::int64) (time * 14.0f));
        const int specks = (int) (tape * tape * 20.0f);
        for (int i = 0; i < specks; ++i)
        {
            g.setColour (col::ink.withAlpha (0.05f + 0.16f * r.nextFloat()));
            g.fillRect (left + r.nextFloat() * w, top + r.nextFloat() * (floor - top), 1.0f, 1.0f);
        }
    }
    if (layer < 0.999f) g.endTransparencyLayer();
    g.restoreState();
}

DronesPicture::DronesPicture (GrainFieldProcessor& p) : Picture (p)
{
    setHint (*this, "drones", "Each line is a voice at its pitch, as bright as it is loud; the orange one is the melody. When the chord or key changes, the old notes fade as the new ones come up");
}

void DronesPicture::paint (juce::Graphics& g)
{
    panel (g);
    const auto& eng = proc.engine;
    const bool running = eng.isRunning(), on = eng.isAudible (Engine::stDrones), bedOn = eng.isAudible (Engine::stBed);
    const float W = (float) getWidth(), H = (float) getHeight();
    const float top = 12.0f, floor = H - 16.0f, xA = 12.0f, xB = W - 42.0f;
    if (proc.getSlotSourceId (slotTone) == "none" && proc.getSlotSourceId (slotBed) == "none")
    {
        text (g, "pick a sound above", W * 0.5f, H * 0.5f - 8.0f, col::dim (0.22f), 10.0f, juce::Justification::centred, 200.0f);
        return;
    }
    const float drop = juce::jlimit (0.0f, 1.0f, eng.getTapeDrop (1)), wow = eng.getTapeWow (1);
    const float tape = disp (drTape), space = disp (drSpace), mass = eng.getMass();
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));
    if (! running) g.beginTransparencyLayer (0.3f);

    // The bed: a low glow along the floor.
    if (proc.getSlotSourceId (slotBed) != "none")
    {
        const float bed = disp (drBed) * (bedOn ? 1.0f : 0.2f);
        g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (0.02f + 0.11f * bed), 0.0f, H, col::ink.withAlpha (0.0f), 0.0f, H - 30.0f, false));
        g.fillRect (1.0f, H - 30.0f, W - 2.0f, 29.0f);
    }
    // Merge: the texture coming in from next door.
    const float merge = disp (txFeed) * juce::jmin (1.0f, eng.getEventRate() / 4.0f);
    if (merge > 0.02f)
    {
        g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (0.09f * merge), 0.0f, 0.0f, col::ink.withAlpha (0.0f), 90.0f, 0.0f, false));
        g.fillRect (1.0f, 1.0f, 90.0f, H - 2.0f);
    }

    const float layer = (on ? 1.0f : 0.25f) * (1.0f - 0.65f * drop);
    if (layer < 0.999f) g.beginTransparencyLayer (layer);
    struct Label { float y; juce::String n; float a; };
    std::vector<Label> labels;
    // A line that comes out of nothing at each end, without a visible corner where it gets there.
    auto fadeEnds = [&] (juce::Colour c, float alpha)
    {
        juce::ColourGradient gr (c.withAlpha (0.0f), xA, 0.0f, c.withAlpha (0.0f), xB, 0.0f, false);
        static const float knee[5][2] = { { 0.04f, 0.1f }, { 0.08f, 0.35f }, { 0.12f, 0.68f }, { 0.16f, 0.9f }, { 0.2f, 1.0f } };
        for (auto& k : knee)
        {
            gr.addColour ((double) k[0], c.withAlpha (alpha * k[1]));
            gr.addColour (1.0 - (double) k[0], c.withAlpha (alpha * k[1]));
        }
        return gr;
    };
    auto voiceLine = [&] (int v, float hz, float a)
    {
        if (a < 0.01f) return;
        const float midi = 69.0f + 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f);
        // The drones' own tape: every line wavers a little, each its own way.
        const float waver = wow * (1.5f + 3.0f * tape) * (v % 2 == 0 ? 1.0f : -0.7f);
        const float vy = juce::jlimit (top, floor, juce::jmap (midi, 26.0f, 88.0f, floor, top + 2.0f) + waver);
        const float th = 0.8f + 2.0f * a;
        const juce::Colour vc = v == Engine::lineVoice ? col::accent : col::ink;
        // A soft light round each voice: wider when it's loud, and the more of it goes into the room.
        const float halo = (5.0f + 9.0f * a) * (0.6f + 0.9f * space), peak = 0.03f + 0.08f * a;
        for (int k = 1; k <= 6; ++k)
        {
            const float off = halo * (float) k / 6.0f, fall = 1.0f - (float) k / 7.0f;
            g.setGradientFill (fadeEnds (vc, peak * fall * fall));
            g.fillRect (xA, vy - off - halo / 12.0f, xB - xA, halo / 6.0f);
            g.fillRect (xA, vy + off - halo / 12.0f, xB - xA, halo / 6.0f);
        }
        g.setGradientFill (fadeEnds (vc, 0.1f + 0.7f * a));
        g.fillRect (xA, vy - th * 0.5f, xB - xA, th);
        if (a > 0.08f) labels.push_back ({ vy, noteName (hz), a });
    };
    const float halfPi = juce::MathConstants<float>::halfPi;
    for (int v = 0; v <= Engine::subVoice; ++v)
    {
        const float a = juce::jlimit (0.0f, 1.0f, eng.getVoiceAmp (v) * 1.4f);
        const float x = eng.getVoiceXfade (v);
        if (x < 0.999f)
        {
            // Melting from one note into the next: both are there for a while.
            voiceLine (v, eng.getVoiceOldHz (v), a * std::cos (x * halfPi));
            voiceLine (v, eng.getVoiceHz (v), a * std::sin (x * halfPi));
        }
        else voiceLine (v, eng.getVoiceHz (v), a);
    }
    std::sort (labels.begin(), labels.end(), [] (const Label& l1, const Label& l2) { return l1.y < l2.y; });
    float lastY = -100.0f;
    for (auto& l : labels)
        if (l.y - lastY >= 11.0f)
        {
            text (g, l.n, xB + 10.0f, l.y - 6.0f, col::dim (0.2f + 0.3f * l.a), 9.0f);
            lastY = l.y;
        }
    juce::ignoreUnused (mass);
    if (layer < 0.999f) g.endTransparencyLayer();
    if (! running) g.endTransparencyLayer();
    g.restoreState();
}

OutputPicture::OutputPicture (GrainFieldProcessor& p) : Picture (p)
{
    setHint (*this, "output", "Where texture and drones come together. The core is how loud it is, warmer with drive; the rings are the room; the dots trailing off are the echo");
}

void OutputPicture::paint (juce::Graphics& g)
{
    panel (g);
    const auto& eng = proc.engine;
    const bool running = eng.isRunning();
    const bool outside = proc.apvts.getRawParameterValue (ids::spaceMode)->load() > 0.5f;
    const float W = (float) getWidth(), H = (float) getHeight();
    const float level = juce::jlimit (0.0f, 1.0f, eng.getLevel() * 4.0f) * (running ? 1.0f : 0.0f);
    const float drive = disp (outDrive), size = disp (spSize), echo = disp (spEcho);
    const float space = juce::jmax (disp (txSpace), disp (drSpace));
    const float cx = W * 0.4f, cy = H * 0.5f;
    const float maxR = juce::jmax (20.0f, juce::jmin (cx - 10.0f, cy - 12.0f, 130.0f));
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));
    if (! running) g.beginTransparencyLayer (0.35f);

    // The room: a haze as big as the room, and slow rings opening out through it.
    const float radius = maxR * (0.4f + 0.6f * size), dim = outside ? 0.45f : 1.0f;
    const float hazeA = (0.02f + 0.09f * space) * dim * (0.75f + 0.25f * std::sin (time * 0.4f));
    g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (hazeA), cx, cy, col::ink.withAlpha (0.0f), cx + radius, cy, true));
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    const float ringA = (0.04f + 0.16f * space) * dim;
    for (int i = 0; i < 3; ++i)
    {
        float ph = time * (0.11f / (0.5f + size)) + (float) i / 3.0f;
        ph -= std::floor (ph);
        const float rr = 5.0f + ph * (radius - 5.0f);
        g.setColour (col::ink.withAlpha (ringA * (1.0f - ph) * (1.0f - ph) * juce::jmin (1.0f, ph * 8.0f)));
        g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.0f);
    }

    // The echo: the sound again and again, fainter each time.
    const int echoes = juce::roundToInt (echo * 5.0f);
    const float gapX = juce::jmin (24.0f, (W - cx - 14.0f) / 5.5f);
    for (int i = 1; i <= echoes; ++i)
    {
        const float fall = std::pow (0.68f, (float) i);
        dot (g, cx + gapX * (float) i, cy, juce::jmax (1.0f, (2.2f + 2.5f * level) * std::pow (0.85f, (float) i)), col::ink.withAlpha ((0.25f + 0.5f * level) * fall));
    }

    // The core: everything together. Brighter and bigger when it's loud, warmer with drive.
    const juce::Colour core = col::ink.interpolatedWith (col::accent, juce::jmin (1.0f, drive * 1.25f));
    const float cr = 12.0f + 26.0f * level + 10.0f * drive;
    g.setGradientFill (juce::ColourGradient (core.withAlpha (0.10f + 0.5f * level), cx, cy, core.withAlpha (0.0f), cx + cr, cy, true));
    g.fillEllipse (cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
    dot (g, cx, cy, 2.0f + 3.0f * level, core.withAlpha (0.55f + 0.45f * level));

    if (outside) text (g, "the room goes to send", W * 0.5f, H - 22.0f, col::dim (0.32f), 9.0f, juce::Justification::centred, W - 10.0f);
    if (! running) g.endTransparencyLayer();
    g.restoreState();
}
} // namespace gf::ui
