#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "SourceLibrary.h"

namespace gf
{
// What the texture screen draws. Clouds are the texture's own grains; the rest are Detail.
enum EventType { evCloud, evSpeck, evBurst, evGlitch, evDust, numEventTypes };

// x: where in its sound the grain starts (0..1), span: how much of the sound it reads, slot: which texture sound (-1: none).
struct VisEvent { int type = 0; float x = 0, semis = 0, dur = 0, near = 1, pan = 0, midi = -1; bool rev = false; int slot = -1; float span = 0; };

// A part as the audio thread sees it.
struct SceneData
{
    bool used = false;
    std::array<float, numP> norm {};
    std::array<SourceData*, numSlots> src {};
    Progression prog;
};

// Output pairs the engine can fill. Main is always present; the others only when the host enables them.
struct Outputs
{
    float* main[2] = { nullptr, nullptr };
    float* texture[2] = { nullptr, nullptr };
    float* drones[2] = { nullptr, nullptr };
    float* send[2] = { nullptr, nullptr };
    const float* input[2] = { nullptr, nullptr };   // the side chain, if anything is sent in
};

// A zero-delay-feedback state-variable filter (band-pass and low-pass).
struct Svf
{
    void set (float hz, float q, double sr)
    {
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.0f, (float) sr * 0.45f, hz) / (float) sr);
        k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (float v0, float& bp, float& lp)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        bp = v1 * k; lp = v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
};

// The sound engine. process() runs on the audio thread and never allocates or locks.
class Engine
{
public:
    explicit Engine (juce::AudioProcessorValueTreeState& state);

    void prepare (double sampleRate, int maxBlockSize);
    void process (const Outputs& out, int numSamples, juce::AudioPlayHead* playHead);

    // Message thread
    void setSlotSource (int slot, SourceData* s) { slotSrc[(size_t) slot].store (s); }
    void setScene (int index, const SceneData& scene);
    void setProgression (const Progression& p, int playingNowAt = -1);
    void requestRecall (int index) { manualRecall.store (index); }
    // The arrangement: how long each part lasts and how long the piece takes to blend into it.
    void setPartInfo (int index, bool used, float lengthSeconds, float blendInSeconds)
    {
        partUsedA[(size_t) index].store (used);
        partLenA[(size_t) index].store (lengthSeconds);
        partBlendA[(size_t) index].store (blendInSeconds);
    }
    void setActivePart (int index) { adoptPart.store (index); }   // a new part that starts as what is playing: no recall needed
    void requestRewind() { rewindReq.store (true); }              // back to the first part, clock at zero
    int getCurPart() const { return curPartPub.load (std::memory_order_relaxed); }
    double getPartElapsed() const { return partElapsedPub.load (std::memory_order_relaxed); }
    void adoptSceneParam() { adoptScene.store (true); }
    void requestCapture() { captureRequested.store (true); }
    bool fetchCapture (juce::AudioBuffer<float>& dest, double& sampleRateOut);
    int popEvents (VisEvent* dest, int maxEvents);

