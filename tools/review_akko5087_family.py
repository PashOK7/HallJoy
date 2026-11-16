"""Scoped RY5088 replay for the Akko3131 investigation; no target-image claim.

Creates a NEW private evidence bundle. Exact3131 firmware is not available here.
The manufacturer's exact3131 host class inherits the reviewed family getter.
Synthetic RAM and the explicitly named post-conversion producer boundary do not
model boot, physical switches, calibration, USB, interrupts or real timing.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
from unicorn.arm_const import (UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R5,
                              UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10)
from firmware_replay import thumb_machine, run_until
from firmware_behavior import CHECKS, validate, render
from review_monsgeek_firmware import CASES, HASHES, RESEARCH

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def replay(board, image, entry, slots):
    base = 0x08000000
    cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    ins = list(cs.disasm(image[entry-base:entry-base+1500], entry))
    def literal(i):
        assert i.mnemonic == 'ldr' and '[pc,' in i.op_str
        offset = int(i.op_str.split('#')[1].split(']')[0], 0)
        return struct.unpack_from('<I', image, ((i.address+4)&~3)+offset-base)[0]
    command, state = literal(ins[1]), literal(ins[4])
    branch = next(ins[n+1] for n, i in enumerate(ins)
                  if i.mnemonic == 'cmp' and i.op_str == 'r1, #0xfe')
    target = int(branch.op_str.lstrip('#'), 0)
    tail = list(cs.disasm(image[target-base:target-base+20], target))
    assert tail[0].op_str == 'r0, r2, r0, lsl #6'
    assert tail[1].mnemonic == 'movw' and tail[3].op_str == 'r2, #0x40'
    depth = state + int(tail[1].op_str.split('#')[1], 0)
    u = thumb_machine(image, base)
    stop = base + 0x100  # explicit, in-image return boundary; not executed
    scenarios = [
        ('all_zero', [0]*128),
        ('87_distinct_simultaneous', [20+5*i if i in slots else 0 for i in range(128)]),
        ('two_independent_holds', [350 if i in slots[:2] else 0 for i in range(128)]),
        ('release_first_keep_second', [350 if i == slots[1] else 0 for i in range(128)]),
        ('release_all', [0]*128),
        ('shallow_9_10_boundary', [9 if i == slots[0] else 10 if i == slots[1] else 0 for i in range(128)]),
    ]
    results = []
    for name, requested in scenarios:
        values = list(requested)
        if board == 2949:
            # Exact sibling v410 post-conversion producer, monitor flag left zero.
            # Inputs are synthetic already-converted depths, not ADC samples.
            for index in slots:
                u.reg_write(UC_ARM_REG_SP, 0x2001f000)
                u.mem_write(0x2001f010, struct.pack('<I', index//6))
                u.reg_write(UC_ARM_REG_R9, index%6)
                u.reg_write(UC_ARM_REG_R10, 0x20014000)
                u.mem_write(0x20014f44, struct.pack('<H', requested[index]))
                u.reg_write(UC_ARM_REG_R5, state+0x7000+2*index)
                u.reg_write(UC_ARM_REG_R8, state+0x7000)
                run_until(u, 0x0800e2ce, {0x0800e32c}, budget=300)
                values[index] = requested[index] if requested[index] >= 10 else 0
                assert struct.unpack('<H', u.mem_read(depth+2*index, 2))[0] == values[index]
        else:
            u.mem_write(depth, struct.pack('<128H', *values))
        pages = []
        for page in range(4):
            u.mem_write(command, bytes(70))
            u.mem_write(command+10, bytes([255])*56)
            u.mem_write(command+3, bytes([0xfe, 1, page]))
            u.reg_write(UC_ARM_REG_SP, 0x2001f000)
            u.reg_write(UC_ARM_REG_LR, stop | 1)
            before = bytes(u.mem_read(0x20000000, 0x20000))
            run_until(u, entry, {stop}, budget=5000)
            observed = bytes(u.mem_read(command+2, 64))
            expected = struct.pack('<32H', *values[page*32:(page+1)*32])
            assert observed == expected
            after = bytes(u.mem_read(0x20000000, 0x20000))
            allowed = range(command+2-0x20000000, command+66-0x20000000)
            assert all(a == b or n in allowed or 0x1e000 <= n < 0x1f000
                       for n, (a, b) in enumerate(zip(before, after)))
            pages.append(list(struct.unpack('<32H', observed)))
        results.append(dict(name=name, pages=pages, getter_only_reply_writes=True,
                            producer_executed=board == 2949))
    return dict(board=board, sha256=sha(image), entry=hex(entry), stop=hex(stop),
                command=hex(command), depth_table=hex(depth), scenarios=results,
                outcome='pass', target3131_image=False)


def main():
    if not __debug__:
        raise RuntimeError('Assertions are required for this research audit')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('Evidence directory must be new')
    inputs = {Path(__file__): 'audit-source', ROOT/'tools/firmware_replay.py': 'audit-source',
              ROOT/'tools/review_monsgeek_firmware.py': 'audit-source'}
    for name in ('index.4289e208.js', 'a0c3a7d8.js', '26f7a06d.js'):
        inputs[RESEARCH/name] = 'producer-code'
    for _, name, _ in CASES:
        inputs[RESEARCH/name] = 'firmware-input'
    originals = {p: p.read_bytes() for p in inputs}
    loader = originals[RESEARCH/'a0c3a7d8.js'].decode('utf-8')
    section = loader[loader.index('class e5 extends k'):]
    matrix = list(map(int, re.search(r'defaultMatrix=\[([\d,]+)\]', section).group(1).split(',')))
    assert len(matrix) == 512
    slots = [i for i in range(128) if
             (matrix[4*i] == 0 and matrix[4*i+1] == 0 and matrix[4*i+3] == 0 and matrix[4*i+2]) or
             matrix[4*i:4*i+2] == [10, 1]]
    assert len(slots) == 87
    rows = []
    for board, name, entry in CASES:
        image = originals[RESEARCH/name]
        assert sha(image) == HASHES[board]
        rows.append(replay(board, image, entry, slots))
    args.output.mkdir(parents=True)
    assets = {}
    for n, (path, role) in enumerate(inputs.items()):
        data = originals[path]
        name = f'{n}-{path.name}'
        with (args.output/name).open('xb') as stream:
            stream.write(data)
        assets[path.name] = dict(path=name, sha256=sha(data), role=role)
    raw = (json.dumps(dict(schema=1, target3131_image=False, rows=rows), indent=2)+'\n').encode()
    with (args.output/'raw.json').open('xb') as stream:
        stream.write(raw)
    assets['raw'] = dict(path='raw.json', sha256=sha(raw), role='raw-result')
    evidence = lambda asset, locator, level='static': [dict(asset=asset, locator=locator, level=level)]
    findings = {k: dict(state='unknown', detail='Exact3131 binary and physical device behavior have not been executed.', evidence=[]) for k in CHECKS}
    findings['physical_mapping'] = dict(state='established', detail='Exact manufacturer3131 class e5 has87 keyboard positions including Fn; calculator is a separate consumer action.', evidence=evidence('a0c3a7d8.js', 'class e5 extends k / defaultMatrix'))
    findings['persistent_settings'] = dict(state='established', detail='Selected inherited E5/FE getter issues read-only commands; sibling execution modifies only the response buffer (plus stack). No calibration or settings enable/write is selected.', evidence=evidence('26f7a06d.js', '_getJiaoZhunXinXi / _getMulitMagnetismCMD(254,4,1,1)')+evidence('raw', '/rows/*/scenarios/*/getter_only_reply_writes', 'branch_replay'))
    findings['depth_scale']['detail'] = 'Producer documents version/feature-based units; stock3.5mm range is a manufacturer specification. Exact3131 firmware transfer curve and replacement switches remain unverified.'
    findings['release']['detail'] = 'Sibling getters copy every modeled slot including zeros; v410 post-conversion producer stores releases with monitor flag off. This does not execute3131 sensor processing.'
    findings['simultaneous_keys']['detail'] = 'Sibling full pages retain87 independent modeled values. Exact3131 scan scheduling and physical multikey behavior remain unverified.'
    findings['shallow_input']['detail'] = 'Sibling v410 post-conversion producer suppresses values below10. Its threshold cannot be claimed as an exact3131 property.'
    findings['normal_exit'] = dict(state='established', detail='Read-only getter requires no state restoration; host closes its exclusive handle and clears publications. This establishes command-side behavior, not physical USB recovery.', evidence=evidence('26f7a06d.js', '_getJiaoZhunXinXi: no enable/disable or configuration write'))
    report = dict(schema=1, model='Akko 5087 V3 HE / board3131',
        scope='Exact manufacturer host class plus four named sibling RY5088 binary fragments; NOT an acquired3131 firmware image or a physical test.',
        assets=assets, execution=dict(outcome='pass', entry_boundary='Four GET_MULTI_MAGNETISM entries listed in raw.json; sibling2949 producer0800e2ce',
            stop_boundary='Getter LR08000100 before execution; producer0800e32c before optional event branch',
            mocked='Synthetic RAM, already-converted depth inputs, LR/registers and stack; no USB submit call in selected getter',
            not_modeled='Exact3131 binary; boot, calibration, ADC, interrupts, physical USB, HID coexistence and timing'),
        findings=findings, candidates=[
            dict(id='snapshot_e5_fe', disposition='suitable', reason='Best reviewed read-only full-matrix path; factory position data does not depend on receiving every change event.', evidence=evidence('26f7a06d.js', '_getJiaoZhunXinXi / E5FE')),
            dict(id='single_key_events', disposition='rejected', reason='No advantage over direct current-state snapshots; introduces additional report state and potential lost-release dependence.', evidence=evidence('26f7a06d.js', 'setJiaoZhun / getJiaoZhunXinXiLoop')),
            dict(id='raw_adc_ff', disposition='unresolved', reason='Raw ADC needs calibration/conversion; calibrated E5FE makes a guessed host baseline unnecessary.', evidence=evidence('26f7a06d.js', 'getMultiMagnetismInfo / subtype255'))],
        selection=dict(candidate='snapshot_e5_fe', rationale='Use exact3131 factory mapping and inherited calibrated getter, independently prove live replies in HallJoy; retain explicit untested scope.',
            coverage_limits='Bounded getter/producer replay on four pinned sibling images; no exhaustive protocol search or exact3131 sensor/boot replay. Target API lookup and filename probes did not acquire that image.'))
    result = validate(report, args.output)
    for name, text in [('assessment.json', json.dumps(report, indent=2)+'\n'), ('REPORT.md', render(report, result))]:
        with (args.output/name).open('x', encoding='utf-8') as stream:
            stream.write(text)
    if any(p.read_bytes() != data for p, data in originals.items()):
        raise RuntimeError('Pinned input changed during audit')
    print('AKKO_FAMILY_REPLAY=PASS images=4 getter_scenarios=24 pages=96 producer_passes=522 target3131_image=0 verdict='+result['verdict'])


if __name__ == '__main__':
    main()
