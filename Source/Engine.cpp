#include "Engine.h"

namespace gf
{
namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float twoPi = juce::MathConstants<float>::twoPi;

inline float hermite (const float* d, int n, double pos)
{
    const int i = (int) pos;
    const float f = (float) (pos - i);
    auto at = [d, n] (int k) { return d[k < 0 ? 0 : (k >= n ? n - 1 : k)]; };
    const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
    const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b = w + a;
    return (((a * f) - b) * f + c) * f + x0;
}

template <size_t N>
inline float lookup (const std::array<float, N>& t, float p)
{
    const float x = juce::jlimit (0.0f, 1.0f, p) * (float) (N - 1);
    const int i = juce::jmin ((int) N - 2, (int) x);
    const float f = x - (float) i;
    return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
}

inline float midiToHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }
inline float clamp01 (float x) { return juce::jlimit (0.0f, 1.0f, x); }

// A change of note: the new one swells up, slowly at first; the old one is let go, quickly at first and
// then with a long tail. The two curves are not mirror images, so the change sounds like one chord ending
// and the next one arriving rather than one sound morphing into another.
inline float swellIn (float x) { return std::sin (x * juce::MathConstants<float>::halfPi); }
inline float letGo (float x)   { const float r = 1.0f - x; return r * std::sqrt (r); }

inline float combTick (std::vector<float>& buf, int& w, float& lp, float delay, float in, float g, float a)
{
    const int n = (int) buf.size();
    float rp = (float) w - delay;
    if (rp < 0.0f) rp += (float) n;
    const int i0 = juce::jlimit (0, n - 1, (int) rp);
    const int i1 = i0 + 1 == n ? 0 : i0 + 1;
    const float fr = rp - (float) i0;
    const float d = buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    lp += a * (d - lp);
    const float y = in + g * lp;
    buf[(size_t) w] = y;
    if (++w == n) w = 0;
    return y;
}
} // namespace

Engine::Engine (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    for (int p = 0; p < numP; ++p)
    {
        raw[(size_t) p] = apvts.getRawParameterValue (spec (p).id);
        jassert (raw[(size_t) p] != nullptr);
        ranges[(size_t) p] = apvts.getParameterRange (spec (p).id);
        curVal[(size_t) p] = raw[(size_t) p]->load();
        curNorm[(size_t) p] = rawNorm (p);
        curNormPub[(size_t) p].store (curNorm[(size_t) p]);
        dispNormPub[(size_t) p].store (curNorm[(size_t) p]);
    }
    rawVolume = apvts.getRawParameterValue (ids::volume);
    rawPart = apvts.getRawParameterValue (ids::part);
    rawBlend = apvts.getRawParameterValue (ids::blend);
    rawPlay = apvts.getRawParameterValue (ids::play);
    rawSync = apvts.getRawParameterValue (ids::sync);
    rawSpaceMode = apvts.getRawParameterValue (ids::spaceMode);
    rawFreeze = apvts.getRawParameterValue (ids::freeze);
    rawHold = apvts.getRawParameterValue (ids::hold);
    rawTempo = apvts.getRawParameterValue (ids::tempo);
    rawArrive = apvts.getRawParameterValue (ids::arrive);
    rawWear = apvts.getRawParameterValue (ids::wear);
    arrive = rawArrive->load();
    updateArrive (1.0e3f);
    for (auto& b : partBlendA) b.store (-1.0f);
    for (auto& x : voiceXPub) x.store (1.0f);
    lastSceneParam = juce::roundToInt (rawPart->load());
    liveProg = prog = Progression::makeDefault();

    const size_t T = hannTable.size();
    for (size_t i = 0; i < T; ++i)
    {
        const float p = (float) i / (float) (T - 1);
        hannTable[i] = 0.5f - 0.5f * std::cos (twoPi * p);
        percTable[i] = (p < 0.02f ? p / 0.02f : std::exp (-(p - 0.02f) * 5.0f)) * juce::jmin (1.0f, (1.0f - p) * 30.0f);
        swellTable[i] = std::pow (p, 2.5f) * juce::jmin (1.0f, (1.0f - p) * 40.0f);
    }
    for (int i = 0; i < 4096; ++i)
    {
        sinTab[(size_t) i] = std::sin (twoPi * (float) i / 4096.0f);
        cosTab[(size_t) i] = std::cos (twoPi * (float) i / 4096.0f);
    }
    for (int s = 0; s < numSlots; ++s) { slotSrc[(size_t) s].store (nullptr); playingSrc[(size_t) s].store (nullptr); }
    for (int v = 0; v <= numVoices; ++v) { voiceAmpPub[(size_t) v].store (0.0f); voiceHzPub[(size_t) v].store (0.0f); voiceTargetPub[(size_t) v].store (0.0f); }
    for (int v = 0; v < numVoices; ++v) voices[(size_t) v].lfoRate = 0.03f + 0.012f * (float) v;
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (controlBlock, maxBlockSize);

    grains.assign (maxGrains, Grain());
    freeList.resize (maxGrains);
    for (int i = 0; i < maxGrains; ++i) freeList[(size_t) i] = maxGrains - 1 - i;
    activeList.clear();
    activeList.reserve (maxGrains);

    for (auto* b : { &txNear, &txFar, &rawNear, &rawFar, &bed }) { b->setSize (2, controlBlock); b->clear(); }
    for (auto* b : { &mainBus, &sendBus, &echoIn, &textureOut, &dronesOut, &txSend, &drSend, &revRoom, &revVast }) { b->setSize (2, maxBlock); b->clear(); }

    fftBuf.assign ((size_t) fftSize * 2, 0.0f);
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) fftSize);
    frame.assign ((size_t) fftSize, 0.0f);
    sedFrame.assign ((size_t) fftSize, 0.0f);
    olaL.assign ((size_t) fftSize, 0.0f);
    olaR.assign ((size_t) fftSize, 0.0f);
    vOlaL.assign ((size_t) fftSize, 0.0f);
    vOlaR.assign ((size_t) fftSize, 0.0f);
    envBuf.assign ((size_t) fftSize / 2 + 1, 0.0f);
    fineBuf.assign ((size_t) fftSize / 2 + 1, 0.0f);
    prefix.assign ((size_t) fftSize / 2 + 2, 0.0f);
    for (auto& v : vMag) v.assign ((size_t) fftSize / 2 + 1, 0.0f);
    wearHead = wearCount = 0;
    olaPos = 0; hopCount = 0;
    sediment.assign ((size_t) (sr * sedimentSeconds), 0.0f);
    sedWrite = 0; sedPower = 0.0f;
    const int combLen = juce::nextPowerOfTwo ((int) (sr / 20.0)) + 4;   // room for drones moved an octave down
    for (auto& v : voices)
    {
        for (auto* set : { &v.comb, &v.old })
            for (auto& c : *set) { c.buf.assign ((size_t) combLen, 0.0f); c.w = 0; c.lp = 0.0f; c.delay = (float) (sr / 110.0); }
        v.amp = v.targetAmp = 0.0f;
        v.xfade = 1.0f; v.gNew = 1.0f; v.gOld = 0.0f;
    }
    for (auto& f : droneTone) f.reset();
    for (auto& f : bedTone) f.reset();
    for (auto& set : tuned) for (auto& f : set) f.reset();
    for (auto& set : tunedOld) for (auto& f : set) f.reset();
    tunedX = 1.0f; tunedPending = false;
    for (auto& f : farTone) f.reset();
    for (auto& f : farToneRaw) f.reset();
    txIdx = drIdx = -1;
    stepIndex = -1; lastKey = -1; lineBar = -1;
    voices[lineVoice].midi = 66.0f;
    mass = 0.0f; subAmp = 0.0f;

    echoBuf.setSize (2, (int) (sr * 3.5) + 8);
    echoBuf.clear();
    echoWrite = 0; echoDelaySm = 0.5 * sr; echoLp = {}; echoHpLow = {};

    for (auto* s : { &roomGain, &vastGain }) s->reset (sr, 0.3);
    for (auto* s : { &masterGain, &textureGain, &dronesGain }) s->reset (sr, 0.08);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    roomVerb.prepare (spec); vastVerb.prepare (spec);
    makeImpulseResponses();
    glue.prepare (spec); glue.setThreshold (-9.0f); glue.setRatio (2.0f); glue.setAttack (15.0f); glue.setRelease (400.0f);
    limiter.prepare (spec); limiter.setThreshold (-0.8f); limiter.setRelease (200.0f);

    for (auto& t : tapes)
    {
        for (auto& pair : t.buf) for (auto& b : pair) b.assign ((size_t) juce::nextPowerOfTwo ((int) (sr * 0.05)), 0.0f);
        t.write = 0; t.lp = {}; t.dcX = {}; t.dcY = {};
        t.dropEnv = t.dropTarget = 0.0f; t.dropHold = 0;
    }
    tapes[1].wowRate = 0.43f; tapes[1].flutterPh = 0.37f;   // the two tapes never move together
    driveDcX = {}; driveDcY = {};

    captureRing.setSize (2, (int) (sr * captureSeconds));
    captureRing.clear();
    captureOut.setSize (2, captureRing.getNumSamples());
    captureWrite = 0;
}

void Engine::makeImpulseResponses()
{
    auto make = [this] (double seconds, double decay, double dark, juce::int64 seed) {
        juce::Random r (seed);
        const int len = (int) (sr * seconds), pre = (int) (sr * 0.02);
        juce::AudioBuffer<float> ir (2, len);
        ir.clear();
        double power = 0.0;
        for (int c = 0; c < 2; ++c)
        {
            float* d = ir.getWritePointer (c);
            double y = 0.0;
            for (int i = pre; i < len; ++i)
            {
                const double t = (i - pre) / sr;
                const double a = 0.55 * std::exp (-t / dark) + 0.05;
                y += a * ((r.nextDouble() * 2.0 - 1.0) - y);
                d[i] = (float) (y * std::exp (-t / decay) * juce::jmin (1.0, t / 0.04));
                power += (double) d[i] * d[i];
            }
        }
        const double rms = std::sqrt (power / (2.0 * len));
        if (rms > 0.0) ir.applyGain ((float) (0.00125 / rms * (44100.0 / sr)));
        return ir;
    };
    using C = juce::dsp::Convolution;
    roomVerb.loadImpulseResponse (make (2.6, 0.6, 0.6, 7), sr, C::Stereo::yes, C::Trim::no, C::Normalise::no);
    vastVerb.loadImpulseResponse (make (12.0, 3.6, 2.8, 9), sr, C::Stereo::yes, C::Trim::no, C::Normalise::no);
}

/* ---------------- Message thread ---------------- */

void Engine::setScene (int index, const SceneData& scene)
{
    const juce::SpinLock::ScopedLockType sl (sceneLock);
    scenes[(size_t) index] = scene;
}

void Engine::setProgression (const Progression& p, int playingNowAt)
{
    const juce::SpinLock::ScopedLockType sl (progLock);
    liveProg = p;
    liveRemap = playingNowAt;
}

bool Engine::fetchCapture (juce::AudioBuffer<float>& dest, double& sampleRateOut)
{
    if (! captureReady.load()) return false;
    dest.makeCopyOf (captureOut);
    sampleRateOut = sr;
    captureReady.store (false);
    return true;
}

