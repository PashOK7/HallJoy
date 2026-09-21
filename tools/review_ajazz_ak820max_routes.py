"""SG8994HE offline command and matrix-loop checks with synthetic RAM/ADC.

No hardware access. Stops before post-scan processing; stubs only event wait.
Does not establish interrupt timing, physical calibration or RGB behavior.
"""
import runpy
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R9, UC_ARM_REG_PC, UC_ARM_REG_SP, UC_ARM_REG_LR

state = runpy.run_path(str(Path(__file__).with_name('review_ajazz_ak820max_firmware.py')))
cpu, image = state['cpu'], state['image']
base, output, stop = state['base'], state['output'], state['stop']
request = 0x20018000

def call(address, end=stop):
    cpu.reg_write(UC_ARM_REG_SP, 0x2001f000)
    cpu.reg_write(UC_ARM_REG_LR, stop | 1)
    cpu.emu_start(address | 1, end, count=300000)
    assert cpu.reg_read(UC_ARM_REG_PC) == end

def command(sub, payload=b''):
    cpu.mem_write(request, bytes(64))
    cpu.mem_write(request + 6, bytes([sub]) + payload)
    cpu.reg_write(UC_ARM_REG_R0, request)
    call(0x9fe4)

cpu.mem_write(0x20000000, bytes(0x20000))
cpu.mem_write(base + 0x125, bytes([17] * 132))
command(2, bytes([63] * 22))
assert bytes(cpu.mem_read(base + 0x10f, 22)) == bytes([63] * 22)
assert bytes(cpu.mem_read(base + 0x125, 132)) == bytes([17] * 132)
assert struct.unpack('<I', cpu.mem_read(state['flags'], 4))[0] == 1 << 21
state['send']()
assert cpu.mem_read(output + 6, 1) == b'\x00'
assert not struct.unpack('<I', cpu.mem_read(state['flags'], 4))[0]
command(3)
assert bytes(cpu.mem_read(base + 0x10f, 22)) == bytes(22)
assert bytes(cpu.mem_read(base + 0x125, 132)) == bytes([17] * 132)
print('PASS: subscribe/unsubscribe preserve cached depths; subscription schedules ACK only.')

config = struct.unpack_from('<I', image, 0xa30c)[0]
cpu.mem_write(config + (1 * 22 + 2) * 8, bytes([1, 2, 3, 4, 5, 6, 7, 8]))
answers = []
for depth in [0, 17, 40]:
    cpu.mem_write(base + 0x125 + 24, bytes([depth]))
    command(5, bytes([1, 2]))
    call(0xa25c)
    answers.append(bytes(cpu.mem_read(output + 7, 5)))
assert answers == [bytes([1, 3, 2, 7, 8])] * 3
print('PASS: addressed settings reply unchanged at live depths 0/17/40.')

cpu.mem_write(0x20000000, bytes(0x20000))
for slot, ptr in enumerate(struct.unpack_from('<132I', image, 0x1393c)):
    if ptr:
        cpu.mem_write(ptr + 0x11, bytes([(slot // 22) * 32 + slot % 22]))
positions = []
for col in range(16):
    for row in range(8):
        ptr = struct.unpack_from('<I', image, 0x1352c + row * 64 + col * 4)[0]
        if not ptr or image[0x1fe60 + col] & (1 << row):
            continue
        rc = cpu.mem_read(ptr + 0x11, 1)[0]
        logical_row, logical_col = rc >> 5, rc & 31
        assert logical_row < 6 and logical_col < 22
        positions.append((row, col, logical_row, logical_col))
        cpu.mem_write(0x20002070 + row * 32 + col * 2, struct.pack('<H', 400 * 8))
        cpu.mem_write(0x20002170 + row * 32 + col * 2, struct.pack('<H', 400))
        cpu.mem_write(0x20001d48 + (logical_row * 22 + logical_col) * 2, struct.pack('<H', 400))
        cpu.mem_write(config + (logical_row * 22 + logical_col) * 8, bytes([0, 255, 255, 0, 144, 1, 0, 0]))
cpu.mem_write(base + 0x10f, bytes([63] * 22))
accepted = []
def hook(uc, address, size, data):
    if address == 0x5c56:
        uc.reg_write(UC_ARM_REG_R0, 0)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))
    if address == 0x88d4:
        accepted.append((uc.reg_read(UC_ARM_REG_R3), uc.reg_read(UC_ARM_REG_R2), uc.reg_read(UC_ARM_REG_R9)))
cpu.hook_add(UC_HOOK_CODE, hook)
keys = positions[:4]
def phase(samples):
    for (r, c, lr, lc), sample in zip(keys, samples):
        cpu.mem_write(0x20002070 + r * 32 + c * 2, struct.pack('<H', sample * 8))
    batches = []
    for _ in range(8):
        mark = len(accepted)
        call(0x83b4, 0x8ad0)
        batches.append(accepted[mark:])
    print('Matrix batches:', batches)
    return batches
batches = phase([360, 350, 340, 330])
assert any(len({(r, c) for r, c, v in batch if v}) == 4 for batch in batches)
report = state['send']()
assert report[6:10] == bytes([1, *accepted[-1]])
print('PASS: four depths accepted in one matrix traversal, one pending event:', report[:10].hex(' '))
cpu.mem_write(0x200032b4 + 0xb8, bytes(4))
batches = phase([400, 320, 340, 330])
assert any(any(v == 0 for r, c, v in batch) and any(v > 0 for r, c, v in batch) for batch in batches)
report = state['send']()
assert report[6:10] == bytes([1, *accepted[-1]]) and report[9] > 0
assert cpu.mem_read(base + 0x125 + keys[0][2] * 22 + keys[0][3], 1) == b'\x00'
print('PASS: release overwritten before service:', report[:10].hex(' '))
mark = len(accepted)
assert all(not batch for batch in phase([400, 320, 340, 330]))
assert len(accepted) == mark
assert not struct.unpack('<I', cpu.mem_read(state['flags'], 4))[0] & (1 << 22)
command(3)
command(2, bytes([63] * 22))
assert all(not batch for batch in phase([400, 320, 340, 330]))
assert not struct.unpack('<I', cpu.mem_read(state['flags'], 4))[0] & (1 << 22)
print('PASS: unchanged samples and resubscription do not recover the dropped release.')
print(__doc__)
