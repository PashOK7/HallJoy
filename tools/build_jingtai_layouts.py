"""Extract five simple, source-proven JingTai V1 layouts; no vendor JS execution."""
import argparse
import hashlib
import json
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_import as common
import layout_pipeline as pipeline
from build_irok_mg75_layouts import literal_array, model_class
from check_jingtai_v1_profiles import ROOT, SOURCE, SHA, prepare as check_profiles


def prepare():
    check_profiles()
    s = SOURCE.read_bytes().decode('utf-8')
    pi,it,ds,ot,io = [literal_array(s,n) for n in ('Pi','it','ds','Ot','Io')]
    rows87 = [pi+ds[0],it[0]+ds[1],it[1]+ds[2],it[2],it[3]+ot[0],io+[93,163]+ot[1]]
    rows68 = [[27,49,50,51,52,53,54,55,56,57,48,189,187,8,45],it[1]+[46],it[2]+[33],it[3]+ot[0]+[34],io+[163]+ot[1]]
    rows64 = [rows68[0][:-1],it[1],it[2],it[3]+ot[0]+[110],io+ot[1]]
    for expression in (
        'ar=[[...Pi,...ds[0]],[...it[0],...ds[1]],[...it[1],...ds[2]],[...it[2]],[...it[3],...Ot[0]],[...Io,93,163,...Ot[1]]]',
        'ta=[[27,49,50,51,52,53,54,55,56,57,48,189,187,8,45],[...it[1],46],[...it[2],33],[...it[3],...Ot[0],34],[...Io,163,...Ot[1]]]',
        'sa=[[27,49,50,51,52,53,54,55,56,57,48,189,187,8,45],[...it[1],46],[...it[2],33],[...it[3],...Ot[0],34],[...Io,163,...Ot[1]]]',
        'm7t=[[27,49,50,51,52,53,54,55,56,57,48,189,187,8],[...it[1]],[...it[2]],[...it[3],...Ot[0],110],[...Io,...Ot[1]]]'):
        assert expression in s
    a=s.index('gs=[{Text:'); b=s.index('];',a)
    entries=re.findall(r'Keys:new Map\(\[(.*?)\]\).*?VirtualKey:(\d+),HID:(\d+),Disable:',s[a:b])
    labels={hid:label for hid,label in common.CODES.values()};labels[1033]='Fn'
    specs=[('K','IROK','NA87 Pro','cDt','X5t','IROKNA87','ar',rows87,87,['JT1-K']),
           ('le','IYX','MU68 Pro','Fc','Y5t','IYXMu68','ta',rows68,68,['JT1-LE','JT1-LE-CYAN']),
           ('we','IROK','ND63','J5t',None,'IROKNd63','m7t',rows64,64,['JT1-WE','JT1-WE-CYAN']),
           ('Ie','IROK','Mercury68','nMt',None,'CarotmasMer68','sa',rows68,68,['JT1-IE']),
           ('Te','IROK','Mercury68 Pro','nMt',None,'CarotmasMer68','sa',rows68,68,['JT1-TE'])]
    assert 'class CDt extends nMt' in s
    reports=[]
    for alias,brand,model,cls,parent,square,rowname,rows,count,products in specs:
        body=model_class(s,cls)
        inherited=model_class(s,parent) if parent else body
        assert f'r(this,"vkcodes",{rowname})' in inherited
        assert f'r(this,"square",mt(h.{square}))' in inherited
        size=re.search(r'Zt.set\(h\.'+square+r',\{w:([\d.]+),h:([\d.]+),x:([\d.]+),y:([\d.]+)\}\)',s)
        w,h,x,y=map(Decimal,size.groups());assert w==h
        default=[x,y,w,h]
        year=re.search(r'r\(this,"y_number",(\d+)\)',body) or re.search(r'r\(this,"y_number",(\d+)\)',inherited)
        yn=year[1] if year else '0'
        custom=re.search(r'r\(this,"customLayout",\{(.*?)\}\)',body)[1]
        assert re.sub(r'\d+:this.createSquare\([^)]*\),?','',custom)==''
        overrides={}
        for m in re.finditer(r'(\d+):this.createSquare\(([^)]*)\)',custom):
            args=[Decimal(n.replace('this.y_number',yn)) for n in m[2].split(',')]
            values=default.copy();values[:len(args)]=args;overrides[int(m[1])]=values
        mapping={}
        for pairs,vk,hid in entries:
            match=re.search(r'\['+alias+r',(\d+)\]',pairs)
            if match:
                selector=int(match[1]);usage=1033 if selector==1 else int(hid)
                assert int(vk) not in mapping
                mapping[int(vk)]=(selector,usage)
        assert len(mapping)==count
        assert {v[1] for v in mapping.values()} == {1033 if v[0]==1 else v[0] for v in mapping.values()}
        keys=[];top=Decimal(0)
        for row in rows:
            left=Decimal(0);height=Decimal(0)
            for vk in row:
                if vk not in mapping:continue  # Vendor getKeyDataAndLayout filters absent model keys.
                selector,hid=mapping[vk]
                x,y,kw,kh=overrides.get(selector,default)
                keys.append(dict(hid=hid,label=labels[hid],x=left+x,y=top+y,w=kw,h=kh))
                left+=x+kw;height=max(height,y+kh)
            top+=height
        assert len(keys)==count and {k['hid'] for k in keys}=={v[1] for v in mapping.values()}
        minx=min(k['x'] for k in keys);miny=min(k['y'] for k in keys)
        for k in keys:
            k['x']-=minx;k['y']-=miny
            for field in ('x','y','w','h'):
                k[field]=int((k[field]*42/w).quantize(Decimal(1),rounding=ROUND_HALF_UP))
        symbol=brand.lower()+'_'+model.lower().replace(' ','_')+'_ansi'
        report=dict(schema=1,id=symbol,brand=brand,model=model,variant='ANSI',name=f'{brand} {model} ANSI',
                    status='ready',unresolved=[],keys=keys,identity=dict(protocol='irok-mg75-pro',products=products),
                    sources=[dict(path=SOURCE.relative_to(ROOT).as_posix(),sha256=SHA,url='https://hid.irok.cn/assets/index-D22onkw6.js')],
                    notes=['Pinned manufacturer row order, per-model key membership and geometry, normalized to 42px.',
                           'Exact verified-session auto selection; no hardware or visual test claimed.'])
        pipeline.validate_report(report);reports.append(report)
    return reports


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write-new',action='store_true');args=parser.parse_args()
    for report in prepare():
        p=ROOT/'docs/research/jingtai-layouts'/(report['id']+'.json')
        data=(json.dumps(report,indent=2)+'\n').encode()
        if args.write_new:
            p.parent.mkdir(parents=True,exist_ok=True)
            with p.open('xb') as f:f.write(data)
        else:assert p.read_bytes()==data
        print(report['name'],len(report['keys']),'PASS')


if __name__=='__main__':main()
