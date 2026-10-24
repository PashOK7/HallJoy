"""Extract reviewed EPOMAKER geometry as data, never execute vendor JavaScript.

Run --acquire once from the downloaded gearhub.top/v4 sources; default verifies
the pinned extraction. Ambiguous factory/UI mappings fail closed.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from extract_attackshark_family_layouts import usage
from layout_pipeline import validate_report

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/research/epomaker-layouts'
MAIN = 'index.ad43be22.js'
BOARDS = {3664: 'ISO', 2520: 'ANSI', 3691: 'ANSI', 3692: 'ANSI',
          3518: 'ANSI', 3613: 'ANSI', 3365: 'ANSI', 3703: 'JIS'}
# OEM IntlRo is the non-US key beside Enter, NOT the Japanese Ro key.
# Its source legend/position and factory HID50 agree on both ISO and JIS.
REGIONAL = {'IntlBackslash': ('\\', 100), 'IntlRo': ('#', 50),
            'IntlYen': ('Yen', 137), 'R_IntlBackslash': ('Ro', 135),
            'NonConvert': ('Muhenkan', 139), 'Convert': ('Henkan', 138),
            'KanaMode': ('Kana', 136)}

def sha(b): return hashlib.sha256(b).hexdigest()
def blob(obj): return (json.dumps(obj, indent=2)+'\n').encode()
def save(path, data):
    if path.exists():
        assert path.read_bytes() == data, str(path)
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as f: f.write(data)

def prepare(acquire=False):
    source = ROOT/'.local/research/epomaker-layout-20260926' if acquire else OUT/'sources'
    main = (source/MAIN).read_text(encoding='utf8')
    links = json.loads((source/'layout-links.json').read_bytes())
    profiles = json.loads((ROOT/'docs/research/rongyuan-stream/profiles.json').read_bytes())
    records = {r['id']: r for r in json.loads((ROOT/'docs/research/rongyuan-stream/admission_sources.json').read_bytes())['records']}
    reports = []
    for p in profiles:
        board = p['board']
        if board not in BOARDS: continue
        needle = f'id:{board},vid:{p["vid"]},pid:{p["pid"]},keyLayout:u.'
        if board in (3692, 3703):
            # Web registry uses the shared 39AB:9016 frontend identity for these
            # wireless models. Preserve the separately verified runtime USB IDs.
            needle = f'id:{board},vid:14763,pid:36886,keyLayout:u.'
        assert main.count(needle) == 1
        layout = re.match(r'\w+', main.split(needle)[1])[0]
        archived_layout = re.search(r'keyLayout:c\.(\w+)', records[board]['record_literal'])[1]
        if board == 3692:
            assert layout == 'Common61_sg9040' and archived_layout == 'Common61_gk06'
            layout = archived_layout  # geometry for the admitted revision
        else:
            assert layout == archived_layout
        link = links[layout]
        assert f'./keyboard_ui_info/{link["ui"]}.ts' in main
        assert f'import("./{link["chunk"]}")' in main
        text = (source/link['chunk']).read_text(encoding='utf8')
        assert f'as {link["ui"]}' in text
        obj = text[text.index('={')+1:text.index(';export')]
        obj = re.sub(r'("(?:\\.|[^"\\])*")|([a-zA-Z_$][\w$]*)(?=\s*:)',
                     lambda m: m[1] or json.dumps(m[2]), obj)
        geometry = json.loads(obj)['layout']
        keys, omitted = [], []
        for code, rect in geometry.items():
            if rect.get('type') != 'key': continue
            if code.startswith('Audio') or code == 'LaunchCalculator':
                omitted.append(code); continue
            label, hid = REGIONAL[code] if code in REGIONAL else usage(code)
            keys.append(dict(hid=hid, label=label, x=rect['x'], y=rect['y'], w=rect['width'], h=rect['height']))
        x, y = min(k['x'] for k in keys), min(k['y'] for k in keys)
        for k in keys: k['x']-=x; k['y']-=y
        keys.sort(key=lambda k: (k['y'], k['x']))
        # Mirrors runtime rongyuan::Decode, including standard JIS usages.
        matrix = p['matrix']; physical = set()
        for i in range(0, len(matrix), 4):
            r = matrix[i:i+4]
            h = r[2] if r[0] == r[1] == r[3] == 0 else 1033 if r[:2] == [10,1] else 0
            if h: physical.add(h)
        assert physical == {k['hid'] for k in keys}, (board, physical^{k['hid'] for k in keys})
        paths = [OUT/'sources'/n for n in (MAIN, 'layout-links.json', link['chunk'])]
        paths += [ROOT/'docs/research/rongyuan-stream'/n for n in ('profiles.json', 'admission_sources.json')]
        sources = []
        for path in paths:
            data = (source/path.name).read_bytes() if path.parent == OUT/'sources' else path.read_bytes()
            if acquire: save(path, data)
            sources.append(dict(path=path.relative_to(ROOT).as_posix(), sha256=sha(data)))
        report = dict(schema=1, id=f'epomaker_layout_{board}', brand='EPOMAKER', model=p['model'],
                      variant=BOARDS[board], name=f'EPOMAKER {p["model"]} {BOARDS[board]}', status='ready', unresolved=[], keys=keys,
                      identity=dict(protocol='rongyuan-stream', products=[p['product']]), sources=sources,
                      models=[p['model']], boards=[board],
                      notes=['Source: https://gearhub.top/v4/ (2026-09-26). Board and layout traced through independently archived OEM registry. Wireless frontend IDs differ; runtime admission unchanged. Board3692 uses archived Common61_gk06, not newer web sg9040.',
                             'All keyboard/Fn usages match the runtime factory matrix; no physical or visual test claimed.',
                             'Consumer controls are not keyboard analog keys: '+', '.join(omitted)])
        validate_report(report)
        path = OUT/(report['id']+'.json')
        if acquire: save(path, blob(report))
        else: assert path.read_bytes() == blob(report)
        reports.append(report)
    assert len(reports) == len(BOARDS)
    print(f'EPOMAKER_LAYOUTS=PASS presets={len(reports)} keys={sum(len(r["keys"]) for r in reports)}')
    return reports

if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('--acquire', action='store_true')
    prepare(p.parse_args().acquire)