    // Published state, safe to read from any thread
    SourceData* getPlayingSource (int slot) const { return playingSrc[(size_t) slot].load(); }
    float getCurrentNorm (int p) const { return curNormPub[(size_t) p].load (std::memory_order_relaxed); }
    float getDisplayNorm (int p) const { return dispNormPub[(size_t) p].load (std::memory_order_relaxed); }
    float getMass() const { return massPub.load (std::memory_order_relaxed); }
    // Each module's tape: where its wobble is right now (-1..1, scaled by the amount) and how deep a dropout is.
    float getTapeWow (int stage) const { return tapeWowPub[(size_t) stage].load (std::memory_order_relaxed); }
    float getTapeDrop (int stage) const { return tapeDropPub[(size_t) stage].load (std::memory_order_relaxed); }
    // Where each sound is being read (0..1): the three texture sounds, the drones' sound, the bed.
    float getReadPos (int slot) const { return readPosPub[(size_t) slot].load (std::memory_order_relaxed); }
    float getEventRate() const { return ratePub.load (std::memory_order_relaxed); }
    float getVoiceAmp (int v) const { return voiceAmpPub[(size_t) v].load (std::memory_order_relaxed); }
    float getVoiceHz (int v) const { return voiceHzPub[(size_t) v].load (std::memory_order_relaxed); }
    float getVoiceTargetHz (int v) const { return voiceTargetPub[(size_t) v].load (std::memory_order_relaxed); } // the note it is heading for
    // While one note melts into the next: the note it is leaving, and how far the change has got (1: done).
    float getVoiceOldHz (int v) const { return voiceOldPub[(size_t) v].load (std::memory_order_relaxed); }
    float getVoiceXfade (int v) const { return voiceXPub[(size_t) v].load (std::memory_order_relaxed); }
    float getLevel() const { return levelPub.load (std::memory_order_relaxed); }
    int getKey() const { return keyPub.load (std::memory_order_relaxed); }
    int getStepIndex() const { return stepPub.load (std::memory_order_relaxed); }
    float getStepProgress() const { return stepProgPub.load (std::memory_order_relaxed); }
    // Go to this chord of the progression at the next bar (then carry on from there, unless held).
    // when: 0 when the playing chord's bars are done, 1 at the next bar line, 2 right away.
    void cueStep (int index, int when = 0) { cueWhen.store (when); cue.store (index); }
    int getCueWhen() const { return cueWhen.load(); }
    // A MIDI keyboard plays the harmony: the note or chord you play becomes the chord, and stays until the
    // progression moves on, you pick a slot, or you let it go.
    void midiNote (int note, bool on);                         // audio thread
    void clearKeysChord() { keysClear.store (true); }
    // -1: none. Otherwise the root (0..11), the kind of chord and the octave: root | kind << 4 | (octave + 2) << 8.
    int getKeysChord() const { return keysPub.load (std::memory_order_relaxed); }
    int getCue() const { return cue.load(); }
    double getRunSeconds() const { return runSecondsPub.load (std::memory_order_relaxed); }
    float getBlendProgress() const { return blendPub.load (std::memory_order_relaxed); } // -1 when not blending
    bool isRunning() const { return runningPub.load (std::memory_order_relaxed); }
    bool isGliding() const { return glidingPub.load (std::memory_order_relaxed); }
    bool hostHasTempo() const { return hostTempoPub.load (std::memory_order_relaxed); }
    float getBpm() const { return bpmPub.load (std::memory_order_relaxed); }
    int getRecallCount() const { return recallCount.load(); }
    int getLastRecalled() const { return lastRecalled.load(); }
    int getActiveGrains() const { return grainsPub.load (std::memory_order_relaxed); }
    // The side chain: where it's written, and how loud it is.
    void setLiveSource (SourceData* s) { liveSrc = s; liveWrite = 0; liveFilled = 0; }
    float getInputLevel() const { return inputPub.load (std::memory_order_relaxed); }

    // Listening to one part at a time: 0 texture, 1 drones, 2 the bed. Not saved, not automated.
    enum Stage { stTexture, stDrones, stBed, numStages };
    void setSolo (int stage) { soloStage.store (stage); }          // -1: none
    int getSolo() const { return soloStage.load(); }
    void setMuted (int stage, bool m) { stageMuted[(size_t) stage].store (m); }
    bool isMuted (int stage) const { return stageMuted[(size_t) stage].load(); }
    bool isAudible (int stage) const { const int s = soloStage.load(); return s >= 0 ? s == stage : ! stageMuted[(size_t) stage].load(); }

    static constexpr int maxGrains = 448;
    static constexpr int controlBlock = 32;
    static constexpr int numVoices = 9;      // 3 drone, 5 pad, 1 line. The sub is published as voice 9.
    static constexpr int lineVoice = 8, subVoice = 9;
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, hop = fftSize / 4;
    static constexpr double captureSeconds = 8.0, sedimentSeconds = 6.0;

private:
    enum Env { envHann, envPerc, envGate, envSwell, envDust };
    enum Dest { toTexture, toBed };

    struct Grain
    {
        const SourceData* src = nullptr;
        double pos = 0.0, inc = 1.0, incMul = 1.0;
        int len = 0, age = 0, delay = 0;
        int env = envHann, dest = toTexture;
        float amp = 0.0f, nl = 0, nr = 0, fl = 0, fr = 0;
        int crush = 0, crushCount = 0;
        float held0 = 0, held1 = 0, bits = 0;
        juce::uint32 seed = 1;
        float dustLp = 0, dustPrev = 0;
        bool raw = false;   // a sound left at its own pitch: it skips the texture's tuned resonators
    };

    struct Comb
    {
        std::vector<float> buf;
        int w = 0;
        float lp = 0.0f, delay = 100.0f;
    };

