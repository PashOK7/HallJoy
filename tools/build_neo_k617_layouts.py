"""Two ready ANSI geometries, validated against the enabled physical maps."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import layout_import as common
from layout_pipeline import validate_report
from build_rongyuan_layouts import save, blob

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/research/neo-k617-layouts'
LOCKS=[('neo65-20260924','3848334949.json','c7f672d11ea14b61a20663f9ef6439a9150193e7e3d1bd6e0706caa771d51ba5'),
       ('k617-20260924','7153USHEXYXCPARGB.json','e5ada069dbc3655fbad06147c4e01b9e81643f66e1aa9b18c7276ae6d6ff23e8')]


def prepare(acquire=False):
    labels={hid:label for hid,label in common.CODES.values()};labels[1033]=labels[250]='Fn'
    reports=[]
    for folder,name,digest in LOCKS:
        p=OUT/name
        if acquire:save(p,(ROOT/'.local/research'/folder/name).read_bytes())
        raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()==digest;j=json.loads(raw)
        keys=[]
        if name.startswith('384'):
            brand,model,symbol,protocol,products='Neo','Neo65 Sonic HE+','neo65_sonic_he_ansi','neo65',['ANSI']
            codepath=ROOT/'src/HallJoyProject/HallJoy/neo65_protocol.h';code=codepath.read_text(encoding='utf-8')
            matrix=list(map(int,re.search(r'kAnsiMap=\{([0-9,]+)\}',code)[1].split(',')))
            assert j['vendorProductId']==0xe560ee65 and j['matrix']==dict(rows=5,cols=16)
            slots=set()
            for k in j['layouts']['keys']:
                assert not k['r'] and not k['ghost'] and k['w']==k['w2'] and k['h']==k['h2']
                slot=k['row']*16+k['col'];assert slot not in slots;slots.add(slot);hid=matrix[slot];assert hid
                keys.append(dict(hid=hid,label=labels[hid],x=round(k['x']*46),y=round(k['y']*46),w=round(k['w']*46)-4,h=round(k['h']*46)-4))
            assert slots=={i for i,hid in enumerate(matrix) if hid} and len(keys)==67
        else:
            brand,model,symbol,protocol,products='Redragon','K617 HE','redragon_k617_he_ansi','aula-w669',['7153USHEXYXCPARGB']
            codepath=ROOT/'src/HallJoyProject/HallJoy/aula_w669_protocol.cpp';code=codepath.read_text(encoding='utf-8')
            body=re.search(r'PositionToHid K617UsFactoryMap\(\) noexcept\s*\{(.*?)\n\}',code,re.S)[1]
            matrix=list(map(int,re.search(r'return \{([0-9,]+)\}',body)[1].split(',')))
            assert j['type']=='us' and len(matrix)==132
            slots=set()
            for k in j['keys']:
                slot=int(k['index']);hid=int(k['hidCode'],16);assert slot not in slots and hid==matrix[slot];slots.add(slot)
                keys.append(dict(hid=hid,label=labels[hid],x=round((float(k['x'])-10)*1.2),y=round((float(k['y'])-10)*1.2),w=round(float(k['width'])*1.2),h=round(float(k['height'])*1.2)))
            assert slots=={i for i,hid in enumerate(matrix) if hid} and len(keys)==61
        r=dict(schema=1,id=symbol,brand=brand,model=model,variant='ANSI',name=f'{brand} {model} ANSI',status='ready',unresolved=[],keys=keys,
               identity=dict(protocol=protocol,products=products),sources=[dict(path=p.relative_to(ROOT).as_posix(),sha256=digest),dict(path=codepath.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(codepath.read_bytes()).hexdigest())],
               notes=['Exact official geometry and every factory matrix position checked. ANSI only; regional compound geometry deferred. No physical-device or visual test claimed.'])
        validate_report(r);reports.append(r)
        dest=OUT/(symbol+'.json')
        if acquire:save(dest,blob(r))
        else:assert dest.read_bytes()==blob(r)
    print('NEO_K617_LAYOUTS=PASS models=2 keys=128');return reports


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--acquire',action='store_true');args=p.parse_args();prepare(args.acquire)
