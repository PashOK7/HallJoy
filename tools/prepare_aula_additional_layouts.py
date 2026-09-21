"""Reproduce two pinned AULA layouts using static data only."""
import argparse
import hashlib
import json
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_pipeline as pipeline
from layout_aula_w669 import LABELS, factory_map, PROTOCOL

ROOT = Path(__file__).resolve().parents[1]
BASE = 'docs/research/aula-additional-layout-sources/'
OUT = 'docs/research/aula-additional-layout-reports/'
LOCKS = {
 'hero-app.js': ('16030c913fe6f6a4b0326a346ec9920a6a33ba16fe569bfcb4c5b6e9ce6bbaab', 'https://heb.aulacn.com/app-B6-MiDbc.js'),
 'SI2851UKKZHEARGB.json': ('fcb98f5df82c2e00d94501389c452172c4fab6f103df3cbf489fffe9b89945c3', 'https://hed.aulacn.com/config/keys/SI2851UKKZHEARGB.json')}

def source(name):
    data=(ROOT/BASE/name).read_bytes(); sha,url=LOCKS[name]
    assert hashlib.sha256(data).hexdigest()==sha
    return data.decode('utf8'),dict(path=BASE+name,sha256=sha,url=url)

def array(text,start):
    depth=0;quote=None;escaped=False
    for i in range(start,len(text)):
        c=text[i]
        if quote:
            if escaped: escaped=False
            elif c==chr(92): escaped=True
            elif c==quote: quote=None
        elif c in ('"',"'",'`'): quote=c
        elif c=='[': depth+=1
        elif c==']':
            depth-=1
            if not depth:return text[start:i+1]
    raise ValueError('Unclosed keyboard array')

def geometry(raw,unit):
    lowx=min(Decimal(str(k['x'])) for k in raw);lowy=min(Decimal(str(k['y'])) for k in raw)
    def px(n):return int((n*42/unit).quantize(Decimal(1),rounding=ROUND_HALF_UP))
    keys=[]
    for k in raw:
        x=Decimal(str(k['x']))-lowx;y=Decimal(str(k['y']))-lowy
        w=Decimal(str(k['width']));h=Decimal(str(k['height']));hid=k['hid']
        key=dict(hid=hid,label='Fn' if hid==0x409 else LABELS[hid],x=px(x),y=px(y),w=px(x+w)-px(x),h=px(y+h)-px(y))
        if 'index' in k:key['matrix']=[int(k['index'])//22,int(k['index'])%22]
        if k.get('compound'):key.update(notchW=px(x+w*Decimal('.2'))-px(x),notchY=px(y+h*Decimal('.48'))-px(y))
        keys.append(key)
    return keys

def report(ident,model,variant,keys,sources,identity):
    r=dict(schema=1,id=ident,brand='Aula',model=model,variant=variant,name='Aula '+model+' '+variant,status='ready',unresolved=[],keys=keys,omitted=[],sources=sources,identity=identity)
    pipeline.validate_report(r);return r

def reports():
    text,src=source('hero-app.js');start=text.index('keyboard:[',text.index('Yxt="AULA_2829"'))+9
    objects=re.findall(r'\{x:.*?pos:\d+\}',array(text,start));raw=[];positions=[]
    for obj in objects:
        k={f:re.search(r'\b'+f+r':([0-9.]+)',obj)[1] for f in ('x','y','width','height')}
        value=int(re.search(r'value:"(0x[0-9A-Fa-f]+)"',obj)[1],16)
        if value==0x0d000000:hid=0x409
        elif value<=0xff:hid=value
        else:
            bits=value>>16;assert value==bits<<16 and bits.bit_count()==1 and bits<=128
            hid=0xe0+bits.bit_length()-1
        pos=int(re.search(r'pos:(\d+)',obj)[1]);positions.append(pos);k['hid']=hid;raw.append(k)
    assert len(raw)==84 and set(positions)==set(range(1,78))|{95,98,99,100,101,102,103}
    assert raw[positions.index(53)]['hid']==0x34
    header=(ROOT/'src/HallJoyProject/HallJoy/aula_hero84he_factory.h').read_text()
    native=[(int(p),int(h,16)) for p,h in re.findall(r'\{(\d+), (0x[0-9A-F]+)\}',header)]
    assert native==list(zip(positions,[k['hid'] for k in raw]))
    backend=(ROOT/'src/HallJoyProject/HallJoy/aula_hero84he_backend.cpp').read_text()
    values=re.search(r'kPositions\{\s*\{(.*?)\}\}',backend,re.S)[1]
    assert {int(v) for v in re.findall(r'\d+',values)}==set(positions)
    hero=report('aula_hero84_he_ansi','HERO84 HE','ANSI',geometry(raw,34),[src],dict(protocol='aula-hero84',products=['110000000005'],requiresVerifiedSession=True))
    hero['limitations']=['Experimental backend remains unverified on hardware; Fn uses physical position 72.']
    text,src=source('SI2851UKKZHEARGB.json');data=json.loads(text);assert data['type']=='uk'
    raw=data['keys'];actual={int(k['index']):int(k['hidCode'],16) for k in raw}
    code=PROTOCOL.read_text();expected=factory_map(code,'Win68FactoryMap')
    body=re.search(r'PositionToHid KpTe153UkFactoryMap\(\) noexcept\s*\{(.*?)\n\}',code,re.S)[1]
    for m in re.finditer(r'map\[(\d+)\]\s*=\s*(0x[0-9a-f]+|0);',body):
        i,h=int(m[1]),int(m[2],0)
        if h:expected[i]=h
        else:expected.pop(i,None)
    assert len(raw)==69 and actual==expected
    csspath='docs/research/redragon-layout-sources/mainIndex8.min.css';css=(ROOT/csspath).read_bytes()
    csssha='e90906657ed201b9cfbde5de88d082920e9184c8a0d4a807f630065752f62c43'
    assert hashlib.sha256(css).hexdigest()==csssha
    for selector,values in (('.devKeyPanelUK .devKeyPanelView::before',('height:48%','top:0')),('.devKeyPanelUK .devKeyPanelView::after',('width:80%','height:calc(52% + 1px)','right:0'))):
        assert any(all(v in rule for v in values) for rule in re.findall(re.escape(selector)+r'\{([^}]+)\}',css.decode()))
    assert (ROOT/BASE/'illumipc-SI2851UKKZHEARGB.json').read_bytes()==(ROOT/BASE/'SI2851UKKZHEARGB.json').read_bytes()
    for k in raw:k['hid']=int(k['hidCode'],16);k['compound']=k['hid']==40
    kp=report('aula_kp_te153_iso','KP-TE153','ISO',geometry(raw,35),[src,dict(path=csspath,sha256=csssha,url='https://www.illumipc.com/css/mainIndex8.min.css')],dict(protocol='aula-w669',products=['SI2851UKKZHEARGB'],requiresVerifiedSession=True,vid=0x2e3c,pid=0xc365,usagePage=0xff1b,usage=0x91,rows=6,columns=22))
    kp['limitations']=['Vendor UK profile includes IntlRo; not a generic UK substitution. AULA/shared-driver profile attribution, not independently verified retail branding.']
    return [hero,kp]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
    for r in reports():
        p=ROOT/OUT/(r['id']+'.json');data=(json.dumps(r,indent=2)+'\n').encode()
        if args.check:assert p.read_bytes()==data,p
        else:
            p.parent.mkdir(parents=True,exist_ok=True)
            with p.open('xb') as f:f.write(data)
        print(r['name'],len(r['keys']))
if __name__=='__main__':main()
