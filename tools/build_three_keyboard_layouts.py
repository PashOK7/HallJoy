"""Offline extraction of pinned manufacturer layouts for the three USB backends."""
from pathlib import Path
import hashlib,json,re,struct
import layout_import as common
import layout_pipeline as pipeline
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'docs/research/three-keyboard-sources'
LOCKS={
'm1v5-geometry.js':('1bea282276d5bfe3f369054b639693bde8cdd32bc088c603d3b0928b2ff0a693','https://app.monsgeek.com/js/2771d791.js'),
'epomaker-hub.js':('9b717152b4c5bd48e6787f6adeb3d6239bfcc4117f5cdea330b1f94bce0779e3','https://hub.epomaker.com/assets/index-niFDUSiN.js'),
'slice-console.js':('63c69b63836ba2a223d82d14a8c7eec3bd57e9041627b9fdbde9370178521d69','https://slice75he.chilkey.com/assets/index-BvHTL_W_.js')}
def read(name):
 b=(SRC/name).read_bytes();assert hashlib.sha256(b).hexdigest()==LOCKS[name][0];return b.decode('utf-8')
def prepare():
 labels={hid:label for hid,label in common.CODES.values()};labels[1033]='Fn'
 hids={f'Key{c}':i+4 for i,c in enumerate('ABCDEFGHIJKLMNOPQRSTUVWXYZ')}
 hids.update({f'Digit{c}':i+30 for i,c in enumerate('1234567890')});hids.update({f'F{i+1}':58+i for i in range(12)})
 hids.update(dict(Escape=41,Backspace=42,Tab=43,Space=44,Minus=45,Equal=46,BracketLeft=47,BracketRight=48,Backslash=49,Semicolon=51,Quote=52,Backquote=53,Comma=54,Period=55,Slash=56,CapsLock=57,Enter=40,Delete=76,Insert=73,Home=74,End=77,PageUp=75,PageDown=78,ArrowLeft=80,ArrowRight=79,ArrowUp=82,ArrowDown=81,ControlLeft=224,ShiftLeft=225,AltLeft=226,MetaLeft=227,ControlRight=228,ShiftRight=229,AltRight=230,Fn=1033))
 reports=[]
 def report(brand,model,id,keys,protocol,products,source):
  r=dict(schema=1,id=id,brand=brand,model=model,variant='ANSI',name=f'{brand} {model} ANSI',status='ready',unresolved=[],keys=keys,identity=dict(protocol=protocol,products=products),sources=[dict(path=(SRC/source).relative_to(ROOT).as_posix(),sha256=LOCKS[source][0],url=LOCKS[source][1])],notes=['Official manufacturer geometry; USB experimental native integration. No physical-device test claimed.'])
  pipeline.validate_report(r);reports.append(r)
 s=read('m1v5-geometry.js');keys=[]
 for m in re.finditer(r'(\w+):\{x:(\d+),y:(\d+),width:(\d+),height:(\d+),type:"key"',s):
  name,x,y,w,h=m.groups();hid=hids[name];keys.append(dict(hid=hid,label=labels[hid],x=round(int(x)*1.05),y=round(int(y)*1.05),w=round(int(w)*1.05),h=round(int(h)*1.05)))
 assert len(keys)==82
 report('MonsGeek','M1 V5 HE','monsgeek_m1_v5_he_ansi',keys,'rongyuan-snapshot',['M1V5HE-2819'],'m1v5-geometry.js')
 s=read('epomaker-hub.js');a=s.index('ks={name:');z=s.index(']}',a);text=s[a:z];keys=[]
 for c in text[text.index('keys:[')+6:].split('},{'):
  code=re.search(r'keyCode:R\.(\w+)',c)[1]
  y,x,w,h,index=[float(re.search(k+r':([\d.]+)',c)[1]) for k in ('row','col','width','height','matrixIndex')]
  hid=1033 if code=='Fn' else int(re.search(r'tableCode:(\d+)',c)[1])
  keys.append(dict(hid=hid,label=labels[hid],x=round(x*46),y=round(y*46),w=round(w*46)-4,h=round(h*46)-4))
 assert len(keys)==84
 report('EPOMAKER','G84 HE','epomaker_g84_he_ansi',keys,'rongyuan-snapshot',['G84HE-2642','G84HE-2959'],'epomaker-hub.js')
 s=read('slice-console.js')
 def arr(name):
  a=s.index(name+':')+len(name)+1;d=0
  for z in range(a,len(s)):
   if s[z]=='[':d+=1
   if s[z]==']':
    d-=1
    if d==0:return json.loads(re.sub(r'(?<=[,\[])\.(?=\d)', '0.', s[a:z+1]))
 width,height,left,right,bottom=[arr('Keyboard_Layout_'+x+'_80B') for x in ['width','height','marginLeft','marginRight','marginBottom']]
 # Pinned official Slice75 HE 1.1.7.3 matrix at file offset 0x4B8.
 matrix=[41,58,59,60,61,62,63,64,65,66,67,68,69,73,76,0,0,0,0,0,0,53,30,31,32,33,34,35,36,37,38,39,45,46,42,75,0,0,0,0,0,0,43,20,26,8,21,23,28,24,12,18,19,47,48,49,78,0,0,0,0,0,0,57,4,22,7,9,10,11,13,14,15,51,52,0,40,0,0,0,0,0,0,0,225,0,29,27,6,25,5,17,16,54,55,56,0,229,82,0,0,0,0,0,0,224,227,226,0,0,0,44,0,0,0,0,61441,228,80,81,79,0,0,0,0,0]
 keys=[];y=0
 for row in range(6):
  x=0;rowheight=0
  for col in range(21):
   hid=matrix[row*21+col]
   if not hid:continue
   if hid==0xf001:hid=1033
   x+=left[row][col]*46
   keys.append(dict(hid=hid,label=labels[hid],x=round(x),y=round(y),w=round(width[row][col]*46)-4,h=round(height[row][col]*46)-4))
   x+=(width[row][col]+right[row][col])*46;rowheight=max(rowheight,(height[row][col]+bottom[row][col])*46)
  y+=rowheight
 assert len(keys)==80
 report('Chilkey','Slice75 HE','chilkey_slice75_he_ansi',keys,'chilkey-slice75',['SLICE75-1CA3-0701'],'slice-console.js')
 return reports
if __name__=='__main__':
 import argparse
 ap=argparse.ArgumentParser();ap.add_argument('--write-new',action='store_true');args=ap.parse_args()
 for r in prepare():
  p=ROOT/'docs/research/three-keyboard-layout-reports'/(r['id']+'.json');b=(json.dumps(r,indent=2)+'\n').encode()
  if args.write_new:
   p.parent.mkdir(exist_ok=True)
   with p.open('xb') as f:f.write(b)
  else:assert p.read_bytes()==b
  print(r['name'],len(r['keys']),'PASS')
