"""Batch exact OEM registry -> UI component -> SVG layouts, rejecting ambiguity.

Uses the archived official ATTACK SHARK client: its registry contains other OEM
brands too. Both registry identities and layout names must match the separately
reviewed WOMIER protocol catalog. No geometry inferred from matrix positions.
"""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path
from extract_attackshark_family_layouts import usage
from generate_attackshark_family import decode
from layout_pipeline import validate_report

ROOT=Path(__file__).resolve().parents[1]
LIVE=ROOT/'.local/research/attackshark-pro/desktop/resources/app/dist/js'
OUT=ROOT/'docs/research/rongyuan-layouts'
MAIN='index.2e5bd916.js'
MAIN_SHA='000d6f2f25cc31836e4a68cd1610b5e8c047a0d63216f623513062c26963ba07'


def blob(obj):return (json.dumps(obj,indent=2)+'\n').encode()
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,b):
    if p.exists():assert p.read_bytes()==b, str(p);return
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('xb') as f:f.write(b)


def prepare(acquire=False):
    source=LIVE if acquire else OUT/'sources'
    s=(source/MAIN).read_text(encoding='utf-8')
    assert sha((source/MAIN).read_bytes())==MAIN_SHA
    profiles=json.loads((ROOT/'docs/research/rongyuan-stream/profiles.json').read_bytes())
    # Pinned protocol catalog retains the manufacturer's exact keyLayout expression.
    records=json.loads((ROOT/'docs/research/rongyuan-stream/admission_sources.json').read_bytes())['records']
    records={r['id']:r for r in records}
    ui=collections.defaultdict(list)
    for cases,component in re.findall(r'((?:case u\.\w+:)+)\w+=m\.jsx\((\w+),',s):
        for layout in re.findall(r'case u\.(\w+):',cases):ui[layout].append(component)
    lazy=dict(re.findall(r'\b(\w+)=_\.lazy\(\(\)=>\w+\(\(\)=>import\("([^\"]+)"\)',s))
    cache={};ready=[];held=[];files={MAIN:(source/MAIN).read_bytes()}
    existing=json.loads((ROOT/'tools/layout_catalog.json').read_bytes())['brands']
    existing_tokens={p for spec in existing.values() for m in spec.get('models',[]) if not m.get('source','').startswith('docs/research/rongyuan-layouts/') for p in m['products']}
    def geometry(layout):
        if layout in cache:return cache[layout]
        assert layout in ui, 'geometry component absent'
        found=[]
        for component in ui[layout]:
            path=lazy.get(component,'')
            if not path.startswith('./') or not path.endswith('.js'):continue
            chunk=path[2:]
            if not (source/chunk).exists():continue
            rt=(source/chunk).read_text(encoding='utf-8')
            for name in [chunk]+re.findall(r'from"\./([^"/]+\.js)"',rt):
                if name==MAIN or not (source/name).exists():continue
                text=(source/name).read_text(encoding='utf-8')
                for m in re.finditer(r'(Keyboard_[A-Za-z0-9_]+_KeyMappings):\{type:"svg",str:await \w+\(\(\)=>import\("\./([^"/]+\.js)"\)',text):
                    found.append((name,m[2]))
            if found:break
        assert len(found)==1,'plain key-mapping SVG absent or ambiguous'
        mapping,svg=found[0];text=(source/svg).read_text(encoding='utf-8')
        keys={};omitted=[]
        for code,body in re.findall(r'<g id="#([^"]+)">(.*?)(?=<g id="#|</svg>)',text,re.S):
            if code in keys or code in omitted:continue
            if code.startswith('Audio'):omitted.append(code);continue
            assert not code.startswith(('Intl','Lang')), 'regional geometry requires separate review'
            try:label,hid=usage(code)
            except (KeyError,ValueError):raise AssertionError('unreviewed key '+code)
            rect=re.search(r'<rect ([^>]+)>',body)
            assert rect and 'transform=' not in rect[1], 'nonrectangular or transformed key'
            attrs=dict(re.findall(r'([a-z]+)="([^"]+)"',rect[1]))
            keys[code]=dict(hid=hid,label=label,x=round(float(attrs['x'])),y=round(float(attrs['y'])),w=round(float(attrs['width'])),h=round(float(attrs['height'])))
        assert keys,'empty SVG'
        x=min(k['x'] for k in keys.values());y=min(k['y'] for k in keys.values())
        for k in keys.values():k['x']-=x;k['y']-=y
        keys=sorted(keys.values(),key=lambda k:(k['y'],k['x']))
        validate_report(dict(status='ready',unresolved=[],name=layout,keys=keys))
        chain=list(dict.fromkeys([MAIN,chunk,mapping,svg]))
        for name in chain:files[name]=(source/name).read_bytes()
        cache[layout]=(keys,chain,omitted);return cache[layout]
    for r in profiles:
        item={k:r[k] for k in ('board','brand','model','product')}
        if r['product'] in existing_tokens:
            held.append(dict(item,reason='existing exact automatic preset'));continue
        try:
            needle=f'id:{r["board"]},vid:{r["vid"]},pid:{r["pid"]},keyLayout:u.'
            assert needle in s,'exact USB/board registry entry absent'
            layout=re.match(r'\w+',s.split(needle,1)[1])[0]
            assert re.search(r'keyLayout:c\.(\w+)',records[r['board']]['record_literal'])[1]==layout,'cross-client layout mismatch'
            keys,chain,omitted=geometry(layout)
            physical=set(decode(r['matrix']))-{0}
            assert physical=={k['hid'] for k in keys},'factory HID set differs from SVG'
            ready.append(dict(item,layout=layout,keys=keys,chain=chain,omitted=omitted))
        except (AssertionError,ValueError,KeyError,FileNotFoundError) as error:
            held.append(dict(item,reason=str(error)))
    groups=collections.defaultdict(list)
    for r in ready:groups[(r['brand'],json.dumps(r['keys'],sort_keys=True))].append(r)
    reports=[]
    for (brand,_),members in groups.items():
        names=sorted({r['model'] for r in members});model=' + '.join(names)
        name=f'{brand} {model} ANSI'
        if len(name)>100:
            held.extend(dict(r,reason='combined selector exceeds existing name limit') for r in members);continue
        symbol='ry_layout_'+str(min(r['board'] for r in members))
        chain=sorted({n for r in members for n in r['chain']})
        sources=[dict(path=(OUT/'sources'/n).relative_to(ROOT).as_posix(),sha256=sha(files[n])) for n in chain]
        for p in ['docs/research/rongyuan-stream/profiles.json','docs/research/rongyuan-stream/admission_sources.json']:
            sources.append(dict(path=p,sha256=sha((ROOT/p).read_bytes())))
        report=dict(schema=1,id=symbol,brand=brand,model=model,variant='ANSI',name=name,status='ready',unresolved=[],keys=members[0]['keys'],
                    identity=dict(protocol='rongyuan-stream',products=sorted({r['product'] for r in members})),sources=sources,
                    models=names,boards=sorted({r['board'] for r in members}),
                    notes=['Exact board/USB registry and keyLayout agree across official OEM clients. Full keyboard HID set agrees with the enabled factory map.',
                           'Only plain rectangular ANSI geometry admitted. Identical same-brand layouts grouped; no physical or visual test claimed.'])
        validate_report(report);reports.append(report)
    if acquire:
        for n,b in files.items():save(OUT/'sources'/n,b)
        for r in reports:save(OUT/(r['id']+'.json'),blob(r))
        save(OUT/'inventory.json',blob(dict(reports=[r['id'] for r in reports],held=held)))
    else:
        for r in reports:assert (OUT/(r['id']+'.json')).read_bytes()==blob(r)
    print('RONGYUAN_LAYOUTS=PASS presets='+str(len(reports))+' models='+str(len({(r['brand'],m) for r in reports for m in r['models']}))+' revisions='+str(sum(len(r['boards']) for r in reports)))
    print('Holds:',dict(collections.Counter(r['reason'] for r in held)))
    return reports


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--acquire',action='store_true');args=p.parse_args();prepare(args.acquire)
