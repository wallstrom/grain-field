#include "PluginProcessor.h"
#if ! GF_HEADLESS
 #include "PluginEditor.h"
#endif

using namespace gf;

GrainFieldProcessor::GrainFieldProcessor()
    : AudioProcessor (BusesProperties()
                          // The side chain: another track sent in becomes a sound to work with.
                          .withInput ("Side chain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          // On by default: Logic's Multi Output mode and Ableton's routing both
                          // expect the extra outputs to exist. A plain stereo setup simply ignores them.
                          .withOutput ("Texture", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Drones", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Send", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createLayout()),
      engine (apvts)
{
    newPiece();
    status = "Pick sounds for the texture and drones, or a root sound, then press play.";
    startTimerHz (30);
}

GrainFieldProcessor::~GrainFieldProcessor()
{
    stopTimer();
    if (isRecording()) stopRecording();
    recThread.stopThread (2000);
}

void GrainFieldProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (isRecording() && std::abs (sampleRate - recRate) > 1.0) stopRecording();   // a file has one sample rate
    library.prepareLive (sampleRate);
    engine.setLiveSource (library.live().get());
    sideIn.setSize (2, juce::jmax (samplesPerBlock, 4096));
    engine.prepare (sampleRate, samplesPerBlock);
}

bool GrainFieldProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& set = layouts.outputBuses.getReference (i);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
    }
    if (layouts.inputBuses.size() > 1) return false;
    for (const auto& set : layouts.inputBuses)
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo() && set != juce::AudioChannelSet::mono()) return false;
    return true;
}

void GrainFieldProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // A MIDI keyboard plays the harmony.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.midiNote (m.getNoteNumber(), true);
        else if (m.isNoteOff()) engine.midiNote (m.getNoteNumber(), false);
    }

    Outputs out;
    auto pair = [this, &buffer] (int bus, float** dest) {
        if (bus >= getBusCount (false)) return;
        auto* b = getBus (false, bus);
        if (b == nullptr || ! b->isEnabled() || b->getNumberOfChannels() < 2) return;
        auto busBuffer = getBusBuffer (buffer, false, bus);
        dest[0] = busBuffer.getWritePointer (0);
        dest[1] = busBuffer.getWritePointer (1);
    };
    // Read the side chain before anything writes over the shared channels.
    const int ns = buffer.getNumSamples();
    const bool sideOn = getBusCount (true) > 0 && getBus (true, 0)->isEnabled() && getBus (true, 0)->getNumberOfChannels() > 0;
    sideConnected.store (sideOn, std::memory_order_relaxed);
    if (sideOn && ns <= sideIn.getNumSamples())
    {
        auto in = getBusBuffer (buffer, true, 0);
        sideIn.copyFrom (0, 0, in, 0, 0, ns);
        sideIn.copyFrom (1, 0, in, in.getNumChannels() > 1 ? 1 : 0, 0, ns);
        out.input[0] = sideIn.getReadPointer (0);
        out.input[1] = sideIn.getReadPointer (1);
    }
    pair (0, out.main); pair (1, out.texture); pair (2, out.drones); pair (3, out.send);
    if (out.main[0] == nullptr) { buffer.clear(); return; }
    engine.process (out, buffer.getNumSamples(), getPlayHead());

    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto t = pos->getTimeInSeconds()) hostSeconds.store (*t, std::memory_order_relaxed);

    // Recording: hand what was just made to the writer's thread. Nothing here waits for the disk.
    if (recActive.load (std::memory_order_relaxed) != nullptr && ! isNonRealtime())
    {
        const juce::ScopedTryLock sl (recLock);
        if (sl.isLocked())
            if (auto* w = recActive.load())
            {
                if (! recStarted.load (std::memory_order_relaxed) && engine.isRunning()) recStarted.store (true);
                if (recStarted.load (std::memory_order_relaxed))
                {
                    const float* chans[2] = { out.main[0], out.main[1] };
                    if (w->write (chans, ns)) recSamples.fetch_add (ns, std::memory_order_relaxed);
                    else recDropped.fetch_add (1, std::memory_order_relaxed);
                }
            }
    }
}

/* ---------------- Recording ---------------- */

juce::File GrainFieldProcessor::getRecordingsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Grain Field").getChildFile ("Recordings");
}

