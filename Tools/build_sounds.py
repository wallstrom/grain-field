"""Builds Grain Field's built-in sounds.

Real instruments come from the Versilian Community Sample Library (CC0, public domain).
Each sound is a run of single notes, trimmed, levelled, sorted low to high, with a note map
so the plugin knows the pitch at every point. The beds that need real places (rain, wind,
a distant city) are synthesised here, since there's no CC0 field recording source to hand.

Output: Resources/Sounds/<id>.ogg and Source/BuiltinSounds.inc (the catalogue and note maps).
"""
import argparse, json, re, subprocess, sys, os
from pathlib import Path
import numpy as np
from scipy import signal

SR = 44100
ROOT = Path(__file__).resolve().parent.parent
VCSL = ROOT / '.tools' / 'VCSL'
OUT = ROOT / 'Resources' / 'Sounds'
INC = ROOT / 'Source' / 'BuiltinSounds.inc'
WORK = ROOT / 'build' / 'soundbuild'
sel = {}
rng = np.random.default_rng(23)

def configure_paths(argv=None):
    """Resolve defaults from the repository, not the caller's working directory."""
    global VCSL, OUT, INC, WORK, sel
    parser = argparse.ArgumentParser(description='Rebuild Grain Field sound assets (requires NumPy, SciPy and ffmpeg).')
    parser.add_argument('--vcsl', type=Path, default=VCSL, help='VCSL checkout (default: .tools/VCSL in the repository)')
    parser.add_argument('--selection', type=Path, default=ROOT / 'Tools' / 'selection.json')
    parser.add_argument('--output', type=Path, default=OUT, help='Directory for generated Ogg files')
    parser.add_argument('--include', dest='include_file', type=Path, default=INC, help='Generated C++ catalogue')
    parser.add_argument('--work-dir', type=Path, default=WORK, help='Scratch files and catalogue.json')
    args = parser.parse_args(argv)
    if not args.vcsl.is_dir():
        parser.error(f'VCSL checkout not found: {args.vcsl}. Clone VCSL there or pass --vcsl.')
    try:
        sel = json.loads(args.selection.read_text())
    except (OSError, ValueError) as error:
        parser.error(f'Cannot read selection: {error}')
    VCSL, OUT, INC, WORK = args.vcsl.resolve(), args.output.resolve(), args.include_file.resolve(), args.work_dir.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    INC.parent.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)

NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}

def midi_from_name(path):
    m = re.search(r'_([A-G])(#?)(-?\d)(?=[_.])', os.path.basename(path))
    if not m: return None
    return NOTE[m.group(1)] + (1 if m.group(2) else 0) + 12 * (int(m.group(3)) + 1)

def load(path):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'f32le', '-ac', '2', '-ar', str(SR), '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).copy()

def trim_start(x, thresh_db=-42):
    mono = np.abs(x).max(axis=1)
    pk = mono.max() + 1e-12
    idx = np.argmax(mono > pk * 10 ** (thresh_db / 20))
    return x[max(0, idx - int(0.004 * SR)):]

def fade(x, fade_in, fade_out):
    n = len(x)
    fi, fo = min(int(fade_in * SR), n // 4), min(int(fade_out * SR), n // 2)
    if fi > 0: x[:fi] *= (0.5 - 0.5 * np.cos(np.pi * np.arange(fi) / fi))[:, None]
    if fo > 0: x[n - fo:] *= (0.5 + 0.5 * np.cos(np.pi * np.arange(fo) / fo))[:, None]
    return x

def measure_pitch(x, expect):
    """Fine pitch near the expected note: returns cents offset, or None if unclear."""
    mono = x[int(0.08 * SR): int(1.2 * SR)].mean(axis=1)
    if len(mono) < SR // 4: mono = x.mean(axis=1)
    n = 1 << 17
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)), n))
    freqs = np.fft.rfftfreq(n, 1 / SR)
    f0 = 440 * 2 ** ((expect - 69) / 12)
    best = None
    for h in (1, 2, 3, 4):  # look for the fundamental or a low partial, within a semitone
        lo, hi = f0 * h * 2 ** (-1 / 12), f0 * h * 2 ** (1 / 12)
        band = (freqs > lo) & (freqs < hi)
        if not band.any(): continue
        i = np.argmax(np.where(band, spec, 0))
        strength = spec[i] / (spec[band].mean() + 1e-12)
        if i > 0 and i < len(spec) - 1:  # parabolic interpolation
            a, b, c = np.log(spec[i - 1] + 1e-12), np.log(spec[i] + 1e-12), np.log(spec[i + 1] + 1e-12)
            p = 0.5 * (a - c) / (a - 2 * b + c)
            fi = (i + p) * SR / n
        else: fi = freqs[i]
        cents = 1200 * np.log2(fi / (f0 * h))
        if strength > 6 and (best is None or strength > best[1]): best = (cents, strength)
    return None if best is None else float(best[0])

