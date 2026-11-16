"""Everglide SU75 Pro ANSI layout from a reviewed transcription (2026-10-02).

Geometry source: the official SparkLink PlayJoy driver (xsyd.top) screenshot of
a connected "Su75PRO", published on keebforce.com; the retail photo confirms
the same 81 keys (the knob right of Up is not a key). The images stay local
(.local/research/everglide-20261002/layout/); the unit transcription below is
the reviewed evidence. The driver itself reads geometry from the keyboard, so
no static vendor file exists. No HID access or vendor code execution.

Usage: python tools/build_everglide_layouts.py [--write]
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/everglide-layout-sources/su75-pro-geometry.json'
REPORT = ROOT / 'docs/research/everglide-layout-reports/everglide_su75_pro_ansi.json'
REVISION_SOURCE = ROOT / 'docs/research/everglide-layout-sources/su75-revision-identities.json'
V2_REPORT = REPORT.with_name("everglide_su75_pro_v2_ansi.json")

# (label, HID usage, x in units, width in units) per row, from the screenshot.
ROWS = [
    [('Esc', 41, 0, 1), ('F1', 58, 1.25, 1), ('F2', 59, 2.25, 1), ('F3', 60, 3.25, 1), ('F4', 61, 4.25, 1),
     ('F5', 62, 5.5, 1), ('F6', 63, 6.5, 1), ('F7', 64, 7.5, 1), ('F8', 65, 8.5, 1),
     ('F9', 66, 9.75, 1), ('F10', 67, 10.75, 1), ('F11', 68, 11.75, 1), ('F12', 69, 12.75, 1),
     ('Delete', 76, 14, 1), ('Home', 74, 15, 1)],
    [('`', 53, 0, 1), ('1', 30, 1, 1), ('2', 31, 2, 1), ('3', 32, 3, 1), ('4', 33, 4, 1), ('5', 34, 5, 1),
     ('6', 35, 6, 1), ('7', 36, 7, 1), ('8', 37, 8, 1), ('9', 38, 9, 1), ('0', 39, 10, 1), ('-', 45, 11, 1),
     ('=', 46, 12, 1), ('Backspace', 42, 13, 2), ('End', 77, 15, 1)],
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
    {'url': 'https://keebforce.com/wp-content/uploads/2025/08/su75-pro-web-software.webp',
     'sha256': 'cf04a7fb49b8119576426d910a4d58ed46696893480ac8eb2e54576e6eb19ea6',
     'what': 'Official SparkLink PlayJoy driver, device "Su75PRO" connected, full key map'},
    {'url': 'https://keebforce.com/wp-content/uploads/2025/08/su75-pro-front.webp',
     'sha256': 'c9fe6f1b51d273ae51e50af8282f923084c1b076416cf612e2bf89ff0937f9ec',
     'what': 'Retail top view: same 81 keys, knob right of Up, no right Alt'},
]
PITCH, SIZE, ROW_Y = 44, 42, (0, 54, 98, 142, 186, 230)


def source_bytes():
    data = {'schema': 1, 'model': 'Everglide SU75 Pro', 'variant': 'ANSI', 'units': 'key units, 1u pitch',
            'evidence': EVIDENCE, 'rows': [[list(k) for k in row] for row in ROWS]}
    return (json.dumps(data, indent=1) + '\n').encode()


def report_bytes(source_sha):
    keys = []
    for row, y in zip(ROWS, ROW_Y):
        for label, hid, x, w in row:
            keys.append({'hid': hid, 'label': label, 'x': round(x * PITCH), 'y': y,
                         'w': round(w * PITCH) - (PITCH - SIZE), 'h': SIZE})
    assert len(keys) == 81 and len({k['hid'] for k in keys}) == 81
    report = {'schema': 1, 'id': 'everglide_su75_pro_ansi', 'brand': 'Everglide', 'model': 'SU75 Pro',
              'variant': 'ANSI', 'name': 'Everglide SU75 Pro ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'sparklink', 'products': ['SU75PRO-3002-00300002-ANSI']},
              'sources': [{'path': SOURCE.relative_to(ROOT).as_posix(), 'sha256': source_sha,
                           'url': EVIDENCE[0]['url']}],
              'notes': ['Transcribed from the official driver key map (81 keys), 42 px keys on a 44 px pitch.',
                        'SparkLink V2 1CA6:3002 / board00300002; automatic selection after live analog proof.',
                        'Fn uses HallJoy code 0x409 (SparkLink 0xF101).']}
    return (json.dumps(report, indent=1) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    source = source_bytes()
    report = report_bytes(hashlib.sha256(source).hexdigest())
    import copy
    revisions = json.loads(REVISION_SOURCE.read_bytes())
    assert len(revisions['versions']) == 9
    factory = [[1033 if x == 0xf101 else x for x in row if x] for row in revisions['matrix']]
    assert factory == [[key[1] for key in row] for row in ROWS]
    first = json.loads(report)
    second = copy.deepcopy(first)
    second.update(id='everglide_su75_pro_v2_ansi', model='SU75 Pro V2 / SU75 Ultra', name='Everglide SU75 Pro V2 (SU75 Ultra) ANSI')
    second['identity']['products'] = ['SU75ULTRA-3007-00300007-ANSI']
    second['sources'].append({'path': REVISION_SOURCE.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(REVISION_SOURCE.read_bytes()).hexdigest(), 'url': revisions['pcb_source']})
    second['notes'] = ['Exact XS125 1CA6:3007 / board00300007; automatic selection after live analog proof.', 'All 81 factory keys and physical arrangement match the SU75 Pro ANSI transcription.', 'Fn uses HallJoy code 0x409 (SparkLink 0xF101).']
    v2 = (json.dumps(second, indent=1) + '\n').encode()
    for path, data in ((SOURCE, source), (REPORT, report), (V2_REPORT, v2)):
        if args.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        elif path.read_bytes() != data:
            raise SystemExit('Out of date: ' + path.relative_to(ROOT).as_posix())
    print('EVERGLIDE_LAYOUTS=PASS models=2 keys=162 sha256=' + hashlib.sha256(report).hexdigest())


if __name__ == '__main__':
    main()
