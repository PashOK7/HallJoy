"""WLMOUSE Ying75 ANSI layout from a reviewed transcription (2026-10-05).

Geometry source: the official Web Hub (https://kb75.wlmouse.gg/, script
index-B5Byzn-K.js) draws keys from per-board width tables; the 84-key table
`Keyboard_Layout_width_84` sums to 16 units in every row (compact 75%, no gaps).
Its rounded widths (1.3/1.2/1.8/2.2/1.7/2.3/6.2) are written here as the
standard 1.25/1.75/2.25/6.25 units they approximate. Key membership and matrix
positions come from the official firmware's Windows factory matrix (see
src/HallJoyProject/HallJoy/wlmouse_ying75_protocol.h). The vendor script and
firmware stay local (.local/research/ying75-20261005/). No HID access or vendor
code execution.

Usage: python tools/build_wlmouse_layouts.py [--write]
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/wlmouse-layout-sources/ying75-geometry.json'
REPORT = ROOT / 'docs/research/wlmouse-layout-reports/wlmouse_ying75_ansi.json'
PROTOCOL = ROOT / 'src/HallJoyProject/HallJoy/wlmouse_ying75_protocol.h'

# (label, HID usage, width in units) per row, left to right without gaps.
ROWS = [
    [('Esc', 41, 1), ('F1', 58, 1), ('F2', 59, 1), ('F3', 60, 1), ('F4', 61, 1), ('F5', 62, 1), ('F6', 63, 1),
     ('F7', 64, 1), ('F8', 65, 1), ('F9', 66, 1), ('F10', 67, 1), ('F11', 68, 1), ('F12', 69, 1),
     ('PrtSc', 70, 1), ('ScrLk', 71, 1), ('Delete', 76, 1)],
    [('`', 53, 1), ('1', 30, 1), ('2', 31, 1), ('3', 32, 1), ('4', 33, 1), ('5', 34, 1), ('6', 35, 1),
     ('7', 36, 1), ('8', 37, 1), ('9', 38, 1), ('0', 39, 1), ('-', 45, 1), ('=', 46, 1),
     ('Backspace', 42, 2), ('Home', 74, 1)],
    [('Tab', 43, 1.5), ('Q', 20, 1), ('W', 26, 1), ('E', 8, 1), ('R', 21, 1), ('T', 23, 1), ('Y', 28, 1),
     ('U', 24, 1), ('I', 12, 1), ('O', 18, 1), ('P', 19, 1), ('[', 47, 1), (']', 48, 1), ('\\', 49, 1.5),
     ('PgUp', 75, 1)],
    [('Caps', 57, 1.75), ('A', 4, 1), ('S', 22, 1), ('D', 7, 1), ('F', 9, 1), ('G', 10, 1), ('H', 11, 1),
     ('J', 13, 1), ('K', 14, 1), ('L', 15, 1), (';', 51, 1), ("'", 52, 1), ('Enter', 40, 2.25),
     ('PgDn', 78, 1)],
    [('Shift', 225, 2.25), ('Z', 29, 1), ('X', 27, 1), ('C', 6, 1), ('V', 25, 1), ('B', 5, 1), ('N', 17, 1),
     ('M', 16, 1), (',', 54, 1), ('.', 55, 1), ('/', 56, 1), ('Shift', 229, 1.75), ('Up', 82, 1),
     ('End', 77, 1)],
    [('Ctrl', 224, 1.25), ('Win', 227, 1.25), ('Alt', 226, 1.25), ('Space', 44, 6.25), ('Alt', 230, 1),
     ('Fn', 1033, 1), ('Ctrl', 228, 1), ('Left', 80, 1), ('Down', 81, 1), ('Right', 79, 1)],
]
EVIDENCE = [
    {'url': 'https://kb75.wlmouse.gg/assets/index-B5Byzn-K.js',
     'sha256': '6627840b32cf29d671f3425027c2960897b27e9083ee7ba920bd173daabf15be',
     'what': 'Official Web Hub: Keyboard_Layout_width_84 key widths per 6x21 position'},
    {'url': 'https://cdn.shopify.com/s/files/1/0774/7531/6010/files/XS117_YING75_App_v1.0.2_20250423b.bin',
     'sha256': 'e7a6583ecb8373eb3d4436a68a24e83b6962a12fab17126475e89cd232c69e7a',
     'what': 'Official firmware: Windows factory matrix at 0x4B8 (84 keys)'},
]
PITCH, SIZE = 44, 42


def protocol_keys():
    # The 84 HID usages of the firmware matrix, in row order (Fn -> 0x409).
    text = PROTOCOL.read_text(encoding='utf-8')
    body = text[text.index('{{') + 2:text.index('}}')]
    actions = [int(v) for v in body.replace('\n', ' ').replace(' ', '').split(',') if v]
    assert len(actions) == 126
    return [[1033 if a == 0xf001 else a for a in actions[r * 21:(r + 1) * 21] if a] for r in range(6)]


def source_bytes():
    data = {'schema': 1, 'model': 'WLMOUSE Ying75', 'variant': 'ANSI', 'units': 'key units, 1u pitch',
            'evidence': EVIDENCE, 'rows': [[list(k) for k in row] for row in ROWS]}
    return (json.dumps(data, indent=1) + '\n').encode()


def report_bytes(source_sha):
    assert [[hid for _, hid, _ in row] for row in ROWS] == protocol_keys(), 'Geometry/firmware matrix mismatch'
    keys = []
    for r, row in enumerate(ROWS):
        x = 0.0
        for label, hid, w in row:
            keys.append({'hid': hid, 'label': label, 'x': round(x * PITCH), 'y': r * PITCH,
                         'w': round(w * PITCH) - (PITCH - SIZE), 'h': SIZE})
            x += w
        assert abs(x - 16) < 1e-9, ('row width', r, x)
    assert len(keys) == 84 and len({k['hid'] for k in keys}) == 84
    report = {'schema': 1, 'id': 'wlmouse_ying75_ansi', 'brand': 'WLMOUSE', 'model': 'Ying75',
              'variant': 'ANSI', 'name': 'WLMOUSE Ying75 ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'irok-mg75-pro', 'products': ['YING75-36A7-F887']},
              'sources': [{'path': SOURCE.relative_to(ROOT).as_posix(), 'sha256': source_sha,
                           'url': EVIDENCE[0]['url']}],
              'notes': ['Compact 75% (16 units per row) from the official Web Hub 84-key width table.',
                        'Rows and keys equal the official firmware Windows factory matrix; Fn = 0x409.',
                        'Automatic selection by the verified JingTai session identity.']}
    return (json.dumps(report, indent=1) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    source = source_bytes()
    report = report_bytes(hashlib.sha256(source).hexdigest())
    for path, data in ((SOURCE, source), (REPORT, report)):
        if args.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        elif path.read_bytes() != data:
            raise SystemExit('Out of date: ' + path.relative_to(ROOT).as_posix())
    print('WLMOUSE_LAYOUTS=PASS models=1 keys=84 sha256=' + hashlib.sha256(report).hexdigest())


if __name__ == '__main__':
    main()
