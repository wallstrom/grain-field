#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// Every control of the instrument, in the order sound flows:
// texture -> drones -> output, with harmony above and swell below.
namespace gf
{
constexpr int numScenes = 8;   // parts of a piece

// Sound slots: three for Texture, the sound the drones grow from, and the bed underneath.
constexpr int numSlots = 5;
constexpr int slotTone = 3, slotBed = 4;
extern const char* const slotNames[numSlots];
extern const char* const defaultSlotSources[numSlots];

// Controls stored in parts, in a fixed order.
enum P
{
    // Texture: clouds of grains
    txDensity, txLength, txSmear, txReverse, txSpread, txDetail, txFeed, txLevel,
    txMix1, txMix2, txMix3,   // how much of the cloud comes from each texture sound
    txPos1, txPos2, txPos3,   // where in each sound the grains are taken from
    txTape, txSpace,          // the texture's own tape wear, and how much of it goes into the room
    txNote,                   // where the texture plays, counted from the root
    txNote1, txNote2, txNote3,// where each texture sound plays, counted from the texture (or its own pitch)
    // Drones: the growing body
    drGrow, drBody, drBreath, drColour, drSub, drBed, drLevel,
    drTape, drSpace,
    drNote, bedNote,          // where the drones and the bed play, counted from the root
    // Harmony
    hmKey, hmLine, hmDissolve,
    // Output: what happens to everything on the way out
    outDrive, spSize, spEcho,
    // From almost nothing to a full wall
    pSwell,
    numP
};

enum class Kind { Float, Bool, Choice };
enum class Curve { Lin, Exp };

struct Spec
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    Curve curve;
    bool discrete;      // switches at the start of a blend instead of gliding
    const char* hint;
};

const Spec& spec (int p);
juce::StringArray choicesFor (int p);
// Whether Swell moves this control (it is drawn in the accent colour).
bool swellMoves (int p);

namespace ids
{
    inline const juce::String volume = "volume", part = "part", blend = "blend", play = "play",
                              sync = "sync", spaceMode = "spacemode", freeze = "freeze", hold = "hold", tempo = "tempo";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
juce::String formatValue (int p, float v);
// A rate such as grains per second: one decimal below 10, whole numbers above.
inline juce::String rateText (float v) { return v < 10.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v)); }
juce::String timeText (double seconds); // 7:42
float colourCutoffHz (float colour01);

// Notes in layers: the root (the key), then each module counted from the root, then each texture sound
// counted from the texture. A step is an interval (a fifth, a fourth), an octave, or the sound's own pitch.
struct NoteStep { int pc = 0, oct = 0; bool own = false; };
bool isNoteParam (int p);
bool noteCanBeOwn (int p);                 // the texture sounds and the bed can keep their own pitch
int numNoteSteps (int p);
NoteStep noteStep (int index);
juce::String noteStepName (int p, int index); // "root", "follows", "+5th", "-oct", "own pitch"

// Harmony
extern const char* const keyNames[12];
extern const char* const chordNames[6];
struct ChordVoicing { int drone[3]; int pad[5]; };
const ChordVoicing& chordVoicing (int chord);

// A progression: up to eight chords, each a root above the key, a kind of chord and a length in bars.
// loop: whether the chord is part of the cycle when the chords are set to loop.
// oct: the register, in octaves above or below where the chord would sit by itself.
struct ChordStep { int root = 0, type = 1, bars = 4; bool loop = true; int oct = 0; };
struct Progression
{
    static constexpr int maxSteps = 8;
    std::array<ChordStep, maxSteps> steps {};
    int count = 1;
    int totalBars() const { int t = 0; for (int i = 0; i < count; ++i) t += steps[(size_t) i].bars; return juce::jmax (1, t); }
    bool operator== (const Progression& o) const;
    juce::String toString() const;                 // "0:1:4,9:2:4"; a chord may add ":loop" (0 or 1) and ":octave"
    static Progression fromString (const juce::String&, const Progression& fallback);
    static Progression makeDefault();
};
juce::String chordName (int key, const ChordStep& c); // "d open"
// The lowest note of a chord as it will sound (a MIDI note), and a note's name with its octave ("e2").
int chordBassMidi (int key, const ChordStep& c);
juce::String noteNameWithOctave (int midi);
constexpr int minChordOct = -1, maxChordOct = 2;
} // namespace gf
