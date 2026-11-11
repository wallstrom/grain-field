#include "PluginEditor.h"

using namespace gf;
using namespace gf::ui;

// A small label at the top of a cell, and a value under it.
static void cellCaption (juce::Graphics& g, const juce::String& word, juce::Colour c = col::dim (0.5f))
{
    label (g, word, 8.0f, 6.0f, c, 7.0f);
}

GrainFieldEditor::GrainFieldEditor (GrainFieldProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      scenesView (p), harmony (p), textureSources (p, SourcesView::Mode::texture), droneSources (p, SourcesView::Mode::drones),
      texturePicture (p), dronesPicture (p), outputPicture (p), swell (p), play (p),
      sync (p, ids::sync, "sync", false), freeze (p, ids::freeze, "freeze", true),
      spaceMode (p),
      textureLevel (p, txLevel, "level"), dronesLevel (p, drLevel, "level"), volume (p, ids::volume, "volume"), take (p),
      textureStage (p, gf::Engine::stTexture, "texture", false), dronesStage (p, gf::Engine::stDrones, "drones", false),
      bedStage (p, gf::Engine::stBed, "", true)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    // Header: the name, the root sound, the pieces, the size. Every control is a cell of its own.
    rootButton.draw = [this] (juce::Graphics& g, bool over)
    {
        bool isSet = false;
        const auto name = textureSources.rootLabel (isSet);
        raised (g, rootButton.getLocalBounds().toFloat(), false, over);
        label (g, "root sound", 10.0f, (float) rootButton.getHeight() * 0.5f - 6.0f, col::dim (0.5f), 7.0f);
        text (g, isSet ? name.fromFirstOccurrenceOf (": ", false, false) : juce::String ("none"), 100.0f, (float) rootButton.getHeight() * 0.5f - 9.0f,
              over ? col::bright : (isSet ? col::ink : col::dim (0.45f)), 12.0f, juce::Justification::centredLeft, (float) rootButton.getWidth() - 106.0f);
    };
    rootButton.onClick = [this] (const juce::MouseEvent&) { textureSources.rootMenu (rootButton); };
    setHint (rootButton, "root sound", "One recording for everything: it becomes every module's sound, and the root moves to its core note");
    // The root note: the whole piece is counted from it. Every layer that follows it moves with it, and the chords too.
    rootNote.draw = [this] (juce::Graphics& g, bool)
    {
        const float w = (float) rootNote.getWidth(), h = (float) rootNote.getHeight();
        const auto pos = rootNote.getMouseXYRelative();
        const bool over = rootNote.isMouseOver();
        const int zone = ! over ? -1 : (pos.x < 34 ? 0 : (pos.x >= (int) w - 34 ? 2 : 1));
        raised (g, { 0.0f, 0.0f, 34.0f, h }, false, zone == 0);
        raised (g, { w - 34.0f, 0.0f, 34.0f, h }, false, zone == 2);
        chevronPx (g, 17.0f, h * 0.5f, -1.0f, zone == 0 ? col::bright : col::dim (0.7f));
        chevronPx (g, w - 17.0f, h * 0.5f, 1.0f, zone == 2 ? col::bright : col::dim (0.7f));
        const int k = harmony.keyShown();
        text (g, k >= 0 ? juce::String (keyNames[k]).toLowerCase() : juce::String(), w * 0.5f, h * 0.5f - 21.0f, zone == 1 ? col::bright : col::bright.withAlpha (0.92f),
              28.0f, juce::Justification::centred, w - 70.0f);
    };
    rootNote.onClick = [this] (const juce::MouseEvent& e)
    {
        if (e.x < 34) harmony.stepKey (-1);
        else if (e.x >= rootNote.getWidth() - 34) harmony.stepKey (1);
        else harmony.keyMenu (rootNote);
        rootNote.repaint();
    };
    setHint (rootNote, "root note", "The note the whole piece is counted from. Every layer that follows it moves with it, keeping its interval, and so do the chords. Sounds left at their own pitch stay. The arrows step a semitone; click the note for the list");
    prevPreset.draw = [this] (juce::Graphics& g, bool over)
    {
        raised (g, prevPreset.getLocalBounds().toFloat(), false, over);
        chevronPx (g, (float) prevPreset.getWidth() * 0.5f, (float) prevPreset.getHeight() * 0.5f, -1.0f, over ? col::bright : col::dim (0.7f));
    };
    nextPreset.draw = [this] (juce::Graphics& g, bool over)
    {
        raised (g, nextPreset.getLocalBounds().toFloat(), false, over);
        chevronPx (g, (float) nextPreset.getWidth() * 0.5f, (float) nextPreset.getHeight() * 0.5f, 1.0f, over ? col::bright : col::dim (0.7f));
    };
    presetName.draw = [this] (juce::Graphics& g, bool over)
    {
        lcd (g, presetName.getLocalBounds().toFloat());
        label (g, proc.getPresetName(), (float) presetName.getWidth() * 0.5f, (float) presetName.getHeight() * 0.5f - 7.0f,
               over ? col::bright : col::ink, 10.0f, juce::Justification::centred, (float) presetName.getWidth() - 12.0f);
    };
    prevPreset.onClick = [this] (const juce::MouseEvent&) { proc.stepPreset (-1); };
    nextPreset.onClick = [this] (const juce::MouseEvent&) { proc.stepPreset (1); };
    presetName.onClick = [this] (const juce::MouseEvent&) { presetMenu(); };
    sizeButton.draw = [this] (juce::Graphics& g, bool over)
    {
        raised (g, sizeButton.getLocalBounds().toFloat(), false, over);
        label (g, "size " + juce::String (juce::roundToInt (100.0f * scale)) + "%", (float) sizeButton.getWidth() * 0.5f, (float) sizeButton.getHeight() * 0.5f - 6.0f,
               over ? col::bright : col::dim (0.7f), 8.0f, juce::Justification::centred, (float) sizeButton.getWidth());
    };
    sizeButton.onClick = [this] (const juce::MouseEvent&) { sizeMenu(); };
    setHint (sizeButton, "size", "How big everything is: 100, 150 or 200%, so every pixel stays sharp. Drag the window's corner to make it wider or taller: the modules stretch");
    setHint (prevPreset, "previous piece", "Load the piece before this one");
    setHint (nextPreset, "next piece", "Load the next piece");
    setHint (presetName, "piece", "A whole piece: its sounds, chords and scenes. Click to pick one or save yours");
    setHint (sync, "sync", "Play when your song plays and follow its tempo. Off: Grain Field plays on its own");
    setHint (freeze, "freeze", "Hold the texture where it is. The drones keep breathing");
    setHint (textureLevel, "texture level", "How loud the texture is");
    setHint (dronesLevel, "drones level", "How loud the drones are");
    setHint (volume, "volume", "How loud Grain Field is. The last thing before it leaves");
    setHint (bedStage, "bed", "Solo to hear only the bed underneath, mute to take it out");
    for (auto* c : std::initializer_list<juce::Component*> { &rootButton, &prevPreset, &presetName, &nextPreset, &sizeButton })
        canvas.addAndMakeVisible (c);

    // The transport: play, the clock, record, loop. The root has a bay of its own; the harmony's knobs, tempo and sync another.
    clock.draw = [this] (juce::Graphics& g, bool over)
    {
        const int t = (int) juce::jmax (0.0, proc.engine.getRunSeconds());
        g.setColour (over ? col::raised : col::cell);
        g.fillRect (clock.getLocalBounds());
        cellCaption (g, "clock");
        text (g, juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2), 8.0f, 15.0f,
              over ? col::bright : col::ink, 22.0f);
    };
    clock.onClick = [this] (const juce::MouseEvent&) { proc.rewind(); };
    setHint (clock, "clock", "How long Grain Field has been playing. Click to go back to the first chord, with the clock at zero");
    loopButton.draw = [this] (juce::Graphics& g, bool over)
    {
        // Two pixel arrows chasing each other: lit when the chords go round, dark when each one stays.
        const bool looping = ! harmony.staying();
        const float w = (float) loopButton.getWidth(), h = (float) loopButton.getHeight();
        raised (g, loopButton.getLocalBounds().toFloat(), looping, over);
        const juce::Colour c = looping ? col::bg : (over ? col::bright : col::dim (0.7f));
        g.setColour (c);
        const float x = std::round (w * 0.5f - 8.0f), y = std::round (h * 0.5f - 14.0f);
        g.fillRect (x + 2.0f, y + 2.0f, 12.0f, 2.0f); g.fillRect (x, y + 4.0f, 2.0f, 4.0f);
        g.fillRect (x + 12.0f, y, 2.0f, 2.0f); g.fillRect (x + 12.0f, y + 4.0f, 2.0f, 2.0f);
        g.fillRect (x + 2.0f, y + 12.0f, 12.0f, 2.0f); g.fillRect (x + 14.0f, y + 8.0f, 2.0f, 4.0f);
        g.fillRect (x + 2.0f, y + 10.0f, 2.0f, 2.0f); g.fillRect (x + 2.0f, y + 14.0f, 2.0f, 2.0f);
        label (g, "loop", w * 0.5f, h - 18.0f, c, 7.0f, juce::Justification::centred, w);
    };
    loopButton.onClick = [this] (const juce::MouseEvent&) { harmony.setStaying (! harmony.staying()); loopButton.repaint(); };
    setHint (loopButton, "loop", "Lit: when a chord's bars are done, the next one in the loop plays, round and round. Dark: each chord stays until you queue the next");
    tempoButton.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool host = proc.engine.hostHasTempo();
        g.setColour (over && ! host ? col::raised : col::cell);
        g.fillRect (tempoButton.getLocalBounds());
        cellCaption (g, host ? "tempo · song" : "tempo");
        text (g, juce::String (juce::roundToInt (proc.engine.getBpm())), 8.0f, 15.0f, over && ! host ? col::bright : col::ink, 22.0f);
    };
    tempoButton.onClick = [this] (const juce::MouseEvent&) { tempoMenu(); };
    setHint (tempoButton, "tempo", "How long a bar is, for the chords and the echo. In a DAW it's your song's tempo; on its own, click to set it");
    record.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        const float w = (float) record.getWidth(), h = (float) record.getHeight();
        raised (g, record.getLocalBounds().toFloat(), false, over);
        const float cx = std::round (w * 0.5f), cy = std::round (h * 0.5f - 6.0f);
        g.setColour (col::rec.withAlpha (proc.isRecordingWaiting() ? (blink ? 1.0f : 0.35f) : 1.0f));
        if (proc.isRecording() && ! proc.isRecordingWaiting()) { g.setColour (col::rec.withAlpha (blink ? 1.0f : 0.55f)); g.fillRect (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f); }
        else g.drawRect (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, 2.0f);
        label (g, "rec", w * 0.5f, h - 18.0f, proc.isRecording() ? col::rec : (over ? col::bright : col::dim (0.6f)), 7.0f, juce::Justification::centred, w);
    };
    record.onClick = [this] (const juce::MouseEvent&) { proc.toggleRecording(); };
    setHint (record, "record", "Records everything you hear to a file, from now until you click again. The take then appears beside the scenes: drag it onto an audio track. With sync on, it starts when your song does");
    for (auto* c : std::initializer_list<juce::Component*> { &play, &clock, &record, &loopButton, &rootNote, &tempoButton, &sync })
        canvas.addAndMakeVisible (c);

    // The scenes, with how long a blend takes and the last take.
    blendTime.draw = [this] (juce::Graphics& g, bool over)
    {
        const float secs = proc.apvts.getRawParameterValue (ids::blend)->load();
        const float pr = proc.engine.getBlendProgress();
        const float h = (float) blendTime.getHeight();
        raised (g, blendTime.getLocalBounds().toFloat(), false, over);
        label (g, "blend", 10.0f, h * 0.5f - 6.0f, col::dim (0.55f), 7.0f);
        if (pr >= 0.0f && secs > 0.5f)
            text (g, timeText (secs * (1.0f - pr)), 54.0f, h * 0.5f - 10.0f, col::bright, 13.0f);
        else
            text (g, secs < 0.5f ? juce::String ("cut") : timeText (secs), 54.0f, h * 0.5f - 10.0f, over ? col::bright : col::ink, 13.0f);
    };
    blendTime.onClick = [this] (const juce::MouseEvent&) { blendMenu(); };
    setHint (blendTime, "blend", "How long going into another scene takes, from a cut to three minutes");
    for (auto* c : std::initializer_list<juce::Component*> { &scenesView, &blendTime, &take })
        canvas.addAndMakeVisible (c);

    // Harmony.
    canvas.addAndMakeVisible (harmony);
    knob (hmLine, "line").horizontal = true;
    knob (hmDissolve, "dissolve").horizontal = true;

    // The three modules, each with its picture.
    resample.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool listening = proc.getPendingCaptureSlot() >= 0;
        const bool on = ! listening || (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        const float h = (float) resample.getHeight();
        raised (g, resample.getLocalBounds().toFloat(), false, over);
        g.setColour (col::rec.withAlpha (on ? (over || listening ? 1.0f : 0.75f) : 0.3f));
        g.fillRect (9.0f, std::round (h * 0.5f) - 3.0f, 6.0f, 6.0f);
        label (g, listening ? "listening" : "resample", 21.0f, h * 0.5f - 6.0f, listening || over ? col::bright : col::dim (0.7f), 7.0f);
    };
    resample.onClick = [this] (const juce::MouseEvent&) { proc.resample(); };
    // Where the texture plays, counted from the root. Its sounds move with it.
    textureNote.draw = [this] (juce::Graphics& g, bool over)
    {
        const int idx = textureSources.shownChoice (txNote);
        const float h = (float) textureNote.getHeight();
        raised (g, textureNote.getLocalBounds().toFloat(), false, over);
        arrow (g, 9.0f, h * 0.5f, 8.0f, col::dim (over ? 0.9f : 0.45f));
        const auto note = textureSources.playsNote (txNote, idx);
        text (g, note, 23.0f, h * 0.5f - 10.0f, over ? col::bright : (idx != 0 ? col::bright : col::dim (0.7f)), 13.0f);
        label (g, noteStepName (txNote, idx), 23.0f + textWidth (note, 13.0f) + 6.0f, h * 0.5f - 6.0f, col::dim (over ? 0.8f : 0.5f), 7.0f);
    };
    textureNote.onClick = [this] (const juce::MouseEvent&) { textureSources.noteStepMenu (txNote, textureNote, {}); };
    setHint (textureNote, "where the texture plays", "Counted from the root: on it, a fifth or a fourth above, or an octave up or down. All three sounds move with it, each keeping its own step; the drones and the bed stay");
    setHint (resample, "resample", "Records the last 8 seconds of everything you hear and adds it to the texture as a sound of its own, so the piece can feed on itself. It takes the first empty sound, or the one you use least");
    for (auto* c : std::initializer_list<juce::Component*> { &texturePicture, &dronesPicture, &outputPicture, &textureSources, &droneSources,
                                                              &textureLevel, &dronesLevel, &textureStage, &dronesStage, &bedStage, &resample, &spaceMode, &textureNote })
        canvas.addAndMakeVisible (c);
    knob (txDensity, "density"); knob (txLength, "length"); knob (txSmear, "smear"); knob (txReverse, "reverse");
    knob (txSpread, "spread"); knob (txDetail, "detail"); knob (txTape, "tape"); knob (txSpace, "space");
    knob (drGrow, "grow"); knob (drBody, "body"); knob (drBreath, "breath"); knob (drColour, "colour");
    knob (drSub, "sub"); knob (txFeed, "merge"); knob (drTape, "tape"); knob (drSpace, "space");
    knob (outDrive, "drive"); knob (spEcho, "echo"); knob (spSize, "room");
    canvas.addAndMakeVisible (volume);

    // Swell and the footer.
    canvas.addAndMakeVisible (swell);
    canvas.addAndMakeVisible (freeze);
    fadeTime.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool fading = proc.isSwellFading();
        const float w = (float) fadeTime.getWidth(), h = (float) fadeTime.getHeight();
        raised (g, fadeTime.getLocalBounds().toFloat(), fading, over);
        label (g, "fade " + timeText (proc.getSwellFadeSeconds()), w * 0.5f, h * 0.5f - 6.0f, fading ? col::bg : (over ? col::bright : col::dim (0.7f)), 8.0f,
               juce::Justification::centred, w);
    };
    fadeTime.onClick = [this] (const juce::MouseEvent&) { fadeMenu(); };
    setHint (fadeTime, "fade", "How long Swell takes to travel when you click a point on its line. Click to change it, or to send Swell all the way down or up");
    canvas.addAndMakeVisible (fadeTime);
    doneEditing.draw = [] (juce::Graphics& g, bool over)
    {
        label (g, "done", 100.0f, 2.0f, over ? col::ink : col::bright, 8.0f, juce::Justification::centredRight, 100.0f);
    };
    doneEditing.onClick = [this] (const juce::MouseEvent&) { proc.setEditingNext (false); };
    setHint (doneEditing, "done", "Back to the part that's playing");
    canvas.addChildComponent (doneEditing);
    presetSaver.setVisible (false);

    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };
    overlay.onPaint = [this] (juce::Graphics& g) { for (auto& p : plates) plate (g, p.x, p.y, p.name, false); };
    overlay.setInterceptsMouseClicks (false, false);
    canvas.addAndMakeVisible (overlay);
    canvas.setOpaque (true);
    addAndMakeVisible (canvas);
    addMouseListener (this, true);

    // Any shape from the standard size up; the scale snaps to 100, 150 or 200% so the pixels stay sharp,
    // and whatever is left over goes to the modules. The size is kept with your project.
    setResizable (true, true);
    setResizeLimits (baseW, baseH, 3600, 2400);
    const int w0 = proc.uiW > 0 ? proc.uiW : baseW, h0 = proc.uiH > 0 ? proc.uiH : baseH;
    setSize (juce::jlimit (baseW, 3600, w0), juce::jlimit (baseH, 2400, h0));
    sized = true;
    resized();
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

