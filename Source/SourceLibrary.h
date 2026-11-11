#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <atomic>

namespace gf
{
// What a built-in sound is best at. Every sound works in every slot; this only groups the menus.
enum class BuiltinKind { played, sustained, place };

// One stretch of a sound that is a single note, so grains can be tuned to the harmony.
struct NoteSpan { int start, end; float midi; };

// One sound a slot can play from. Never freed while the plugin is open,
// so the audio thread can hold plain pointers to it safely.
struct SourceData : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SourceData>;
    juce::String id;     // "chimes", "file:/path/to/sound.wav" ...
    juce::String name;   // shown in the interface
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    std::vector<float> peaks;     // waveform overview for the display
    std::vector<NoteSpan> notes;  // empty when the sound has no clear pitch
    // Your own sounds and recordings: the pitch that keeps coming back in them, found when loaded.
    float coreMidi = -1.0f, coreStrength = 0.0f;
    // The track sent in through the side chain: a ring the audio thread keeps writing into.
    bool live = false;
    // Your own sounds are treated as one note from start to finish (the core note). You can say what that
    // note really is: a MIDI note here replaces what the listening found, rootAuto leaves it as found,
    // rootNone says the sound has no note and should be left untuned.
    static constexpr float rootAuto = -1.0f, rootNone = -2.0f;
    bool wholeNote = false;
    std::atomic<float> rootSet { rootAuto };

    // The note to tune from at a position: what you set, or else what the sound's own map says.
    float noteFor (double samplePos) const
    {
        if (wholeNote)
        {
            const float r = rootSet.load (std::memory_order_relaxed);
            if (r >= 0.0f) return r;
            if (r < -1.5f) return -1.0f;
        }
        return noteAt (samplePos);
    }

    // The note playing at a sample position, or -1 if there isn't one. Safe on the audio thread.
    float noteAt (double samplePos) const
    {
        if (notes.empty()) return -1.0f;
        const int p = (int) samplePos;
        size_t lo = 0, hi = notes.size();
        while (lo + 1 < hi)
        {
            const size_t mid = (lo + hi) / 2;
            if (notes[mid].start <= p) lo = mid; else hi = mid;
        }
        return (p >= notes[lo].start && p < notes[lo].end) ? notes[lo].midi : -1.0f;
    }
};

struct BuiltinInfo { juce::String id, name; BuiltinKind kind; };

class SourceLibrary
{
public:
    SourceLibrary();

    // The side chain input as a sound: always the last half minute of whatever is sent in.
    SourceData::Ptr live() const { return liveSrc; }
    void prepareLive (double sampleRate);   // call while the audio is stopped

    // Message thread. Finds a sound by id, decoding a built-in the first time it's asked for.
    SourceData::Ptr find (const juce::String& id);

    // Every built-in sound, for the menus.
    static std::vector<BuiltinInfo> builtins();
    // Sounds loaded or captured this session.
    juce::Array<SourceData::Ptr> getLoaded() const;
    // Audio files in Music/Grain Field/Sounds, ready to pick.
    static juce::Array<juce::File> folderSounds();
    static juce::File getSoundsFolder();
    static juce::File getCaptureFolder();

    // Message thread. Decodes a file (WAV, AIFF, FLAC, MP3, Ogg...). Returns nullptr on failure.
    SourceData::Ptr loadFile (const juce::File& file, juce::String& error);

    // Message thread. Adds audio captured from the output and saves it as a WAV file.
    SourceData::Ptr addCapture (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name = {});

private:
    SourceData::Ptr add (SourceData::Ptr s);
    static void computePeaks (SourceData& s);
public:
    // Listens for the note a sound keeps returning to (a hum, wind in wires, a voice) and, if the sound
    // has no note map, makes it one note long so its grains can be tuned by the smallest step.
    static void findCoreNote (SourceData& s);
private:

    struct BuiltinCache;
    juce::SharedResourcePointer<BuiltinCache> cache;

    SourceData::Ptr liveSrc;
    mutable juce::CriticalSection lock;
    juce::Array<SourceData::Ptr> sources;
    juce::AudioFormatManager formats;
};
} // namespace gf