def rms(x): return float(np.sqrt(np.mean(x ** 2)) + 1e-12)

def pick_even(items, n):
    if len(items) <= n: return items
    idx = np.round(np.linspace(0, len(items) - 1, n)).astype(int)
    return [items[i] for i in idx]

def build_notes(key, max_notes, note_len, fade_out, sustained=False, lo=None, hi=None, trust='small'):
    """trust: 'name' uses the file's note, 'measure' the measured pitch, 'small' the measurement only when it's close."""
    files = [(midi_from_name(p), p) for p in sel[key]]
    files = [f for f in files if f[0] is not None and (lo is None or f[0] >= lo) and (hi is None or f[0] <= hi)]
    files.sort()
    files = pick_even(files, max_notes)
    parts, notes, pos, report = [], [], 0, []
    for midi, p in files:
        x = trim_start(load(os.path.join(VCSL, p)))
        if sustained:  # skip the very start of bowed or rubbed notes, keep the singing middle
            x = x[int(0.15 * SR):]
        x = x[: int(note_len * SR)].copy()
        cents = measure_pitch(x, midi)
        level = rms(x[: int(min(len(x), 1.5 * SR))])
        x *= 0.17 / level
        pk = np.abs(x).max()
        if pk > 0.95: x *= 0.95 / pk
        x = fade(x, 0.002 if not sustained else 0.25, fade_out)
        use = cents is not None and (trust == 'measure' or (trust == 'small' and abs(cents) < 25))
        notes.append((pos, pos + len(x), midi + (cents / 100 if use else 0.0)))
        report.append(f'{midi}{"" if cents is None else f" {cents:+.0f}c"}')
        parts.append(x); pos += len(x)
    print(f'  {key}: {len(files)} notes, {pos / SR:.1f} s  [{", ".join(report)}]')
    return np.concatenate(parts), notes

def build_untuned(key, seg_len, fade_out, files=None, skip=0.0):
    paths = files or sel[key]
    parts = []
    for p in paths:
        x = trim_start(load(os.path.join(VCSL, p)))[int(skip * SR):]
        x = x[: int(seg_len * SR)].copy()
        x *= 0.17 / rms(x[: int(min(len(x), 1.5 * SR))])
        pk = np.abs(x).max()
        if pk > 0.95: x *= 0.95 / pk
        parts.append(fade(x, 0.003, fade_out))
    out = np.concatenate(parts)
    print(f'  {key}: {len(paths)} parts, {len(out) / SR:.1f} s')
    return out

def make_loop(x, xfade=2.0):
    """Crossfades the end into the start so a bed can loop without a seam."""
    n = int(xfade * SR)
    head, tail = x[:n].copy(), x[-n:].copy()
    w = (0.5 - 0.5 * np.cos(np.pi * np.arange(n) / n))[:, None]
    out = x[:-n].copy()
    out[:n] = head * w + tail * (1 - w)
    return out

def norm(x, peak=0.9):
    return x * (peak / (np.abs(x).max() + 1e-12))

# ---------------- Synthesised places ----------------

def pink(n, seed):
    r = np.random.default_rng(seed)
    w = r.standard_normal(n)
    b, a = [0.049922035, -0.095993537, 0.050612699, -0.004408786], [1, -2.494956002, 2.017265875, -0.522189400]
    return signal.lfilter(b, a, w)

def smooth_random(n, rate_hz, seed, lo=0.0, hi=1.0):
    r = np.random.default_rng(seed)
    k = max(2, int(n / SR * rate_hz) + 3)
    pts = r.uniform(lo, hi, k)
    t = np.linspace(0, k - 1, n)
    i = np.floor(t).astype(int); f = t - i
    f = 0.5 - 0.5 * np.cos(np.pi * f)
    return pts[i] * (1 - f) + pts[np.minimum(i + 1, k - 1)] * f

