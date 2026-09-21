"""Pinned firmware component audit. No HID access or firmware/configuration writes.
Execute application scatter loading, switch selection, travel lookup and USB
read handlers separately. This is not full ADC/scan-loop/hardware emulation.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.local/research/attackshark-x68he/pydeps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import *

FLASH, RAM, SP, STOP = 0x08000000, 0x20000000, 0x2001e000, 0x08030000
PINS = {
    '2755_v504.bin': 'd9044605e2b5b9447a2b250a5a307b16ca81b6d901f309e4f945eea3f95f3c78',
    '2935_v503.bin': '0ce46d01a2e8d40b2728e7203ed7152cc64c577d309abd573ab93589d2990395',
}

def load(name):
    image = (ROOT / '.local/research/attackshark-pro' / name).read_bytes()
    assert hashlib.sha256(image).hexdigest() == PINS[name]
    c = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    c.mem_map(FLASH, 0x40000)
    c.mem_write(FLASH, image)
    c.mem_map(RAM, 0x20000)
    c.reg_write(UC_ARM_REG_SP, SP)
    return c, image

def run(c, start, end, count=10000):
    c.emu_start(start | 1, end, count=count)
    assert c.reg_read(UC_ARM_REG_PC) == end

def get(c, base, request, handler, command, page):
    packet = bytearray(66)
    packet[2:6] = bytes([0xe5, command, 1, page])
    packet[9] = (255 - sum(packet[2:9])) & 255
    c.mem_write(request, bytes(packet))
    before = bytes(c.mem_read(RAM, 0x20000))
    c.reg_write(UC_ARM_REG_SP, SP)
    c.reg_write(UC_ARM_REG_LR, STOP | 1)
    run(c, handler, STOP)
    after = bytes(c.mem_read(RAM, 0x20000))
    changed = [RAM+i for i, (a,b) in enumerate(zip(before,after)) if a != b]
    assert all(request+2 <= a < request+66 or SP-8 <= a < SP for a in changed)
    return bytes(c.mem_read(request+2, 64))

# X68 MAX: execute the actual initialization selector for all byte values and
# all 126 physical matrix positions, not an inferred one-to-one FC mapping.
c, image = load('2755_v504.bin')
base = 0x20001180
mapping = []
for value in range(256):
    c.mem_write(base+0x41df, bytes([value])*126)
    for reg, number in [(UC_ARM_REG_R5,0),(UC_ARM_REG_R6,1),(UC_ARM_REG_R7,base),
                        (UC_ARM_REG_R8,2),(UC_ARM_REG_R9,3),(UC_ARM_REG_R10,5),(UC_ARM_REG_R11,6)]:
        c.reg_write(reg, number)
    run(c, 0x0800f4e2, 0x0800f56c, 20000)
    banks = bytes(c.mem_read(base+0x425d,126))
    expected = 1 if value in (1,2,3,9) else 2 if value in (5,10) else 3 if value == 6 else 5 if value == 8 else 6 if value in (4,11) else 0
    assert banks == bytes([expected])*126, (value,banks)
    mapping.append(expected)
# FC reads the exact source bytes of that selector; test both 64-byte pages.
pattern = bytes(range(128))
c.mem_write(base+0x41df, pattern)
for page in range(2):
    assert get(c,base,0x20008a24,0x08006310,0xfc,page) == pattern[page*64:(page+1)*64]
max_banks=[]
for bank in range(7):
    table=struct.unpack_from('<2048H',image,0x17444+bank*4096)
    max_banks.append(dict(bank=bank,raw_ceiling=table[2046],index_limit=table[2047],
                          monotonic=all(a<=b for a,b in zip(table[:2046],table[1:2047])),
                          switch_types=[i for i in range(12) if mapping[i]==bank]))

# Both X68 publication branches preserve 350/700 unchanged through FE.
max_transitions = 0
for branch in (0,1):
    for slot in (14,32,64,96,125):
        for depth in (350,700,0):
            raw,flags=0x2001b000,0x2001c000
            c.mem_write(raw+0xfc0,struct.pack('<H',depth))
            c.mem_write(flags+0x975,b'\0')
            c.reg_write(UC_ARM_REG_SP,SP)
            if branch==0:
                c.mem_write(SP+8,struct.pack('<I',flags))
                c.reg_write(UC_ARM_REG_R11,raw)
                c.reg_write(UC_ARM_REG_R7,slot//6);c.reg_write(UC_ARM_REG_R5,slot%6)
                run(c,0x0800e462,0x0800e4d0)
            else:
                c.mem_write(SP,struct.pack('<I',flags))
                c.reg_write(UC_ARM_REG_R9,raw)
                c.reg_write(UC_ARM_REG_R10,slot//6);c.reg_write(UC_ARM_REG_R6,slot%6)
                run(c,0x0800e986,0x0800ea0a)
            page=get(c,base,0x20008a24,0x08006310,0xfe,slot//32)
            assert struct.unpack_from('<H',page,2*(slot%32))[0]==depth
            max_transitions+=1

# X82: the reset vector at offset zero starts the BOOTLOADER. The application
# vector is at 0x5200 and its own scatter loader materializes seven RAM tables.
c, image = load('2935_v503.bin')
assert struct.unpack_from('<I',image,0x1390)[0] == 0x08005200
assert struct.unpack_from('<I',image,0x5204)[0] == 0x08005501
assert struct.unpack_from('<I',image,0x552c)[0] == 0x080053e1
assert struct.unpack_from('<4I',image,0x1cedc) == (0x0801cefc,0x20000000,0x7458,0x0800541c)
run(c,0x080053e8,0x080054ac,2000000)
ram_digest=hashlib.sha256(bytes(c.mem_read(RAM,0x7458))).hexdigest()
x82_banks=[]
for bank in range(7):
    table=struct.unpack('<2048H',bytes(c.mem_read(RAM+12+bank*4096,4096)))
    assert table[2046:] == (730,2040)
    assert all(a<=b for a,b in zip(table[:2046],table[1:2047]))
    for index in range(2050):
        c.reg_write(UC_ARM_REG_R2,index)
        c.reg_write(UC_ARM_REG_R11,0x20017000)
        c.reg_write(UC_ARM_REG_R6,0x20016000)
        c.mem_write(0x2001625d,bytes([bank]))
        c.reg_write(UC_ARM_REG_LR,RAM+12)
        c.reg_write(UC_ARM_REG_R1,0x20018000)
        run(c,0x0800e2a0,0x0800e2da,100)
        actual=struct.unpack('<I',bytes(c.mem_read(0x20018004,4)))[0]
        assert actual == (730 if index>2040 else table[index])
    x82_banks.append(dict(bank=bank,raw_ceiling=730,index_limit=2040,lookup_cases=2050,
                         table_sha256=hashlib.sha256(struct.pack('<2048H',*table)).hexdigest()))
# Preserve 730 through both publication branches and the actual E5 FE getter.
# The intervening ADC/hysteresis/scan loop is intentionally not simulated.
transitions=0
base=0x20008594
for branch in (0,1):
    for slot in (0,14,31,32,63,64,95,96,125):
        for depth in (350,700,730,0):
            raw,flags=0x2001b000,0x2001c000
            c.mem_write(raw+0xfc0,struct.pack('<H',depth))
            c.mem_write(flags+0x975,b'\0')
            c.reg_write(UC_ARM_REG_SP,SP)
            if branch==0:
                c.mem_write(SP,struct.pack('<III',raw,0,flags))
                c.reg_write(UC_ARM_REG_R8,slot//6);c.reg_write(UC_ARM_REG_R7,slot%6)
                run(c,0x0800ea3e,0x0800eab0)
            else:
                c.mem_write(SP+4,struct.pack('<I',flags))
                c.reg_write(UC_ARM_REG_R9,raw)
                c.reg_write(UC_ARM_REG_R10,slot//6);c.reg_write(UC_ARM_REG_R8,slot%6)
                run(c,0x0800f29e,0x0800f30c)
            page=get(c,base,0x2000ff98,0x0800638c,0xfe,slot//32)
            assert struct.unpack_from('<H',page,2*(slot%32))[0]==depth
            assert bytes(c.mem_read(flags+0x975,1))==b'\0'
            transitions+=1

result=dict(scope=__doc__,firmware_sha256=PINS,
    x68max=dict(selector='0800f4e2..0800f56c',selector_cases=256*126,
                fc_getter_pages=2,switch_to_bank=mapping,banks=max_banks,
                publication_and_read_transitions=max_transitions),
    x82=dict(application_vector='08005200',scatter_entry='080053e8',
             initialized_ram_sha256=ram_digest,banks=x82_banks,
             publication_and_read_transitions=transitions),
    hardware_access=0)
print(json.dumps(result,indent=2))
