"""Static manufacturer-source extraction; explicit staging only, no vendor execution.

Optional extraction dependencies: json5==0.12.1, svgpathtools==1.7.1.
Install into .local/layout-python-deps. Runtime/build checks do not require them.
"""
import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path
from decimal import Decimal, ROUND_HALF_UP
import xml.etree.ElementTree as ET
import layout_import as common
import layout_pipeline as pipeline

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/layout-python-deps'))
import json5
from svgpathtools import parse_path, Line
SOURCE=ROOT/'docs/research/final-layout-sources'
LABELS={h:l for h,l in common.CODES.values()}
LABELS.update({0x409:'Fn',0x403:'Profile 1',0x404:'Profile 2',0x405:'Profile 3',0x408:'Mode'})
LOCKS={'nuphy.js':'8b8fe5fdb1c13092bd3f38476688e4435b2ffdfcca552ba4b36d5fe149f27fba',
       'wootility-v4.js':'7d3be2fce864792b14730633387e24e4d3e036408afb206d0a84c24045792cd5',
       'razer-mini.js':'2db1c309a8cdc1ea4f4e32df9364f1c0e87ba1d47d4a48052b0fe85da4a5705e'}
URLS={'nuphy.js':'https://drive.nuphy.io/static/js/main.23dc78ef.js',
      'wootility-v4.js':'https://v4.wootility.io/assets/index-154c0c6c.js',
      'razer-mini.js':'https://synapse.razer.com/products/688/static/js/main.2776d388.js'}

def read(name):
    text,sha=common.read_source(SOURCE/name)
    if name in LOCKS: common.require(sha==LOCKS[name],'Source changed: '+name)
    return text

def literal(text, marker):
    start=text.index(marker)+len(marker)
    opening=text[start]; closing={'{':'}','[':']'}[opening]
    depth=0; quoted=None; escaped=False
    for i in range(start,len(text)):
        c=text[i]
        if quoted:
            if escaped: escaped=False
            elif c=='\\': escaped=True
            elif c==quoted: quoted=None
        elif c in ('"',"'"): quoted=c
        elif c==opening: depth+=1
        elif c==closing:
            depth-=1
            if not depth: return text[start:i+1]
    raise ValueError('Unterminated literal: '+marker)

def roundpx(value):
    return int(Decimal(str(value)).quantize(Decimal(1),rounding=ROUND_HALF_UP))

def report(brand,model,variant,keys,files,omitted=None):
    minx=min(k['x'] for k in keys); miny=min(k['y'] for k in keys)
    for k in keys: k['x']-=minx; k['y']-=miny
    symbol=re.sub('[^a-z0-9]+','_',brand.lower()+'_'+model.lower().replace('+','_plus')+'_'+variant.lower()).strip('_')
    result=dict(schema=1,id=symbol,brand=brand,model=model,variant=variant,name=f'{brand} {model} {variant}',
                status='ready',unresolved=[],keys=keys,omitted=omitted or [],
                sources=[dict(path=(SOURCE/f).relative_to(ROOT).as_posix(),
                              sha256=hashlib.sha256((SOURCE/f).read_bytes()).hexdigest(),
                              url=URLS.get(f,'')) for f in files],
                identity=dict(protocol='manual-layout',products=[],requiresVerifiedSession=True),
                autoSelection='Disabled: current session telemetry does not prove the physical variant.')
    pipeline.validate_report(result)
    return result

def nuphy():
    text=read('nuphy.js'); module=text[text.index('97075:function'):]
    aliases={'KC_FN1':(0x409,'Fn'),'KC_DOT':(55,'.')}
    reports=[]
    for variable,model in [('y','Air60 HE'),('C','Air75 HE')]:
        raw=literal(module,variable+'=')
        raw=re.sub(r's\.A\.(KC_\w+)',r'"\1"',raw).replace('!0','true').replace('!1','false')
        data=json5.loads(raw); keys=[]; omitted=[]
        for k in data:
            name=k['keyCode']
            if name=='KC_FN_WIN_AREA_SCREENSHOT':
                omitted.append(dict(code=name,reason='Firmware macro has no published UAP HID identity')); continue
            common.require(not k.get('hidden'),'Unexpected hidden entry in selected NuPhy array')
            hid,label=aliases.get(name,common.CODES.get(name,(None,None)))
            common.require(hid is not None,'Unknown NuPhy action: '+name)
            # Manufacturer unit grid; HallJoy theme uses 46px pitch and 4px gap.
            x,y=float(k['x'])*46,float(k['y'])*46
            w,h=float(k.get('w',1))*46,float(k.get('h',1))*46
            keys.append(dict(hid=hid,label=label,x=roundpx(x),y=roundpx(y),
                             w=roundpx(x+w)-roundpx(x)-4,h=roundpx(y+h)-roundpx(y)-4))
        reports.append(report('NuPhy',model,'ANSI',keys,['nuphy.js'],omitted))
    return reports

