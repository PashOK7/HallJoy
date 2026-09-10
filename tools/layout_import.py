"""Offline, fail-closed Keychron Launcher -> HallJoy layout preparation.

No downloaded JavaScript/C is executed. No HID access, firmware writes or user
settings changes. Unknown actions produce a review report, never guessed HIDs.
"""
from __future__ import annotations

import argparse
from decimal import Decimal, ROUND_HALF_UP
import hashlib
import json
from pathlib import Path
import re

MAX_SOURCE_BYTES = 4 * 1024 * 1024
MAX_KEYS = 4096


class ImportErrorDetail(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ImportErrorDetail(message)


def integer(value, low, high, field):
    require(type(value) is int and low <= value <= high, f'{field}: expected integer {low}..{high}')
    return value


def number(value, field):
    require(type(value) in (int, float, Decimal), f'{field}: expected number')
    result = Decimal(str(value))
    require(result.is_finite() and abs(result) <= 10000, f'{field}: invalid number')
    return result


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f'Duplicate JSON field: {key}')
        result[key] = value
    return result


def read_source(path):
    with Path(path).open('rb') as stream:
        raw = stream.read(MAX_SOURCE_BYTES + 1)
    require(len(raw) <= MAX_SOURCE_BYTES, f'Source too large: {path}')
    return raw.decode('utf-8-sig'), hashlib.sha256(raw).hexdigest()


def read_json(path):
    text, digest = read_source(path)
    return json.loads(text, object_pairs_hook=unique_object,
                      parse_constant=lambda value: (_ for _ in ()).throw(ImportErrorDetail(f'Invalid number: {value}'))), digest


def qmk_codes():
    result = {}
    for i, letter in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ', 4):
        result['KC_' + letter] = (i, letter)
    for i, digit in enumerate('1234567890', 30):
        result['KC_' + digit] = (i, digit)
    for i in range(1, 25):
        result[f'KC_F{i}'] = (57 + i if i <= 12 else 91 + i, f'F{i}')
    definitions = [
        (40,'Enter','ENT ENTER'), (41,'Esc','ESC ESCAPE'), (42,'Back','BSPC BACKSPACE'),
        (43,'Tab','TAB'), (44,'Space','SPC SPACE'), (45,'-','MINS MINUS'), (46,'=','EQL EQUAL'),
        (47,'[','LBRC LEFT_BRACKET'), (48,']','RBRC RIGHT_BRACKET'), (49,'\\','BSLS BACKSLASH'),
        (50,'#','NUHS NONUS_HASH'), (51,';','SCLN SEMICOLON'), (52,"'",'QUOT QUOTE'),
        (53,'`','GRV GRAVE'), (54,',','COMM COMMA'), (55,'.','DOT'), (56,'/','SLSH SLASH'),
        (57,'Caps','CAPS CAPS_LOCK'), (70,'PrtSc','PSCR PRINT_SCREEN'), (71,'ScrLk','SCRL SCROLL_LOCK'),
        (72,'Pause','PAUS PAUSE'), (73,'Ins','INS INSERT'), (74,'Home','HOME'), (75,'PgUp','PGUP PAGE_UP'),
        (76,'Del','DEL DELETE'), (77,'End','END'), (78,'PgDn','PGDN PAGE_DOWN'),
        (79,'Right','RGHT RIGHT'), (80,'Left','LEFT'), (81,'Down','DOWN'), (82,'Up','UP'),
        (83,'Num','NUM NUM_LOCK'), (84,'/','PSLS KP_SLASH'), (85,'*','PAST KP_ASTERISK'),
        (86,'-','PMNS KP_MINUS'), (87,'+','PPLS KP_PLUS'), (88,'NEnt','PENT KP_ENTER'),
        (98,'0','P0 KP_0'), (99,'.','PDOT KP_DOT'), (100,'\\','NUBS NONUS_BACKSLASH'),
        (101,'Menu','APP APPLICATION'), (103,'=','PEQL KP_EQUAL'),
        (224,'Ctrl','LCTL LEFT_CTRL'), (225,'Shift','LSFT LEFT_SHIFT'),
        (226,'Alt','LALT LEFT_ALT'), (227,'Win','LGUI LWIN LEFT_GUI'),
        (228,'Ctrl','RCTL RIGHT_CTRL'), (229,'Shift','RSFT RIGHT_SHIFT'),
        (230,'Alt','RALT RIGHT_ALT'), (231,'Win','RGUI RWIN RIGHT_GUI'),
    ]
    for i in range(1, 10):
        definitions.append((88+i, str(i), f'P{i} KP_{i}'))
        definitions.append((134+i, {1:'Ro',3:'Yen',4:'Henkan',5:'Muhenkan'}.get(i,f'Intl{i}'), f'INT{i} INTERNATIONAL_{i}'))
        definitions.append((143+i, f'Lang{i}', f'LNG{i} LANGUAGE_{i}'))
    for hid, label, aliases in definitions:
        for alias in aliases.split():
            result['KC_' + alias] = (hid, label)
    return result