GrainFieldEditor::~GrainFieldEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

Knob& GrainFieldEditor::knob (int sceneParam, const juce::String& label)
{
    auto* k = knobs.add (new Knob (proc, sceneParam, label));
    canvas.addAndMakeVisible (k);
    return *k;
}

/* ---------------- Layout ---------------- */

void GrainFieldEditor::resized()
{
    // Snap the scale to whole steps so pixel art stays crisp, then give whatever is left over to the modules.
    const float raw = juce::jmin ((float) getWidth() / (float) baseW, (float) getHeight() / (float) baseH);
    // Below the standard size (a host can insist) it shrinks smoothly rather than falling apart.
    scale = raw >= 2.0f ? 2.0f : (raw >= 1.5f ? 1.5f : (raw >= 1.0f ? 1.0f : raw));
    W = (float) getWidth() / scale;
    H = (float) getHeight() / scale;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    canvas.setBounds (0, 0, (int) std::ceil (W), (int) std::ceil (H));
    layout();
    overlay.setBounds (canvas.getLocalBounds());
    overlay.toFront (false);
    if (sized) { proc.uiW = getWidth(); proc.uiH = getHeight(); }
}

void GrainFieldEditor::layout()
{
    // Three columns, used all the way down: texture, the output strip, drones. Each row is made of bays
    // on those columns, so every control lines up with the module it belongs to.
    const float m = 10.0f, gap = 10.0f, colB = 176.0f;
    const float colA = std::floor ((W - 2.0f * m - 2.0f * gap - colB) * 0.5f);
    const float xA = m, xB = xA + colA + gap, xC = xB + colB + gap, colC = W - m - xC;
    const float yH = 10.0f, hH = 40.0f, yT = 64.0f, hT = 60.0f, yCh = 138.0f, hCh = 64.0f, yS = 216.0f, hS = 40.0f;
    const float yM = 270.0f, ySw = H - 80.0f, hSw = 44.0f, yF = H - 28.0f;
    const float hM = ySw - 14.0f - yM;
    auto ri = [] (float x, float y, float w, float h) { return juce::Rectangle<float> (x, y, w, h).toNearestInt(); };

    bays.clear();
    plates.clear();
    // Header.
    bays.push_back ({ xA, yH, colA, hH });
    bays.push_back ({ xB, yH, colB, hH });
    bays.push_back ({ xC, yH, colC, hH });
    titleCell = { xA + 1.0f, yH + 1.0f, 118.0f, hH - 2.0f };
    rootButton.setBounds (ri (xA + 119.0f, yH + 1.0f, colA - 120.0f, hH - 2.0f));
    prevPreset.setBounds (ri (xB + 1.0f, yH + 1.0f, 34.0f, hH - 2.0f));
    presetName.setBounds (ri (xB + 35.0f, yH + 1.0f, colB - 70.0f, hH - 2.0f));
    nextPreset.setBounds (ri (xB + colB - 35.0f, yH + 1.0f, 34.0f, hH - 2.0f));
    sizeButton.setBounds (ri (xC + colC - 97.0f, yH + 1.0f, 96.0f, hH - 2.0f));
    versionCell = { xC + 1.0f, yH + 1.0f, colC - 98.0f, hH - 2.0f };

    // Transport.
    bays.push_back ({ xA, yT, colA, hT });
    plates.push_back ({ xA + 10.0f, yT, "transport" });
    play.setBounds (ri (xA + 1.0f, yT + 1.0f, 64.0f, hT - 2.0f));
    record.setBounds (ri (xA + colA - 129.0f, yT + 1.0f, 64.0f, hT - 2.0f));
    loopButton.setBounds (ri (xA + colA - 65.0f, yT + 1.0f, 64.0f, hT - 2.0f));
    clock.setBounds (ri (xA + 65.0f, yT + 1.0f, colA - 194.0f, hT - 2.0f));
    bays.push_back ({ xB, yT, colB, hT });
    plates.push_back ({ xB + 10.0f, yT, "root" });
    rootNote.setBounds (ri (xB + 1.0f, yT + 1.0f, colB - 2.0f, hT - 2.0f));
    bays.push_back ({ xC, yT, colC, hT });
    const float kw = std::floor ((colC - 2.0f - 74.0f - 64.0f) * 0.5f);
    knobs[0]->setBounds (ri (xC + 1.0f, yT + 1.0f, kw, hT - 2.0f));
    knobs[1]->setBounds (ri (xC + 1.0f + kw, yT + 1.0f, kw, hT - 2.0f));
    tempoButton.setBounds (ri (xC + 1.0f + 2.0f * kw, yT + 1.0f, colC - 2.0f - 2.0f * kw - 64.0f, hT - 2.0f));
    sync.setBounds (ri (xC + colC - 65.0f, yT + 1.0f, 64.0f, hT - 2.0f));

    // The chords: one bay across everything.
    bays.push_back ({ xA, yCh, W - 2.0f * m, hCh });
    harmony.setBounds (ri (xA + 1.0f, yCh - HarmonyView::topPad, W - 2.0f * m - 2.0f, hCh + HarmonyView::topPad - 1.0f));

    // Scenes over texture and the strip; blend and the take over the drones.
    bays.push_back ({ xA, yS, colA + gap + colB, hS });
    plates.push_back ({ xA + 10.0f, yS, "scenes" });
    scenesView.setBounds (ri (xA + 1.0f, yS + 1.0f, colA + gap + colB - 2.0f, hS - 2.0f));
    bays.push_back ({ xC, yS, colC, hS });
    blendTime.setBounds (ri (xC + 1.0f, yS + 1.0f, 112.0f, hS - 2.0f));
    take.setBounds (ri (xC + 121.0f, yS + 1.0f, colC - 124.0f, hS - 2.0f));

    // The modules: texture, the output strip, drones. Each: a row of controls, its picture, its sounds, its knobs.
    modules[0] = { xA, yM, colA, hM };
    modules[1] = { xB, yM, colB, hM };
    modules[2] = { xC, yM, colC, hM };
    const float in = 7.0f, rowY = yM + 12.0f, rowH = 26.0f, srcH = 3.0f * (float) SourcesView::rowH;
    const float knobsH = 2.0f * (float) Knob::height, yKnobs = yM + hM - in - knobsH;
    const float ySrc = yKnobs - 8.0f - srcH, yPic = rowY + rowH + 8.0f;
    auto placeKnobs = [&] (int first, int count, int across, float x, float w, float y)
    {
        const float cw = std::floor (w / (float) across);
        for (int k = 0; k < count; ++k)
        {
            juce::Component* c = first + k < knobs.size() ? (juce::Component*) knobs[first + k] : (juce::Component*) &volume;
            c->setBounds (ri (x + cw * (float) (k % across), y + (float) (k / across) * (float) Knob::height,
                              (k % across == across - 1) ? w - cw * (float) (across - 1) : cw, (float) Knob::height));
        }
    };
    // Texture.
    {
        const float x = xA + in, w = colA - 2.0f * in;
        textureStage.setBounds (ri (x, rowY, 96.0f, rowH));
        textureNote.setBounds (ri (x + w - 100.0f, rowY, 100.0f, rowH));
        resample.setBounds (ri (x + w - 210.0f, rowY, 108.0f, rowH));
        textureLevel.setBounds (ri (x + 106.0f, rowY, w - 326.0f, rowH));
        texturePicture.setBounds (ri (x, yPic, w, ySrc - 8.0f - yPic));
        textureSources.setBounds (ri (x, ySrc, w, srcH));
        placeKnobs (2, 8, 4, x, w, yKnobs);
        grids[0] = { x, yKnobs, w, knobsH };
        tables[0] = { x, ySrc, w, srcH };
    }
    // The output strip.
    {
        const float x = xB + in, w = colB - 2.0f * in;
        spaceMode.setBounds (ri (x, rowY, w, rowH));
        outputPicture.setBounds (ri (x, yPic, w, yKnobs - 8.0f - yPic));
        placeKnobs (18, 4, 2, x, w, yKnobs);
        grids[1] = { x, yKnobs, w, knobsH };
    }
    // Drones.
    {
        const float x = xC + in, w = colC - 2.0f * in;
        dronesStage.setBounds (ri (x, rowY, 96.0f, rowH));
        dronesLevel.setBounds (ri (x + 106.0f, rowY, w - 106.0f, rowH));
        dronesPicture.setBounds (ri (x, yPic, w, ySrc - 8.0f - yPic));
        droneSources.setBounds (ri (x, ySrc, w, srcH));
        bedStage.setBounds (ri (x + w - 96.0f, ySrc + 2.0f * (float) SourcesView::rowH + 1.0f, 96.0f, (float) SourcesView::rowH - 3.0f));
        placeKnobs (10, 8, 4, x, w, yKnobs);
        grids[2] = { x, yKnobs, w, knobsH };
        tables[1] = { x, ySrc, w, srcH };
    }

    // Swell across everything, with fade and freeze at its end.
    bays.push_back ({ xA, ySw, W - 2.0f * m, hSw });
    plates.push_back ({ xA + 10.0f, ySw, "swell" });
    swell.setBounds (ri (xA + 1.0f, ySw + 1.0f, W - 2.0f * m - 2.0f - 192.0f, hSw - 2.0f));
    fadeTime.setBounds (ri (W - m - 193.0f, ySw + 1.0f, 96.0f, hSw - 2.0f));
    freeze.setBounds (ri (W - m - 97.0f, ySw + 1.0f, 96.0f, hSw - 2.0f));
    footer = { xA, yF, W - 2.0f * m, 20.0f };
    doneEditing.setBounds (ri (W - m - 100.0f, yF + 2.0f, 100.0f, 18.0f));
    canvas.repaint();
}