int Engine::popEvents (VisEvent* dest, int maxEvents)
{
    int s1, n1, s2, n2;
    eventFifo.prepareToRead (maxEvents, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) dest[i] = events[(size_t) (s1 + i)];
    for (int i = 0; i < n2; ++i) dest[n1 + i] = events[(size_t) (s2 + i)];
    eventFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

/* ---------------- Controls and parts ---------------- */

float Engine::rawNorm (int p) const
{
    return ranges[(size_t) p].convertTo0to1 (ranges[(size_t) p].snapToLegalValue (raw[(size_t) p]->load (std::memory_order_relaxed)));
}

float Engine::gauss()
{
    float u = 0.0f;
    while (u <= 1.0e-7f) u = rng.nextFloat();
    return std::sqrt (-2.0f * std::log (u)) * std::cos (twoPi * rng.nextFloat());
}

void Engine::startRecall (int index, float blendSeconds)
{
    if (index < 0 || index >= numScenes) return;
    if (! sceneLock.tryEnter()) { pendingRecall = index; return; }
    const SceneData sc = scenes[(size_t) index];
    sceneLock.exit();
    if (! sc.used) return;

    morphSeconds = blendSeconds >= 0.0f ? blendSeconds : juce::jmax (0.0f, rawBlend->load());
    keysOn = false;   // a part brings its own harmony
    curPart = index;
    partElapsed = 0.0;
    for (int p = 0; p < numP; ++p)
    {
        const auto i = (size_t) p;
        morphFrom[i] = curNorm[i];
        overrideOn[i] = true;
        overrideVal[i] = sc.norm[i];
        rawAtRecall[i] = rawNorm (p);
    }
    for (int s = 0; s < numSlots; ++s)
        if (sc.src[(size_t) s] != nullptr)
        {
            slotOverride[(size_t) s] = sc.src[(size_t) s];
            slotAtRecall[(size_t) s] = slotSrc[(size_t) s].load();
        }
    progScene = sc.prog;
    progAtRecall = prog;
    progOverride = sc.prog.count > 0;
    morphing = morphSeconds > 0.05;
    morphPos = morphing ? 0.0 : 1.0;
    lastRecalled.store (index);
    recallCount.fetch_add (1);
}

void Engine::updateControl (int numSamples)
{
    if (morphing)
    {
        morphPos += numSamples / (sr * morphSeconds);
        if (morphPos >= 1.0) { morphPos = 1.0; morphing = false; }
    }
    const float k = (float) morphPos, s = k * k * (3.0f - 2.0f * k);
    for (int p = 0; p < numP; ++p)
    {
        const auto i = (size_t) p;
        float base = rawNorm (p);
        if (overrideOn[i])
        {
            if (std::abs (base - rawAtRecall[i]) > 1.0e-4f) overrideOn[i] = false; // the knob moved: it takes over
            else base = overrideVal[i];
        }
        float v = base;
        if (morphing && ! spec (p).discrete) v = morphFrom[i] + (base - morphFrom[i]) * s;
        curNorm[i] = v;
        curNormPub[i].store (v, std::memory_order_relaxed);
        curVal[i] = ranges[i].convertFrom0to1 (v);
    }
}

void Engine::updateSwell()
{
    swell = cur (pSwell);
    densityEff = juce::jlimit (0.05f, 24.0f, cur (txDensity) * (0.3f + 1.7f * swell));
    bodyEff = clamp01 (cur (drBody) * (0.5f + 0.5f * swell) + 0.35f * swell);
    subEff = clamp01 (cur (drSub) * (0.25f + 0.75f * swell) + 0.15f * swell * swell);
    driveEff = clamp01 (cur (outDrive) + 0.55f * swell * swell);
    txSpaceEff = clamp01 (cur (txSpace) + 0.3f * swell * (1.0f - cur (txSpace)));
    drSpaceEff = clamp01 (cur (drSpace) + 0.3f * swell * (1.0f - cur (drSpace)));
    spreadEff = clamp01 (cur (txSpread) + 0.2f * swell);
    frozen = rawFreeze->load() > 0.5f;
}

void Engine::updateArrive (float dt)
{
    // Follow the control with a short lag, so a jump in it is still a fade.
    arrive += (rawArrive->load() - arrive) * (1.0f - std::exp (-dt / 0.4f));
    auto stage = [a = arrive] (float from, float to)
    {
        const float x = clamp01 ((a - from) / (to - from));
        return x * x * (3.0f - 2.0f * x);
    };
    arrOpen    = stage (0.00f, 0.04f);                    // at nothing it is silent: no tape hiss either
    arrBed     = stage (0.00f, 0.30f);
    arrTexture = stage (0.10f, 0.45f);
    arrGrains  = 0.08f + 0.92f * stage (0.10f, 0.65f);   // the texture starts as a few grains and fills in
    arrDrones  = stage (0.30f, 0.75f);
    arrSub     = stage (0.55f, 0.85f);
    arrRoom    = 0.35f + 0.65f * stage (0.45f, 1.00f);   // close at first; the room opens last
}

/* ---------------- Harmony ---------------- */

float Engine::snapToChord (float midi, int pcOff) const
{
    if (numChordPcs == 0) return midi;
    float best = midi, bd = 1.0e9f;
    for (int i = 0; i < numChordPcs; ++i)
    {
        const int pc = (chordPcs[(size_t) i] + pcOff) % 12;
        const float base = (float) pc + 12.0f * std::floor ((midi - (float) pc) / 12.0f);
        for (float c : { base, base + 12.0f })
            if (std::abs (c - midi) < bd) { bd = std::abs (c - midi); best = c; }
    }
    return best;
}

float Engine::snapInterval (float semis, int pcOff) const
{
    // An interval above the chord's root, moved onto the chord.
    return snapToChord ((float) (60 + rootPc) + semis, pcOff) - (float) (60 + rootPc);
}

NoteStep Engine::slotStep (int k) const
{
    // The texture's own step, then the sound's on top of it.
    const auto own = noteStep (curInt (txNote1 + k));
    NoteStep st;
    st.pc = (txStep.pc + own.pc) % 12;
    st.oct = juce::jlimit (-2, 2, txStep.oct + own.oct);
    st.own = own.own;
    return st;
}

float Engine::voiceHz (float midi) const
{
    float m = midi + (float) drShift;
    while (m < 22.0f) m += 12.0f;   // never below what the resonators can hold
    return midiToHz (m);
}

void Engine::setChord (int newRoot, int type, int oct, bool jump)
{
    const int prevShift = 12 * chordOct;
    rootPc = newRoot;
    chordType = type;
    chordOct = juce::jlimit (minChordOct, maxChordOct, oct);
    const int shift = 12 * chordOct;      // the register you gave this chord
    const auto& v = chordVoicing (type);
    int base = 48 + rootPc;               // by itself the chord's root sits between E2 and D#3
    if (base > 51) base -= 12;

    // The notes of the chord, for tuning grains
    numChordPcs = 0;
    auto addPc = [this] (int pc) {
        pc = ((pc % 12) + 12) % 12;
        for (int i = 0; i < numChordPcs; ++i) if (chordPcs[(size_t) i] == pc) return;
        if (numChordPcs < (int) chordPcs.size()) chordPcs[(size_t) numChordPcs++] = pc;
    };
    for (int iv : v.drone) addPc (rootPc + iv);
    for (int iv : v.pad) addPc (rootPc + iv);

    for (int i = 0; i < 3; ++i) voices[(size_t) i].midi = (float) (base + v.drone[i] + shift);
    for (int i = 0; i < 5; ++i)
    {
        // Each upper voice moves to the nearest octave of its new note, so the chord changes without lurching.
        auto& V = voices[(size_t) (3 + i)];
        float t = (float) (base + v.pad[i]);
        while (t > 81.0f) t -= 12.0f;
        if (! jump)
        {
            const float was = V.midi - (float) prevShift;
            float best = t, bd = 1.0e9f;
            for (float c : { t - 12.0f, t, t + 12.0f })
                if (c >= 48.0f && c <= 81.0f && std::abs (c - was) < bd) { bd = std::abs (c - was); best = c; }
            t = best;
        }
        V.midi = t + (float) shift;
    }
    if (jump) voices[lineVoice].midi = (float) (base + 24 + v.pad[2] % 12 + shift);
    moveLine (true);

    subMidi = (float) (base - 12 + shift);
    retargetVoices (jump);

    // The resonators the texture rings through follow the same way.
    if (jump) retuneBank();
    else crossfadeBank();
}

void Engine::retargetVoices (bool jump)
{
    // Every voice that has a new note lets the old one go and swells up on the new one. They start only a
    // moment apart, so the old chord is released as one gesture; notes the two chords share keep sounding.
    const float spreadOut = 0.08f * cur (hmDissolve);
    for (int i = 0; i < numVoices; ++i)
    {
        auto& V = voices[(size_t) i];
        const float newHz = voiceHz (V.midi);
        if (std::abs (newHz / V.targetHz - 1.0f) > 0.003f && V.xfade >= 1.0f) V.wait = rng.nextFloat() * spreadOut;
        V.targetHz = newHz;
        if (jump) { V.hz = V.oldHz = V.targetHz; V.xfade = 1.0f; V.wait = 0.0f; }
    }
    float sm = subMidi + (float) drShift;
    while (sm < 24.0f) sm += 12.0f;   // a sub below about 33 Hz is felt more than heard: keep it where it sounds
    subTargetHz = midiToHz (sm);
    if (jump) { subHz = subOldHz = subTargetHz; subX = 1.0f; }
}

void Engine::crossfadeBank()
{
    if (tunedX >= 1.0f) { tunedOld = tuned; tunedX = 0.0f; for (auto& set : tuned) for (auto& f : set) f.reset(); retuneBank(); }
    else tunedPending = true;   // a change is still under way: this one follows it
}

void Engine::retuneBank()
{
    for (int i = 0; i < 6; ++i)
    {
        // One resonator per note of the chord; spare ones sit an octave up on the chord's root and fifth.
        // All of them move to where the texture plays.
        const int pc = (i < numChordPcs ? chordPcs[(size_t) i] : (rootPc + (i % 2 == 0 ? 0 : 7)) % 12) + txStep.pc;
        const float hz = midiToHz ((float) (60 + pc % 12 + (i < numChordPcs ? 0 : 12) + 12 * juce::jlimit (-1, 1, txStep.oct)));
        for (auto& set : tuned) set[(size_t) i].set (hz, 22.0f, sr);
    }
}

void Engine::moveLine (bool chordChanged)
{
    // The line walks through the chord: usually to a near note, sometimes a leap.
    auto& V = voices[lineVoice];
    const float m = V.midi;
    const float lo = 60.0f + 12.0f * (float) chordOct, hi = lo + 19.0f;
    float cands[24];
    int n = 0;
    for (int o = 2; o <= 9 && n < 24; ++o)
        for (int i = 0; i < numChordPcs && n < 24; ++i)
        {
            const float c = (float) (12 * (o + 1) + chordPcs[(size_t) i]);
            if (c >= lo && c <= hi) cands[n++] = c;
        }
    if (n == 0) return;
    std::sort (cands, cands + n, [m] (float a, float b) { return std::abs (a - m) < std::abs (b - m); });
    int pick = 0;
    if (! chordChanged || std::abs (cands[0] - m) < 0.5f) pick = juce::jmin (n - 1, 1 + (rng.nextFloat() < 0.3f ? 1 : 0));
    if (! chordChanged && std::abs (cands[pick] - m) > 5.0f) return;
    V.midi = cands[pick];
    V.targetHz = voiceHz (V.midi);
}

void Engine::setCombDelays (Voice& V, std::array<Comb, 2>& combs, float hz, float cents, float aV)
{
    juce::ignoreUnused (V);
    const float om = twoPi * hz / (float) sr;
    const float lpDelay = std::atan2 ((1.0f - aV) * std::sin (om), 1.0f - (1.0f - aV) * std::cos (om)) / om;
    const float dl = (float) (sr / (hz * std::pow (2.0f, -cents / 1200.0f))) - lpDelay;
    const float dr = (float) (sr / (hz * std::pow (2.0f, cents / 1200.0f))) - lpDelay;
    for (int c = 0; c < 2; ++c)
        combs[(size_t) c].delay = juce::jlimit (2.0f, (float) combs[(size_t) c].buf.size() - 3.0f, c == 0 ? dl : dr);
}

int Engine::noteIndexAt (const SourceData* s, int x) const
{
    // The note under a point in a sound, or the nearest one.
    int best = 0, bestDist = std::numeric_limits<int>::max();
    const int count = (int) s->notes.size();
    for (int i = 0; i < count; ++i)
    {
        const auto& sp = s->notes[(size_t) i];
        const int d = x < sp.start ? sp.start - x : (x >= sp.end ? x - sp.end + 1 : 0);
        if (d < bestDist) { bestDist = d; best = i; if (d == 0) break; }
    }
    return best;
}

float Engine::fitInNote (const SourceData* s, int noteIndex, double x, float semis, float& len) const
{
    // Grains stay inside one note, so none of them runs into the next one out of tune.
    const auto& span = s->notes[(size_t) noteIndex];
    const double inc = std::pow (2.0, semis / 12.0) * s->sampleRate / sr;
    const double spanLen = juce::jmax (64, span.end - span.start - 64);
    double grainSrc = (double) len * sr * inc;
    if (grainSrc > spanLen * 0.85) { grainSrc = spanLen * 0.85; len = (float) (grainSrc / (sr * inc)); }
    const double lo = span.start + 32.0, hi = lo + (spanLen - grainSrc);
    return (float) (juce::jlimit (lo, hi, x - grainSrc * 0.5) / (double) juce::jmax (1, s->audio.getNumSamples() - 1));
}

/* ---------------- A MIDI keyboard plays the harmony ---------------- */

void Engine::midiNote (int note, bool on)
{
    if (note < 0 || note > 127) return;
    keysHeld[(size_t) note] = on;
    if (! on) return;
    keysRecent[(size_t) note] = true;
    keysSettle = (int) (0.07 * sr);   // fingers never land together: wait a moment, then read them as one chord
}

void Engine::applyKeys()
{
    int lowest = -1;
    unsigned pcs = 0;
    int count = 0;
    for (int n = 0; n < 128; ++n)
        if (keysHeld[(size_t) n] || keysRecent[(size_t) n])
        {
            if (lowest < 0) lowest = n;
            if ((pcs & (1u << (n % 12))) == 0) ++count;
            pcs |= 1u << (n % 12);
        }
    keysRecent.fill (false);
    if (lowest < 0) return;

    int root = lowest % 12, type = chordType;
    if (count > 1)
    {
        // Which chord is this? Try each note as the root, and each kind of chord against what is held.
        // The plain shapes are known by name; anything else goes to the kind it shares most notes with.
        static const unsigned full[6] = { 0x081, 0x095, 0x48D, 0x895, 0x4A5, 0x0AD };   // fifth, open, minor, major, sus, cluster
        static const int order[6] = { 3, 2, 4, 1, 5, 0 };                               // ties go to major, minor, sus, open, cluster, fifth
        auto bits = [] (unsigned v) { int c = 0; for (; v != 0; v &= v - 1) ++c; return c; };
        float best = -100.0f;
        for (int r = 0; r < 12; ++r)
        {
            if ((pcs & (1u << r)) == 0) continue;
            const unsigned rel = ((pcs >> r) | (pcs << (12 - r))) & 0xFFFu;
            int exact = -1;
            switch (rel)
            {
                case 0x081: exact = 0; break;                                       // root and fifth
                case 0x091: case 0x011: exact = 3; break;                           // major
                case 0x089: case 0x009: exact = 2; break;                           // minor
                case 0x0A1: case 0x085: case 0x021: case 0x0A5: exact = 4; break;   // sus
                case 0x095: case 0x005: exact = 1; break;                           // open
                case 0x007: case 0x00D: case 0x00B: case 0x003: exact = 5; break;   // notes side by side
                default: break;
            }
            const float rootBonus = r == lowest % 12 ? 0.5f : 0.0f;
            if (exact >= 0)
            {
                const float sc = 20.0f + rootBonus;
                if (sc > best) { best = sc; root = r; type = exact; }
                continue;
            }
            for (int t : order)
            {
                const float sc = (float) bits (rel & full[t]) - 2.0f * (float) bits (rel & ~full[t]) + rootBonus;
                if (sc > best) { best = sc; root = r; type = t; }
            }
        }
    }
    // The register follows where on the keyboard you play: the chord's lowest note is as near your lowest as it can be.
    const int base = 48 + root - (root > 3 ? 12 : 0);
    keysOct = juce::jlimit (minChordOct, maxChordOct, (int) std::floor ((float) (lowest - base) / 12.0f + 0.5f));
    keysRoot = root;
    keysType = type;
    keysOn = true;
}

void Engine::updateHarmony (float dt, bool hostPlaying, bool havePpq, double ppq, double barLength)
{
    // The progression in use: the part's own until you change the live one.
    Progression live;
    bool gotLive = false;
    int remap = -1;
    if (progLock.tryEnter()) { live = liveProg; remap = liveRemap; liveRemap = -1; progLock.exit(); gotLive = true; }
    if (gotLive)
    {
        if (progOverride && ! (live == progAtRecall)) progOverride = false;
        prog = progOverride ? progScene : live;
        // Slots were moved: keep playing the same chord in its new place.
        if (remap >= 0 && remap < prog.count && ! progOverride) stepIndex = remap;
    }

    key = juce::jlimit (0, 11, curInt (hmKey));
    keyPub.store (key, std::memory_order_relaxed);

    // Where each layer plays. A change to the texture's note moves its resonators as a crossfade; a change to the
    // drones' note moves every voice the way a chord change does. Each sound's own step is read as its grains are made.
    bedStep = noteStep (curInt (bedNote));
    const int ti = curInt (txNote), di = curInt (drNote);
    if (ti != txIdx)
    {
        txStep = noteStep (ti);
        if (txIdx >= 0 && stepIndex >= 0) crossfadeBank();
        txIdx = ti;
    }
    if (di != drIdx)
    {
        drStep = noteStep (di);
        drShift = drStep.pc + 12 * drStep.oct;
        if (drIdx >= 0 && stepIndex >= 0) retargetVoices (false);
        drIdx = di;
    }

    if (hostPlaying && havePpq) barPos = ppq / barLength;
    else
    {
        if (running && ! frozen) internalBars += dt * bpm / 60.0 / barLength;
        barPos = internalBars;
    }
    // Where we are in the progression. It moves on by itself unless Hold is on, and a chord you
    // pick is taken at the next bar line.
    const double bp = juce::jmax (0.0, barPos);
    const bool hold = rawHold->load() > 0.5f;
    int idx = stepIndex;
    const bool jumped = idx < 0 || idx >= prog.count || bp < lastBarPos - 0.01 || bp > lastBarPos + 1.0;
    if (jumped)
    {
        if (hold && idx >= 0) { idx = juce::jlimit (0, prog.count - 1, idx); stepStart = std::floor (bp); }
        else
        {
            // Follow the song's position, as if the chords in the loop had been cycling from bar one.
            int order[Progression::maxSteps], nIn = 0, loopBars = 0;
            for (int i = 0; i < prog.count; ++i) if (prog.steps[(size_t) i].loop) { order[nIn++] = i; loopBars += prog.steps[(size_t) i].bars; }
            if (nIn == 0) { for (int i = 0; i < prog.count; ++i) order[nIn++] = i; loopBars = prog.totalBars(); }
            double pos = std::fmod (bp, (double) juce::jmax (1, loopBars));
            int k = 0;
            while (k < nIn - 1 && pos >= prog.steps[(size_t) order[k]].bars) { pos -= prog.steps[(size_t) order[k]].bars; ++k; }
            idx = order[k];
            stepStart = bp - pos;
        }
    }
    int want = cue.load();
    if (want >= prog.count) { cue.store (-1); want = -1; }
    if (want >= 0)
    {
        // A queued chord comes in when this one's bars are done (or, if we've stayed past them, at
        // the next bar line), sooner if asked for: at the next bar, or right away.
        const int when = cueWhen.load();
        const bool barLine = std::floor (bp) > std::floor (lastBarPos);
        const bool done = bp - stepStart >= (double) prog.steps[(size_t) idx].bars - 1.0e-6;
        const bool take = want == idx || ! running || when >= 2 || (when == 1 && barLine) || (when == 0 && done && barLine);
        if (take && cue.compare_exchange_strong (want, -1))
        {
            idx = want;
            stepStart = when >= 2 ? bp : std::floor (bp);
        }
    }
    else if (! hold)
    {
        int guard = 0;
        while (bp - stepStart >= (double) prog.steps[(size_t) idx].bars && guard++ < 64)
        {
            stepStart += prog.steps[(size_t) idx].bars;
            // On to the next chord that's in the loop (this one again, if it's the only one).
            for (int j = 1; j <= prog.count; ++j)
            {
                const int cand = (idx + j) % prog.count;
                if (prog.steps[(size_t) cand].loop) { idx = cand; break; }
            }
        }
    }
    lastBarPos = bp;
    double pos = bp - stepStart;
    const auto& st = prog.steps[(size_t) idx];
    // A chord played on the keys holds until the progression moves on, a slot is picked, or it is let go.
    if (keysClear.exchange (false)) keysOn = false;
    if (keysOn && stepIndex >= 0 && idx != stepIndex) keysOn = false;
    const int newRoot = keysOn ? keysRoot : (key + st.root) % 12;
    const int newType = keysOn ? keysType : st.type, newOct = keysOn ? keysOct : st.oct;
    if (idx != stepIndex || key != lastKey || newRoot != rootPc || newType != chordType || newOct != chordOct)
    {
        const bool first = stepIndex < 0;
        stepIndex = idx;
        lastKey = key;
        setChord (newRoot, newType, newOct, first);
    }
    keysPub.store (keysOn ? (keysRoot | (keysType << 4) | ((keysOct + 2) << 8)) : -1, std::memory_order_relaxed);
    // Within a long chord the line still moves every two bars.
    const int lb = (int) std::floor (barPos / 2.0);
    if (lb != lineBar)
    {
        if (lineBar >= 0 && rng.nextFloat() < 0.55f) moveLine (false);
        lineBar = lb;
    }
    stepPub.store (idx, std::memory_order_relaxed);
    stepProgPub.store ((float) std::fmod (pos / juce::jmax (1, st.bars), 1.0000001), std::memory_order_relaxed);
}

/* ---------------- Texture ---------------- */

void Engine::spawnGrain (int delay, const SourceData* s, float posNorm, float durSec, float semis, int env, float amp,
                         bool rev, float pan, float nearFar, int dest, int visType, double incMul, int crush, float bits)
{
    if (freeList.empty()) return;
    const bool dust = env == envDust;
    if (! dust && (s == nullptr || s->audio.getNumSamples() < 64)) return;

    const int gi = freeList.back();
    freeList.pop_back();
    auto& g = grains[(size_t) gi];
    g = Grain();
    g.src = dust ? nullptr : s;
    g.env = env; g.dest = dest; g.amp = amp; g.delay = delay;
    g.len = juce::jmax (16, (int) (durSec * sr));
    g.incMul = incMul; g.crush = crush; g.bits = bits;
    g.seed = (juce::uint32) rng.nextInt() | 1u;
    g.raw = grainOwn && dest == toTexture;

    if (! dust)
    {
        const int n = s->audio.getNumSamples();
        // Sounds that know their notes are moved onto the chord.
        if (! s->notes.empty() && dest == toTexture && ! pretuned && ! grainOwn)
        {
            const float m = s->noteFor (posNorm * (n - 1));
            if (m > 0.0f && rng.nextFloat() < 0.92f)
                semis = juce::jlimit (-36.0f, 24.0f, snapToChord (m + semis, grainPc) - m);
        }
        const double inc = std::pow (2.0, semis / 12.0) * s->sampleRate / sr;
        double span = g.len * inc;
        if (incMul < 1.0) span = inc * (1.0 - std::pow (incMul, (double) g.len)) / (1.0 - incMul);
        if (span > n - 4) { g.len = juce::jmax (16, (int) ((n - 4) / inc)); span = g.len * inc; }
        double off = posNorm * (n - 1);
        if (off + span > n - 3) off = juce::jmax (0.0, n - 3 - span);
        g.inc = rev ? -inc : inc;
        g.pos = rev ? off + span : off;
        queueWear (s, off, off + span);
    }

    pan = juce::jlimit (-1.0f, 1.0f, pan);
    float pl, pr;
    if (! dust && s->audio.getNumChannels() > 1) { pl = pan > 0 ? 1.0f - pan : 1.0f; pr = pan < 0 ? 1.0f + pan : 1.0f; }
    else { const float a = (pan + 1.0f) * pi * 0.25f; pl = std::cos (a) * 1.41f; pr = std::sin (a) * 1.41f; }
    const float nf = clamp01 (nearFar);
    const float ng = std::cos (nf * pi * 0.5f), fg = std::sin (nf * pi * 0.5f);
    g.nl = pl * ng; g.nr = pr * ng; g.fl = pl * fg; g.fr = pr * fg;
    activeList.push_back (gi);

    if (dest == toTexture)
    {
        ++eventsThisWindow;
        int s1, n1, s2, n2;
        eventFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0)
        {
            const float x = dust ? rng.nextFloat() : (float) (g.inc > 0 ? g.pos : g.pos + g.inc * g.len) / (float) s->audio.getNumSamples();
            const float covers = dust ? 0.0f : (float) (std::abs (g.inc) * g.len) / (float) s->audio.getNumSamples();
            events[(size_t) s1] = { visType, clamp01 (x), semis, durSec, 1.0f - nf, pan, visMidi, rev, visSlot, covers };
        }
        eventFifo.finishedWrite (n1);
    }
}

