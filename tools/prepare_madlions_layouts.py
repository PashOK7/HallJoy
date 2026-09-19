"""Reproduce four ANSI layouts from official literals, without running vendor JS."""
import argparse
import ast
from decimal import Decimal, ROUND_HALF_UP
import hashlib
import json
from pathlib import Path
import re
import layout_import as common
import layout_pipeline as pipeline
from layout_madlions_strings import decode

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / 'docs/research/madlions-layout-sources'
LOCKS = {
    'index-CfSMCN5O.js': 'c2855cf201713fe102b110f96423f70310c8e7623afc0a943119a66d6dd27418',
    'ConfigPage-jy-tY0c4.js': 'a0e0112d7102ef2ad1e57eb7d8ddfe57868f51cd375c6b2b2428c65ac464e1a7',
    'ConfigPage-8vLbbb0u.js': '35d7ef4d3ffc4bc276ecf51fa505bbb550a9c0d73ad5f2fba81214db4d8a3a9f',
    'vue-draggable-plus-cdvt9v0M.js': 'efd01837485ec46bd6d7bec74d88d2ee8116365b33b8b900380ea9194be09834',
}

def object_literal(text, start):
    common.require(text[start] == '{', 'Expected object')
    depth, quote, escaped = 0, None, False
    for end in range(start, len(text)):
        char = text[end]
        if quote:
            if escaped: escaped = False
            elif char == '\\': escaped = True
            elif char == quote: quote = None
        elif char in "\"'": quote = char
        elif char == '{': depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0: return text[start:end + 1]
    raise ValueError('Unterminated object')

def extract(text, symbol, fire):
    start = re.search(r'\b' + symbol + r'=\{', text).end() - 1
    body = object_literal(text, start)
    common.require("'w':0x32,'h':0x32,'gap':0x2" in body[:100], 'Unexpected base size')
    codes = dict(common.CODES)
    aliases = {'BSLASH':'BACKSLASH', 'BSPACE':'BACKSPACE', 'CAPSLOCK':'CAPS_LOCK', 'LBRACKET':'LEFT_BRACKET', 'RBRACKET':'RIGHT_BRACKET',
               'SCOLON':'SEMICOLON', 'PGDOWN':'PAGE_DOWN', 'LSHIFT':'LEFT_SHIFT',
               'RSHIFT':'RIGHT_SHIFT', 'LCTRL':'LEFT_CTRL', 'RCTRL':'RIGHT_CTRL'}
    for alias, canonical in aliases.items(): codes['KC_' + alias] = codes['KC_' + canonical]
    modifiers = {'LCtrl':224, 'LShift':225, 'LAlt':226, 'LWin':227,
                 'RCtrl':228, 'RShift':229, 'RAlt':230, 'RWin':231}
    labels = {hid:label for hid,label in common.CODES.values()}
    keys, positions = [], set()
    for match in re.finditer(r"\{'l':", body):
        item = object_literal(body, match.start())
        head, action = item.split("'defaultKeyAction':" if fire else "'defaultValue':", 1)
        fields = ast.literal_eval(head.rstrip(',') + '}')
        common.require(set(fields) <= {'l','t','w','h','position'}, 'Unreviewed key field')
        common.require(fields['position'] not in positions, 'Duplicate position')
        positions.add(fields['position'])
        if not fire:
            common.require(action.startswith('{0x0:'), 'Missing Windows base action')
            action = action.split('),', 1)[0] + ')'
        keycode = re.search(r"\[['\"](KC_\w+)['\"]\]", action)
        if keycode: hid, label = codes[keycode[1]]
        elif fire and (modifier := re.search(r"\[['\"]([LR](?:Ctrl|Shift|Alt|Win))['\"]\]", action)):
            hid = modifiers[modifier[1]]
            label = labels[hid]
        else:
            fn = r"a40_0x2a20c6\[['\"]create['\"]\]\(0xff,0x1\)" if fire else r"a8_0x49efab\[['\"]create['\"]\]\(0x21\)"
            common.require(re.search(fn, action) is not None, 'Unknown base action: ' + action)
            hid, label = 0x409, 'Fn'
        keys.append(dict(hid=hid, label=label, x=Decimal(fields['l']), y=Decimal(fields['t']),
                         w=Decimal(str(fields.get('w',1))) * 50, h=Decimal(str(fields.get('h',1))) * 50))
    common.require(len(keys) == body.count("'position':"), 'Unparsed physical key')
    minx, miny = min(k['x'] for k in keys), min(k['y'] for k in keys)
    for key in keys:
        key['x'] -= minx
        key['y'] -= miny
        # Absolute coordinates and base-size multipliers match the web renderer.
        for field in ('x','y','w','h'):
            key[field] = int((key[field] * Decimal(42) / 50).quantize(Decimal(1), rounding=ROUND_HALF_UP))
    return keys

