"""Builds Grain Field 0.4's built-in sounds: abstract textures that blend.

Recordings from the Versilian Community Sample Library (CC0) are stretched into slow
smears (a Paulstretch-style spectral stretch), blurred, reversed and layered. Each tonal
texture keeps a note map, so the plugin can still move its grains onto the chord.
The places (rain, wind, a low room) are synthesised; the ocean drum is a real recording.
"""
import json, os, subprocess, sys
import numpy as np
from scipy import signal

sys.path.insert(0, os.path.dirname(__file__))
import build_sounds as B   # loaders, trimming, synthesised places

SR = B.SR
OUT = B.OUT
INC = B.INC
VCSL = B.VCSL
sel = B.sel
rng = np.random.default_rng(41)

def stretch(x, factor, win=8192, seed=0):
    """Spectral stretch: keep each frame's magnitudes, randomise phases, overlap-add."""
    r = np.random.default_rng(seed)
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 1: x = x[:, None]
    n, ch = x.shape
    hop_out = win // 4
    hop_in = hop_out / factor
    frames = int((n - win) / hop_in)
    out = np.zeros((frames * hop_out + win, ch))
    w = np.hanning(win)
    for c in range(ch):
        for f in range(frames):
            p = int(f * hop_in)
            spec = np.abs(np.fft.rfft(x[p:p + win, c] * w))
            ph = r.uniform(0, 2 * np.pi, len(spec))
            y = np.fft.irfft(spec * np.exp(1j * ph)) * w
            out[f * hop_out: f * hop_out + win, c] += y
    return out

def notes_material(key, picks, seg, sustained, trust='small', lo=None, hi=None, reverse=False):
    """Short segments of chosen notes, joined, with a note map (in samples)."""
    files = [(B.midi_from_name(p), p) for p in sel[key]]
    files = sorted(f for f in files if f[0] is not None and (lo is None or f[0] >= lo) and (hi is None or f[0] <= hi))
    files = B.pick_even(files, picks)
    parts, notes, pos = [], [], 0
    for midi, p in files:
        x = B.trim_start(B.load(os.path.join(VCSL, p)))
        if sustained: x = x[int(0.3 * SR):]
        x = x[: int(seg * SR)].copy()
        cents = B.measure_pitch(x, midi)
        use = cents is not None and (trust == 'measure' or (trust == 'small' and abs(cents) < 25))
        x *= 0.15 / B.rms(x)
        if reverse: x = x[::-1].copy()
        x = B.fade(x, 0.05, 0.05)
        notes.append([pos, pos + len(x), midi + (cents / 100 if use else 0.0)])
        parts.append(x); pos += len(x)
    return np.concatenate(parts), notes

def scale_notes(notes, factor, offset=0):
    return [[int(a * factor) + offset, int(b * factor) + offset, m] for a, b, m in notes]

def lowpass(x, hz, order=2): return signal.sosfilt(signal.butter(order, hz, 'lowpass', fs=SR, output='sos'), x, axis=0)
def highpass(x, hz, order=2): return signal.sosfilt(signal.butter(order, hz, 'highpass', fs=SR, output='sos'), x, axis=0)
def peak(x, hz, q, gain):
    b, a = signal.iirpeak(hz, q, fs=SR)
    return x + gain * signal.lfilter(b, a, x, axis=0)

def octave_up(x):
    """Plays the material twice as fast (an octave up) and loops it to the same length."""
    y = x[::2]
    return np.concatenate([y, y])[: len(x)]

def wow(x, depth_ms=6.0, rate=0.25, seed=3):
    """Slow tape wow: a gently wandering delay."""
    r = np.random.default_rng(seed)
    n = len(x)
    k = int(n / SR * rate) + 4
    pts = r.uniform(-1, 1, k)
    t = np.linspace(0, k - 1, n)
    i = np.floor(t).astype(int); f = t - i
    f = 0.5 - 0.5 * np.cos(np.pi * f)
    lfo = pts[i] * (1 - f) + pts[np.minimum(i + 1, k - 1)] * f
    d = (depth_ms / 1000 * SR) * (1 + lfo)
    idx = np.clip(np.arange(n) - d, 0, n - 1)
    i0 = np.floor(idx).astype(int); fr = (idx - i0)[:, None]
    i1 = np.minimum(i0 + 1, n - 1)
    return x[i0] * (1 - fr) + x[i1] * fr

