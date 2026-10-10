"""Reviewed extraction for MonsGeek FUN60 Ultra TMR, board 2352 (USB 3151:5029).

Vendor files are read as data, never executed. Every input is SHA-256 pinned.
Outputs (LF, like their neighbours):
  docs/research/rongyuan-stream/monsgeek-app-models.json   admission evidence
  docs/research/rongyuan-stream/sources/fun60ultra-2352-geometry.js  geometry excerpt
  docs/research/rongyuan-layouts/ry_layout_2352.json       layout report
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
BOARD, VID, PID = 2352, 12625, 20521
NAME = 'ry5088_fun60ultra_8k_dm_dfn4'
PRODUCT = 'RY1B-2352'
INPUTS = {
    # MonsGeek web driver: catalog record and RY5088 device classes.
    'catalog': ('monsgeek-20260922/index.13c08899.js',
                'e88543d0138780139c5c8dbadeac4f3f860d728a5b3d1c39dbb430d494ccb96a',
                'https://app.monsgeek.com/js/index.13c08899.js'),
    'classes': ('monsgeek-20260922/a0c3a7d8.js',
                'c721ddc06480359ef8bc31908b7a80e724b0883d51e646274ccf47628c1774e6',
                'https://app.monsgeek.com/js/a0c3a7d8.js'),
    # Two RongYuan desktop clients: independent matrix and geometry copies.
    'cn_app': ('attackshark-pro/cn-app/resources/app/dist/static/js/main_c366b7f7.js',
               'cc1b0d471dab2d3f26d4be0aea445962be4da52ae1412d6935100af25acc1c53', ''),
    'iot': ('attackshark-pro/iot-main_68eaf5ce.js',
            '3708db6c84edce9ca8a2424d744a9ee8a1ef66a267f6f3eca019711d9bf3feca', ''),
    # Stock firmware v305 from the vendor update API (local only, not redistributable).
    'firmware': ('monsgeek-firmware-20261010/extract/2352_v305/decompressed.bin',
                 '56842a28', 'https://api2.rongyuan.tech:3816/download/fw_upgrade_file/2352_v305'),
}
EVIDENCE = ROOT / 'docs/research/rongyuan-stream/monsgeek-app-models.json'
GEOMETRY = ROOT / 'docs/research/rongyuan-stream/sources/fun60ultra-2352-geometry.js'
LAYOUT = ROOT / 'docs/research/rongyuan-layouts/ry_layout_2352.json'
# HallJoy labels; Fn is HallJoy's virtual Fn code 1033 (vendor 0x0A010000).
LABELS = {'Backspace': 'Back', 'Caps Lock': 'Caps', 'r_Shift': 'Shift', 'r_Alt': 'Alt',
          'r_Ctrl': 'Ctrl', '(space)': 'Space', 'fn': 'Fn', '(Application)': 'Menu'}
VENDOR_FN = 0x0A010000
HALLJOY_FN = 1033


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read(key):
    relative, pinned, _ = INPUTS[key]
    raw = (LOCAL / relative).read_bytes()
    if pinned and not sha(raw).startswith(pinned):
        raise SystemExit(f'pinned input changed: {relative}')
    return raw


def class_text(text, name):
    start = re.search(r'class ' + re.escape(name) + r' extends (\w+)\{', text)
    i, depth = start.end(), 1
    while depth:
        depth += (text[i] == '{') - (text[i] == '}')
        i += 1
    return start.group(1), text[start.start():i]


def literal(text, name):
    match = re.search(r'(?<![\w$])' + re.escape(name) + r'=(\[[0-9,]+\])', text)
    return json.loads(match.group(1))


def transpiled_matrix(text, loader):
    # Babel output: defineProperty(this,"defaultMatrix",<const>) inside the loader's class.
    var = re.search(re.escape(f'case"{NAME}":return new ') + r'([\w$]+)\(', text).group(1)
    if var != loader:
        raise SystemExit('loader changed')
    body = text[text.index(f'var {var}=function(e)'):][:6000]
    const = re.search(r'"defaultMatrix",([\w$]+)\)', body).group(1)
    return literal(text, const)


def geometry(text):
    var = re.search(re.escape(NAME) + r':\{layout:([\w$]+)\.layout,', text).group(1)
    start = re.search(r'(?<![\w$])' + re.escape(var) + r'=\{deltX:0,deltY:0,layout:\[', text).start() + len(var) + 1
    i, depth, quoted = start, 0, False
    while True:  # brace matching outside string literals (key names include [ ] { })
        c = text[i]
        if quoted:
            if c == '\\':
                i += 1
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if not depth:
                return text[start:i + 1]
        i += 1


def keys_of(excerpt):
    keys = []
    for item in re.findall(r'\{([^{}]*)\}', excerpt[excerpt.index('layout:['):]):
        fields = {k: json.loads(v) for k, v in re.findall(r'(\w+):("[^"]*"|-?\d+)', item)}
        keys.append(fields)
    return keys


def stream_fingerprint(raw):
    """Stock firmware chain: 0x1B sets the stream flag, the producer reads it and calls
    the report sender with 0x1B, the sender builds report ID 5 (same chain as boards
    2304 and 2398; see approved-aliases.json firmware-stream)."""
    import struct
    enable = re.search(rb'\x1b\x29\x04\xbf\xe0\x78.\xf8', raw, re.S).start()
    h1, h2 = struct.unpack_from('<HH', raw, enable + 6)
    flag = h2 & 0xfff
    sender = re.search(rb'\x05\x20\xdf\xf8..\x8c\xf8\x00\x00\x8c\xf8\x01.\x8c\xf8\x02.', raw, re.S).start()
    calls = [m.start() + 2 for m in re.finditer(rb'\x1b\x20.\xf0..', raw, re.S)]

    def target(off):
        a, b = struct.unpack_from('<HH', raw, off)
        s = (a >> 10) & 1
        imm = (s << 24) | ((1 - (((b >> 13) & 1) ^ s)) << 23) | ((1 - (((b >> 11) & 1) ^ s)) << 22) | ((a & 0x3ff) << 12) | ((b & 0x7ff) << 1)
        return off + 4 + (imm - (1 << 25) if s else imm)
    targets = {target(c) for c in calls}
    if len(targets) != 1 or not 0 < sender - next(iter(targets)) < 64:
        raise SystemExit('producer does not call the report-5 sender')
    reads = [o for o in range(calls[0] - 80, calls[0], 2)
             if (struct.unpack_from('<H', raw, o)[0] & 0xfff0) == 0xf890 and (struct.unpack_from('<H', raw, o + 2)[0] & 0xfff) == flag]
    if not reads:
        raise SystemExit('producer does not read the stream flag')
    return {'firmware_sha256': sha(raw), 'flag_offset': flag,
            'enable_offset': enable, 'enable_hex': raw[enable:enable + 10].hex(),
            'producer_offset': reads[0], 'producer_hex': raw[reads[0]:calls[0] + 4].hex(),
            'sender_function': next(iter(targets)), 'sender_offset': sender,
            'sender_hex': raw[sender:sender + 18].hex()}


def lf_json(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf8')


def build():
    catalog = read('catalog').decode('utf8')
    record = re.search(r'\{id:' + str(BOARD) + r',vid:' + str(VID) + r',pid:' + str(PID) + r',[^{}]*(?:\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\}[^{}]*)*\}', catalog).group(0)
    if f'name:"{NAME}"' not in record or 'magnetism:!0' not in record:
        raise SystemExit('catalog record changed')
    classes = read('classes').decode('utf8')
    loader = re.search(re.escape(f'case"{NAME}":return new ') + r'(\w+)\(', classes).group(1)
    parent, text = class_text(classes, loader)
    stripped = re.sub(r'default\w*Matrix=\[[0-9,]+\];?', '', text)
    if parent != 'k' or not re.fullmatch(r'class \w+ extends k\{\}', stripped):
        raise SystemExit('class is not data-only on stream parent k')
    matrix = json.loads(re.search(r'defaultMatrix=(\[[0-9,]+\])', text).group(1))
    cn = read('cn_app').decode('utf8', errors='replace')
    iot = read('iot').decode('utf8', errors='replace')
    copies = {'cn_app': transpiled_matrix(cn, 'AYa'), 'iot': transpiled_matrix(iot, 'yJa')}
    if any(copy != matrix for copy in copies.values()):
        raise SystemExit('vendor clients disagree on the factory matrix')
    excerpt = geometry(cn)
    if geometry(iot) != excerpt:
        raise SystemExit('vendor clients disagree on geometry')
    vendor = keys_of(excerpt)
    factory = []
    for slot in range(128):
        b = matrix[slot * 4:slot * 4 + 4]
        if b[0] == 0 and b[1] == 0 and b[2]:
            factory.append(b[2])
        elif b == [10, 1, 0, 0]:
            factory.append(HALLJOY_FN)
    rows = sorted({k['y'] for k in vendor}, reverse=True)
    left = min(k['x'] for k in vendor)
    keys = []
    for k in vendor:
        hid = HALLJOY_FN if k['value'] == VENDOR_FN else k['value']
        label = LABELS.get(k['keyName'], k['keyName'])
        # Vendor draws 30 px caps on a 46 px pitch; HallJoy draws 40 px caps on 46.
        keys.append({'hid': hid, 'label': label, 'x': k['x'] - left, 'y': rows.index(k['y']) * 46,
                     'w': k['w'] + 10, 'h': 40})
    if sorted(k['hid'] for k in keys) != sorted(factory) or len(keys) != 61:
        raise SystemExit('geometry key set differs from the factory matrix')
    fw = stream_fingerprint(read('firmware'))
    evidence = {'schema': 1, 'models': [{
        'board': BOARD, 'vid': VID, 'pid': PID, 'brand': 'MonsGeek', 'model': 'FUN60 Ultra TMR',
        'product': PRODUCT, 'name': NAME,
        'source_record': {'source': '.local/research/' + INPUTS['catalog'][0], 'sha256': INPUTS['catalog'][1],
                          'url': INPUTS['catalog'][2], 'record': record},
        'model_source': {'source': '.local/research/' + INPUTS['classes'][0], 'sha256': INPUTS['classes'][1],
                         'url': INPUTS['classes'][2], 'case': f'{NAME} -> {loader}'},
        'model_class': text,
        'matrix_sha256': sha(bytes(matrix)),
        'independent_copies': {'.local/research/' + INPUTS[k][0]: 'defaultMatrix equal' for k in copies},
        'firmware_local': '.local/research/' + INPUTS['firmware'][0],
        'firmware_url': INPUTS['firmware'][2],
        **fw,
        'note': ('Tester log 2026-10-10 (HallJoy 1.6.8): the tester\'s FUN60 Ultra TMR enumerates as '
                 '3151:5029 and answers 0x8F with board 2352. Official class extends the stream parent k '
                 'with data only; its matrix differs from FUN60 Pro 2600 in slots 59-83 (RAlt, Fn, Menu, '
                 'RCtrl one column left). Stock firmware v305 has the RY1B stream chain (0x1B flag, '
                 'producer, report-5 sender) at the same structure as 2304 and 2398.')}]}
    layout = {'schema': 1, 'id': 'ry_layout_2352', 'brand': 'MonsGeek', 'model': 'FUN60 Ultra TMR',
              'variant': 'ANSI', 'name': 'MonsGeek FUN60 Ultra TMR ANSI', 'status': 'ready', 'unresolved': [],
              'keys': keys, 'identity': {'protocol': 'rongyuan-stream', 'products': [PRODUCT]},
              'sources': [{'path': GEOMETRY.relative_to(ROOT).as_posix(), 'sha256': sha(excerpt.encode() + b'\n')}],
              'models': ['FUN60 Ultra TMR'], 'boards': [BOARD],
              'notes': ['Geometry from the RongYuan client layout object for ' + NAME +
                        ' (identical in the CN and IOT clients); 30 px vendor caps on a 46 px pitch drawn as 40 px caps.',
                        'Key set equals the official factory matrix (61 keys). No visual or physical test claimed.']}
    return {EVIDENCE: lf_json(evidence), GEOMETRY: excerpt.encode() + b'\n', LAYOUT: lf_json(layout)}, matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--print-matrix', action='store_true')
    args = parser.parse_args()
    outputs, matrix = build()
    if args.print_matrix:
        print(','.join(map(str, matrix)))
        return 0
    changed = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        if changed:
            print('FUN60_ULTRA_2352_EXTRACTION=FAIL changed=' + ','.join(p.name for p in changed))
            return 1
    else:
        for path in changed:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(outputs[path])
    print(f'FUN60_ULTRA_2352_EXTRACTION=PASS keys=61 written={0 if args.check else len(changed)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
