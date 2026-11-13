#include <functional>
#include <set>
// Offline test: renders Grain Field without a host or window, checks the output
// is healthy, measures CPU cost and writes WAV files for listening.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "../Source/PluginProcessor.h"

using namespace gf;

namespace
{
struct Stats { float peak = 0, rms = 0; bool finite = true; };

Stats measure (const juce::AudioBuffer<float>& b, int ch0 = 0, int numCh = 2, int start = 0)
{
    Stats s; double sum = 0;
    const int n = b.getNumSamples() - start;
    for (int c = ch0; c < ch0 + numCh; ++c)
        for (int i = start; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (v));
            sum += (double) v * v;
        }
    s.rms = (float) std::sqrt (sum / juce::jmax (1, numCh * n));
    return s;
}

void writeWav (const juce::AudioBuffer<float>& b, double sr, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (16);
    if (auto w = wav.createWriterFor (os, opts)) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

void setP (GrainFieldProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}
void setP (GrainFieldProcessor& p, int index, float value) { setP (p, spec (index).id, value); }

constexpr double sr = 48000.0;
constexpr int block = 256;

void pump (int ms = 5) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

// A host's transport, for the checks that follow a song's timeline.
struct FakeHead : juce::AudioPlayHead
{
    double time = 0.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setTimeInSeconds (time);
        info.setTimeInSamples ((juce::int64) (time * 48000.0));
        info.setBpm (80.0);
        info.setPpqPosition (time * 80.0 / 60.0);
        return info;
    }
};
FakeHead* fakeHead = nullptr;

// What a track sends into the side chain (silence unless a test sets it).
std::function<float (int channel, long long sample)> sideChain;

juce::AudioBuffer<float> render (GrainFieldProcessor& proc, double seconds, double& cpuSeconds, int channels = 2)
{
    static long long sideClock = 0;
    const int total = (int) (seconds * sr), chans = juce::jmax (channels, proc.getTotalNumOutputChannels());
    juce::AudioBuffer<float> out (chans, total), buf (chans, block);
    juce::MidiBuffer midi;
    cpuSeconds = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (chans, n, false, false, true);
        buf.clear();
        if (sideChain)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i) buf.setSample (c, i, sideChain (c, sideClock + i));
        sideClock += n;
        const auto t0 = juce::Time::getHighResolutionTicks();
        proc.processBlock (buf, midi);
        cpuSeconds += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        if (fakeHead != nullptr && fakeHead->playing) fakeHead->time += (double) n / sr;
        for (int c = 0; c < chans; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        if ((pos / block) % 8 == 0) pump (1);
    }
    return out;
}