def dust_layer(n, rate=6, seed=9):
    r = np.random.default_rng(seed)
    out = np.zeros((n, 2))
    for t in r.integers(0, n - 400, int(n / SR * rate)):
        L = r.integers(20, 200)
        burst = r.standard_normal(L) * np.exp(-np.arange(L) / (L / 5)) * r.lognormal(-4.2, 0.7)
        burst = np.diff(np.concatenate([[0], burst]))
        c = r.integers(0, 2)
        out[t:t + L, c] += burst
        out[t:t + L, 1 - c] += burst * 0.4
    return lowpass(out, 6000)

catalogue = []

def add(id_, name, kind, audio, notes=None, credit='VCSL (CC0), stretched'):
    audio = B.norm(audio, 0.85)
    path = os.path.join(OUT, id_ + '.ogg')
    tmp = B.WORK / 'textures-tmp.f32'
    audio.astype(np.float32).tofile(tmp)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', '2', '-i', tmp,
                    '-c:a', 'libvorbis', '-q:a', '4', path], check=True)
    catalogue.append({'id': id_, 'name': name, 'kind': kind, 'file': id_ + '.ogg', 'notes': notes or [],
                      'seconds': round(len(audio) / SR, 2), 'credit': credit})
    print(f'  {id_}: {len(audio) / SR:.1f} s, {len(notes or [])} notes')

