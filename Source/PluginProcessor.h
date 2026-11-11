#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include "Params.h"
#include "SourceLibrary.h"
#include "Engine.h"
#include "Scenes.h"

class GrainFieldProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    GrainFieldProcessor();
    ~GrainFieldProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return "Grain Field"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Message thread API used by the interface ---

    // Sounds
    juce::String getSlotSourceId (int slot) const { return sourceIds[(size_t) slot]; }
    void setSlotSourceId (int slot, const juce::String& id);
    void clearSlot (int slot);
    void loadFileIntoSlot (int slot, const juce::File& file);
    void captureIntoSlot (int slot);
    int getPendingCaptureSlot() const { return pendingCaptureSlot; }
    void resample();  // records the last 8 seconds of what you hear as a new sound in the texture
    // The note a slot's sound is tuned from (-1 if it has none): what you've said it is, or else what listening found.
    float getSlotCoreNote (int slot);
    // Your own sounds: say what note one really is. choice is a note (0 = c ... 11 = b), rootAsFound or rootNoNote.
    enum { rootAsFound = -1, rootNoNote = -2 };
    bool slotRootCanBeSet (int slot);                  // true for your own files and recordings
    int getSlotRootChoice (int slot);                  // what you chose
    int getSlotFoundNote (int slot);                   // what listening found (0..11), or -1 if nothing clear
    void setSlotRoot (int slot, int choice);
    void tuneToSlot (int slot);        // the root becomes the sound's core note
    void pieceFromSlot (int slot);     // five scenes, opening to wind down, made from this one sound
    // One recording as the root of everything: every slot plays it, and the key moves to its core note.
    void setRootSound (const juce::String& id);
    void setRootFile (const juce::File& file);

    // Harmony
    const gf::Progression& getProgression() const { return progression; }
    void setProgression (const gf::Progression& p, int playingNowAt = -1);
    // The chord last played on a MIDI keyboard, while it holds: keep it as a slot, or let it go.
    bool getKeysChord (gf::ChordStep& chord) const;
    void keepKeysChord();
    void releaseKeysChord() { engine.clearKeysChord(); }

    // The side chain: whether the host has anything connected to it at all.
    bool isSideChainConnected() const { return sideConnected.load (std::memory_order_relaxed); }

    // Recording: everything you hear, written to a file you can drag onto an audio track.
    void startRecording();
    void stopRecording();
    void toggleRecording() { if (isRecording()) stopRecording(); else startRecording(); }
    bool isRecording() const { return recActive.load (std::memory_order_relaxed) != nullptr; }
    bool isRecordingWaiting() const { return isRecording() && ! recStarted.load (std::memory_order_relaxed); }   // armed, until the piece plays
    double getRecordedSeconds() const { return (double) recSamples.load (std::memory_order_relaxed) / juce::jmax (1.0, recRate); }
    juce::File getLastTake() const { return lastTake; }
    double getLastTakeSeconds() const { return lastTakeSeconds; }
    static juce::File getRecordingsFolder();

    // Scenes (called parts inside): whole sounds you keep and blend between. Nothing moves on by itself;
    // you click one, or your DAW automates the Scene parameter.
    const gf::SceneBank& getScenes() const { return scenes; }
    int getActivePart() const { return activeScene; }
    int getNumParts() const { int n = 0; for (auto& sl : scenes.slots) n += sl.used ? 1 : 0; return n; }
    // The part after this one, or -1 when moving on would start a new part as a copy of now.
    int getNextPart() const { return activeScene + 1 < gf::numScenes && scenes.slots[(size_t) activeScene + 1].used ? activeScene + 1 : -1; }
    void moveOn();                                     // on to the next scene, or a new one after the last
    void goToPart (int index);                         // blend into any scene: nothing new is made
    void newPart();                                    // keep what you hear as a new scene, and be in it
    void rewind();                                     // back to the first chord, the clock at zero
    void renamePart (int index, const juce::String& name);
    void deletePart (int index);

    // Swell can travel to a point by itself, over a time you choose.
    void swellFadeTo (float targetNorm);
    void cancelSwellFade();
    bool isSwellFading() const { return fade.active; }
    float getSwellFadeTarget() const { return fade.to; }
    double getSwellFadeRemaining() const { return fade.active ? juce::jmax (0.0, fade.seconds - (juce::Time::getMillisecondCounterHiRes() * 0.001 - fade.start)) : 0.0; }
    double getSwellFadeSeconds() const { return fadeSeconds; }
    void setSwellFadeSeconds (double seconds) { fadeSeconds = juce::jlimit (1.0, 900.0, seconds); sendChangeMessage(); }

    // Editing the next part while this one plays
    bool isEditingNext() const { return editingNext && getNextPart() >= 0; }
    void setEditingNext (bool on);
    float getPartValue (int part, int p) const { return scenes.slots[(size_t) part].norm[(size_t) p]; }
    void setPartValue (int part, int p, float norm);
    void setPartProgression (int part, const gf::Progression& prog);

    // Presets: whole pieces
    juce::StringArray getPresetNames() const;      // factory first, then yours
    juce::String getPresetName() const { return presetName; }
    void loadPreset (int index);
    void newPiece();                               // empty slots, one part, one chord, stopped
    void stepPreset (int delta);
    void savePreset (const juce::String& name);
    static juce::File getPresetFolder();

    void setStatus (const juce::String& text);
    juce::String getStatus() const { return status; }

    int uiW = 0, uiH = 0;   // the editor's size, kept with the project (0: not set yet)

    juce::AudioProcessorValueTreeState apvts;
    gf::SourceLibrary library;
    gf::Engine engine;

private:
    void timerCallback() override;
    void pushScene (int index);
    void keepCurrent();
    void recall (int index);
    void compactParts();
    void carryInto (int part, const gf::SceneSlot& before);
    void pushAllScenes();
    void loadFactory (const gf::FactoryPreset& preset);
    void applySlot (const gf::SceneSlot& slot);
    juce::ValueTree pieceToTree();
    void pieceFromTree (const juce::ValueTree& tree);
    gf::SourceData::Ptr resolveSource (const juce::String& id, int slotForFallback);
    juce::String partLabel (int index) const;

    std::array<juce::String, gf::numSlots> sourceIds;
    std::map<juce::String, float> rootChoices;   // by sound: the note you've said it is
    juce::AudioBuffer<float> sideIn;
    std::atomic<bool> sideConnected { false };

    juce::TimeSliceThread recThread { "Grain Field recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recWriter;
    juce::CriticalSection recLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> recActive { nullptr };
    std::atomic<bool> recStarted { false };
    std::atomic<juce::int64> recSamples { 0 };
    std::atomic<int> recDropped { 0 };
    std::atomic<double> hostSeconds { 0.0 };
    double recRate = 48000.0, lastTakeSeconds = 0.0;
    juce::File recFile, lastTake;
    gf::SceneBank scenes;
    gf::Progression progression;
    int activeScene = 0;
    bool editingNext = false;
    int lastRecallSeen = 0;
    int pendingCaptureSlot = -1;
    bool resamplePending = false;
    struct Fade { bool active = false; float from = 0, to = 0; double start = 0, seconds = 0; } fade;
    double fadeSeconds = 20.0;
    juce::String status, presetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFieldProcessor)
};