void GrainFieldProcessor::startRecording()
{
    if (recWriter != nullptr) return;
    const double rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const auto folder = getRecordingsFolder();
    folder.createDirectory();
    const auto name = juce::File::createLegalFileName (presetName.isNotEmpty() ? presetName : juce::String ("grain field"))
                      + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S") + ".wav";
    recFile = folder.getChildFile (name);
    std::unique_ptr<juce::OutputStream> stream = recFile.createOutputStream();
    if (stream == nullptr) { setStatus ("Couldn't make a file in Music/Grain Field/Recordings."); return; }

    // The file carries where in your song it began, so a DAW can put it back at that spot.
    const auto songAt = (juce::int64) (juce::jmax (0.0, hostSeconds.load()) * rate);
    const auto bwav = juce::WavAudioFormat::createBWAVMetadata ("Grain Field: " + presetName, "Grain Field", {}, juce::Time::getCurrentTime(), songAt, {});
    std::unordered_map<juce::String, juce::String> meta;
    for (auto& key : bwav.getAllKeys()) meta[key] = bwav[key];
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (2).withBitsPerSample (24).withMetadataValues (meta));
    if (writer == nullptr) { setStatus ("Couldn't start recording."); return; }

    recRate = rate;
    recSamples.store (0);
    recDropped.store (0);
    recStarted.store (false);
    recThread.startThread();
    recWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), recThread, 1 << 17);
    {
        const juce::ScopedLock sl (recLock);
        recActive.store (recWriter.get());
    }
    // Recording starts with the piece. If it's stopped and playing by itself, start it; with sync on, it waits for your song.
    const bool syncOn = apvts.getRawParameterValue (ids::sync)->load() > 0.5f;
    if (! syncOn && apvts.getRawParameterValue (ids::play)->load() < 0.5f)
        if (auto* pl = apvts.getParameter (ids::play)) pl->setValueNotifyingHost (1.0f);
    setStatus (syncOn && ! engine.isRunning() ? "Ready to record: it starts when your song does."
                                              : "Recording everything you hear. Click record again to finish.");
}

void GrainFieldProcessor::stopRecording()
{
    if (recWriter == nullptr) return;
    {
        const juce::ScopedLock sl (recLock);
        recActive.store (nullptr);
    }
    recWriter.reset();   // writes out whatever is still waiting, and closes the file
    const auto samples = recSamples.load();
    if (samples < (juce::int64) (0.2 * recRate))
    {
        recFile.deleteFile();
        setStatus ("Nothing was recorded: the piece never played.");
        return;
    }
    lastTake = recFile;
    lastTakeSeconds = (double) samples / recRate;
    setStatus ("Recorded " + timeText (lastTakeSeconds) + ". Drag the take onto an audio track, or find it in Music/Grain Field/Recordings."
               + (recDropped.load() > 0 ? " The disk fell behind for a moment, so there may be a gap." : ""));
}

juce::AudioProcessorEditor* GrainFieldProcessor::createEditor()
{
#if GF_HEADLESS
    return nullptr;
#else
    return new GrainFieldEditor (*this);
#endif
}

bool GrainFieldProcessor::hasEditor() const
{
#if GF_HEADLESS
    return false;
#else
    return true;
#endif
}

void GrainFieldProcessor::setStatus (const juce::String& text)
{
    status = text;
    sendChangeMessage();
}

juce::String GrainFieldProcessor::partLabel (int index) const
{
    if (index < 0 || index >= numScenes) return "a new scene";
    const auto& s = scenes.slots[(size_t) index];
    return s.name.isNotEmpty() ? s.name : "scene " + juce::String (index + 1);
}

/* ---------------- Sounds ---------------- */

SourceData::Ptr GrainFieldProcessor::resolveSource (const juce::String& id, int slot)
{
    if (id == "none") return nullptr;
    // A sound comes back with the note you gave it.
    auto withRoot = [this] (SourceData::Ptr s)
    {
        const auto it = rootChoices.find (s->id);
        if (it != rootChoices.end() && s->wholeNote) s->rootSet.store (it->second);
        return s;
    };
    if (auto s = library.find (id)) return withRoot (s);
    if (id.startsWith ("file:"))
    {
        juce::String err;
        const juce::File f (id.fromFirstOccurrenceOf ("file:", false, false));
        if (f.existsAsFile())
            if (auto s = library.loadFile (f, err)) return withRoot (s);
    }
    return library.find (defaultSlotSources[slot]);
}

void GrainFieldProcessor::setSlotSourceId (int slot, const juce::String& id)
{
    auto s = resolveSource (id, slot);
    sourceIds[(size_t) slot] = s != nullptr ? s->id : juce::String ("none");
    engine.setSlotSource (slot, s.get());
    sendChangeMessage();
}

void GrainFieldProcessor::clearSlot (int slot)
{
    setSlotSourceId (slot, "none");
    setStatus (juce::String (slotNames[slot]) + " is empty.");
}