void Engine::spawnCloud (int delay)
{
    // Each texture source gives as much of the cloud as its amount asks for.
    float mixW[3], mixSum = 0.0f;
    for (int j = 0; j < 3; ++j)
    {
        const float a = cur (txMix1 + j);
        mixW[j] = src[(size_t) j] != nullptr ? a * a : 0.0f;
        mixSum += mixW[j];
    }
    if (mixSum < 1.0e-4f) return;
    float pickAt = rng.nextFloat() * mixSum;
    int k = 2;
    for (int j = 0; j < 3; ++j) { pickAt -= mixW[j]; if (pickAt <= 0.0f && mixW[j] > 0.0f) { k = j; break; } }
    if (mixW[k] <= 0.0f) for (int j = 0; j < 3; ++j) if (mixW[j] > 0.0f) k = j;
    const SourceData* s = src[(size_t) k];
    const int n = s->audio.getNumSamples();
    // Where this sound plays: the texture's note, its own step on top, or its own pitch.
    const auto st = slotStep (k);
    grainOwn = st.own;
    grainPc = st.pc;

    const float smear = cur (txSmear);
    float len = juce::jlimit (0.03f, 10.0f, cur (txLength) * std::exp (0.25f * gauss()));
    const bool rev = rng.nextFloat() < cur (txReverse);
    float semis = 0.0f, pos = 0.0f;

    // Where this grain is taken from: around the place you set for its sound, scattered more widely with Smear.
    const float q = clamp01 (cloudAt[(size_t) k] + gauss() * (0.004f + 0.2f * smear * smear));
    if (s->live)
    {
        pos = livePos (len);
        if (pos < 0.0f) { grainOwn = false; grainPc = 0; return; }   // nothing has been sent in yet
        if (! st.own) semis = 12.0f * (float) st.oct;
    }
    else if (! st.own && ! s->notes.empty() && numChordPcs > 0 && ! (s->wholeNote && s->rootSet.load (std::memory_order_relaxed) < -1.5f))
    {
        // The note under that point (or the note you've said the sound is) is moved by the smallest step onto the
        // chord, as it sits where this sound plays, then up or down an octave if that's where it plays.
        const double x = (double) q * (n - 1);
        const int note = noteIndexAt (s, (int) x);
        const float m = s->noteFor (x);
        semis = snapToChord (m, st.pc) - m + 12.0f * (float) st.oct;
        pos = fitInNote (s, note, x, semis, len);
        pretuned = true;
        visMidi = m + semis;
    }
    else
    {
        pos = q;
        if (! st.own) semis = 12.0f * (float) st.oct;   // no clear note: only an octave can move it
    }

    // Overlapping grains are quieter, so a dense cloud is as loud as a sparse one.
    const float overlap = juce::jmax (1.0f, densityEff * len * 0.7f);
    const float amp = 0.55f / std::sqrt (overlap);
    const int env = len < 0.12f ? envPerc : envHann;
    const float nf = 0.15f + 0.45f * rng.nextFloat() + 0.2f * (1.0f - swell);
    visSlot = k;
    spawnGrain (delay, s, pos, len, semis, env, amp, rev, rnd() * spreadEff, nf, toTexture, evCloud);
    pretuned = false;
    visMidi = -1.0f;
    visSlot = -1;
    grainOwn = false;
    grainPc = 0;
}