def gen_rain(seconds=32):
    n = int(seconds * SR)
    out = np.zeros((n, 2))
    # The wash: pink noise, shaped like distant rain on leaves.
    for c in range(2):
        bed = pink(n, 100 + c)
        bed = signal.sosfilt(signal.butter(2, [500, 7000], 'bandpass', fs=SR, output='sos'), bed)
        out[:, c] += bed / np.std(bed) * 0.035 * (0.8 + 0.4 * smooth_random(n, 0.1, 5 + c))
    # Small drops: short resonant ticks scattered in stereo.
    r = np.random.default_rng(7)
    count = int(seconds * 140)
    times = r.integers(0, n - 2000, count)
    for t in times:
        f = r.uniform(1200, 5500)
        L = int(SR * r.uniform(0.002, 0.008))
        burst = r.standard_normal(L) * np.exp(-np.arange(L) / (L / 4))
        b, a = signal.iirpeak(f, 6, fs=SR)
        y = signal.lfilter(b, a, np.concatenate([burst, np.zeros(200)])) * r.lognormal(-3.2, 0.6)
        pan = r.uniform(0, 1)
        out[t:t + len(y), 0] += y * np.sqrt(1 - pan); out[t:t + len(y), 1] += y * np.sqrt(pan)
    # Fat drops landing in puddles: a falling 'plink'.
    count = int(seconds * 6)
    for t in r.integers(0, n - 8000, count):
        f0 = r.uniform(900, 2600)
        L = int(SR * r.uniform(0.03, 0.07))
        tt = np.arange(L) / SR
        y = np.sin(2 * np.pi * f0 * (tt + 0.25 * tt ** 2 * -f0 / 3)) * np.exp(-tt / (L / SR / 4)) * r.lognormal(-3.6, 0.5)
        pan = r.uniform(0.15, 0.85)
        out[t:t + L, 0] += y * np.sqrt(1 - pan); out[t:t + L, 1] += y * np.sqrt(pan)
    # A gutter dripping somewhere close, not quite regular.
    t = int(0.4 * SR)
    while t < n - 20000:
        f0 = 1150 * r.uniform(0.97, 1.03)
        L = int(0.18 * SR); tt = np.arange(L) / SR
        y = (np.sin(2 * np.pi * f0 * tt * (1 - 0.15 * tt)) * np.exp(-tt / 0.035) + 0.3 * np.sin(2 * np.pi * 2.7 * f0 * tt) * np.exp(-tt / 0.01)) * 0.05
        out[t:t + L, 0] += y * 0.8; out[t:t + L, 1] += y * 0.45
        t += int(SR * r.uniform(0.9, 1.9))
    # Rain heard from inside: the top rolls off, the body stays.
    body = signal.sosfilt(signal.butter(2, [250, 1400], 'bandpass', fs=SR, output='sos'), out, axis=0)
    out = signal.sosfilt(signal.butter(2, 5200, 'lowpass', fs=SR, output='sos'), out, axis=0) + body * 0.8
    out = signal.sosfilt(signal.butter(1, 40, 'highpass', fs=SR, output='sos'), out, axis=0)
    return norm(make_loop(out), 0.8)

def gen_wind(seconds=34):
    n = int(seconds * SR)
    out = np.zeros((n, 2))
    gust = smooth_random(n, 0.18, 31, 0.15, 1.0) ** 1.6
    gust = 0.7 * gust + 0.3 * smooth_random(n, 0.6, 32, 0.0, 1.0) * gust
    block = 512
    for c in range(2):
        src = pink(n, 200 + c)
        src = src / np.std(src)
        brown = signal.lfilter([1], [1, -0.995], np.random.default_rng(300 + c).standard_normal(n))
        brown = brown / np.std(brown)
        centre = 280 + 900 * smooth_random(n, 0.12, 40 + c) * (0.4 + 0.6 * gust)
        whistle_f = 620 + 380 * smooth_random(n, 0.07, 50 + c)
        y = np.zeros(n)
        zi1 = zi2 = None
        for s in range(0, n, block):
            e = min(n, s + block)
            sos1 = signal.butter(2, [centre[s] * 0.6, centre[s] * 1.6], 'bandpass', fs=SR, output='sos')
            b, a = signal.iirpeak(whistle_f[s], 35, fs=SR)
            if zi1 is None: zi1 = signal.sosfilt_zi(sos1) * 0; zi2 = np.zeros(2)
            y1, zi1 = signal.sosfilt(sos1, src[s:e], zi=zi1)
            y2, zi2 = signal.lfilter(b, a, src[s:e], zi=zi2)
            y[s:e] = y1 * 0.5 + y2 * 0.12 * gust[s:e] ** 2
        rumble = signal.sosfilt(signal.butter(2, 140, 'lowpass', fs=SR, output='sos'), brown)
        out[:, c] = (y + rumble * 0.25) * (0.25 + 0.75 * gust)
    # Gusts lean left and right.
    lean = smooth_random(n, 0.09, 60, -0.35, 0.35)
    out[:, 0] *= 1 - lean; out[:, 1] *= 1 + lean
    out = signal.sosfilt(signal.butter(1, 30, 'highpass', fs=SR, output='sos'), out, axis=0)
    return norm(make_loop(out, 3.0), 0.8)

