#include "SourceLibrary.h"
#include <juce_dsp/juce_dsp.h>
#include "SoundData.h"

namespace gf
{
namespace
{
void normalise (juce::AudioBuffer<float>& b, float peak)
{
    float m = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        m = juce::jmax (m, b.findMinMax (c, 0, b.getNumSamples()).getEnd(), -b.findMinMax (c, 0, b.getNumSamples()).getStart());
    if (m > 0.0f)
        b.applyGain (peak / m);
}

struct BuiltinEntry
{
    const char* id;
    const char* name;
    BuiltinKind kind;
    const char* data;
    int size;
    std::vector<NoteSpan> notes;
};

const std::vector<BuiltinEntry>& table()
{
    static const std::vector<BuiltinEntry> t = {
#include "BuiltinSounds.inc"
    };
    return t;
}

bool isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf");
}
} // namespace

// Built-in sounds are decoded once and shared by every open copy of the plugin.
struct SourceLibrary::BuiltinCache
{
    SourceData::Ptr get (const juce::String& id)
    {
        const juce::ScopedLock sl (lock);
        for (auto& s : decoded)
            if (s->id == id) return s;
        for (auto& e : table())
        {
            if (id != e.id) continue;
            juce::OggVorbisAudioFormat ogg;
            std::unique_ptr<juce::AudioFormatReader> reader (ogg.createReaderFor (new juce::MemoryInputStream (e.data, (size_t) e.size, false), true));
            if (reader == nullptr) return nullptr;
            SourceData::Ptr s = new SourceData();
            s->id = e.id;
            s->name = e.name;
            s->sampleRate = reader->sampleRate;
            s->audio.setSize ((int) juce::jmin (2u, reader->numChannels), (int) reader->lengthInSamples);
            reader->read (&s->audio, 0, (int) reader->lengthInSamples, 0, true, true);
            s->notes = e.notes;
            SourceLibrary::computePeaks (*s);
            decoded.add (s);
            return s;
        }
        return nullptr;
    }

    juce::CriticalSection lock;
    juce::Array<SourceData::Ptr> decoded;
};

SourceLibrary::SourceLibrary()
{
    formats.registerBasicFormats();
    liveSrc = new SourceData();
    liveSrc->id = "live";
    liveSrc->name = "the track sent in";
    liveSrc->live = true;
    prepareLive (48000.0);
}

void SourceLibrary::prepareLive (double sampleRate)
{
    if (liveSrc->audio.getNumSamples() == (int) (sampleRate * 30.0) && std::abs (liveSrc->sampleRate - sampleRate) < 1.0) return;
    liveSrc->sampleRate = sampleRate;
    liveSrc->audio.setSize (2, (int) (sampleRate * 30.0));
    liveSrc->audio.clear();
}

std::vector<BuiltinInfo> SourceLibrary::builtins()
{
    std::vector<BuiltinInfo> out;
    for (auto& e : table()) out.push_back ({ e.id, e.name, e.kind });
    return out;
}

juce::File SourceLibrary::getSoundsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Grain Field").getChildFile ("Sounds");
}

juce::Array<juce::File> SourceLibrary::folderSounds()
{
    juce::Array<juce::File> out;
    const auto folder = getSoundsFolder();
    if (! folder.isDirectory()) return out;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
    {
        if (isAudioFile (entry.getFile())) out.add (entry.getFile());
        if (out.size() >= 200) break;
    }
    struct ByName { static int compareElements (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()); } };
    ByName sorter;
    out.sort (sorter);
    return out;
}

SourceData::Ptr SourceLibrary::add (SourceData::Ptr s)
{
    computePeaks (*s);
    const juce::ScopedLock sl (lock);
    sources.add (s);
    return s;
}

SourceData::Ptr SourceLibrary::find (const juce::String& id)
{
    if (id == "live") return liveSrc;
    {
        const juce::ScopedLock sl (lock);
        for (auto& s : sources)
            if (s->id == id) return s;
    }
    return cache->get (id);
}

juce::Array<SourceData::Ptr> SourceLibrary::getLoaded() const
{
    const juce::ScopedLock sl (lock);
    return sources;
}

void SourceLibrary::computePeaks (SourceData& s)
{
    const int bins = 512, n = s.audio.getNumSamples();
    s.peaks.assign (bins, 0.0f);
    if (n == 0) return;
    for (int c = 0; c < s.audio.getNumChannels(); ++c)
    {
        const float* d = s.audio.getReadPointer (c);
        for (int b = 0; b < bins; ++b)
        {
            const int i0 = (int) ((juce::int64) b * n / bins), i1 = (int) ((juce::int64) (b + 1) * n / bins);
            float m = s.peaks[(size_t) b];
            for (int i = i0; i < i1; i += 2) m = juce::jmax (m, std::abs (d[i]));
            s.peaks[(size_t) b] = m;
        }
    }
}

