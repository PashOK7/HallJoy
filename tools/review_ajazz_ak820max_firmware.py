"""Offline SG8994HE firmware fragment test; no device access or flashing.

Tests accepted-change producer and the complete report scheduler with synthetic
RAM. Does not emulate the full matrix scan or prove real-device scheduling.
"""
import hashlib
import json
import struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R9, UC_ARM_REG_R11, UC_ARM_REG_R14, UC_ARM_REG_PC

root = Path(__file__).resolve().parents[1] / '.local/research/ajazz-ak820max'
image = (root / 'SG8994HE_V1_13_02_flash512k.bin').read_bytes()
assert hashlib.sha256(image).hexdigest() == 'fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680'
assert b'M484,01,KB,SG,SG8994HE,V1.13.02' in image
cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
cpu.mem_map(0, 0x100000)
cpu.mem_write(0, image)
cpu.mem_map(0x20000000, 0x20000)
base, flags, output = 0x20003100, 0x20000400, 0x200032e4
stop = 0xf0000
cpu.mem_write(base + 0x10f, bytes([0x3f] * 22))

def produce(row, column, depth):
    cpu.reg_write(UC_ARM_REG_SP, 0x2001f000)
    cpu.reg_write(UC_ARM_REG_R11, base + row * 22 + column)
    cpu.reg_write(UC_ARM_REG_R14, base)
    cpu.reg_write(UC_ARM_REG_R9, depth)
    cpu.reg_write(UC_ARM_REG_R2, column)
    cpu.reg_write(UC_ARM_REG_R3, row)
    cpu.emu_start(0x88d5, 0x8912, count=100)
    assert cpu.reg_read(UC_ARM_REG_PC) == 0x8912

def send():
    cpu.reg_write(UC_ARM_REG_SP, 0x2001f000)
    cpu.reg_write(UC_ARM_REG_LR, stop | 1)
    cpu.emu_start(0xa5df, stop, count=10000)
    assert cpu.reg_read(UC_ARM_REG_PC) == stop
    return bytes(cpu.mem_read(output, 64))

results = []
for first, second in [(17, 29), (0, 30)]:
    cpu.mem_write(0x200032b4 + 0xb8, bytes(4))
    produce(1, 2, first)
    produce(3, 4, second)
    assert cpu.mem_read(base + 0x125 + 24, 1) == bytes([first])
    assert cpu.mem_read(base + 0x125 + 70, 1) == bytes([second])
    assert struct.unpack('<I', cpu.mem_read(flags, 4))[0] & (1 << 22)
    report = send()
    assert report[:2] == bytes([1, 0x21])
    assert report[6:10] == bytes([1, 3, 4, second])
    assert not struct.unpack('<I', cpu.mem_read(flags, 4))[0] & (1 << 22)
    results.append(dict(first=first, second=second, report=report[:10].hex(' ')))
print(json.dumps(dict(status='PASS', scope=__doc__, cases=results), indent=2))