CODES = qmk_codes()


def factory_map(info, code, layer):
    require(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', layer), 'Invalid layer name')
    # Intentionally supports simple factory token lists only, not a C evaluator.
    stripped = re.sub(r'/\*.*?\*/|//[^\r\n]*', '', code, flags=re.S)
    matches = list(re.finditer(r'\[' + re.escape(layer) + r'\]\s*=\s*(LAYOUT\w*)\s*\(', stripped))
    require(len(matches) == 1, 'Expected exactly one selected factory layer')
    match = matches[0]
    depth, end = 1, match.end()
    while end < len(stripped) and depth:
        depth += (stripped[end] == '(') - (stripped[end] == ')')
        end += 1
    require(depth == 0, 'Unterminated factory layout')
    end -= 1
    tokens = [part.strip() for part in stripped[match.end():end].split(',')]
    require(all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*(?:\([A-Za-z_][A-Za-z_0-9]*\))?', token) for token in tokens),
            'Factory layer contains expressions; provide a reviewed simple token map instead')
    layout = info.get('layouts', {}).get(match[1], {}).get('layout')
    require(isinstance(layout, list) and 0 < len(layout) <= MAX_KEYS, 'Missing QMK layout')
    require(len(tokens) == len(layout), 'QMK factory token / geometry count mismatch')
    result = {}
    for key, token in zip(layout, tokens):
        matrix = key.get('matrix')
        require(isinstance(matrix, list) and len(matrix) == 2, 'Invalid QMK matrix coordinate')
        row, col = (integer(v, 0, 255, 'matrix') for v in matrix)
        require((row,col) not in result, 'Duplicate QMK matrix coordinate')
        result[row,col] = token
    return result


def label_text(value):
    require(isinstance(value, str) and 0 < len(value) <= 100, 'Label must be 1..100 characters')
    require(not any(ord(char) < 32 or 0xD800 <= ord(char) <= 0xDFFF for char in value), 'Control character in label')
    return value


def convert(definition, info, code, layer, overrides=None, pitch_x=48, pitch_y=46, gap=6):
    require(isinstance(definition, dict) and isinstance(info, dict), 'Expected JSON objects')
    name = label_text(definition.get('name'))
    vpid = integer(definition.get('vendorProductId'), 1, 0xFFFFFFFF, 'vendorProductId')
    usb = info.get('usb', {})
    require('pid' in usb, 'QMK identity has no PID; identity must be verified')
    require(int(str(usb['pid']), 0) == (vpid & 65535), 'QMK / Launcher PID mismatch')
    if 'vid' in usb:
        require(int(str(usb['vid']), 0) == (vpid >> 16), 'QMK / Launcher VID mismatch')
    px, py, gap = (number(v, n) for v,n in [(pitch_x,'pitch_x'),(pitch_y,'pitch_y'),(gap,'gap')])
    require(18 <= px <= 600 and 18 <= py <= 600 and 0 <= gap < min(px, py), 'Invalid scale or gap')
    layouts = definition.get('layouts', {})
    require(not layouts.get('optionKeys'), 'Selectable layout variants require explicit extraction; refusing to merge variants')
    keys = layouts.get('keys')
    require(isinstance(keys, list) and 0 < len(keys) <= MAX_KEYS, 'Missing / excessive layout keys')
    factory = factory_map(info, code, layer)
    overrides = {} if overrides is None else overrides
    require(isinstance(overrides, dict), 'Overrides must be an object keyed by matrix row,col')
    matrix_shape = definition.get('matrix', {})
    matrix_rows = integer(matrix_shape.get('rows'), 1, 256, 'matrix rows')
    matrix_cols = integer(matrix_shape.get('cols'), 1, 256, 'matrix cols')
    parsed, seen = [], set()
    for key in keys:
        require(isinstance(key, dict), 'Key must be an object')
        row = integer(key.get('row'), 0, matrix_rows-1, 'row')
        col = integer(key.get('col'), 0, matrix_cols-1, 'col')
        require((row,col) not in seen, f'Duplicate Launcher matrix coordinate {row},{col}')
        seen.add((row,col))
        require(not key.get('d') and not key.get('g'), 'Decals / ghost keys need explicit review')
        require(number(key.get('r', 0), 'r') == 0, 'Rotated keys are not representable in HallJoy')
        # A rotation origin is inert at zero degrees; Launcher keeps it on Q2.
        for prop in ('rx', 'ry'):
            number(key.get(prop, 0), prop)
        x,y,w,h = (number(key.get(prop, 1 if prop in ('w','h') else None),prop) for prop in ('x','y','w','h'))
        require(w > 0 and h > 0, 'Key dimensions must be positive')
        notch_w = notch_y = Decimal(0)
        if any(prop in key for prop in ('x2','y2','w2','h2')):
            x2,y2,w2,h2 = (number(key.get(prop, default), prop) for prop,default in
                           [('x2',0),('y2',0),('w2',w),('h2',h)])
            require(x2 < 0 and y2 == 0 and w2 == w-x2 and 0 < h2 < h,
                    'Unsupported compound outline; expected an ISO/JIS top arm and right leg')
            notch_w,notch_y = -x2,h2
            x,w = x+x2,w-x2
        parsed.append((row,col,x,y,w,h,notch_w,notch_y))
    require(seen == set(factory), 'Launcher / QMK matrix sets differ; wrong variant or missing keys')
    require(set(overrides) <= {f'{r},{c}' for r,c in seen}, 'Override references an absent key')
    min_x, min_y = min(k[2] for k in parsed), min(k[3] for k in parsed)
    rounded = lambda value: int(value.quantize(Decimal('1'), rounding=ROUND_HALF_UP))
    result, unresolved, warnings, assigned, excluded = [], [], [], {}, []
    if 'vid' not in usb:
        warnings.append('QMK child info has no VID; PID and full matrix checked, verify source repository identity.')
    for row,col,x,y,w,h,notch_w,notch_y in parsed:
        token = factory[row,col]
        override = overrides.get(f'{row},{col}')
        if override is not None:
            require(isinstance(override, dict), 'Override must be an object')
            require(override.get('token') == token and override.get('vendorProductId') == vpid,
                    'Override model/token mismatch; re-review for this device')
            reason = label_text(override.get('reason'))
            if override.get('exclude') is True:
                require('hid' not in override, 'Excluded key cannot have a HID')
                excluded.append(dict(matrix=[row,col], token=token, reason=reason))
                continue
            hid = integer(override.get('hid'), 1, 65535, 'override HID')
            label = label_text(override.get('label'))
            warnings.append(f'{row},{col}: {token} -> {hid} ({reason})')
        elif token in CODES:
            hid, label = CODES[token]
        else:
            hid, label = None, token
            unresolved.append({'matrix':[row,col], 'token':token, 'reason':'No verified HallJoy key code'})
        left, top = rounded((x-min_x)*px), rounded((y-min_y)*py)
        width = rounded((x+w-min_x)*px-gap)-left
        height = rounded((y+h-min_y)*py-gap)-top
        require(0 <= left <= 4000 and 0 <= top <= 4000, 'Geometry exceeds HallJoy coordinate limits')
        require(18 <= width <= 600 and 18 <= height <= 600, 'Geometry exceeds HallJoy key size limits')
        if hid is not None:
            require(hid not in assigned, f'Duplicate HID {hid}: {assigned.get(hid)} and {(row,col)}; review required')
            assigned[hid] = (row,col)
        result.append(dict(matrix=[row,col], token=token, hid=hid, label=label,
                           x=left, y=top, w=width, h=height))
        if notch_w:
            nw = rounded((x+notch_w-min_x)*px)-left
            ny = rounded((y+notch_y-min_y)*py-gap)-top
            require(0 < nw < width and 0 < ny < height, 'Invalid compound outline after rounding')
            result[-1].update(notchW=nw, notchY=ny)
    def rectangles(key):
        x,y,w,h = (key[n] for n in ('x','y','w','h'))
        if key.get('notchW'):
            return [(x,y,w,key['notchY']), (x+key['notchW'],y,w-key['notchW'],h)]
        return [(x,y,w,h)]
    for i,a in enumerate(result):
        for b in result[i+1:]:
            require(not any(max(ax,bx) < min(ax+aw,bx+bw) and max(ay,by) < min(ay+ah,by+bh)
                            for ax,ay,aw,ah in rectangles(a) for bx,by,bw,bh in rectangles(b)),
                    f'Overlapping keys: {a["matrix"]} / {b["matrix"]}')
    require(result, 'No remaining keys')
    return dict(schema=1, name=name, vendorProductId=vpid, layer=layer,
                scale=dict(pitch_x=str(px), pitch_y=str(py), gap=str(gap)),
                keys=result, unresolved=unresolved, warnings=warnings, excluded=excluded,
                status='needs-review' if unresolved else 'ready')


def ini_text(report):
    require(report['status'] == 'ready' and not report['unresolved'], 'Unresolved keys; INI export blocked')
    lines = ['[HallJoyPersistence]', 'SchemaVersion=1', 'Kind=LayoutPreset', '',
             '[LayoutPreset]', 'Brand='+label_text(report.get('brand','Keychron')), f'Count={len(report["keys"])}', 'UniformSpacing=0',
             'UniformGap=6', 'BuiltinGeometryRevision=0']
    for i,key in enumerate(report['keys']):
        label = key['label'].replace('\\', '\\\\').replace('|', '\\|')
        row = min(20, key['y']//46)
        lines += [f'K{i}={key["hid"]}|{row}|{key["x"]}|{key["w"]}|{key["h"]}|{label}', f'Y{i}={key["y"]}']
        if key.get('notchW'):
            lines += [f'NotchW{i}={key["notchW"]}', f'NotchY{i}={key["notchY"]}']
    return '\r\n'.join(lines)+'\r\n'


def export(report, output_dir):
    # Exclusive directory creation and exclusive file modes prevent overwrites.
    output_dir = Path(output_dir)
    payload = ini_text(report) if report['status'] == 'ready' else None
    output_dir.mkdir(parents=True, exist_ok=False)
    with (output_dir/'review.json').open('x', encoding='utf-8') as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    if payload is not None:
        with (output_dir/'Imported layout.ini').open('x', encoding='utf-16', newline='') as stream:
            stream.write(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launcher', required=True, type=Path)
    parser.add_argument('--qmk-info', required=True, type=Path)
    parser.add_argument('--qmk-keymap', required=True, type=Path)
    parser.add_argument('--layer', required=True, help='Explicit factory layer, e.g. WIN_BASE')
    parser.add_argument('--overrides', type=Path)
    parser.add_argument('--output-dir', required=True, type=Path, help='NEW directory; never overwrites')
    parser.add_argument('--pitch-x', type=int, default=48)
    parser.add_argument('--pitch-y', type=int, default=46)
    parser.add_argument('--gap', type=int, default=6)
    args = parser.parse_args()
    try:
        definition, launcher_hash = read_json(args.launcher)
        info, info_hash = read_json(args.qmk_info)
        code, code_hash = read_source(args.qmk_keymap)
        overrides, overrides_hash = read_json(args.overrides) if args.overrides else ({}, None)
        report = convert(definition, info, code, args.layer, overrides, args.pitch_x, args.pitch_y, args.gap)
        report['sources'] = {name:dict(path=str(path), sha256=digest) for name,path,digest in
                             [('launcher',args.launcher,launcher_hash), ('qmk_info',args.qmk_info,info_hash),
                              ('qmk_keymap',args.qmk_keymap,code_hash), ('overrides',args.overrides,overrides_hash)] if path}
        export(report, args.output_dir)
        print(f'{report["name"]}: {len(report["keys"])} keys; {len(report["unresolved"])} unresolved; {args.output_dir}')
        return 2 if report['unresolved'] else 0
    except (ValueError, OSError, KeyError, TypeError, RecursionError) as error:
        parser.exit(1, f'Import rejected: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
