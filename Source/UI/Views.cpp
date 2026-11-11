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
    // One cell per scene, sharing the bay, then the cell that keeps what you hear as a new one.
    chips.clear();
    int n = 0;
    for (int i = 0; i < numScenes; ++i) n += proc.getScenes().slots[(size_t) i].used ? 1 : 0;
    const bool canAdd = proc.getScenes().firstFree() >= 0;
    const float addW = canAdd ? 92.0f : 0.0f, h = (float) getHeight();
    const float w = std::floor (((float) getWidth() - addW) / (float) juce::jmax (5, n));
    float x = 0.0f;
    for (int i = 0; i < numScenes; ++i)
    {
        if (! proc.getScenes().slots[(size_t) i].used) continue;
        chips.push_back ({ i, { x, 0.0f, w, h } });
        x += w;
    }
    addRect = canAdd ? juce::Rectangle<float> ((float) getWidth() - addW, 0.0f, addW, h) : juce::Rectangle<float>();
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
    const int active = proc.getActivePart();
    const float pr = proc.engine.getBlendProgress();
    for (auto& c : chips)
    {
        const bool on = c.part == active, over = hover == c.part;
        const auto r = c.r.withTrimmedRight (1.0f);
        if (on && pr >= 0.0f && pr < 1.0f)
        {
            // Blending in: the cell lights up from the left.
            raised (g, r, false, false);
            g.setColour (col::ink);
            g.fillRect (r.withWidth (std::round (r.getWidth() * pr)));
        }
        else raised (g, r, on, over);
        ruleV (g, c.r.getRight() - 1.0f, 0.0f, c.r.getBottom());
        label (g, sceneName (c.part), r.getCentreX(), r.getCentreY() - 6.0f, on ? col::bg : (over ? col::bright : col::dim (0.7f)), 8.0f,
               juce::Justification::centred, r.getWidth() - 8.0f);
    }
    if (! addRect.isEmpty())
    {
        const bool over = hover == onAdd;
        raised (g, addRect, false, over);
        label (g, "+ keep", addRect.getCentreX(), addRect.getCentreY() - 6.0f, over ? col::bright : col::dim (0.5f), 8.0f, juce::Justification::centred, addRect.getWidth());
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

    // A chord played on a MIDI keyboard: its name, and what to do with it, set into the top border.
    juce::String keysName;
    float xKeysName = 0.0f, xKeep = 0.0f, xLetGo = 0.0f;
    const bool keysHold = keysLayout (keysName, xKeysName, xKeep, xLetGo);
    const bool blink = (juce::Time::getMillisecondCounter() / 350) % 2 == 0;
    const int firstEmpty = p.count;
    for (int i = 0; i < Progression::maxSteps; ++i)
    {
        auto r = slotRect (i);
        const bool used = i < p.count;
        const bool isDragged = dragging && i == pressed;
        if (i < Progression::maxSteps - 1) ruleV (g, r.getRight() - 1.0f, r.getY(), r.getBottom());
        r = r.withTrimmedRight (i < Progression::maxSteps - 1 ? 1.0f : 0.0f);
        if (! used)
        {
            // Off until you set it up.
            g.setColour (i == firstEmpty && hover == i ? col::raised : juce::Colour (0xff060606));
            g.fillRect (r);
            if (i == firstEmpty)
                text (g, "+", r.getCentreX(), r.getCentreY() - 11.0f, col::dim (hover == i ? 1.0f : 0.4f), 16.0f, juce::Justification::centred, 30.0f);
            continue;
        }
        if (isDragged) { g.setColour (juce::Colour (0xff060606)); g.fillRect (r); continue; }

        const auto& st = p.steps[(size_t) i];
        const bool on = i == playing, queued = i == cue;
        const bool over = hover == i || hover == onEditBase + i || hover == onNowBase + i || hover == onLoopBase + i;
        const bool looping = ! ed && ! staying(), out = looping && ! st.loop;   // out of the loop: it waits until you queue it
        g.setColour (on ? juce::Colour (0xff161616) : (over ? col::raisedOver : (out ? juce::Colour (0xff0C0C0C) : juce::Colour (0xff111111))));
        g.fillRect (r);

        // The name on a title bar: inverse on the chord that's playing.
        const auto cname = chordName (key, st);
        const auto head = r.withHeight (19.0f);
        if (on && ! keysHold) { g.setColour (col::ink); g.fillRect (head); }
        else ruleH (g, head.getX(), head.getRight(), head.getBottom() - 1.0f, col::faint);
        const juce::Colour nameC = on && ! keysHold ? col::bg : (over || queued ? col::bright : col::dim (out ? 0.4f : 0.85f));
        text (g, cname, r.getX() + 7.0f, r.getY() + 1.0f, nameC, 12.0f, juce::Justification::centredLeft, r.getWidth() - 24.0f);
        // Moved up or down from where it would sit: say so beside the name.
        if (st.oct != 0 && textWidth (cname, 12.0f) + 40.0f < r.getWidth())
            text (g, octText (st.oct), r.getX() + 7.0f + textWidth (cname, 12.0f) + 5.0f, r.getY() + 2.0f, on && ! keysHold ? col::bg : col::dim (0.7f), 10.0f);
        if (looping)
        {
            // In the loop: a lit square. Out of it: a hollow one.
            const float lx = r.getRight() - 11.0f, ly = r.getY() + 7.0f;
            const juce::Colour lc = hover == onLoopBase + i ? col::bright : (on && ! keysHold ? col::bg : col::dim (st.loop ? 0.7f : 0.35f));
            g.setColour (lc);
            if (st.loop) g.fillRect (lx, ly, 5.0f, 5.0f);
            else g.drawRect (lx, ly, 5.0f, 5.0f, 1.0f);
        }
        text (g, chordNotesFromBass (key, st), r.getX() + 7.0f, r.getY() + 20.0f, col::dim (out ? 0.3f : 0.5f), 10.0f, juce::Justification::centredLeft, r.getWidth() - 10.0f);

        // The bars as pixels: lit as they pass. Queued chords say when they come in instead.
        const float by = r.getBottom() - 9.0f;
        if (queued)
            label (g, when == 0 ? "next" : (when == 1 ? "at bar" : "now"), r.getX() + 7.0f, by - 5.0f, blink ? col::bright : col::dim (0.5f), 7.0f);
        else
        {
            const int nb = juce::jlimit (1, 16, st.bars);
            const float pr = on ? juce::jlimit (0.0f, 1.0f, proc.engine.getStepProgress()) : 0.0f;
            const int done = (int) std::floor (pr * (float) nb);
            for (int k = 0; k < nb; ++k)
            {
                g.setColour (k < done ? col::bright : (k == done && on && blink ? juce::Colour (0xff8A8A8A) : juce::Colour (0xff2E2E2E)));
                g.fillRect (r.getX() + 7.0f + (float) k * 7.0f, by, 5.0f, 4.0f);
            }
        }
        if (over && ! dragging)
        {
            label (g, "edit", r.getRight() - 6.0f, by - 5.0f, hover == onEditBase + i ? col::bright : col::dim (0.55f), 7.0f, juce::Justification::centredRight, 40.0f);
            if (! ed && ! on)
                label (g, "now", r.getRight() - 38.0f, by - 5.0f, hover == onNowBase + i ? col::bright : col::dim (0.55f), 7.0f, juce::Justification::centredRight, 40.0f);
        }
        if (queued)
        {
            g.setColour (col::bright.withAlpha (blink ? 0.9f : 0.3f));
            g.drawRect (r, 1.0f);
        }
    }

    if (keysHold)
    {
        const float x0 = xKeysName - labelWidth ("keys", 8.0f) - 16.0f, x1 = xLetGo + textWidth ("let go", 11.0f) + 8.0f;
        g.setColour (col::bg);
        g.fillRect (x0, 0.0f, x1 - x0, topPad * 2.0f);
        label (g, "keys", x0 + 6.0f, topPad - 6.0f, col::dim (0.6f), 8.0f);
        text (g, keysName, xKeysName, topPad - 9.0f, col::bright, 11.0f);
        text (g, "keep", xKeep, topPad - 9.0f, hover == onKeep ? col::bright : col::dim (0.75f), 11.0f);
        text (g, "let go", xLetGo, topPad - 9.0f, hover == onLetGo ? col::bright : col::dim (0.5f), 11.0f);
    }

    if (dragging && pressed >= 0 && pressed < p.count)
    {
        // The slot you're moving, and where it will land.
        const int target = juce::jlimit (0, p.count - 1, slotAt (dragX));
        const auto tr = slotRect (target);
        g.setColour (col::bright);
        g.fillRect (target <= pressed ? tr.getX() : tr.getRight() - 2.0f, tr.getY(), 2.0f, tr.getHeight());
        auto r = slotRect (pressed).withX (dragX - grabOffset);
        g.setColour (col::raised);
        g.fillRect (r);
        g.setColour (col::ink);
        g.drawRect (r, 1.0f);
        text (g, chordName (key, p.steps[(size_t) pressed]), r.getX() + 7.0f, r.getY() + 1.0f, col::ink, 12.0f, juce::Justification::centredLeft, r.getWidth() - 10.0f);
    }
}