void GrainFieldProcessor::loadFileIntoSlot (int slot, const juce::File& file)
{
    juce::String err;
    if (auto s = library.loadFile (file, err))
    {
        setSlotSourceId (slot, s->id);
        // Whatever comes in follows the root the piece is on.
        static const char* const nn[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
        const int key = juce::roundToInt (apvts.getRawParameterValue (spec (hmKey).id)->load());
        const float own = getSlotCoreNote (slot);
        setStatus (juce::String (slotNames[slot]) + " now uses " + s->name + ". "
                   + (own >= 0.0f ? "Its own note is " + juce::String (nn[((int) own) % 12]) + "; it's shifted to follow the root (" + juce::String (nn[((key % 12) + 12) % 12]) + "). Click the note by its name if that's wrong."
                                  : juce::String ("No clear note was found in it. Click \"note\" by its name to say what it is.")));
    }
    else setStatus (err);
}

void GrainFieldProcessor::captureIntoSlot (int slot)
{
    pendingCaptureSlot = slot;
    engine.requestCapture();
    setStatus ("Recording the last 8 seconds into " + juce::String (slotNames[slot]) + "...");
}

void GrainFieldProcessor::resample()
{
    // What you hear right now becomes a sound of its own in the texture, so the piece can feed on itself.
    // It goes into the first empty texture sound, or else replaces the one you're using least.
    int slot = -1;
    for (int k = 0; k < 3 && slot < 0; ++k) if (sourceIds[(size_t) k] == "none") slot = k;
    if (slot < 0)
    {
        slot = 0;
        for (int k = 1; k < 3; ++k)
            if (apvts.getRawParameterValue (spec (txMix1 + k).id)->load() < apvts.getRawParameterValue (spec (txMix1 + slot).id)->load()) slot = k;
    }
    resamplePending = true;
    pendingCaptureSlot = slot;
    engine.requestCapture();
    setStatus ("Resampling: the last 8 seconds become texture sound " + juce::String (slot + 1) + "...");
}

float GrainFieldProcessor::getSlotCoreNote (int slot)
{
    auto s = resolveSource (sourceIds[(size_t) slot], slot);
    if (s == nullptr) return -1.0f;
    if (s->wholeNote)
    {
        const float said = s->rootSet.load();
        if (said >= 0.0f) return said;
        if (said < -1.5f) return -1.0f;
    }
    return s->coreMidi > 0.0f && s->coreStrength >= 1.6f ? s->coreMidi : -1.0f;
}

bool GrainFieldProcessor::slotRootCanBeSet (int slot)
{
    const auto& id = sourceIds[(size_t) slot];
    if (! (id.startsWith ("file:") || id.startsWith ("capture:"))) return false;
    auto s = resolveSource (id, slot);
    return s != nullptr && s->wholeNote;
}

int GrainFieldProcessor::getSlotRootChoice (int slot)
{
    auto s = resolveSource (sourceIds[(size_t) slot], slot);
    if (s == nullptr || ! s->wholeNote) return rootAsFound;
    const float said = s->rootSet.load();
    return said >= 0.0f ? ((int) said) % 12 : (said < -1.5f ? (int) rootNoNote : (int) rootAsFound);
}

int GrainFieldProcessor::getSlotFoundNote (int slot)
{
    auto s = resolveSource (sourceIds[(size_t) slot], slot);
    return s != nullptr && s->coreMidi > 0.0f && s->coreStrength >= 1.6f ? ((int) s->coreMidi) % 12 : -1;
}

void GrainFieldProcessor::setSlotRoot (int slot, int choice)
{
    auto s = resolveSource (sourceIds[(size_t) slot], slot);
    if (s == nullptr || ! s->wholeNote) return;
    static const char* const nn[12] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" };
    float value = SourceData::rootAuto;
    if (choice == rootNoNote) value = SourceData::rootNone;
    else if (choice >= 0)
    {
        // The same note name, in the octave nearest to where the sound's energy is.
        const float around = s->coreMidi > 0.0f ? s->coreMidi : 48.0f;
        float m = (float) (((int) around / 12) * 12 + (choice % 12));
        if (m - around > 6.0f) m -= 12.0f;
        if (around - m > 6.0f) m += 12.0f;
        value = juce::jlimit (12.0f, 115.0f, m);
    }
    s->rootSet.store (value);
    if (choice == rootAsFound) rootChoices.erase (s->id); else rootChoices[s->id] = value;
    const int found = getSlotFoundNote (slot);
    setStatus (choice >= 0 ? s->name + " is now treated as " + nn[choice % 12] + ": it's shifted from there to fit each chord."
               : choice == rootNoNote ? s->name + " is left at its own pitch. Only the chord's filter tunes it."
               : s->name + " goes by what listening found: " + (found >= 0 ? juce::String (nn[found]) : juce::String ("no clear note")) + ".");
}

void GrainFieldProcessor::tuneToSlot (int slot)
{
    const float core = getSlotCoreNote (slot);
    if (core < 0.0f) { setStatus ("No clear note in that sound."); return; }
    const int pc = ((int) core) % 12;
    auto* key = apvts.getParameter (spec (hmKey).id);
    key->beginChangeGesture();
    key->setValueNotifyingHost (key->convertTo0to1 ((float) pc));
    key->endChangeGesture();
    // Start on the sound's own note: the chord's root becomes the key itself.
    auto p = progression;
    p.steps[0].root = 0;
    setProgression (p);
    engine.cueStep (0);
    setStatus ("Tuned to " + resolveSource (sourceIds[(size_t) slot], slot)->name + ": its core note is "
               + juce::String (keyNames[pc]).toLowerCase() + ". The root is now " + juce::String (keyNames[pc]).toLowerCase() + ".");
}

void GrainFieldProcessor::pieceFromSlot (int slot)
{
    auto src = resolveSource (sourceIds[(size_t) slot], slot);
    const float core = getSlotCoreNote (slot);
    if (src == nullptr || core < 0.0f) { setStatus ("No clear note in that sound, so there's nothing to tune the piece to."); return; }
    const int pc = ((int) core) % 12;
    const juce::String id = src->id;
    const char* sid = id.toRawUTF8();

    // The arc, grown out of one sound: it starts as the recording with a faint drone in it,
    // finds its root, builds to a peak and winds back down to the recording.
    FactoryPreset fp { "from a sound", pc, { sid, sid, "none", sid, sid }, {
        { "opening", "0:0:8", { { pSwell, 0.05f }, { drBed, 0.75f }, { txLevel, 0.35f }, { txLength, 4.0f }, { txDensity, 0.8f },
                                 { drBody, 0.25f }, { hmLine, 0.0f }, { drGrow, 0.35f } } },
        { "root", "0:1:8", { { pSwell, 0.28f }, { drBed, 0.6f }, { txLevel, 0.5f }, { txLength, 3.5f }, { hmLine, 0.15f } } },
        { "build", "0:1:8,5:1:4,7:4:4", { { pSwell, 0.6f }, { drBed, 0.45f }, { txDensity, 2.4f }, { drBody, 0.7f }, { outDrive, 0.3f }, { hmLine, 0.35f } } },
        { "peak", "0:1:4,5:3:4,9:2:4,7:4:4", { { pSwell, 0.95f }, { drBed, 0.35f }, { txDensity, 4.0f }, { outDrive, 0.5f }, { drSub, 0.6f },
                                                { txSpace, 0.75f }, { drSpace, 0.65f }, { hmLine, 0.45f } } },
        { "wind down", "0:0:8", { { pSwell, 0.12f }, { drBed, 0.8f }, { txTape, 0.5f }, { drTape, 0.4f }, { txDensity, 0.6f }, { hmLine, 0.0f }, { spSize, 0.95f } } },
    } };
    loadFactory (fp);
    presetName = "from " + src->name.toLowerCase();
    setStatus ("A piece made from " + src->name + ", in " + juce::String (keyNames[pc]).toLowerCase()
               + ". It starts as the recording; click the next scenes to grow it.");
}

void GrainFieldProcessor::setRootSound (const juce::String& id)
{
    auto s = resolveSource (id, 0);
    if (s == nullptr) return;
    for (int k = 0; k < numSlots; ++k) setSlotSourceId (k, s->id);
    if (s->live)
    {
        setStatus (isSideChainConnected() ? "The track sent in is the root of everything now. Play it, and Grain Field is made from it."
                                          : "The track sent in is the root now, but nothing is connected yet: pick the track in the Side Chain menu at the top right of this window.");
        return;
    }
    if (getSlotCoreNote (slotTone) >= 0.0f) tuneToSlot (slotTone);
    else setStatus (s->name + " is the root of everything now. It has no clear note, so the root stays where it was.");
}

void GrainFieldProcessor::setRootFile (const juce::File& file)
{
    juce::String err;
    if (auto s = library.loadFile (file, err)) setRootSound (s->id);
    else setStatus (err);
}

/* ---------------- Harmony ---------------- */

void GrainFieldProcessor::setProgression (const Progression& p, int playingNowAt)
{
    progression = p;
    engine.setProgression (p, playingNowAt);
    sendChangeMessage();
}

bool GrainFieldProcessor::getKeysChord (ChordStep& chord) const
{
    const int packed = engine.getKeysChord();
    if (packed < 0) return false;
    const int key = juce::jlimit (0, 11, juce::roundToInt (apvts.getRawParameterValue (spec (hmKey).id)->load()));
    chord.root = (((packed & 15) - key) % 12 + 12) % 12;
    chord.type = (packed >> 4) & 15;
    chord.oct = ((packed >> 8) & 15) - 2;
    chord.bars = 4;
    chord.loop = true;
    return true;
}

void GrainFieldProcessor::keepKeysChord()
{
    ChordStep c;
    if (! getKeysChord (c)) return;
    auto p = progression;
    if (p.count >= Progression::maxSteps) { setStatus ("All eight slots are used. Remove one to keep this chord."); return; }
    const int key = juce::jlimit (0, 11, juce::roundToInt (apvts.getRawParameterValue (spec (hmKey).id)->load()));
    p.steps[(size_t) p.count++] = c;
    setProgression (p, p.count - 1);   // it carries on playing, now as a slot of its own
    engine.clearKeysChord();
    setStatus ("Kept " + chordName (key, c) + " as chord " + juce::String (p.count) + ".");
}

/* ---------------- Parts ---------------- */

void GrainFieldProcessor::pushScene (int i)
{
    const auto& slot = scenes.slots[(size_t) i];
    SceneData d;
    d.used = slot.used;
    d.norm = slot.norm;
    d.prog = slot.prog;
    for (int k = 0; k < numSlots; ++k)
        d.src[(size_t) k] = slot.used && slot.src[(size_t) k].isNotEmpty() ? resolveSource (slot.src[(size_t) k], k).get() : nullptr;
    engine.setScene (i, d);
    engine.setPartInfo (i, slot.used, (float) slot.length, slot.blendIn);
}

void GrainFieldProcessor::pushAllScenes()
{
    for (int i = 0; i < numScenes; ++i) pushScene (i);
}

void GrainFieldProcessor::carryInto (int part, const SceneSlot& before)
{
    // A part you haven't been in or shaped yet takes on whatever you changed in this one: the sounds you
    // swapped, the key and chords if you moved them, every control you touched. What you left alone
    // follows the piece's own plan, and Swell always does, so the piece still builds and falls back.
    auto& slot = scenes.slots[(size_t) part];
    if (! slot.used || slot.shaped) return;
    const auto now = SceneBank::capture (apvts, sourceIds, progression, slot.name);
    for (int p = 0; p < numP; ++p)
        if (p != pSwell && std::abs (now.norm[(size_t) p] - before.norm[(size_t) p]) > 1.0e-4f)
            slot.norm[(size_t) p] = now.norm[(size_t) p];
    for (int k = 0; k < numSlots; ++k)
        if (now.src[(size_t) k] != before.src[(size_t) k]) slot.src[(size_t) k] = now.src[(size_t) k];
    if (! (now.prog == before.prog)) slot.prog = now.prog;
    pushScene (part);
}

void GrainFieldProcessor::setEditingNext (bool on)
{
    const int nextPart = getNextPart();
    if (on && nextPart >= 0)
    {
        if (activeScene >= 0) carryInto (nextPart, scenes.slots[(size_t) activeScene]);   // start from what you've changed
        scenes.slots[(size_t) nextPart].shaped = true;
    }
    editingNext = on && nextPart >= 0;
    setStatus (editingNext ? "Editing " + partLabel (nextPart) + ". What you hear doesn't change until you move on."
                           : "Back to the part that's playing.");
}

void GrainFieldProcessor::setPartValue (int part, int p, float norm)
{
    if (part < 0 || part >= numScenes) return;
    scenes.slots[(size_t) part].norm[(size_t) p] = juce::jlimit (0.0f, 1.0f, norm);
    pushScene (part);
    sendChangeMessage();
}

void GrainFieldProcessor::setPartProgression (int part, const Progression& prog)
{
    if (part < 0 || part >= numScenes) return;
    scenes.slots[(size_t) part].prog = prog;
    pushScene (part);
    sendChangeMessage();
}

void GrainFieldProcessor::keepCurrent()
{
    // Keep what you've done to the part you're in, so coming back finds it as you left it.
    if (activeScene < 0) return;
    auto& slot = scenes.slots[(size_t) activeScene];
    auto kept = SceneBank::capture (apvts, sourceIds, progression, slot.name);
    kept.length = slot.length;
    kept.blendIn = slot.blendIn;
    kept.shaped = true;
    slot = kept;
    pushScene (activeScene);
}

void GrainFieldProcessor::recall (int index)
{
    auto* p = apvts.getParameter (ids::part);
    if (juce::roundToInt (apvts.getRawParameterValue (ids::part)->load()) == index + 1) engine.requestRecall (index);
    else p->setValueNotifyingHost (p->convertTo0to1 ((float) (index + 1)));
    activeScene = index;
    editingNext = false;
    cancelSwellFade();
}

void GrainFieldProcessor::moveOn()
{
    if (activeScene < 0) return;
    const int from = activeScene;
    const int target = getNextPart();
    if (target < 0) { newPart(); return; }

    const auto before = scenes.slots[(size_t) from];   // as it was when you came into it
    carryInto (target, before);
    keepCurrent();

    const float blend = apvts.getRawParameterValue (ids::blend)->load();
    scenes.slots[(size_t) target].blendIn = blend;
    pushScene (target);
    recall (target);
    setStatus ("Blending into " + partLabel (target) + (blend > 0.5f ? " over " + timeText (blend) : juce::String()) + ".");
}

void GrainFieldProcessor::newPart()
{
    if (activeScene < 0) return;
    const int target = scenes.firstFree();
    if (target < 0) { setStatus ("All eight scenes are used. Remove one to make room."); return; }
    keepCurrent();

    // A new scene starts as a copy of what is playing; you change it as it plays.
    auto& slot = scenes.slots[(size_t) target];
    slot = SceneBank::capture (apvts, sourceIds, progression, "scene " + juce::String (target + 1));
    slot.blendIn = 0.0f;
    pushScene (target);
    engine.setActivePart (target);
    activeScene = target;
    editingNext = false;
    if (auto* p = apvts.getParameter (ids::part)) p->setValueNotifyingHost (p->convertTo0to1 ((float) (target + 1)));
    setStatus ("Kept what you hear as " + partLabel (target) + ". You're in it now: whatever you change is kept with it.");
}

void GrainFieldProcessor::goToPart (int index)
{
    if (index < 0 || index >= numScenes || ! scenes.slots[(size_t) index].used || index == activeScene) return;
    keepCurrent();
    recall (index);
    const float blend = apvts.getRawParameterValue (ids::blend)->load();
    setStatus ("Blending into " + partLabel (index) + (blend > 0.5f ? " over " + timeText (blend) : juce::String()) + ".");
}

void GrainFieldProcessor::rewind()
{
    engine.requestRewind();
    setStatus ("Back to the first chord, with the clock at zero.");
}

void GrainFieldProcessor::compactParts()
{
    // Parts are always 1, 2, 3... with no gaps.
    int w = 0;
    for (int r = 0; r < numScenes; ++r)
        if (scenes.slots[(size_t) r].used)
        {
            if (r != w) { scenes.slots[(size_t) w] = scenes.slots[(size_t) r]; scenes.slots[(size_t) r] = SceneSlot(); if (activeScene == r) activeScene = w; }
            ++w;
        }
    if (activeScene < 0 || activeScene >= numScenes || ! scenes.slots[(size_t) activeScene].used) activeScene = 0;
}

/* ---------------- Swell fades ---------------- */

void GrainFieldProcessor::swellFadeTo (float targetNorm)
{
    auto* p = apvts.getParameter (spec (pSwell).id);
    if (fade.active) p->endChangeGesture();
    fade.active = true;
    fade.from = p->getValue();
    fade.to = juce::jlimit (0.0f, 1.0f, targetNorm);
    fade.start = juce::Time::getMillisecondCounterHiRes() * 0.001;
    fade.seconds = fadeSeconds;
    p->beginChangeGesture();
    setStatus ("Swell is travelling to " + juce::String (juce::roundToInt (fade.to * 100.0f)) + " over " + timeText (fadeSeconds) + ". Drag it to take over.");
}

void GrainFieldProcessor::cancelSwellFade()
{
    if (! fade.active) return;
    fade.active = false;
    apvts.getParameter (spec (pSwell).id)->endChangeGesture();
    setStatus ("Swell stays where it is.");
}

void GrainFieldProcessor::renamePart (int index, const juce::String& name)
{
    if (index < 0 || index >= numScenes) return;
    scenes.slots[(size_t) index].name = name.trim().toLowerCase();
    sendChangeMessage();
}

void GrainFieldProcessor::deletePart (int index)
{
    if (index < 0 || index >= numScenes || index == activeScene || ! scenes.slots[(size_t) index].used) return;
    const juce::String gone = partLabel (index);
    scenes.slots[(size_t) index] = SceneSlot();
    const int was = activeScene;
    compactParts();
    pushAllScenes();
    if (activeScene != was)
    {
        // The part you're in has moved up one place; nothing you hear changes.
        engine.setActivePart (activeScene);
        if (auto* p = apvts.getParameter (ids::part)) p->setValueNotifyingHost (p->convertTo0to1 ((float) (activeScene + 1)));
    }
    editingNext = false;
    setStatus ("Removed " + gone + ".");
}

/* ---------------- Presets ---------------- */

juce::File GrainFieldProcessor::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Grain Field").getChildFile ("Presets");
}

