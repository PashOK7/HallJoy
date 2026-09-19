"""Execute extracted read-handler code with synthetic samples, without USB access."""
from pathlib import Path
import hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/ipi-reverse-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
P=ROOT/'docs/research/ipi-firmware-20260914'
RAM=0x20000000; META=0x200d0000; PACKET=0x200d1000; TEMP=0x200d2000; OUTPUT=0x200e0000; STACK=0x200f0000

def run(row,ids,variant):
 data=(P/row['file']).read_bytes();assert hashlib.sha256(data).hexdigest()==row['sha256']
 uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB);uc.mem_map(0x08000000,0x100000);uc.mem_write(0x08008000,data);uc.mem_map(RAM,0x100000)
 text='\n'.join(row['addressed_code'])
 regs={UC_ARM_REG_SP:STACK,UC_ARM_REG_R8:OUTPUT,UC_ARM_REG_R9:TEMP}
 regnames={'r'+str(i):globals()['UC_ARM_REG_R'+str(i)] for i in range(13)}
 regnames.update(sb=UC_ARM_REG_R9,sl=UC_ARM_REG_R10,fp=UC_ARM_REG_R11)
 packetreg=re.findall(r'add.w r0, (\w+), r[01], lsl #6',text)[-1]
 metareg=re.findall(r'ldrb r1, \[(\w+), #7\]',text)[-1]
 for name,value in [(packetreg,PACKET),(metareg,META)]:
  alias=re.search(r': mov '+name+r', (\w+)\n',text)
  regs[regnames[alias[1] if alias else name]]=value
 for name in re.findall(r'strb(?:.w)? r1, \[(\w+), #1\]',text):
  if name!='r7':regs[regnames[name]]=TEMP
 for r,v in regs.items():uc.reg_write(r,v)
 packet=bytearray(64);packet[:3]=bytes([9,0x94,2]);packet[6]=len(ids)*2
 for i,key in enumerate(ids):
  offset=8 if variant=='halljoy' else 7
  packet[offset+i*2:offset+i*2+2]=key.to_bytes(2,'little' if variant=='halljoy' else 'big')
 uc.mem_write(PACKET,bytes(packet));seen=[]
 start=int(row['addressed_handler'],16);addresses=[int(x.split(':')[0],16) for x in row['addressed_code']]
 finished=[]
 def hook(u,a,size,user):
  if a not in addresses and not int(row['id_lookup'],16)<=a<int(row['id_lookup'],16)+128 and a!=int(row['sample_reader'],16):
   finished.append(a);u.emu_stop();return
  if a==int(row['sample_reader'],16):
   matrix=u.reg_read(UC_ARM_REG_R0);seen.append(matrix)
   u.mem_write(u.reg_read(UC_ARM_REG_R1),struct.pack('<H',0x5566))
   u.reg_write(UC_ARM_REG_R0,0x9234);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 uc.hook_add(UC_HOOK_CODE,hook)
 try:uc.emu_start(start|1,0,count=30000)
 except Exception as error:raise RuntimeError((row['file'],hex(uc.reg_read(UC_ARM_REG_PC)),variant)) from error
 assert finished,(row['file'],'did not finish')
 wanted=b''.join(k.to_bytes(2,'big')+bytes.fromhex('92345566') for k in ids)
 mem=bytes(uc.mem_read(RAM,0x100000));match=mem.find(wanted)
 if variant=='official':assert len(seen)==len(ids) and match>=0,(row['file'],seen,match)
 else:assert len(seen)==len(ids) and match>=0,(row['file'],'current-HallJoy byte compatibility failed',seen)
 return dict(variant=variant,requested=ids,sample_calls=len(seen),exact_records=match>=0)

def read_sample(row,raw,flag,secondary):
 from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB
 data=(P/row['file']).read_bytes();uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
 uc.mem_map(0x08000000,0x100000);uc.mem_write(0x08008000,data);uc.mem_map(RAM,0x100000)
 uc.reg_write(UC_ARM_REG_SP,STACK);uc.reg_write(UC_ARM_REG_LR,0x08000001);uc.reg_write(UC_ARM_REG_R0,1);uc.reg_write(UC_ARM_REG_R1,OUTPUT)
 m=Cs(CS_ARCH_ARM,CS_MODE_THUMB);a=int(row['raw_reader'],16)
 raw_ins=list(m.disasm(data[a-0x08008000:a-0x08008000+48],a))
 load=next(x for x in raw_ins if x.mnemonic=='ldr.w' and x.op_str.startswith('r0, [r0, #'))
 raw_offset=int(re.search(r'#(0x[0-9a-f]+)',load.op_str)[1],16)
 divisor=192 if '#0xc0' in row['raw_reader_code'] else 128
 value_lines=row['sample_reader_code']
 flagline=next(x for x in value_lines if 'ldrb.w r3, [r1, #' in x)
 depthline=next(x for x in value_lines if ': ldrh r1, [r1, #' in x)
 def offset(line):return int(re.search(r'#(0x[0-9a-f]+)',line)[1],16)
 def hook(u,pc,size,user):
  if pc==0x08000000:u.emu_stop();return
  if pc==load.address:u.mem_write(u.reg_read(UC_ARM_REG_R0)+raw_offset,struct.pack('<I',raw*divisor))
  if pc==int(flagline[:8],16):u.mem_write(u.reg_read(UC_ARM_REG_R1)+offset(flagline),bytes([flag]))
  if pc==int(depthline[:8],16):u.mem_write(u.reg_read(UC_ARM_REG_R1)+offset(depthline),struct.pack('<H',secondary))
 uc.hook_add(UC_HOOK_CODE,hook);uc.emu_start(int(row['sample_reader'],16)|1,0,count=300)
 actual=uc.reg_read(UC_ARM_REG_R0);out=struct.unpack('<H',uc.mem_read(OUTPUT,2))[0]
 assert actual==raw|(flag<<15) and out==secondary,(row['file'],actual,out)
 return dict(raw=raw,flag=flag,secondary=secondary,accumulator_divisor=divisor)

def main():
 rows=json.loads((P/'firmware-audit.json').read_bytes());results=[]
 for row in rows:
  cases=[]
  for count in [1,4,9]:
   ids=row['physical_ids'][:count]
   for variant in ['official','halljoy']:cases.append(run(row,ids,variant))
  samples=[read_sample(row,raw,flag,0x1234) for raw in [1800,8400,10112] for flag in [0,1]]
  results.append(dict(file=row['file'],uuid=row['uuid'],cases=cases,sample_cases=samples))
 blob=(json.dumps(results,indent=2)+'\n').encode();out=P/'record-and-sample-emulation.json'
 if out.exists():assert out.read_bytes()==blob
 else:
  with out.open('xb') as f:f.write(blob)
 print('IPI_RECORD_EMULATION=PASS images='+str(len(rows))+' cases='+str(sum(len(r['cases']) for r in results)))
 print('Scope: real ID lookup/serializer with stubbed samples plus real sample/raw readers with synthetic RAM; USB entry/ADC not emulated.')
if __name__=='__main__':main()
