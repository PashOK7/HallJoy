"""Static extraction of the pinned official IROK MG75 Pro/Max geometry.

No vendor JavaScript execution, HID access or firmware changes.
"""
import argparse
import hashlib
import json
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_import as common
import layout_pipeline as pipeline

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/research/irok-mg75-layout-sources/index-BQvZQSA6.js'
HASH='2b80dd9398565e13ebf315351af96c3bfcf059fa917a774638b0be42c69c44dc'

def literal_array(text,name):
    match=re.search(r'\b'+re.escape(name)+r'=([\[][0-9,\[\]]+)',text)
    common.require(match is not None,'Missing numeric array '+name)
    # Stop at the closing bracket of this initializer; adjacent declarations
    # use commas, which are also valid inside an array.
    start=match.start(1);depth=0
    for end in range(start,len(text)):
        if text[end]=='[':depth+=1
        elif text[end]==']':
            depth-=1
            if not depth:return json.loads(text[start:end+1])
    raise ValueError('Unclosed array '+name)

def model_class(text,name):
    match=re.search(r'class '+name+r'\b.*?(?=class |$)',text)
    common.require(match is not None,'Missing class '+name)
    return match[0]

def prepare():
    raw=SOURCE.read_bytes();common.require(hashlib.sha256(raw).hexdigest()==HASH,'Source lock mismatch')
    text=raw.decode('utf-8')
    expected='Qn=[[...Pi,ds[0][0],ds[2][0]],[...it[0],ds[1][0]],[...it[1],ds[1][2]],[...it[2],ds[2][2]],[...it[3],...Ot[0]],[...Io,...Ot[1]]]'
    common.require(expected in text,'Unreviewed row order')
    pi,it,ds,ot,io=[literal_array(text,n) for n in ('Pi','it','ds','Ot','Io')]
    rows=[pi+[ds[0][0],ds[2][0]],it[0]+[ds[1][0]],it[1]+[ds[1][2]],it[2]+[ds[2][2]],it[3]+ot[0],io+ot[1]]
    common.require('class hDt extends W5t' in text and 'new hDt' in text and 'new wDt' in text,'Model inheritance changed')
    common.require('Yt.set(h.IYXPolar75,{w:36.5,h:36.5,x:2,y:2})' in text,'Default key dimensions changed')
    # Browser VK -> USB HID, independent of the manufacturer's matrix address.
    vk_hid={ord(c):i+4 for i,c in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ')}
    vk_hid.update({ord(c):i+30 for i,c in enumerate('1234567890')})
    vk_hid.update({112+i:58+i for i in range(12)})
    vk_hid.update({27:41,8:42,9:43,13:40,32:44,192:53,189:45,187:46,219:47,221:48,220:49,186:51,222:52,20:57,188:54,190:55,191:56,44:70,145:71,19:72,45:73,36:74,33:75,46:76,35:77,34:78,37:80,38:82,39:79,40:81,160:225,161:229,162:224,163:228,164:226,165:230,91:227,93:101,250:1033})
    labels={hid:label for hid,label in common.CODES.values()};labels[1033]='Fn'
    # Max uses matrix addresses for custom geometry; the HID-based Pro uses
    # HID usages (with the driver's private Fn selector 1). Only overridden
    # addresses are needed: all other positions use the default square.
    max_overrides={1:58,5:62,9:66,13:70,256:53,257:30,258:31,259:32,260:33,261:34,262:35,263:36,264:37,265:38,266:39,267:45,268:46,269:42,270:73,512:43,525:49,768:57,781:40,1024:225,1036:229,1280:224,1281:227,1282:226,1286:44,1290:230,1291:1033,1292:80}
    reports=[]
    for model,cls in [('MG75 Max','wDt'),('MG75 Pro','W5t')]:
        body=model_class(text,cls)
        custom=re.search(r'r\(this,"customLayout",\{(.*?)\}\)',body)[1]
        overrides={};matches=list(re.finditer(r'(\d+):this.createSquare\(([^)]*)\)',custom))
        common.require(re.sub(r'\d+:this.createSquare\([^)]*\),?','',custom)=='','Unparsed geometry')
        for match in matches:
            selector=int(match[1]);hid=max_overrides[selector] if cls=='wDt' else 1033 if selector==1 else selector
            args=[Decimal(a.replace('this.y_number','10')) for a in match[2].split(',')]
            defaults=[Decimal(2),Decimal(2),Decimal('36.5'),Decimal('36.5')]
            defaults[:len(args)]=args;overrides[hid]=defaults
        if cls=='wDt':common.require(len(matches)==len(max_overrides),'Unreviewed Max override selectors')
        keys=[];top=Decimal(0)
        for row in rows:
            left=Decimal(0);row_height=Decimal(0)
            for vk in row:
                hid=vk_hid[vk];x,y,w,h=overrides.get(hid,[Decimal(2),Decimal(2),Decimal('36.5'),Decimal('36.5')])
                keys.append(dict(hid=hid,label=labels[hid],x=left+x,y=top+y,w=w,h=h))
                left+=x+w;row_height=max(row_height,y+h)
            top+=row_height
        minx=min(k['x'] for k in keys);miny=min(k['y'] for k in keys)
        for k in keys:
            k['x']-=minx;k['y']-=miny
            for field in ('x','y','w','h'):k[field]=int((k[field]*Decimal(42)/Decimal('36.5')).quantize(Decimal(1),rounding=ROUND_HALF_UP))
        report=dict(schema=1,id='irok_'+model.lower().replace(' ','_')+'_ansi',brand='IROK',model=model,variant='ANSI',name='IROK '+model+' ANSI',status='ready',unresolved=[],keys=keys,
            identity=dict(protocol='sparklink' if model=='MG75 Max' else 'irok-mg75-pro',products=[] if model=='MG75 Max' else ['MG75PRO-1CA5-0807']),sources=[dict(path=str(SOURCE.relative_to(ROOT)).replace('\\','/'),sha256=HASH,url='https://hid.irok.cn/assets/index-BQvZQSA6.js')],
            notes=['Official model geometry, normalized to 42 px standard keys.',('Manual preset selection: no new exact-model identity published by SparkLink.' if model=='MG75 Max' else 'Automatic selection after exact product, factory matrix and travel response proof; live base-layer remaps.'),('Fn action 0xF101 now maps to HallJoy 0x409 through SparkLink; hardware validation pending.' if model=='MG75 Max' else 'SparkLink V1 native independent 6x21 travel; Fn compact matrix index 116. Experimental until tested on hardware.'),'MG75 v2 is excluded.'])
        common.require(len(keys)==81,'Unexpected MG75 key count');pipeline.validate_report(report);reports.append(report)
    return reports

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write-new',action='store_true');args=parser.parse_args()
    for report in prepare():
        p=ROOT/'docs/research/irok-mg75-layout-reports'/(report['id']+'.json')
        data=(json.dumps(report,indent=2)+'\n').encode()
        if args.write_new:
            p.parent.mkdir(parents=True,exist_ok=True)
            with p.open('xb') as out:out.write(data)
        else:common.require(p.read_bytes()==data,'Reviewed report changed')
        print(report['name']+': 81 keys, source/geometry PASS')
if __name__=='__main__':main()
