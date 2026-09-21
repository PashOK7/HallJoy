"""Execute the audited O3C read handlers with synthetic RAM; no hardware writes."""
from pathlib import Path
import hashlib,re,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/research/irok-na87/python-deps'))
from unicorn import Uc,UC_ARCH_RISCV,UC_MODE_RISCV32,UC_HOOK_CODE
from unicorn.riscv_const import *
base=ROOT/'docs/research/sayo-config-sources/app_O3C.decrypted.bin'
data=base.with_suffix('.bin').read_bytes()
from Crypto.Cipher import AES
assert AES.new(bytes.fromhex('C4053DDF225E89F74868C1E1F4C00D514F02A8A8692F997869ABEB155250150C'),AES.MODE_CBC,bytes(16)).decrypt((base.parent/'app_O3C.bin').read_bytes())==data
assert hashlib.sha256((base.parent/'app_O3C.bin').read_bytes()).hexdigest()=='d81a3e001a2b5f13fcaabfe6a8e357ecaedc14a36ae21cce4f9dc6aed863068f'
listing=subprocess.check_output(['llvm-objdump','-d','--adjust-vma=0x4000','--mattr=+c,+m,+xwchc','--start-address=0x508e','--stop-address=0x65c0',str(base.with_suffix('.elf'))]).decode()
custom={}
for line in listing.splitlines():
 m=re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]{4})\s+(lbu|lhu|sb|sh)\s+(\w+), (0x[0-9a-f]+)\((\w+)\)',line)
 if m:custom[int(m[1],16)]=(m[3],m[4],int(m[5],16),m[6],int(m[2],16))
assert custom
u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV32);u.mem_map(0,0x40000);u.mem_write(0x4000,data);u.mem_map(0x20000000,0x10000)
def hook(uc,pc,size,user):
 word=int.from_bytes(uc.mem_read(pc,2),'little')
 if pc in custom:
  op,reg,offset,base,expected=custom[pc];assert word==expected and size==2
  regid=globals()['UC_RISCV_REG_'+reg.upper()];baseid=globals()['UC_RISCV_REG_'+base.upper()]
  at=(uc.reg_read(baseid)+offset)&0xffffffff;length=1 if op in ('lbu','sb') else 2
  if op.startswith('l'):uc.reg_write(regid,int.from_bytes(uc.mem_read(at,length),'little'))
  else:uc.mem_write(at,(uc.reg_read(regid)&((1<<(length*8))-1)).to_bytes(length,'little'))
  uc.reg_write(UC_RISCV_REG_PC,pc+2)
 elif size==2 and word&3 in (0,2) and word>>13 in (1,3,5,7):raise AssertionError(('unknown vendor instruction',hex(pc)))
u.hook_add(UC_HOOK_CODE,hook)
gp=0x20001000;src=0x20008000;dst=0x20008400;stop=0x2000fff0
u.reg_write(UC_RISCV_REG_GP,gp)
def call(cmd,index):
 u.mem_write(src,struct.pack('<HBB',4,cmd,index));u.mem_write(dst,bytes(1024))
 for reg,v in [(UC_RISCV_REG_A0,src),(UC_RISCV_REG_A1,dst),(UC_RISCV_REG_A2,1020),(UC_RISCV_REG_SP,0x2000e000),(UC_RISCV_REG_RA,stop)]:u.reg_write(reg,v)
 u.emu_start(0x908e,stop,count=10000)
 assert u.reg_read(UC_RISCV_REG_PC)==stop
 n=int.from_bytes(u.mem_read(dst,2),'little');assert 4<=n<=1020
 return bytes(u.mem_read(dst,n))
ram=bytes(range(150));u.mem_write(0x20003d64,ram)
for index in range(3):
 reply=call(0x10,index);assert len(reply)==60 and reply[2:4]==bytes([0x10,index])
 p=reply[4:];assert p[0]==1
 assert struct.unpack_from('<4H',p,4)==(1000+2000*index,3000,1800,1800)
 for layer in range(5):
  at=16+8*layer
  assert p[at]==ram[index+30*layer]
  assert p[at+4:at+8]==bytes(ram[index+30*layer+6*k] for k in range(1,5))
print('O3C key serializer: three positions, five action layers, exact fields PASS')
u.mem_write(gp-0x7f2,b'\xff')
for values in [(0,1000,4000),(3500,0,2000),(0,0,0)]:
 for i,v in enumerate(values):u.mem_write(gp+0x1a0+i*0xc0+14,struct.pack('<H',v))
 reply=call(0x15,1);assert reply[:4]==bytes([10,0,0x15,1]) and struct.unpack('<3H',reply[4:])==values
print('O3C depth serializer: independent values and releases PASS')
