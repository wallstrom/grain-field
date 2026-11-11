#include "Scenes.h"

namespace gf
{
SceneSlot SceneBank::capture (juce::AudioProcessorValueTreeState& state, const std::array<juce::String, numSlots>& sources,
                              const Progression& prog, const juce::String& name)
{
    SceneSlot s;
    s.used = true;
    s.name = name;
    for (int p = 0; p < numP; ++p)
        if (auto* param = state.getParameter (spec (p).id))
            s.norm[(size_t) p] = param->getValue();
    s.src = sources;
    s.prog = prog;
    return s;
}

int SceneBank::firstFree() const
{
    for (int i = 0; i < numScenes; ++i) if (! slots[(size_t) i].used) return i;
    return -1;
}

juce::ValueTree SceneBank::toValueTree() const
{
    juce::ValueTree t ("Parts");
    for (int i = 0; i < numScenes; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (! s.used) continue;
        juce::ValueTree c ("Part");
        c.setProperty ("index", i, nullptr);
        c.setProperty ("name", s.name, nullptr);
        c.setProperty ("prog", s.prog.toString(), nullptr);
        c.setProperty ("shaped", s.shaped, nullptr);
        c.setProperty ("length", s.length, nullptr);
        c.setProperty ("blendIn", s.blendIn, nullptr);
        for (int p = 0; p < numP; ++p)
            c.setProperty (juce::Identifier (juce::String ("p_") + spec (p).id), s.norm[(size_t) p], nullptr);
        for (int k = 0; k < numSlots; ++k)
            c.setProperty (juce::Identifier ("src" + juce::String (k)), s.src[(size_t) k], nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void SceneBank::fromValueTree (const juce::ValueTree& t, juce::AudioProcessorValueTreeState& state)
{
    if (! t.isValid()) return;
    std::array<SceneSlot, numScenes> loaded;
    for (const auto& c : t)
    {
        const int i = c.getProperty ("index", -1);
        if (i < 0 || i >= numScenes) continue;
        auto& s = loaded[(size_t) i];
        s.used = true;
        s.name = c.getProperty ("name").toString();
        s.prog = Progression::fromString (c.getProperty ("prog").toString(), Progression::makeDefault());
        s.shaped = (bool) c.getProperty ("shaped", false);
        s.length = (double) c.getProperty ("length", 0.0);
        s.blendIn = (float) c.getProperty ("blendIn", -1.0f);
        for (int p = 0; p < numP; ++p)
        {
            const juce::Identifier key (juce::String ("p_") + spec (p).id);
            auto* param = state.getParameter (spec (p).id);
            s.norm[(size_t) p] = c.hasProperty (key) ? (float) c.getProperty (key) : (param != nullptr ? param->getDefaultValue() : 0.0f);
        }
        for (int k = 0; k < numSlots; ++k)
            s.src[(size_t) k] = c.getProperty (juce::Identifier ("src" + juce::String (k))).toString();
    }
    slots = loaded;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "arc", 2, { "glasshaze", "tapechoir", "dusthalo", "bowedair", "lowroom" }, {
            { "opening", "0:1:8", { { pSwell, 0.08f }, { txDensity, 0.9f }, { txLength, 3.5f }, { drBody, 0.35f }, { hmLine, 0.15f } } },
            { "root", "0:1:8,9:2:4", { { pSwell, 0.3f }, { txSmear, 0.75f }, { txReverse, 0.5f }, { hmLine, 0.3f } } },
            { "build", "0:1:4,9:2:4,5:3:8,7:4:4", { { pSwell, 0.62f }, { txDensity, 2.6f }, { drBody, 0.7f }, { outDrive, 0.3f }, { hmLine, 0.45f } } },
            { "peak", "0:1:4,5:3:4,9:2:4,7:4:4", { { pSwell, 0.95f }, { txDensity, 4.5f }, { outDrive, 0.55f }, { txTape, 0.35f }, { drSub, 0.6f }, { txSpace, 0.75f }, { drSpace, 0.65f }, { hmLine, 0.5f } } },
            { "wind down", "0:0:8", { { pSwell, 0.18f }, { txTape, 0.7f }, { drTape, 0.55f }, { txDensity, 0.6f }, { txDetail, 0.08f }, { hmLine, 0.1f }, { spSize, 0.95f } } },
        } },
        { "haze", 9, { "glasshaze", "dusthalo", "tapechoir", "bowedair", "wind" }, {
            { "haze", "0:1:8,5:3:8", { { pSwell, 0.4f }, { txLength, 5.0f }, { txSmear, 0.8f }, { txReverse, 0.55f }, { drColour, 0.3f }, { hmLine, 0.25f }, { spSize, 0.9f } } },
            { "thaw", "0:1:8,7:4:8,5:3:8", { { pSwell, 0.55f }, { txLength, 3.0f }, { txSmear, 0.6f }, { hmLine, 0.45f }, { drColour, 0.45f } } },
        } },
        { "night rain", 4, { "dusthalo", "glasshaze", "lowhum", "tapechoir", "rain" }, {
            { "rain", "0:2:8,8:3:8", { { pSwell, 0.25f }, { txDensity, 1.2f }, { txLength, 1.4f }, { drBed, 0.6f }, { hmLine, 0.4f }, { txTape, 0.35f } } },
            { "flood", "0:2:4,8:3:4,3:3:4,10:4:4", { { pSwell, 0.75f }, { txDensity, 3.0f }, { drBed, 0.5f }, { outDrive, 0.4f }, { hmLine, 0.5f } } },
        } },
        { "wall", 7, { "tapechoir", "bowedair", "glasshaze", "lowhum", "lowroom" }, {
            { "wall", "0:1:8,10:3:8", { { pSwell, 0.92f }, { txDensity, 6.0f }, { txLength, 4.0f }, { outDrive, 0.7f }, { txTape, 0.4f }, { drTape, 0.3f }, { drSub, 0.7f }, { drBody, 0.9f }, { txSpace, 0.8f }, { drSpace, 0.7f } } },
        } },
    };
    return presets;
}

SceneSlot makeFactoryPart (juce::AudioProcessorValueTreeState& state, const FactoryPreset& preset, const FactoryPart& part)
{
    SceneSlot s;
    s.used = true;
    s.name = part.name;
    for (int p = 0; p < numP; ++p)
        if (auto* param = state.getParameter (spec (p).id))
            s.norm[(size_t) p] = param->getDefaultValue();
    s.norm[hmKey] = state.getParameterRange (spec (hmKey).id).convertTo0to1 ((float) preset.key);
    for (auto& kv : part.values)
        s.norm[(size_t) kv.first] = state.getParameterRange (spec (kv.first).id).convertTo0to1 (kv.second);
    for (int k = 0; k < numSlots; ++k) s.src[(size_t) k] = preset.sources[(size_t) k];
    s.prog = Progression::fromString (part.prog, Progression::makeDefault());
    s.length = part.length;
    return s;
}
} // namespace gf
