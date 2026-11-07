"""Exact Gen3 branch replay. Synthetic ADC input; modeled USB RX/TX and GPIO row-select only."""
from pathlib import Path
import struct,hashlib,json,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from firmware_replay import thumb_machine,run_until
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
if len(sys.argv)!=3:raise SystemExit("Usage: verify_apex_tkl_gen3_firmware.py <exact-1.19.7.bin> <new-private-output-dir>")
FW=Path(sys.argv[1]);ROOT=Path(sys.argv[2]);assert not ROOT.exists();ROOT.mkdir(parents=True)
PRODUCER=Path(__file__).read_bytes();CORE=Path(__file__).with_name('firmware_replay.py').read_bytes()
b=FW.read_bytes();BASE=0x8009000
assert hashlib.sha256(b).hexdigest()=='e492fccadfdbc6bb6eb4872166a8a20c1abc68ca5cf8ff0235af0ff7f8b8018f'
CTX=0x801c4a8;STATE=0x200006f4;REQ=0x20009000;OUT=0x20005294;STOP=BASE
mapping={hid:index for hid,index in enumerate(b[0x801b2a8-BASE:0x801b2a8-BASE+256]) if index<70}
assert len(mapping)==68 and len(set(mapping.values()))==68

def machine():
 u=thumb_machine(b,BASE,ram_size=0x10000)
 run_until(u,0x8012744,{0x801276a})
 assert bytes(u.mem_read(0x20000000,0x12a8))==b[0x801ea40-BASE:0x801ea40-BASE+0x12a8]
 return u

def call(u,entry,args=()):
 for i,x in enumerate(args):u.reg_write([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3][i],x)
 u.reg_write(UC_ARM_REG_SP,0x2000a000);u.reg_write(UC_ARM_REG_LR,STOP|1)
 run_until(u,entry,{STOP})
 return u.reg_read(UC_ARM_REG_R0)

def dispatch(u,req):
 assert len(req)<=64
 u.mem_write(REQ,req+bytes(64-len(req)));u.mem_write(0x20005290,struct.pack('<I',64));u.mem_write(0x2000528f,b'\0')
 before=bytes(u.mem_read(0x20000000,0x10000));writes=[];packets=[]
 def code(uc,a,n,d):
  if a==0x800e2e0: # modeled vendor RX-buffer getter, exact request supplied by host
   uc.reg_write(UC_ARM_REG_R0,REQ);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
  elif a==0x800edca: # modeled USB submission, capture actual firmware buffer
   assert uc.reg_read(UC_ARM_REG_R1)==0x82 and uc.reg_read(UC_ARM_REG_R3)==64
   packets.append(bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R2),64)))
   uc.reg_write(UC_ARM_REG_R0,0);uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
 def watch(uc,access,a,n,v,d):
  assert OUT<=a and a+n<=OUT+64 or 0x20009800<=a and a+n<=0x2000a000 or a==0x20005290,hex(a)
  writes.append((a,n))
 hs=[u.hook_add(UC_HOOK_CODE,code),u.hook_add(UC_HOOK_MEM_WRITE,watch)]
 try:call(u,0x80117f4)
 finally:
  for h in hs:u.hook_del(h)
 assert len(packets)==1
 after=bytes(u.mem_read(0x20000000,0x10000))
 assert before[:0x528f]==after[:0x528f] and before[0x52d4:0x9800]==after[0x52d4:0x9800]
 return packets[0]

u=machine();results=[]
# Firmware scan-DMA completion callback consumes five blocks of fourteen halfwords.
# GPIO row-select 0801337C is isolated, not executed or claimed as hardware emulation.
def gpio(uc,a,n,d):
 if a==0x801337c:uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
h=u.hook_add(UC_HOOK_CODE,gpio)
try:
 for phase in range(8):
  level=[500,550,1800,3200,1800,550,500,500][phase]
  vals=[level+i for i in range(70)]
  for block in range(5):
   data=struct.pack('<14H',*vals[block*14:block*14+14]);u.mem_write(REQ,data)
   assert call(u,0x8014b2c,(CTX,block,REQ,28))==block+1
  assert struct.unpack('<70H',u.mem_read(0x20006710,140))==tuple(vals)
  call(u,0x8014b00,(CTX,))
  assert [struct.unpack('<H',u.mem_read(STATE+i*24+2,2))[0] for i in range(70)]==vals
  # Distinct synthetic learned endpoints. They remain untouched by read commands.
  for i in range(70):u.mem_write(STATE+i*24+4,struct.pack('<HH',3200+i,500+i))
  for start in range(0,68,20):
   keys=[next(h for h,index in mapping.items() if index==i) for i in range(68)][start:start+20];req=bytes([0xf3,0x50,len(keys)])+b''.join(bytes([k,0,0]) for k in keys)
   p=dispatch(u,req);assert p[:4]==bytes([0xf3,0,0x50,len(keys)])
   assert p[4:4+3*len(keys)]==b''.join(struct.pack('<BH',k,vals[mapping[k]]) for k in keys)
   assert not any(p[4+3*len(keys):]);results.append({'phase':phase,'kind':'depth','keys':len(keys),'request':req.hex(),'reply':p.hex()})
  for start in range(0,68,11):
   keys=[next(h for h,index in mapping.items() if index==i) for i in range(68)][start:start+11];req=bytes([0xf3,0x51,0,len(keys)])+b''.join(bytes([k,0,0,0,0]) for k in keys)
   p=dispatch(u,req);assert p[:5]==bytes([0xf3,0,0x51,0,len(keys)])
   assert p[5:5+5*len(keys)]==b''.join(struct.pack('<BHH',k,3200+mapping[k],500+mapping[k]) for k in keys)
   assert not any(p[5+5*len(keys):]);results.append({'phase':phase,'kind':'ranges','keys':len(keys),'request':req.hex(),'reply':p.hex()})
