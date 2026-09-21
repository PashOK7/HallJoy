"""Trace exact vendor registry -> RT component -> key SVG, without running client code."""
import hashlib
import json
import re
from pathlib import Path
from generate_attackshark_layouts import usage, SPECIAL
from generate_attackshark_family import decode
from layout_pipeline import validate_report

ROOT = Path(__file__).resolve().parents[1]
JS = ROOT / '.local/research/attackshark-pro/desktop/resources/app/dist/js'
OUT = ROOT / 'docs/research/attackshark-layout-sources'
SPECIAL.update({
    'PrintScreen': ('PrtSc',70), 'ScrollLock': ('ScrLk',71), 'Pause': ('Pause',72),
    'NumLock': ('Num',83), 'NumpadDivide': ('/',84), 'NumpadMultiply': ('*',85),
    'NumpadSubtract': ('-',86), 'NumpadAdd': ('+',87), 'NumpadEnter': ('Enter',88),
    'Numpad0': ('0',98), 'NumpadDecimal': ('.',99), 'ContextMenu': ('Menu',101),
})
SPECIAL.update({f'Numpad{i}': (str(i),88+i) for i in range(1,10)})


def digest(data):
    return hashlib.sha256(data).hexdigest()


def save(path, data):
    before = path.read_bytes() if path.exists() else None
    if before == data:
        return
    if before is not None:
        raise ValueError(f'Review existing changed extraction before replacing: {path}')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(data)


def extract():
    registry = (JS/'index.2e5bd916.js').read_bytes()
    assert digest(registry) == '000d6f2f25cc31836e4a68cd1610b5e8c047a0d63216f623513062c26963ba07'
    source = registry.decode('utf8')
    records = json.loads((ROOT/'docs/research/attackshark-family-profiles-20260920.json').read_text())
    cache, result = {}, []
    for record in records:
        start = source.index(f'id:{record["id"]},vid:12625,')
        entry = source[start:source.index('displayName:',start)]
        layout = re.search(r'keyLayout:u\.(\w+)',entry)[1]
        if layout not in cache:
            component = re.search(r'case u\.'+layout+r':d=m\.jsx\((\w+),',source)[1]
            chunk = re.search(r'\b'+component+r'=_.lazy\(.*?import\("\./([^"/]+\.js)"\)',source)[1]
            rt = (JS/chunk).read_text(encoding='utf8')
            found = []
            for candidate in [chunk] + re.findall(r'from"\./([^"/]+\.js)"',rt):
                if candidate == 'index.2e5bd916.js':
                    continue
                text = (JS/candidate).read_text(encoding='utf8')
                match = re.search(r'(Keyboard_[A-Za-z0-9_]+_KeyMappings):\{type:"svg",str:await \w+\(\(\)=>import\("\./([^"/]+\.js)"\)',text)
                if match:
                    found.append((candidate,match[2]))
            assert len(found) == 1, (layout, found)
            mapping, svg = found[0]
            svg_text = (JS/svg).read_text(encoding='utf8')
            keys, omitted = {}, []
            for code, body in re.findall(r'<g id="#([^"]+)">(.*?)(?=<g id="#|</svg>)',svg_text,re.S):
                if code in keys or code in omitted:
                    continue
                if code.startswith('Audio'):
                    omitted.append(code)
                    continue
                label, hid = usage(code)
                rect = re.search(r'<rect ([^>]+)>',body)
                assert rect and 'transform=' not in rect[1], (layout,code)
                attrs = dict(re.findall(r'([a-z]+)="([^"]+)"',rect[1]))
                keys[code] = dict(hid=hid,label=label,**{a:round(float(attrs[a])) for a in ('x','y','width','height')})
            ox,oy = min(k['x'] for k in keys.values()),min(k['y'] for k in keys.values())
            for k in keys.values():
                k['x']-=ox;k['y']-=oy;k['w']=k.pop('width');k['h']=k.pop('height')
            ordered = sorted(keys.values(),key=lambda k:(k['y'],k['x']))
            validate_report(dict(status='ready',unresolved=[],name=layout,keys=ordered))
            chain = list(dict.fromkeys(['index.2e5bd916.js',chunk,mapping,svg]))
            sources=[]
            for file in chain:
                data=(JS/file).read_bytes();save(OUT/file,data)
                sources.append(dict(path=(OUT/file).relative_to(ROOT).as_posix(),sha256=digest(data)))
            cache[layout]=dict(layout=layout,svg=svg,sources=sources,keys=ordered,omitted=omitted)
        item=dict(cache[layout],id=record['id'],display=record['display'])
        factory=set(decode(record['factory']))-{0}
        geometry={k['hid'] for k in item['keys']}
        item['geometryWithoutNativeUsage']=sorted(geometry-factory)
        item['nativeUsageWithoutGeometry']=sorted(factory-geometry)
        result.append(item)
        print(record['id'],record['display'],layout,len(item['keys']),
              'geometry_only',item['geometryWithoutNativeUsage'],'native_only',item['nativeUsageWithoutGeometry'])
    save(OUT/'extraction.json',(json.dumps(result,indent=2)+'\n').encode())
    print('Exact registry/geometry extraction PASS',len(result),'revisions',len(cache),'schemes')


if __name__ == '__main__':
    extract()
