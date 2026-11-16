"""Offline F75 USB/control/producer audit and an IN-MEMORY firmware experiment.

Never opens a keyboard, flashes it, or emits a flashable patched firmware file.
The prototype is a firmware modification, NOT a stock HallJoy protocol solution.
Synthetic cache values test data flow; ADC, boot/USB hardware, integrity checks,
timing, interrupts, flash installation and physical typing remain unmodeled.
Vendor inputs and all generated analysis stay in a new private output directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                               UC_ARM_REG_R3, UC_ARM_REG_R4, UC_ARM_REG_SP,
                               UC_ARM_REG_LR, UC_ARM_REG_PC)
from firmware_replay import run_until
from review_aula_f75_firmware import PRIVATE, IMAGES, machine, RX, TX, CAL, SIM, ACK, READY

ROOT = Path(__file__).resolve().parents[1]
RAM, STACK, STOP = 0x20000000, 0x2001f000, 0x100
PATCH, CAVE = 0x12ff2, 0x20000
ASSEMBLY = r"""
.syntax unified
.cpu cortex-m4
.thumb
.section .text,"ax",%progbits
.global entry
.type entry,%function
.extern resume
.extern finish
entry:
    ldr r0, rx
    ldrb r0, [r0, #1]
    cmp r0, #0x68
    beq read68
    ldr r0, precommand_flag
    ldrb r0, [r0]
    b.w resume
read68:
    push {r0-r7}
    ldr r6, rx
    ldr r7, tx
    movs r2, #0
header:
    ldrb r0, [r6, r2]
    strb r0, [r7, r2]
    adds r2, #1
    cmp r2, #8
    blo header
    movs r0, #0x55
    strb r0, [r7]
    movs r0, #0
    movs r1, #8
clear:
    strb r0, [r7, r1]
    adds r1, #1
    cmp r1, #64
    blo clear
    ldrb r5, [r6, #2]
    cmp r5, #56
    bhi invalid
    tst r5, #7
    bne invalid
    movs r4, #0
record:
    cmp r4, r5
    bhs done
    add r0, r6, #8
    ldrb r3, [r0, r4]
    add r2, r7, #8
    add r2, r4
    movs r0, #255
    strb r0, [r2]
    ldr r1, channel_map
    movs r0, #0
lookup:
    ldrb r12, [r1, r0]
    cmp r12, r3
    beq found
    adds r0, #1
    cmp r0, #128
    blo lookup
    b next
found:
    cmp r3, #125
    beq next
    ldr r1, status
    ldrb r1, [r1, r0]
    strb r1, [r2]
    ldr r1, adc
    ldrh r1, [r1, r0, lsl #1]
    strb r1, [r2, #1]
    lsrs r1, #8
    strb r1, [r2, #2]
    ldr r1, depth
    ldrh r1, [r1, r0, lsl #1]
    strb r1, [r2, #3]
    lsrs r1, #8
    strb r1, [r2, #4]
next:
    adds r4, #8
    b record
invalid:
    movs r0, #0
    strb r0, [r7, #2]
done:
    pop {r0-r7}
    b.w finish
.balign 4
rx: .word 0x20015fb0
tx: .word 0x20015f70
channel_map: .word 0x1bb1c
status: .word 0x2000acde
adc: .word 0x20003eee
depth: .word 0x200032de
precommand_flag: .word 0x20000129
"""


def sha(data):
    return hashlib.sha256(data).hexdigest()


def invoke(u, entry, args=(), stops=(STOP,), budget=20000):
    for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args):
        u.reg_write(reg, value)
    u.reg_write(UC_ARM_REG_SP, STACK)
    u.reg_write(UC_ARM_REG_LR, STOP | 1)
    return run_until(u, entry, set(stops), budget=budget)


def initialized(image):
    u = machine(image)
    invoke(u, 0x6e4, stops=(0x1c8,), budget=200000)
    return u


def request(u, opcode, size=0, payload=b'', entry=0x12fd8):
    packet = bytearray(64)
    packet[:8] = bytes((0xaa, opcode, size, 0, 0, 0, 1, 0))
    packet[8:8 + len(payload)] = payload
    u.mem_write(RX, bytes(packet))
    u.mem_write(READY, b'\1')
    u.mem_write(ACK, b'\0')
    invoke(u, entry)
    return bytes(u.mem_read(TX, 64))


def control_review(image, shift=0):
    u = initialized(image)
    config = bytes(u.mem_read(0x200012a8, 0x8c))
    callbacks = struct.unpack_from('<4I', config, 0x7c)
    callback_shift = -0x1c if shift else 0
    assert callbacks == (0, 0, 0x1a751 + callback_shift, 0x1b6b9 + callback_shift)
    # Actual library config registration. Peripheral/controller startup is omitted.
    invoke(u, 0x16640 + shift, (0x200012a8,))
    obj, dev, setup = 0x20016364, 0x20016b68, 0x2001d000
    u.mem_write(0x200004dc, struct.pack('<I', obj))
    u.mem_write(0x200004ec, struct.pack('<I', dev))
    capacities = []
    for index in range(4):
        entry = obj + 0xc0 + index * 328
        source = config[4 + index * 24:4 + (index + 1) * 24]
        capacity = struct.unpack_from('<H', source, 8)[0]
        capacities.append(capacity)
        u.mem_write(entry, bytes((source[0], index)))
        u.mem_write(entry + 0xf2, struct.pack('<H', capacity))
    marker = bytes((i * 73 + 19) % 256 for i in range(64))
    u.mem_write(0x20015f2c, marker)
    u.mem_write(0x200032de, bytes((i * 29 + 7) % 256 for i in range(256)))
    captures, stalls, rows = [], [], []

    def usb_stub(m, pc, size, _):
        if pc == 0x156f4 + shift:
            ptr, count = m.reg_read(UC_ARM_REG_R1), m.reg_read(UC_ARM_REG_R2)
            captures.append(bytes(m.mem_read(ptr, count)))
        elif pc == 0x15690 + shift:
            stalls.append(True)
        else:
            return
        m.reg_write(UC_ARM_REG_R0, 0)
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    hook = u.hook_add(UC_HOOK_CODE, usb_stub)
    try:
        for interface in range(4):
            for kind in (1, 3):  # GET_REPORT Input and Feature; no SET_REPORT.
                for report_id in (0, 1, 2, 255):
                    captures.clear(); stalls.clear()
                    u.mem_write(setup, struct.pack('<BBHHH', 0xa1, 1, kind * 256 + report_id, interface, 64))
                    invoke(u, 0x16764 + shift, (0x200004dc, setup))
                    expected = kind == 3 and capacities[interface] != 0 and report_id in (0, 2)
                    assert bool(captures) == expected
                    if captures:
                        assert captures == [marker]
                    else:
                        assert stalls
                    rows.append(dict(interface=interface, kind=kind, report_id=report_id,
                                     result='update_buffer' if captures else 'stall'))
    finally:
        u.hook_del(hook)
    # Callback domain coverage, separate from modeled control transport.
    for report_id in range(256):
        u.mem_write(setup, bytes(80))
        invoke(u, callbacks[2] & ~1, (1, report_id, setup, setup + 76))
        success = u.reg_read(UC_ARM_REG_R0) == 0
        assert success == (report_id in (0, 2))
        if success:
            assert bytes(u.mem_read(setup, 64)) == marker
    return dict(callbacks=[hex(x) for x in callbacks], feature_capacities=capacities,
                control_cases=rows, callback_report_ids=256,
                feature_get_side_effect='OR bit2 into RAM byte20000442',
                limitation='Class objects use firmware config geometry; EP0 send/stall are explicit stubs, not a USB controller.')


def stock_review(image, shift=0):
    u = initialized(image)
    routes = []
    for opcode in (0x60, 0x68, 0x69, 0x6a):
        payload = bytes((1, 0, 0, 0, 0, 0, 0, 0)) * 3
        reply = request(u, opcode, 24, payload, 0x12fd8 + shift)
        assert reply[8:32] == payload and u.mem_read(CAL, 1) == b'\0'
        routes.append(dict(opcode=hex(opcode), request_echo=True))
    map_address = struct.unpack_from('<I', image, 0x77f8)[0]
    channel_map = image[map_address:map_address + 128]
    pairs = [(channel, key) for channel, key in enumerate(channel_map) if key != 125]
    expected_channels = 81 if shift else 80
    assert len(pairs) == expected_channels and len({key for channel, key in pairs}) == expected_channels
    counts = {'ordinary': 0, 'calibration': 0}
    for cal in (0, 1):
        for sim in (0, 1):
            for channel, key in pairs:
                for depth in (0, 1, 170, 340):
                    u.mem_write(CAL, bytes((cal, sim)))
                    u.mem_write(0x2000039b, b'\1')
                    u.mem_write(0x200000b5, bytes((channel,)))
                    u.mem_write(0x2000015e, b'\0')
                    u.mem_write(0x2000450e, bytes(128))
                    u.mem_write(0x2000390e, bytes(64))
                    u.mem_write(0x20002ede + channel * 2, struct.pack('<H', depth))
                    u.reg_write(UC_ARM_REG_R4, key)
                    stop = invoke(u, 0x7742, stops=(0x77de, 0x77e2), budget=2000)
                    assert stop == (0x77e2 if cal else 0x77de)
                    assert struct.unpack('<H', bytes(u.mem_read(0x200032de + channel * 2, 2)))[0] == depth
                    if cal:
                        frame = bytes(u.mem_read(0x2000390e, 14))
                        assert frame[:3] == bytes((0x55, 0xfb, key))
                        assert struct.unpack_from('<H', frame, 10)[0] == depth
                    else:
                        assert bytes(u.mem_read(0x2000390e, 14)) == bytes(14)
                    counts['calibration' if cal else 'ordinary'] += 1
    return dict(routes=routes, cases=counts, synthetic_depths=[0, 1, 170, 340],
                ordinary_branch_reached='0x77de before BE00; actual HID delivery not executed',
                calibration_branch='FB report generated; normal BE00 call skipped',
                channel_map_address=hex(map_address), channels=len(pairs),
                channel_map_sha256=sha(channel_map))


def prototype(image, payload):
    assert image[PATCH:PATCH + 4] == bytes.fromhex('f6480078')
    assert image[CAVE:CAVE + len(payload)] == bytes(len(payload))
    # Thumb B.W, verified by successful execution of the original dispatcher.
    offset = CAVE - (PATCH + 4)
    sign, i1, i2 = (offset >> 24) & 1, (offset >> 23) & 1, (offset >> 22) & 1
    j1, j2 = 1 ^ i1 ^ sign, 1 ^ i2 ^ sign
    branch = struct.pack('<HH', 0xf000 | sign << 10 | (offset >> 12) & 0x3ff,
                         0x9000 | j1 << 13 | j2 << 11 | (offset >> 1) & 0x7ff)
    patched = bytearray(image)
    patched[PATCH:PATCH + 4] = branch
    patched[CAVE:CAVE + len(payload)] = payload
    u = initialized(bytes(patched))
    pairs = [(i, k) for i, k in enumerate(image[0x1bb1c:0x1bb9c]) if k != 125]
    cases, write_addresses = [], set()

    def writes(m, access, addr, size, value, _):
        assert TX <= addr and addr + size <= TX + 64 or addr in (ACK, READY) or STACK - 0x1000 <= addr < STACK, hex(addr)
        write_addresses.add(addr)

    hook = u.hook_add(UC_HOOK_MEM_WRITE, writes)
    try:
        for phase in range(5):
            # Each channel has independent values, including a full zero-release phase.
            depths = [(ch * 17 + phase * 83) % 341 if phase != 4 else 0 for ch in range(128)]
            adc_values = [1200 + ch * 9 + phase for ch in range(128)]
            u.mem_write(0x200032de, struct.pack('<128H', *depths))
            u.mem_write(0x20003eee, struct.pack('<128H', *adc_values))
            u.mem_write(0x2000acde, bytes(ch % 3 for ch in range(128)))
            # An inherited command-dispatch cleanup must not release held keys.
            u.mem_write(0x20000129, bytes((phase % 2,)))
            held = bytes(i % 80 + 1 for i in range(128))
            u.mem_write(0x2000264e, held)
            for start in range(0, len(pairs), 7):
                group = pairs[start:start + 7]
                query = b''.join(bytes((key,)) + bytes(7) for ch, key in group)
                reply = request(u, 0x68, len(query), query)
                for index, (ch, key) in enumerate(group):
                    status, adc, depth = struct.unpack_from('<BHH', reply, 8 + index * 8)
                    assert (status, adc, depth) == (ch % 3, adc_values[ch], depths[ch])
                assert u.mem_read(CAL, 2) == b'\0\0'
                assert u.mem_read(0x20000129, 1) == bytes((phase % 2,))
                assert bytes(u.mem_read(0x2000264e, 128)) == held
                cases.append(dict(phase=phase, start=start, keys=len(group), reply_sha256=sha(reply)))
        for size in (0, 1, 7, 9, 55, 57, 64, 255):
            reply = request(u, 0x68, size)
            assert reply[2] == 0 and reply[8:] == bytes(56)
        reply = request(u, 0x68, 24, bytes((255,)) + bytes(7) + bytes((125,)) + bytes(7) + bytes((pairs[0][1],)) + bytes(7))
        assert reply[8] == 255 and reply[16] == 255
        # Duplicate physical keys are legitimate repeated reads.
        query = bytes((pairs[0][1],)) + bytes(7)
        reply = request(u, 0x68, 16, query * 2)
        assert reply[8:16] == reply[16:24]
    finally:
        u.hook_del(hook)
    # Join the actual ordinary producer tail, new command and existing USB sender.
    bridge, submitted = [], []
    busy = [False]

    def submit(m, pc, size, _):
        if pc != 0x166c0:
            return
        assert m.reg_read(UC_ARM_REG_R0) == 2
        assert m.reg_read(UC_ARM_REG_R2) == 64
        frame = bytes(m.mem_read(m.reg_read(UC_ARM_REG_R1), 64))
        submitted.append((not busy[0], frame))
        m.reg_write(UC_ARM_REG_R0, int(busy[0]))
        m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))

    hook = u.hook_add(UC_HOOK_CODE, submit)
    try:
        active = (pairs[0][0], pairs[17][0], pairs[-1][0])
        for stage in range(4):
            expected = {}
            for ch, key in pairs:
                value = (340, 170, 1)[active.index(ch)] if ch in active and stage in (1, 2) else 0
                if stage == 2 and ch == active[0]:
                    value = 0
                expected[ch] = value
                u.mem_write(0x2000039b, b'\1')
                u.mem_write(0x200000b5, bytes((ch,)))
                u.mem_write(0x20002ede + ch * 2, struct.pack('<H', value))
                u.reg_write(UC_ARM_REG_R4, key)
                assert invoke(u, 0x7742, stops=(0x77de, 0x77e2), budget=2000) == 0x77de
            for start in range(0, len(pairs), 7):
                group = pairs[start:start + 7]
                query = b''.join(bytes((key,)) + bytes(7) for ch, key in group)
                reply = request(u, 0x68, len(query), query)
                actual = [struct.unpack_from('<H', reply, 11 + i * 8)[0] for i in range(len(group))]
                assert actual == [expected[ch] for ch, key in group]
                busy[0] = start == 0
                invoke(u, 0x19f08)
                assert submitted[-1] == (not busy[0], reply)
                if busy[0]:
                    assert u.mem_read(ACK, 1) == b'\1'
                    busy[0] = False
                    invoke(u, 0x19f08)
                    assert submitted[-1] == (True, reply)
                assert u.mem_read(ACK, 1) == b'\0'
            bridge.append(dict(stage=stage, ordinary_channels=80,
                               positive_channels=sum(v > 0 for v in expected.values())))
    finally:
        u.hook_del(hook)
    passthrough = []
    for flag in (0, 1):
        for opcode in (0x10, 0x11, 0x12, 0x16, 0x60, 0x66, 0x67, 0x69, 0x6a):
            a, b = initialized(image), initialized(bytes(patched))
            a.mem_write(0x20000129, bytes((flag,)))
            b.mem_write(0x20000129, bytes((flag,)))
            ra, rb = request(a, opcode, 24), request(b, opcode, 24)
            assert ra == rb
            # Scratch stack/register state is not persistent firmware state.
            assert bytes(a.mem_read(RAM, 0x1d000)) == bytes(b.mem_read(RAM, 0x1d000))
            passthrough.append(dict(opcode=hex(opcode), precommand_flag=flag))
    return dict(in_memory_image_sha256=sha(patched), hook_address=hex(PATCH), cave=hex(CAVE),
                hook_hex=branch.hex(), own_payload_bytes=len(payload), batches=cases,
                malformed_sizes=8, unmapped_and_duplicate_keys=True,
                writes_only='TX64, ACK, READY and bounded dispatcher/prototype stack',
                producer_usb_bridge=bridge, usb_attempts=len(submitted),
                modeled_busy_retries=4,
                inherited_key_release_cleanup_bypassed_for_read68=True,
                passthrough=passthrough, flashable_file_written=False)


def main():
    if not __debug__:
        raise RuntimeError('Assertions required')
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--prototype-only', action='store_true',
                   help='Only the changed firmware-modification experiment; no repeated stock review')
    args = p.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    inputs = {Path(__file__): Path(__file__).read_bytes()}
    for name in ('firmware_replay.py', 'review_aula_f75_firmware.py'):
        path = ROOT / 'tools' / name
        inputs[path] = path.read_bytes()
    images = []
    for label, name, expected in IMAGES:
        path = PRIVATE / name
        image = path.read_bytes()
        assert sha(image) == expected
        inputs[path] = image
        images.append((label, image, 0))
    uk = ROOT / '.local/research/aula-f75-deep-20261116/f75-uk-v105.bin'
    image = uk.read_bytes()
    assert sha(image) == '74889f23b9171aae09a0b234eb5240e3ed3ff5d4557df4cfb2b0a5b88862f20f'
    inputs[uk] = image
    images.append(('UK-v105-separate-board', image, -0x28))
    args.output.mkdir(parents=True)
    assembly = args.output / 'read68-prototype.s'
    with assembly.open('x', encoding='ascii') as f:
        f.write(ASSEMBLY)
    clang, objcopy = shutil.which('clang'), shutil.which('llvm-objcopy')
    if not clang or not objcopy:
        raise RuntimeError('Local LLVM ARM assembler/objcopy required')
    elf, binary = args.output / 'own-payload.elf', args.output / 'own-payload.bin'
    subprocess.run([clang, '--target=arm-none-eabi', '-mcpu=cortex-m4', '-mthumb',
                    '-nostdlib', '-fuse-ld=lld', '-Wl,-Ttext=0x20000', '-Wl,--entry=entry',
                    '-Wl,--defsym=resume=0x12ff6', '-Wl,--defsym=finish=0x136d0',
                    str(assembly), '-o', str(elf)], check=True, capture_output=True)
    subprocess.run([objcopy, '-O', 'binary', '--only-section=.text', str(elf), str(binary)], check=True)
    payload = binary.read_bytes()
    results = []
    for label, image, shift in images:
        if args.prototype_only and shift:
            continue
        row = dict(image=label, sha256=sha(image))
        if not args.prototype_only:
            row.update(control=control_review(image, shift), stock=stock_review(image, shift))
        if shift == 0:
            row['firmware_modification_experiment'] = prototype(image, payload)
        results.append(row)
        print('completed ' + label, flush=True)
    assert all(path.read_bytes() == data for path, data in inputs.items()), 'Input changed during audit'
    receipt = dict(schema=1, results=results, own_payload_sha256=sha(payload),
                   inputs={str(path.relative_to(ROOT)): sha(data) for path, data in inputs.items()},
                   limitations=__doc__, prototype_requires_firmware_change=True,
                   prototype_only=args.prototype_only,
                   physical_test=False, device_io=False, support_promotion=False)
    with (args.output / 'raw.json').open('x', encoding='utf-8') as f:
        json.dump(receipt, f, indent=2)
        f.write('\n')
    if args.prototype_only:
        print('F75_FIRMWARE_PROTOTYPE=PASS images=2 batches=120 bridge_batches=96 physical_test=0')
    else:
        producer_cases = sum(sum(r['stock']['cases'].values()) for r in results)
        print(f'F75_DEEP_REPLAY=PASS images=3 control_cases=96 callback_ids=768 producer_cases={producer_cases} prototype_batches=120 physical_test=0')


if __name__ == '__main__':
    main()
