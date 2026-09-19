"""Offline component checks for exact X82 Pro HE dev2935 v503. No HID access.
Scanner tests inject already calculated depth at the publication block boundary;
they do not emulate ADC, the full scan loop, USB scheduling or physical typing.
"""
from pathlib import Path
import hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'.local/research/attackshark-pro'
sys.path.insert(0,str(ROOT/'.local/research/attackshark-x68he/pydeps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import *
image=(ART/'2935_v503.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='0ce46d01a2e8d40b2728e7203ed7152cc64c577d309abd573ab93589d2990395'
BASE=0x20008594; TABLE=BASE+0x10bc; REQUEST=0x2000ff98; SP=0x2001e000
assert struct.unpack_from('<II',image,0x66f4)==(REQUEST,BASE)
assert struct.unpack_from('<I',image,0xeb5c)[0]==BASE
assert struct.unpack_from('<I',image,0xf3b0)[0]==BASE

def cpu():
 c=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
 c.mem_map(0x08000000,0x40000);c.mem_write(0x08000000,image)
 c.mem_map(0x20000000,0x20000);c.reg_write(UC_ARM_REG_SP,SP)
 return c

def read(c,page):
 packet=bytearray(66);packet[2:6]=bytes([0xe5,0xfe,1,page]);packet[9]=(255-sum(packet[2:9]))&255
 c.mem_write(REQUEST,bytes(packet));before=bytes(c.mem_read(0x20000000,0x20000))
 c.reg_write(UC_ARM_REG_SP,SP);c.reg_write(UC_ARM_REG_LR,0x08030001)
 c.emu_start(0x0800638d,0x08030000,count=10000)
 assert c.reg_read(UC_ARM_REG_PC)==0x08030000
 after=bytes(c.mem_read(0x20000000,0x20000))
 changed=[0x20000000+i for i,(a,b) in enumerate(zip(before,after)) if a!=b]
 assert all(REQUEST+2<=a<REQUEST+66 or SP-8<=a<SP for a in changed)
 return struct.unpack('<32H',bytes(c.mem_read(REQUEST+2,64)))

cases=[]
for pattern in [list(range(128)),[(i*317+19)&65535 for i in range(128)],[0]*128]:
 c=cpu();c.mem_write(TABLE,struct.pack('<128H',*pattern))
 for page in range(4):assert read(c,page)==tuple(pattern[page*32:(page+1)*32])
 cases.append('four-page-copy-and-RAM-side-effect-check')

# Two scanner branches publish the same table before consulting the stream flag.
for branch in [0,1]:
 c=cpu();expected=[0]*128;raw=0x2001b000;flags=0x2001c000
 for slot,depth in [(14,200),(9,400),(15,600),(21,700),(14,0),(9,19),(15,20),(21,0)]:
  c.mem_write(raw+0xfc0,struct.pack('<H',depth));c.mem_write(flags+0x975,b'\0')
  c.reg_write(UC_ARM_REG_SP,SP)
  if branch==0:
   c.mem_write(SP,struct.pack('<III',raw,0,flags));c.reg_write(UC_ARM_REG_R8,slot//6);c.reg_write(UC_ARM_REG_R7,slot%6)
   c.emu_start(0x0800ea3f,0x0800eab0,count=1000)
  else:
   c.mem_write(SP+4,struct.pack('<I',flags));c.reg_write(UC_ARM_REG_R9,raw);c.reg_write(UC_ARM_REG_R10,slot//6);c.reg_write(UC_ARM_REG_R8,slot%6)
   c.emu_start(0x0800f29f,0x0800f30c,count=1000)
  expected[slot]=depth if depth>=20 else 0
  assert struct.unpack('<128H',bytes(c.mem_read(TABLE,256)))==tuple(expected)
  assert read(c,0)==tuple(expected[:32])
  assert bytes(c.mem_read(flags+0x975,1))==b'\0'
  cases.append({'scanner_branch':branch,'slot':slot,'depth':depth,'published':expected[slot]})

js=ART/'desktop/resources/app/dist/js';registry=(js/'index.2e5bd916.js').read_text(encoding='utf-8')
base=(js/'f9b6af43.js').read_text(encoding='utf-8');assert '_getMulitMagnetismCMD(254,4,1,1)' in base
boards=[]
for bid,pid,name,chunk in [(2308,0x502f,'X65 Pro HE','6589a4f6.js'),(2938,0x5030,'X65 Pro HE','aaf1260b.js'),(2370,0x502f,'X68 Pro HE','c3329646.js'),(2901,0x502f,'X68 Pro HE','c99d34c9.js'),(2356,0x502f,'X82 Pro HE','359ae4d7.js'),(2935,0x5030,'X82 Pro HE','fb3f6dd5.js')]:
 assert f'id:{bid},vid:12625,pid:{pid},' in registry
 p=js/chunk;s=p.read_text(encoding='utf-8');assert 'from"./f9b6af43.js"' in s
 matrix=json.loads(re.search(r'defaultMatrix=(\[[0-9,]+\])',s).group(1));assert len(matrix)==512
 slots={key:[i//4 for i in range(0,512,4) if matrix[i:i+4]==[0,0,hid,0]] for key,hid in [('W',26),('A',4),('S',22),('D',7),('LWin',227),('LAlt',226)]}
 assert [slots[k] for k in ['W','A','S','D']]==[[14],[9],[15],[21]]
 boards.append(dict(dev_id=bid,pid=hex(pid),model=name,chunk=chunk,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),slots=slots,matrix=matrix))
result={'firmware_sha256':hashlib.sha256(image).hexdigest(),'handler':'0800638c','table':hex(TABLE),'cases':cases,'boards':boards,'scope':'component emulation, not hardware validation'}
p=ART/'review-20260919.json';data=(json.dumps(result,indent=2)+'\n').encode()
if p.exists():assert p.read_bytes()==data
else:
 with p.open('xb') as f:f.write(data)
print('ATTACKSHARK_PRO_REVIEW=PASS pages=12 scanner_transitions=16 exact_models=6 hardware_access=0')
