"""Offline IPI firmware command/geometry audit. Never runs updater executables."""
from pathlib import Path
import ast
import hashlib
import json
import re
import struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'docs/research/ipi-firmware-20260914'
LOCKS={'vendor-0013b2866640c78d694dc4ae7503e7d7-0-0.bin': '78d313eb1ae5037e194e3127c0a9b65118179144391b5fe655f6f8a8be0c7b29', 'vendor-063d20c5488802e611533ffc59e2f7aa-0-0.bin': '73df383b3094dc4102d81d71629e1f6da5175fe95b5062f0f6cc65ebddca8353', 'vendor-16235907b003d97bd15c2541aa09dddc-0-0.bin': '5ba73c19f1c582110346a86c19f71551da1b2d1a276aecafaa3200e07354d5ab', 'vendor-1af361a4bf28e9ea517b02837d603783-0-0.bin': 'ce64c226c5815ae2c691250eb31a1e979a60e4996f1c568318b0cff51c52d283', 'vendor-1c097aecd3cb5c413bde351336bf195a-0-0.bin': 'bd29744af73347acd5328393ed3a52de50809790fe4f73f2dc89dbec461fc2b3', 'vendor-211e02f538a1b5c0f1c59a11987bb7a0-0-0.bin': 'a796cc597fce1797508b827dfe6726bf002f47361bd3ca230251c8cf0cb73078', 'vendor-29e74b0d0b1e1ed5a3360751fdbeba80-0-0.bin': 'd206136a10051953e9f9494a1f2bedad497d47c229e2af1bc5f22b11a50bbd8f', 'vendor-4f019f99b4ca0d460efbfe164097cfa0-0-0.bin': '8664641b9a299ee950cb5cee522190dd90ff8193c3818bc4fcdaee4130fa3558', 'vendor-7e8f8a700bbc99aff4a3da6191b34326-0-0.bin': 'f925119b0eaf719c13ce696daea13948b67b3130db1351b0d3bd4c4189d35ad3', 'vendor-93db7497790cc46cbc22dd6dae5cb3d8-0-0.bin': '5ba73c19f1c582110346a86c19f71551da1b2d1a276aecafaa3200e07354d5ab', 'vendor-b88e40c05a4e27254cca378183a1c049-0-0.bin': 'e9628cb5b435c6974c28d0b0b46238e1aa253c8423c15aa460117d56c5c6f30d', 'vendor-bbb89e79bc76731cac50473fab7a4b30-0-0.bin': '6303dbb0869b3ee9b93b2d07258ea72329249687f15012835429e0b1cb905160', 'vendor-df5505cdc4e1f6a422a98411012e5aa7-0-0.bin': 'f7adae8ee38e5acb066935324f81a5f0e93847ea96ea2bdc9446bb543f27c2fc', 'vendor-e4c703022c7a22b43b049e0db1538e1e-0-0.bin': '105af47e1579af123270e4494c9c2a1c8c7f2c0b4960879ec3a80cec4bae721e', 'package0-firmware.bin': '7fa165ecf181285db8d1b4da56636f209cd263b53dd9a51ab806414208fdeaf9', 'package1-firmware.bin': '7298caf97873a45508d86a8258e0ca0f41d5f779b012b3cd8fe37791f9ebdd68', 'package2-firmware.bin': 'd83b237fb247a06302cb8631251503875624f072108c84b07637b433e6361ea1', 'package3-firmware.bin': '1872b107814d6f814504cb85d5ecf643459b6812f5ae136465169d8270e3e05c'}
BASE=0x08008000
md=Cs(CS_ARCH_ARM,CS_MODE_THUMB);md.skipdata=True