def wooting():
    text=read('wootility-v4.js')
    generic=json5.loads(literal(text,'GenericDeviceLayout='))
    newgeneric=json5.loads(literal(text,'NewGenericDeviceLayout='))
    raw=literal(text,'DeviceLayout80HE=').replace('...NewGenericDeviceLayout,','').replace('void 0','null')
    eighty={**newgeneric,**json5.loads(raw)}
    aliases={'Escape':41,'Enter':40,'Backspace':42,'Tab':43,'Spacebar':44,'Underscore':45,'Plus':46,
             'OpenBracket':47,'CloseBracket':48,'Backslash':49,'Colon':51,'Quote':52,'Tilde':53,
             'Comma':54,'Dot':55,'Slash':56,'CapsLock':57,'Printscreen':70,'ScrollLock':71,'Pause':72,
             'Insert':73,'Home':74,'PageUp':75,'Delete':76,'End':77,'PageDown':78,
             'Right':79,'Left':80,'Down':81,'Up':82,'KeypadNumLock':83,'KeypadDivide':84,
             'KeypadMultiply':85,'KeypadMinus':86,'KeypadPlus':87,'KeypadEnter':88,'Keypad0':98,'KeypadAt':99,
             'ExtraIso':100,'Application':101,'Int1':135,'Int3':137,'ImeOff':139,'ImeOn':138,
             'ModifierLeftCtrl':224,'ModifierLeftShift':225,'ModifierLeftAlt':226,'ModifierLeftUi':227,
             'ModifierRightCtrl':228,'ModifierRightShift':229,'ModifierRightAlt':230,'ModifierRightUi':231}
    aliases.update({chr(65+i):4+i for i in range(26)})
    aliases.update({'Number'+str(i):29+i if i else 39 for i in range(10)})
    aliases.update({'F'+str(i):57+i for i in range(1,13)})
    aliases.update({'Keypad'+str(i):88+i for i in range(1,10)})
    custom={'FnKey':0x409,'SelectProfile1':0x403,'SelectProfile2':0x404,'SelectProfile3':0x405,'ModeKey':0x408}
    reports=[]
    for model,start,rows,cols,mapping,geometry,variants in [
        ('One',0,6,17,'defaultFirstLayer',generic,('ANSI','ISO')),
        ('Two',0,6,21,'defaultFirstLayer',generic,('ANSI','ISO')),
        ('Two HE',0,6,21,'defaultFirstLayer',generic,('ANSI','ISO')),
        ('60HE',1,5,14,'defaultFirstLayer60HE',generic,('ANSI','ISO')),
        ('60HE+',1,5,14,'defaultFirstLayer60HE',generic,('ANSI','ISO')),
        ('80HE',0,6,17,'defaultFirstLayer80HE',eighty,('ANSI','ISO','JIS'))]:
        matrix=json5.loads(re.sub(r'(Hid|Custom)\.(\w+)',r'"\1.\2"',literal(text,mapping+'=')))
        for variant in variants:
            keys=[]; y=0
            for row in range(start,start+rows):
                x=0; advance=0
                for col in range(cols):
                    props=geometry.get(f'{row},{col}')
                    while isinstance(props,dict) and any(k in props for k in ('ansi','iso','jis','rgb','nonRgb')):
                        props=props.get(variant.lower()) if any(k in props for k in ('ansi','iso','jis')) else props.get('nonRgb')
                    if props is None: continue
                    w,h=props.get('width',1),props.get('height',1)
                    mt,mb=props.get('mt',0),props.get('mb',0)
                    x+=props.get('ml',0); top=y+mt
                    value=matrix[row][col]
                    common.require(value is not None,f'Visible Wooting key lacks mapping: {model} {row},{col}')
                    prefix,name=value.split('.')
                    hid=(aliases if prefix=='Hid' else custom)[name]
                    left,width=x,w
                    key=dict(hid=hid,label=LABELS[hid],x=roundpx(left*46),y=roundpx(top*46),
                             w=roundpx((left+width)*46)-roundpx(left*46)-4,
                             h=roundpx((top+h)*46)-roundpx(top*46)-4,matrix=[row,col])
                    if hid==40 and variant in ('ISO','JIS'):
                        # Stem is 1.25u, top extends .25u left into the blank
                        # backslash position. Match the existing ISO compound format.
                        key['x']=roundpx((left-.25)*46)
                        key['w']=roundpx((left+w)*46)-key['x']-4
                        key['notchW']=roundpx(left*46)-key['x']
                        key['notchY']=42
                    keys.append(key)
                    if props.get('position')!='absolute':
                        x+=w+props.get('mr',0); advance=max(advance,mt+h+mb)
                y+=advance
            reports.append(report('Wooting',model,variant,keys,['wootility-v4.js']))
    return reports