def main():
    global OUT, INC, VCSL, sel
    B.configure_paths()
    OUT, INC, VCSL, sel = B.OUT, B.INC, B.VCSL, B.sel
    catalogue.clear()
    print('Building textures')

    # Glass haze: rubbed wine glasses, stretched four times, with a faint octave shimmer.
    x, n = notes_material('glass', 4, 2.0, True, trust='measure')
    y = stretch(x, 4.0, seed=1)
    shimmer = np.zeros_like(y)
    for a, b, _ in scale_notes(n, 4.0):   # an octave up, note by note, so every moment keeps one pitch
        seg = y[a:min(b, len(y))]
        if len(seg) > 16: shimmer[a:a + len(seg)] = octave_up(seg)
    y = y + 0.18 * shimmer
    y = lowpass(highpass(y, 120), 7000)
    add('glasshaze', 'glass haze', 'played', B.make_loop(y, 2.0), scale_notes(n, 4.0))

    # Tape choir: chapel organ through vowel-like resonances, stretched, then worn by tape wow.
    xo, no = notes_material('organ', 6, 1.3, True, lo=48, hi=72)
    m = len(xo)
    y = stretch(xo, 4.5, seed=3) + 0.6 * stretch(xo, 4.5, seed=13)   # two passes: a fuller, chorus-like smear
    for hz, q, g in ((700, 3, 1.6), (1150, 4, 1.2), (2600, 5, 0.8)):
        y = peak(y, hz, q, g)
    y = wow(lowpass(highpass(y, 90), 5500), depth_ms=5.0, rate=0.2)
    add('tapechoir', 'tape choir', 'played', B.make_loop(y, 2.0), scale_notes([v for v in no if v[0] < m], 4.5))

    # Dust halo: vibraphone notes played backwards and stretched into a halo, with old-record dust.
    x, n = notes_material('vibes', 6, 1.3, False, reverse=True)
    y = stretch(x, 4.0, seed=4)
    y = lowpass(highpass(y, 150), 6500)
    y = y + dust_layer(len(y)) * 0.6
    add('dusthalo', 'dust halo', 'played', B.make_loop(y, 2.0), scale_notes(n, 4.0))

    # Bowed air: bowed psaltery stretched, with breath around it.
    x, n = notes_material('psaltery', 6, 1.3, True)
    y = stretch(x, 4.0, seed=5)
    air = B.pink(len(y), 77)
    air = signal.sosfilt(signal.butter(2, [900, 6000], 'bandpass', fs=SR, output='sos'), air)
    air = air / np.std(air) * 0.018
    y = y + np.stack([air, np.roll(air, 4410)], axis=1) * (0.6 + 0.4 * B.smooth_random(len(y), 0.15, 8))[:, None]
    y = lowpass(highpass(y, 80), 8000)
    add('bowedair', 'bowed air', 'sustained', B.make_loop(y, 2.0), scale_notes(n, 4.0))

    # Low hum: bowed vibes, stretched long and kept dark.
    xb, nb = notes_material('bowedvibes', 5, 1.6, True)
    m = len(xb)
    y = lowpass(stretch(xb, 4.5, seed=6), 2200)
    add('lowhum', 'low hum', 'sustained', B.make_loop(y, 2.0), scale_notes([v for v in nb if v[0] < m], 4.5))

    # Glass drone: the same glasses, stretched much further, for the drones.
    x, n = notes_material('glass', 3, 2.5, True, trust='measure')
    y = lowpass(highpass(stretch(x, 5.0, seed=7), 100), 5000)
    add('glassdrone', 'glass drone', 'sustained', B.make_loop(y, 2.0), scale_notes(n, 5.0))

    print('Building places')
    parts = []
    for p in sel['ocean']:
        y = B.trim_start(B.load(os.path.join(VCSL, p)))[int(0.5 * SR): int(12.5 * SR)].copy()
        parts.append(y * (0.12 / B.rms(y)))
    ov = int(2.5 * SR); w = (0.5 - 0.5 * np.cos(np.pi * np.arange(ov) / ov))[:, None]
    x = parts[0]
    for y in parts[1:]:
        x = np.concatenate([x[:-ov], x[-ov:] * (1 - w) + y[:ov] * w, y[ov:]])
    add('ocean', 'ocean drum', 'place', B.make_loop(lowpass(x, 9000), 2.5), credit='VCSL (CC0)')

    def low_room(seconds=36):
        n = int(seconds * SR)
        out = np.zeros((n, 2))
        for c in range(2):
            brown = signal.lfilter([1], [1, -0.997], np.random.default_rng(500 + c).standard_normal(n))
            brown = signal.sosfilt(signal.butter(2, [70, 400], 'bandpass', fs=SR, output='sos'), brown)
            out[:, c] = brown / np.std(brown) * 0.05 * (0.7 + 0.5 * B.smooth_random(n, 0.04, 60 + c))
            # The air of a room: soft broadband hush, with a faint resonance or two.
            air = signal.sosfilt(signal.butter(2, [300, 5000], 'bandpass', fs=SR, output='sos'), B.pink(n, 90 + c))
            air = peak(air[:, None], 820, 6, 1.5)[:, 0]
            out[:, c] += air / np.std(air) * 0.03 * (0.8 + 0.4 * B.smooth_random(n, 0.07, 95 + c))
            drift = 1 + 0.002 * B.smooth_random(n, 0.1, 70 + c, -1, 1)
            for h, a in ((50, 0.004), (100, 0.004), (150, 0.003), (200, 0.0015)):
                out[:, c] += a * np.sin(2 * np.pi * h * np.cumsum(drift) / SR)
            hiss = signal.sosfilt(signal.butter(2, [2000, 9000], 'bandpass', fs=SR, output='sos'), np.random.default_rng(80 + c).standard_normal(n))
            out[:, c] += hiss / np.std(hiss) * 0.004
        return B.make_loop(out, 3.0)

    add('lowroom', 'low room', 'place', low_room(), credit='Generated')
    add('rain', 'rain', 'place', B.gen_rain(), credit='Generated')
    add('wind', 'wind', 'place', B.gen_wind(), credit='Generated')

    (B.WORK / 'catalogue.json').write_text(json.dumps(catalogue, indent=1) + '\n')
    lines = ['// Generated by Tools/build_textures.py. The built-in sounds and where each note sits.', '']
    for s in catalogue:
        sym = s['id'] + '_ogg'
        notes = ', '.join(f'{{ {a}, {b}, {m:.3f}f }}' for a, b, m in s['notes'])
        lines.append(f'{{ "{s["id"]}", "{s["name"]}", BuiltinKind::{s["kind"]}, SoundData::{sym}, SoundData::{sym}Size, {{ {notes} }} }},')
    open(INC, 'w').write('\n'.join(lines) + '\n')
    total = sum(os.path.getsize(os.path.join(OUT, s['file'])) for s in catalogue)
    print(f'Done: {len(catalogue)} sounds, {total / 1e6:.1f} MB')


if __name__ == '__main__':
    main()