def gen_city(seconds=36):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    out = np.zeros((n, 2))
    for c in range(2):
        brown = signal.lfilter([1], [1, -0.997], np.random.default_rng(400 + c).standard_normal(n))
        brown = signal.sosfilt(signal.butter(2, [35, 380], 'bandpass', fs=SR, output='sos'), brown)
        out[:, c] = brown / np.std(brown) * 0.08 * (0.7 + 0.5 * smooth_random(n, 0.05, 70 + c))
        # Mains hum, drifting a little, very low.
        drift = 1 + 0.002 * smooth_random(n, 0.1, 80 + c, -1, 1)
        for h, a in ((50, 0.010), (100, 0.006), (150, 0.003)):
            out[:, c] += a * np.sin(2 * np.pi * h * np.cumsum(drift) / SR)
    # Distant traffic passing: band-limited swells moving across.
    r = np.random.default_rng(90)
    for _ in range(9):
        start = int(r.uniform(0, seconds - 9) * SR); L = int(r.uniform(5, 9) * SR)
        env = np.sin(np.pi * np.arange(L) / L) ** 2
        z = signal.sosfilt(signal.butter(2, [120, r.uniform(700, 1400)], 'bandpass', fs=SR, output='sos'), r.standard_normal(L))
        z = z / np.std(z) * env * r.uniform(0.02, 0.05)
        pan = np.linspace(r.uniform(0, 0.4), r.uniform(0.6, 1), L)
        if r.random() < 0.5: pan = pan[::-1]
        out[start:start + L, 0] += z * np.sqrt(1 - pan); out[start:start + L, 1] += z * np.sqrt(pan)
    # Far-off birds now and then.
    for _ in range(14):
        s0 = int(r.uniform(0, seconds - 1) * SR)
        for k in range(r.integers(2, 5)):
            L = int(r.uniform(0.05, 0.12) * SR); tt = np.arange(L) / SR
            f = r.uniform(2800, 4200) * (1 + r.uniform(-0.25, 0.25) * tt / tt[-1])
            y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.sin(np.pi * tt / tt[-1]) ** 2 * 0.012
            s = s0 + int(k * r.uniform(0.12, 0.22) * SR)
            if s + L < n:
                pan = r.uniform(0.2, 0.8)
                out[s:s + L, 0] += y * np.sqrt(1 - pan); out[s:s + L, 1] += y * np.sqrt(pan)
    return norm(make_loop(out, 3.0), 0.75)

# ---------------- The catalogue ----------------
# kind: played (Chance), sustained (good for Growth's Tone), place (good for the Bed)