void stereoOnly (GrainFieldProcessor& p)
{
    p.loadPreset (0);   // tests start from the first piece, playing
    if (auto* pl = p.apvts.getParameter (ids::play)) pl->setValueNotifyingHost (1.0f);
    if (auto* h = p.apvts.getParameter (ids::hold)) h->setValueNotifyingHost (0.0f);   // tests let the chords move on
    auto layout = p.getBusesLayout();
    for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference (i) = juce::AudioChannelSet::disabled();
    p.setBusesLayout (layout);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const bool quick = argc > 2 && juce::String (argv[2]) == "quick";
    int failures = 0;
    auto check = [&] (const char* name, bool ok, const juce::String& detail) {
        std::printf ("%-32s %-4s %s\n", name, ok ? "OK" : "FAIL", detail.toRawUTF8());
        if (! ok) ++failures;
    };
    auto levels = [&] (const char* name, const juce::AudioBuffer<float>& b, double cpu, double seconds, float minRms) {
        const auto s = measure (b, 0, 2, b.getNumSamples() / 2);
        check (name, s.finite && s.peak <= 1.0f && s.rms >= minRms,
               juce::String::formatted ("peak %.3f  rms %.4f (%.1f dBFS)  cpu %.1f%%", s.peak, s.rms, juce::Decibels::gainToDecibels (s.rms), cpu / seconds * 100.0));
    };

    const auto tLoad = juce::Time::getMillisecondCounterHiRes();
    GrainFieldProcessor proc;
    std::printf ("%-32s %.0f ms\n", "Time to load the plugin", juce::Time::getMillisecondCounterHiRes() - tLoad);
    stereoOnly (proc);
    proc.prepareToPlay (sr, block);
    double cpu = 0;

    // 1. The first part of the default piece: quiet, sparse.
    auto a = render (proc, quick ? 20.0 : 40.0, cpu);
    levels ("Opening, left running", a, cpu, quick ? 20.0 : 40.0, 0.003f);
    check ("Drones grow", proc.engine.getMass() > 0.04f, juce::String::formatted ("mass %.2f", proc.engine.getMass()));
    writeWav (a, sr, outDir.getChildFile ("test_opening.wav"));

    // 2. Move on through every part, blending over 3 seconds.
    setP (proc, ids::blend, 3.0f);
    for (int k = 1; k < 5; ++k)
    {
        proc.moveOn();
        pump (20);
        auto b = render (proc, quick ? 10.0 : 16.0, cpu);
        const auto name = "Part " + juce::String (proc.getActivePart() + 1) + " " + proc.getScenes().slots[(size_t) proc.getActivePart()].name;
        levels (name.toRawUTF8(), b, cpu, quick ? 10.0 : 16.0, 0.003f);
        writeWav (b, sr, outDir.getChildFile ("test_part" + juce::String (k + 1) + ".wav"));
    }
    check ("Move on reaches the parameters", std::abs (proc.apvts.getRawParameterValue (spec (pSwell).id)->load() - 0.18f) < 0.02f,
           juce::String::formatted ("swell %.2f, in part %d of %d", proc.apvts.getRawParameterValue (spec (pSwell).id)->load(), proc.getActivePart() + 1, proc.getNumParts()));

    // 3. Worst case: everything up.
    for (auto [p, v] : std::initializer_list<std::pair<int, float>> { { pSwell, 1 }, { txDensity, 12 }, { txLength, 8 }, { txDetail, 1 }, { outDrive, 1 },
             { drBody, 1 }, { drSub, 1 }, { hmLine, 1 }, { txTape, 1 }, { drTape, 1 }, { txFeed, 1 }, { txSpace, 1 }, { drSpace, 1 }, { spEcho, 1 } })
        setP (proc, p, v);
    auto w = render (proc, 10.0, cpu);
    levels ("Worst case (all up)", w, cpu, 10.0, 0.01f);
    std::printf ("%-32s %d\n", "Grains active at the end", proc.engine.getActiveGrains());

    // 4. The progression moves with the bars (80 bpm when there is no host: a 4-bar chord lasts 12 s).
    {
        GrainFieldProcessor pp;
        stereoOnly (pp);
        pp.prepareToPlay (sr, block);
        pp.setProgression (gf::Progression::makeDefault());
        render (pp, 1.0, cpu);
        const int first = pp.engine.getStepIndex();
        render (pp, 13.0, cpu);
        check ("Progression moves on", pp.engine.getStepIndex() != first, juce::String::formatted ("step %d -> %d", first, pp.engine.getStepIndex()));
    }

    // 5. Resample: the last 8 seconds become a sound in the texture.
    proc.clearSlot (2);
    proc.resample();
    render (proc, 0.5, cpu);
    for (int i = 0; i < 20; ++i) pump (20);
    check ("Resample", proc.getSlotSourceId (2).startsWith ("file:") || proc.getSlotSourceId (2).startsWith ("capture:"), "texture sound 3 is " + proc.getSlotSourceId (2));

    // 6. State save and restore.
    {
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        GrainFieldProcessor p2;
        p2.setStateInformation (state.getData(), (int) state.getSize());
        const bool same = std::abs (p2.apvts.getRawParameterValue (spec (txDetail).id)->load() - 1.0f) < 1.0e-5f
                          && p2.getSlotSourceId (2) == proc.getSlotSourceId (2) && p2.getScenes().slots[4].used
                          && p2.getProgression() == proc.getProgression() && p2.getActivePart() == proc.getActivePart();
        check ("State save and restore", same, juce::String ((int) state.getSize()) + " bytes");
    }

    // 7. Separate outputs, and the space moved Outside.
    {
        GrainFieldProcessor mp;
        mp.loadPreset (0);
        setP (mp, ids::play, 1.0f);
        setP (mp, ids::hold, 0.0f);
        mp.prepareToPlay (sr, block);
        setP (mp, pSwell, 0.7f);
        auto mo = render (mp, 12.0, cpu, 8);
        const auto mainS = measure (mo, 0, 2, mo.getNumSamples() / 2), txS = measure (mo, 2, 2, mo.getNumSamples() / 2),
                   drS = measure (mo, 4, 2, mo.getNumSamples() / 2), seS = measure (mo, 6, 2, mo.getNumSamples() / 2);
        check ("Four outputs all carry sound", mo.getNumChannels() == 8 && txS.rms > 0.002f && drS.rms > 0.002f && seS.rms > 0.002f,
               juce::String::formatted ("main %.3f texture %.3f drones %.3f send %.3f", mainS.rms, txS.rms, drS.rms, seS.rms));
        setP (mp, ids::spaceMode, 1);
        auto dry = render (mp, 6.0, cpu, 8);
        const auto dryS = measure (dry, 0, 2, dry.getNumSamples() / 2), sendS = measure (dry, 6, 2, dry.getNumSamples() / 2);
        check ("Outside: main is dry, Send live", dryS.rms > 0.002f && sendS.rms > 0.002f && dryS.finite,
               juce::String::formatted ("main %.3f send %.3f", dryS.rms, sendS.rms));
    }

    // 8. Texture alone, for checking the tuning.
    {
        GrainFieldProcessor tp;
        stereoOnly (tp);
        tp.prepareToPlay (sr, block);
        setP (tp, drLevel, 0.0f); setP (tp, drBed, 0.0f); setP (tp, txSpace, 0.0f); setP (tp, spEcho, 0.0f);
        setP (tp, txDetail, 0.0f); setP (tp, pSwell, 0.6f);
        gf::Progression one; one.count = 1; one.steps[0] = { 0, 1, 16 };
        tp.setProgression (one);
        auto tb = render (tp, 20.0, cpu);
        writeWav (tb, sr, outDir.getChildFile ("test_tuned.wav"));
        check ("Texture alone renders", measure (tb).rms > 0.002f, juce::String::formatted ("rms %.4f", measure (tb).rms));
        // Each texture on its own, for checking its note map.
        for (auto* id : { "all", "glasshaze", "tapechoir", "dusthalo", "lowhum", "bowedair", "glassdrone" })
        {
            GrainFieldProcessor sp;
            stereoOnly (sp);
            sp.prepareToPlay (sr, block);
            setP (sp, drLevel, 0.0f); setP (sp, drBed, 0.0f); setP (sp, txSpace, 0.0f); setP (sp, spEcho, 0.0f);
            setP (sp, txDetail, 0.0f); setP (sp, pSwell, 0.6f);
            sp.setProgression (one);
            if (juce::String (id) != "all") { sp.setSlotSourceId (0, id); sp.setSlotSourceId (1, "none"); sp.setSlotSourceId (2, "none"); }
            auto solo = render (sp, 20.0, cpu);
            writeWav (solo, sr, outDir.getChildFile (juce::String ("test_tuned_") + id + ".wav"));
        }
    }

    // 8a. It opens empty and stopped; stop is silence; moving on keeps what you've done.
    {
        GrainFieldProcessor ep;
        ep.prepareToPlay (sr, block);
        const auto quietStart = measure (render (ep, 3.0, cpu));
        bool empty = true;
        for (int k = 0; k < numSlots; ++k) empty = empty && ep.getSlotSourceId (k) == "none";
        check ("Opens empty and stopped", quietStart.rms < 1.0e-6f && empty && ! ep.engine.isRunning(),
               juce::String::formatted ("rms %.7f", quietStart.rms));

        GrainFieldProcessor sp;
        stereoOnly (sp);
        sp.prepareToPlay (sr, block);
        render (sp, 15.0, cpu);
        setP (sp, ids::play, 0.0f);
        auto after = render (sp, 4.0, cpu);
        const auto tail = measure (after, 0, 2, (int) (2.0 * sr));
        check ("Stop fades to silence", tail.rms < 1.0e-6f, juce::String::formatted ("rms after 2 s %.7f", tail.rms));

        // Change key, a sound and a knob in the first part, then move on: the next part keeps them.
        setP (sp, ids::play, 1.0f);
        setP (sp, hmKey, 7.0f);
        setP (sp, txLength, 6.0f);
        sp.setSlotSourceId (1, "rain");
        setP (sp, ids::blend, 0.0f);
        render (sp, 2.0, cpu);
        sp.moveOn(); pump (20);
        render (sp, 2.0, cpu); pump (20);
        const float keyNow = sp.apvts.getRawParameterValue (spec (hmKey).id)->load();
        const float lenNow = sp.apvts.getRawParameterValue (spec (txLength).id)->load();
        const float swellNow = sp.apvts.getRawParameterValue (spec (pSwell).id)->load();
        check ("Moving on keeps your key, sounds and knobs",
               juce::roundToInt (keyNow) == 7 && std::abs (lenNow - 6.0f) < 0.05f && sp.getSlotSourceId (1) == "rain" && std::abs (swellNow - 0.3f) < 0.02f,
               juce::String::formatted ("key %.0f, length %.2f, swell %.2f (the part's own), slot 2 ", keyNow, lenNow, swellNow) + sp.getSlotSourceId (1));
    }

    // 8e. The side chain: a track sent in becomes the sound everything is made from.
    {
        GrainFieldProcessor lp;
        stereoOnly (lp);
        lp.prepareToPlay (sr, block);
        lp.setRootSound ("live");
        setP (lp, pSwell, 0.6f);
        sideChain = [] (int, long long i)
        {
            const double t = (double) i / sr;
            return (float) (0.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 196.0 * t) * (0.6 + 0.4 * std::sin (t * 0.5)));
        };
        const auto first = measure (render (lp, 12.0, cpu));
        sideChain = nullptr;
        const auto later = measure (render (lp, 20.0, cpu), 0, 2, (int) (16.0 * sr));   // eight seconds after the track stops, then more
        bool allLive = true;
        for (int k = 0; k < numSlots; ++k) allLive = allLive && lp.getSlotSourceId (k) == "live";
        check ("Side chain becomes the sound", allLive && first.rms > 0.01f && first.finite && first.peak <= 1.0f,
               juce::String::formatted ("with input rms %.3f, long after it stops %.4f", first.rms, later.rms));
    }

    // 8b. Hold and picking the next chord.
    {
        GrainFieldProcessor hp;
        stereoOnly (hp);
        hp.prepareToPlay (sr, block);
        hp.setProgression (Progression::makeDefault());
        setP (hp, ids::hold, 1.0f);
        render (hp, 1.0, cpu);
        const int s0 = hp.engine.getStepIndex();
        render (hp, 40.0, cpu);   // ten bars at 80 bpm: held, so it stays
        const int s1 = hp.engine.getStepIndex();
        hp.engine.cueStep (2);
        render (hp, 4.0, cpu);    // the next bar line comes within three seconds
        const int s2 = hp.engine.getStepIndex();
        check ("Hold, then pick a chord", s0 == s1 && s2 == 2 && hp.engine.getCue() < 0,
               juce::String (s0) + " -> " + juce::String (s1) + " held, picked -> " + juce::String (s2));
    }

    // 8b2. Loop: chords that are out of the loop are passed over, until you queue one.
    {
        GrainFieldProcessor lp;
        stereoOnly (lp);
        lp.prepareToPlay (sr, block);
        auto pr = Progression::fromString ("0:1:1,5:3:1:0,7:4:1", Progression::makeDefault());
        lp.setProgression (pr);
        const bool kept = Progression::fromString (pr.toString(), Progression::makeDefault()) == pr && ! pr.steps[1].loop;
        bool sawOut = false, saw0 = false, saw2 = false;
        for (int k = 0; k < 60; ++k)   // thirty seconds: ten bars at 80 bpm
        {
            render (lp, 0.5, cpu);
            const int st = lp.engine.getStepIndex();
            sawOut = sawOut || st == 1; saw0 = saw0 || st == 0; saw2 = saw2 || st == 2;
        }
        lp.engine.cueStep (1, 2);
        render (lp, 0.3, cpu);
        const bool queued = lp.engine.getStepIndex() == 1;
        check ("Loop skips chords that are out", kept && saw0 && saw2 && ! sawOut && queued,
               juce::String ("cycled 1 and 3 only: ") + (saw0 && saw2 && ! sawOut ? "yes" : "no") + ", queued 2 plays: " + (queued ? "yes" : "no"));
    }

    // 8c. Chord changes dissolve: no voice slides in pitch.
    {
        GrainFieldProcessor dp;
        stereoOnly (dp);
        dp.prepareToPlay (sr, block);
        setP (dp, drGrow, 0.9f);
        setP (dp, ids::hold, 1.0f);
        render (dp, 20.0, cpu);
        dp.engine.cueStep (1);
        float worst = 0.0f;
        std::array<float, 10> before {};
        for (int v = 0; v < 10; ++v) before[(size_t) v] = dp.engine.getVoiceHz (v);
        for (int k = 0; k < 120; ++k)
        {
            render (dp, 0.05, cpu);
            for (int v = 0; v < 10; ++v)
            {
                const float hz = dp.engine.getVoiceHz (v), b = before[(size_t) v];
                if (b > 1.0f && hz > 1.0f && std::abs (hz / b - 1.0f) > 0.002f)
                {
                    // A change must be a jump to the new note (taken in silence), never a slide through others.
                    const float target = dp.engine.getVoiceTargetHz (v);
                    if (std::abs (hz / target - 1.0f) > 0.002f) worst = juce::jmax (worst, std::abs (hz / b - 1.0f));
                }
                before[(size_t) v] = hz;
            }
        }
        check ("Chords dissolve, nothing slides", worst < 0.001f, juce::String::formatted ("largest slide %.4f", worst));
    }


    // 8f. Scenes: go straight to any of them; nothing moves on by itself.
    {
        GrainFieldProcessor ap;
        stereoOnly (ap);
        ap.prepareToPlay (sr, block);
        setP (ap, ids::blend, 0.0f);
        const int partsAtStart = ap.getNumParts();
        render (ap, 3.0, cpu);
        ap.moveOn(); pump (40); render (ap, 4.0, cpu); pump (40);
        ap.moveOn(); pump (40); render (ap, 1.0, cpu); pump (40);
        setP (ap, txLength, 5.5f);                       // something done to part 3
        ap.goToPart (0); pump (40); render (ap, 1.0, cpu); pump (40);
        const bool back = ap.getActivePart() == 0 && ap.getNumParts() == partsAtStart && ap.engine.getCurPart() == 0;
        const float swellBack = ap.apvts.getRawParameterValue (spec (pSwell).id)->load();
        ap.goToPart (2); pump (40); render (ap, 1.0, cpu); pump (40);
        const float lenKept = ap.apvts.getRawParameterValue (spec (txLength).id)->load();
        check ("Go back to a part, nothing new", back && std::abs (lenKept - 5.5f) < 0.05f && ap.getActivePart() == 2,
               juce::String::formatted ("parts %d -> %d, back in part 1 with swell %.2f, part 3 kept length %.1f", partsAtStart, ap.getNumParts(), swellBack, lenKept));
        // Nothing moves on by itself: half a minute later, still in the same scene.
        render (ap, 30.0, cpu); pump (40);
        const bool stays = ap.getActivePart() == 2 && ap.engine.getCurPart() == 2;
        // Your DAW automating the Scene parameter blends into that scene, and the interface follows.
        if (auto* sc = ap.apvts.getParameter (ids::part)) sc->setValueNotifyingHost (sc->convertTo0to1 (1.0f));
        render (ap, 1.0, cpu); pump (40);
        const bool automated = ap.getActivePart() == 0 && ap.engine.getCurPart() == 0;
        check ("Scenes stay until you, or the DAW, move", stays && automated,
               juce::String::formatted ("after 30 s still in scene %d; automation took it to scene %d", 3, ap.getActivePart() + 1));

        // A new part at the end, and removing one in the middle.
        ap.goToPart (1); pump (40); render (ap, 0.5, cpu); pump (40);
        const int n0 = ap.getNumParts();
        ap.deletePart (0); pump (40); render (ap, 0.5, cpu); pump (40);
        check ("Removing a part keeps you where you are", ap.getNumParts() == n0 - 1 && ap.getActivePart() == 0 && ap.engine.getCurPart() == 0
               && ap.getScenes().slots[0].name == "root", "now part 1 is " + ap.getScenes().slots[0].name);
    }

    // 8f2. With sync on, Grain Field plays with the song, and the song's position never moves the scene.
    {
        GrainFieldProcessor hp;
        stereoOnly (hp);
        hp.prepareToPlay (sr, block);
        FakeHead head;
        hp.setPlayHead (&head);
        fakeHead = &head;
        setP (hp, ids::sync, 1.0f);
        hp.goToPart (1); pump (40);
        render (hp, 1.0, cpu); pump (40);
        const bool waits = ! hp.engine.isRunning();
        head.time = 12.0; head.playing = true;
        render (hp, 2.0, cpu); pump (60);
        const bool plays = hp.engine.isRunning();
        head.time = 300.0;
        render (hp, 1.0, cpu); pump (60);
        const int a = hp.getActivePart();
        fakeHead = nullptr;
        hp.setPlayHead (nullptr);
        check ("Plays with the song, scene stays", waits && plays && a == 1,
               juce::String::formatted ("waited: %s, played with the song: %s, scene after a jump: %d", waits ? "yes" : "no", plays ? "yes" : "no", a + 1));
    }

    // 8g. A change of key is a crossfade: the drones never drop out and nothing slides.
    {
        GrainFieldProcessor kp;
        stereoOnly (kp);
        kp.prepareToPlay (sr, block);
        setP (kp, drGrow, 0.9f); setP (kp, ids::hold, 1.0f); setP (kp, txLevel, 0.0f); setP (kp, pSwell, 0.6f);
        setP (kp, hmDissolve, 4.0f); setP (kp, drBed, 0.0f);
        render (kp, 30.0, cpu);
        const float beforeRms = measure (render (kp, 2.0, cpu)).rms;
        std::array<float, 10> hz0 {};
        for (int v = 0; v < 10; ++v) hz0[(size_t) v] = kp.engine.getVoiceHz (v);
        setP (kp, hmKey, (float) ((juce::roundToInt (kp.apvts.getRawParameterValue (spec (hmKey).id)->load()) + 5) % 12));
        float lowest = 1.0e9f, slide = 0.0f;
        bool sawBoth = false;
        juce::AudioBuffer<float> change (2, (int) (8.0 * sr));
        for (int k = 0; k < 16; ++k)
        {
            auto win = render (kp, 0.5, cpu);
            for (int c = 0; c < 2; ++c) change.copyFrom (c, (int) (k * 0.5 * sr), win, c, 0, win.getNumSamples());
            lowest = juce::jmin (lowest, measure (win).rms);
            for (int v = 0; v < 10; ++v)
            {
                const float hz = kp.engine.getVoiceHz (v), x = kp.engine.getVoiceXfade (v);
                if (x > 0.05f && x < 0.95f && std::abs (kp.engine.getVoiceOldHz (v) / hz0[(size_t) v] - 1.0f) < 0.002f) sawBoth = true;
                // A voice is only ever on its old note or its new one.
                if (std::abs (hz / hz0[(size_t) v] - 1.0f) > 0.002f && std::abs (hz / kp.engine.getVoiceTargetHz (v) - 1.0f) > 0.002f)
                    slide = juce::jmax (slide, std::abs (hz / hz0[(size_t) v] - 1.0f));
            }
        }
        writeWav (change, sr, outDir.getChildFile ("test_key_change.wav"));
        bool arrived = true;
        for (int v = 0; v < 9; ++v) arrived = arrived && kp.engine.getVoiceXfade (v) >= 1.0f;
        check ("Key change crossfades", sawBoth && slide < 0.001f && lowest > beforeRms * 0.45f && arrived,
               juce::String::formatted ("level before %.3f, lowest during %.3f, old and new sound together: %s", beforeRms, lowest, sawBoth ? "yes" : "no"));
    }

    // 8g2. Arrive: from silence, the layers come in one after another. Drones alone stay silent while the
    // texture is already arriving, and everything is there at the top.
    {
        auto layer = [&] (bool drones, float arriveAt)
        {
            GrainFieldProcessor ap;
            stereoOnly (ap);
            ap.prepareToPlay (sr, block);
            setP (ap, drGrow, 0.9f); setP (ap, pSwell, 0.6f); setP (ap, drBed, 0.0f);
            setP (ap, drones ? txLevel : drLevel, 0.0f);
            setP (ap, ids::arrive, arriveAt);
            render (ap, 20.0, cpu);
            return measure (render (ap, 2.0, cpu)).rms;
        };
        const float silent = layer (false, 0.0f), txEarly = layer (false, 0.35f), drEarly = layer (true, 0.25f);
        const float txFull = layer (false, 1.0f), drFull = layer (true, 1.0f);
        {
            // A fade-in from silence over 40 seconds, for listening.
            GrainFieldProcessor ap;
            stereoOnly (ap);
            ap.prepareToPlay (sr, block);
            setP (ap, pSwell, 0.55f); setP (ap, ids::arrive, 0.0f);
            render (ap, 1.0, cpu);
            juce::AudioBuffer<float> sweep (2, (int) (48.0 * sr));
            for (int k = 0; k < 96; ++k)
            {
                setP (ap, ids::arrive, juce::jmin (1.0f, (float) k / 80.0f));
                auto win = render (ap, 0.5, cpu);
                for (int c = 0; c < 2; ++c) sweep.copyFrom (c, (int) (k * 0.5 * sr), win, c, 0, win.getNumSamples());
            }
            writeWav (sweep, sr, outDir.getChildFile ("test_arrive.wav"));
        }
        check ("Arrive: layers come in one after another", silent < 1.0e-4f && txEarly > txFull * 0.1f && drEarly < drFull * 0.02f && drFull > 0.01f,
               juce::String::formatted ("at 0: %.5f | at 35%% texture %.4f of %.4f | at 25%% drones %.5f of %.4f", silent, txEarly, txFull, drEarly, drFull));
    }

    // 8g3. Voice: the drones made of their own sound. At either end the drones are about as loud, and
    // different sounds give different drones.
    {
        auto drones = [&] (const char* sound, float voice, const char* wav)
        {
            GrainFieldProcessor vp;
            stereoOnly (vp);
            vp.prepareToPlay (sr, block);
            setP (vp, drGrow, 0.9f); setP (vp, pSwell, 0.6f); setP (vp, drBed, 0.0f); setP (vp, txLevel, 0.0f); setP (vp, txFeed, 0.0f);
            setP (vp, drVoice, voice);
            vp.setSlotSourceId (slotTone, sound);
            render (vp, 20.0, cpu);
            auto b = render (vp, 6.0, cpu);
            if (wav != nullptr) writeWav (b, sr, outDir.getChildFile (wav));
            return measure (b).rms;
        };
        const float strings = drones ("glasshaze", 0.0f, "test_voice_strings_glasshaze.wav");
        const float glass = drones ("glasshaze", 1.0f, "test_voice_glasshaze.wav");
        const float choir = drones ("tapechoir", 1.0f, "test_voice_tapechoir.wav");
        const float wind = drones ("wind", 1.0f, "test_voice_wind.wav");
        drones ("bowedair", 1.0f, "test_voice_bowedair.wav");
        drones ("lowhum", 1.0f, "test_voice_lowhum.wav");
        const float ratio = glass / juce::jmax (1.0e-6f, strings);
        check ("Voice: drones made of their own sound", ratio > 0.5f && ratio < 2.0f && choir > 0.01f && wind > 0.01f,
               juce::String::formatted ("strings %.4f, glass %.4f (x%.2f), choir %.4f, wind %.4f", strings, glass, ratio, choir, wind));
    }

    // 8g4. Wear: the sounds become the piece's own copies, which erode as they play. The originals stay.
    {
        GrainFieldProcessor wp;
        stereoOnly (wp);
        wp.prepareToPlay (sr, block);
        wp.setSlotSourceId (0, "glasshaze"); wp.setSlotSourceId (1, "none"); wp.setSlotSourceId (2, "none");
        setP (wp, txDensity, 8.0f); setP (wp, txLength, 3.0f);
        auto original = wp.library.find ("glasshaze");
        auto air = [] (const juce::AudioBuffer<float>& b)   // how much is in a sound's highs
        {
            double d = 0;
            const float* x = b.getReadPointer (0);
            for (int i = 1; i < b.getNumSamples(); ++i) d += (double) (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
            return (float) d;
        };
        const juce::AudioBuffer<float> before (original->audio);
        const float airBefore = air (before);
        setP (wp, ids::wear, 1.0f);
        pump (100);
        const juce::String id = wp.getSlotSourceId (0);
        auto worn = wp.library.find (id);
        render (wp, 90.0, cpu);
        pump (50);
        const bool own = worn != nullptr && worn->worn && worn.get() != original.get();
        const float airAfter = own ? air (worn->audio) : airBefore;
        bool untouched = true;
        for (int i = 0; i < before.getNumSamples(); i += 97) untouched = untouched && before.getSample (0, i) == original->audio.getSample (0, i);
        // Saved with the piece: the file holds the eroded sound.
        juce::MemoryBlock state;
        wp.getStateInformation (state);
        GrainFieldProcessor back;
        back.setStateInformation (state.getData(), (int) state.getSize());
        auto reloaded = back.library.find (back.getSlotSourceId (0));
        const float airReloaded = reloaded != nullptr ? air (reloaded->audio) : 0.0f;
        check ("Wear erodes the piece's own copy, for good", own && untouched && airAfter < airBefore * 0.95f && reloaded != nullptr && reloaded->worn
                   && std::abs (airReloaded - airAfter) < airAfter * 0.02f,
               juce::String::formatted ("highs %.1f -> %.1f, original untouched: %s, reopened with the wear: %.1f", airBefore, airAfter, untouched ? "yes" : "no", airReloaded)
                   + " | " + id.fromLastOccurrenceOf ("/", false, false));
        {
            // For listening: the texture as it starts wearing, then after three more minutes of it.
            juce::AudioBuffer<float> both (2, (int) (31.0 * sr));
            both.clear();
            auto early = render (wp, 15.0, cpu);
            render (wp, 180.0, cpu);
            auto late = render (wp, 15.0, cpu);
            for (int c = 0; c < 2; ++c) { both.copyFrom (c, 0, early, c, 0, early.getNumSamples()); both.copyFrom (c, (int) (16.0 * sr), late, c, 0, late.getNumSamples()); }
            writeWav (both, sr, outDir.getChildFile ("test_wear.wav"));
        }
        if (id.startsWith ("file:")) juce::File (id.fromFirstOccurrenceOf ("file:", false, false)).deleteFile();
    }

    // 8h. Registers: the same chord an octave up, and the notes a slot shows.
    {
        GrainFieldProcessor rp;
        stereoOnly (rp);
        rp.prepareToPlay (sr, block);
        setP (rp, ids::hold, 1.0f); setP (rp, hmKey, 4.0f); setP (rp, hmDissolve, 0.3f);
        gf::Progression pr; pr.count = 2; pr.steps[0] = { 0, 0, 4 }; pr.steps[1] = { 0, 0, 4, true, 1 };
        rp.setProgression (pr);
        render (rp, 3.0, cpu);
        const float low = rp.engine.getVoiceHz (0);
        rp.engine.cueStep (1, 2);
        render (rp, 3.0, cpu);
        const float high = rp.engine.getVoiceHz (0);
        const auto shown = gf::noteNameWithOctave (gf::chordBassMidi (4, pr.steps[0])) + " then " + gf::noteNameWithOctave (gf::chordBassMidi (4, pr.steps[1]));
        check ("A chord's register", std::abs (high / low - 2.0f) < 0.01f && gf::chordBassMidi (4, pr.steps[1]) - gf::chordBassMidi (4, pr.steps[0]) == 12,
               juce::String::formatted ("lowest voice %.1f Hz -> %.1f Hz, shown as ", low, high) + shown);
    }

    // 8i. Where the grains are taken from follows each sound's window.
    {
        GrainFieldProcessor wp;
        stereoOnly (wp);
        wp.prepareToPlay (sr, block);
        wp.setSlotSourceId (0, "rain"); wp.setSlotSourceId (1, "none"); wp.setSlotSourceId (2, "none");
        setP (wp, txSmear, 0.0f); setP (wp, txDensity, 6.0f); setP (wp, pSwell, 0.5f);
        auto spread = [&] (float pos, float& mean) {
            setP (wp, txPos1, pos);
            render (wp, 1.0, cpu);
            std::array<gf::VisEvent, 256> ev;
            while (wp.engine.popEvents (ev.data(), 256) > 0) {}
            float sum = 0.0f, dev = 0.0f; int n = 0;
            for (int k = 0; k < 12; ++k)
            {
                render (wp, 0.5, cpu);
                const int got = wp.engine.popEvents (ev.data(), 256);
                for (int i = 0; i < got; ++i) if (ev[(size_t) i].slot == 0) { sum += ev[(size_t) i].x; dev += std::abs (ev[(size_t) i].x - pos); ++n; }
            }
            mean = n > 0 ? sum / (float) n : -1.0f;
            return n > 0 ? dev / (float) n : 1.0f;
        };
        float m1 = 0, m2 = 0;
        const float d1 = spread (0.2f, m1), d2 = spread (0.8f, m2);
        check ("Grains come from the window", d1 < 0.06f && d2 < 0.06f && std::abs (wp.engine.getReadPos (0) - 0.8f) < 0.05f,
               juce::String::formatted ("window at 20%%: grains around %.0f%%; at 80%%: around %.0f%%", m1 * 100.0f, m2 * 100.0f));
    }

    // 8k. Notes in layers: the root, the texture and each of its sounds, the drones.
    {
        GrainFieldProcessor np;
        stereoOnly (np);
        np.prepareToPlay (sr, block);
        setP (np, ids::hold, 1.0f); setP (np, txDensity, 8.0f); setP (np, txDetail, 0.0f); setP (np, pSwell, 0.5f);
        setP (np, hmDissolve, 0.3f); setP (np, drGrow, 0.9f);
        render (np, 3.0, cpu);
        // Which notes each texture sound's grains land on, and whether any were left untuned.
        auto listen = [&] (int slot, const std::set<int>& want, int& total, int& outside, int& untouched)
        {
            std::array<gf::VisEvent, 256> ev;
            render (np, 2.0, cpu);
            while (np.engine.popEvents (ev.data(), 256) > 0) {}
            total = outside = untouched = 0;
            for (int k = 0; k < 12; ++k)
            {
                render (np, 0.5, cpu);
                const int got = np.engine.popEvents (ev.data(), 256);
                for (int i = 0; i < got; ++i)
                {
                    const auto& e = ev[(size_t) i];
                    if (e.type != gf::evCloud || e.slot != slot) continue;
                    ++total;
                    if (e.midi < 0.0f && std::abs (e.semis) < 1.0e-4f) { ++untouched; continue; }
                    if (want.count (((juce::roundToInt (e.midi) % 12) + 12) % 12) == 0) ++outside;
                }
            }
        };
        int t0, o0, u0, t1, o1, u1, t2, o2, u2, t3, o3, u3;
        // D open is d e f# a. A fifth above, the texture plays a b c# e.
        listen (0, { 2, 4, 6, 9 }, t0, o0, u0);
        setP (np, txNote, 1.0f);
        listen (0, { 9, 11, 1, 4 }, t1, o1, u1);
        // Sound 2 at its own pitch: none of its grains are moved.
        setP (np, txNote2, 5.0f);
        listen (1, {}, t2, o2, u2);
        // The root to E: sound 1 follows (E open a fifth up: b c# d# f#); sound 2 still stays as it is.
        setP (np, hmKey, 4.0f);
        listen (0, { 11, 1, 3, 6 }, t3, o3, u3);
        int t4, o4, u4;
        listen (1, {}, t4, o4, u4);
        check ("Each texture sound plays where you set it", t0 > 5 && o0 == 0 && t1 > 5 && o1 == 0 && t2 > 5 && u2 == t2 && t3 > 5 && o3 == 0 && t4 > 5 && u4 == t4,
               juce::String::formatted ("on the root %d/%d, texture +5th %d/%d, own pitch untouched %d/%d, root to e %d/%d and %d/%d",
                                        t0 - o0, t0, t1 - o1, t1, u2, t2, t3 - o3, t3, u4, t4));

        // The drones: an octave down, then a fifth above the root. Each is a crossfade, like a chord change.
        const float hz0 = np.engine.getVoiceHz (0);
        setP (np, drNote, 4.0f);
        render (np, 4.0, cpu);
        const float hzDown = np.engine.getVoiceHz (0);
        setP (np, drNote, 1.0f);
        render (np, 4.0, cpu);
        const float hzFifth = np.engine.getVoiceHz (0);
        const auto ns = measure (render (np, 1.0, cpu));
        check ("The drones play where you set them", std::abs (hzDown / hz0 - 0.5f) < 0.005f && std::abs (hzFifth / hz0 - 1.4983f) < 0.005f && ns.finite && ns.peak <= 1.0f,
               juce::String::formatted ("lowest voice %.1f Hz, an octave down %.1f Hz, a fifth up %.1f Hz", hz0, hzDown, hzFifth));
    }

    // 8j. Swell travels to a point by itself.
    {
        GrainFieldProcessor fp2;
        stereoOnly (fp2);
        fp2.prepareToPlay (sr, block);
        fp2.setSwellFadeSeconds (1.0);
        const float from = fp2.apvts.getRawParameterValue (spec (pSwell).id)->load();
        fp2.swellFadeTo (0.9f);
        pump (500);
        const float mid = fp2.apvts.getRawParameterValue (spec (pSwell).id)->load();
        pump (800);
        const float to = fp2.apvts.getRawParameterValue (spec (pSwell).id)->load();
        check ("Swell fades over time", mid > from + 0.05f && mid < 0.85f && std::abs (to - 0.9f) < 0.01f && ! fp2.isSwellFading(),
               juce::String::formatted ("%.2f -> %.2f halfway -> %.2f", from, mid, to));
    }

    // 8k. A MIDI keyboard plays the harmony: chords are recognised, the register follows the hands.
    {
        GrainFieldProcessor mp;
        stereoOnly (mp);
        mp.prepareToPlay (sr, block);
        setP (mp, ids::hold, 1.0f); setP (mp, hmKey, 2.0f); setP (mp, hmDissolve, 0.3f);
        render (mp, 2.0, cpu);
        auto playKeys = [&] (std::initializer_list<int> notes)
        {
            juce::MidiBuffer on, off;
            for (int n : notes) { on.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0); off.addEvent (juce::MidiMessage::noteOff (1, n), 0); }
            juce::AudioBuffer<float> b (2, block);
            b.clear(); mp.processBlock (b, on);
            b.clear(); mp.processBlock (b, off);    // a short tap is enough
            render (mp, 1.5, cpu); pump (20);
            gf::ChordStep c;
            return mp.getKeysChord (c) ? gf::chordName (2, c) + " " + juce::String (c.oct) : juce::String ("none");
        };
        const auto a = playKeys ({ 52, 55, 59 });          // e g b
        const float eHz = mp.engine.getVoiceHz (0);
        const auto b2 = playKeys ({ 55, 59, 64 });         // the same chord, turned over
        const auto c2 = playKeys ({ 50, 55, 57 });         // d g a
        const auto d2 = playKeys ({ 45 });                 // one low a: the root moves, the kind stays
        const float aHz = mp.engine.getVoiceHz (0);
        const int before = mp.getProgression().count;
        mp.keepKeysChord();
        render (mp, 1.0, cpu); pump (20);
        const bool kept = mp.getProgression().count == before + 1 && mp.engine.getKeysChord() < 0
                          && mp.engine.getStepIndex() == before && std::abs (mp.engine.getVoiceHz (0) / aHz - 1.0f) < 0.002f;
        check ("A MIDI keyboard plays the harmony", a == "e minor 1" && b2 == "e minor 1" && c2 == "d sus 0" && d2 == "a sus 0"
               && std::abs (eHz - 164.8f) < 0.5f && std::abs (aHz - 110.0f) < 0.5f && kept,
               a + " | " + b2 + " | " + c2 + " | " + d2 + juce::String::formatted (" | lowest voice %.1f then %.1f Hz | kept as a slot: ", eHz, aHz) + (kept ? "yes" : "no"));
    }

    // 8l. Recording: what you hear becomes a file.
    {
        GrainFieldProcessor rp;
        stereoOnly (rp);
        rp.prepareToPlay (sr, block);
        setP (rp, ids::play, 0.0f);
        rp.startRecording();                                 // stopped: recording starts the piece
        const bool playing = rp.apvts.getRawParameterValue (ids::play)->load() > 0.5f;
        juce::AudioBuffer<float> heard (2, (int) (3.0 * sr));
        for (int k = 0; k < 6; ++k)
        {
            auto part = render (rp, 0.5, cpu);
            for (int c = 0; c < 2; ++c) heard.copyFrom (c, (int) (k * 0.5 * sr), part, c, 0, part.getNumSamples());
            juce::Thread::sleep (30);
        }
        rp.stopRecording();
        const auto file = rp.getLastTake();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        double seconds = 0.0; float rms = 0.0f, worstDiff = 0.0f; int bits = 0;
        if (reader != nullptr)
        {
            seconds = (double) reader->lengthInSamples / reader->sampleRate;
            bits = (int) reader->bitsPerSample;
            juce::AudioBuffer<float> got (2, (int) reader->lengthInSamples);
            reader->read (&got, 0, (int) reader->lengthInSamples, 0, true, true);
            rms = measure (got).rms;
            // The file is what was heard, sample for sample (to the last of its 24 bits).
            if (got.getNumSamples() == heard.getNumSamples())
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < got.getNumSamples(); ++i)
                        worstDiff = juce::jmax (worstDiff, std::abs (got.getSample (c, i) - heard.getSample (c, i)));
            else worstDiff = 1.0f;
        }
        reader.reset();
        check ("Records a take", playing && file.existsAsFile() && std::abs (seconds - 3.0) < 0.05 && bits == 24 && rms > 0.002f
               && std::abs (rp.getLastTakeSeconds() - seconds) < 0.01 && ! rp.isRecording() && worstDiff < 1.0e-6f,
               juce::String::formatted ("%.2f s, %d bit, rms %.4f, largest difference from what was heard %.8f, ", seconds, bits, rms, worstDiff) + file.getFileName());
        file.deleteFile();
    }

    // 8d. A field recording: noise with a hum on F#, found and made into a piece.
    {
        const double fsr = 44100.0;
        const int fn = (int) (40.0 * fsr);
        juce::AudioBuffer<float> rec (2, fn);
        juce::Random r (5);
        float pink = 0.0f;
        for (int i = 0; i < fn; ++i)
        {
            pink = 0.98f * pink + 0.02f * (r.nextFloat() * 2.0f - 1.0f);
            const double t = i / fsr;
            const double hum = 0.05 * std::sin (2.0 * juce::MathConstants<double>::pi * 92.5 * t)
                             + 0.03 * std::sin (2.0 * juce::MathConstants<double>::pi * 185.0 * t)
                             + 0.015 * std::sin (2.0 * juce::MathConstants<double>::pi * 277.5 * t);
            const float v = pink * 2.0f + (float) (hum * (0.7 + 0.3 * std::sin (t * 0.7)));
            rec.setSample (0, i, v + 0.01f * (r.nextFloat() - 0.5f));
            rec.setSample (1, i, v * 0.9f + 0.01f * (r.nextFloat() - 0.5f));
        }
        const auto wavFile = outDir.getChildFile ("test_field_recording.wav");
        writeWav (rec, fsr, wavFile);

        GrainFieldProcessor fp;
        stereoOnly (fp);
        fp.prepareToPlay (sr, block);
        fp.loadFileIntoSlot (slotTone, wavFile);
        const float core = fp.getSlotCoreNote (slotTone);
        check ("Finds a recording's core note", core > 0.0f && ((int) core) % 12 == 6, "midi " + juce::String (core, 1) + " (f# expected)");

        // The same recording as the bed, in the key of C: its F# hum is moved the smallest step, up to G.
        {
            GrainFieldProcessor bp;
            stereoOnly (bp);
            bp.prepareToPlay (sr, block);
            bp.loadFileIntoSlot (slotBed, wavFile);
            setP (bp, hmKey, 0.0f); setP (bp, drBed, 1.0f); setP (bp, drTape, 0.0f); setP (bp, drSpace, 0.0f); setP (bp, pSwell, 0.5f);
            bp.engine.setSolo (gf::Engine::stBed);
            auto bedOut = render (bp, 14.0, cpu);
            auto power = [&] (double hz)
            {
                const int from = (int) (6.0 * sr), n = bedOut.getNumSamples() - from;
                const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr, c = 2.0 * std::cos (w);
                double s1 = 0.0, s2 = 0.0;
                const float* d = bedOut.getReadPointer (0);
                for (int i = 0; i < n; ++i) { const double s0 = d[from + i] + c * s1 - s2; s2 = s1; s1 = s0; }
                return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / n;
            };
            const double atG = juce::jmax (power (97.8), power (98.0), power (98.2)), atFs = juce::jmax (power (92.3), power (92.5), power (92.7));
            check ("The bed follows the key", atG > atFs * 3.0, juce::String::formatted ("hum at g (98 Hz) %.5f, left at f# (92.5 Hz) %.5f", atG, atFs));

            // Say the recording is really an E: now it is moved up three steps, to G from E, and the hum lands on A.
            const int foundPc = bp.getSlotFoundNote (slotBed);
            bp.setSlotRoot (slotBed, 4);
            bedOut = render (bp, 14.0, cpu);
            const double atA = juce::jmax (power (109.8), power (110.0), power (110.2)), stillG = juce::jmax (power (97.8), power (98.0), power (98.2));
            juce::MemoryBlock state;
            bp.getStateInformation (state);
            GrainFieldProcessor bp2;
            bp2.setStateInformation (state.getData(), (int) state.getSize());
            const bool remembered = bp2.getSlotRootChoice (slotBed) == 4 && ((int) bp2.getSlotCoreNote (slotBed)) % 12 == 4;
            bp.setSlotRoot (slotBed, GrainFieldProcessor::rootNoNote);
            const bool none = bp.getSlotCoreNote (slotBed) < 0.0f;
            bp.setSlotRoot (slotBed, GrainFieldProcessor::rootAsFound);
            const bool back = ((int) bp.getSlotCoreNote (slotBed)) % 12 == 6 && bp.getSlotRootChoice (slotBed) == GrainFieldProcessor::rootAsFound;
            check ("A sound's own note can be set", foundPc == 6 && atA > stillG * 3.0 && remembered && none && back,
                   juce::String::formatted ("found f#; called e: hum at a (110 Hz) %.5f, at g %.5f; saved with the piece: %s", atA, stillG, remembered ? "yes" : "no"));

            // The bed a fourth above the root (on F and C): the F# hum moves down to F. At its own pitch it stays on F#.
            setP (bp, bedNote, 2.0f);
            bedOut = render (bp, 14.0, cpu);
            const double atF = juce::jmax (power (87.1), power (87.3), power (87.5)), notG = juce::jmax (power (97.8), power (98.0), power (98.2));
            setP (bp, bedNote, 5.0f);
            bedOut = render (bp, 14.0, cpu);
            const double ownFs = juce::jmax (power (92.3), power (92.5), power (92.7)), ownG = juce::jmax (power (97.8), power (98.0), power (98.2));
            check ("The bed plays where you set it", atF > notG * 3.0 && ownFs > ownG * 3.0,
                   juce::String::formatted ("a fourth up: hum at f %.5f, at g %.5f; own pitch: at f# %.5f, at g %.5f", atF, notG, ownFs, ownG));
        }

        fp.pieceFromSlot (slotTone);
        setP (fp, ids::blend, 12.0f);
        pump (20);
        const bool keyed = fp.engine.getKey() == 6 || juce::roundToInt (fp.apvts.getRawParameterValue (spec (hmKey).id)->load()) == 6;
        juce::AudioBuffer<float> piece (2, (int) (5 * 24.0 * sr));
        float quiet = 0.0f, loud = 0.0f;
        for (int k = 0; k < 5; ++k)
        {
            if (k > 0) { fp.moveOn(); pump (20); }
            auto part = render (fp, 24.0, cpu);
            for (int c = 0; c < 2; ++c) piece.copyFrom (c, (int) (k * 24.0 * sr), part, c, 0, part.getNumSamples());
            const auto st = measure (part, 0, 2, part.getNumSamples() / 2);
            if (k == 0) quiet = st.rms;
            if (k == 3) loud = st.rms;
        }
        const int fade = (int) (6.0 * sr);
        for (int c = 0; c < 2; ++c) piece.applyGainRamp (c, piece.getNumSamples() - fade, fade, 1.0f, 0.0f);
        writeWav (piece, sr, outDir.getChildFile ("Grain Field from a field recording.wav"));
        const auto ps = measure (piece);
        check ("A piece from a recording", keyed && ps.finite && ps.peak <= 1.0f && loud > quiet * 2.0f,
               juce::String::formatted ("in f#, opening rms %.3f, peak rms %.3f", quiet, loud));
    }

    // 9. A demo piece: the default piece moving on every 30 seconds, blending over 20.
    if (! quick)
    {
        GrainFieldProcessor demo;
        stereoOnly (demo);
        demo.prepareToPlay (sr, block);
        setP (demo, ids::blend, 20.0f);
        setP (demo, drGrow, 0.75f);
        const double partSeconds = 30.0;
        juce::AudioBuffer<float> all (2, (int) (5 * partSeconds * sr));
        for (int k = 0; k < 5; ++k)
        {
            if (k > 0) { demo.moveOn(); pump (20); }
            auto part = render (demo, partSeconds, cpu);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, (int) (k * partSeconds * sr), part, c, 0, part.getNumSamples());
        }
        const int fade = (int) (8.0 * sr);
        for (int c = 0; c < 2; ++c) all.applyGainRamp (c, all.getNumSamples() - fade, fade, 1.0f, 0.0f);
        writeWav (all, sr, outDir.getChildFile ("Grain Field 0.5.2 demo.wav"));
        const auto ds = measure (all);
        check ("Demo piece", ds.finite && ds.peak <= 1.0f, juce::String::formatted ("peak %.3f rms %.4f", ds.peak, ds.rms));
    }

    // 10. A listening guide: the same moment as texture alone, drones alone, the bed alone, then all together.
    if (! quick)
    {
        GrainFieldProcessor gp;
        stereoOnly (gp);
        gp.prepareToPlay (sr, block);
        setP (gp, ids::blend, 0.0f);
        setP (gp, drGrow, 0.8f);
        gp.moveOn(); pump (5); gp.moveOn(); pump (5);   // "rising"
        render (gp, 40.0, cpu);                          // let the drones grow
        const int stages[4] = { gf::Engine::stTexture, gf::Engine::stDrones, gf::Engine::stBed, -1 };
        const double lens[4] = { 16.0, 16.0, 10.0, 24.0 };
        juce::AudioBuffer<float> guide (2, (int) ((16.0 + 16.0 + 10.0 + 24.0 + 4 * 1.0) * sr));
        guide.clear();
        int at = 0;
        for (int k = 0; k < 4; ++k)
        {
            gp.engine.setSolo (stages[k]);
            auto part = render (gp, lens[k], cpu);
            const int fade = (int) (0.4 * sr);
            for (int c = 0; c < 2; ++c)
            {
                part.applyGainRamp (c, 0, fade, 0.0f, 1.0f);
                part.applyGainRamp (c, part.getNumSamples() - fade, fade, 1.0f, 0.0f);
                guide.copyFrom (c, at, part, c, 0, part.getNumSamples());
            }
            at += part.getNumSamples() + (int) (1.0 * sr);
        }
        writeWav (guide, sr, outDir.getChildFile ("Grain Field parts guide.wav"));
        check ("Listening guide", measure (guide).finite, "texture, drones, bed, all");
    }

    std::printf ("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
