"""Logitech G RAPID ANSI layouts from reviewed transcriptions (2026-10-05).

PRO X TKL RAPID (046D:C35B): standard ANSI TKL (87 keys incl. Fn and Menu), as
listed by the RigDeck protocol notes, whose key ids 00..56 were matched against
HID key-downs on the device (docs/current/LOGITECH_RAPID_2026-10-05.md).

PRO X2 RAPID (046D:C364): the official top view (logitechg.com gallery image 1)
shows the same TKL with an OLED screen and roller in place of the third
navigation column: Pause, Page Up and Page Down are absent, 84 keys, matching
the official specification ("TKL design", 84 keys). The top-left key carries the
G logo in the Esc position. The image stays local
(.local/research/logitech-20261005/images/).

Every key here must exist in the analog key-id table
(src/HallJoyProject/HallJoy/logitech_rapid_protocol.h). No HID access or vendor
code execution.

Usage: python tools/build_logitech_layouts.py [--write]
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/logitech-layout-sources/logitech-rapid-geometry.json'
REPORTS = ROOT / 'docs/research/logitech-layout-reports'
PROTOCOL = ROOT / 'src/HallJoyProject/HallJoy/logitech_rapid_protocol.h'
FN = 1033

# (label, HID usage, x in units, width) per row; y in units per row.
TKL = [
    (0, [('Esc', 0x29, 0, 1), ('F1', 0x3a, 2, 1), ('F2', 0x3b, 3, 1), ('F3', 0x3c, 4, 1), ('F4', 0x3d, 5, 1),
         ('F5', 0x3e, 6.5, 1), ('F6', 0x3f, 7.5, 1), ('F7', 0x40, 8.5, 1), ('F8', 0x41, 9.5, 1),
         ('F9', 0x42, 11, 1), ('F10', 0x43, 12, 1), ('F11', 0x44, 13, 1), ('F12', 0x45, 14, 1),
         ('PrtSc', 0x46, 15.25, 1), ('ScrLk', 0x47, 16.25, 1), ('Pause', 0x48, 17.25, 1)]),
    (1.25, [('`', 0x35, 0, 1), ('1', 0x1e, 1, 1), ('2', 0x1f, 2, 1), ('3', 0x20, 3, 1), ('4', 0x21, 4, 1),
            ('5', 0x22, 5, 1), ('6', 0x23, 6, 1), ('7', 0x24, 7, 1), ('8', 0x25, 8, 1), ('9', 0x26, 9, 1),
            ('0', 0x27, 10, 1), ('-', 0x2d, 11, 1), ('=', 0x2e, 12, 1), ('Backspace', 0x2a, 13, 2),
            ('Insert', 0x49, 15.25, 1), ('Home', 0x4a, 16.25, 1), ('PgUp', 0x4b, 17.25, 1)]),
    (2.25, [('Tab', 0x2b, 0, 1.5), ('Q', 0x14, 1.5, 1), ('W', 0x1a, 2.5, 1), ('E', 0x08, 3.5, 1),
            ('R', 0x15, 4.5, 1), ('T', 0x17, 5.5, 1), ('Y', 0x1c, 6.5, 1), ('U', 0x18, 7.5, 1),
            ('I', 0x0c, 8.5, 1), ('O', 0x12, 9.5, 1), ('P', 0x13, 10.5, 1), ('[', 0x2f, 11.5, 1),
            (']', 0x30, 12.5, 1), ('\\', 0x31, 13.5, 1.5),
            ('Delete', 0x4c, 15.25, 1), ('End', 0x4d, 16.25, 1), ('PgDn', 0x4e, 17.25, 1)]),
    (3.25, [('Caps', 0x39, 0, 1.75), ('A', 0x04, 1.75, 1), ('S', 0x16, 2.75, 1), ('D', 0x07, 3.75, 1),
            ('F', 0x09, 4.75, 1), ('G', 0x0a, 5.75, 1), ('H', 0x0b, 6.75, 1), ('J', 0x0d, 7.75, 1),
            ('K', 0x0e, 8.75, 1), ('L', 0x0f, 9.75, 1), (';', 0x33, 10.75, 1), ("'", 0x34, 11.75, 1),
            ('Enter', 0x28, 12.75, 2.25)]),
    (4.25, [('Shift', 0xe1, 0, 2.25), ('Z', 0x1d, 2.25, 1), ('X', 0x1b, 3.25, 1), ('C', 0x06, 4.25, 1),
            ('V', 0x19, 5.25, 1), ('B', 0x05, 6.25, 1), ('N', 0x11, 7.25, 1), ('M', 0x10, 8.25, 1),
            (',', 0x36, 9.25, 1), ('.', 0x37, 10.25, 1), ('/', 0x38, 11.25, 1), ('Shift', 0xe5, 12.25, 2.75),
            ('Up', 0x52, 16.25, 1)]),
    (5.25, [('Ctrl', 0xe0, 0, 1.25), ('Win', 0xe3, 1.25, 1.25), ('Alt', 0xe2, 2.5, 1.25),
            ('Space', 0x2c, 3.75, 6.25), ('Alt', 0xe6, 10, 1.25), ('Fn', FN, 11.25, 1.25),
            ('Menu', 0x65, 12.5, 1.25), ('Ctrl', 0xe4, 13.75, 1.25),
            ('Left', 0x50, 15.25, 1), ('Down', 0x51, 16.25, 1), ('Right', 0x4f, 17.25, 1)]),
]
X2_ABSENT = {0x48, 0x4b, 0x4e}  # Pause, Page Up, Page Down: OLED and roller column
MODELS = [
    ('logitech_pro_x_tkl_rapid_ansi', 'PRO X TKL RAPID', 'PROXTKLRAPID-046D-C35B', set(), 87),
    ('logitech_pro_x2_rapid_ansi', 'PRO X2 RAPID', 'PROX2RAPID-046D-C364', X2_ABSENT, 84),
]
EVIDENCE = [
    {'url': 'https://github.com/gabriellaines/rigdeck/blob/HEAD/docs/protocols/logitech-pro-x-tkl-rapid.md',
     'what': 'PRO X TKL RAPID key ids 00-56 (ANSI TKL incl. Fn, Menu) matched to HID key-downs'},
    {'url': 'https://resource.logitechg.com/content/dam/gaming/en/products/pro-x2-rapid-pdp/gallery/pro-x2-rapid-black-gallery1.png',
     'sha256': '85114262a3376a18274189c0354d7d59e602dffd5f1abdb1dbdf9cb35bdf59c4',
     'what': 'Official PRO X2 RAPID top view (1800 px): TKL without Pause/PgUp/PgDn, OLED + roller column'},
    {'url': 'https://support.logi.com/hc/en-us/articles/42543842856855-Specification-PRO-X2-RAPID',
     'what': 'Official specification: TKL design, 84 keys'},
]
PITCH, SIZE = 44, 42


def protocol_usages():
    text = PROTOCOL.read_text(encoding='utf-8')
    body = text[text.index('const std::uint16_t ordered[] = {'):text.index('};', text.index('const std::uint16_t ordered[]'))]
    body = re.sub(r'//[^\n]*', '', body.split('{', 1)[1])
    values = [v.strip() for v in body.split(',') if v.strip()]
    return {FN if v == 'kFn' else int(v, 16) for v in values}


def source_bytes():
    data = {'schema': 1, 'models': [m[1] for m in MODELS], 'variant': 'ANSI', 'units': 'key units, 1u pitch',
            'evidence': EVIDENCE, 'rows': [[y, [list(k) for k in keys]] for y, keys in TKL],
            'absent': {m[1]: sorted(m[3]) for m in MODELS}}
    return (json.dumps(data, indent=1) + '\n').encode()


def report_bytes(model, source_sha, usages):
    symbol, name, product, absent, count = model
    keys = []
    for y, row in TKL:
        for label, hid, x, w in row:
            if hid in absent:
                continue
            assert hid in usages, (name, label)
            keys.append({'hid': hid, 'label': label, 'x': round(x * PITCH), 'y': round(y * PITCH),
                         'w': round(w * PITCH) - (PITCH - SIZE), 'h': SIZE})
    assert len(keys) == count and len({k['hid'] for k in keys}) == count, (name, len(keys))
    report = {'schema': 1, 'id': symbol, 'brand': 'Logitech G', 'model': name, 'variant': 'ANSI',
              'name': f'Logitech G {name} ANSI', 'status': 'ready', 'unresolved': [], 'keys': keys,
              'identity': {'protocol': 'logitech-rapid', 'products': [product]},
              'sources': [{'path': SOURCE.relative_to(ROOT).as_posix(), 'sha256': source_sha,
                           'url': EVIDENCE[1]['url'] if absent else EVIDENCE[0]['url']}],
              'notes': [f'{count} keys, standard ANSI TKL geometry' + (' without Pause/PgUp/PgDn.' if absent else '.'),
                        'Automatic selection by the verified HID++ analog session; Fn = 0x409.']}
    return (json.dumps(report, indent=1) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    source = source_bytes()
    sha = hashlib.sha256(source).hexdigest()
    usages = protocol_usages()
    outputs = [(SOURCE, source)] + [(REPORTS / f'{m[0]}.json', report_bytes(m, sha, usages)) for m in MODELS]
    for path, data in outputs:
        if args.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        elif path.read_bytes() != data:
            raise SystemExit('Out of date: ' + path.relative_to(ROOT).as_posix())
    print('LOGITECH_LAYOUTS=PASS models=2 keys=171 ' +
          ' '.join(f'{p.name}={hashlib.sha256(d).hexdigest()}' for p, d in outputs[1:]))


if __name__ == '__main__':
    main()