finally:u.hook_del(h)
# Explicitly modeled read-only factory flash outside this application image.
# Values are test inputs, not claims about a particular retail unit.
from unicorn import UC_PROT_READ
u.mem_map(0x8008000,0x1000,UC_PROT_READ)
for layout,region in [(0,1),(1,3),(2,13)]:
 u.mem_write(0x8008818,struct.pack('<II',layout,region))
 for req,expected in [(bytes([0xf2]),bytes([0xf2,0,layout])),(bytes([0xf5]),bytes([0xf5,0,region]))]:
  p=dispatch(u,req);assert p==expected+bytes(64-len(expected));results.append({'kind':'factory_metadata_model','request':req.hex(),'reply':p.hex()})
p=dispatch(u,bytes([0x90,0]));assert p.startswith(b'\x90\x001.19.7\x00');results.append({'kind':'version','request':'9000','reply':p.hex()})
for sub,count in [(0x50,21),(0x51,12)]:
 req=bytes([0xf3,sub,0,count]) if sub==0x51 else bytes([0xf3,sub,count])
 p=dispatch(u,req);assert p[:3]==bytes([0xf3,2,sub]);results.append({'kind':'oversize_refused','request':req.hex(),'reply':p.hex()})
for hid,index in mapping.items():u.mem_write(0x20001da8+0x50+index*5,bytes([0x51,(hid+1)%255,0,0,0]))
for start in range(0,68,10):
 keys=[next(h for h,index in mapping.items() if index==i) for i in range(68)][start:start+10];req=bytes([0xb6,0,len(keys)])+b''.join(bytes([k,0,0,0,0,0]) for k in keys)
 p=dispatch(u,req);assert p[:4]==bytes([0xb6,0,0,len(keys)])
 assert p[4:4+6*len(keys)]==b''.join(bytes([k,0x51,(k+1)%255,0,0,0]) for k in keys)
 assert not any(p[4+6*len(keys):]);results.append({'kind':'live_mapping_model','request':req.hex(),'reply':p.hex()})
report={'firmware_sha256':hashlib.sha256(b).hexdigest(),'boot':'real data copy; stops before init','scope':'modeled read-only factory flash at08008818 and synthetic active mappings; real DMA buffer assembly, current-sensor copy, full top+nested read dispatcher; synthetic ADC/endpoints; USB RX/TX and GPIO row selection modeled; no actual USB/ADC/timing','mapping':mapping,'results':results,'outcome':'PASS'}
p=ROOT/'raw.json';assert not p.exists()
with p.open('x',encoding='utf8') as f:json.dump(report,f,indent=2)
print('PASS',len(results),'cases; real current-sensor path and full dispatcher, isolated host reads, no sensor/config writes')
for name,data in [('firmware.bin',b),('producer.py',PRODUCER),('firmware_replay.py',CORE)]:
 with (ROOT/name).open('xb') as f:f.write(data)
assert Path(__file__).read_bytes()==PRODUCER and Path(__file__).with_name('firmware_replay.py').read_bytes()==CORE
assets={}
for name,role in [('firmware.bin','firmware-input'),('producer.py','producer-code'),('firmware_replay.py','producer-code'),('raw.json','raw-result')]:
 assets[name]={'path':name,'role':role,'sha256':hashlib.sha256((ROOT/name).read_bytes()).hexdigest()}
def finding(state,detail,locator,level='branch_replay'):
 return {'state':state,'detail':detail,'evidence':[{'asset':'raw.json','level':level,'locator':locator}]}
