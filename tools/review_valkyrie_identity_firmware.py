"""Replay the pinned VK Mag75 Max packet writer; no USB/device access.
Requires Unicorn and the locally archived vendor firmware. This is component
validation, not a physical keyboard or full firmware emulation test.
"""
import hashlib
import json
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7
ROOT = Path(__file__).resolve().parents[1]
evidence = json.loads((ROOT / "docs/research/usb-identity-audit/approved-aliases.json").read_bytes())
a = next(a for a in evidence["aliases"] if a["board"] == 2398)
raw = (ROOT / a["firmware_local"]).read_bytes()
assert hashlib.sha256(raw).hexdigest() == a["firmware_sha256"]
for slot, depth in [(0, 0), (14, 1), (31, 350), (95, 700), (127, 1023)]:
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    uc.mem_map(0x08000000, 0x20000)
    uc.mem_write(0x08000000, raw)
    uc.mem_map(0x20000000, 0x20000)
    for reg, value in [(UC_ARM_REG_R7, 27), (UC_ARM_REG_R6, depth & 255),
                       (UC_ARM_REG_R5, depth >> 8), (UC_ARM_REG_R4, slot)]:
        uc.reg_write(reg, value)
    uc.emu_start(0x08011e55, 0x08011e78, count=100)
    assert bytes(uc.mem_read(0x2000da98, 5)) == bytes([5, 27, depth & 255, depth >> 8, slot])
    assert int.from_bytes(uc.mem_read(0x2000701c, 4), "little") & 0x10
print("VALKYRIE_FIRMWARE_PACKET_REPLAY=PASS cases=5 (synthetic RAM, no hardware)")