    struct Voice
    {
        std::array<Comb, 2> comb, old;   // the note sounding, and the one it is taking over from
        float amp = 0.0f, targetAmp = 0.0f, hz = 110.0f, targetHz = 110.0f, midi = 45.0f;
        // A change of note is a true crossfade: the old note keeps ringing while the new one comes up beside it.
        float oldHz = 110.0f, xfade = 1.0f, wait = 0.0f, gNew = 1.0f, gOld = 0.0f;
        float lfoA = 0, lfoB = 0, lfoT = 1, lfoRate = 0.05f, lfo = 0;
    };

    // One module's tape: its own transport, so texture and drones drift against each other.
    struct Tape
    {
        std::array<std::array<std::vector<float>, 2>, 2> buf;   // the module's sound, and what it sends to the room
        int write = 0;
        float wowA = 0, wowB = 0, wowT = 1, wowRate = 0.6f, flutterPh = 0;
        std::array<std::array<float, 2>, 2> lp {}, dcX {}, dcY {};
        float dropEnv = 0.0f, dropTarget = 0.0f, hissPrev = 0.0f, wowNow = 0.0f;
        int dropHold = 0;
    };

    // Parameters
    float cur (int p) const { return curVal[(size_t) p]; }
    int curInt (int p) const { return juce::roundToInt (curVal[(size_t) p]); }
    float rawNorm (int p) const;
    void updateControl (int numSamples);
    void updateSwell();
    void startRecall (int index, float blendSeconds);   // below 0: the blend time set now
    void publishDisplay();

    // Texture
    void scheduleTexture (int numSamples, float dt);
    void spawnCloud (int delay);
    void spawnDetail (int delay);
    void spawnGrain (int delay, const SourceData* src, float posNorm, float durSec, float semis, int env, float amp,
                     bool rev, float pan, float nearFar, int dest, int visType, double incMul = 1.0, int crush = 0, float bits = 0);
    // The nearest note of the current chord, or of the chord moved up by pcOff semitones (a layer's own note).
    float snapToChord (float midi, int pcOff = 0) const;
    float snapInterval (float semis, int pcOff = 0) const;
    NoteStep slotStep (int k) const;       // where texture sound k plays: the texture's step plus its own
    void renderGrains (int numSamples);

    // Harmony and drones
    void updateHarmony (float dt, bool hostPlaying, bool havePpq, double ppq, double barLength);
    void setChord (int rootPc, int type, int oct, bool jump);
    void applyKeys();
    void retuneBank();
    void crossfadeBank();                  // the texture's resonators move to new notes, as a crossfade
    void retargetVoices (bool jump);       // every drone voice heads for its note (the chord's, moved by the drones' note)
    float voiceHz (float midi) const;      // a voice's pitch, moved to where the drones play
    void setCombDelays (Voice& V, std::array<Comb, 2>& combs, float hz, float cents, float aV);
    int noteIndexAt (const SourceData* s, int samplePos) const;
    float fitInNote (const SourceData* s, int noteIndex, double samplePos, float semis, float& lenSeconds) const;
    void runTape (Tape& t, juce::AudioBuffer<float>& a, juce::AudioBuffer<float>& b, int numSamples, float amount, int stage);
    void moveLine (bool bigStep);
    void scheduleBed (int numSamples);
    void makeSmearFrame();
    float rnd() { return rng.nextFloat() * 2.0f - 1.0f; }
    float gauss();

    void makeImpulseResponses();

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::atomic<float>*, numP> raw {};
    std::array<juce::NormalisableRange<float>, numP> ranges;
    std::atomic<float>* rawVolume = nullptr;
    std::atomic<float>* rawPart = nullptr;
    std::atomic<float>* rawBlend = nullptr;
    std::atomic<float>* rawPlay = nullptr;
    std::atomic<float>* rawSync = nullptr;
    std::atomic<float>* rawSpaceMode = nullptr;
    std::atomic<float>* rawFreeze = nullptr;
    std::atomic<float>* rawHold = nullptr;
    std::atomic<float>* rawTempo = nullptr;
    std::atomic<float>* rawArrive = nullptr;
    std::atomic<float>* rawWear = nullptr;

    // Wear: stretches of worn sounds that have just been read, waiting to be eroded a little more.
    struct WearSpan { SourceData* s = nullptr; int start = 0, pos = 0, end = 0, dropA = 0, dropB = 0; float dropDepth = 0.0f; };
    static constexpr int maxWearSpans = 64;
    std::array<WearSpan, maxWearSpans> wearSpans {};
    int wearHead = 0, wearCount = 0, smearWearTick = 0;
    std::array<float, 2> wearLp {}, wearHiss {};
    void queueWear (const SourceData* s, double from, double to);
    void runWear (int budget);

