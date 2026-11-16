"""Exact3113 stock producer-to-E5/FE cache review; synthetic converted sensors.

Stops before later ordinary-key processing. This proves the scoped cache/monitor
relationship and observed cutoff, not physical typing, ADC, USB latency or scale.
No device I/O, firmware patch or flash operation.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R9,
    UC_ARM_REG_R11, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)
from firmware_replay import thumb_machine, run_until
from generate_attackshark_family import decode

ROOT = Path(__file__).resolve().parents[1]


def main():
    assert __debug__
    output = Path(sys.argv[1])
    if output.exists():
        raise FileExistsError(output)
    source = Path(__file__).read_bytes()
    image_path = ROOT / '.local/research/monsgeek-20260922/3113_v510_oledv107_flashv102.bin'
    image = image_path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '97934a52a825cb97c033c0eb664f28ba7f79501274d86ce92c9c1068444c9dfa'
    maps = (ROOT / '.local/research/monsgeek-20260922/manufacturer-model-maps.json').read_bytes()
    matrix = next(r['matrix'] for r in json.loads(maps) if r['board'] == 3113)
    slots = [slot for slot, hid in enumerate(decode(matrix)) if hid]
    assert len(slots) == 80
    cache, command, state = 0x20002490, 0x20008c94, 0x200013d4
    stack, scratch, flag_base = 0x2001f000, 0x20016000, 0x20014000
    scenarios = []
    for branch, entry, stop in [('A', 0x0800eb66, 0x0800ebd4), ('B', 0x0800f0ac, 0x0800f10e)]:
        for monitor in (0, 1):
            u = thumb_machine(image, 0x08000000)
            event_calls = []

            def event(m, pc, size, _):
                if pc == 0x08013620:
                    event_calls.append([m.reg_read(UC_ARM_REG_R1), m.reg_read(UC_ARM_REG_R2), m.reg_read(UC_ARM_REG_R3)])
                    m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

            hook = u.hook_add(UC_HOOK_CODE, event)
            expected = [0] * 128
            summaries = []
            try:
                for phase in range(9):
                    for slot in slots:
                        raw = [0, 1, 19, 20, 200 + slot, 800, 200 + slot, 19, 0][phase]
                        if phase == 6 and slot == slots[0]:
                            raw = 0
                        u.mem_write(scratch + 0xfc0, struct.pack('<H', raw))
                        u.mem_write(flag_base + 0x975, bytes((monitor,)))
                        u.reg_write(UC_ARM_REG_SP, stack)
                        if branch == 'A':
                            u.mem_write(stack + 8, struct.pack('<I', flag_base))
                            u.reg_write(UC_ARM_REG_R11, scratch)
                            u.reg_write(UC_ARM_REG_R7, slot // 6)
                            u.reg_write(UC_ARM_REG_R5, slot % 6)
                        else:
                            u.mem_write(stack, struct.pack('<I', flag_base))
                            u.reg_write(UC_ARM_REG_R9, scratch)
                            u.reg_write(UC_ARM_REG_R0, (slot // 6) * 3)
                            u.reg_write(UC_ARM_REG_R6, slot % 6)
                            u.reg_write(UC_ARM_REG_R1, raw)
                            u.reg_write(UC_ARM_REG_R3, slot)
                        run_until(u, entry, {stop}, budget=300)
                        expected[slot] = raw if raw >= 20 else 0
                        actual = struct.unpack('<H', bytes(u.mem_read(cache + slot * 2, 2)))[0]
                        assert actual == expected[slot], (branch, monitor, phase, slot, actual)
                    # Same RAM instance: execute the actual stock read-only getter.
                    for page in range(4):
                        packet = bytearray(70)
                        packet[3:6] = bytes((0xfe, 1, page))
                        packet[10:66] = bytes((255,)) * 56
                        u.mem_write(command, bytes(packet))
                        u.reg_write(UC_ARM_REG_SP, stack)
                        u.reg_write(UC_ARM_REG_LR, 0x08000101)
                        run_until(u, 0x08006318, {0x08000100}, budget=5000)
                        assert bytes(u.mem_read(command + 2, 64)) == struct.pack('<32H', *expected[page*32:(page+1)*32])
                    summaries.append(dict(phase=phase, positive=sum(x > 0 for x in expected), first=expected[slots[0]], last=expected[slots[-1]]))
            finally:
                u.hook_del(hook)
            assert bool(event_calls) == bool(monitor)
            scenarios.append(dict(branch=branch, entry=hex(entry), stop=hex(stop), monitor=monitor,
                                  producer_passes=720, getter_pages=36, event_calls=len(event_calls), phases=summaries))
    assert image_path.read_bytes() == image and Path(__file__).read_bytes() == source
    result = dict(schema=1, board=3113, firmware_sha256=hashlib.sha256(image).hexdigest(),
                  source_sha256=hashlib.sha256(source).hexdigest(), cache=hex(cache),
                  scenarios=scenarios, observed_cutoff_raw=20, monitor_not_required_for_cache=True,
                  ordinary_typing_hid_executed=False, physical_test=False, limitations=__doc__)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print('AKKO3113_PRODUCER=PASS branches=2 monitor_states=2 physical=80 producer_passes=2880 getter_pages=144 cutoff_raw=20')


if __name__ == '__main__':
    main()