def razer():
    text=read('razer-mini.js')
    selection=dict(re.findall(r'(KEY_\w+):"(sel\w+)"',text))
    # Reverse selection IDs can alias across regional layouts; use the matched
    # region's own declared AnalogInput HID, never the current remapped output.
    reports=[]
    for variant,svg,button in [('ANSI','razer-US.3007e3a0.svg','razer-1823.f2c58d81.js'),
                               ('ISO','razer-UK.b62b4afd.svg','razer-1844.c0d61943.js'),
                               ('JIS','razer-JPN.7e1f2cc0.svg','razer-mini.js')]:
        code=read(button)
        if variant=='JIS': code=literal(code[code.index('45578:'):],'const a=')
        pairs=re.findall(r'inputID:"(KEY_\w+)",inputType:"(?:AnalogInput|KeyInput)",(?:AnalogInput|keyInput):\{pageID:"7",HID:"(\d+)"\}',code)
        hids={selection[k]:int(v) for k,v in pairs if k in selection}
        hids['selFunction']=0x409
        hids['selWindow']=227  # SVG Windows key; USB keyboard Left GUI.
        # Shared button list includes all three physical locations under the
        # same selector; the selected regional SVG determines which is present.
        hids['selBackslash']={'ANSI':49,'ISO':100,'JIS':135}[variant]
        root=ET.fromstring(read(svg)); keys=[]
        for node in root.iter():
            if 'selection' not in node.get('class','').split(): continue
            ident=node.get('id'); common.require(ident in hids,'Unknown Razer selector '+str(ident))
            path=None
            if node.tag.endswith('rect'):
                x,y,w,h=[float(node.get(k)) for k in ('x','y','width','height')]
            elif node.tag.endswith('path'):
                path=parse_path(node.get('d')); xmin,xmax,ymin,ymax=path.bbox();x,y,w,h=xmin,ymin,xmax-xmin,ymax-ymin
            else: raise ValueError('Unknown SVG selection shape')
            scale=42/34.47
            hid=hids[ident]; common.require(hid in LABELS,'Unknown Razer HID')
            key=dict(hid=hid,label=LABELS[hid],x=roundpx(x*scale),y=roundpx(y*scale),
                     w=roundpx((x+w)*scale)-roundpx(x*scale),h=roundpx((y+h)*scale)-roundpx(y*scale))
            if ident=='selEnter' and variant!='ANSI':
                lines=[s for s in path if isinstance(s,Line) and abs(s.start.real-s.end.real)<.001
                       and abs(s.start.imag-s.end.imag)>h*.25 and x+3<s.start.real<x+w-3]
                common.require(len(lines)==1,'Unreviewed Razer Enter outline')
                line=lines[0]; key['notchW']=roundpx(line.start.real*scale)-key['x']
                # Recover the sharp corner before the concave rounding, not
                # the tangent at the end of its curve (JIS has a 5.06px radius).
                previous=path[list(path).index(line)-1]
                notch_y=min(line.start.imag,line.end.imag)
                if not isinstance(previous,Line): notch_y=min(notch_y,previous.start.imag)
                key['notchY']=roundpx(notch_y*scale)-key['y']
            keys.append(key)
        reports.append(report('Razer','Huntsman V3 Pro Mini',variant,keys,['razer-mini.js',svg,button]))
    return reports

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output-dir',required=True);args=parser.parse_args()
    reports=nuphy()+wooting()+razer()
    common.require(len({r['id'] for r in reports})==len(reports),'Duplicate stage identity')
    out=Path(args.output_dir);common.require(not out.exists(),'Stage already exists')
    out.mkdir(parents=True)
    for item in reports:
        with (out/(item['id']+'.json')).open('x',encoding='utf-8') as stream:json.dump(item,stream,ensure_ascii=True,indent=2)
        print(item['name'],len(item['keys']))
    print('PREPARED',len(reports))

if __name__=='__main__':main()
