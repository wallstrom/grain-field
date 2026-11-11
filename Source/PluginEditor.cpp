#include "PluginEditor.h"

using namespace gf;
using namespace gf::ui;

static void chevron (juce::Graphics& g, float cx, float cy, float dir, juce::Colour c)
{
    juce::Path p;
    p.startNewSubPath (cx - 2.0f * dir, cy - 4.0f);
    p.lineTo (cx + 2.0f * dir, cy);
    p.lineTo (cx - 2.0f * dir, cy + 4.0f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

GrainFieldEditor::GrainFieldEditor (GrainFieldProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      scenesView (p), harmony (p), textureSources (p, SourcesView::Mode::texture), droneSources (p, SourcesView::Mode::drones),
      texturePicture (p), dronesPicture (p), outputPicture (p), swell (p), play (p),
      sync (p, ids::sync, "sync", false), freeze (p, ids::freeze, "freeze", true),
      spaceMode (p),
      textureLevel (p, txLevel, {}), dronesLevel (p, drLevel, {}), volume (p, ids::volume, "volume"), take (p),
      textureStage (p, gf::Engine::stTexture, "texture", false), dronesStage (p, gf::Engine::stDrones, "drones", false),
      bedStage (p, gf::Engine::stBed, "", true)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    // Header: name, root sound, pieces, size.
    rootButton.draw = [this] (juce::Graphics& g, bool over)
    {
        bool isSet = false;
        const auto label = textureSources.rootLabel (isSet);
        text (g, label, 0.0f, 1.0f, over ? col::accent : (isSet ? col::accent.withAlpha (0.85f) : col::dim (0.45f)), 11.0f,
              juce::Justification::centredLeft, (float) rootButton.getWidth());
    };
    rootButton.onClick = [this] (const juce::MouseEvent&) { textureSources.rootMenu (rootButton); };
    setHint (rootButton, "root sound", "One recording for everything: it becomes every module's sound, and the root moves to its core note");
    // The root note: the whole piece is counted from it. Every layer that follows it moves with it, and the chords too.
    rootNote.draw = [this] (juce::Graphics& g, bool over)
    {
        const auto c = over ? col::accent : col::ink;
        text (g, "root", 0.0f, 2.0f, col::dim (0.4f), 11.0f);
        chevron (g, 43.0f, 11.0f, -1.0f, col::dim (over ? 0.8f : 0.45f));
        text (g, harmony.keyShown() >= 0 ? juce::String (keyNames[harmony.keyShown()]).toLowerCase() : juce::String(), 60.0f, 0.0f, c, 14.0f,
              juce::Justification::centred, 26.0f);
        chevron (g, 77.0f, 11.0f, 1.0f, col::dim (over ? 0.8f : 0.45f));
    };
    rootNote.onClick = [this] (const juce::MouseEvent& e)
    {
        if (e.x >= 36 && e.x < 50) harmony.stepKey (-1);
        else if (e.x >= 71) harmony.stepKey (1);
        else harmony.keyMenu (rootNote);
        rootNote.repaint();
    };
    setHint (rootNote, "root note", "The note the whole piece is counted from. Every layer that follows it moves with it, keeping its interval, and so do the chords. Sounds left at their own pitch stay. The arrows step a semitone");
    prevPreset.draw = [] (juce::Graphics& g, bool over) { chevron (g, 10.0f, 10.0f, -1.0f, col::dim (over ? 0.9f : 0.5f)); };
    nextPreset.draw = [] (juce::Graphics& g, bool over) { chevron (g, 10.0f, 10.0f, 1.0f, col::dim (over ? 0.9f : 0.5f)); };
    presetName.draw = [this] (juce::Graphics& g, bool over)
    {
        const auto name = proc.getPresetName().toLowerCase();
        text (g, name, (float) presetName.getWidth() * 0.5f, 2.0f, over ? col::accent : col::ink, 12.0f, juce::Justification::centred,
              (float) presetName.getWidth());
    };
    prevPreset.onClick = [this] (const juce::MouseEvent&) { proc.stepPreset (-1); };
    nextPreset.onClick = [this] (const juce::MouseEvent&) { proc.stepPreset (1); };
    presetName.onClick = [this] (const juce::MouseEvent&) { presetMenu(); };
    sizeButton.draw = [this] (juce::Graphics& g, bool over)
    {
        text (g, "size " + juce::String (juce::roundToInt (100.0f * scale)) + "%", 0.0f, 1.0f, col::dim (over ? 0.8f : 0.4f), 11.0f);
    };
    sizeButton.onClick = [this] (const juce::MouseEvent&) { sizeMenu(); };
    setHint (sizeButton, "size", "How big everything is. Drag the window's bottom-right corner to make it wider or taller: the modules stretch");
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

    // The transport: play, the clock, record, loop. Then the chords, the root, the tempo and sync.
    clock.draw = [this] (juce::Graphics& g, bool over)
    {
        const int t = (int) juce::jmax (0.0, proc.engine.getRunSeconds());
        text (g, juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2), 0.0f, 1.0f,
              over ? col::accent : col::dim (0.55f), 11.0f);
    };
    clock.onClick = [this] (const juce::MouseEvent&) { proc.rewind(); };
    setHint (clock, "clock", "How long Grain Field has been playing. Click to go back to the first chord, with the clock at zero");
    loopButton.draw = [this] (juce::Graphics& g, bool over)
    {
        // Two arrows chasing each other: lit when the chords go round, dim when each one stays.
        const bool looping = ! harmony.staying();
        g.setColour (over ? col::accent : (looping ? col::ink : col::dim (0.35f)));
        juce::Path p;
        p.startNewSubPath (4.0f, 10.0f); p.lineTo (4.0f, 8.0f); p.quadraticTo (4.0f, 5.0f, 7.0f, 5.0f); p.lineTo (17.0f, 5.0f);
        p.startNewSubPath (14.5f, 2.5f); p.lineTo (17.0f, 5.0f); p.lineTo (14.5f, 7.5f);
        p.startNewSubPath (18.0f, 10.0f); p.lineTo (18.0f, 12.0f); p.quadraticTo (18.0f, 15.0f, 15.0f, 15.0f); p.lineTo (5.0f, 15.0f);
        p.startNewSubPath (7.5f, 12.5f); p.lineTo (5.0f, 15.0f); p.lineTo (7.5f, 17.5f);
        g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    loopButton.onClick = [this] (const juce::MouseEvent&) { harmony.setStaying (! harmony.staying()); loopButton.repaint(); };
    setHint (loopButton, "loop", "Lit: when a chord's bars are done, the next one in the loop plays, round and round. Dim: each chord stays until you queue the next");
    tempoButton.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool host = proc.engine.hostHasTempo();
        text (g, "tempo", 0.0f, 1.0f, col::dim (0.4f), 11.0f);
        text (g, juce::String (juce::roundToInt (proc.engine.getBpm())), 46.0f, 1.0f, over && ! host ? col::accent : col::ink, 11.0f);
    };
    tempoButton.onClick = [this] (const juce::MouseEvent&) { tempoMenu(); };
    setHint (tempoButton, "tempo", "How long a bar is, for the chords and the echo. In a DAW it's your song's tempo; on its own, click to set it");
    record.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        if (proc.isRecordingWaiting())
        {
            g.setColour (col::accent.withAlpha (blink ? 1.0f : 0.35f));
            g.drawEllipse (5.0f, 4.0f, 12.0f, 12.0f, 1.5f);
        }
        else if (proc.isRecording()) dot (g, 11.0f, 10.0f, 6.0f, col::accent.withAlpha (blink ? 1.0f : 0.55f));
        else
        {
            g.setColour (over ? col::accent : col::dim (0.55f));
            g.drawEllipse (5.0f, 4.0f, 12.0f, 12.0f, 1.0f);
            dot (g, 11.0f, 10.0f, 3.0f, col::accent.withAlpha (over ? 1.0f : 0.75f));
        }
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
        text (g, "blend", 0.0f, 1.0f, col::dim (0.4f), 11.0f);
        if (pr >= 0.0f && secs > 0.5f)
            text (g, timeText (secs * (1.0f - pr)) + " left", 44.0f, 1.0f, col::accent, 11.0f);
        else
            text (g, secs < 0.5f ? juce::String ("a cut") : timeText (secs), 44.0f, 1.0f, over ? col::accent : col::ink, 11.0f);
    };
    blendTime.onClick = [this] (const juce::MouseEvent&) { blendMenu(); };
    setHint (blendTime, "blend", "How long going into another scene takes, from a cut to three minutes");
    for (auto* c : std::initializer_list<juce::Component*> { &scenesView, &blendTime, &take })
        canvas.addAndMakeVisible (c);

    // Harmony.
    canvas.addAndMakeVisible (harmony);
    knob (hmLine, "line");
    knob (hmDissolve, "dissolve");

    // The three modules, each with its picture.
    resample.draw = [this] (juce::Graphics& g, bool over)
    {
        const bool listening = proc.getPendingCaptureSlot() >= 0;
        const bool on = ! listening || (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        dot (g, 4.5f, 9.0f, 3.5f, col::accent.withAlpha (on ? (over || listening ? 1.0f : 0.7f) : 0.3f));
        text (g, listening ? "listening" : "resample", 15.0f, 1.0f, listening || over ? col::accent : col::dim (0.6f), 11.0f);
    };
    resample.onClick = [this] (const juce::MouseEvent&) { proc.resample(); };
    // Where the texture plays, counted from the root. Its sounds move with it.
    textureNote.draw = [this] (juce::Graphics& g, bool over)
    {
        const int idx = textureSources.shownChoice (txNote);
        arrow (g, 0.0f, 9.0f, 7.0f, col::dim (over ? 0.8f : 0.3f));
        const auto note = textureSources.playsNote (txNote, idx);
        text (g, note, 13.0f, 1.0f, over ? col::accent : (idx != 0 ? col::ink : col::dim (0.6f)), 11.0f);
        text (g, noteStepName (txNote, idx), 13.0f + textWidth (note) + 5.0f, 1.5f, col::dim (over ? 0.7f : 0.45f), 9.5f);
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
        text (g, "fade " + timeText (proc.getSwellFadeSeconds()), 0.0f, 0.0f, fading ? col::accent : col::dim (over ? 0.85f : 0.45f), 11.0f);
    };
    fadeTime.onClick = [this] (const juce::MouseEvent&) { fadeMenu(); };
    setHint (fadeTime, "fade", "How long Swell takes to travel when you click a point on its line. Click to change it, or to send Swell all the way down or up");
    canvas.addAndMakeVisible (fadeTime);
    doneEditing.draw = [] (juce::Graphics& g, bool over)
    {
        text (g, "done", 100.0f, 0.0f, over ? col::ink : col::accent, 11.0f, juce::Justification::centredRight, 100.0f);
    };
    doneEditing.onClick = [this] (const juce::MouseEvent&) { proc.setEditingNext (false); };
    setHint (doneEditing, "done", "Back to the part that's playing");
    canvas.addChildComponent (doneEditing);
    presetSaver.setVisible (false);

    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };
    canvas.setOpaque (true);
    addAndMakeVisible (canvas);
    addMouseListener (this, true);

    // Any size and shape from 70% up; the size is kept with your project.
    setResizable (true, true);
    setResizeLimits (baseW * 7 / 10, baseH * 7 / 10, 3600, 2400);
    const int w0 = proc.uiW > 0 ? proc.uiW : juce::roundToInt (baseW * 1.1f), h0 = proc.uiH > 0 ? proc.uiH : juce::roundToInt (baseH * 1.1f);
    setSize (juce::jlimit (baseW * 7 / 10, 3600, w0), juce::jlimit (baseH * 7 / 10, 2400, h0));
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
    // Scale to fit the base layout, then give whatever is left over, sideways or downwards, to the modules.
    scale = juce::jmin ((float) getWidth() / (float) baseW, (float) getHeight() / (float) baseH);
    W = (float) getWidth() / scale;
    H = (float) getHeight() / scale;
    canvas.setTransform (juce::AffineTransform::scale (scale));
    canvas.setBounds (0, 0, (int) std::ceil (W), (int) std::ceil (H));
    layout();
    if (sized) { proc.uiW = getWidth(); proc.uiH = getHeight(); }
}