SourceData::Ptr SourceLibrary::loadFile (const juce::File& file, juce::String& error)
{
    const juce::String id = "file:" + file.getFullPathName();
    if (auto existing = find (id)) return existing;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Couldn't read " + file.getFileName() + ". Try a WAV, AIFF, FLAC or MP3 file.";
        return nullptr;
    }
    const auto maxLen = (juce::int64) (reader->sampleRate * 120.0); // first two minutes
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    const int chans = (int) juce::jmin ((juce::uint32) 2, reader->numChannels);
    if (len <= 0 || chans <= 0) { error = file.getFileName() + " has no audio."; return nullptr; }

    SourceData::Ptr s = new SourceData();
    s->id = id;
    s->name = file.getFileNameWithoutExtension().substring (0, 28);
    s->sampleRate = reader->sampleRate;
    s->audio.setSize (chans, len);
    reader->read (&s->audio, 0, len, 0, true, chans > 1);
    findCoreNote (*s);
    return add (s);
}

juce::File SourceLibrary::getCaptureFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Grain Field").getChildFile ("Captures");
}

SourceData::Ptr SourceLibrary::addCapture (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
{
    juce::AudioBuffer<float> copy (audio);
    normalise (copy, 0.9f);

    auto folder = getCaptureFolder();
    folder.createDirectory();
    auto file = folder.getNonexistentChildFile ("Capture " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S"), ".wav", false);

    SourceData::Ptr s = new SourceData();
    s->name = name.isNotEmpty() ? name : "capture " + juce::Time::getCurrentTime().formatted ("%H:%M:%S");
    s->sampleRate = sampleRate;
    s->audio = std::move (copy);

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    bool saved = false;
    if (stream != nullptr)
    {
        auto opts = juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (s->audio.getNumChannels()).withBitsPerSample (24);
        if (auto writer = wav.createWriterFor (stream, opts))
            saved = writer->writeFromAudioSampleBuffer (s->audio, 0, s->audio.getNumSamples());
    }
    s->id = saved ? "file:" + file.getFullPathName() : "capture:" + juce::Uuid().toString();
    findCoreNote (*s);
    return add (s);
}
void SourceLibrary::findCoreNote (SourceData& s)
{
    // Average the spectrum over the first minute: noise smooths out into a gentle slope, while a pitch
    // that keeps coming back (a hum, wind in wires, a voice) stands up out of it as a peak.
    // The pitch class with the most peak energy is the core; its strongest octave is the core note.
    const int n = s.audio.getNumSamples();
    if (n < 4096) return;
    constexpr int order = 13, size = 1 << order, hop = size / 2;
    juce::dsp::FFT fft (order);
    std::vector<float> window ((size_t) size), buf ((size_t) size * 2);
    std::vector<double> avg ((size_t) size / 2, 0.0);
    for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);

    const int limit = juce::jmin (n, (int) (s.sampleRate * 60.0));
    const int chans = s.audio.getNumChannels();
    int frames = 0;
    for (int start = 0; start < juce::jmax (1, limit - size + 1); start += hop)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < size && start + i < n; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < chans; ++c) v += s.audio.getSample (c, start + i);
            buf[(size_t) i] = v * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 0; k < size / 2; ++k) avg[(size_t) k] += buf[(size_t) k];
        ++frames;
    }
    if (frames == 0) return;

    const double binHz = s.sampleRate / size;
    const int lo = juce::jmax (8, (int) (40.0 / binHz)), hi = juce::jmin (size / 2 - 40, (int) (2000.0 / binHz));
    std::array<double, 128> byMidi {};
    double total = 0.0;
    for (int k = lo; k <= hi; ++k)
    {
        const double m = avg[(size_t) k];
        if (m <= avg[(size_t) k - 1] || m < avg[(size_t) k + 1]) continue;
        // The floor around the peak, leaving out the peak's own few bins.
        double floor = 0.0;
        int cnt = 0;
        for (int j = k - 30; j <= k + 30; ++j)
            if (std::abs (j - k) > 4 && j > 0) { floor += avg[(size_t) j]; ++cnt; }
        floor /= juce::jmax (1, cnt);
        const double ratio = m / juce::jmax (1.0e-12, floor);
        if (ratio < 1.8) continue;
        const double a = avg[(size_t) k - 1], b = m, c = avg[(size_t) k + 1];
        const double den = a - 2.0 * b + c;   // negative at a peak
        const double d = std::abs (den) > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / den) : 0.0;
        const double hz = ((double) k + d) * binHz;
        const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
        if (midi < 20 || midi > 100) continue;
        const double weight = (ratio - 1.0) * std::sqrt (m);
        byMidi[(size_t) midi] += weight;
        total += weight;
    }
    if (total <= 0.0) { s.coreStrength = 0.0f; return; }

    std::array<double, 12> chroma {};
    for (int m = 0; m < 128; ++m) chroma[(size_t) (m % 12)] += byMidi[(size_t) m];
    int pc = 0;
    for (int i = 1; i < 12; ++i) if (chroma[(size_t) i] > chroma[(size_t) pc]) pc = i;
    int best = pc;
    for (int m = pc; m < 128; m += 12) if (byMidi[(size_t) m] > byMidi[(size_t) best]) best = m;
    s.coreMidi = (float) best;
    s.coreStrength = (float) (chroma[(size_t) pc] / (total / 12.0));   // 1: spread evenly, 12: one note only
    if (s.notes.empty()) { s.notes.push_back ({ 0, n, s.coreMidi }); s.wholeNote = true; }
}
} // namespace gf
