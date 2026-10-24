"""Reviewed layout extraction for MonsGeek M2 V5 HE, board 2845 (USB 3151:5030).

Source: the official MonsGeek web driver (app.monsgeek.com). Its registry maps board 2845
("ry5088_akko_m2v5_3m_8k_8k", displayName "M2 V5 HE") to keyLayout Common98_MonsGeek_M2V5,
whose key-mapping UI info is the chunk below. Vendor files are read as data, never executed.
The key set must equal the board's factory matrix in profiles.json (98 keys including Fn).
Geometry is scaled like the M1 V5 layout (vendor 40 px caps -> HallJoy 42 px, factor 1.05).
Outputs (LF):
  docs/research/rongyuan-stream/sources/m2v5he-2845-geometry.js   geometry excerpt (publication denied)
  docs/research/rongyuan-stream/monsgeek_m2_v5_he_ansi.json         layout report
Run with --acquire to copy the pinned download from .local; default verifies the committed outputs.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import layout_import as common
from generate_attackshark_family import decode
from layout_pipeline import validate_report

BOARD = 2845
PRODUCT = 'RY1B-2845'
DOWNLOAD = ROOT / '.local/research/monsgeek-m2v5-20261024/234b54ea.js'
SOURCE = ROOT / 'docs/research/rongyuan-stream/sources/m2v5he-2845-geometry.js'
SOURCE_SHA = '7ce02b97d478d7223ef9c22773418f2dd98a7156d9f8b414d05ef7c332bc3a81'
URL = 'https://app.monsgeek.com/js/234b54ea.js'
REPORT = ROOT / 'docs/research/rongyuan-stream/monsgeek_m2_v5_he_ansi.json'
SCALE = 1.05


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def hid_table():
    hids = {f'Key{c}': i + 4 for i, c in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ')}
    hids.update({f'Digit{c}': i + 30 for i, c in enumerate('1234567890')})
    hids.update({f'F{i + 1}': 58 + i for i in range(12)})
    hids.update({f'Numpad{i}': (98 if i == 0 else 88 + i) for i in range(10)})
    hids.update(dict(Escape=41, Backspace=42, Tab=43, Space=44, Minus=45, Equal=46, BracketLeft=47,
                     BracketRight=48, Backslash=49, Semicolon=51, Quote=52, Backquote=53, Comma=54,
                     Period=55, Slash=56, CapsLock=57, Enter=40, Delete=76, Home=74, End=77, PageUp=75,
                     PageDown=78, ArrowRight=79, ArrowLeft=80, ArrowDown=81, ArrowUp=82, NumLock=83,
                     NumpadDivide=84, NumpadMultiply=85, NumpadSubtract=86, NumpadAdd=87, NumpadEnter=88,
                     NumpadDecimal=99, ControlLeft=224, ShiftLeft=225, AltLeft=226, MetaLeft=227,
                     ShiftRight=229, AltRight=230, Fn=1033))
    return hids


def build(raw):
    if sha(raw) != SOURCE_SHA:
        raise SystemExit('pinned geometry changed')
    text = raw.decode('utf-8')
    labels = {hid: label for hid, label in common.CODES.values()}
    labels[1033] = 'Fn'
    hids = hid_table()
    keys = []
    for name, x, y, w, h in re.findall(r'(\w+):\{x:(\d+),y:(\d+),width:(\d+),height:(\d+),type:"key"', text):
        hid = hids[name]
        keys.append(dict(hid=hid, label=labels[hid], x=round(int(x) * SCALE), y=round(int(y) * SCALE),
                         w=round(int(w) * SCALE), h=round(int(h) * SCALE)))
    keys.sort(key=lambda k: (k['y'], k['x']))
    profile = next(p for p in json.loads((ROOT / 'docs/research/rongyuan-stream/profiles.json').read_bytes())
                   if p['board'] == BOARD)
    if len(keys) != 98 or {k['hid'] for k in keys} != set(decode(profile['matrix'])) - {0}:
        raise SystemExit('geometry key set differs from the factory matrix')
    report = dict(schema=1, id='monsgeek_m2_v5_he_ansi', brand='MonsGeek', model='M2 V5 HE', variant='ANSI',
                  name='MonsGeek M2 V5 HE ANSI', status='ready', unresolved=[], keys=keys,
                  identity=dict(protocol='rongyuan-stream', products=[PRODUCT]),
                  sources=[dict(path=SOURCE.relative_to(ROOT).as_posix(), sha256=SOURCE_SHA, url=URL)],
                  notes=['Official MonsGeek web driver geometry for keyLayout Common98_MonsGeek_M2V5 (board 2845); '
                         'key set equals the factory matrix (98 keys including Fn). Scaled 1.05 like M1 V5. '
                         'No visual or physical test claimed.'])
    validate_report(report)
    return (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acquire', action='store_true')
    args = parser.parse_args()
    raw = DOWNLOAD.read_bytes() if args.acquire else SOURCE.read_bytes()
    report = build(raw)
    if args.acquire:
        for path, data in ((SOURCE, raw), (REPORT, report)):
            if path.exists():
                if path.read_bytes() != data:
                    raise SystemExit(f'existing output differs: {path}')
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open('xb') as out:
                out.write(data)
    elif REPORT.read_bytes() != report:
        raise SystemExit('committed report differs from a fresh extraction')
    print('MONSGEEK_M2V5HE_LAYOUT=PASS keys=98')


if __name__ == '__main__':
    main()