void GrainFieldEditor::layout()
{
    const int w = (int) std::ceil (W), h = (int) std::ceil (H);
    const int pad = 40, in = 24;

    // Rows: header, transport, scenes, the modules, swell, footer. One-pixel seams between them.
    const int yT = 57, yS = 163, yMod = 215, ySwell = h - 105, yFooter = h - 40;
    // Columns of the module row: texture and drones side by side, the output narrower.
    const int x1 = juce::roundToInt (W * 0.385f), x2 = juce::roundToInt (W * 0.77f);

    frames[fHeader]    = { 0.0f, 0.0f, W, (float) yT - 1.0f };
    frames[fTransport] = { 0.0f, (float) yT, W, (float) (yS - yT) - 1.0f };
    frames[fScenes]    = { 0.0f, (float) yS, W, (float) (yMod - yS) - 1.0f };
    frames[fTexture]   = { 0.0f, (float) yMod, (float) x1, (float) (ySwell - yMod) - 1.0f };
    frames[fDrones]    = { (float) x1 + 1.0f, (float) yMod, (float) (x2 - x1) - 1.0f, (float) (ySwell - yMod) - 1.0f };
    frames[fTape]      = { (float) x2 + 1.0f, (float) yMod, W - (float) x2 - 1.0f, (float) (ySwell - yMod) - 1.0f };
    frames[fSwell]     = { 0.0f, (float) ySwell, W, (float) (yFooter - ySwell) - 1.0f };
    frames[fFooter]    = { 0.0f, (float) yFooter, W, H - (float) yFooter };

    // Header.
    rootButton.setBounds (214, 20, 220, 18);
    presetName.setBounds (w / 2 - 140, 18, 280, 20);
    prevPreset.setBounds (w / 2 - 164, 18, 20, 20);
    nextPreset.setBounds (w / 2 + 144, 18, 20, 20);
    sizeButton.setBounds (w - pad - 84, 20, 84, 18);

    // The transport: play, clock, record and loop; the chords across the middle; then line and dissolve,
    // the root, the tempo and sync.
    const int cy = yT + 53;   // the row's middle
    play.setBounds (pad, cy - 18, 36, 36);
    clock.setBounds (pad + 46, cy - 9, 46, 18);
    record.setBounds (pad + 96, cy - 10, 22, 20);
    loopButton.setBounds (pad + 124, cy - 10, 22, 20);
    const int hx = pad + 166, rightW = 430;
    harmony.setBounds (hx, yT + 13, juce::jmax (300, w - hx - rightW), 86);
    knobs[0]->setBounds (w - rightW + 14, yT + 22, Knob::width, Knob::height);
    knobs[1]->setBounds (w - rightW + 82, yT + 22, Knob::width, Knob::height);
    rootNote.setBounds (w - rightW + 168, cy - 11, 90, 22);
    tempoButton.setBounds (w - rightW + 282, cy - 9, 78, 18);
    sync.setBounds (w - pad - 34, cy - 9, 40, 18);

    // The scenes, then how long a blend takes, then the last take.
    scenesView.setBounds (pad, yS + 11, w - pad - 340, (int) ScenesView::chipH);
    blendTime.setBounds (w - 300, yS + 16, 90, 18);
    take.setBounds (w - 210, yS + 16, 196, 18);

    // Modules: where each one's contents start and end.
    const int txA = pad, txB = x1 - in, drA = x1 + 1 + in, drB = x2 - in, outA = x2 + 1 + in, outB = w - pad;
    titleY = (float) yMod + 14.0f;
    outTitleX = (float) outA;

    textureStage.setBounds (txA, yMod + 10, 152, 20);
    textureLevel.setBounds (txB - 90, yMod + 13, 90, 16);
    resample.setBounds (txB - 90 - 20 - 84, yMod + 12, 84, 18);
    textureNote.setBounds (txA + 146, yMod + 12, juce::jmax (40, txB - 90 - 20 - 84 - (txA + 146) - 6), 18);
    dronesStage.setBounds (drA, yMod + 10, 160, 20);
    dronesLevel.setBounds (drB - 90, yMod + 13, 90, 16);
    spaceMode.setBounds (outB - 112, yMod + 13, 112, 20);

    // Knobs along the bottom of each module.
    const int yKnobA = ySwell - 164, yKnobB = ySwell - 88;
    auto placeKnobs = [this, yKnobA, yKnobB] (int first, int count, int across, int a, int b)
    {
        for (int k = 0; k < count; ++k)
        {
            const float cx = (float) a + (float) (b - a) * ((float) (k % across) + 0.5f) / (float) across;
            knobs[first + k]->setBounds (juce::roundToInt (cx) - Knob::width / 2, k < across ? yKnobA : yKnobB, Knob::width, Knob::height);
        }
    };
    placeKnobs (2, 8, 4, txA, txB);
    placeKnobs (10, 8, 4, drA, drB);
    placeKnobs (18, 3, 2, outA, outB);
    volume.setBounds (juce::roundToInt ((float) outA + (float) (outB - outA) * 0.75f) - Knob::width / 2, yKnobB, Knob::width, Knob::height);

    // Each module's picture on top, its sounds (with their waveforms and notes) under it, above the knobs.
    const int srcH = 3 * SourcesView::rowH;
    const int ySrc = yKnobA - 18 - srcH;
    const int yPic = yMod + 42, picH = juce::jmax (60, ySrc - 12 - yPic);
    texturePicture.setBounds (txA, yPic, txB - txA, picH);
    dronesPicture.setBounds (drA, yPic, drB - drA, picH);
    outputPicture.setBounds (outA, yPic, outB - outA, ySrc + srcH - yPic);
    textureSources.setBounds (txA, ySrc, txB - txA, srcH);
    droneSources.setBounds (drA, ySrc, drB - drA, srcH);
    bedStage.setBounds (drB - 96, ySrc + 2 * SourcesView::rowH, 96, 20);
    seam1 = { (float) txB, (float) yPic, (float) (drA - txB), (float) picH };
    seam2 = { (float) drB, (float) yPic, (float) (outA - drB), (float) picH };
    seamY = (float) yPic + (float) picH * 0.5f;

    // Swell and the footer.
    swell.setBounds (pad, ySwell, w - pad - 190, yFooter - ySwell - 1);
    fadeTime.setBounds (w - 178, ySwell + 18, 80, 16);
    freeze.setBounds (w - 90, ySwell + 18, 60, 16);
    doneEditing.setBounds (w - pad - 100, yFooter + 11, 100, 18);
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
    for (int pc : { 70, 85, 100, 110, 125, 150, 175, 200 })
        m.addItem (juce::String (pc) + "%", true, std::abs (cur - pc) <= 2, [this, pc]
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
    canvas.repaint (frames[fFooter].toNearestInt());
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
    const auto pt = e.getEventRelativeTo (&canvas).position;
    int f = -1;
    for (int i = 0; i < numFrames; ++i) if (frames[(size_t) i].contains (pt)) f = i;
    if (f != hoverFrame) { hoverFrame = f; canvas.repaint(); }
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
    g.fillAll (juce::Colour (0xff070707));
}

void GrainFieldEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff070707));   // the seams

    // Each part of the instrument is its own module; the one under the mouse lifts a little.
    for (int i = 0; i < numFrames; ++i)
    {
        g.setColour (i == hoverFrame && i != fHeader && i != fFooter ? juce::Colour (0xff151515) : juce::Colour (0xff101010));
        g.fillRect (frames[(size_t) i]);
    }

    g.setColour (col::ink);
    g.setFont (serif (27.0f));
    g.drawText ("Grain Field", juce::Rectangle<float> (40.0f, 11.0f, 180.0f, 34.0f), juce::Justification::centredLeft);

    text (g, "output", outTitleX, titleY, col::dim (0.4f), 11.0f);

    // Between the pictures: the texture melts into the drones as far as Merge says, and both go on to the output.
    const bool running = proc.engine.isRunning();
    const float merge = proc.engine.getDisplayNorm (txFeed);
    arrow (g, seam1.getX() + 8.0f, seamY, seam1.getWidth() - 16.0f, col::dim ((0.16f + 0.64f * merge) * (running ? 1.0f : 0.6f)));
    text (g, "merge", seam1.getCentreX(), seamY + 7.0f, col::dim (0.18f + 0.3f * merge), 9.0f, juce::Justification::centred, 60.0f);
    arrow (g, seam2.getX() + 8.0f, seamY, seam2.getWidth() - 16.0f, col::dim (running ? 0.6f : 0.35f));

    // Footer: what's under the mouse, or what just happened.
    juce::String label = hintLabel, body = hintText;
    juce::Colour labelColour = col::dim (0.45f);
    if (label.isEmpty())
    {
        if (juce::Time::getMillisecondCounterHiRes() < statusUntil && statusText.isNotEmpty())
        {
            label = statusText;
            labelColour = col::dim (0.7f);
        }
        else if (proc.isEditingNext())
        {
            const auto& s = proc.getScenes().slots[(size_t) proc.getNextPart()];
            label = "shaping " + juce::String (proc.getNextPart() + 1) + " " + s.name;
            body = "knobs, chords and swell now shape that part. what you hear stays the same until you move on";
            labelColour = col::accent;
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
        as.append (juce::String::fromUTF8 ("  —  "), mono (11.0f), col::dim (0.25f));
        as.append (body.toLowerCase(), mono (11.0f), col::dim (0.45f));
    }
    as.setWordWrap (juce::AttributedString::none);
    as.draw (g, juce::Rectangle<float> (40.0f, frames[fFooter].getY() + 11.0f, W - 80.0f - (proc.isEditingNext() ? 120.0f : 0.0f), 18.0f));
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
    canvas.repaint (seam1.getSmallestIntegerContainer());
    canvas.repaint (seam2.getSmallestIntegerContainer());

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