float Engine::livePos (float& len)
{
    if (liveSrc == nullptr) return -1.0f;
    const int n = liveSrc->audio.getNumSamples();
    const double reach = juce::jmin ((double) liveFilled, 8.0 * sr);   // the last eight seconds
    if (reach < 0.4 * sr) return -1.0f;
    double span = (double) len * sr;
    if (span > reach * 0.8) { span = reach * 0.8; len = (float) (span / sr); }
    const double age = span + 64.0 + (double) rng.nextFloat() * (reach - span - 64.0);
    double start = (double) liveWrite - age;
    if (start < 0.0) start += n;
    if (start + span > n - 4)   // would run across the ring's seam: take the stretch just behind the write point instead
    {
        start = (double) liveWrite - span - 64.0;
        if (start < 0.0) return -1.0f;
    }
    return (float) (start / (double) juce::jmax (1, n - 1));
}

void Engine::spawnDetail (int delay)
{
    // The small accidents that used to be the whole of Chance: now a seasoning.
    const float weights[4] = { 0.45f, 0.2f, 0.18f, 0.5f };
    float r = rng.nextFloat() * (weights[0] + weights[1] + weights[2] + weights[3]);
    int type = 0;
    while (type < 3 && r > weights[type]) { r -= weights[type]; ++type; }

    const SourceData* pool[3];
    int poolK[3], filled = 0;
    for (int k = 0; k < 3; ++k) if (src[(size_t) k] != nullptr) { poolK[filled] = k; pool[filled++] = src[(size_t) k]; }
    if (filled == 0) type = 3;
    const int pick = filled > 0 ? rng.nextInt (filled) : -1;
    const SourceData* s = pick >= 0 ? pool[pick] : nullptr;
    // The accidents play where their sound plays.
    const auto st = pick >= 0 && type != 3 ? slotStep (poolK[pick]) : NoteStep();
    grainOwn = st.own;
    grainPc = st.pc;

    const float W = 0.3f + 0.5f * cur (txDetail);
    const float d = 0.01f * std::pow (25.0f, rng.nextFloat());
    const float nf = clamp01 (0.3f + rnd() * 0.4f);
    const float spread = 0.4f + 0.6f * spreadEff;
    auto semisFor = [this, W, st] {
        float sm = gauss() * W * W * 7.0f;
        if (rng.nextFloat() < W * 0.3f) sm += rng.nextBool() ? 12.0f : -12.0f;
        sm = juce::jlimit (-24.0f, 24.0f, sm);
        return st.own ? sm : snapInterval (sm, st.pc) + 12.0f * (float) st.oct;
    };

    switch (type)
    {
        case 0:
            spawnGrain (delay, s, rng.nextFloat(), d, semisFor(), envPerc, 0.35f, rng.nextFloat() < 0.3f, rnd() * spread, nf, toTexture, evSpeck);
            break;
        case 1:
        {
            const int n = 3 + rng.nextInt (8);
            const float p0 = rng.nextFloat(), base = semisFor(), centre = rnd() * spread;
            for (int i = 0; i < n; ++i)
            {
                const float k = (float) i / (float) n;
                spawnGrain (delay + (int) (0.4f * k * sr * (0.6f + 0.8f * rng.nextFloat())), s, clamp01 (p0 + rnd() * 0.02f),
                            juce::jmax (0.006f, d * 0.6f), base, envPerc, 0.28f * (1.0f - 0.6f * k), false, centre + rnd() * 0.3f, nf, toTexture, evBurst);
            }
            break;
        }
        case 2:
        {
            const float p0 = rng.nextFloat(), base = semisFor(), pan = rnd() * spread;
            if (rng.nextFloat() < 0.6f)
            {
                const float beat = (float) (60.0 / bpm);
                float L = juce::jlimit (0.01f, 0.25f, beat * (rng.nextBool() ? 0.25f : 0.125f));
                const int reps = 2 + rng.nextInt (6);
                float t = 0.0f;
                for (int i = 0; i < reps; ++i)
                {
                    spawnGrain (delay + (int) (t * sr), s, p0, L, base, envGate, 0.3f, false, pan, nf, toTexture, evGlitch);
                    t += L;
                }
            }
            else
            {
                const float dur = 0.2f + rng.nextFloat() * 0.5f;
                spawnGrain (delay, s, p0, dur, base, envGate, 0.32f, false, pan, nf, toTexture, evGlitch,
                            std::pow (0.02, 1.0 / juce::jmax (1, (int) (dur * sr))));
            }
            break;
        }
        default:
        {
            const int n = 1 + rng.nextInt (4);
            for (int i = 0; i < n; ++i)
            {
                const float amp = 0.1f + (rng.nextFloat() < 0.12f ? 0.6f : 0.25f) * rng.nextFloat();
                spawnGrain (delay + (int) (rng.nextFloat() * 0.25f * sr), nullptr, 0.0f, 0.0006f + rng.nextFloat() * 0.005f, 0.0f, envDust, amp,
                            false, rnd(), clamp01 (nf + rnd() * 0.2f), toTexture, evDust);
            }
            break;
        }
    }
    grainOwn = false;
    grainPc = 0;
}

void Engine::scheduleTexture (int numSamples, float dt)
{
    // Each cloud wanders a little around the place you set, unless frozen. Smear lets it wander further.
    static const double rates[3] = { 0.021, 0.034, 0.027 };
    for (int k = 0; k < 3; ++k)
    {
        if (! frozen) { wanderPh[(size_t) k] += dt * rates[k]; if (wanderPh[(size_t) k] >= 1.0) wanderPh[(size_t) k] -= 1.0; }
        const float wander = std::sin (twoPi * (float) wanderPh[(size_t) k]) * (0.008f + 0.05f * cur (txSmear));
        cloudAt[(size_t) k] = clamp01 (cur (txPos1 + k) + wander);
        readPosPub[(size_t) k].store (cloudAt[(size_t) k], std::memory_order_relaxed);
    }

    int guard = 0;
    while (nextCloudIn < numSamples && guard++ < 16)
    {
        spawnCloud ((int) juce::jmax (0.0, nextCloudIn));
        nextCloudIn += juce::jmax (1.0, (0.4 + 1.2 * rng.nextFloat()) / (densityEff * arrGrains) * sr);
    }
    nextCloudIn -= numSamples;

    const float detailRate = std::pow (cur (txDetail), 1.5f) * 9.0f;
    if (detailRate > 0.01f)
    {
        guard = 0;
        while (nextDetailIn < numSamples && guard++ < 8)
        {
            spawnDetail ((int) juce::jmax (0.0, nextDetailIn));
            const float u = juce::jmin (0.9999f, rng.nextFloat());
            nextDetailIn += juce::jmax (1.0, -std::log (1.0 - u) / detailRate * sr);
        }
        nextDetailIn -= numSamples;
    }
    else nextDetailIn = 0.0;
}

void Engine::renderGrains (int numSamples)
{
    for (size_t a = 0; a < activeList.size();)
    {
        auto& g = grains[(size_t) activeList[a]];
        if (g.delay >= numSamples) { g.delay -= numSamples; ++a; continue; }
        int s = g.delay;
        g.delay = 0;

        auto& near = g.dest == toBed ? bed : (g.raw ? rawNear : txNear);
        auto& far = g.raw ? rawFar : txFar;
        float* oL = near.getWritePointer (0);
        float* oR = near.getWritePointer (1);
        float* fL = far.getWritePointer (0);
        float* fR = far.getWritePointer (1);
        const float invLen = 1.0f / (float) g.len;
        bool done = false;

        if (g.env == envDust)
        {
            for (; s < numSamples; ++s)
            {
                g.seed = g.seed * 1664525u + 1013904223u;
                const float noise = (float) (g.seed >> 8) * (1.0f / 8388608.0f) - 1.0f;
                g.dustLp += 0.5f * (noise - g.dustLp);
                const float v = (g.dustLp - g.dustPrev) * lookup (percTable, (float) g.age * invLen) * g.amp;
                g.dustPrev = g.dustLp;
                oL[s] += v * g.nl; oR[s] += v * g.nr;
                fL[s] += v * g.fl; fR[s] += v * g.fr;
                if (++g.age >= g.len) { done = true; break; }
            }
        }
        else
        {
            const auto& audio = g.src->audio;
            const int n = audio.getNumSamples();
            const float* c0 = audio.getReadPointer (0);
            const float* c1 = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : c0;
            const auto& table = g.env == envPerc ? percTable : (g.env == envSwell ? swellTable : hannTable);
            for (; s < numSamples; ++s)
            {
                float w;
                if (g.env == envGate)
                {
                    const int fade = juce::jmin ((int) (0.002 * sr), g.len / 3);
                    w = juce::jmin (1.0f, juce::jmin ((float) g.age, (float) (g.len - g.age)) / (float) juce::jmax (1, fade));
                }
                else w = lookup (table, (float) g.age * invLen);
                w *= g.amp;

                float v0, v1;
                if (g.crush > 0)
                {
                    if (g.crushCount <= 0)
                    {
                        g.held0 = hermite (c0, n, g.pos);
                        g.held1 = c1 == c0 ? g.held0 : hermite (c1, n, g.pos);
                        if (g.bits > 0.0f) { g.held0 = std::round (g.held0 * g.bits) / g.bits; g.held1 = std::round (g.held1 * g.bits) / g.bits; }
                        g.crushCount = g.crush;
                    }
                    --g.crushCount;
                    v0 = g.held0; v1 = g.held1;
                }
                else
                {
                    v0 = hermite (c0, n, g.pos);
                    v1 = c1 == c0 ? v0 : hermite (c1, n, g.pos);
                }
                oL[s] += v0 * w * g.nl; oR[s] += v1 * w * g.nr;
                if (g.dest == toTexture) { fL[s] += v0 * w * g.fl; fR[s] += v1 * w * g.fr; }

                g.pos += g.inc;
                g.inc *= g.incMul;
                if (++g.age >= g.len || g.pos < 0.0 || g.pos >= n - 1) { done = true; break; }
            }
        }

        if (done)
        {
            freeList.push_back (activeList[a]);
            activeList[a] = activeList.back();
            activeList.pop_back();
        }
        else ++a;
    }
}

/* ---------------- Drones ---------------- */