def audit(name):
 data=(SOURCE/name).read_bytes()
 assert hashlib.sha256(data).hexdigest()==LOCKS[name]
 assert data[:2]==b'KB'
 uuid='0x'+data[2:8].hex()
 sp,reset=struct.unpack_from('<II',data,0x8000)
 assert 0x20000000<sp<0x20080000 and 0x08010000<reset<0x08020000 and reset&1
 def dis(a,size):return list(md.disasm(data[a-BASE:a-BASE+size],a))
 def listing(a,size):return [f'{x.address:08x}: {x.mnemonic} {x.op_str}' for x in dis(a,size)]
 code=dis(0x08010000,len(data)-0x8000)
 matches=[i for i,x in enumerate(code) if x.mnemonic=='sub.w' and x.op_str.endswith(', #0x82') and any(y.mnemonic=='tbh' for y in code[i+1:i+5])]
 assert len(matches)<=1
 if matches:
  at=next(x.address for x in code[matches[0]+1:matches[0]+5] if x.mnemonic=='tbh')
  table=at+4
  cmd=lambda c:table+2*struct.unpack_from('<H',data,table-BASE+2*(c-0x82))[0]
 else:
  def cmd(c):
   hits=[i for i,x in enumerate(code) if x.mnemonic=='cmp' and x.op_str==f'r1, #{hex(c)}' and code[i+1].mnemonic in ('beq','bne')]
   assert len(hits)==1,(name,c,hits)
   i=hits[0];branch=code[i+1] if code[i+1].mnemonic=='beq' else code[i+2]
   assert branch.mnemonic in ('beq','b','b.w')
   return int(branch.op_str[1:],16)
  at=next(x.address for x in code if x.mnemonic=='cmp' and x.op_str=='r1, #0x94')
 handler=cmd(0x94)
 for _ in range(3):
  first=dis(handler,4)[0]
  if first.mnemonic not in ('b','b.w'):break
  handler=int(first.op_str[1:],16)
 switch=next(x.address for x in dis(handler,32) if x.mnemonic=='tbb');subtable=switch+4
 start=subtable+2*data[subtable-BASE+2];end=subtable+2*data[subtable-BASE+3]
 body=dis(start,end-start);calls=[int(x.op_str[1:],16) for x in body if x.mnemonic=='bl']
 assert len(calls)==2,(name,'two key/value helpers',calls)
 lookup,value=calls
 def function(a):
  result=[]
  for ins in dis(a,128):
   result.append(ins)
   if ins.mnemonic=='bx' and ins.op_str=='lr' or ins.mnemonic.startswith('pop') and 'pc' in ins.op_str:break
  return result
 lookup_body=function(lookup);value_body=function(value)
 literal=next(x for x in lookup_body if x.mnemonic=='ldr' and x.op_str.startswith('r2, [pc,'))
 displacement=int(re.search(r'#(0x[0-9a-f]+|\d+)',literal.op_str)[1],0)
 idptr=struct.unpack_from('<I',data,((literal.address+4)&~3)+displacement-BASE)[0]
 count=int(next(x.op_str.split('#')[1] for x in lookup_body if x.mnemonic=='cmp' and x.op_str.startswith('r0, #')),0)
 ids=list(struct.unpack_from('<'+str(count)+'H',data,idptr-BASE));present=sorted(set(ids)-{0})
 reader_calls=[int(x.op_str[1:],16) for x in value_body if x.mnemonic=='bl'];assert len(reader_calls)==1
 raw_body=function(reader_calls[0]);raw_text='\n'.join(x.mnemonic+' '+x.op_str for x in raw_body)
 record_text='\n'.join(x.mnemonic+' '+x.op_str for x in body)
 assert 'lsl #1' in record_text and '#7]' in record_text and '#8]' in record_text
 assert '#2]' in record_text and '#3]' in record_text and '#4]' in record_text and '#5]' in record_text
 geometry=[]
 for f in (ROOT/'docs/research/remaining-layout-sources-20260914').glob('ipi-'+uuid+'-*.js'):
  s=f.read_text(encoding='utf-8');keys=ast.literal_eval('['+s.split("'keycaps':[",1)[1].split(']};',1)[0]+']');expected={k['id'] for k in keys}
  geometry.append(dict(file=f.name,count=len(expected),missing=sorted(expected-set(present)),extra=sorted(set(present)-expected)))
 return dict(file=name,sha256=LOCKS[name],uuid=uuid,bytes=len(data),base=hex(BASE),vector_file_offset='0x8000',
  dispatch=hex(at),map_handler=hex(cmd(0x83)),travel_handler=hex(handler),addressed_handler=hex(start),
  id_lookup=hex(lookup),id_table=hex(idptr),matrix_slots=count,physical_ids=present,geometry=geometry,
  sample_reader=hex(value),raw_reader=hex(reader_calls[0]),raw_reader_code=raw_text,
  addressed_code=listing(start,end-start),sample_reader_code=[f'{x.address:08x}: {x.mnemonic} {x.op_str}' for x in value_body],
  map_entry_code=listing(cmd(0x83),40))

def main():
 rows=[audit(name) for name in LOCKS]
 target=SOURCE/'firmware-audit.json';blob=(json.dumps(rows,indent=2)+'\n').encode()
 if target.exists():assert target.read_bytes()==blob,'Audit output drift'
 else:
  with target.open('xb') as f:f.write(blob)
 for r in rows:print(r['uuid'],r['file'],r['addressed_handler'],len(r['physical_ids']),r['geometry'])
 print('IPI_FIRMWARE_STATIC_AUDIT=PASS images='+str(len(rows)))
if __name__=='__main__':main()
