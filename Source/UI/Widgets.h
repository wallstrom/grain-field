#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../PluginProcessor.h"

namespace gf::ui
{
// Base for every control bound to a parameter. Controls stored in parts (sceneParam >= 0)
// write to the next part instead of the sound while you're editing it.
class ParamControl : public juce::Component
{
public:
    ParamControl (GrainFieldProcessor& p, int sceneParam);              // a control stored in parts
    ParamControl (GrainFieldProcessor& p, const juce::String& paramId); // a control outside parts
    virtual void tick();

protected:
    bool editingNext() const { return sceneParam >= 0 && proc.isEditingNext(); }
    float getNorm() const;
    float getDisplayNorm() const;   // where the sound really is, after Swell and blends
    float getDefaultNorm() const { return param->getDefaultValue(); }
    void beginEdit();
    void setNorm (float n);
    void endEdit();
    juce::String valueText() const;   // for the norm shown now
    juce::String valueTextFor (float norm) const;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    // How a drag maps to the value: vertical for knobs, horizontal for lines.
    virtual float dragToNorm (const juce::MouseEvent& e, float startNorm);

    GrainFieldProcessor& proc;
    int sceneParam = -1;
    juce::RangedAudioParameter* param = nullptr;
    float dragStartNorm = 0.0f;
    bool hover = false, dragging = false;
    bool ignoreDrag = false;   // after a double-click, until the button comes up

private:
    float shownNorm = -1.0f, shownDisplay = -1.0f;
    bool shownEditing = false;
};

// A pixel knob in its own cell, with its name above and its value below (or beside it, when horizontal).
// Controls Swell moves are drawn at full brightness; while the sound is elsewhere, the tip blinks.
class Knob : public ParamControl
{
public:
    Knob (GrainFieldProcessor& p, int sceneParam, const juce::String& label);
    Knob (GrainFieldProcessor& p, const juce::String& paramId, const juce::String& label);   // a control outside parts
    void paint (juce::Graphics&) override;
    static constexpr int width = 60, height = 66;
    bool horizontal = false;

private:
    juce::String name;
};

// A level as a row of square segments: the module levels and the bed.
class HairSlider : public ParamControl
{
public:
    HairSlider (GrainFieldProcessor& p, int sceneParam, const juce::String& caption);
    HairSlider (GrainFieldProcessor& p, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;

private:
    float dragToNorm (const juce::MouseEvent& e, float startNorm) override;
    juce::Rectangle<float> track() const;
    juce::String caption;
};

// A word and a number you drag up or down, such as "feed 35".
class DragNumber : public ParamControl
{
public:
    DragNumber (GrainFieldProcessor& p, int sceneParam, const juce::String& word);
    void paint (juce::Graphics&) override;

private:
    juce::String word;
};

// Swell: a long line from quiet to wall. Drag the handle to move it yourself; click anywhere on the
// line and it travels there over the fade time. Small marks show where the other parts sit.
class SwellLine : public ParamControl, public HintSource
{
public:
    explicit SwellLine (GrainFieldProcessor& p);
    void paint (juce::Graphics&) override;
    void tick() override;
    std::pair<juce::String, juce::String> hintAt (juce::Point<int>) override;
    // Cells: quiet, the bar, wall, then the value.
    static constexpr float cellW = 64.0f, lineX = cellW + 12.0f, lineYpos = 21.0f;
    float lineW() const { return (float) getWidth() - lineX - 12.0f - 8.0f - cellW - 64.0f; }

private:
    enum Zone { nowhere, onLine, onQuiet, onWall };
    Zone zoneAt (juce::Point<float>) const;
    float dragToNorm (const juce::MouseEvent& e, float startNorm) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::array<float, numScenes> marks {};
    Zone pressZone = nowhere, hoverZone = nowhere;
    int shownFade = -1;
};

// A button cell that switches an on/off parameter: sync, freeze. Lit when on.
class TextToggle : public juce::Component
{
public:
    TextToggle (GrainFieldProcessor& p, const juce::String& paramId, const juce::String& word, bool accentWhenOn);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void tick();

private:
    juce::RangedAudioParameter* param;
    juce::String word;
    bool accentWhenOn, shown = false;
};

// inside / outside, as two cells
class SpaceModeSwitch : public juce::Component
{
public:
    explicit SpaceModeSwitch (GrainFieldProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();

private:
    juce::RangedAudioParameter* param;
    int shown = -1;
};

// The play button: lit while playing.
class PlayButton : public juce::Component
{
public:
    explicit PlayButton (GrainFieldProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void tick();

private:
    GrainFieldProcessor& proc;
    juce::RangedAudioParameter* param;
    bool shownOn = false, shownRunning = false;
};

// Solo and mute for one stage (texture, drones, bed), as two button cells.
class StageLabel : public juce::Component
{
public:
    StageLabel (GrainFieldProcessor& p, int stage, const juce::String& word, bool rightAligned);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    void tick();

private:
    enum { onWord, onSolo, onMute };
    void layout (float& xWord, float& xSolo, float& xMute) const;
    int zoneAt (float x) const;
    GrainFieldProcessor& proc;
    int stage;
    juce::String word;
    bool right;
    int hover = -1, shown = -99;
};

// The recording: its running time while it is being made, then the take, which you drag onto an audio track.
class TakeChip : public juce::Component, public HintSource
{
public:
    explicit TakeChip (GrainFieldProcessor& p) : proc (p) {}
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool hitTest (int, int) override { return proc.isRecording() || proc.getLastTake().existsAsFile(); }
    std::pair<juce::String, juce::String> hintAt (juce::Point<int>) override;

private:
    GrainFieldProcessor& proc;
    bool draggingOut = false;
    juce::String shown;
};

// Clickable text drawn by a function, for the few one-off actions: lock this, move on, done.
class TextAction : public juce::Component
{
public:
    std::function<void (juce::Graphics&, bool hover)> draw;
    std::function<void (const juce::MouseEvent&)> onClick;
    void paint (juce::Graphics& g) override { if (draw) draw (g, isMouseOverOrDragging()); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition())) onClick (e);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};
} // namespace gf::ui