    // Voices made of the sound itself: its spectrum, held, laid on every note of the chord.
    std::vector<float> envBuf, fineBuf, prefix, vOlaL, vOlaR;
    std::array<std::vector<float>, 2> vMag;
    float smearRefMidi = -1.0f, voiceMixNow = 0.7f;
    static constexpr float spectralVoiceGain = 0.2f;   // set so the voices match the resonators in level
    void makeVoiceFrame();
    void addVoiceSpectrum (std::vector<float>& mag, float f0, float w, bool pitched, float refHz);
    std::atomic<int> cue { -1 }, cueWhen { 0 };
    std::array<bool, 128> keysHeld {}, keysRecent {};   // down now, and pressed since the last chord was read
    int keysSettle = -1;                                // samples until what was played is read as one chord
    bool keysOn = false;
    int keysRoot = 0, keysType = 1, keysOct = 0;
    std::atomic<bool> keysClear { false };
    std::atomic<int> keysPub { -1 };
    int liveRemap = -1;   // the playing chord's new place after you reorder the slots
    double stepStart = 0.0, lastBarPos = 0.0;

    std::array<float, numP> curVal {}, curNorm {}, morphFrom {}, overrideVal {}, rawAtRecall {};
    std::array<bool, numP> overrideOn {};
    std::array<std::atomic<float>, numP> curNormPub, dispNormPub;
    bool morphing = false;
    double morphPos = 0.0, morphSeconds = 0.0;

    // What Swell has made of the controls it moves
    float swell = 0.35f, densityEff = 1.0f, bodyEff = 0.5f, subEff = 0.3f, driveEff = 0.2f, txSpaceEff = 0.5f, drSpaceEff = 0.5f, spreadEff = 0.7f;
    bool frozen = false;

    // Arrive: how far the piece has come in from silence. The layers come in one after another:
    // the bed, then the texture (a few grains first), then the drones, the sub, and last the room.
    float arrive = 1.0f, arrOpen = 1.0f, arrBed = 1.0f, arrTexture = 1.0f, arrGrains = 1.0f, arrDrones = 1.0f, arrSub = 1.0f, arrRoom = 1.0f;
    void updateArrive (float dt);

    double sr = 44100.0;
    int maxBlock = 512;
    juce::Random rng { 1234 };
    double bpm = 90.0;
    bool running = false, wasRunning = false;
    double runSeconds = 0.0;
    float mass = 0.0f;

    // Sound slots
    std::array<std::atomic<SourceData*>, numSlots> slotSrc {}, playingSrc {};
    std::array<SourceData*, numSlots> slotOverride {}, slotAtRecall {}, src {};

    // Texture
    std::vector<Grain> grains;
    std::vector<int> freeList, activeList;
    double nextCloudIn = 0.0, nextDetailIn = 0.0;
    // Each cloud reads around the place you set, with a slow wander of its own.
    std::array<double, 3> wanderPh { 0.0, 0.33, 0.71 };
    std::array<float, 3> cloudAt { 0.25f, 0.5f, 0.75f };
    bool pretuned = false;
    float visMidi = -1.0f;   // the note a cloud grain will sound at, for the picture
    int visSlot = -1;
    std::atomic<int> soloStage { -1 };
    std::array<std::atomic<bool>, 3> stageMuted {};
    float bedOn = 1.0f;
    float subOldHz = 36.7f, subX = 1.0f;
    double subOldPhase = 0.0;
    int eventsThisWindow = 0;
    double rateWindow = 0.0;
    float rateEst = 0.0f;
    std::array<std::array<Svf, 6>, 4> tuned, tunedOld;   // near L, near R, far L, far R; the old set rings out as a chord changes
    float tunedX = 1.0f;
    bool tunedPending = false;
    std::array<Svf, 4> farTone;
    std::array<Svf, 2> farToneRaw;

    // Notes in layers. The texture's step moves its resonators and all its sounds; each sound adds its own.
    // The drones' step moves every drone voice; the bed's moves the bed. A grain about to be made carries its sound's step.
    int txIdx = -1, drIdx = -1, drShift = 0;
    NoteStep txStep, bedStep, drStep;
    int grainPc = 0;
    bool grainOwn = false;

    // Harmony
    juce::SpinLock progLock;
    Progression liveProg, prog, progAtRecall, progScene;
    bool progOverride = false;
    double barPos = 0.0, internalBars = 0.0;
    int stepIndex = -1, rootPc = 2, chordType = 1, chordOct = 0, key = 2, lastKey = -1, lineBar = -1;
    std::array<int, 7> chordPcs {};
    int numChordPcs = 0;