int HarmonyView::slotAt (float x) const
{
    return juce::jlimit (0, Progression::maxSteps - 1, (int) std::floor (x / (slotW() + gap)));
}

int HarmonyView::itemAt (juce::Point<float> pt) const
{
    const bool ed = proc.isEditingNext();
    if (pt.y < topPad + 6.0f && ! ed)
    {
        // keep / let go, set into the top border while a keyboard chord holds.
        juce::String kn;
        float xn = 0.0f, xk = 0.0f, xg = 0.0f;
        if (keysLayout (kn, xn, xk, xg))
        {
            if (pt.x >= xk - 5.0f && pt.x < xk + textWidth ("keep") + 6.0f) return onKeep;
            if (pt.x >= xg - 5.0f && pt.x < xg + textWidth ("let go") + 6.0f) return onLetGo;
        }
    }
    for (int i = 0; i < Progression::maxSteps; ++i)
    {
        const auto r = slotRect (i);
        if (r.contains (pt))
        {
            const auto p = shownProg();
            if (i < p.count)
            {
                if (pt.y > r.getBottom() - 18.0f)
                {
                    if (pt.x > r.getRight() - 30.0f) return onEditBase + i;
                    if (! ed && i != playingIndex() && pt.x > r.getRight() - 62.0f) return onNowBase + i;
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
    xName = 12.0f + labelWidth ("keys", 8.0f) + 10.0f;
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
        // A row of the table: a step up from the panel, a rule underneath.
        g.setColour (over || (r == hover && (hoverPlays || hoverNote)) ? col::raised : juce::Colour (0xff0D0D0D));
        g.fillRect (0.0f, (float) (r * rowH), (float) getWidth(), (float) rowH - 1.0f);
        ruleH (g, 0.0f, (float) getWidth(), (float) (r * rowH + rowH - 1), col::faint);
        if (mode == Mode::drones) label (g, r == 0 ? "from" : "under", 6.0f, y + 2.0f, col::dim (0.45f), 7.0f);

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
        const float mid = std::round (sr.getCentreY());
        g.setColour (col::faint);
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
            // A pixel waveform: columns of stacked 2-pixel cells, mirrored about the middle.
            float mx = 1.0e-4f;
            for (float pk : src->peaks) mx = juce::jmax (mx, pk);
            const int cols = juce::jmax (1, (int) (sr.getWidth() / 3.0f)), bins = (int) src->peaks.size();
            for (int cI = 0; cI < cols; ++cI)
            {
                const float f0 = (float) cI / (float) cols, f1 = (float) (cI + 1) / (float) cols;
                float pk = 0.0f;
                for (int b = (int) (f0 * (float) bins); b <= juce::jmin (bins - 1, (int) (f1 * (float) bins)); ++b) pk = juce::jmax (pk, src->peaks[(size_t) b]);
                const int cells = juce::jmax (1, juce::roundToInt (std::sqrt (pk / mx) * (sr.getHeight() * 0.5f) / 3.0f));
                const bool inside = isTexture && f1 >= wa && f0 <= wb;
                g.setColour (inside ? col::ink.withAlpha (0.9f * fade) : juce::Colour (0xff4A4A4A).withMultipliedAlpha (fade));
                const float cx = sr.getX() + (float) cI * 3.0f;
                for (int k = 0; k < cells; ++k)
                {
                    g.fillRect (cx, mid - 2.0f - (float) k * 3.0f, 2.0f, 2.0f);
                    g.fillRect (cx, mid + 1.0f + (float) k * 3.0f, 2.0f, 2.0f);
                }
            }
        }

        if (isTexture)
        {
            // The window the grains are cut from. Drag it.
            const bool live = (hover == r && hoverStrip) || dragRow == r || (dragAll && dragRow >= 0);
            const float ax = std::round (sr.getX() + wa * sr.getWidth()), bx = std::round (sr.getX() + wb * sr.getWidth());
            dither (g, { ax, sr.getY(), bx - ax, sr.getHeight() }, 0.12f, live ? 0.35f : 0.18f);
            g.setColour (col::bright.withAlpha ((live ? 1.0f : 0.6f) * fade));
            g.fillRect (ax, sr.getY(), 2.0f, sr.getHeight());
            g.fillRect (bx - 2.0f, sr.getY(), 2.0f, sr.getHeight());
            // Each grain as it is cut.
            for (auto& f : flashes)
            {
                if (f.slot != i) continue;
                const float t = 1.0f - f.age / 0.7f;
                const float fx = sr.getX() + juce::jlimit (0.0f, 1.0f, f.x) * sr.getWidth();
                const float fw = juce::jlimit (1.5f, sr.getRight() - fx, f.span * sr.getWidth());
                dither (g, { std::round (fx), sr.getY(), std::round (fw), sr.getHeight() }, 0.5f, 0.5f * t * t);
                g.setColour (col::bright.withAlpha (0.9f * t * t));
                g.fillRect (std::round (fx), sr.getY(), 2.0f, sr.getHeight());
            }
        }
        else if (running && ! quiet)
        {
            // Where the drones are listening, or the bed is playing, right now.
            // The read head, with a short dithered trail behind it.
            const float px = std::round (sr.getX() + juce::jlimit (0.0f, 1.0f, proc.engine.getReadPos (i)) * sr.getWidth());
            const float tx = juce::jmax (sr.getX(), px - 24.0f);
            dither (g, { tx, sr.getY(), px - tx, sr.getHeight() }, 0.25f, 0.45f);
            g.setColour (col::bright);
            g.fillRect (px, sr.getY(), 2.0f, sr.getHeight());
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
    screen (g, getLocalBounds().toFloat());
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
        // Each grain drifts a little, away from the middle, so it leaves a trail.
        d.vx = (d.u - 0.5f) * 0.08f + (rng.nextFloat() - 0.5f) * 0.06f;
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
    const float W = (float) getWidth(), H = (float) getHeight();
    const float w = W - 28.0f, left = 14.0f, top = 12.0f, floor = H - 12.0f;
    bool anySound = false;
    for (int k = 0; k < 3; ++k) anySound = anySound || proc.getSlotSourceId (k) != "none";
    if (! anySound)
    {
        label (g, "pick a sound below", W * 0.5f, H * 0.5f - 6.0f, col::dim (0.3f), 8.0f, juce::Justification::centred, 220.0f);
        scanlines (g, getLocalBounds().toFloat().reduced (1.0f));
        return;
    }

    // The texture's own tape: the whole cloud sways with its wow and dims when it drops out.
    const float sway = std::round (eng.getTapeWow (0) * 9.0f), drop = juce::jlimit (0.0f, 1.0f, eng.getTapeDrop (0));
    const float smear = disp (txSmear), space = disp (txSpace), tape = disp (txTape);
    const float layer = (running ? 1.0f : 0.3f) * (on ? 1.0f : 0.25f) * (1.0f - 0.65f * drop);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (4));
    // The room behind the cloud: a faint dither that thickens with space and smear.
    dither (g, getLocalBounds().toFloat().reduced (4.0f), space + smear > 1.0f ? 0.25f : 0.12f, (0.04f + 0.1f * space) * layer, std::fmod (time * 6.0f, 4.0f));
    if (layer < 0.999f) g.beginTransparencyLayer (layer);
    for (auto& d : dots)
    {
        const float t = d.age / d.life;
        // Forward grains arrive and fade; reversed ones swell in and vanish.
        float env = t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f;
        if (d.rev) env = t < 0.8f ? t / 0.8f : 1.0f - (t - 0.8f) / 0.2f;
        env = env * env * (3.0f - 2.0f * env);
        const float dx = std::round (left + (d.u + d.vx * d.age) * w + sway), dy = std::round (floor - d.v * (floor - top));
        const float a = juce::jlimit (0.0f, 1.0f, d.alpha * env * 1.2f);
        if (d.type == evGlitch) { g.setColour (col::bright.withAlpha (a)); g.fillRect (dx - 3.0f, dy, 6.0f, 1.0f); continue; }
        // A square pixel: bigger for longer grains, and wider the more the cloud is smeared.
        const float px = d.type == evCloud ? juce::jlimit (2.0f, 5.0f, std::round (d.r * (0.9f + 0.5f * smear))) : (d.type == evDust ? 1.0f : 2.0f);
        g.setColour (col::bright.withAlpha (a));
        g.fillRect (dx, dy, px, px);
        if (d.type == evCloud && std::abs (d.vx) > 0.004f)
        {
            // Its trail: a few fainter pixels behind it, the way it came.
            const float dir = d.vx > 0.0f ? -1.0f : 1.0f;
            for (int k = 1; k <= 3; ++k)
            {
                g.setColour (col::bright.withAlpha (a * 0.45f / (float) k));
                g.fillRect (dx + dir * (float) k * (px + 2.0f), dy, px, px);
            }
        }
    }
    if (running && tape > 0.2f)
    {
        // Worn tape: a little dust across the picture.
        juce::Random r ((juce::int64) (time * 14.0f));
        const int specks = (int) (tape * tape * 20.0f);
        for (int i = 0; i < specks; ++i)
        {
            g.setColour (col::bright.withAlpha (0.08f + 0.2f * r.nextFloat()));
            g.fillRect (std::round (left + r.nextFloat() * w), std::round (top + r.nextFloat() * (floor - top)), 1.0f, 1.0f);
        }
    }
    if (layer < 0.999f) g.endTransparencyLayer();
    g.restoreState();
    scanlines (g, getLocalBounds().toFloat().reduced (1.0f));
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
    const float top = 12.0f, floor = H - 16.0f, xA = 12.0f, xB = W - 46.0f;
    if (proc.getSlotSourceId (slotTone) == "none" && proc.getSlotSourceId (slotBed) == "none")
    {
        label (g, "pick a sound below", W * 0.5f, H * 0.5f - 6.0f, col::dim (0.3f), 8.0f, juce::Justification::centred, 220.0f);
        scanlines (g, getLocalBounds().toFloat().reduced (1.0f));
        return;
    }
    const float drop = juce::jlimit (0.0f, 1.0f, eng.getTapeDrop (1)), wow = eng.getTapeWow (1);
    const float tape = disp (drTape), space = disp (drSpace);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (4));
    if (! running) g.beginTransparencyLayer (0.3f);
    const float phase = std::fmod (time * 5.0f, 4.0f);

    // The bed: a dithered floor, as thick as it is loud.
    if (proc.getSlotSourceId (slotBed) != "none")
    {
        const float bed = disp (drBed) * (bedOn ? 1.0f : 0.2f);
        dither (g, { 4.0f, H - 26.0f, W - 8.0f, 22.0f }, bed > 0.5f ? 0.25f : 0.12f, 0.15f + 0.35f * bed, phase);
    }
    // Merge: the texture coming in from the strip next door, as a dithered edge on the left.
    const float merge = disp (txFeed) * juce::jmin (1.0f, eng.getEventRate() / 4.0f);
    if (merge > 0.02f) dither (g, { 4.0f, 4.0f, 60.0f, H - 8.0f }, 0.12f, 0.35f * merge, phase);

    const float layer = (on ? 1.0f : 0.25f) * (1.0f - 0.65f * drop);
    if (layer < 0.999f) g.beginTransparencyLayer (layer);
    struct Label { float y; juce::String n; float a; };
    std::vector<Label> labels;
    auto voiceLine = [&] (int v, float hz, float a)
    {
        if (a < 0.01f) return;
        const float midi = 69.0f + 12.0f * std::log2 (juce::jmax (hz, 1.0f) / 440.0f);
        // The drones' own tape: every line wavers a little, each its own way.
        const float waver = wow * (1.5f + 3.0f * tape) * (v % 2 == 0 ? 1.0f : -0.7f);
        const float vy = std::round (juce::jlimit (top, floor, juce::jmap (midi, 26.0f, 88.0f, floor, top + 2.0f) + waver));
        // A dithered band round each voice that breathes: wider when it's loud, and the more of it goes into the room.
        const float breath = 0.75f + 0.25f * std::sin (time * 0.9f + (float) v * 1.7f);
        const float band = std::round ((4.0f + 8.0f * a) * (0.7f + 0.8f * space));
        dither (g, { xA, vy - band, xB - xA, band * 2.0f + 2.0f }, a > 0.5f ? 0.5f : 0.25f, (0.1f + 0.3f * a) * breath, phase + (float) v);
        // The voice itself: a pixel line, dashed for the melody, its ends broken into dither.
        g.setColour (col::bright.withAlpha (juce::jlimit (0.0f, 1.0f, 0.25f + 0.75f * a)));
        const float th = a > 0.5f ? 2.0f : 1.0f;
        if (v == Engine::lineVoice)
            for (float x = xA + 16.0f; x < xB - 16.0f; x += 6.0f) g.fillRect (x, vy, 4.0f, th);
        else g.fillRect (xA + 16.0f, vy, xB - xA - 32.0f, th);
        dither (g, { xA, vy, 16.0f, th }, 0.5f, 0.7f * a);
        dither (g, { xB - 16.0f, vy, 16.0f, th }, 0.5f, 0.7f * a);
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
    if (layer < 0.999f) g.endTransparencyLayer();
    // Each voice's note in a small box at the end of its line.
    std::sort (labels.begin(), labels.end(), [] (const Label& l1, const Label& l2) { return l1.y < l2.y; });
    float lastY = -100.0f;
    for (auto& l : labels)
        if (l.y - lastY >= 15.0f)
        {
            const auto r = juce::Rectangle<float> (xB + 6.0f, l.y - 7.0f, 34.0f, 14.0f);
            g.setColour (col::bg);
            g.fillRect (r);
            g.setColour (juce::Colour (0xff5A5A5A).withMultipliedAlpha (0.4f + 0.6f * l.a));
            g.drawRect (r, 1.0f);
            text (g, l.n, r.getCentreX(), r.getY() - 2.0f, col::bright.withAlpha (0.4f + 0.6f * l.a), 10.0f, juce::Justification::centred, r.getWidth());
            lastY = l.y;
        }
    if (! running) g.endTransparencyLayer();
    g.restoreState();
    scanlines (g, getLocalBounds().toFloat().reduced (1.0f));
}

OutputPicture::OutputPicture (GrainFieldProcessor& p) : Picture (p)
{
    setHint (*this, "output", "The hand-over: grains cross from the texture into the drones as far as merge lets them. The square rings are the room, the core is how loud it is with the echo rising off it, and the meter on the right is the level going out");
}

void OutputPicture::advance (float dt)
{
    // The meter follows the level, and its peak holds for a moment before it falls back.
    const float lv = juce::jlimit (0.0f, 1.0f, proc.engine.getLevel() * 4.0f) * (proc.engine.isRunning() ? 1.0f : 0.0f);
    meter = lv > meter ? lv : meter + (lv - meter) * juce::jmin (1.0f, dt * 6.0f);
    if (meter >= peak) { peak = meter; peakAge = 0.0f; }
    else if ((peakAge += dt) > 1.2f) peak = juce::jmax (meter, peak - dt * 0.5f);
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
    const float merge = disp (txFeed) * juce::jmin (1.0f, eng.getEventRate() / 4.0f);
    const float meterW = 8.0f, mx = W - 10.0f - meterW;          // the meter, along the right edge
    const float cx = std::round ((W - meterW - 8.0f) * 0.5f), cy = std::round (H * 0.38f);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (4));
    if (! running) g.beginTransparencyLayer (0.35f);
    const float phase = std::fmod (time * 5.0f, 4.0f);

    // The room: square rings of dither opening out from the core, as big as the room, as bright as the space.
    const float maxR = juce::jmin (cx - 10.0f, cy - 10.0f), radius = maxR * (0.45f + 0.55f * size);
    const float ringA = (0.15f + 0.45f * space) * (outside ? 0.45f : 1.0f);
    for (int i = 0; i < 3; ++i)
    {
        float ph = time * (0.11f / (0.5f + size)) + (float) i / 3.0f;
        ph -= std::floor (ph);
        const float rr = std::round (8.0f + ph * (radius - 8.0f));
        const float a = ringA * (1.0f - ph) * juce::jmin (1.0f, ph * 6.0f);
        const float th = 3.0f;
        dither (g, { cx - rr, cy - rr, rr * 2.0f, th }, 0.5f, a, phase);
        dither (g, { cx - rr, cy + rr - th, rr * 2.0f, th }, 0.5f, a, phase);
        dither (g, { cx - rr, cy - rr, th, rr * 2.0f }, 0.5f, a, phase);
        dither (g, { cx + rr - th, cy - rr, th, rr * 2.0f }, 0.5f, a, phase);
    }

    // The echo: repeats rising off the core, fainter each time.
    const int echoes = juce::roundToInt (echo * 5.0f);
    for (int i = 1; i <= echoes; ++i)
    {
        const float rise = std::fmod (time * 9.0f, 12.0f);
        g.setColour (col::bright.withAlpha ((0.25f + 0.5f * level) * std::pow (0.62f, (float) i)));
        g.fillRect (cx - 2.0f, cy - 10.0f - (float) i * 12.0f - rise, 4.0f, 4.0f);
    }

    // The core: everything together, bigger when it's loud, with a dithered halo that grows with drive.
    const float core = std::round (6.0f + 12.0f * level), halo = std::round (core + 6.0f + 14.0f * drive);
    dither (g, { cx - halo * 0.5f, cy - halo * 0.5f, halo, halo }, drive > 0.5f ? 0.5f : 0.25f, 0.2f + 0.5f * level, phase);
    g.setColour (col::bright.withAlpha (0.5f + 0.5f * level));
    g.fillRect (cx - core * 0.5f, cy - core * 0.5f, core, core);

    // Grains crossing from the texture (left) to the drones (right), sinking toward the drones' floor.
    const int crossing = juce::roundToInt (merge * 14.0f);
    const float lane = mx - 8.0f;
    for (int i = 0; i < crossing; ++i)
    {
        const float sp = 18.0f + 9.0f * (float) (i % 4);
        const float x = std::fmod ((float) i * 23.0f + time * sp, lane);
        const float y0 = 20.0f + std::fmod ((float) i * 37.0f, H * 0.5f);
        const float u = x / lane, y = std::round (y0 + (H - 40.0f - y0) * u * u);
        for (int k = 0; k < 3; ++k)
        {
            g.setColour (col::bright.withAlpha (k == 0 ? 0.85f : 0.35f / (float) k));
            g.fillRect (std::round (6.0f + x) - (float) k * 4.0f, y, 2.0f, 2.0f);
        }
    }
    if (! running) g.endTransparencyLayer();
    g.restoreState();

    // The meter: stacked cells; the top few turn red near clipping, and the peak holds.
    const int cells = juce::jmax (8, (int) ((H - 40.0f) / 5.0f));
    const int on = juce::roundToInt (meter * (float) cells), pk = juce::roundToInt (peak * (float) cells);
    for (int c = 0; c < cells; ++c)
    {
        const bool lit = c < on, isPeak = c == juce::jmin (cells - 1, pk) && peak > 0.02f;
        const bool hot = c >= cells - 3;
        g.setColour (lit ? (hot ? col::rec : col::ink) : (isPeak ? juce::Colour (0xff8A8A8A) : col::faint));
        g.fillRect (mx, H - 22.0f - (float) c * 5.0f, meterW, 3.0f);
    }
    label (g, "out", mx + meterW * 0.5f, H - 17.0f, col::dim (0.5f), 7.0f, juce::Justification::centred, 30.0f);
    label (g, outside ? "room to send" : "texture > drones", (W - meterW) * 0.5f, H - 17.0f, col::dim (0.45f), 7.0f, juce::Justification::centred, W - meterW - 16.0f);
    scanlines (g, getLocalBounds().toFloat().reduced (1.0f));
}

} // namespace gf::ui
