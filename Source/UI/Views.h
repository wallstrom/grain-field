#pragma once
#include "Widgets.h"

namespace gf::ui
{
// A one-line text box that appears over a label to rename something, then disappears.
class InlineEditor : public juce::TextEditor
{
public:
    InlineEditor();
    void start (juce::Component& parent, juce::Rectangle<int> area, const juce::String& initial, std::function<void (const juce::String&)> done);

private:
    void finish (bool commit);
    std::function<void (const juce::String&)> onDone;
};

// Scenes: whole sounds you keep and blend between, as a row of chips. Click one to blend into it,
// double-click to rename, right-click to remove. The last chip keeps what you hear as a new scene.
class ScenesView : public juce::Component, public HintSource
{
public:
    explicit ScenesView (GrainFieldProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override { layout(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = onNothing; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::pair<juce::String, juce::String> hintAt (juce::Point<int>) override;

    static constexpr float chipH = 38.0f;
    juce::Rectangle<float> chipRect (int part) const;
    juce::Rectangle<float> addChipRect() const { return addRect; }

private:
    enum { onNothing = -1, onAdd = -2 };
    struct Chip { int part; juce::Rectangle<float> r; };
    void layout();
    int itemAt (juce::Point<float>) const;
    void showMenu (int part);
    void rename (int part);
    juce::String sceneName (int part) const;

    GrainFieldProcessor& proc;
    std::vector<Chip> chips;
    juce::Rectangle<float> addRect;
    int hover = onNothing;
    juce::String shown;
    InlineEditor renamer;
};

// Harmony: eight chord slots. Empty ones are off until you set them up. Click one to queue it,
// drag to reorder, and the small pencil (or clicking the playing one) opens its editor.
class HarmonyView : public juce::Component, public HintSource
{
public:
    explicit HarmonyView (GrainFieldProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = onNothing; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::pair<juce::String, juce::String> hintAt (juce::Point<int>) override;

    // Used by the chord editor.
    Progression shownProg() const;
    int keyShown() const;
    int playingIndex() const;
    void apply (const Progression& p, int playingNowAt = -1);
    void openEditor (int index);
    // The root: every chord, and every layer that follows it, is counted from it. Shown at the top of the window.
    void stepKey (int delta) { setKey (keyShown() + delta); }
    void keyMenu (juce::Component& target, juce::Rectangle<int> areaInTarget = {});
    // When a chord's bars are done: stay on it, or go round the loop.
    bool staying() const;
    void setStaying (bool on);

    // The eight slots fill the bay edge to edge, divided by rules. Above them is a strip where the top border runs,
    // which holds the chord played on a MIDI keyboard (and keep / let go) when there is one.
    static constexpr float topPad = 8.0f, gap = 0.0f;
    float slotW() const { return (float) getWidth() / 8.0f; }
    juce::Rectangle<float> durationCellsRect (int index) const
    {
        const auto r = slotRect (index);
        return { r.getX() + 7.0f, r.getBottom() - 24.0f, 110.0f, 4.0f };
    }
    juce::Rectangle<float> actionRowRect (int index) const
    {
        const auto r = slotRect (index);
        return { r.getX(), r.getBottom() - 14.0f, r.getWidth(), 10.0f };
    }

private:
    enum { onKey = -2, onStay = -3, onPlayOn = -4, onKeyDown = -5, onKeyUp = -6, onKeep = -7, onLetGo = -8, onNothing = -9, onEditBase = 100, onNowBase = 200, onLoopBase = 300 };
    void setKey (int key);
    // The chord being played on a MIDI keyboard, and where its name, keep and let go sit in the header.
    bool keysLayout (juce::String& name, float& xName, float& xKeep, float& xLetGo) const;
    float keyNameX() const;
    juce::Rectangle<float> slotRect (int i) const { return { std::round (slotW() * (float) i), topPad + 1.0f, std::round (slotW() * (float) (i + 1)) - std::round (slotW() * (float) i), (float) getHeight() - topPad - 2.0f }; }
    int itemAt (juce::Point<float>) const;
    int slotAt (float x) const;

    GrainFieldProcessor& proc;
    int hover = onNothing;
    int pressed = -1;
    bool dragging = false;
    float dragX = 0.0f, grabOffset = 0.0f;
    juce::String shownState;
};

// The small editor a chord slot opens (also used by the snapshot tool).
std::unique_ptr<juce::Component> makeChordEditor (HarmonyView& owner, int index);

// The sounds a module is made from, listed inside that module, each with its waveform.
// Texture: its three sounds, each with the window its grains are cut from (drag it) and an amount.
// Drones: the sound they grow from and the bed underneath, with where each is being read.
class SourcesView : public juce::Component, public HintSource
{
public:
    enum class Mode { texture, drones };
    SourcesView (GrainFieldProcessor& p, Mode m);
    void tick (float dt);
    void addEvents (const VisEvent* ev, int n);   // grains, to flash where they were cut from
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; hoverStrip = false; hoverNote = false; hoverPlays = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::pair<juce::String, juce::String> hintAt (juce::Point<int>) override;
    void noteMenu (int row);   // say what note one of your own sounds is

    // One recording for everything: its menu, and what to show for it.
    void rootMenu (juce::Component& target);
    juce::String rootLabel (bool& isSet) const;

    static constexpr int rowH = 26;
    int numRows() const { return mode == Mode::texture ? 3 : 2; }
    // Where each sound plays: a small column between its name and its waveform.
    float playsX() const { return nameX() + nameW() + 8.0f; }
    static constexpr float playsW = 62.0f;
    float stripX() const { return playsX() + playsW + 6.0f; }
    juce::Rectangle<float> playsRect (int row) const { return { playsX() - 3.0f, (float) (row * rowH), playsW + 4.0f, 20.0f }; }
    // The note parameter a row sets: each texture sound's, the drones', the bed's.
    int notePOf (int row) const { return mode == Mode::texture ? txNote1 + row : (row == 0 ? drNote : bedNote); }
    void playsMenu (int row);
    // The menu for any note parameter: where that layer plays.
    void noteStepMenu (int p, juce::Component& target, juce::Rectangle<int> areaInTarget);
    // A note parameter as shown: the part being shaped, or what is playing.
    int shownChoice (int p) const;
    void setChoice (int p, int index);
    // Where a layer ends up, as a note name ("a"), and what its own step says ("+5th"), for the module's header too.
    juce::String playsNote (int p, int index) const;
    juce::Rectangle<float> stripRect (int row) const;

private:
    struct Flash { int slot; float x, span, age; };
    int slotOf (int row) const { return mode == Mode::texture ? row : (row == 0 ? slotTone : slotBed); }
    float nameX() const { return mode == Mode::texture ? 8.0f : 52.0f; }
    float nameW() const { return juce::jlimit (84.0f, 150.0f, (float) getWidth() * 0.27f) - (mode == Mode::texture ? 0.0f : 30.0f); }
    float sliderX() const { return (float) getWidth() - 84.0f; }
    int rowAt (juce::Point<float>, bool& onStrip) const;
    juce::String nameOf (int slot) const;
    void showMenu (int row);
    bool onNoteChip (juce::Point<float> p, int row) const { return row >= 0 && row < 3 && ! chipRects[(size_t) row].isEmpty() && chipRects[(size_t) row].contains (p); }
    void setWindow (int row, float pos, bool all, float delta);
    float windowHalf() const;

    GrainFieldProcessor& proc;
    Mode mode;
    std::unique_ptr<HairSlider> sliders[3];
    std::array<float, numSlots> cores {};
    std::array<bool, numSlots> canSetNote {};
    std::array<int, numSlots> noteChoice {};
    std::array<juce::Rectangle<float>, 3> chipRects;   // where each row's note sits, as last drawn
    std::vector<Flash> flashes;
    int hover = -1, dragRow = -1;
    bool hoverStrip = false, hoverNote = false, hoverPlays = false, dragAll = false;
    std::array<juce::String, 3> playsShown;
    std::array<float, 3> playsFlash {};   // a moment of colour when where a sound plays changes
    std::array<float, 3> dragStart {};
    float dragStartX = 0.0f;
    juce::String shown;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Each module's picture of what it is doing right now, in its own dark panel.
class Picture : public juce::Component
{
public:
    explicit Picture (GrainFieldProcessor& p) : proc (p) {}
    void tick (float dt) { time += dt; advance (dt); repaint(); }

protected:
    virtual void advance (float) {}
    void panel (juce::Graphics&) const;
    float disp (int p) const { return proc.engine.getDisplayNorm (p); }
    static float pitchToV (float midi) { return juce::jlimit (0.0f, 1.0f, (midi - 26.0f) / 62.0f); }
    GrainFieldProcessor& proc;
    float time = 0.0f;
};

// Texture: every grain a pixel particle with a short trail, left to right where you hear it, low to high by its note.
class TexturePicture : public Picture
{
public:
    explicit TexturePicture (GrainFieldProcessor& p);
    void addEvents (const VisEvent* ev, int n);
    void paint (juce::Graphics&) override;

private:
    void advance (float dt) override;
    struct Dot { float u, v, r, age, life, alpha, vx; int type; bool rev; };
    std::vector<Dot> dots;
    juce::Random rng { 7 };
};

// Drones: each voice a line at its pitch inside a dithered band that breathes, as bright as it is loud.
// When the chord or root changes, the old notes fade as the new ones come up beside them.
class DronesPicture : public Picture
{
public:
    explicit DronesPicture (GrainFieldProcessor& p);
    void paint (juce::Graphics&) override;
};

// Output, the strip between texture and drones: grains crossing from one to the other (as many as merge lets
// through), the room as dithered square rings, the level as a core with the echo rising off it, and a pixel meter.
class OutputPicture : public Picture
{
public:
    explicit OutputPicture (GrainFieldProcessor& p);
    void paint (juce::Graphics&) override;
    // The input is smoothed linear RMS, not a sample peak. Animation uses its own scaling.
    static constexpr float meterFloorDb = -60.0f, meterHotDb = -1.0f;
    static float rmsToMeter (float rms)
    {
        const float db = juce::Decibels::gainToDecibels (rms, meterFloorDb);
        return juce::jlimit (0.0f, 1.0f, (db - meterFloorDb) / -meterFloorDb);
    }

private:
    void advance (float dt) override;
    float meter = 0.0f, peak = 0.0f, peakAge = 0.0f;
};
} // namespace gf::ui