void Engine::scheduleBed (int numSamples)
{
    const SourceData* s = src[slotBed];
    if (s == nullptr) { nextBedIn = 0.0; return; }
    int guard = 0;
    while (nextBedIn < numSamples && guard++ < 4)
    {
        if (! frozen) bedDrift = clamp01 (bedDrift + rnd() * 0.06f);
        float len = juce::jmin (6.0f, (float) (s->audio.getNumSamples() / s->sampleRate) * 0.8f);
        float at = bedDrift, semis = 0.0f;
        if (s->live) { len = 3.0f; at = livePos (len); if (! bedStep.own) semis = 12.0f * (float) bedStep.oct; }
        else if (bedStep.own) {}   // left at its own pitch
        else
        {
            // A bed with a note of its own is moved, by the smallest step, onto the key (its root or its fifth),
            // so a new sound sits in the piece instead of beside it. Rain, wind and rooms are left as they are.
            const double x = (double) bedDrift * (s->audio.getNumSamples() - 1);
            int note = -1;
            float m = -1.0f;
            const float said = s->wholeNote ? s->rootSet.load (std::memory_order_relaxed) : SourceData::rootAuto;
            if (said >= 0.0f) m = said;                       // you've said what note it is
            else if (said < -1.5f) m = -1.0f;                 // you've said it has none
            else if (s->coreMidi > 0.0f) { if (s->coreStrength >= 1.6f) m = s->coreMidi; }
            else if (! s->notes.empty()) { note = noteIndexAt (s, (int) x); m = s->notes[(size_t) note].midi; }
            if (m > 0.0f)
            {
                // Where the bed plays: the root and its fifth, moved by the bed's own note.
                float best = 100.0f;
                for (int pc : { (key + bedStep.pc) % 12, (key + 7 + bedStep.pc) % 12 })
                {
                    float d = std::fmod ((float) pc - m, 12.0f);
                    if (d > 6.0f) d -= 12.0f;
                    if (d < -6.0f) d += 12.0f;
                    if (std::abs (d) < std::abs (best)) best = d;
                }
                semis = best + 12.0f * (float) bedStep.oct;
                if (note >= 0) at = fitInNote (s, note, x, semis, len);
            }
            else semis = 12.0f * (float) bedStep.oct;   // rain, wind and rooms only move by octaves, and only if you ask
        }
        readPosPub[slotBed].store (clamp01 (at), std::memory_order_relaxed);
        if (at >= 0.0f)
            spawnGrain ((int) juce::jmax (0.0, nextBedIn), s, at, len, semis, envHann, 0.8f, false, 0.0f, 0.0f, toBed, evCloud);
        nextBedIn += len * 0.5 * sr;
    }
    nextBedIn -= numSamples;
}

void Engine::makeSmearFrame()
{
    const int N = fftSize;
    const float breath = cur (drBreath), feed = cur (txFeed);

    // The drones' own sound, read at a slowly moving point and tuned to the chord
    float srcPow = 0.0f;
    const SourceData* s = src[slotTone];
    if (s != nullptr && s->audio.getNumSamples() > 64)
    {
        const int n = s->audio.getNumSamples();
        const float* c0 = s->audio.getReadPointer (0);
        const float* c1 = s->audio.getNumChannels() > 1 ? s->audio.getReadPointer (1) : c0;
        double ratio = s->sampleRate / sr;
        const float m = s->noteFor (smearPos);
        smearRefMidi = -1.0f;
        if (m > 0.0f)
        {
            smearRefMidi = snapToChord (m, drStep.pc);
            ratio *= std::pow (2.0, (smearRefMidi - m) / 12.0);
        }
        double start = smearPos + (double) (rnd() * (float) N * (0.2f + 0.8f * breath));
        if (s->live)   // the last few seconds of what's sent in
            start = (double) liveWrite - N - 64 - (double) rng.nextFloat() * juce::jmax (0.0, juce::jmin ((double) liveFilled - N - 64, 4.0 * sr));
        for (int i = 0; i < N; ++i)
        {
            double p = std::fmod (start + i * ratio, (double) n);
            if (p < 0) p += n;
            const int i0 = juce::jlimit (0, n - 1, (int) p), i1 = i0 + 1 < n ? i0 + 1 : 0;
            const float fr = (float) (p - i0);
            const float a = 0.5f * (c0[i0] + c1[i0]), b = 0.5f * (c0[i1] + c1[i1]);
            const float v = a + (b - a) * fr;
            frame[(size_t) i] = v;
            srcPow += v * v;
        }
        // Reading the drones' sound wears it, a little, every so often.
        if (! s->live && ++smearWearTick >= 8) { smearWearTick = 0; queueWear (s, start, start + N * ratio); }
        if (! frozen)
        {
            smearPos += hop * (0.03 + 0.3 * breath) * s->sampleRate / sr;
            if (smearPos >= n) smearPos -= n;
        }
        readPosPub[slotTone].store ((float) (smearPos / (double) n), std::memory_order_relaxed);
    }
    else { std::fill (frame.begin(), frame.end(), 0.0f); smearRefMidi = -1.0f; }
    const float srcRms = std::sqrt (srcPow / (float) N);

    // The sediment: what the texture has left behind
    const int sedLen = (int) sediment.size();
    float sedPow = 0.0f;
    if (feed > 0.001f && sedLen > N)
    {
        const int start = (sedWrite - N - rng.nextInt (sedLen - N) + 2 * sedLen) % sedLen;
        for (int i = 0; i < N; ++i)
        {
            const float v = sediment[(size_t) ((start + i) % sedLen)];
            sedFrame[(size_t) i] = v;
            sedPow += v * v;
        }
    }
    const float sedRms = std::sqrt (sedPow / (float) N);
    sedPower += 0.2f * (sedRms - sedPower);

    const bool haveSrc = srcRms > 1.0e-4f, haveSed = sedRms > 2.0e-4f && feed > 0.001f;
    const float wSrc = haveSrc ? (haveSed ? 1.0f - feed : 1.0f) / srcRms : 0.0f;
    const float wSed = haveSed ? (haveSrc ? feed : 1.0f) / sedRms : 0.0f;
    const float air = 0.22f + (haveSrc || haveSed ? 0.0f : 0.5f);

    for (int i = 0; i < N; ++i)
        fftBuf[(size_t) i] = (frame[(size_t) i] * wSrc + (haveSed ? sedFrame[(size_t) i] * wSed : 0.0f) + air * rnd()) * window[(size_t) i];
    std::fill (fftBuf.begin() + N, fftBuf.end(), 0.0f);
    fft.performRealOnlyForwardTransform (fftBuf.data(), true);

    // Keep each frequency's strength, throw away its phase: the sound becomes an endless smear.
    for (int k = 0; k <= N / 2; ++k)
    {
        const float re = fftBuf[(size_t) (2 * k)], im = fftBuf[(size_t) (2 * k + 1)];
        frame[(size_t) k] = std::sqrt (re * re + im * im);
    }
    constexpr float olaScale = 0.22f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int k = 0; k <= N / 2; ++k)
        {
            const float m = frame[(size_t) k];
            if (k == 0 || k == N / 2) { fftBuf[(size_t) (2 * k)] = m; fftBuf[(size_t) (2 * k + 1)] = 0.0f; continue; }
            const int r = rng.nextInt (4096);
            fftBuf[(size_t) (2 * k)] = m * cosTab[(size_t) r];
            fftBuf[(size_t) (2 * k + 1)] = m * sinTab[(size_t) r];
        }
        fft.performRealOnlyInverseTransform (fftBuf.data());
        auto& ola = ch == 0 ? olaL : olaR;
        for (int i = 0; i < N; ++i)
        {
            int idx = olaPos + i;
            if (idx >= N) idx -= N;
            ola[(size_t) idx] += fftBuf[(size_t) i] * window[(size_t) i] * olaScale;
        }
    }
    makeVoiceFrame();
}

/* ---------------- Voices made of the sound ---------------- */

void Engine::addVoiceSpectrum (std::vector<float>& mag, float f0, float w, bool pitched, float refHz)
{
    // One note of the chord, made of the sound: its spectral envelope (its colour, its formants) stays where
    // it is, while its fine structure (its partials) is moved so that its note lands on f0. A sound with no
    // note gets partials of its own: its noise is gathered into the harmonics of f0, like wind singing a note.
    const int K = fftSize / 2;
    const float binHz = (float) sr / (float) fftSize;
    if (f0 < 20.0f || w < 1.0e-5f) return;
    const float wid = juce::jlimit (0.05f, 0.45f, 1.4f * binHz / f0);
    const float invWid = 1.0f / wid, step = binHz / f0, shift = refHz / f0;
    const int k0 = juce::jmax (1, (int) (0.5f * f0 / binHz));
    for (int k = k0; k < K; ++k)
    {
        const float h = (float) k * step;
        const float d = (h - std::round (h)) * invWid;
        const float peak = d * d < 1.0f ? (1.0f - d * d) * (1.0f - d * d) : 0.0f;
        float shape;
        if (pitched)
        {
            const float kk = (float) k * shift;
            if (kk >= (float) (K - 1)) break;
            const int i0 = (int) kk;
            const float fs = fineBuf[(size_t) i0] + (fineBuf[(size_t) i0 + 1] - fineBuf[(size_t) i0]) * (kk - (float) i0);
            shape = fs * (0.15f + 0.85f * peak);
        }
        else shape = 1.6f * peak;
        mag[(size_t) k] += w * envBuf[(size_t) k] * shape;
    }
}

void Engine::makeVoiceFrame()
{
    const int N = fftSize, K = N / 2;
    for (auto& v : vMag) std::fill (v.begin(), v.end(), 0.0f);
    if (voiceMixNow < 0.001f) return;

    // The envelope: the spectrum smoothed over a width that grows with frequency. What is left over
    // (the spectrum divided by it) is the fine structure: where the partials are.
    prefix[0] = 0.0f;
    for (int k = 0; k <= K; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + frame[(size_t) k];
    for (int k = 0; k <= K; ++k)
    {
        const int r = juce::jmax (3, (int) ((float) k * 0.09f));
        const int a = juce::jmax (0, k - r), b = juce::jmin (K, k + r);
        envBuf[(size_t) k] = (prefix[(size_t) b + 1] - prefix[(size_t) a]) / (float) (b - a + 1);
        fineBuf[(size_t) k] = frame[(size_t) k] / (envBuf[(size_t) k] + 1.0e-9f);
    }

    const bool pitched = smearRefMidi > 0.0f;
    const float refHz = pitched ? midiToHz (smearRefMidi) : 0.0f;
    const float bodyEffNow = bodyEff, breath = cur (drBreath);
    for (int v = 0; v < numVoices; ++v)
    {
        const auto& V = voices[(size_t) v];
        if (V.amp < 1.0e-4f) continue;
        const bool isDrone = v < 3, isLine = v == lineVoice;
        const float cents = (isLine ? 1.5f : isDrone ? 2.0f + 4.0f * bodyEffNow : 4.0f + 10.0f * bodyEffNow) + breath * 6.0f * V.lfo;
        const float spreadL = std::pow (2.0f, -cents / 1200.0f), spreadR = std::pow (2.0f, cents / 1200.0f);
        const float wNew = V.amp * V.gNew, wOld = V.xfade < 1.0f ? V.amp * V.gOld : 0.0f;
        addVoiceSpectrum (vMag[0], V.hz * spreadL, wNew, pitched, refHz);
        addVoiceSpectrum (vMag[1], V.hz * spreadR, wNew, pitched, refHz);
        if (wOld > 1.0e-5f)
        {
            addVoiceSpectrum (vMag[0], V.oldHz * spreadL, wOld, pitched, refHz);
            addVoiceSpectrum (vMag[1], V.oldHz * spreadR, wOld, pitched, refHz);
        }
    }

    // Random phases again: the voices are held, never played from the start.
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& mag = vMag[(size_t) ch];
        for (int k = 0; k <= K; ++k)
        {
            const float m = mag[(size_t) k];
            if (k == 0 || k == K) { fftBuf[(size_t) (2 * k)] = 0.0f; fftBuf[(size_t) (2 * k + 1)] = 0.0f; continue; }
            const int r = rng.nextInt (4096);
            fftBuf[(size_t) (2 * k)] = m * cosTab[(size_t) r];
            fftBuf[(size_t) (2 * k + 1)] = m * sinTab[(size_t) r];
        }
        fft.performRealOnlyInverseTransform (fftBuf.data());
        auto& ola = ch == 0 ? vOlaL : vOlaR;
        for (int i = 0; i < N; ++i)
        {
            int idx = olaPos + i;
            if (idx >= N) idx -= N;
            ola[(size_t) idx] += fftBuf[(size_t) i] * window[(size_t) i];
        }
    }
}

/* ---------------- Wear ---------------- */

