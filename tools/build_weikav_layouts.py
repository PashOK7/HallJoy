"""Weikav EG75 HE ANSI layout from a reviewed transcription (2026-10-26).

Key assignments: the official SparkLink PlayJoy driver (星闪悦动) with "EG75"
connected, shown on the Weikav product page. The driver draws every board on
one generic 75% template from the keyboard's row/column code table, so its key
gaps are not physical. Physical positions and widths come from the retail top
view on the same page (80 keys, 0.5u F-row gaps, Insert top right, no key
right of Up). The two disagree only in legends: the retail keycaps right of
Space read ALT and FN, the driver key map reads Fn1 and R-Ctrl; the map is
what the keyboard reports, so it is used. The images stay local
(.local/research/weikav-eg75-20261026/web/); the unit transcription below is
the reviewed evidence. No HID access or vendor code execution.

Usage: python tools/build_weikav_layouts.py [--write]
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/weikav-layout-sources/eg75-he-geometry.json'
REPORT = ROOT / 'docs/research/weikav-layout-reports/weikav_eg75_he_ansi.json'

# (label, HID usage, x in units, width in units) per row.
ROWS = [
    [('Esc', 41, 0, 1), ('F1', 58, 1.5, 1), ('F2', 59, 2.5, 1), ('F3', 60, 3.5, 1), ('F4', 61, 4.5, 1),
     ('F5', 62, 6, 1), ('F6', 63, 7, 1), ('F7', 64, 8, 1), ('F8', 65, 9, 1),
     ('F9', 66, 10.5, 1), ('F10', 67, 11.5, 1), ('F11', 68, 12.5, 1), ('F12', 69, 13.5, 1),
     ('Insert', 73, 15, 1)],
    [('`', 53, 0, 1), ('1', 30, 1, 1), ('2', 31, 2, 1), ('3', 32, 3, 1), ('4', 33, 4, 1), ('5', 34, 5, 1),
     ('6', 35, 6, 1), ('7', 36, 7, 1), ('8', 37, 8, 1), ('9', 38, 9, 1), ('0', 39, 10, 1), ('-', 45, 11, 1),
     ('=', 46, 12, 1), ('Backspace', 42, 13, 2), ('Delete', 76, 15, 1)],
    [('Tab', 43, 0, 1.5), ('Q', 20, 1.5, 1), ('W', 26, 2.5, 1), ('E', 8, 3.5, 1), ('R', 21, 4.5, 1),
     ('T', 23, 5.5, 1), ('Y', 28, 6.5, 1), ('U', 24, 7.5, 1), ('I', 12, 8.5, 1), ('O', 18, 9.5, 1),
     ('P', 19, 10.5, 1), ('[', 47, 11.5, 1), (']', 48, 12.5, 1), ('\\', 49, 13.5, 1.5), ('PgUp', 75, 15, 1)],
    [('Caps', 57, 0, 1.75), ('A', 4, 1.75, 1), ('S', 22, 2.75, 1), ('D', 7, 3.75, 1), ('F', 9, 4.75, 1),
     ('G', 10, 5.75, 1), ('H', 11, 6.75, 1), ('J', 13, 7.75, 1), ('K', 14, 8.75, 1), ('L', 15, 9.75, 1),
     (';', 51, 10.75, 1), ("'", 52, 11.75, 1), ('Enter', 40, 12.75, 2.25), ('PgDn', 78, 15, 1)],
    [('Shift', 225, 0, 2.25), ('Z', 29, 2.25, 1), ('X', 27, 3.25, 1), ('C', 6, 4.25, 1), ('V', 25, 5.25, 1),
     ('B', 5, 6.25, 1), ('N', 17, 7.25, 1), ('M', 16, 8.25, 1), (',', 54, 9.25, 1), ('.', 55, 10.25, 1),
     ('/', 56, 11.25, 1), ('Shift', 229, 12.25, 1.75), ('Up', 82, 14, 1)],
    [('Ctrl', 224, 0, 1.25), ('Win', 227, 1.25, 1.25), ('Alt', 226, 2.5, 1.25), ('Space', 44, 3.75, 6.25),
     ('Fn', 1033, 10, 1.25), ('Ctrl', 228, 11.25, 1.25), ('Left', 80, 13, 1), ('Down', 81, 14, 1),
     ('Right', 79, 15, 1)],
]
EVIDENCE = [
    {'url': 'https://weikav.com/wp-content/uploads/2026/06/driver.jpg',
     'sha256': '5aaa7c95094970d8514efe755c711e279248e0daa10676f5a79b020013bb4a06',
     'what': 'Official SparkLink PlayJoy driver, device "EG75" connected, full key map'},
    {'url': 'https://weikav.com/wp-content/uploads/2026/06/main-1-1.jpg',
     'sha256': '22d1754fd051c3a44bfc71bce6e740cc15b5e94f550ddf3401fcc547e0440d75',
     'what': 'Retail top view: 80 keys, F-row gaps, Insert top right, no key right of Up'},
]
PITCH, SIZE, ROW_Y = 44, 42, (0, 54, 98, 142, 186, 230)
COUNT = 80


def source_bytes():
    data = {'schema': 1, 'model': 'Weikav EG75 HE', 'variant': 'ANSI', 'units': 'key units, 1u pitch',
            'evidence': EVIDENCE, 'rows': [[list(k) for k in row] for row in ROWS]}
    return (json.dumps(data, indent=1, ensure_ascii=False) + '\n').encode()


def report_bytes(source_sha):
    keys = []
    for row, y in zip(ROWS, ROW_Y):
        for label, hid, x, w in row:
            keys.append({'hid': hid, 'label': label, 'x': round(x * PITCH), 'y': y,
                         'w': round(w * PITCH) - (PITCH - SIZE), 'h': SIZE})
    assert len(keys) == COUNT and len({k['hid'] for k in keys}) == COUNT
    report = {'schema': 1, 'id': 'weikav_eg75_he_ansi', 'brand': 'Weikav', 'model': 'EG75 HE',
              'variant': 'ANSI', 'name': 'Weikav EG75 HE ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'sparklink', 'products': []},
              'sources': [{'path': SOURCE.relative_to(ROOT).as_posix(), 'sha256': source_sha,
                           'url': EVIDENCE[0]['url']}],
              'notes': ['Key map from the official driver, physical geometry from the retail top view (80 keys), '
                        '42 px keys on a 44 px pitch.',
                        'SparkLink V2 1CA6:5A01; manual selection (SparkLink publishes no layout token).',
                        'Right of Space: driver map Fn1 and R-Ctrl (retail keycaps read ALT and FN).',
                        'Fn uses HallJoy code 0x409 (SparkLink 0xF101).']}
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
    print(f'WEIKAV_LAYOUTS=PASS models=1 keys={COUNT} sha256=' + hashlib.sha256(report).hexdigest())


if __name__ == '__main__':
    main()
