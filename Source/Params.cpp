#include "Params.h"

namespace gf
{
const char* const slotNames[numSlots] = { "Texture 1", "Texture 2", "Texture 3", "Drones", "Bed" };
const char* const defaultSlotSources[numSlots] = { "glasshaze", "tapechoir", "dusthalo", "bowedair", "lowroom" };
const char* const keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const chordNames[6] = { "Fifth", "Open", "Minor", "Major", "Sus", "Cluster" };

static const Spec specs[numP] = {
    { "tx_density", "Density",  Kind::Float, 0.1f, 12,   1.8f,  Curve::Exp, false, "How many grains are born each second. Swell raises it" },
    { "tx_length",  "Length",   Kind::Float, 0.05f, 8,   2.4f,  Curve::Exp, false, "How long each grain lasts, from a blip to a long smear" },
    { "tx_smear",   "Smear",    Kind::Float, 0,    1,    0.55f, Curve::Lin, false, "How far grains wander through the sound, and how blurred the cloud is" },
    { "tx_reverse", "Reverse",  Kind::Float, 0,    1,    0.3f,  Curve::Lin, false, "How many grains play backwards, swelling in instead of fading out" },
    { "tx_spread",  "Spread",   Kind::Float, 0,    1,    0.7f,  Curve::Lin, false, "How wide the cloud is, from the centre to far left and right" },
    { "tx_detail",  "Detail",   Kind::Float, 0,    1,    0.0f,  Curve::Lin, false, "Small accidents on top: specks, stutters, crackle" },
    { "tx_feed",    "Merge",    Kind::Float, 0,    1,    0.6f,  Curve::Lin, false, "How far the texture and the bed melt into the drones. Low: separate layers. High: one sound, reshaped as it goes" },
    { "tx_level",   "Texture level", Kind::Float, 0, 1,  0.7f,  Curve::Lin, false, "How loud the texture is" },
    { "tx_mix1",    "Source 1", Kind::Float, 0,    1,    1.0f,  Curve::Lin, false, "How much of the texture comes from this sound" },
    { "tx_mix2",    "Source 2", Kind::Float, 0,    1,    1.0f,  Curve::Lin, false, "How much of the texture comes from this sound" },
    { "tx_mix3",    "Source 3", Kind::Float, 0,    1,    1.0f,  Curve::Lin, false, "How much of the texture comes from this sound" },
    { "tx_pos1",    "Place 1",  Kind::Float, 0,    1,    0.25f, Curve::Lin, false, "Where in this sound the grains are taken from" },
    { "tx_pos2",    "Place 2",  Kind::Float, 0,    1,    0.5f,  Curve::Lin, false, "Where in this sound the grains are taken from" },
    { "tx_pos3",    "Place 3",  Kind::Float, 0,    1,    0.75f, Curve::Lin, false, "Where in this sound the grains are taken from" },
    { "tx_tape",    "Tape",     Kind::Float, 0,    1,    0.25f, Curve::Lin, false, "The texture's own tape: pitch that never quite sits still, lost highs, small dropouts" },
    { "tx_space",   "Space",    Kind::Float, 0,    1,    0.6f,  Curve::Lin, false, "How far back in the room the texture sits. Swell raises it" },
    { "tx_note",    "Texture note", Kind::Choice, 0, 4,  0,     Curve::Lin, true,  "Where the texture plays, counted from the root: on it, a fifth or a fourth above, or an octave up or down. Its sounds move with it" },
    { "tx_note1",   "Sound 1 note", Kind::Choice, 0, 5,  0,     Curve::Lin, true,  "Where this sound plays, counted from the texture, or its own pitch, left as recorded" },
    { "tx_note2",   "Sound 2 note", Kind::Choice, 0, 5,  0,     Curve::Lin, true,  "Where this sound plays, counted from the texture, or its own pitch, left as recorded" },
    { "tx_note3",   "Sound 3 note", Kind::Choice, 0, 5,  0,     Curve::Lin, true,  "Where this sound plays, counted from the texture, or its own pitch, left as recorded" },

    { "dr_grow",    "Grow",     Kind::Float, 0,    1,    0.45f, Curve::Lin, false, "How fast the drones build or fade: minutes on the left, seconds on the right" },
    { "dr_body",    "Body",     Kind::Float, 0,    1,    0.6f,  Curve::Lin, false, "From one thin tone to a thick stack of voices. Swell raises it" },
    { "dr_breath",  "Breath",   Kind::Float, 0,    1,    0.5f,  Curve::Lin, false, "Slow movement inside the drones" },
    { "dr_colour",  "Colour",   Kind::Float, 0,    1,    0.4f,  Curve::Lin, false, "From dark to bright" },
    { "dr_sub",     "Sub",      Kind::Float, 0,    1,    0.3f,  Curve::Lin, false, "A low octave under the chord. Swell raises it" },
    { "dr_bed",     "Bed",      Kind::Float, 0,    1,    0.35f, Curve::Lin, false, "How loud the bed recording underneath is" },
    { "dr_level",   "Drones level", Kind::Float, 0, 1,   0.75f, Curve::Lin, false, "How loud the drones are" },
    { "dr_tape",    "Tape",     Kind::Float, 0,    1,    0.2f,  Curve::Lin, false, "The drones' own tape: a slow waver, lost highs, small dropouts" },
    { "dr_space",   "Space",    Kind::Float, 0,    1,    0.5f,  Curve::Lin, false, "How far back in the room the drones sit. Swell raises it" },
    { "dr_voice",   "Voice",    Kind::Float, 0,    1,    0.7f,  Curve::Lin, false, "What the drones are made of: on the left, strings that ring in tune; on the right, the drones' own sound, held and sung on every note of the chord" },
    { "dr_note",    "Drones note", Kind::Choice, 0, 4,   0,     Curve::Lin, true,  "Where the drones play, counted from the root: on it, a fifth or a fourth above, or an octave up or down" },
    { "bed_note",   "Bed note",    Kind::Choice, 0, 5,   0,     Curve::Lin, true,  "Where the bed plays, counted from the root, or its own pitch, left as recorded" },

    { "hm_key",     "Root",      Kind::Choice, 0,   11,   2,     Curve::Lin, true,  "The root: the note the whole piece is counted from. Every layer that follows it moves with it, and so do the chords" },
    { "hm_line",    "Line",     Kind::Float, 0,    1,    0.35f, Curve::Lin, false, "A slow melody that walks through the chords, from buried to singing" },
    { "hm_dissolve", "Dissolve", Kind::Float, 0.3f, 20,  4.0f,  Curve::Exp, false, "How long one chord or key takes to melt into the next" },

    { "tp_drive",   "Drive",    Kind::Float, 0,    1,    0.25f, Curve::Lin, false, "Saturation on everything, up to distortion. Swell raises it" },
    { "sp_size",    "Size",     Kind::Float, 0,    1,    0.75f, Curve::Lin, false, "The room everything is sent into, from small to vast" },
    { "sp_echo",    "Echo",     Kind::Float, 0,    1,    0.2f,  Curve::Lin, false, "A tape echo on the texture, in time with your song" },

    { "swell",      "Swell",    Kind::Float, 0,    1,    0.35f, Curve::Lin, false, "From almost nothing to a full wall. Moves density, body, sub, drive and space together" },
};

const Spec& spec (int p) { return specs[p]; }

bool swellMoves (int p)
{
    return p == txDensity || p == drBody || p == drSub || p == outDrive || p == txSpace || p == drSpace;
}

bool isNoteParam (int p) { return p == txNote || p == txNote1 || p == txNote2 || p == txNote3 || p == drNote || p == bedNote; }
bool noteCanBeOwn (int p) { return p == txNote1 || p == txNote2 || p == txNote3 || p == bedNote; }
int numNoteSteps (int p) { return noteCanBeOwn (p) ? 6 : 5; }

NoteStep noteStep (int index)
{
    switch (index)
    {
        case 1: return { 7, 0, false };    // a fifth above
        case 2: return { 5, 0, false };    // a fourth above
        case 3: return { 0, 1, false };    // an octave up
        case 4: return { 0, -1, false };   // an octave down
        case 5: return { 0, 0, true };     // its own pitch
        default: return {};
    }
}

juce::String noteStepName (int p, int index)
{
    static const char* const names[6] = { "root", "+5th", "+4th", "+oct", "-oct", "own pitch" };
    if (index == 0 && (p == txNote1 || p == txNote2 || p == txNote3)) return "follows";
    return names[juce::jlimit (0, 5, index)];
}

juce::StringArray choicesFor (int p)
{
    juce::StringArray s;
    if (p == hmKey) for (auto* k : keyNames) s.add (k);
    if (isNoteParam (p)) for (int i = 0; i < numNoteSteps (p); ++i) s.add (noteStepName (p, i));
    return s;
}

juce::String timeText (double seconds)
{
    const int t = juce::jmax (0, (int) seconds);
    return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
}

float colourCutoffHz (float c) { return 650.0f * std::pow (24.0f, juce::jlimit (0.0f, 1.0f, c)); }

const ChordVoicing& chordVoicing (int c)
{
    // Semitones above the chord's root in octave 2. Drone voices stay low; pad voices carry the chord.
    static const ChordVoicing v[6] = {
        { { 0, 12, 7 },  { 19, 24, 31, 36, 43 } },  // Fifth
        { { 0, 12, 7 },  { 19, 26, 28, 31, 36 } },  // Open
        { { 0, 12, 7 },  { 15, 19, 22, 26, 27 } },  // Minor
        { { 0, 12, 7 },  { 16, 19, 23, 26, 28 } },  // Major
        { { 0, 12, 7 },  { 14, 17, 19, 22, 26 } },  // Sus
        { { 0, 12, 14 }, { 14, 15, 17, 19, 24 } },  // Cluster
    };
    return v[juce::jlimit (0, 5, c)];
}

bool Progression::operator== (const Progression& o) const
{
    if (count != o.count) return false;
    for (int i = 0; i < count; ++i)
    {
        const auto& a = steps[(size_t) i];
        const auto& b = o.steps[(size_t) i];
        if (a.root != b.root || a.type != b.type || a.bars != b.bars || a.loop != b.loop || a.oct != b.oct) return false;
    }
    return true;
}

juce::String Progression::toString() const
{
    juce::StringArray parts;
    for (int i = 0; i < count; ++i)
    {
        const auto& s = steps[(size_t) i];
        juce::String t = juce::String (s.root) + ":" + juce::String (s.type) + ":" + juce::String (s.bars);
        if (! s.loop || s.oct != 0) t << ":" << (s.loop ? 1 : 0);
        if (s.oct != 0) t << ":" << s.oct;
        parts.add (t);
    }
    return parts.joinIntoString (",");
}

Progression Progression::fromString (const juce::String& text, const Progression& fallback)
{
    Progression p;
    p.count = 0;
    for (auto& item : juce::StringArray::fromTokens (text, ",", ""))
    {
        auto f = juce::StringArray::fromTokens (item, ":", "");
        if (f.size() < 3 || f.size() > 5 || p.count >= maxSteps) continue;
        auto& s = p.steps[(size_t) p.count++];
        s.root = juce::jlimit (0, 11, f[0].getIntValue());
        s.type = juce::jlimit (0, 5, f[1].getIntValue());
        s.bars = juce::jlimit (1, 32, f[2].getIntValue());
        s.loop = f.size() < 4 || f[3].getIntValue() != 0;
        s.oct = f.size() > 4 ? juce::jlimit (minChordOct, maxChordOct, f[4].getIntValue()) : 0;
    }
    return p.count > 0 ? p : fallback;
}

Progression Progression::makeDefault()
{
    // In D: D open, B minor, G major (twice as long), A sus.
    Progression p;
    p.count = 4;
    p.steps[0] = { 0, 1, 4 };
    p.steps[1] = { 9, 2, 4 };
    p.steps[2] = { 5, 3, 8 };
    p.steps[3] = { 7, 4, 4 };
    return p;
}

juce::String chordName (int key, const ChordStep& c)
{
    return (juce::String (keyNames[(key + c.root) % 12]) + " " + chordNames[juce::jlimit (0, 5, c.type)]).toLowerCase();
}

int chordBassMidi (int key, const ChordStep& c)
{
    int base = 48 + (key + c.root) % 12;   // by itself the root sits between E2 and D#3
    if (base > 51) base -= 12;
    return base + 12 * juce::jlimit (minChordOct, maxChordOct, c.oct);
}

juce::String noteNameWithOctave (int midi)
{
    return juce::String (keyNames[((midi % 12) + 12) % 12]).toLowerCase() + juce::String (midi / 12 - 1);
}

juce::String formatValue (int p, float v)
{
    switch (p)
    {
        case txDensity: return rateText (v) + "/s";
        case txLength:  return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 1) + " s";
        case txSpread:  return v < 0.2f ? "narrow" : (v < 0.6f ? juce::String (juce::roundToInt (v * 100.0f)) : "wide");
        case drGrow:
        {
            const float tau = 600.0f * std::pow (3.0f / 600.0f, v);
            return tau >= 60.0f ? juce::String (tau / 60.0f, 1) + " min" : juce::String (juce::roundToInt (tau)) + " s";
        }
        case drColour:  return v < 0.33f ? "dark" : (v < 0.66f ? "warm" : "bright");
        case drLevel: case txLevel:
            return v <= 0.001f ? "off" : juce::String (juce::Decibels::gainToDecibels (v * v / 0.5625f), 1) + " db";
        case hmKey:     return juce::String (keyNames[juce::jlimit (0, 11, juce::roundToInt (v))]).toLowerCase();
        case txNote: case txNote1: case txNote2: case txNote3: case drNote: case bedNote:
            return noteStepName (p, juce::roundToInt (v));
        case spSize:    return v < 0.33f ? "room" : (v < 0.7f ? "hall" : "vast");
        case hmDissolve: return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 1) + " s";
        case txPos1: case txPos2: case txPos3: return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        default:        return juce::String (juce::roundToInt (v * 100.0f));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto texture = std::make_unique<AudioProcessorParameterGroup> ("texture", "Texture", " | ");
    auto drones = std::make_unique<AudioProcessorParameterGroup> ("drones", "Drones", " | ");
    auto other = std::make_unique<AudioProcessorParameterGroup> ("piece", "Piece", " | ");