void Engine::queueWear (const SourceData* s, double from, double to)
{
    if (s == nullptr || ! s->worn || s->live || rawWear == nullptr || rawWear->load() < 0.001f || wearCount >= maxWearSpans) return;
    const int n = s->audio.getNumSamples();
    int a = (int) juce::jmin (from, to), b = (int) std::ceil (juce::jmax (from, to));
    a = juce::jlimit (0, n, a); b = juce::jlimit (0, n, b);
    if (b - a < 16) return;
    auto& w = wearSpans[(size_t) ((wearHead + wearCount) % maxWearSpans)];
    // A worn sound is this piece's own copy, made for it, so it may be written to here.
    w.s = const_cast<SourceData*> (s);
    w.start = w.pos = a; w.end = b;
    // Now and then a pass leaves a hole: a stretch that comes back quieter every time.
    const float amt = std::pow (rawWear->load(), 1.5f);
    if (rng.nextFloat() < 0.35f * amt)
    {
        const int len = (int) (s->sampleRate * (0.03 + 0.4 * rng.nextFloat()));
        w.dropA = a + rng.nextInt (juce::jmax (1, b - a));
        w.dropB = juce::jmin (b, w.dropA + len);
        w.dropDepth = 0.15f + 0.35f * rng.nextFloat();
    }
    else w.dropA = w.dropB = 0;
    ++wearCount;
}

void Engine::runWear (int budget)
{
    // Every pass over a stretch takes a little of its air (the highs), adds a little hiss, rounds its
    // peaks off a little, and deepens any hole in it. It adds up, and it never comes back.
    const float wear = rawWear != nullptr ? rawWear->load() : 0.0f;
    if (wear < 0.001f) { wearCount = 0; return; }
    const float eps = 0.1f * std::pow (wear, 1.5f);
    while (budget > 0 && wearCount > 0)
    {
        auto& w = wearSpans[(size_t) wearHead];
        auto* s = w.s;
        const int chans = s->audio.getNumChannels();
        const float aLp = 1.0f - std::exp (-twoPi * 2600.0f / (float) s->sampleRate);
        if (w.pos == w.start) for (int c = 0; c < 2; ++c) { wearLp[(size_t) c] = s->audio.getSample (juce::jmin (c, chans - 1), w.start); wearHiss[(size_t) c] = 0.0f; }
        const int stop = juce::jmin (w.end, w.pos + budget);
        const float dropLen = (float) juce::jmax (1, w.dropB - w.dropA);
        for (int c = 0; c < chans; ++c)
        {
            float* d = s->audio.getWritePointer (c);
            float lp = wearLp[(size_t) c], hiss = wearHiss[(size_t) c];
            for (int i = w.pos; i < stop; ++i)
            {
                float x = d[i];
                lp += aLp * (x - lp);
                x += eps * (lp - x);
                hiss += 0.6f * (rnd() - hiss);
                x += eps * 0.01f * hiss;
                x -= eps * 0.25f * (x - std::tanh (1.8f * x) / 1.8f);
                if (i >= w.dropA && i < w.dropB)
                {
                    const float t = (float) (i - w.dropA) / dropLen;
                    x *= 1.0f - eps * 4.0f * w.dropDepth * (0.5f - 0.5f * std::cos (twoPi * t));
                }
                d[i] = x;
            }
            wearLp[(size_t) c] = lp; wearHiss[(size_t) c] = hiss;
        }
        budget -= stop - w.pos;
        w.pos = stop;
        s->wornChanged.store (true, std::memory_order_relaxed);
        if (w.pos >= w.end) { wearHead = (wearHead + 1) % maxWearSpans; --wearCount; }
    }
}

/* ---------------- Tape ---------------- */

void Engine::runTape (Tape& t, juce::AudioBuffer<float>& a, juce::AudioBuffer<float>& b, int numSamples, float amount, int stage)
{
    // One knob: a little is a gentle waver, a lot is a worn machine.
    amount = clamp01 (amount);
    const float wobble = juce::jmin (1.0f, amount * 1.2f), age = std::pow (amount, 1.3f);
    juce::AudioBuffer<float>* pairs[2] = { &a, &b };
    const int tlen = (int) t.buf[0][0].size(), tmask = tlen - 1;
    const float baseD = (float) (0.004 * sr);
    const float lossHz = 18000.0f * std::pow (3500.0f / 18000.0f, age);
    const float driveT = 1.0f + 2.2f * age, bias = 0.12f * age;
    const float tb = std::tanh (bias), norm = std::pow (driveT, 0.72f) * (1.0f - tb * tb);
    const float hissLevel = (stage == 0 ? 0.0004f * juce::jmin (1.0f, amount * 10.0f) + 0.005f * age * age : 0.002f * age * age);
    const float hissA = 1.0f - std::exp (-twoPi * 5000.0f / (float) sr);
    const float dropRate = age * age * 0.35f + (age > 0.3f ? 0.05f : 0.0f);
    const float fastC = 1.0f - std::exp (-1.0f / (0.01f * (float) sr)), slowC = 1.0f - std::exp (-1.0f / (0.04f * (float) sr));
    for (int s = 0; s < numSamples; ++s)
    {
        t.wowT += t.wowRate / (float) sr;
        if (t.wowT >= 1.0f) { t.wowT -= 1.0f; t.wowA = t.wowB; t.wowB = rnd(); t.wowRate = 0.35f + rng.nextFloat() * 0.8f; }
        const float wow = t.wowA + (t.wowB - t.wowA) * (0.5f - 0.5f * std::cos (pi * t.wowT));
        t.wowNow = wow;
        t.flutterPh += 6.3f / (float) sr;
        if (t.flutterPh >= 1.0f) t.flutterPh -= 1.0f;
        const float flutter = std::sin (twoPi * t.flutterPh) * (0.8f + 0.2f * wow);
        const float d = baseD + wobble * (wow * 0.0028f * (float) sr + flutter * 0.00012f * (float) sr);

        if (t.dropHold > 0) --t.dropHold;
        else if (dropRate > 0.0f && rng.nextFloat() < dropRate / (float) sr)
        {
            t.dropTarget = (0.3f + 0.6f * rng.nextFloat()) * age;
            t.dropHold = (int) ((0.04f + rng.nextFloat() * 0.36f) * (float) sr);
        }
        t.dropEnv += t.dropHold > 0 ? (t.dropTarget - t.dropEnv) * fastC : (0.0f - t.dropEnv) * slowC;
        const float gainDrop = 1.0f - t.dropEnv;
        const float lpA = 1.0f - std::exp (-twoPi * lossHz * (1.0f - 0.7f * t.dropEnv) / (float) sr);

        float rp = (float) t.write - d;
        if (rp < 0.0f) rp += (float) tlen;
        const int i0 = juce::jlimit (0, tmask, (int) rp), i1 = (i0 + 1) & tmask;
        const float fr = rp - (float) i0;
        for (int p = 0; p < 2; ++p)
            for (int c = 0; c < 2; ++c)
            {
                auto& line = t.buf[(size_t) p][(size_t) c];
                line[(size_t) t.write] = pairs[p]->getSample (c, s);
                float y = line[(size_t) i0] + (line[(size_t) i1] - line[(size_t) i0]) * fr;
                y = (std::tanh (driveT * y + bias) - tb) / norm;
                auto& lpS = t.lp[(size_t) p][(size_t) c];
                lpS += lpA * (y - lpS);
                y = lpS;
                const float dc = y - t.dcX[(size_t) p][(size_t) c] + 0.9995f * t.dcY[(size_t) p][(size_t) c];
                t.dcX[(size_t) p][(size_t) c] = y; t.dcY[(size_t) p][(size_t) c] = dc;
                y = dc * gainDrop;
                if (p == 0) { t.hissPrev += hissA * (rnd() - t.hissPrev); y += t.hissPrev * hissLevel; }
                pairs[p]->setSample (c, s, y);
            }
        t.write = (t.write + 1) & tmask;
    }
    tapeWowPub[(size_t) stage].store (t.wowNow * wobble, std::memory_order_relaxed);
    tapeDropPub[(size_t) stage].store (t.dropEnv, std::memory_order_relaxed);
}

/* ---------------- Main process ---------------- */

