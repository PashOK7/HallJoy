"""Reviewed extraction for IROK Mars68 Pro, board 2438 (USB 3151:5029).

Tester log 2026-10-20 (HallJoy 1.6.11, "irok mars68 pro", USB product string
"CAROTMAS Mars68 PRO"): the keyboard answers 0x8F with board 2438 on 3151:5029.
Vendor files are read as data, never executed. Every input is SHA-256 pinned.
Outputs (LF):
  docs/research/rongyuan-stream/rongyuan-client-models.json   admission evidence
  docs/research/rongyuan-stream/sources/mars68pro-2438-geometry.js  geometry excerpt
  docs/research/rongyuan-layouts/ry_layout_2438.json            layout report
Run with --check to verify that committed outputs equal a fresh extraction.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / '.local/research'
BOARD, VID, PID = 2438, 12625, 20521
NAME = 'ry5088_gk8157gpro_8k_dm_spaceled'
PRODUCT = 'RY1B-2438'
INPUTS = {
    # RongYuan desktop clients (local copies, same files as the 2352 extraction).
    'cn_app': ('attackshark-pro/cn-app/resources/app/dist/static/js/main_c366b7f7.js',
               'cc1b0d471dab2d3f26d4be0aea445962be4da52ae1412d6935100af25acc1c53'),
    'iot': ('attackshark-pro/iot-main_68eaf5ce.js',
            '3708db6c84edce9ca8a2424d744a9ee8a1ef66a267f6f3eca019711d9bf3feca'),
}
EVIDENCE = ROOT / 'docs/research/rongyuan-stream/rongyuan-client-models.json'
GEOMETRY = ROOT / 'docs/research/rongyuan-stream/sources/mars68pro-2438-geometry.js'
LAYOUT = ROOT / 'docs/research/rongyuan-layouts/ry_layout_2438.json'
# Reviewed stream parent: the CN client class of FUN60 Ultra TMR 2352 (AYa -> zda -> Epa),
# a tester-confirmed RY1B stream board, descends from the same parent Epa.
PARENT = 'Epa'
REFERENCE = ('ry5088_fun60ultra_8k_dm_dfn4', 'AYa', 'zda')
LABELS = {'BackSpace': 'Back', 'Caps Lock': 'Caps', 'r_Shift': 'Shift', 'r_Alt': 'Alt',
          'r_Ctrl': 'Ctrl', '(space)': 'Space', 'fn': 'Fn', 'Insert': 'Ins', 'Delete': 'Del',
          'Page\nUp': 'PgUp', 'Page\nDown': 'PgDn', '↑': 'Up', '↓': 'Down', '←': 'Left', '→': 'Right'}
VENDOR_FN = 0x0A010000
HALLJOY_FN = 1033
# Babel class with a constructor that only assigns the default matrices; `r` and `a`
# (prototype and static method tables) are never assigned, so it declares no methods.
CLASS = re.compile(
    r'var (?P<var>[\w$]+)=function\(e\)\{!function\(e,t\)\{if\("function"!=typeof t&&null!==t\)throw new TypeError\('
    r'"Super expression must either be null or a function"\);e\.prototype=Object\.create\(t&&t\.prototype,\{constructor:'
    r'\{value:e,writable:!0,configurable:!0\}\}\),Object\.defineProperty\(e,"prototype",\{writable:!1\}\),t&&[\w$]+\(e,t\)'
    r'\}\(o,e\);var t,r,a,n=[\w$]+\(o\);function o\(\)\{var e;!function\(e,t\)\{if\(!\(e instanceof t\)\)throw new '
    r'TypeError\("Cannot call a class as a function"\)\}\(this,o\);for\(var t=arguments\.length,r=new Array\(t\),a=0;'
    r'a<t;a\+\+\)r\[a\]=arguments\[a\];return (?P<assign>(?:[\w$]+\([\w$]+\((?:e=n\.call\.apply\(n,\[this\]\.concat\(r\)\)|e)\),'
    r'"default\w*Matrix",[\w$]+\),)+)e\}return t=o,r&&[\w$]+\(t\.prototype,r\),a&&[\w$]+\(t,a\),Object\.defineProperty\(t,'
    r'"prototype",\{writable:!1\}\),t\}\((?P<parent>[\w$]+)\)')


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read(key):
    relative, pinned = INPUTS[key]
    raw = (LOCAL / relative).read_bytes()
    if sha(raw) != pinned:
        raise SystemExit(f'pinned input changed: {relative}')
    return raw.decode('utf8', errors='replace')


def balanced(text, start, opening='{', closing='}'):
    depth, quoted, i = 0, False, start
    while True:
        c = text[i]
        if quoted:
            if c == '\\':
                i += 1
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == opening:
            depth += 1
        elif c == closing:
            depth -= 1
            if not depth:
                return text[start:i + 1]
        i += 1


def record(text):
    starts = [m.start() for m in re.finditer(r'\{id:' + str(BOARD) + r',vid:' + str(VID) + r',pid:' + str(PID) + ',', text)]
    if len(starts) != 1:
        raise SystemExit('registry record not unique')
    rec = balanced(text, starts[0])
    if f'name:"{NAME}"' not in rec or 'magnetism:!0' not in rec:
        raise SystemExit('registry record changed')
    return rec


def model_class(text, name):
    loader = re.search(re.escape(f'case"{name}":return new ') + r'([\w$]+)\(', text).group(1)
    start = text.index(f'var {loader}=function(e)')
    match = CLASS.match(text, start)
    if not match or match.group('var') != loader:
        raise SystemExit(f'{name}: class is not a data-only Babel class')
    names = re.findall(r'"(default\w*Matrix)",([\w$]+)\)', match.group('assign'))
    matrices = {}
    for prop, const in names:
        literal = re.search(r'(?<![\w$])' + re.escape(const) + r'=(\[[0-9,]+\])', text)
        matrices[prop] = json.loads(literal.group(1))
    return loader, match.group('parent'), match.group(0), matrices


def parent_of(text, var):
    start = text.index(f'var {var}=function(e)')
    return re.search(r'\}\(([\w$]+)\)', text[start:start + 8000]).group(1)


def geometry(text):
    var = re.search(re.escape(NAME) + r':\{layout:([\w$]+)\.layout,', text).group(1)
    start = re.search(r'(?<![\w$])' + re.escape(var) + r'=\{deltX:0,deltY:0,layout:\[', text).start() + len(var) + 1
    return balanced(text, start)


def keys_of(excerpt):
    keys = []
    for item in re.findall(r'\{([^{}]*)\}', excerpt[excerpt.index('layout:['):]):
        keys.append({k: json.loads(v) for k, v in re.findall(r'(\w+):("(?:[^"\\]|\\.)*"|-?\d+)', item)})
    return keys


def lf_json(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf8')


def build():
    cn, iot = read('cn_app'), read('iot')
    cn_record, iot_record = record(cn), record(iot)
    loader, parent, text, matrices = model_class(cn, NAME)
    if parent != PARENT or set(matrices) != {'defaultMatrix', 'defaultFnMatrix'}:
        raise SystemExit('unexpected parent or class data')
    ref_loader, ref_parent, _, _ = model_class(cn, REFERENCE[0])
    if (ref_loader, ref_parent) != REFERENCE[1:] or parent_of(cn, ref_parent) != PARENT:
        raise SystemExit('reference stream parent changed')
    matrix = matrices['defaultMatrix']
    if len(matrix) != 512:
        raise SystemExit('matrix size')
    excerpt = geometry(cn)
    vendor = keys_of(excerpt)
    factory = []
    for slot in range(128):
        b = matrix[slot * 4:slot * 4 + 4]
        if b[0] == 0 and b[1] == 0 and b[2]:
            factory.append(b[2])
        elif b == [10, 1, 0, 0]:
            factory.append(HALLJOY_FN)
    # Vendor y decreases downwards (same as the 2352 object): top row first.
    rows = sorted({k['y'] for k in vendor}, reverse=True)
    left = min(k['x'] for k in vendor)
    # This object draws 40 px caps on a ~53.4 px pitch: pitch from the number row
    # (1 .. =, eleven 1u steps), gap = pitch - cap.
    number_row = {k['value']: k['x'] for k in vendor if k['y'] == rows[0]}
    pitch = (number_row[46] - number_row[30]) / 11
    cap = next(k['w'] for k in vendor if k['value'] == 30)
    keys = []
    for k in vendor:
        hid = HALLJOY_FN if k['value'] == VENDOR_FN else k['value']
        label = LABELS.get(k['keyName'], k['keyName'])
        units = (k['w'] + pitch - cap) / pitch
        # HallJoy grid: 46 px pitch, 40 px caps (a key of u units is u*46-6 wide).
        keys.append({'hid': hid, 'label': label, 'x': round((k['x'] - left) * 46 / pitch),
                     'y': rows.index(k['y']) * 46, 'w': round(units * 46 - 6), 'h': 40})
    if sorted(k['hid'] for k in keys) != sorted(factory) or len(keys) != 68:
        raise SystemExit('geometry key set differs from the factory matrix')
    evidence = {'schema': 1, 'models': [{
        'board': BOARD, 'vid': VID, 'pid': PID, 'brand': 'IROK', 'model': 'Mars68 Pro',
        'product': PRODUCT, 'name': NAME,
        'source_record': {'source': '.local/research/' + INPUTS['cn_app'][0], 'sha256': INPUTS['cn_app'][1],
                          'record': cn_record},
        'iot_record': {'source': '.local/research/' + INPUTS['iot'][0], 'sha256': INPUTS['iot'][1],
                       'record': iot_record},
        'model_source': {'source': '.local/research/' + INPUTS['cn_app'][0], 'sha256': INPUTS['cn_app'][1],
                         'case': f'{NAME} -> {loader}', 'parent': parent,
                         'reference': f'{REFERENCE[0]} -> {REFERENCE[1]} -> {REFERENCE[2]} -> {PARENT}'},
        'model_class': text,
        'matrix': matrix,
        'matrix_sha256': sha(bytes(matrix)),
        'note': ('Tester log 2026-10-20 (HallJoy 1.6.11): the tester\'s IROK Mars68 Pro (USB product '
                 '"CAROTMAS Mars68 PRO") enumerates as 3151:5029 and answers 0x8F with board 2438. The '
                 'RongYuan CN client class is a data-only Babel class on the stream parent Epa, the same '
                 'parent as the tester-confirmed FUN60 Ultra TMR 2352 class. No stock firmware checked.')}]}
    layout = {'schema': 1, 'id': 'ry_layout_2438', 'brand': 'IROK', 'model': 'Mars68 Pro',
              'variant': 'ANSI', 'name': 'IROK Mars68 Pro ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'rongyuan-stream', 'products': [PRODUCT]},
              'sources': [{'path': GEOMETRY.relative_to(ROOT).as_posix(), 'sha256': sha(excerpt.encode() + b'\n')}],
              'models': ['Mars68 Pro'], 'boards': [BOARD],
              'notes': ['Geometry from the RongYuan CN client layout object for ' + NAME +
                        '; 30 px vendor caps on a 46 px pitch drawn as 40 px caps.',
                        'Key set equals the official factory matrix (68 keys). No visual or physical test claimed.']}
    return {EVIDENCE: lf_json(evidence), GEOMETRY: excerpt.encode() + b'\n', LAYOUT: lf_json(layout)}, matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    outputs, _ = build()
    changed = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        if changed:
            print('MARS68PRO_2438_EXTRACTION=FAIL changed=' + ','.join(p.name for p in changed))
            return 1
    else:
        for path in changed:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(outputs[path])
    print(f'MARS68PRO_2438_EXTRACTION=PASS keys=68 written={0 if args.check else len(changed)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