void GrainFieldEditor::tempoMenu()
{
    if (proc.engine.hostHasTempo())
    {
        proc.setStatus ("The tempo is your song's: change it in your DAW.");
        return;
    }
    auto* param = proc.apvts.getParameter (ids::tempo);
    const int cur = juce::roundToInt (proc.apvts.getRawParameterValue (ids::tempo)->load());
    juce::PopupMenu m;
    m.addSectionHeader ("tempo");
    for (int bpm : { 50, 60, 70, 80, 90, 100, 110, 120, 140 })
        m.addItem (juce::String (bpm), true, cur == bpm, [param, bpm]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) bpm));
            param->endChangeGesture();
        });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tempoButton));
}

void GrainFieldEditor::fadeMenu()
{
    juce::PopupMenu m;
    const double cur = proc.getSwellFadeSeconds();
    m.addSectionHeader ("swell travels over");
    for (double secs : { 5.0, 10.0, 20.0, 45.0, 90.0, 180.0, 300.0 })
        m.addItem (timeText (secs), true, std::abs (cur - secs) < 0.5, [this, secs] { proc.setSwellFadeSeconds (secs); });
    m.addSeparator();
    m.addItem ("fade to quiet now", [this] { proc.swellFadeTo (0.0f); });
    m.addItem ("rise to the wall now", [this] { proc.swellFadeTo (1.0f); });
    if (proc.isSwellFading()) m.addItem ("stop where it is", [this] { proc.cancelSwellFade(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fadeTime));
}

void GrainFieldEditor::presetMenu()
{
    juce::PopupMenu m;
    const auto names = proc.getPresetNames();
    const int factory = (int) factoryPresets().size();
    const auto current = proc.getPresetName();
    m.addItem ("new piece: empty, pick your own sounds", [this] { proc.newPiece(); });
    m.addSeparator();
    m.addSectionHeader ("pieces");
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == factory) m.addSectionHeader ("yours");
        m.addItem (names[i].toLowerCase(), true, names[i] == current, [this, i] { proc.loadPreset (i); });
    }
    m.addSeparator();
    m.addItem (juce::String::fromUTF8 ("save this piece…"), [this]
    {
        presetSaver.start (canvas, presetName.getBounds().withSizeKeepingCentre (220, 20), proc.getPresetName().toLowerCase(),
                           [this] (const juce::String& name) { proc.savePreset (name); });
    });
    m.addItem ("open the presets folder", []
    {
        auto f = GrainFieldProcessor::getPresetFolder();
        f.createDirectory();
        f.revealToUser();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName));
}

