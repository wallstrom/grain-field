#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

namespace gf
{
// Message-thread copy of one part. Values are normalised (0..1), exactly as the host
// sees parameters, so a part can be written straight back.
struct SceneSlot
{
    bool used = false;
    juce::String name;
    std::array<float, numP> norm {};
    std::array<juce::String, numSlots> src;
    Progression prog;
    bool shaped = false;   // you've been in it or shaped it with the pencil: it comes back exactly as you left it
    double length = 0.0;   // seconds the part lasts when the piece plays through; 0: until you move on
    float blendIn = -1.0f; // seconds the piece takes to blend into this part; below 0: the blend time set now
};

class SceneBank
{
public:
    std::array<SceneSlot, numScenes> slots;

    static SceneSlot capture (juce::AudioProcessorValueTreeState& state, const std::array<juce::String, numSlots>& sources,
                              const Progression& prog, const juce::String& name);
    int firstFree() const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree, juce::AudioProcessorValueTreeState& state);
};

// A factory piece: its sounds and its parts. Loading one replaces everything.
struct FactoryPart
{
    const char* name;
    const char* prog;  // Progression::toString format
    std::vector<std::pair<int, float>> values;
    double length = 90.0;   // seconds, when the piece plays through
};
struct FactoryPreset
{
    const char* name;
    int key;
    std::array<const char*, numSlots> sources;
    std::vector<FactoryPart> parts;
};
const std::vector<FactoryPreset>& factoryPresets();
SceneSlot makeFactoryPart (juce::AudioProcessorValueTreeState& state, const FactoryPreset& preset, const FactoryPart& part);
} // namespace gf
