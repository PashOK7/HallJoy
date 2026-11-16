"""Bounded GT75 V1.08 RISC-V branch replay with explicit synthetic flash/RAM.

File-offset PCs normalize the linked code base. This is not boot, ADC, radio,
USB delivery or a physical typing test. No vendor bytes enter public outputs.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from unicorn import (Uc, UC_ARCH_RISCV, UC_MODE_RISCV32, UC_HOOK_CODE,
                     UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC)
from unicorn import riscv_const as regs

SHA = '42bc64ccd6ea2411cd405558429495ba8149fe2dc0a3aeed958d145594e2fbf6'
FLASH, ALIAS, SECTOR = 0x65000, 0x08065000, 4096
REQUEST, ACTIVE, PROFILE = 0x20178308, 0x2017a7e0, 0x20178512
FACTORY, FACTORY_FILE, BUFFER = 0x20119600, 0x136f4, 0x20178290
GP = 0x20188000  # Explicit synthetic gp; only non-input bookkeeping uses it here.


def reg(name):
    return getattr(regs, 'UC_RISCV_REG_' + name.strip().upper())


class Replay:
    def __init__(self, image, listing):
        self.image = image
        self.instructions = {}
        for line in listing.splitlines():
            match = re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)\s*(.*)', line)
            if not match:
                continue
            pc, encoded, op, args = match.groups()
            pc = int(pc, 16)
            raw = bytes.fromhex(encoded)[::-1]
            if image[pc:pc + len(raw)] == raw:
                self.instructions[pc] = (len(raw), op, [x.strip() for x in args.split(',')])

    def machine(self):
        u = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
        u.mem_map(0, 0x70000, UC_PROT_READ | UC_PROT_EXEC)
        u.mem_write(0, self.image)
        u.mem_map(0x20110000, 0x80000, UC_PROT_READ | UC_PROT_WRITE)
        u.mem_map(ALIAS, SECTOR, UC_PROT_READ)
        u.reg_write(reg('sp'), 0x2018f000)
        u.reg_write(reg('gp'), GP)
        u.reg_write(reg('ra'), 0x1000)
        return u

    def run(self, u, entry, stops, effects, extra=None):
        pcs = []
        reached = []

        def step(m, pc, size, _):
            pcs.append(pc)
            if pc in stops:
                reached.append(pc)
                m.emu_stop()
                return
            if not 0 <= pc < len(self.image):
                raise RuntimeError('Execution outside pinned image: ' + hex(pc))
            if extra and extra(m, pc):
                return
            length, op, args = self.instructions.get(pc, (size, '', []))
            # Exact decoded WCH compressed byte/halfword accesses, whose generic
            # RVC encodings have different semantics in Unicorn's CPU model.
            if length == 2 and op in ('lbu', 'lhu', 'sb', 'sh'):
                match = re.fullmatch(r'(-?0x[0-9a-f]+|\d+)\((\w+)\)', args[1])
                if not match:
                    raise RuntimeError('Unknown WCH addressing at ' + hex(pc))
                address = m.reg_read(reg(match[2])) + int(match[1], 0)
                width = 2 if op in ('lhu', 'sh') else 1
                if op[0] == 'l':
                    m.reg_write(reg(args[0]), int.from_bytes(m.mem_read(address, width), 'little'))
                else:
                    if not 0x20110000 <= address < address + width <= 0x20190000:
                        raise RuntimeError('Unmodeled compressed store: ' + hex(address))
                    m.mem_write(address, (m.reg_read(reg(args[0])) & ((1 << (8 * width)) - 1)).to_bytes(width, 'little'))
            elif op in ('sh1add', 'sh2add', 'sh3add'):
                value = (m.reg_read(reg(args[1])) << int(op[2])) + m.reg_read(reg(args[2]))
                m.reg_write(reg(args[0]), value & 0xffffffff)
            elif op in ('zext.b', 'zext.h'):
                m.reg_write(reg(args[0]), m.reg_read(reg(args[1])) & (255 if op[-1] == 'b' else 65535))
            elif op in ('bseti', 'bclri'):
                value = m.reg_read(reg(args[1])); bit = 1 << int(args[2], 0)
                m.reg_write(reg(args[0]), value | bit if op == 'bseti' else value & ~bit)
            elif op == 'minu':
                left, right = m.reg_read(reg(args[1])), m.reg_read(reg(args[2]))
                m.reg_write(reg(args[0]), min(left, right))
            elif op in ('csrc', 'csrs') and args[0] == '0x800':
                effects.append({'modeled_private_csr': op})
            else:
                return
            m.reg_write(regs.UC_RISCV_REG_PC, pc + length)

        hook = u.hook_add(UC_HOOK_CODE, step)
        try:
            u.emu_start(entry, 0, timeout=1500000, count=40000)
        except Exception as exc:
            raise RuntimeError({'entry': hex(entry), 'last_pcs': [hex(x) for x in pcs[-16:]], 'error': str(exc)}) from exc
        finally:
            u.hook_del(hook)
        if not reached:
            raise RuntimeError('Replay did not reach an explicit stop')
        return reached[0]

    def writer(self, profile, before, enabled, reset):
        u = self.machine()
        saved = bytearray((i * 17 + (i >> 8)) & 255 for i in range(SECTOR))
        saved[0] = (saved[0] & ~1) | reset
        saved[profile * 64 + 2:profile * 64 + 4] = b'\xaa\xbb'
        saved[profile * 64 + 7] = before
        u.mem_write(FLASH, bytes(saved)); u.mem_write(ALIAS, bytes(saved))
        u.mem_write(ACTIVE, bytes(saved[profile * 64:profile * 64 + 64]))
        u.mem_write(PROFILE, bytes([profile]))
        target = before | 8 if enabled else before & ~8
        offset = profile * 64 + 7
        u.mem_write(REQUEST, bytes([0x55, 6, 0, 0, 1, offset, 0, 0, target]))
        effects = []

        def model(m, pc):
            if pc not in (0xd6e, 0xd8c, 0xcb8, 0xd9a):
                return False
            a0, a1 = m.reg_read(reg('a0')), m.reg_read(reg('a1'))
            if pc == 0xcb8:
                assert a0 == ALIAS
                m.mem_write(ALIAS, b'\xff' * SECTOR); m.mem_write(FLASH, b'\xff' * SECTOR)
                effects.append({'erase': a0})
            elif pc == 0xd9a:
                assert ALIAS <= a0 < ALIAS + SECTOR and a0 % 256 == 0
                data = bytes(m.mem_read(a1, 256))
                m.mem_write(a0, data); m.mem_write(FLASH + a0 - ALIAS, data)
                effects.append({'program': a0})
            m.reg_write(regs.UC_RISCV_REG_PC, m.reg_read(reg('ra')))
            return True

        stop = self.run(u, 0x5688, {0x1000, 0x5754, 0x5772}, effects, model)
        expected = bytearray(saved); expected[offset] = target
        assert bytes(u.mem_read(FLASH, SECTOR)) == bytes(expected)
        assert u.mem_read(ACTIVE + 7, 1) == bytes([target])
        assert u.mem_read(ACTIVE + 1, 1) == bytes([saved[profile * 64 + 1] & 0xe])
        assert sum('erase' in e for e in effects) == 1
        assert sum('program' in e for e in effects) == 16
        assert stop == (0x5754 if reset else 0x1000)
        return {'profile': profile, 'initial': before, 'enabled': enabled, 'reset_flag': reset,
                'stop': hex(stop), 'whole_sector_preserved_except_flag': True,
                'active_mac_byte_mask': '0x0e', 'effects': effects}

    def sample(self, slot, depth, maximum):
        u = self.machine(); effects = []
        table = self.image[FACTORY_FILE:FACTORY_FILE + 128 * 3]
        u.mem_write(FACTORY, table)
        u.reg_write(reg('a0'), slot)
        u.mem_write(0x2017a3e0 + slot * 8, b'\0\0')  # Synthetic switch index0.
        u.mem_write(0x20132348, maximum.to_bytes(2, 'little'))
        u.mem_write(0x2017aca0 + slot * 4, depth.to_bytes(4, 'little'))
        # Stop once the depth/maximum fields have been formed. ADC/USB are not modeled.
        stop = self.run(u, 0xc870, {0xc9ec, 0xcb02}, effects)
        if not table[slot * 3]:
            assert stop == 0xcb02
            return None
        frame = bytes(u.mem_read(BUFFER, 16))
        assert stop == 0xc9ec and frame[0] == 0xa0 and frame[1:4] == table[slot * 3:slot * 3 + 3]
        expected = 0 if depth <= 9 else maximum if depth >= maximum - 5 else depth
        assert int.from_bytes(frame[6:8], 'big') == expected
        assert int.from_bytes(frame[14:16], 'big') == maximum
        return {'slot': slot, 'raw': depth, 'maximum': maximum, 'reported': expected}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path); parser.add_argument('listing', type=Path); parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if not __debug__: raise RuntimeError('Assertions required')
    image = args.image.read_bytes()
    if hashlib.sha256(image).hexdigest() != SHA: raise ValueError('Unreviewed image')
    if args.output.exists(): raise FileExistsError(args.output)
    replay = Replay(image, args.listing.read_text(encoding='utf-8'))
    writers = [replay.writer(p, v, on, 0) for p in range(4) for v in (0, 7, 8, 0x45, 0x4d, 0x80, 0xf7, 0xff) for on in (False, True)]
    writers += [replay.writer(p, 0x45, True, 1) for p in range(4)]
    slots = [i for i in range(96) if image[FACTORY_FILE + i * 3]]
    samples = [replay.sample(slot, depth, 341) for slot in slots for depth in (0, 9, 10, 100, 170, 335, 336, 341)]
    result = {'schema': 1, 'image_sha256': SHA, 'execution': 'pass', 'writers': writers, 'samples': samples,
              'scope': 'Normalized file-offset PCs; synthetic RAM/flash; explicit modeled flash helper effects/private CSR. No ADC, USB delivery, boot or physical timing.'}
    with args.output.open('x', encoding='utf-8') as out: json.dump(result, out, indent=2)
    print('GT75_BRANCH_REPLAY=PASS writers=' + str(len(writers)) + ' samples=' + str(len(samples)) + ' physical_channels=' + str(len(slots)))


if __name__ == '__main__': main()