findings={
 'normal_typing':finding('established','Scoped read-handler property: no mode switch, sensor/config write or peripheral write. Ordinary keyboard scanning and physical USB typing are not modeled.','dispatch write guard + sensor copy;080117F4/08014F34/080106E0'),
 'simultaneous_keys':finding('established','Every mapped key is read independently; 70-channel synthetic multi-key waveform preserves distinct channels. Physical simultaneous USB behavior remains untested.','results/depth: 8 phases x4 chunks'),
 'depth_scale':finding('established','Raw current sensor halfwords and separate high/low extrema; host normalizes sensor range. Does not establish linear millimeters or measured sensor resolution.','results/depth and ranges;08014B00/08014ED0/08014E44'),
 'shallow_input':finding('unknown','No cutoff in the chosen read handlers; physical ADC/front-end low-travel response is not modeled.','08014ED0/08014B00'),
 'temporal_filtering':finding('unknown','The replay supplies DMA completion data and bypasses row-selection GPIO. ADC timing, interrupts and upstream filtering are not emulated.','scope + waveform inputs'),
 'release':finding('established','Synthetic press/hold/release and repeated released state reach the reply unchanged, independent of digital keyboard events. This is not physical release testing.','results/depth phases0..7'),
 'physical_mapping':finding('established','ROM HID-to-channel mapping and F2 factory-layout reply execute. Factory flash values are explicit modeled inputs, not a captured retail unit. SVG geometry and regional channel filtering are checked separately by host tests.','mapping + results/factory_metadata_model;0800EF70/080120B8'),
 'packet_loss':finding('unknown','Actual USB loss/backpressure and Windows transport timing are unmodeled; malformed host replies are separately rejected by atomic parsers.','scope'),
 'persistent_settings':finding('established','Selected commands90/F2/F5/F3-50/F3-51/B6 execute with protected flash and a strict RAM-write allowlist limited to response, stack and RX length.','dispatch write guard, all104 cases'),
 'normal_exit':finding('established','No device mode is enabled by this path, so there is no mode-disable command to omit. Actual OS handle lifetime is a host responsibility outside this replay.','dispatch write guard'),
 'crash_recovery':finding('unknown','No persistent mode is changed; real process death, stale USB queues, power transitions and reconnection were not executed.','scope'),
 'configurator_conflict':finding('unknown','Real GG/Prism concurrency and exclusive-handle contention are not modeled. Host uses an exclusive vendor collection and reports open errors.','scope')}
candidate=lambda id,disposition,reason,locator:{'id':id,'disposition':disposition,'reason':reason,'evidence':[{'asset':'firmware.bin','locator':locator}]}
assessment={'schema':1,'model':'SteelSeries Apex Pro TKL Gen 3 wired 1038:1642','scope':'Exact1.19.7 image; chosen read-only command path, not a universal firmware or physical USB proof.','assets':assets,
 'execution':{'outcome':'pass','entry_boundary':'Reset08012744, DMA assembly08014B2C, sensor copy08014B00, full dispatcher080117F4','stop_boundary':'Reset0801276A before HW init; other functions return to explicit LR sentinel08009000','mocked':'Synthetic ADC, synthetic active-profile mappings, read-only factory flash at08008818; USB RX getter/TX-success capture and GPIO row selection','not_modeled':'USB controller/Windows/GG, ADC hardware, timing, interrupts, flash persistence, full boot'},
 'findings':findings,
 'candidates':[candidate('f3-current-sensors','suitable','Read-only current-sensor snapshot with independent extrema; exact full dispatch and producer replay. Physical behavior unknowns remain visible.','0801B248:50/51;08014F34/08014F6C;08014B00'),
 candidate('legacy-d7-da','rejected','Old command family is absent from the exact modern table; no neighbor aliases are sent.','0801C07C,36 records'),
 candidate('73-11-spi-registers','rejected','Two initialized SPI objects and RGB-sized channel operations; not selected as a complete live-depth path.','0801C4EC/0801C54C..0801C588;080129AA/080129E4'),
 candidate('38-actuation-settings','rejected','Configuration thresholds and rapid-trigger settings, not current sensor readings.','0801B56C'),
 candidate('73-80-calibration-control','rejected','State/configuration operations are unnecessary for the read-only candidate and are withheld.','08018208'),
 candidate('ffc1-notifications','unresolved','Notification producer states were not exhaustively executed; not an absence proof or selected host path.','FFC1 report descriptor')],
 'selection':{'candidate':'f3-current-sensors','rationale':'The intended read namespace exposes actual current scanner values without a configuration write or enabling a diagnostic mode.','coverage_limits':'104 selected replay scenarios, not every payload/state/peripheral interaction. No keyboard or firmware flashing performed.'}}
p=ROOT/'assessment.json'
with p.open('x',encoding='utf8') as f:json.dump(assessment,f,indent=2)
from firmware_behavior import validate,render
verdict=validate(assessment,ROOT)
with (ROOT/'REPORT.md').open('x',encoding='utf8') as f:f.write(render(assessment,verdict))
print(json.dumps(verdict))