void GrainFieldEditor::sizeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("size");
    const int cur = juce::roundToInt (100.0f * scale);
    for (int pc : { 100, 150, 200 })
        m.addItem (juce::String (pc) + "%", true, cur == pc, [this, pc]
        {
            // Keeps the shape you've given the window.
            setSize (juce::roundToInt (W * (float) pc / 100.0f), juce::roundToInt (H * (float) pc / 100.0f));
        });
    m.addSeparator();
    m.addItem ("back to the standard shape", [this]
    {
        setSize (juce::roundToInt ((float) baseW * scale), juce::roundToInt ((float) baseH * scale));
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sizeButton));
}

void GrainFieldEditor::blendMenu()
{
    auto* param = proc.apvts.getParameter (ids::blend);
    const float cur = proc.apvts.getRawParameterValue (ids::blend)->load();
    juce::PopupMenu m;
    m.addSectionHeader ("going into a scene takes");
    for (float s : { 0.0f, 10.0f, 20.0f, 45.0f, 90.0f, 180.0f })
        m.addItem (s < 0.5f ? juce::String ("no time: a cut") : timeText (s), true, std::abs (cur - s) < 0.5f, [param, s]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (s));
            param->endChangeGesture();
        });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&blendTime));
}

/* ---------------- Hints ---------------- */

