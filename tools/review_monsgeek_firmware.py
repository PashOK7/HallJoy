"""Exact downloaded RY5088 GET_MULTI_MAGNETISM fragment execution, no device I/O.

Tests MCU instructions with synthetic RAM. Does not model USB timing, ADC or
physical calibration. Discovery is deliberately restricted to reviewed images.
"""
import hashlib
import json
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import (UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
                              UC_ARM_REG_R5, UC_ARM_REG_R8, UC_ARM_REG_R9,
                              UC_ARM_REG_R10)

ROOT = Path(__file__).resolve().parents[1]
RESEARCH = ROOT / '.local/research/monsgeek-20260922'
CASES = [(2704, '2704_v509.bin', 0x0800634c),
         (2782, '2782_v502.bin', 0x08006310),
         (2949, '2949_v410.bin', 0x08006148),
         (3113, '3113_v510_oledv107_flashv102.bin', 0x08006318)]
HASHES = {
    2704: 'd0942ea4820b2900287338462e63cdc51f8a61f61a6484d803615c7179d7d1aa',
    2782: '2e188151b6025aac685855c6088959fa3d9c9058d9055bfb07a92c380b68f306',
    2949: 'e48e9c44bbe3f9cdca83ac7bacc67f9dea1c6c7c8f5ff718a90029c7df876762',
    3113: '97934a52a825cb97c033c0eb664f28ba7f79501274d86ce92c9c1068444c9dfa',
}

def review(board, filename, entry):
    image = (RESEARCH / filename).read_bytes()
    assert hashlib.sha256(image).hexdigest() == HASHES[board]
    assert image[:8] == bytes.fromhex('b04e0020c1020008')
    base = 0x08000000
    cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    ins = list(cs.disasm(image[entry-base:entry-base+1500], entry))
    assert ins[0].mnemonic == 'push' and ins[0].op_str == '{r4, r5}'
    def literal(i):
        assert i.mnemonic == 'ldr' and '[pc,' in i.op_str
        offset = int(i.op_str.split('#')[1].split(']')[0], 0)
        return struct.unpack_from('<I', image, ((i.address+4)&~3)+offset-base)[0]
    command = literal(ins[1])
    state = literal(ins[4])
    branch = next(ins[n+1] for n,i in enumerate(ins)
                  if i.mnemonic == 'cmp' and i.op_str == 'r1, #0xfe')
    target = int(branch.op_str.lstrip('#'), 0)
    tail = list(cs.disasm(image[target-base:target-base+20], target))
    assert tail[0].op_str == 'r0, r2, r0, lsl #6'
    assert tail[1].mnemonic == 'movw' and tail[1].op_str.startswith('r1, #')
    depth = state + int(tail[1].op_str.split('#')[1], 0)
    assert tail[3].op_str == 'r2, #0x40'
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    cpu.mem_map(base, 0x80000)
    cpu.mem_write(base, image)
    cpu.mem_map(0x20000000, 0x20000)
    stop = base+0x70000
    values = [(i*37)%801 for i in range(128)]
    values[14], values[15], values[21] = 720, 0, 361
    cpu.mem_write(depth, struct.pack('<128H', *values))
    if board == 2949:
        # Real v410 producer tail: stores current depth independently of the
        # monitor-enable flag, then optionally sends the single-key event.
        # Sensor conversion was completed before this fragment; supplied here.
        for index, raw in [(14, 720), (15, 361), (14, 0), (15, 0), (14, 9), (14, 10)]:
            cpu.reg_write(UC_ARM_REG_SP, 0x2001f000)
            cpu.mem_write(0x2001f010, struct.pack('<I', index//6))
            cpu.reg_write(UC_ARM_REG_R9, index%6)
            cpu.reg_write(UC_ARM_REG_R10, 0x20014000)
            cpu.mem_write(0x20014f44, struct.pack('<H', raw))
            cpu.reg_write(UC_ARM_REG_R5, state+0x7000+2*index)
            cpu.reg_write(UC_ARM_REG_R8, state+0x7000)
            cpu.emu_start(0x0800e2cf, 0x0800e32c, count=200)
            assert cpu.reg_read(UC_ARM_REG_PC) == 0x0800e32c
            values[index] = raw if raw >= 10 else 0
            assert struct.unpack('<H', cpu.mem_read(depth+2*index, 2))[0] == values[index]
    for page in range(4):
        cpu.mem_write(command, bytes(70))
        # GET_MULTI_MAGNETISM ignores the unused payload tail. Poisoning it
        # makes an unprocessed/partial shared-buffer reply distinguishable.
        cpu.mem_write(command+10, bytes([255])*56)
        cpu.mem_write(command+3, bytes([0xfe, 1, page]))
        cpu.reg_write(UC_ARM_REG_SP, 0x2001f000)
        cpu.reg_write(UC_ARM_REG_LR, stop|1)
        ram = bytes(cpu.mem_read(0x20000000, 0x18000))
        cpu.emu_start(entry|1, stop, count=5000)
        assert cpu.reg_read(UC_ARM_REG_PC) == stop
        expected = struct.pack('<32H', *values[page*32:(page+1)*32])
        assert bytes(cpu.mem_read(command+2, 64)) == expected
        after = bytes(cpu.mem_read(0x20000000, 0x18000))
        allowed = range(command+2-0x20000000, command+66-0x20000000)
        assert all(a==b or n in allowed for n,(a,b) in enumerate(zip(ram,after)))
    return dict(board=board, file=filename, sha256=hashlib.sha256(image).hexdigest(),
                entry=hex(entry), command=hex(command), state=hex(state),
                depth_table=hex(depth), pages=4, result='PASS',
                limits='Synthetic RAM; real getter instructions; no ADC/USB timing or device test')

if __name__ == '__main__':
    results=[review(*case) for case in CASES]
    print(json.dumps(results, indent=2))
