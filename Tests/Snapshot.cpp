// Opens the editor off-screen while audio runs, and saves pictures of it as PNG files.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

namespace
{
struct AudioThread : juce::Thread
{
    explicit AudioThread (GrainFieldProcessor& p) : juce::Thread ("audio"), proc (p) {}
    void run() override
    {
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        while (! threadShouldExit())
        {
            proc.processBlock (buf, midi);
            wait (5); // about real time at 48 kHz
        }
    }
    GrainFieldProcessor& proc;
};

void shoot (juce::Component& editor, const juce::File& f, float scale)
{
    auto img = editor.createComponentSnapshot (editor.getLocalBounds(), true, scale);
    juce::PNGImageFormat png;
    f.deleteFile();
    juce::FileOutputStream os (f);
    png.writeImageToStream (img, os);
}

// A mouse event made by hand, to try the interface the way a hand would.
juce::MouseEvent mouse (juce::Component& c, juce::Point<float> at, juce::Point<float> downAt, int clicks, bool dragged, bool shift = false)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                             shift ? juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier)
                                   : juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downAt, now, clicks, dragged);
}
void press (juce::Component& c, juce::Point<float> at, int clicks = 1) { c.mouseDown (mouse (c, at, at, clicks, false)); }
void drag (juce::Component& c, juce::Point<float> from, juce::Point<float> to) { c.mouseDrag (mouse (c, to, from, 1, true)); }
void release (juce::Component& c, juce::Point<float> from, juce::Point<float> to) { c.mouseUp (mouse (c, to, from, 1, from.getDistanceFrom (to) > 3.0f)); }
void click (juce::Component& c, juce::Point<float> at) { press (c, at); release (c, at, at); }

template <typename T>
T* findChild (juce::Component& c, int skip = 0)
{
    std::function<T* (juce::Component&, int&)> walk = [&] (juce::Component& parent, int& left) -> T*
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (child)) { if (left == 0) return t; --left; }
            if (auto* t = walk (*child, left)) return t;
        }
        return nullptr;
    };
    return walk (c, skip);
}