def prepare():
    for name, digest in LOCKS.items():
        common.require(hashlib.sha256((SOURCE_DIR/name).read_bytes()).hexdigest() == digest, 'Source lock mismatch: ' + name)
    duck = decode(SOURCE_DIR/'ConfigPage-jy-tY0c4.js')
    fire = decode(SOURCE_DIR/'vue-draggable-plus-cdvt9v0M.js')
    common.require("'MAD60\\x20HE':{'layout':Lt" in duck, 'MAD60 HE binding changed')
    common.require("'MAD68\\x20HE':{'layout':gt" in duck and 'gt={...yt,' in duck, 'MAD68 HE binding changed')
    common.require("'MAD68\\x20R\\x20(LL)':{'layout':wt" in duck and 'wt={...yt,' in duck, 'MAD68 R LL binding changed')
    common.require("'MAD\\x2068\\x20Pro':{'layout':Sn" in fire, 'MAD68 Pro binding changed')
    sources = [dict(path=str((SOURCE_DIR/name).relative_to(ROOT)).replace('\\','/'), sha256=digest,
                    url='https://hub.fgg.com.cn/assets/'+name) for name,digest in LOCKS.items()]
    reports = []
    for model, symbol, count, is_fire in [('MAD60HE','Lt',61,False), ('MAD68HE','yt',68,False),
                                        ('MAD68R','yt',68,False), ('MAD 68 Pro R','Sn',68,True)]:
        keys = extract(fire if is_fire else duck, symbol, is_fire)
        common.require(len(keys) == count, 'Unexpected key count: ' + model)
        notes = ['Official Windows base layer, ANSI; original MAD60/68 HE revisions.',
                 'Official absolute coordinates and size multipliers; 50 px base normalized to 42 px.',
                 'Manual selection; no guessed VID/PID or regional autoselection.',
                 'Existing analog protocols and mappings are unchanged.']
        if model == 'MAD68R': notes.append('Exact supported UAP revision 373B:10A7, official MAD68 R (LL), wt inherits yt; not Fire-family 106E/10A8.')
        if model == 'MAD60HE': notes.append('Physical bottom row has no RGUI at matrix 4,9; unused UAP map entry is not rendered.')
        if model == 'MAD 68 Pro R': notes.append('Fn is displayed as 0x409; the native backend descriptor has HID 0 and does not publish Fn analog.')
        report = dict(schema=1,id='madlions_'+model.lower().replace(' ','_')+'_ansi',brand='MADLIONS',
                      model=model,variant='ANSI',name='MADLIONS '+model+' ANSI',status='ready',unresolved=[],
                      keys=keys,identity=dict(protocol='madlions',products=[]),sources=sources,notes=notes)
        pipeline.validate_report(report)
        reports.append(report)
    return reports

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write-new', action='store_true')
    args = parser.parse_args()
    reports = prepare()
    folder = ROOT/'docs/research/madlions-layout-reports'
    for report in reports:
        path = folder/(report['id']+'.json')
        data = (json.dumps(report, indent=2)+'\n').encode()
        if args.write_new:
            folder.mkdir(exist_ok=True)
            with path.open('xb') as output: output.write(data)
        else: common.require(path.read_bytes() == data, 'Report drift: '+path.name)
    print('MADLIONS layouts: 4 models, 265 keys, source locks and geometry PASS')

if __name__ == '__main__': main()
