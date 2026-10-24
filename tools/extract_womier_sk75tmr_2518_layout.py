"""Reviewed layout extraction for Womier SK75 TMR, board 2518 (USB 3151:5030, RY1B-2518).

Owner 2026-10-21: a user reports SK75 TMR works perfectly; Supported needs a built-in
layout first. Geometry: the RongYuan CN and IOT clients carry the same layout object for
ry5088_womiersk75he_8k_8k (checked byte for byte); its key set must equal the reviewed
factory matrix in profiles.json. Board 3804 (Europe, RAlt instead of RCtrl) has no layout
object in either client and gets no layout here.
Outputs (LF):
  docs/research/rongyuan-stream/sources/sk75tmr-2518-geometry.js
  docs/research/rongyuan-layouts/ry_layout_2518.json
Run with --check to verify that committed outputs equal a fresh extraction.
"""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_rongyuan_irok_mars68pro_2438 as rongyuan  # noqa: E402  (pinned client readers)

ROOT = Path(__file__).resolve().parents[1]
BOARD, NAME, PRODUCT = 2518, 'ry5088_womiersk75he_8k_8k', 'RY1B-2518'
GEOMETRY = ROOT / 'docs/research/rongyuan-stream/sources/sk75tmr-2518-geometry.js'
LAYOUT = ROOT / 'docs/research/rongyuan-layouts/ry_layout_2518.json'
LABELS = dict(rongyuan.LABELS, **{'Home': 'Home', 'End': 'End', '`': '`'})


def excerpt_of(text):
    var = re.search(re.escape(NAME) + r':\{layout:([\w$]+)\.layout,', text).group(1)
    start = re.search(r'(?<![\w$])' + re.escape(var) + r'=\{deltX:0,deltY:0,layout:\[', text).start() + len(var) + 1
    return rongyuan.balanced(text, start)


def build():
    cn, iot = rongyuan.read('cn_app'), rongyuan.read('iot')
    excerpt = excerpt_of(cn)
    if excerpt_of(iot) != excerpt:
        raise SystemExit('clients disagree on geometry')
    profile = next(m for m in json.loads((ROOT / 'docs/research/rongyuan-stream/profiles.json').read_bytes())
                   if m['board'] == BOARD)
    matrix = profile['matrix']
    factory = []
    for slot in range(128):
        b = matrix[slot * 4:slot * 4 + 4]
        if b[0] == 0 and b[1] == 0 and b[2]:
            factory.append(b[2])
        elif b == [10, 1, 0, 0]:
            factory.append(rongyuan.HALLJOY_FN)
    vendor = rongyuan.keys_of(excerpt)
    rows = sorted({k['y'] for k in vendor}, reverse=True)
    left = min(k['x'] for k in vendor)
    number_row = {k['value']: k['x'] for k in vendor if k['y'] == rows[1]}
    pitch = (number_row[46] - number_row[30]) / 11
    cap = next(k['w'] for k in vendor if k['value'] == 30)
    keys = []
    for k in vendor:
        hid = rongyuan.HALLJOY_FN if k['value'] == rongyuan.VENDOR_FN else k['value']
        units = (k['w'] + pitch - cap) / pitch
        keys.append({'hid': hid, 'label': LABELS.get(k['keyName'], k['keyName']),
                     'x': round((k['x'] - left) * 46 / pitch), 'y': rows.index(k['y']) * 46,
                     'w': round(units * 46 - 6), 'h': 40})
    if sorted(k['hid'] for k in keys) != sorted(factory) or len(keys) != 81:
        raise SystemExit('geometry key set differs from the factory matrix')
    layout = {'schema': 1, 'id': 'ry_layout_2518', 'brand': 'Womier', 'model': 'SK75 TMR',
              'variant': 'ANSI', 'name': 'Womier SK75 TMR ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'rongyuan-stream', 'products': [PRODUCT]},
              'sources': [{'path': GEOMETRY.relative_to(ROOT).as_posix(),
                           'sha256': rongyuan.sha(excerpt.encode() + b'\n')}],
              'models': ['SK75 TMR'], 'boards': [BOARD],
              'notes': ['Geometry from the RongYuan CN and IOT client layout object for ' + NAME +
                        ' (identical in both); vendor caps scaled to the 46 px HallJoy pitch.',
                        'Key set equals the reviewed factory matrix (81 keys). Board 3804 has no layout object.',
                        'No visual or physical test claimed.']}
    return {GEOMETRY: excerpt.encode() + b'\n', LAYOUT: rongyuan.lf_json(layout)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    outputs = build()
    changed = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        if changed:
            print('SK75TMR_2518_LAYOUT=FAIL changed=' + ','.join(p.name for p in changed))
            return 1
    else:
        for path in changed:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(outputs[path])
    print(f'SK75TMR_2518_LAYOUT=PASS keys=81 written={0 if args.check else len(changed)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