    // Drones
    std::array<Voice, numVoices> voices;
    double subPhase = 0.0;
    float subAmp = 0.0f, subHz = 36.7f, subTargetHz = 36.7f;
    float subMidi = 26.0f;   // the sub's note for this chord, before the drones' own note moves it
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> fftBuf, window, frame, sedFrame, olaL, olaR;
    std::array<float, 4096> sinTab {}, cosTab {};
    int olaPos = 0, hopCount = 0;
    double smearPos = 0.0;
    std::vector<float> sediment;
    int sedWrite = 0;
    float sedPower = 0.0f;
    std::array<Svf, 2> droneTone, bedTone;
    std::array<float, 2> droneDcX {}, droneDcY {};
    double nextBedIn = 0.0;
    float bedDrift = 0.0f;

    // Buses for one control block
    juce::AudioBuffer<float> txNear, txFar, rawNear, rawFar, bed, mainBus, sendBus, echoIn, textureOut, dronesOut, txSend, drSend, revRoom, revVast;

    // Echo
    juce::AudioBuffer<float> echoBuf;
    int echoWrite = 0;
    double echoDelaySm = 0.0, echoLfo = 0.0;
    std::array<float, 2> echoLp {}, echoHpLow {};

    // Reverb, dynamics
    juce::dsp::Convolution roomVerb { juce::dsp::Convolution::NonUniform { 256 } };
    juce::dsp::Convolution vastVerb { juce::dsp::Convolution::NonUniform { 512 } };
    juce::SmoothedValue<float> roomGain, vastGain, masterGain, textureGain, dronesGain;
    juce::dsp::Compressor<float> glue;
    juce::dsp::Limiter<float> limiter;

    // Tape: one for the texture, one for the drones. Drive sits after them, on the whole mix.
    std::array<Tape, 2> tapes;
    std::array<std::array<float, 2>, 2> driveDcX {}, driveDcY {};

    std::array<float, 2048> hannTable {}, percTable {}, swellTable {};

    // Parts
    juce::SpinLock sceneLock;
    std::array<SceneData, numScenes> scenes;
    std::atomic<int> manualRecall { -1 };
    std::atomic<bool> adoptScene { false };
    int pendingRecall = -1, lastSceneParam = -1;
    // The arrangement, as the audio thread needs it
    std::array<std::atomic<bool>, numScenes> partUsedA {};
    std::array<std::atomic<float>, numScenes> partLenA {}, partBlendA {};
    std::atomic<int> adoptPart { -1 }, curPartPub { 0 };
    std::atomic<bool> rewindReq { false };
    std::atomic<double> partElapsedPub { 0.0 };
    int curPart = 0;
    double partElapsed = 0.0;
    bool hostWasPlaying = false;
    std::atomic<int> recallCount { 0 }, lastRecalled { -1 };

    // Capture
    juce::AudioBuffer<float> captureRing, captureOut;
    int captureWrite = 0;
    std::atomic<bool> captureRequested { false }, captureReady { false };

    // Events for the display
    static constexpr int fifoSize = 2048;
    juce::AbstractFifo eventFifo { fifoSize };
    std::array<VisEvent, fifoSize> events;
    float levelMeter = 0.0f;
    float playFade = 0.0f;
    SourceData* liveSrc = nullptr;
    int liveWrite = 0, liveFilled = 0;
    float inputLevel = 0.0f;
    std::atomic<float> inputPub { 0.0f };
    // Where a grain of length len can safely start in the live ring: behind what's being written.
    float livePos (float& len);   // stop fades everything out, play fades back in

    std::atomic<float> massPub { 0 }, ratePub { 0 }, stepProgPub { 0 }, blendPub { -1 }, levelPub { 0 };
    std::array<std::atomic<float>, 2> tapeWowPub {}, tapeDropPub {};
    std::array<std::atomic<float>, numSlots> readPosPub {};
    std::atomic<double> runSecondsPub { 0 };
    std::atomic<float> bpmPub { 80.0f };
    std::array<std::atomic<float>, numVoices + 1> voiceAmpPub {}, voiceHzPub {}, voiceTargetPub {}, voiceOldPub {}, voiceXPub {};
    std::atomic<int> keyPub { 2 }, grainsPub { 0 }, stepPub { 0 };
    std::atomic<bool> runningPub { false }, glidingPub { false }, hostTempoPub { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};
} // namespace gf
