"""Offline travel lookup checks in pinned firmware; no HID or calibration writes."""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'.local/research/attackshark-pro'
sys.path.insert(0,str(ROOT/'.local/research/attackshark-x68he/pydeps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R6,UC_ARM_REG_R9,UC_ARM_REG_PC

CONFIGS=[
    ('2268_v309.bin','503940d85d865339bf6250a3c1ad464303a760f3b6da6a6f43dd02fab833a2fa',0x147a8,6,0xcdec,0xce30),
    ('2755_v504.bin','d9044605e2b5b9447a2b250a5a307b16ca81b6d901f309e4f945eea3f95f3c78',0x17444,5,0xdf98,0xdfd2),
]
results=[]
for file,digest,offset,banks,start,end in CONFIGS:
    image=(ART/file).read_bytes()
    assert hashlib.sha256(image).hexdigest()==digest
    cpu=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
    cpu.mem_map(0x08000000,0x40000);cpu.mem_write(0x08000000,image)
    cpu.mem_map(0x20000000,0x20000)
    entries=[]
    for bank in range(banks):
        table=struct.unpack_from('<2048H',image,offset+bank*4096)
        limit,ceiling=table[2047],table[2046]
        assert 0<limit<2046 and 0<ceiling<=1000
        assert all(a<=b for a,b in zip(table[:-3],table[1:-2]))
        observed=[]
        for index in range(0,2050):
            cpu.reg_write(UC_ARM_REG_R2,index)
            cpu.reg_write(UC_ARM_REG_R9,0x20010000)
            if file.startswith('2268'):
                cpu.reg_write(UC_ARM_REG_R1,0x20004000)
                cpu.mem_write(0x20006cb5,bytes([bank]))
                output=struct.unpack_from('<I',image,0xd0cc)[0]+4
            else:
                cpu.reg_write(UC_ARM_REG_R6,0x20004000)
                cpu.mem_write(0x2000425d,bytes([bank]))
                cpu.reg_write(UC_ARM_REG_R1,0x20018000)
                output=0x20018004
            cpu.emu_start(0x08000000+start+1,0x08000000+end,count=100)
            assert cpu.reg_read(UC_ARM_REG_PC)==0x08000000+end
            value=struct.unpack('<I',bytes(cpu.mem_read(output,4)))[0]
            expected=ceiling if index>limit else table[index]
            assert value==expected,(file,bank,index,value,expected)
            observed.append(value)
        entries.append(dict(bank=bank,table=hex(0x08000000+offset+bank*4096),
                            index_limit=limit,raw_ceiling=ceiling,
                            sampled_max=max(observed),lookup_cases=len(observed)))
    results.append(dict(file=file,sha256=digest,banks=entries))

# Preserve explicit limits: X82 v503 uses a RAM-backed lookup, so its raw
# endpoint is not established by the previous injected-depth publication test.
image=(ART/'2935_v503.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='0ce46d01a2e8d40b2728e7203ed7152cc64c577d309abd573ab93589d2990395'
assert struct.unpack_from('<I',image,0xe468)[0]==0x2000000c
print(json.dumps(dict(scope='lookup blocks with injected normalized sensor index, not full ADC/USB emulation',
                      results=results,x82_lookup_base='0x2000000c',x82_endpoint_verified=False),indent=2))
print('DEPTH_RANGE_COMPONENTS=PASS lookup_cases=22550 hardware_access=0')
