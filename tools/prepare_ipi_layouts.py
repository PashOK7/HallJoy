"""Source-locked IPI physical layouts and official demo defaults; no device I/O."""
import argparse
import ast
from decimal import Decimal, ROUND_HALF_UP
import hashlib
import json
from pathlib import Path
import re
import layout_import as common
import layout_pipeline as pipeline
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/remaining-layout-sources-20260914'
LOCKS = {'ipi-vendor-bytech-Dmk7DNgV.js': '5c51ebb210ae87614d9ab5412695e592066a535489687e480e464878d43ea022', 'ipi-demo-device-session-CMlZtzIh.js': '1e1ba311bfde0eb24183847b582a73a7d2331efbe3fa1e5bb490ce044f1f4c26', 'ipi-keyboard-index-DgQeyrdV.js': '10a6804779ef9d0d6fd83ff579468dcbfed1445877d5f54a453b280492c521e0', 'ipi-devices.json': '9a0a83cf312faff353efeced08cc4611dcefedd9fd1d7ac4dd5d9afecd240f01', 'ipi-0x110000000006-CjMPaV7w.js': 'eb01f156e761a6dc88e663bb4d626356a8033dd58a9780ad7076bc6645822597', 'ipi-0x110000000010-BvH-X4CH.js': 'b750de6070fddd1f312f105141b175d62d6b68a58c47356741090dbdd61ac88d', 'ipi-0x110000000013-jdvj5dvW.js': '4c49f95f62165502d66b56400e8087bf56baadfc7e2e52c84a65a75bd9da5feb', 'ipi-0x11000000001f-BvH-X4CH.js': '473301ad3c031a187dd43fbf8d6f36f91f723593f23ca79fb371cbef37265250', 'ipi-0x110000000023-BvH-X4CH.js': '6bc75f9907863a8a469148d6b4786caea88b1d0b5a1362d29c4290954623c6c6', 'ipi-0x11000000002c-jdvj5dvW.js': '8e5922f37a1ec69f079d3c7771f4de39d6f1833b1bc48fbac4c7959372fa89c7', 'ipi-0x110000000040-2mqRiCo3.js': '0c673eb4b147b3ec12231a5269a3fe7c9413a4c5e4ea8a56eea7108c3d3e3d02', 'ipi-0x120000000003-BvH-X4CH.js': 'e02fd8ac70b64824100a54274f1ba25ca5e854cd90f73673ac28df6306495d29'}
GROUPS = [
 ('ipi_qbz75_aurora75_ansi', 'QBZ75 / Aurora 75', 82, ['0x11000000002c-jdvj5dvW.js','0x110000000013-jdvj5dvW.js']),
 ('ipi_qbz65_aurora65_rain65_ansi', 'QBZ65 / AURORA65 / AURORA65W / RAIN65', 67, ['0x110000000023-BvH-X4CH.js','0x110000000010-BvH-X4CH.js','0x120000000003-BvH-X4CH.js','0x11000000001f-BvH-X4CH.js']),
 ('ipi_aurora75_pro_ansi', 'Aurora75 PRO', 82, ['0x110000000040-2mqRiCo3.js']),
 ('ipi_flash68_ansi', 'flash68', 68, ['0x110000000006-CjMPaV7w.js']),
]
def prepare():
 for name, digest in LOCKS.items():
  common.require(hashlib.sha256((SOURCE/name).read_bytes()).hexdigest()==digest, 'Source drift: '+name)
 text=(SOURCE/'ipi-vendor-bytech-Dmk7DNgV.js').read_text(encoding='utf-8')
 common.require('pa as J,' in text, 'Default export changed')
 table=text.split('pa=[',1)[1].split('],',1)[0]
 pairs=re.findall(r'keycode:(\d+),id:(\d+)',table)
 defaults={int(i):int(h) for h,i in pairs}
 common.require(len(defaults)==len(pairs)==145,'Default table changed')
 demo=(SOURCE/'ipi-demo-device-session-CMlZtzIh.js').read_text(encoding='utf-8')
 common.require('J as _0x57d4b9' in demo and "R=new Map(_0x57d4b9['map']" in demo, 'Demo association changed')
 labels={hid:label for hid,label in common.CODES.values()}; labels[0x409]='Fn'
 def hid(value):
  if value==218103808:return 0x409
  if value>=65536:
   bit=value>>16
   common.require(value==bit<<16 and bit>0 and bit&(bit-1)==0 and bit<=128,'Unknown modifier')
   return 224+bit.bit_length()-1
  common.require(value in labels,'Unknown keyboard HID')
  return value
 common.require(hid(defaults[69])==226 and hid(defaults[71])==230 and hid(defaults[66])==229,'Modifier defaults changed')
 catalog=json.loads((SOURCE/'ipi-devices.json').read_bytes())['data']
 index=(SOURCE/'ipi-keyboard-index-DgQeyrdV.js').read_text(encoding='utf-8')
 def geometry(name):
  common.require(name in index,'Missing UUID module')
  uuid=int(name.split('-')[0],16)
  common.require(any(d['uuid']==uuid and d['type']=='keyboard' for d in catalog),'Missing public model identity')
  source=(SOURCE/('ipi-'+name)).read_text(encoding='utf-8')
  return ast.literal_eval('['+source.split("'keycaps':[",1)[1].split(']};',1)[0]+']')
 def px(value):return int((Decimal(str(value))*46).quantize(Decimal(1),rounding=ROUND_HALF_UP))
 reports=[]
 for ident,model,count,files in GROUPS:
  raw=geometry(files[0])
  common.require(len(raw)==count,'Physical count changed')
  for name in files[1:]:common.require(geometry(name)==raw,'Merged layouts differ')
  keys=[]
  for k in raw:
   common.require(set(k)=={'id','x','y','width','height'},'Unsupported compound geometry')
   h=hid(defaults[k['id']]); x,y=k['x'],k['y']
   keys.append(dict(hid=h,label=labels[h],x=px(x),y=px(y),w=px(x+k['width'])-px(x)-4,h=px(y+k['height'])-px(y)-4))
  sources=[]
  for name in list(LOCKS)[:4]+['ipi-'+f for f in files]:
   url='https://api.hubx.pro/v1/device/devices' if name=='ipi-devices.json' else 'https://qbz.ipigame.cn/keyboard/assets/'+name.removeprefix('ipi-').replace('keyboard-index-','index-')
   sources.append(dict(path=(SOURCE/name).relative_to(ROOT).as_posix(),sha256=LOCKS[name],url=url))
  result=dict(schema=1,id=ident,brand='IPI',model=model,variant='ANSI',name='IPI '+model.replace(' / ', ' + ')+' ANSI',status='ready',unresolved=[],keys=keys,
   identity=dict(protocol='ipi-addressed',products=[f.split('-')[0][2:].upper() for f in files]),sources=sources,notes=[
    'Physical UUID-specific geometry with official demo default ID/keycode association; not a captured user keymap.',
    'Manufacturer units normalized to HallJoy 46px pitch and 4px gap; no row reflow.',
    'Identical physical ID/geometry lists are merged after exact comparison.',
    'Exact UUID identities are eligible only after native session proof; wireless forwarding is not inferred.',
    'The IPI native route reads an explicit live map and per-key calibration; no shared-PID calibration seeds.',
    'Vendor Fn maps to 0x409 in the native IPI route. Plus revisions remain excluded.'])
  pipeline.validate_report(result);reports.append(result)
 return reports

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--write-new',action='store_true');args=parser.parse_args()
 for report in prepare():
  p=ROOT/'docs/research/remaining-layout-reports-20260914'/(report['id']+'.json')
  data=(json.dumps(report,indent=2)+'\n').encode()
  if args.write_new:
   with p.open('xb') as f:f.write(data)
  else:common.require(p.read_bytes()==data,'Report drift: '+report['id'])
  print(report['name'],len(report['keys']))
 print('IPI_LAYOUTS=PASS presets=4 models=8')
if __name__=='__main__':main()
