#pragma once
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Widgets.h"
#include "UI/Views.h"

// One screen, built like a rack: a transport bar with the chords and the root, the scenes under it, then
// three modules side by side (texture, drones, output), each with its picture on top, its sounds under it
// and its controls along the bottom, and Swell underneath. Nothing here is a timeline: the DAW decides when.
class GrainFieldEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit GrainFieldEditor (GrainFieldProcessor&);
    ~GrainFieldEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent& e) override { mouseMove (e); }
    void mouseExit (const juce::MouseEvent&) override;

    // The smallest layout, at 100%. A bigger window scales it up; a wider or taller one stretches the modules.
    static constexpr int baseW = 1240, baseH = 840;

private:
    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void layout();

    struct Canvas : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };
    void updateHint (const juce::MouseEvent&);
    void repaintFooter();
    void presetMenu();
    void blendMenu();
    void sizeMenu();
    void fadeMenu();
    void tempoMenu();
    gf::ui::Knob& knob (int sceneParam, const juce::String& label);

    GrainFieldProcessor& proc;
    gf::ui::LookAndFeel lnf;
    Canvas canvas;

    gf::ui::ScenesView scenesView;
    gf::ui::HarmonyView harmony;
    gf::ui::SourcesView textureSources, droneSources;
    gf::ui::TexturePicture texturePicture;
    gf::ui::DronesPicture dronesPicture;
    gf::ui::OutputPicture outputPicture;
    gf::ui::SwellLine swell;
    gf::ui::PlayButton play;
    gf::ui::TextToggle sync, freeze;
    gf::ui::SpaceModeSwitch spaceMode;
    gf::ui::HairSlider textureLevel, dronesLevel;
    gf::ui::Knob volume;
    gf::ui::TakeChip take;
    gf::ui::TextAction prevPreset, nextPreset, presetName, resample, blendTime, doneEditing, sizeButton, rootButton, rootNote, textureNote, clock, loopButton, tempoButton, fadeTime, record;
    gf::ui::InlineEditor presetSaver;
    juce::OwnedArray<gf::ui::Knob> knobs;   // line and dissolve, eight for texture, eight for drones, three for the output
    gf::ui::StageLabel textureStage, dronesStage, bedStage;
    std::array<gf::VisEvent, 256> events;

    juce::String hintLabel, hintText, statusText, lastStatus;
    double statusUntil = 0.0, lastTick = 0.0;
    bool sized = false;

    // The layout, in the canvas's own units.
    float W = (float) baseW, H = (float) baseH, scale = 1.0f;
    enum { fHeader, fTransport, fScenes, fTexture, fDrones, fTape, fSwell, fFooter, numFrames };
    std::array<juce::Rectangle<float>, numFrames> frames;
    float outTitleX = 0.0f, titleY = 0.0f, seamY = 0.0f;
    juce::Rectangle<float> seam1, seam2;   // the gaps between the pictures, where one module hands over to the next
    int hoverFrame = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFieldEditor)
};