void setP (GrainFieldProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    GrainFieldProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> emptyEditor (proc.createEditor());   // how it opens: empty and stopped
    emptyEditor->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (800);
    shoot (*emptyEditor, outDir.getChildFile ("ui_empty.png"), scale);
    emptyEditor.reset();

    proc.loadPreset (0);
    setP (proc, gf::ids::play, 1.0f);
    setP (proc, gf::ids::hold, 0.0f);
    setP (proc, gf::spec (gf::drGrow).id, 0.9f); // grow quickly so there's something to see
    setP (proc, gf::ids::blend, 4.0f);

    AudioThread audio (proc);
    audio.startThread();

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setVisible (true);
    auto run = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };

    proc.startRecording();
    run (9000);
    shoot (*editor, outDir.getChildFile ("ui_opening.png"), scale);

    // Notes in layers: the texture a fifth up, its second sound at its own pitch, the drones an octave down.
    setP (proc, gf::spec (gf::txNote).id, 1.0f);
    setP (proc, gf::spec (gf::txNote2).id, 5.0f);
    setP (proc, gf::spec (gf::drNote).id, 4.0f);
    run (2500);
    shoot (*editor, outDir.getChildFile ("ui_notes.png"), scale);
    setP (proc, gf::spec (gf::txNote).id, 0.0f);
    setP (proc, gf::spec (gf::txNote2).id, 0.0f);
    setP (proc, gf::spec (gf::drNote).id, 0.0f);

    proc.moveOn(); run (6000);   // root
    proc.moveOn(); run (9000);   // rising
    {
        auto pr = proc.getProgression();   // loop the first two chords; the third waits to be queued
        if (pr.count > 2) { pr.steps[2].loop = false; pr.steps[1].oct = 1; proc.setProgression (pr, proc.engine.getStepIndex()); proc.engine.cueStep (3, 0); }
        run (400);
    }
    shoot (*editor, outDir.getChildFile ("ui_rising.png"), scale);

    // A chord played on a MIDI keyboard (f# a c#, up high).
    proc.stopRecording();
    for (int n : { 66, 69, 73 }) proc.engine.midiNote (n, true);
    run (1800);
    shoot (*editor, outDir.getChildFile ("ui_keys.png"), scale);
    for (int n : { 66, 69, 73 }) proc.engine.midiNote (n, false);
    proc.releaseKeysChord();
    run (400);

    // A change of key, part way through its dissolve, with Swell travelling.
    proc.setSwellFadeSeconds (20.0);
    proc.swellFadeTo (0.85f);
    setP (proc, gf::spec (gf::hmDissolve).id, 6.0f);
    setP (proc, gf::spec (gf::hmKey).id, (float) ((proc.engine.getKey() + 7) % 12));
    run (3200);
    shoot (*editor, outDir.getChildFile ("ui_keychange.png"), scale);
    proc.cancelSwellFade();
    run (5000);

    // The chord editor, open on the second slot.
    {
        std::function<gf::ui::HarmonyView* (juce::Component&)> findHarmony = [&] (juce::Component& c) -> gf::ui::HarmonyView*
        {
            if (auto* h = dynamic_cast<gf::ui::HarmonyView*> (&c)) return h;
            for (auto* child : c.getChildren()) if (auto* h = findHarmony (*child)) return h;
            return nullptr;
        };
        if (auto* h = findHarmony (*editor))
        {
            // The pop-up closes itself when the app isn't in front, so place its contents by hand here.
            auto ed = gf::ui::makeChordEditor (*h, 1);
            auto empty = gf::ui::makeChordEditor (*h, -1);
            auto* canvas = h->getParentComponent();
            canvas->addAndMakeVisible (*ed);
            canvas->addAndMakeVisible (*empty);
            ed->setTopLeftPosition (200, 300);
            empty->setTopLeftPosition (620, 300);
            run (300);
            shoot (*editor, outDir.getChildFile ("ui_chord.png"), scale);
            canvas->removeChildComponent (ed.get());
            canvas->removeChildComponent (empty.get());
        }
    }


    proc.moveOn(); run (9000);   // wall
    shoot (*editor, outDir.getChildFile ("ui_wall.png"), scale);

    // The modules stretch: a wide window, then a tall one, then a small one.
    const int w0 = editor->getWidth(), h0 = editor->getHeight();
    editor->setSize (1760, h0); run (1200);
    shoot (*editor, outDir.getChildFile ("ui_wide.png"), scale);
    editor->setSize (w0, 1180); run (1200);
    shoot (*editor, outDir.getChildFile ("ui_tall.png"), scale);
    editor->setSize (868, 588); run (1200);
    shoot (*editor, outDir.getChildFile ("ui_small.png"), scale);
    editor->setSize (w0, h0); run (600);

    proc.engine.setSolo (gf::Engine::stDrones); run (1500);
    shoot (*editor, outDir.getChildFile ("ui_solo.png"), scale);
    proc.engine.setSolo (-1);

    // Try the controls the way a hand would.
    int failures = 0;
    auto check = [&] (const char* name, bool ok, const juce::String& detail)
    {
        std::printf ("%-40s %-4s %s\n", name, ok ? "OK" : "FAIL", detail.toRawUTF8());
        if (! ok) ++failures;
    };
    {
        // Double-click a knob: back to where it starts, and it stays there when the hand moves a little.
        auto* k = findChild<gf::ui::Knob> (*editor, 2);   // density
        auto* param = proc.apvts.getParameter (gf::spec (gf::txDensity).id);
        param->setValueNotifyingHost (0.9f);
        run (100);
        const juce::Point<float> at (30.0f, 16.0f);
        click (*k, at);
        press (*k, at, 2);
        static_cast<juce::Component*> (k)->mouseDoubleClick (mouse (*k, at, at, 2, false));
        drag (*k, at, at + juce::Point<float> (1.0f, -3.0f));
        drag (*k, at, at + juce::Point<float> (2.0f, -4.0f));
        release (*k, at, at + juce::Point<float> (2.0f, -4.0f));
        run (100);
        check ("Double-click resets a knob", std::abs (param->getValue() - param->getDefaultValue()) < 1.0e-4f,
               juce::String::formatted ("density at %.3f, default %.3f", param->getValue(), param->getDefaultValue()));
        // And an ordinary drag still works afterwards.
        press (*k, at);
        drag (*k, at, at + juce::Point<float> (0.0f, -36.0f));
        release (*k, at, at + juce::Point<float> (0.0f, -36.0f));
        check ("A drag still moves it", param->getValue() > param->getDefaultValue() + 0.1f, juce::String::formatted ("now %.3f", param->getValue()));
    }
    {
        // Click a scene: it blends in, nothing new.
        auto* sv = findChild<gf::ui::ScenesView> (*editor);
        const int parts = proc.getNumParts(), was = proc.getActivePart();
        click (*sv, sv->chipRect (0).getCentre());
        run (1500);
        check ("Click a scene to go to it", proc.getActivePart() == 0 && proc.getNumParts() == parts && proc.engine.getCurPart() == 0,
               juce::String::formatted ("from part %d to part %d, still %d parts", was + 1, proc.getActivePart() + 1, proc.getNumParts()));
        shoot (*editor, outDir.getChildFile ("ui_back.png"), scale);
    }
    {
        // Click a point on Swell: it travels there. Take the handle: it's yours again.
        auto* sw = findChild<gf::ui::SwellLine> (*editor);
        auto* param = proc.apvts.getParameter (gf::spec (gf::pSwell).id);
        proc.setSwellFadeSeconds (5.0);
        const float before = param->getValue();
        const float x70 = gf::ui::SwellLine::lineX + sw->lineW() * 0.7f;
        click (*sw, { x70, gf::ui::SwellLine::lineYpos });
        const bool started = proc.isSwellFading() && std::abs (proc.getSwellFadeTarget() - 0.7f) < 0.01f && std::abs (param->getValue() - before) < 0.02f;
        run (2500);
        const float mid = param->getValue();
        const float hx = gf::ui::SwellLine::lineX + sw->lineW() * mid;
        press (*sw, { hx, gf::ui::SwellLine::lineYpos });
        drag (*sw, { hx, gf::ui::SwellLine::lineYpos }, { hx - 60.0f, gf::ui::SwellLine::lineYpos });
        release (*sw, { hx, gf::ui::SwellLine::lineYpos }, { hx - 60.0f, gf::ui::SwellLine::lineYpos });
        check ("Swell: click travels, drag takes over", started && mid > before + 0.05f && mid < 0.69f && ! proc.isSwellFading() && param->getValue() < mid,
               juce::String::formatted ("%.2f, travelling: %.2f after 2.5 s, dragged to %.2f", before, mid, param->getValue()));
    }
    {
        // Drag a texture sound's window.
        auto* sv = findChild<gf::ui::SourcesView> (*editor);
        const auto r = sv->stripRect (0);
        auto* p1 = proc.apvts.getParameter (gf::spec (gf::txPos1).id);
        auto* p2 = proc.apvts.getParameter (gf::spec (gf::txPos2).id);
        const juce::Point<float> a (r.getX() + r.getWidth() * 0.7f, r.getCentreY()), b (r.getX() + r.getWidth() * 0.8f, r.getCentreY());
        press (*sv, a);
        drag (*sv, a, b);
        release (*sv, a, b);
        const float moved = p1->getValue(), other = p2->getValue();
        sv->mouseDown (mouse (*sv, b, b, 1, false, true));
        sv->mouseDrag (mouse (*sv, b - juce::Point<float> (r.getWidth() * 0.1f, 0.0f), b, 1, true, true));
        sv->mouseUp (mouse (*sv, b - juce::Point<float> (r.getWidth() * 0.1f, 0.0f), b, 1, true, true));
        check ("Drag where the grains come from", std::abs (moved - 0.8f) < 0.01f && std::abs (p1->getValue() - 0.7f) < 0.01f && std::abs (p2->getValue() - (other - 0.1f)) < 0.01f,
               juce::String::formatted ("sound 1 to %.2f; shift-drag moved all: %.2f and %.2f", moved, p1->getValue(), p2->getValue()));
        run (2500);
        shoot (*editor, outDir.getChildFile ("ui_window.png"), scale);
    }

    {
        // One of your own sounds, with its note beside its name: found by listening, then set by hand.
        const double fsr = 44100.0;
        juce::AudioBuffer<float> rec (2, (int) (8.0 * fsr));
        juce::Random r (3);
        for (int i = 0; i < rec.getNumSamples(); ++i)
        {
            const float v = (float) (0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 185.0 * i / fsr)) + 0.05f * (r.nextFloat() - 0.5f);
            rec.setSample (0, i, v); rec.setSample (1, i, v);
        }
        const auto f = outDir.getChildFile ("creek hum.wav");
        f.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os = f.createOutputStream();
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (fsr).withNumChannels (2).withBitsPerSample (16)))
                w->writeFromAudioSampleBuffer (rec, 0, rec.getNumSamples());
        }
        proc.loadFileIntoSlot (0, f);
        proc.loadFileIntoSlot (gf::slotBed, f);
        run (800);
        const int found = proc.getSlotFoundNote (0);
        proc.setSlotRoot (gf::slotBed, 9);
        run (800);
        check ("Your own sound shows its note", found == 6 && proc.getSlotRootChoice (gf::slotBed) == 9 && proc.getSlotRootChoice (0) == 9, "found f#, then called a");
        shoot (*editor, outDir.getChildFile ("ui_note.png"), scale);
        f.deleteFile();
    }

    audio.stopThread (1000);
    editor.reset();
    proc.getLastTake().deleteFile();
    std::printf ("%s\n", failures == 0 ? "snapshots written, interface checks passed" : "INTERFACE CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
