"""Approximate layout for YUNZII RT75 Pro, boards 2865 and 3755 (USB 3151:5030).

Owner 2026-10-28: a user (Klutch) reports the RT75 Pro works (HallJoy 1.6.11 log:
board 2865 on the rongyuan-stream path, 81 keys, analog streaming, 0 failures);
Supported needs a built-in layout first. The vendor's own RT75 PRO geometry module
(desktop client `Keyboard_81_RT75_KeyMappings_RT`, keyLayout u.Common81_RT75) is an
uncaptured ESM figma module and is absent from every local client, so the exact SVG
cannot be extracted. Instead this derives an APPROXIMATE layout: the geometry of an
identical-key-set rongyuan 75% sibling (Rampage ZENITH PRO, ry_layout_2310) with its
Home key removed. The resulting 81-key set is verified equal to the reviewed factory
matrix of boards 2865 and 3755 in profiles.json. Boards 3100 (Common82_MK825, 84
keys) and 2445 (Common82_B75PRO, 86 keys) are different layouts and are not covered.

Geometry positions are borrowed, not the vendor's own; no visual or physical test is
claimed. Vendor/report files are read as data, never executed. Outputs (LF):
  docs/research/rongyuan-layouts/ry_layout_2865.json
Run with --check to verify that the committed output equals a fresh derivation.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DONOR = ROOT / 'docs/research/rongyuan-layouts/ry_layout_2310.json'
PROFILES = ROOT / 'docs/research/rongyuan-stream/profiles.json'
OUT = ROOT / 'docs/research/rongyuan-layouts/ry_layout_2865.json'
BOARDS = (2865, 3755)
PRODUCTS = ['RY1B-2865', 'RY1B-3755']
HOME_HID = 74
HALLJOY_FN = 1033


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def factory_set(board):
    profile = next(m for m in json.loads(PROFILES.read_bytes()) if m['board'] == board)
    matrix = profile['matrix']
    keys = set()
    for slot in range(len(matrix) // 4):
        b = matrix[slot * 4:slot * 4 + 4]
        if b[0] == 0 and b[1] == 0 and b[2]:
            keys.add(b[2])
        elif b == [10, 1, 0, 0]:
            keys.add(HALLJOY_FN)
    return keys


def build():
    donor = json.loads(DONOR.read_bytes())
    keys = [dict(k) for k in donor['keys'] if k['hid'] != HOME_HID]
    if len(donor['keys']) - len(keys) != 1:
        raise SystemExit('donor Home key not found exactly once')
    target = factory_set(BOARDS[0])
    for board in BOARDS:
        if factory_set(board) != target:
            raise SystemExit(f'board {board} factory set differs from {BOARDS[0]}')
    if {k['hid'] for k in keys} != target or len(keys) != 81:
        raise SystemExit('derived key set differs from the factory matrix')
    layout = {
        'schema': 1, 'id': 'ry_layout_2865', 'brand': 'YUNZII', 'model': 'RT75 Pro',
        'variant': 'ANSI', 'name': 'YUNZII RT75 Pro ANSI', 'status': 'ready', 'unresolved': [],
        'keys': keys,
        'identity': {'protocol': 'rongyuan-stream', 'products': PRODUCTS},
        'sources': [
            {'path': DONOR.relative_to(ROOT).as_posix(), 'sha256': sha(DONOR.read_bytes())},
            {'path': PROFILES.relative_to(ROOT).as_posix(), 'sha256': sha(PROFILES.read_bytes())},
        ],
        'models': ['RT75 Pro'], 'boards': list(BOARDS),
        'notes': [
            'Approximate layout: geometry adapted from the identical-key-set rongyuan 75% '
            'sibling Rampage ZENITH PRO (ry_layout_2310) with the Home key removed. The '
            'vendor RT75 PRO geometry (desktop client Keyboard_81_RT75_KeyMappings_RT) was '
            'not captured in any local client, so positions are borrowed, not vendor-exact.',
            'Key set verified equal to the reviewed factory matrix of boards 2865 and 3755 '
            '(81 keys). Boards 3100 and 2445 are different layouts and are not covered. No '
            'visual or physical test claimed.',
        ],
    }
    return (json.dumps(layout, ensure_ascii=False, indent=2) + '\n').encode('utf8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = build()
    changed = not OUT.exists() or OUT.read_bytes() != data
    if args.check:
        if changed:
            print('RT75PRO_2865_LAYOUT=FAIL changed=ry_layout_2865.json')
            return 1
    elif changed:
        OUT.write_bytes(data)
    print(f'RT75PRO_2865_LAYOUT=PASS keys=81 sha256={sha(data)} written={0 if args.check else int(changed)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