juce::StringArray GrainFieldProcessor::getPresetNames() const
{
    juce::StringArray names;
    for (auto& f : factoryPresets()) names.add (f.name);
    for (auto& f : getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.gfpiece"))
        names.addIfNotAlreadyThere (f.getFileNameWithoutExtension());
    return names;
}

void GrainFieldProcessor::applySlot (const SceneSlot& slot)
{
    for (int p = 0; p < numP; ++p)
        if (auto* param = apvts.getParameter (spec (p).id))
            param->setValueNotifyingHost (slot.norm[(size_t) p]);
    for (int k = 0; k < numSlots; ++k)
        if (slot.src[(size_t) k].isNotEmpty())
        {
            auto s = resolveSource (slot.src[(size_t) k], k);
            sourceIds[(size_t) k] = s != nullptr ? s->id : juce::String ("none");
            engine.setSlotSource (k, s.get());
        }
    setProgression (slot.prog);
}

void GrainFieldProcessor::loadFactory (const FactoryPreset& preset)
{
    for (auto& s : scenes.slots) s = SceneSlot();
    for (size_t i = 0; i < preset.parts.size() && i < (size_t) numScenes; ++i)
        scenes.slots[i] = makeFactoryPart (apvts, preset, preset.parts[i]);
    pushAllScenes();
    applySlot (scenes.slots[0]);
    activeScene = 0;
    editingNext = false;
    cancelSwellFade();
    if (auto* p = apvts.getParameter (ids::part)) p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
    engine.adoptSceneParam();
    engine.setActivePart (0);
    presetName = preset.name;
}

void GrainFieldProcessor::newPiece()
{
    const FactoryPreset blank { "new piece", 2, { "none", "none", "none", "none", "none" }, {
        { "opening", "0:1:8", { { pSwell, 0.2f } }, 0.0 },
    } };
    loadFactory (blank);
    presetName = "new piece";
    sendChangeMessage();
}

void GrainFieldProcessor::loadPreset (int index)
{
    const auto& factory = factoryPresets();
    if (index >= 0 && index < (int) factory.size())
    {
        loadFactory (factory[(size_t) index]);
        setStatus ("Loaded " + presetName + ".");
        return;
    }
    const auto names = getPresetNames();
    if (index < 0 || index >= names.size()) return;
    const auto file = getPresetFolder().getChildFile (names[index] + ".gfpiece");
    if (auto xml = juce::XmlDocument::parse (file))
    {
        pieceFromTree (juce::ValueTree::fromXml (*xml));
        presetName = names[index];
        setStatus ("Loaded " + presetName + ".");
    }
    else setStatus ("Couldn't read " + file.getFileName() + ".");
}

void GrainFieldProcessor::stepPreset (int delta)
{
    const auto names = getPresetNames();
    if (names.isEmpty()) return;
    int i = names.indexOf (presetName);
    i = ((i < 0 ? 0 : i + delta) % names.size() + names.size()) % names.size();
    loadPreset (i);
}

void GrainFieldProcessor::savePreset (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim().toLowerCase());
    if (clean.isEmpty()) return;
    // Keep what you've done to the part that's playing.
    keepCurrent();
    getPresetFolder().createDirectory();
    const auto file = getPresetFolder().getChildFile (clean + ".gfpiece");
    if (auto xml = pieceToTree().createXml())
        xml->writeTo (file);
    presetName = clean;
    setStatus ("Saved " + clean + " in Music/Grain Field/Presets.");
}

