"""Hash-pinned X68 MAX v504 component checks; no HID or firmware writes."""
from pathlib import Path
import hashlib, struct, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/research/attackshark-x68he/pydeps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import *
image=(ROOT/'.local/research/attackshark-pro/2755_v504.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='d9044605e2b5b9447a2b250a5a307b16ca81b6d901f309e4f945eea3f95f3c78'
BASE=0x20001180; TABLE=BASE+0x10bc; REQUEST=0x20008a24; SP=0x2001e000

def cpu():
 c=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 c.mem_map(0x08000000,0x40000);c.mem_write(0x08000000,image)
 c.mem_map(0x20000000,0x20000);c.reg_write(UC_ARM_REG_SP,SP)
 return c

def read(c,page):
 packet=bytearray(66);packet[2:6]=bytes([0xe5,0xfe,1,page]);packet[9]=(255-sum(packet[2:9]))&255
 c.mem_write(REQUEST,bytes(packet));before=bytes(c.mem_read(0x20000000,0x20000))
 c.reg_write(UC_ARM_REG_SP,SP);c.reg_write(UC_ARM_REG_LR,0x08030001)
 c.emu_start(0x08006311,0x08030000,count=10000)
 assert c.reg_read(UC_ARM_REG_PC)==0x08030000
 after=bytes(c.mem_read(0x20000000,0x20000))
 changed=[0x20000000+i for i,(a,b) in enumerate(zip(before,after)) if a!=b]
 assert all(REQUEST+2<=a<REQUEST+66 or SP-8<=a<SP for a in changed)
 return struct.unpack('<32H',bytes(c.mem_read(REQUEST+2,64)))

for pattern in [list(range(128)),[(i*317+19)&65535 for i in range(128)],[0]*128]:
 c=cpu();c.mem_write(TABLE,struct.pack('<128H',*pattern))
 for page in range(4):assert read(c,page)==tuple(pattern[page*32:(page+1)*32])

# Inject already calculated travel at each actual scanner publication boundary.
# This exercises independent held/released slots, not ADC or USB scheduling.
for branch in [0,1]:
 c=cpu();expected=[0]*128;raw=0x2001b000;flags=0x2001c000
 for slot,depth in [(14,200),(9,400),(15,600),(21,700),(14,0),(9,19),(15,20),(21,0)]:
  c.mem_write(raw+0xfc0,struct.pack('<H',depth));c.mem_write(flags+0x975,b'\0')
  c.reg_write(UC_ARM_REG_SP,SP)
  if branch==0:
   c.mem_write(SP+8,struct.pack('<I',flags));c.reg_write(UC_ARM_REG_R11,raw)
   c.reg_write(UC_ARM_REG_R7,slot//6);c.reg_write(UC_ARM_REG_R5,slot%6)
   c.emu_start(0x0800e463,0x0800e4d0,count=1000)
  else:
   c.mem_write(SP,struct.pack('<I',flags));c.reg_write(UC_ARM_REG_R9,raw)
   c.reg_write(UC_ARM_REG_R10,slot//6);c.reg_write(UC_ARM_REG_R6,slot%6)
   c.emu_start(0x0800e987,0x0800ea0a,count=1000)
  expected[slot]=depth if depth>=20 else 0
  assert struct.unpack('<128H',bytes(c.mem_read(TABLE,256)))==tuple(expected)
  assert read(c,0)==tuple(expected[:32])
  assert bytes(c.mem_read(flags+0x975,1))==b'\0'
print('X68MAX_V504_COMPONENTS=PASS page_copies=12 scanner_transitions=16 stream_disabled=1 hardware_test=0')
