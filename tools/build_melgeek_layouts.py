"""MelGeek MADE68 Air ANSI layout from a reviewed transcription (2026-10-12).

Geometry: the top-view photo of the keyboard supplied by the owner (stays local, see EVIDENCE) shows a 68-key 65%
layout with Esc, a 2u Backspace, a 1.5u Tab and backslash, a 1.75u Caps Lock, a 2.25u Enter and left Shift, a 1.75u
right Shift and 1.25u/6.25u bottom keys; every row is 16 units wide. Key identity: the 68 key ids of the live report
(src/HallJoyProject/HallJoy/melgeek_made68air_protocol.h, kPriorKeyMap) follow the reading order of these rows; 65 of
them were paired with Windows key-downs on the hardware (tester log, docs/current/MELGEEK_MADE68_AIR_2026-10-12.md).
The key at the end of the top row carries a house symbol but sends the grave usage (0x35), so it is listed as grave.
Fn (the dot-grid keycap left of the right Ctrl) uses HallJoy's Fn code 0x409. No HID access or vendor code execution.

Usage: python tools/build_melgeek_layouts.py [--write]
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/melgeek-layout-sources/made68air-geometry.json'
REPORT = ROOT / 'docs/research/melgeek-layout-reports/melgeek_made68air_ansi.json'
PROTOCOL = ROOT / 'src/HallJoyProject/HallJoy/melgeek_made68air_protocol.h'
FN = 1033

# (label, HID usage, width in units) per row, left to right without gaps.
ROWS = [
    [('Esc', 0x29, 1), ('1', 0x1e, 1), ('2', 0x1f, 1), ('3', 0x20, 1), ('4', 0x21, 1), ('5', 0x22, 1), ('6', 0x23, 1),
     ('7', 0x24, 1), ('8', 0x25, 1), ('9', 0x26, 1), ('0', 0x27, 1), ('-', 0x2d, 1), ('=', 0x2e, 1),
     ('Backspace', 0x2a, 2), ('`', 0x35, 1)],
    [('Tab', 0x2b, 1.5), ('Q', 0x14, 1), ('W', 0x1a, 1), ('E', 0x08, 1), ('R', 0x15, 1), ('T', 0x17, 1), ('Y', 0x1c, 1),
     ('U', 0x18, 1), ('I', 0x0c, 1), ('O', 0x12, 1), ('P', 0x13, 1), ('[', 0x2f, 1), (']', 0x30, 1),
     ('\\', 0x31, 1.5), ('Delete', 0x4c, 1)],
    [('Caps', 0x39, 1.75), ('A', 0x04, 1), ('S', 0x16, 1), ('D', 0x07, 1), ('F', 0x09, 1), ('G', 0x0a, 1),
     ('H', 0x0b, 1), ('J', 0x0d, 1), ('K', 0x0e, 1), ('L', 0x0f, 1), (';', 0x33, 1), ("'", 0x34, 1),
     ('Enter', 0x28, 2.25), ('PgUp', 0x4b, 1)],
    [('Shift', 0xe1, 2.25), ('Z', 0x1d, 1), ('X', 0x1b, 1), ('C', 0x06, 1), ('V', 0x19, 1), ('B', 0x05, 1),
     ('N', 0x11, 1), ('M', 0x10, 1), (',', 0x36, 1), ('.', 0x37, 1), ('/', 0x38, 1), ('Shift', 0xe5, 1.75),
     ('Up', 0x52, 1), ('PgDn', 0x4e, 1)],
    [('Ctrl', 0xe0, 1.25), ('Win', 0xe3, 1.25), ('Alt', 0xe2, 1.25), ('Space', 0x2c, 6.25), ('Alt', 0xe6, 1),
     ('Fn', FN, 1), ('Ctrl', 0xe4, 1), ('Left', 0x50, 1), ('Down', 0x51, 1), ('Right', 0x4f, 1)],
]
EVIDENCE = [
    {'what': 'Owner-supplied top-view photo of the MADE68 Air (local only): 68 keys, rows of 15/15/14/14/10, ANSI, purple Esc and Enter',
     'sha256': '5e05c401fd858d7c53d8be0459cdd1b8e540f4e264442116787f1d7189e3e8bf',
     'local': '.local/research/melgeek-made68air-20261012/images/made68air_top_view.webp'},
    {'what': 'Tester log message (8): slow sequential key presses, 66 key ids paired with Windows key-downs by timing, 65 equal to the table',
     'sha256': '1731bf68ac4ca53bee27725ba582c4c5944d1e09d84a480ba28574e4883f2d16',
     'local': '.local/research/melgeek-made68air-20261012/log8_sequential_keys.txt'},
]
PITCH, SIZE = 44, 42


def protocol_usages():
    # The 68 usages of the key map in melgeek_made68air_protocol.h (Fn = halljoy::keycode::kFn = 0x409), by key id.
    text = PROTOCOL.read_text(encoding='utf-8')
    start = text.index('const std::uint16_t pairs[][2] = {')
    body = text[start:text.index('};', start)]
    body = re.sub(r'//[^\n]*', '', body.split('{', 1)[1])
    usages = {}
    for key_id, value in re.findall(r'\{\s*(\d+)\s*,\s*([A-Za-z0-9_:]+)\s*\}', body):
        usages[int(key_id)] = FN if value.endswith('kFn') else int(value, 16)
    assert sorted(usages) == list(range(68)), 'key map does not cover ids 0..67'
    return usages


def source_bytes():
    data = {'schema': 1, 'model': 'MADE68 Air', 'variant': 'ANSI', 'units': 'key units, 1u pitch',
            'evidence': EVIDENCE, 'rows': [[list(k) for k in row] for row in ROWS]}
    return (json.dumps(data, indent=1) + '\n').encode()


def report_bytes(source_sha):
    usages = protocol_usages()
    # The layout lists the keys in the reading order of the key ids: row after row, left to right.
    flat = [hid for row in ROWS for _, hid, _ in row]
    assert flat == [usages[i] for i in range(68)], 'Layout order differs from the key-id table'
    keys = []
    for r, row in enumerate(ROWS):
        x = 0.0
        for label, hid, w in row:
            keys.append({'hid': hid, 'label': label, 'x': round(x * PITCH), 'y': r * PITCH,
                         'w': round(w * PITCH) - (PITCH - SIZE), 'h': SIZE})
            x += w
        assert abs(x - 16) < 1e-9, ('row width', r, x)
    assert len(keys) == 68 and len({k['hid'] for k in keys}) == 68
    report = {'schema': 1, 'id': 'melgeek_made68air_ansi', 'brand': 'MelGeek', 'model': 'MADE68 Air',
              'variant': 'ANSI', 'name': 'MelGeek MADE68 Air ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'melgeek-made68air', 'products': ['MADE68AIR-0416-0114']},
              'sources': [{'path': SOURCE.relative_to(ROOT).as_posix(), 'sha256': source_sha, 'url': ''}],
              'notes': ['68-key 65% (16 units per row) from the owner-supplied top-view photo.',
                        'Rows and keys equal the key-id table of the live report; Fn = 0x409; the top-right key sends grave.',
                        'Automatic selection by the verified live-report session identity (0416:0114).']}
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
    print('MELGEEK_LAYOUTS=PASS models=1 keys=68 sha256=' + hashlib.sha256(report).hexdigest())


if __name__ == '__main__':
    main()