juce::ValueTree GrainFieldProcessor::pieceToTree()
{
    juce::ValueTree root ("GrainFieldPiece");
    root.setProperty ("version", 4, nullptr);
    root.setProperty ("uiW", uiW, nullptr);
    root.setProperty ("uiH", uiH, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    juce::ValueTree extra ("Extra");
    for (int s = 0; s < numSlots; ++s)
        extra.setProperty (juce::Identifier ("slot" + juce::String (s)), sourceIds[(size_t) s], nullptr);
    extra.setProperty ("activePart", activeScene, nullptr);
    extra.setProperty ("swellFade", fadeSeconds, nullptr);
    extra.setProperty ("take", lastTake.getFullPathName(), nullptr);
    extra.setProperty ("takeSeconds", lastTakeSeconds, nullptr);
    extra.setProperty ("prog", progression.toString(), nullptr);
    extra.setProperty ("preset", presetName, nullptr);
    root.appendChild (extra, nullptr);
    juce::ValueTree roots ("Roots");
    for (auto& [id, value] : rootChoices)
    {
        juce::ValueTree r ("Root");
        r.setProperty ("sound", id, nullptr);
        r.setProperty ("note", value, nullptr);
        roots.appendChild (r, nullptr);
    }
    root.appendChild (roots, nullptr);
    root.appendChild (scenes.toValueTree(), nullptr);
    return root;
}

void GrainFieldProcessor::pieceFromTree (const juce::ValueTree& root)
{
    if ((int) root.getProperty ("version", 1) < 3) return;
    uiW = (int) root.getProperty ("uiW", uiW);
    uiH = (int) root.getProperty ("uiH", uiH);
    const auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    engine.adoptSceneParam();

    // The notes you've given your own sounds, before any of them is loaded.
    for (const auto& r : root.getChildWithName ("Roots"))
        rootChoices[r.getProperty ("sound").toString()] = (float) r.getProperty ("note", SourceData::rootAuto);

    const auto extra = root.getChildWithName ("Extra");
    for (int s = 0; s < numSlots; ++s)
    {
        const juce::String id = extra.getProperty (juce::Identifier ("slot" + juce::String (s)), defaultSlotSources[s]).toString();
        auto src = resolveSource (id, s);
        sourceIds[(size_t) s] = src != nullptr ? src->id : juce::String ("none");
        engine.setSlotSource (s, src.get());
    }
    activeScene = extra.getProperty ("activePart", 0);
    fadeSeconds = juce::jlimit (1.0, 900.0, (double) extra.getProperty ("swellFade", fadeSeconds));
    cancelSwellFade();
    presetName = extra.getProperty ("preset", presetName).toString();
    if (! isRecording())
    {
        const juce::File take (extra.getProperty ("take").toString());
        lastTake = take.existsAsFile() ? take : juce::File();
        lastTakeSeconds = lastTake.existsAsFile() ? (double) extra.getProperty ("takeSeconds", 0.0) : 0.0;
    }
    setProgression (Progression::fromString (extra.getProperty ("prog").toString(), Progression::makeDefault()));

    const auto partTree = root.getChildWithName ("Parts");
    if (partTree.isValid())
    {
        scenes.fromValueTree (partTree, apvts);
        compactParts();
        pushAllScenes();
    }
    engine.setActivePart (activeScene);
    editingNext = false;
    sendChangeMessage();
}

/* ---------------- Timer ---------------- */

void GrainFieldProcessor::timerCallback()
{
    // A part was recalled on the audio thread (by Move on, or by automation of the Part control):
    // write its values back so the knobs, the host and saved projects agree with what you hear.
    const int rc = engine.getRecallCount();
    if (rc != lastRecallSeen)
    {
        lastRecallSeen = rc;
        const int idx = engine.getLastRecalled();
        if (idx >= 0 && scenes.slots[(size_t) idx].used)
        {
            // The piece moved by itself (playing through, or the host's automation): keep what you did to
            // the part it left, exactly as moving on by hand would.
            if (activeScene != idx) { keepCurrent(); cancelSwellFade(); }
            const auto& slot = scenes.slots[(size_t) idx];
            for (int p = 0; p < numP; ++p)
                if (auto* param = apvts.getParameter (spec (p).id))
                    if (std::abs (param->getValue() - slot.norm[(size_t) p]) > 1.0e-5f)
                        param->setValueNotifyingHost (slot.norm[(size_t) p]);
            for (int k = 0; k < numSlots; ++k)
                if (slot.src[(size_t) k].isNotEmpty() && slot.src[(size_t) k] != sourceIds[(size_t) k])
                {
                    auto s = resolveSource (slot.src[(size_t) k], k);
                    sourceIds[(size_t) k] = s != nullptr ? s->id : juce::String ("none");
                    engine.setSlotSource (k, s.get());
                }
            if (! (progression == slot.prog)) setProgression (slot.prog);
            if (activeScene != idx)
            {
                activeScene = idx;
                editingNext = false;
                if (auto* pp = apvts.getParameter (ids::part))
                    if (juce::roundToInt (apvts.getRawParameterValue (ids::part)->load()) != idx + 1)
                        pp->setValueNotifyingHost (pp->convertTo0to1 ((float) (idx + 1)));
            }
            sendChangeMessage();
        }
    }

    if (isRecording() && getRecordedSeconds() > 3600.0) stopRecording();   // an hour is the longest take

    if (fade.active)
    {
        auto* sw = apvts.getParameter (spec (pSwell).id);
        const double t = (juce::Time::getMillisecondCounterHiRes() * 0.001 - fade.start) / juce::jmax (0.05, fade.seconds);
        const float x = (float) juce::jlimit (0.0, 1.0, t);
        const float eased = x * x * (3.0f - 2.0f * x);
        sw->setValueNotifyingHost (fade.from + (fade.to - fade.from) * eased);
        if (t >= 1.0) { fade.active = false; sw->endChangeGesture(); sendChangeMessage(); }
    }

    if (pendingCaptureSlot >= 0)
    {
        juce::AudioBuffer<float> audio;
        double rate = 44100.0;
        if (engine.fetchCapture (audio, rate))
        {
            const int slot = pendingCaptureSlot;
            pendingCaptureSlot = -1;
            const juce::String name = resamplePending && activeScene >= 0 ? "resample: " + scenes.slots[(size_t) activeScene].name : juce::String();
            auto s = library.addCapture (audio, rate, name);
            setSlotSourceId (slot, s->id);
            if (resamplePending && slot < 3)
            {
                // Make sure it can be heard.
                auto* mix = apvts.getParameter (spec (txMix1 + slot).id);
                if (mix->getValue() < 0.6f) mix->setValueNotifyingHost (0.6f);
            }
            setStatus (resamplePending ? "Resampled. Texture sound " + juce::String (slot + 1) + " now plays from what you just heard."
                                       : s->name + " is now in " + juce::String (slotNames[slot]) + ".");
            resamplePending = false;
        }
    }
}

/* ---------------- State ---------------- */

void GrainFieldProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = pieceToTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void GrainFieldProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("GrainFieldPiece")) return; // projects from 0.3 and earlier keep the defaults
    pieceFromTree (root);
    // A project opens stopped, unless Grain Field is set to play along with the song.
    if (apvts.getRawParameterValue (ids::sync)->load() < 0.5f)
        if (auto* pl = apvts.getParameter (ids::play)) pl->setValueNotifyingHost (0.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainFieldProcessor();
}