    for (int p = 0; p < numP; ++p)
    {
        const auto& s = spec (p);
        std::unique_ptr<RangedAudioParameter> param;
        if (s.kind == Kind::Choice)
            param = std::make_unique<AudioParameterChoice> (ParameterID { s.id, 1 }, s.name, choicesFor (p), (int) s.def);
        else if (s.kind == Kind::Bool)
            param = std::make_unique<AudioParameterBool> (ParameterID { s.id, 1 }, s.name, s.def > 0.5f);
        else
        {
            NormalisableRange<float> range (s.min, s.max);
            if (s.curve == Curve::Exp)
            {
                const float lo = s.min, hi = s.max;
                range = NormalisableRange<float> (lo, hi,
                    [lo, hi] (float, float, float n) { return lo * std::pow (hi / lo, n); },
                    [lo, hi] (float, float, float v) { return std::log (v / lo) / std::log (hi / lo); });
            }
            const int pi = p;
            param = std::make_unique<AudioParameterFloat> (ParameterID { s.id, 1 }, s.name, range, s.def,
                AudioParameterFloatAttributes().withStringFromValueFunction ([pi] (float v, int) { return formatValue (pi, v); }));
        }
        auto& group = p < drGrow ? texture : (p < hmKey ? drones : other);
        group->addChild (std::move (param));
    }