void GrainFieldEditor::repaintFooter()
{
    canvas.repaint (footer.toNearestInt());
}

void GrainFieldEditor::updateHint (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (auto* hs = dynamic_cast<HintSource*> (c))
        {
            const auto h = hs->hintAt (e.getEventRelativeTo (c).getPosition());
            hintLabel = h.first;
            hintText = h.second;
            repaintFooter();
            return;
        }
        if (c->getProperties().contains ("hintLabel"))
        {
            hintLabel = c->getProperties()["hintLabel"].toString();
            hintText = c->getProperties()["hintText"].toString();
            repaintFooter();
            return;
        }
    }
    if (hintLabel.isNotEmpty())
    {
        hintLabel = {};
        hintText = {};
        repaintFooter();
    }
}

void GrainFieldEditor::mouseMove (const juce::MouseEvent& e)
{
    updateHint (e);
}

void GrainFieldEditor::mouseExit (const juce::MouseEvent&)
{
    hintLabel = {};
    hintText = {};
    repaintFooter();
}

/* ---------------- Drawing ---------------- */

void GrainFieldEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
}

void GrainFieldEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // The bays, each a step up from the page with a firm border, and the modules with their double rule.
    for (auto& b : bays) bay (g, b);
    static const char* const titles[3] = { "texture", "out", "drones" };
    for (int i = 0; i < 3; ++i)
    {
        moduleFrame (g, modules[(size_t) i], false);
        plate (g, modules[(size_t) i].getX() + 10.0f, modules[(size_t) i].getY(), titles[i], true);
    }

    // The name, inverse, in the first cell of the header; the version in the last bay.
    g.setColour (col::ink);
    g.fillRect (titleCell);
    label (g, "grain field", titleCell.getCentreX(), titleCell.getCentreY() - 7.0f, col::bg, 10.0f, juce::Justification::centred, titleCell.getWidth(), true);
    g.setColour (col::cell);
    g.fillRect (versionCell);
    label (g, "v" + juce::String (JucePlugin_VersionString), versionCell.getX() + 10.0f, versionCell.getCentreY() - 6.0f, col::dim (0.35f), 7.0f);

    // Rules round the tables of sounds and the grids of knobs, so each sits in its own box.
    for (auto& t : tables)
    {
        g.setColour (col::rule);
        g.drawRect (t.expanded (1.0f), 1.0f);
    }
    for (auto& gr : grids)
    {
        ruleH (g, gr.getX() - 1.0f, gr.getRight(), gr.getY() - 1.0f);
        ruleV (g, gr.getX() - 1.0f, gr.getY() - 1.0f, gr.getBottom());
    }

    // Footer: what's under the mouse, or what just happened.
    juce::String label = hintLabel, body = hintText;
    juce::Colour labelColour = col::dim (0.6f);
    if (label.isEmpty())
    {
        if (juce::Time::getMillisecondCounterHiRes() < statusUntil && statusText.isNotEmpty())
        {
            label = statusText;
            labelColour = col::dim (0.8f);
        }
        else if (proc.isEditingNext())
        {
            const auto& s = proc.getScenes().slots[(size_t) proc.getNextPart()];
            label = "shaping " + juce::String (proc.getNextPart() + 1) + " " + s.name;
            body = "knobs, chords and swell now shape that part. what you hear stays the same until you move on";
            labelColour = col::bright;
        }
        else
        {
            label = "grain field";
            body = "pick sounds, shape what you hear, keep it as a scene";
        }
    }
    juce::AttributedString as;
    as.append (label.toLowerCase(), mono (11.0f), labelColour);
    if (body.isNotEmpty())
    {
        as.append (juce::String::fromUTF8 ("  —  "), mono (11.0f), col::dim (0.3f));
        as.append (body.toLowerCase(), mono (11.0f), col::dim (0.5f));
    }
    as.setWordWrap (juce::AttributedString::none);
    as.draw (g, footer.withTrimmedRight (proc.isEditingNext() ? 110.0f : 0.0f));
}

void GrainFieldEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.2, (now - lastTick) * 0.001);
    lastTick = now;

    // Every grain goes to the texture's picture, and to the waveform it was cut from.
    const int n = proc.engine.popEvents (events.data(), (int) events.size());
    texturePicture.addEvents (events.data(), n);
    textureSources.addEvents (events.data(), n);

    for (auto* k : knobs) k->tick();
    volume.tick();
    textureLevel.tick();
    dronesLevel.tick();
    swell.tick();
    sync.tick();
    freeze.tick();
    spaceMode.tick();
    play.tick();
    textureStage.tick();
    dronesStage.tick();
    bedStage.tick();
    scenesView.tick();
    harmony.tick();
    textureSources.tick (dt);
    droneSources.tick (dt);
    texturePicture.tick (dt);
    dronesPicture.tick (dt);
    outputPicture.tick (dt);
    rootButton.repaint();
    rootNote.repaint();
    clock.repaint();
    loopButton.repaint();
    tempoButton.repaint();
    textureNote.repaint();
    record.repaint();
    take.tick();
    resample.repaint();
    blendTime.repaint();
    fadeTime.repaint();
    sizeButton.repaint();
    presetName.repaint();

    const auto st = proc.getStatus();
    if (st != lastStatus)
    {
        lastStatus = st;
        statusText = st;
        statusUntil = now + 6000.0;
    }
    doneEditing.setVisible (proc.isEditingNext());

    repaintFooter();
}