def main():
    configure_paths()
    catalogue = []

    def add(id_, name, kind, audio, notes=None, credit='VCSL (CC0)'):
        path = os.path.join(OUT, id_ + '.ogg')
        tmp = WORK / 'sounds-tmp.f32'
        # Played sounds are stored in mono: Chance places every grain in the stereo field itself.
        chans = 1 if kind == 'played' else 2
        (audio.mean(axis=1, keepdims=True) if chans == 1 else audio).astype(np.float32).tofile(tmp)
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', str(chans), '-i', tmp,
                        '-c:a', 'libvorbis', '-q:a', '4', path], check=True)
        catalogue.append({'id': id_, 'name': name, 'kind': kind, 'file': id_ + '.ogg', 'notes': notes or [],
                          'seconds': round(len(audio) / SR, 2), 'credit': credit})

    print('Building sounds')
    x, n = build_notes('chimes', 10, 3.2, 0.8, lo=48, hi=86, trust='name');           add('chimes', 'Hand chimes', 'played', x, n)
    x, n = build_notes('kalimba', 15, 1.9, 0.5, lo=45, hi=90, trust='measure');          add('kalimba', 'Kalimba', 'played', x, n)
    x, n = build_notes('vibes', 10, 3.0, 0.9);                          add('vibes', 'Vibraphone', 'played', x, n)
    x, n = build_notes('piano', 12, 3.0, 1.0, lo=43, hi=84);            add('piano', 'Soft piano', 'played', x, n)
    x, n = build_notes('harp', 12, 2.6, 0.8);                           add('harp', 'Harp', 'played', x, n)
    x, n = build_notes('marimba', 10, 1.8, 0.5);                        add('marimba', 'Marimba', 'played', x, n)
    x, n = build_notes('tubular', 8, 3.6, 1.2, trust='name');                         add('tubular', 'Tubular bells', 'played', x, n)
    x = build_untuned('metal', 3.2, 0.8);                               add('metal', 'Metal', 'played', x)
    x, n = build_notes('glass', 4, 7.0, 1.5, sustained=True, trust='measure');           add('glass', 'Wine glass', 'sustained', x, n)
    x, n = build_notes('bowedvibes', 6, 6.0, 1.5, sustained=True);      add('bowedvibes', 'Bowed vibes', 'sustained', x, n)
    x, n = build_notes('psaltery', 8, 4.5, 1.2, sustained=True);        add('psaltery', 'Bowed psaltery', 'sustained', x, n)
    x, n = build_notes('organ', 9, 4.0, 1.0, sustained=True, lo=40, hi=72); add('organ', 'Chapel organ', 'sustained', x, n)
    x = build_untuned('didge', 7.5, 1.0, files=sel['didge'][:4]);      add('didge', 'Didgeridoo', 'sustained', x)
    x = build_untuned('gong', 9.0, 2.0, files=[p for p in sel['gong'] if 'scrape' not in p][:3] + [p for p in sel['gong'] if 'scrape' in p]); add('gong', 'Gong', 'sustained', x)
    parts = []
    for p in sel['ocean']:  # overlap the three takes so the sea never stops
        y = trim_start(load(os.path.join(VCSL, p)))[int(0.5 * SR): int(12.5 * SR)].copy()
        parts.append(y * (0.12 / rms(y)))
    ov = int(2.5 * SR); w = (0.5 - 0.5 * np.cos(np.pi * np.arange(ov) / ov))[:, None]
    x = parts[0]
    for y in parts[1:]:
        x = np.concatenate([x[:-ov], x[-ov:] * (1 - w) + y[:ov] * w, y[ov:]])
    print(f'  ocean: {len(parts)} takes overlapped, {len(x) / SR:.1f} s')
    x = norm(make_loop(x, 2.5), 0.8); add('ocean', 'Ocean drum', 'place', x)
    print('  rain, wind, city: synthesising')
    add('rain', 'Rain', 'place', gen_rain(), credit='Generated')
    add('wind', 'Wind', 'place', gen_wind(), credit='Generated')
    add('city', 'Distant city', 'place', gen_city(), credit='Generated')

    (WORK / 'catalogue.json').write_text(json.dumps(catalogue, indent=1) + '\n')

    # C++ catalogue
    lines = ['// Generated by build_sounds.py. The built-in sounds and where each note sits.', '']
    for s in catalogue:
        sym = s['id'] + '_ogg'
        notes = ', '.join(f'{{ {a}, {b}, {m:.3f}f }}' for a, b, m in s['notes'])
        lines.append(f'{{ "{s["id"]}", "{s["name"]}", BuiltinKind::{s["kind"]}, SoundData::{sym}, SoundData::{sym}Size, {{ {notes} }} }},')
    open(INC, 'w').write('\n'.join(lines) + '\n')
    total = sum(os.path.getsize(os.path.join(OUT, s['file'])) for s in catalogue)
    print(f'Done: {len(catalogue)} sounds, {total / 1e6:.1f} MB of Ogg')

if __name__ == '__main__':
    main()