void Engine::process (const Outputs& out, int numSamples, juce::AudioPlayHead* playHead)
{
    juce::ScopedNoDenormals noDenormals;
    auto clearPair = [numSamples] (float* const* p) { for (int c = 0; c < 2; ++c) if (p[c] != nullptr) juce::FloatVectorOperations::clear (p[c], numSamples); };
    clearPair (out.main); clearPair (out.texture); clearPair (out.drones); clearPair (out.send);
    if (out.main[0] == nullptr || out.main[1] == nullptr || numSamples > maxBlock || grains.empty()) return;

    // Keys played since the last block, read together as one chord.
    if (keysSettle >= 0)
    {
        keysSettle -= numSamples;
        if (keysSettle < 0) applyKeys();
    }

    // Host transport
    double ppq = 0.0, barLength = 4.0;
    bool hostPlaying = false, havePpq = false, hostTempo = false;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostTempo = true; }
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0) barLength = 4.0 * ts->numerator / ts->denominator;
        }
    if (! hostTempo) bpm = juce::jlimit (40.0, 180.0, (double) rawTempo->load());
    hostTempoPub.store (hostTempo, std::memory_order_relaxed);
    bpmPub.store ((float) bpm, std::memory_order_relaxed);

    // The side chain keeps filling the live ring, playing or not.
    if (liveSrc != nullptr && liveSrc->audio.getNumSamples() > 0)
    {
        auto& a = liveSrc->audio;
        const int n = a.getNumSamples();
        float* l = a.getWritePointer (0);
        float* r = a.getWritePointer (1);
        float pk = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x0 = out.input[0] != nullptr ? out.input[0][i] : 0.0f;
            const float x1 = out.input[1] != nullptr ? out.input[1][i] : x0;
            l[liveWrite] = x0; r[liveWrite] = x1;
            pk = juce::jmax (pk, std::abs (x0), std::abs (x1));
            if (++liveWrite >= n) liveWrite = 0;
        }
        liveFilled = juce::jmin (n, liveFilled + numSamples);
        inputLevel = juce::jmax (pk, inputLevel * 0.97f);
        inputPub.store (inputLevel, std::memory_order_relaxed);
    }

    // Parts
    const int adopted = adoptPart.exchange (-1);
    if (adopted >= 0) { curPart = adopted; partElapsed = 0.0; }
    const int sceneParam = juce::roundToInt (rawPart->load());
    if (adoptScene.exchange (false)) lastSceneParam = sceneParam;
    if (sceneParam != lastSceneParam)
    {
        lastSceneParam = sceneParam;
        if (sceneParam > 0 && sceneParam - 1 != curPart) startRecall (sceneParam - 1, -1.0f);
    }
    const int manual = manualRecall.exchange (-1);
    if (manual >= 0) startRecall (manual, -1.0f);
    else if (pendingRecall >= 0) { const int p = pendingRecall; pendingRecall = -1; startRecall (p, -1.0f); }
    if (rewindReq.exchange (false))
    {
        // From the start: the first chord, the clock at zero. The scene you're in stays.
        runSeconds = 0.0; partElapsed = 0.0; internalBars = 0.0;
        cueWhen.store (2); cue.store (0);
    }

    // Sounds: a part's sound plays until the interface catches up
    for (int s = 0; s < numSlots; ++s)
    {
        auto i = (size_t) s;
        SourceData* base = slotSrc[i].load();
        if (slotOverride[i] != nullptr && base != slotAtRecall[i]) slotOverride[i] = nullptr;
        src[i] = slotOverride[i] != nullptr ? slotOverride[i] : base;
        playingSrc[i].store (src[i]);
    }

    running = rawPlay->load() > 0.5f && (rawSync->load() < 0.5f || hostPlaying);
    wasRunning = running;
    runningPub.store (running, std::memory_order_relaxed);

    // Stop means silence: everything (drones, bed, space) fades out over a second and a half, then
    // Grain Field rests until you press play, carrying on from where it was.
    const float fade0 = playFade;
    {
        const float step = (float) numSamples / (float) sr / (running ? 0.4f : 1.5f);
        playFade = running ? juce::jmin (1.0f, playFade + step) : juce::jmax (0.0f, playFade - step);
    }
    const float fade1 = playFade;
    if (! running && fade0 <= 0.0f)
    {
        levelMeter *= 0.5f;
        levelPub.store (levelMeter, std::memory_order_relaxed);
        return;
    }
    const bool inside = rawSpaceMode->load() < 0.5f;

    // Nothing moves on by itself: you, or your DAW's automation of the scene, decide when.
    hostWasPlaying = hostPlaying;

    for (auto* b : { &mainBus, &sendBus, &echoIn, &textureOut, &dronesOut, &txSend, &drSend }) b->clear (0, numSamples);

    const float ampCoef = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));
    float rateAcc = 0.0f;

    for (int off = 0; off < numSamples; off += controlBlock)
    {
        const int m = juce::jmin (controlBlock, numSamples - off);
        const float dt = (float) m / (float) sr;
        updateControl (m);
        updateSwell();
        updateArrive (dt);
        if (running) { runSeconds += dt; partElapsed += dt; }
        updateHarmony (dt, hostPlaying, havePpq, ppq + (off / sr) * bpm / 60.0, barLength);

        // The drones grow toward what Swell asks for. Freeze holds them.
        if (! frozen && running)
        {
            const float tau = 600.0f * std::pow (3.0f / 600.0f, cur (drGrow));
            const float target = (0.12f + 0.88f * std::pow (swell, 0.8f)) * (0.4f + 0.6f * arrDrones);
            mass += (target - mass) * (1.0f - std::exp (-dt / tau));
        }

        txNear.clear (0, m); txFar.clear (0, m); rawNear.clear (0, m); rawFar.clear (0, m); bed.clear (0, m);
        if (running) scheduleTexture (m, dt);
        if (running && cur (drBed) > 0.001f) scheduleBed (m);
        renderGrains (m);
        runWear (m * 24);

        // Per-block settings
        const float merge = cur (txFeed);
        // Merge: the more of it, the more the texture rings at the chord's notes and the less of it,
        // and of the bed, is heard dry: they're taken up into the drones instead of sitting beside them.
        const float tuneMix = 0.1f + 0.45f * merge, tunedGain = 2.5f - 0.9f * merge;
        for (auto& f : farTone) f.set (5200.0f - 2500.0f * (1.0f - swell), 0.6f, sr);
        for (auto& f : farToneRaw) f.set (5200.0f - 2500.0f * (1.0f - swell), 0.6f, sr);
        const float lift = 0.16f + 0.84f * std::pow (swell, 1.1f);  // the swell is also the loudness: quiet start, full wall
        textureGain.setTargetValue (cur (txLevel) * cur (txLevel) * 1.6f * lift * (1.0f - 0.65f * merge * merge) * arrTexture * (isAudible (stTexture) ? 1.0f : 0.0f));

        const float breath = cur (drBreath);
        const float colourEff = clamp01 (cur (drColour) * (0.55f + 0.45f * mass) + 0.2f * swell);
        const float toneHz = colourCutoffHz (colourEff) * (0.6f + 0.4f * mass);
        for (auto& f : droneTone) f.set (toneHz, 0.7f, sr);
        for (auto& f : bedTone) f.set (colourCutoffHz (0.5f + 0.5f * cur (drColour)), 0.6f, sr);
        dronesGain.setTargetValue (cur (drLevel) * cur (drLevel) * 1.4f * (0.35f + 0.65f * lift) * (1.0f + 0.3f * merge) * arrDrones * (isAudible (stDrones) ? 1.0f : 0.0f));
        bedOn += ((isAudible (stBed) ? 1.0f : 0.0f) - bedOn) * 0.12f;
        const float bedGain = cur (drBed) * (0.6f + 0.4f * swell) * 1.2f * bedOn * bedOn * (1.0f - 0.6f * merge * merge) * arrBed;
        const float bedToDrones = cur (drBed) * merge * 0.8f;
        const float fill = mass * (1.0f + bodyEff * 7.0f);
        const float lineAmt = cur (hmLine);
        voiceMixNow = cur (drVoice);
        const float combMix = std::sqrt (1.0f - voiceMixNow), voiceGain = std::sqrt (voiceMixNow) * spectralVoiceGain;
        const float drive = 1.0f + 1.6f * mass + 0.6f * cur (drColour);

        // Changes of chord and key are crossfades, as long as Dissolve says.
        const float dTime = juce::jmax (0.05f, cur (hmDissolve));
        if (tunedX < 1.0f) tunedX = juce::jmin (1.0f, tunedX + dt / dTime * (tunedPending ? 4.0f : 1.0f));
        else if (tunedPending)
        {
            tunedPending = false;
            tunedOld = tuned;
            tunedX = 0.0f;
            for (auto& set : tuned) for (auto& f : set) f.reset();
            retuneBank();
        }
        const float tunedGN = tunedX < 1.0f ? swellIn (tunedX) : 1.0f, tunedGO = tunedX < 1.0f ? letGo (tunedX) : 0.0f;

        for (int v = 0; v < numVoices; ++v)
        {
            auto& V = voices[(size_t) v];
            V.lfoT += dt * V.lfoRate * (0.5f + breath);
            if (V.lfoT >= 1.0f) { V.lfoT -= 1.0f; V.lfoA = V.lfoB; V.lfoB = rnd(); }
            V.lfo = V.lfoA + (V.lfoB - V.lfoA) * (0.5f - 0.5f * std::cos (pi * V.lfoT));
            const bool isDrone = v < 3, isLine = v == lineVoice;

            // A change of note is a release and a swell, not a morph: the old note is no longer fed and rings
            // out on its own tail, while the new one swells up from nothing beside it. Nothing slides in pitch.
            const float vTime = isLine ? dTime * 0.5f : dTime;
            const bool wantsNew = std::abs (V.targetHz / V.hz - 1.0f) > 0.002f;
            if (V.xfade < 1.0f) V.xfade = juce::jmin (1.0f, V.xfade + dt / vTime * (wantsNew ? 4.0f : 1.0f));   // a newer note is waiting: finish sooner
            else if (wantsNew)
            {
                if (V.wait > 0.0f) V.wait -= dt;
                else
                {
                    std::swap (V.comb, V.old);
                    V.oldHz = V.hz;
                    V.hz = V.targetHz;
                    for (auto& cb : V.comb) { std::fill (cb.buf.begin(), cb.buf.end(), 0.0f); cb.lp = 0.0f; }
                    V.xfade = V.amp < 1.0e-4f ? 1.0f : 0.0f;
                }
            }
            V.gNew = V.xfade < 1.0f ? swellIn (V.xfade) : 1.0f;
            V.gOld = V.xfade < 1.0f ? letGo (V.xfade) : 0.0f;

            const float breathMod = 1.0f - breath * 0.6f * (0.5f + 0.5f * V.lfo);
            if (isLine) V.targetAmp = lineAmt * (0.25f + 0.75f * mass) * 0.9f * (1.0f - breath * 0.3f * (0.5f + 0.5f * V.lfo));
            else V.targetAmp = juce::jlimit (0.0f, 1.0f, fill - (float) v) * (v == 0 ? 0.6f : isDrone ? 0.85f : 0.75f) * breathMod;
            const float cents = (isLine ? 1.5f : isDrone ? 2.0f + 4.0f * bodyEff : 4.0f + 10.0f * bodyEff) + breath * 6.0f * V.lfo;
            const float aV = isLine ? juce::jlimit (0.05f, 0.95f, 0.45f + 0.45f * colourEff)
                                    : juce::jlimit (0.05f, 0.95f, 0.12f + 0.75f * colourEff + breath * 0.12f * V.lfo);
            setCombDelays (V, V.comb, V.hz, cents, aV);
            if (V.xfade < 1.0f) setCombDelays (V, V.old, V.oldHz, cents, aV);
            voiceAmpPub[(size_t) v].store (V.amp, std::memory_order_relaxed);
            voiceHzPub[(size_t) v].store (V.hz, std::memory_order_relaxed);
            voiceTargetPub[(size_t) v].store (V.targetHz, std::memory_order_relaxed);
            voiceOldPub[(size_t) v].store (V.oldHz, std::memory_order_relaxed);
            voiceXPub[(size_t) v].store (V.xfade, std::memory_order_relaxed);
        }
        const bool subWants = std::abs (subTargetHz / subHz - 1.0f) > 0.002f;
        if (subX < 1.0f) subX = juce::jmin (1.0f, subX + dt / dTime * (subWants ? 4.0f : 1.0f));
        else if (subWants)
        {
            subOldHz = subHz; subOldPhase = subPhase;
            subHz = subTargetHz;
            subX = subAmp < 1.0e-4f ? 1.0f : 0.0f;
        }
        const float subGN = subX < 1.0f ? swellIn (subX) : 1.0f, subGO = subX < 1.0f ? letGo (subX) : 0.0f;
        const float subTarget = subEff * mass * 0.32f * arrSub;
        voiceAmpPub[subVoice].store (subAmp, std::memory_order_relaxed);
        voiceHzPub[subVoice].store (subHz, std::memory_order_relaxed);
        voiceTargetPub[subVoice].store (subTargetHz, std::memory_order_relaxed);
        voiceOldPub[subVoice].store (subOldHz, std::memory_order_relaxed);
        voiceXPub[subVoice].store (subX, std::memory_order_relaxed);

        float* sed = sediment.data();
        const int sedLen = (int) sediment.size();
        const float lineA = juce::jlimit (0.05f, 0.95f, 0.45f + 0.45f * colourEff);

        for (int s = 0; s < m; ++s)
        {
            const int i = off + s;
            // Texture
            float nl = txNear.getSample (0, s), nr = txNear.getSample (1, s);
            float fl = txFar.getSample (0, s), fr = txFar.getSample (1, s);
            float bp, lp;
            farTone[0].tick (fl, bp, lp); fl = lp;
            farTone[1].tick (fr, bp, lp); fr = lp;
            {
                float* xs[4] = { &nl, &nr, &fl, &fr };
                for (int b = 0; b < 4; ++b)
                {
                    float sum = 0.0f;
                    for (auto& f : tuned[(size_t) b]) { f.tick (*xs[b], bp, lp); sum += bp; }
                    if (tunedX < 1.0f)
                    {
                        float sumOld = 0.0f;
                        for (auto& f : tunedOld[(size_t) b]) { f.tick (*xs[b], bp, lp); sumOld += bp; }
                        sum = sum * tunedGN + sumOld * tunedGO;
                    }
                    *xs[b] = *xs[b] * (1.0f - tuneMix) + sum * tuneMix * tunedGain;
                }
            }
            {
                // Sounds left at their own pitch join here, after the resonators, so nothing tunes them.
                float rfl = rawFar.getSample (0, s), rfr = rawFar.getSample (1, s);
                farToneRaw[0].tick (rfl, bp, lp); rfl = lp;
                farToneRaw[1].tick (rfr, bp, lp); rfr = lp;
                nl += rawNear.getSample (0, s); nr += rawNear.getSample (1, s);
                fl += rfl; fr += rfr;
            }
            const float tg = textureGain.getNextValue();
            const float tl = (nl + fl * 0.5f) * tg, tr = (nr + fr * 0.5f) * tg;

            // Sediment: the texture leaves layers behind for the drones to feed on
            float& sv = sed[sedWrite];
            sv = sv * 0.93f + (nl + nr + fl + fr) * 0.5f + (bed.getSample (0, s) + bed.getSample (1, s)) * bedToDrones;
            if (std::abs (sv) > 1.0f) sv = std::tanh (sv);
            if (++sedWrite >= sedLen) sedWrite = 0;

            // Drones: the smear excites tuned resonators
            if (hopCount <= 0) { makeSmearFrame(); hopCount = hop; }
            --hopCount;
            const float sL = olaL[(size_t) olaPos], sR = olaR[(size_t) olaPos];
            olaL[(size_t) olaPos] = 0.0f; olaR[(size_t) olaPos] = 0.0f;
            const float vL = vOlaL[(size_t) olaPos], vR = vOlaR[(size_t) olaPos];
            vOlaL[(size_t) olaPos] = 0.0f; vOlaR[(size_t) olaPos] = 0.0f;
            if (++olaPos >= fftSize) olaPos = 0;

            float gl = 0.0f, gr = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                auto& V = voices[(size_t) v];
                V.amp += (V.targetAmp - V.amp) * ampCoef;
                if (V.amp < 1.0e-5f && V.targetAmp < 1.0e-5f) continue;
                const bool isDrone = v < 3, isLine = v == lineVoice;
                const float g = (isLine ? 0.9975f : isDrone ? 0.9965f : 0.9935f) + (frozen ? 0.0015f : 0.0f);
                const float a = isLine ? lineA : juce::jlimit (0.05f, 0.95f, 0.12f + 0.75f * colourEff + breath * 0.12f * V.lfo);
                const float in = (1.0f - g) * (isLine ? 7.0f : 5.0f);
                // A note that is just arriving is fed a little harder, so it rings up as the old one lets go.
                const float inNew = in * (1.0f + 1.5f * (1.0f - V.xfade));
                float cL = combTick (V.comb[0].buf, V.comb[0].w, V.comb[0].lp, V.comb[0].delay, sL * inNew, g, a);
                float cR = combTick (V.comb[1].buf, V.comb[1].w, V.comb[1].lp, V.comb[1].delay, sR * inNew, g, a);
                if (V.xfade < 1.0f)
                {
                    // The old note is let go: nothing feeds it any more, it only rings out.
                    const float oL = combTick (V.old[0].buf, V.old[0].w, V.old[0].lp, V.old[0].delay, 0.0f, g, a);
                    const float oR = combTick (V.old[1].buf, V.old[1].w, V.old[1].lp, V.old[1].delay, 0.0f, g, a);
                    cL = cL * V.gNew + oL * V.gOld;
                    cR = cR * V.gNew + oR * V.gOld;
                }
                const float w = isDrone ? 0.3f : 0.6f + 0.4f * breath;
                gl += (cL * (0.5f + 0.5f * w) + cR * (0.5f - 0.5f * w)) * V.amp;
                gr += (cR * (0.5f + 0.5f * w) + cL * (0.5f - 0.5f * w)) * V.amp;
            }
            // The resonators and the voices made of the sound, as Voice sets.
            gl = gl * combMix + vL * voiceGain;
            gr = gr * combMix + vR * voiceGain;
            droneTone[0].tick (gl, bp, lp); gl = lp;
            droneTone[1].tick (gr, bp, lp); gr = lp;
            gl = std::tanh (gl * drive) / drive * (1.0f + 0.25f * mass);
            gr = std::tanh (gr * drive) / drive * (1.0f + 0.25f * mass);

            // The sub: a plain low sine under everything
            subAmp += (subTarget - subAmp) * ampCoef;
            subPhase += subHz / sr;
            if (subPhase >= 1.0) subPhase -= 1.0;
            float sub = std::sin (twoPi * (float) subPhase) * subGN;
            if (subX < 1.0f)
            {
                subOldPhase += subOldHz / sr;
                if (subOldPhase >= 1.0) subOldPhase -= 1.0;
                sub += std::sin (twoPi * (float) subOldPhase) * subGO;
            }
            sub *= subAmp;
            gl += sub; gr += sub;

            float ys[2] = { gl, gr };
            for (int c = 0; c < 2; ++c)
            {
                const float y = ys[c] - droneDcX[(size_t) c] + 0.9995f * droneDcY[(size_t) c];
                droneDcX[(size_t) c] = ys[c]; droneDcY[(size_t) c] = y; ys[c] = y;
            }
            const float dg = dronesGain.getNextValue();
            float b0 = bed.getSample (0, s), b1 = bed.getSample (1, s);
            bedTone[0].tick (b0, bp, lp); b0 = lp * bedGain;
            bedTone[1].tick (b1, bp, lp); b1 = lp * bedGain;
            const float dOutL = ys[0] * dg + b0, dOutR = ys[1] * dg + b1;

            // Each module keeps its own sound and its own share of the room; they meet after their tapes.
            textureOut.addSample (0, i, tl); textureOut.addSample (1, i, tr);
            txSend.addSample (0, i, (nl * 0.2f + fl * 1.1f) * tg * txSpaceEff * arrRoom);
            txSend.addSample (1, i, (nr * 0.2f + fr * 1.1f) * tg * txSpaceEff * arrRoom);
            dronesOut.addSample (0, i, dOutL); dronesOut.addSample (1, i, dOutR);
            drSend.addSample (0, i, (ys[0] * dg * 0.6f + b0 * 0.35f) * drSpaceEff * arrRoom);
            drSend.addSample (1, i, (ys[1] * dg * 0.6f + b1 * 0.35f) * drSpaceEff * arrRoom);
        }
        rateAcc += dt;
    }

    // Tape: each module has its own, so texture and drones never wobble in step.
    runTape (tapes[0], textureOut, txSend, numSamples, cur (txTape), 0);
    runTape (tapes[1], dronesOut, drSend, numSamples, cur (drTape), 1);

    // Then they come together, through the drive.
    {
        const float driveT = 1.0f + 11.0f * driveEff * driveEff, bias = 0.08f * driveEff;
        const float tb = std::tanh (bias), norm = std::pow (driveT, 0.72f) * (1.0f - tb * tb);
        const float echoSend = cur (spEcho) * 1.2f;
        for (int c = 0; c < 2; ++c)
        {
            const float* t0 = textureOut.getReadPointer (c);
            const float* d0 = dronesOut.getReadPointer (c);
            const float* t1 = txSend.getReadPointer (c);
            const float* d1 = drSend.getReadPointer (c);
            float* dst[2] = { mainBus.getWritePointer (c), sendBus.getWritePointer (c) };
            float* e = echoIn.getWritePointer (c);
            for (int s = 0; s < numSamples; ++s)
            {
                e[s] = t0[s] * echoSend;
                const float in[2] = { t0[s] + d0[s], t1[s] + d1[s] };
                for (int b = 0; b < 2; ++b)
                {
                    const float v = (std::tanh (driveT * in[b] + bias) - tb) / norm;
                    const float dc = v - driveDcX[(size_t) b][(size_t) c] + 0.9995f * driveDcY[(size_t) b][(size_t) c];
                    driveDcX[(size_t) b][(size_t) c] = v;
                    driveDcY[(size_t) b][(size_t) c] = dc;
                    dst[b][s] = dc;
                }
            }
        }
    }

    // Tape echo
    {
        const double target = juce::jlimit (0.02, 2.9, 0.75 * 60.0 / bpm) * sr;
        const double smooth = 1.0 - std::exp (-1.0 / (0.25 * sr));
        const float aLp = 1.0f - std::exp (-twoPi * 2600.0f / (float) sr), aHp = 1.0f - std::exp (-twoPi * 140.0f / (float) sr);
        const float fb = 0.45f + 0.15f * swell;
        const int bufLen = echoBuf.getNumSamples();
        float* bufs[2] = { echoBuf.getWritePointer (0), echoBuf.getWritePointer (1) };
        for (int s = 0; s < numSamples; ++s)
        {
            echoDelaySm += (target - echoDelaySm) * smooth;
            echoLfo += 0.33 / sr;
            if (echoLfo >= 1.0) echoLfo -= 1.0;
            const double d = juce::jlimit (2.0, (double) bufLen - 3.0, echoDelaySm + 0.003 * sr * std::sin (juce::MathConstants<double>::twoPi * echoLfo));
            double rp = echoWrite - d;
            if (rp < 0) rp += bufLen;
            const int i0 = juce::jlimit (0, bufLen - 1, (int) rp), i1 = (i0 + 1) % bufLen;
            const float fr = (float) (rp - i0);
            for (int c = 0; c < 2; ++c)
            {
                const float y = bufs[c][i0] + (bufs[c][i1] - bufs[c][i0]) * fr;
                echoLp[(size_t) c] += aLp * (y - echoLp[(size_t) c]);
                echoHpLow[(size_t) c] += aHp * (echoLp[(size_t) c] - echoHpLow[(size_t) c]);
                bufs[c][echoWrite] = echoIn.getSample (c, s) + (echoLp[(size_t) c] - echoHpLow[(size_t) c]) * fb;
                if (inside)
                {
                    mainBus.addSample (c, s, y * 0.8f);
                    sendBus.addSample (c, s, y * 0.3f);
                }
            }
            if (++echoWrite >= bufLen) echoWrite = 0;
        }
    }

    // The Send output carries what the built-in space would get
    if (out.send[0] != nullptr && out.send[1] != nullptr)
        for (int c = 0; c < 2; ++c)
        {
            juce::FloatVectorOperations::copy (out.send[c], sendBus.getReadPointer (c), numSamples);
            juce::FloatVectorOperations::add (out.send[c], echoIn.getReadPointer (c), numSamples);
            for (int i = 0; i < numSamples; ++i) out.send[c][i] *= fade0 + (fade1 - fade0) * (float) i / (float) numSamples;
        }

    // Space: a room and a vast space, blended by Size
    if (inside)
    {
        const float size = cur (spSize);
        roomGain.setTargetValue (1.1f * std::cos (size * pi * 0.5f));
        vastGain.setTargetValue (1.2f * std::sin (size * pi * 0.5f));
        for (int c = 0; c < 2; ++c)
        {
            revRoom.copyFrom (c, 0, sendBus, c, 0, numSamples);
            revVast.copyFrom (c, 0, sendBus, c, 0, numSamples);
        }
        juce::dsp::AudioBlock<float> rb (revRoom), vb (revVast);
        auto r1 = rb.getSubBlock (0, (size_t) numSamples), v1 = vb.getSubBlock (0, (size_t) numSamples);
        roomVerb.process (juce::dsp::ProcessContextReplacing<float> (r1));
        vastVerb.process (juce::dsp::ProcessContextReplacing<float> (v1));
        for (int s = 0; s < numSamples; ++s)
        {
            const float rg = roomGain.getNextValue(), vg = vastGain.getNextValue();
            for (int c = 0; c < 2; ++c)
                mainBus.addSample (c, s, revRoom.getSample (c, s) * rg + revVast.getSample (c, s) * vg);
        }
    }

    // Master
    const float vol = rawVolume->load();
    masterGain.setTargetValue (vol * vol * 1.4f * arrOpen);
    float pow2 = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        const float g = masterGain.getNextValue() * (fade0 + (fade1 - fade0) * (float) s / (float) numSamples);
        out.main[0][s] = mainBus.getSample (0, s) * g;
        out.main[1][s] = mainBus.getSample (1, s) * g;
        if (out.texture[0] != nullptr && out.texture[1] != nullptr)
        {
            out.texture[0][s] = std::tanh (textureOut.getSample (0, s) * g);
            out.texture[1][s] = std::tanh (textureOut.getSample (1, s) * g);
        }
        if (out.drones[0] != nullptr && out.drones[1] != nullptr)
        {
            out.drones[0][s] = std::tanh (dronesOut.getSample (0, s) * g);
            out.drones[1][s] = std::tanh (dronesOut.getSample (1, s) * g);
        }
    }
    {
        float* chans[2] = { out.main[0], out.main[1] };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        glue.process (ctx);
        limiter.process (ctx);
    }
    for (int s = 0; s < numSamples; ++s) pow2 += out.main[0][s] * out.main[0][s] + out.main[1][s] * out.main[1][s];
    levelMeter += 0.2f * (std::sqrt (pow2 / (2.0f * (float) juce::jmax (1, numSamples))) - levelMeter);

    // Keep the last few seconds for Lock
    {
        const int ringLen = captureRing.getNumSamples();
        for (int s = 0; s < numSamples; ++s)
        {
            captureRing.setSample (0, captureWrite, out.main[0][s]);
            captureRing.setSample (1, captureWrite, out.main[1][s]);
            if (++captureWrite >= ringLen) captureWrite = 0;
        }
        if (captureRequested.load() && ! captureReady.load())
        {
            const int tail = ringLen - captureWrite;
            for (int c = 0; c < 2; ++c)
            {
                captureOut.copyFrom (c, 0, captureRing, c, captureWrite, tail);
                if (captureWrite > 0) captureOut.copyFrom (c, tail, captureRing, c, 0, captureWrite);
            }
            captureRequested.store (false);
            captureReady.store (true);
        }
    }

    // Grains per second, measured over two seconds
    rateWindow += rateAcc;
    if (rateWindow >= 2.0)
    {
        rateEst = (float) eventsThisWindow / (float) rateWindow;
        eventsThisWindow = 0;
        rateWindow = 0.0;
    }
    publishDisplay();
}

