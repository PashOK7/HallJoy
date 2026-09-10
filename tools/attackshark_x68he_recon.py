"""Offline evidence checks, NOT a keyboard backend or physical-device test.

Requires acquired artifacts and local Unicorn under artifacts/pydeps.
Executes the actual X65HE reference image, never claims it is X68HE firmware.
"""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--artifacts', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1] / '.local/research/attackshark-x68he')
args = parser.parse_args()
root = args.artifacts.resolve()
sys.path.insert(0, str(root / 'pydeps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

image = (root / '2268_v309.bin').read_bytes()
digest = hashlib.sha256(image).hexdigest()
assert digest == '503940d85d865339bf6250a3c1ad464303a760f3b6da6a6f43dd02fab833a2fa'
assert image[0x5000:].startswith(b'AT32F405 8KMKB')
source = b''.join(struct.pack('<H', (i * 317 + 19) & 65535) for i in range(128))
for stream in (0, 1):
    for page in range(4):
        cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        cpu.mem_map(0x08000000, 0x30000)
        cpu.mem_write(0x08000000, image)
        cpu.mem_map(0x20000000, 0x10000)
        cpu.mem_write(0x2000222c, source)
        cpu.mem_write(0x20002a69, bytes([stream]))
        packet = bytearray(66)
        packet[2:6] = bytes([0xe5, 0xfe, 1, page])
        cpu.mem_write(0x2000725c, bytes(packet))
        before = bytes(cpu.mem_read(0x20000000, 0x10000))
        cpu.reg_write(UC_ARM_REG_SP, 0x2000f000)
        cpu.reg_write(UC_ARM_REG_LR, 0x08020001)
        cpu.emu_start(0x08006415, 0x08020000, count=10000)
        assert cpu.reg_read(UC_ARM_REG_PC) == 0x08020000
        assert bytes(cpu.mem_read(0x2000725e, 64)) == source[page*64:(page+1)*64]
        after = bytes(cpu.mem_read(0x20000000, 0x10000))
        changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
        assert all(0x725e <= i < 0x729e or 0xeff8 <= i < 0xf000 for i in changed)

jsroot = root / 'desktop/resources/app/dist/js'
driver = (jsroot / 'index.b078bf5f.js').read_text(encoding='utf8')
base = (jsroot / '5e635fe2.js').read_text(encoding='utf8')
assert 'FEA_CMD_GET_MULTI_MAGNETISM=229' in base
assert '_getMulitMagnetismCMD(254,4,1,1)' in base
assert 'FEA_CMD_SET_MAGNETISM_REPOR=27' in base
assert 'FEA_CMD_SET_MAGNETISM_CAL=28' in base
assert 'FEA_CMD_SET_MAGNETISM_MAXIMUM_CALIBRATION=30' in base
boards = []
for dev_id, chunk in ((2270, '51bbd794.js'), (2472, 'bd86972c.js'), (2902, '69a807f0.js')):
    entry = re.search(r'\{id:' + str(dev_id) + r',.{0,300}?displayName:"X68HE"', driver).group()
    assert 'vid:12625,pid:20525' in entry
    text = (jsroot / chunk).read_text(encoding='utf8')
    assert 'C as ' in text and './5e635fe2.js' in text
    match = re.search(r'defaultMatrix=\[([\d,]+)\]', text)
    if not match:
        match = re.search(r'const r=\[([\d,]+)\]', text)
    matrix = bytes(map(int, match.group(1).split(',')))
    assert len(matrix) == 512
    slots = {str(hid): [i//4 for i in range(0,512,4) if matrix[i:i+4] == bytes([0,0,hid,0])]
             for hid in (4, 7, 22, 26)}
    boards.append(dict(dev_id=dev_id, chunk=chunk, matrix_sha256=hashlib.sha256(matrix).hexdigest(), wasd_hid_to_slots=slots))
commands = []
for page in range(4):
    packet = bytearray(64)
    packet[:4] = bytes([0xe5, 0xfe, 1, page])
    packet[7] = 255 - (sum(packet[:7]) & 255)
    assert sum(packet[:8]) & 255 == 255
    commands.append(packet[:8].hex(' '))
print(json.dumps(dict(status='PASS', scope='X65HE reference firmware emulation + X68HE vendor client only',
    target_x68_firmware_obtained=False, reference_image_sha256=digest,
    emulated_cases=8, mutation_scope='reply buffer and stack only', boards=boards,
    request_headers_without_report_id=commands), indent=2))