    other->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ids::spaceMode, 1 }, "Space mode", StringArray { "Inside", "Outside" }, 0));
    other->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::freeze, 1 }, "Freeze", false));
    other->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::hold, 1 }, "Stay on each chord", true));
    // Grain Field's own tempo, for when nothing else sets one (the standalone app). In a DAW the song's tempo leads.
    other->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::tempo, 1 }, "Tempo", NormalisableRange<float> (40.0f, 180.0f, 1.0f), 80.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " bpm"; })));
    other->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::volume, 1 }, "Volume", NormalisableRange<float> (0.0f, 1.0f), 0.75f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    // Wear: the sounds erode wherever they're played, for good. Not stored in scenes.
    other->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::wear, 1 }, "Wear", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    // Arrive: one line from silence to everything, the layers coming in one after another. Not stored in scenes.
    other->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::arrive, 1 }, "Arrive", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    other->addChild (std::make_unique<AudioParameterInt> (ParameterID { ids::part, 1 }, "Scene", 0, numScenes, 1,
                                                          AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("None") : String (v); })));
    other->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ids::blend, 1 }, "Blend time",
                                                            NormalisableRange<float> (0.0f, 180.0f, 0.0f, 0.4f), 45.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return timeText (v); })));
    other->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::play, 1 }, "Play", false));
    other->addChild (std::make_unique<AudioParameterBool> (ParameterID { ids::sync, 1 }, "Play with transport", false));

    layout.add (std::move (texture), std::move (drones), std::move (other));
    return layout;
}
} // namespace gf