void Engine::publishDisplay()
{
    for (int p = 0; p < numP; ++p) dispNormPub[(size_t) p].store (curNorm[(size_t) p], std::memory_order_relaxed);
    auto pubVal = [this] (int p, float value) {
        const auto& r = ranges[(size_t) p];
        dispNormPub[(size_t) p].store (r.convertTo0to1 (juce::jlimit (r.start, r.end, value)), std::memory_order_relaxed);
    };
    pubVal (txDensity, densityEff);
    pubVal (drBody, bodyEff);
    pubVal (drSub, subEff);
    pubVal (outDrive, driveEff);
    pubVal (txSpace, txSpaceEff);
    pubVal (drSpace, drSpaceEff);
    curPartPub.store (curPart, std::memory_order_relaxed);
    partElapsedPub.store (partElapsed, std::memory_order_relaxed);
    pubVal (txSpread, spreadEff);
    massPub.store (mass, std::memory_order_relaxed);
    ratePub.store (rateEst, std::memory_order_relaxed);
    levelPub.store (levelMeter, std::memory_order_relaxed);
    runSecondsPub.store (runSeconds, std::memory_order_relaxed);
    grainsPub.store ((int) activeList.size(), std::memory_order_relaxed);
    glidingPub.store (morphing, std::memory_order_relaxed);
    blendPub.store (morphing ? (float) morphPos : -1.0f, std::memory_order_relaxed);
}
} // namespace gf
