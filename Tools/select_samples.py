"""Select source recordings from a VCSL checkout without absolute machine paths."""
import argparse, re, json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
files = []
def pick(pattern, limit=None, exclude=None):
    out = [f for f in files if re.search(pattern, f) and (not exclude or not re.search(exclude, f))]
    return out[:limit] if limit else out
def main(argv=None):
    global files
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vcsl', type=Path, default=ROOT / '.tools' / 'VCSL', help='VCSL checkout')
    parser.add_argument('--output', type=Path, default=ROOT / 'Tools' / 'selection.json', help='Selection JSON output')
    args = parser.parse_args(argv)
    if not args.vcsl.is_dir():
        parser.error(f'VCSL checkout not found: {args.vcsl}. Clone VCSL there or pass --vcsl.')
    files = sorted(p.relative_to(args.vcsl).as_posix() for p in args.vcsl.rglob('*') if p.is_file())
    if not files:
        parser.error(f'No files found in VCSL checkout: {args.vcsl}')
    output = args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    sel = {
     'chimes':   pick(r'Hand Chimes/sus_.*_r01_main\.wav'),
     'kalimba':  pick(r'Kalimba, Tanzania/MBira3_pluck_Main_.*_rr2\.wav'),
     'vibes':    pick(r'Vibraphone/Soft.*_v2_rr1_Main\.wav'),
     'bowedvibes': pick(r'Vibraphone/Bowed/.*rr1_Main\.wav'),
     'marimba':  pick(r'Marimba/.*_soft_01\.wav'),
     'harp':     pick(r'Concert Harp/KSHarp_[A-G]#?[2-5]_mf1\.wav'),
     'tubular':  pick(r'Tubular Bells 2/TB_hit_.*_v2_1\.wav'),
     'glass':    pick(r'Wine Glasses/Sustains/Slow/.*\.wav'),
     'psaltery': pick(r'Psaltery, Bowed and Plucked/LongBow/.*\.wav'),
     'organ':    pick(r"Renaissance Organ/8'/RenOrgan_8foot_Room_[A-G]#?[2-4]_rr1\.wav"),
     'piano':    pick(r'Grand Piano, Kawai/Sustains/GPiano_sus_[A-G]#?[2-5]_v1_rr1_Player\.wav'),
     'didge':    pick(r'Didgeridoo/.*Phrase.*\.wav'),
     'gong':     pick(r'Gong 1/gong_(p|mf)\.wav') + pick(r'Gong 2/(hit_light1|hit_full1|scrape1)\.mp3'),
     'metal':    pick(r'Triangles/Legacy/(1/triangle1_hit_mp|1/triangle1_hit_mp_muted1|2/triangle2_roll|3/triangle3_hit_p)\.wav') + pick(r'Mark Trees/Legacy/windchimes_(random|slowDesc1)\.wav') + pick(r'Bell Tree/Stroke/BellTree_Stroke_(1|4)_Mid\.wav'),
     'ocean':    pick(r'Ocean Drum/.*\.wav'),
    }
    for k,v in sel.items(): print(k, len(v), [x.split('/')[-1] for x in v][:4])
    output.write_text(json.dumps(sel, indent=1) + '\n')


if __name__ == '__main__':
    main()
