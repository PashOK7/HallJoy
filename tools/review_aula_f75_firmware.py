"""Exact F75 dispatcher and post-conversion producer review, no device I/O.

This audit deliberately reproduces unsupported getters and unused monitor state.
Execution PASS is NOT working analog support. A later tester firmware can differ.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import UC_PROT_READ
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R4
from firmware_replay import thumb_machine, run_until
from firmware_behavior import CHECKS, validate, render

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT/'.local/research/aula-f75-20261113'
IMAGES = [
    ('2729-v155', 'f75-reset-firmware.bin', '8eb7b00451059cf381cc111a4c38fefad5ac9695015dc729a87f91a88fea2884'),
    ('2742-v103', 'new-resource-4000.bin', 'e1f326c35bab42db8b69638cd00073ec35a3f0a10edd97e5bed6addfcedc2909'),
]
RX, TX = 0x20015fb0, 0x20015f70
CAL, SIM, READY, ACK = 0x2000045e, 0x2000045f, 0x20000461, 0x20000460


def digest(data):
    return hashlib.sha256(data).hexdigest()


def machine(image):
    u = thumb_machine(image, 0)
    # Exact modeled MCU identification register read by GET_DEVICE_INFO.
    # Synthetic constant, not a real serial/device identification acquisition.
    u.mem_map(0x00400000, 4096, UC_PROT_READ)
    u.mem_write(0x00400180, struct.pack('<I', 0x110c))
    u.reg_write(UC_ARM_REG_LR, 0x101)
    return u


def command(u, opcode, size=0, address=0, payload=b''):
    request = bytearray(64)
    request[:8] = bytes([0xaa, opcode, size, address & 255, address >> 8, 0, 1, 0])
    request[8:8+len(payload)] = payload
    u.mem_write(RX, bytes(request))
    u.mem_write(READY, b'\1')
    u.mem_write(ACK, b'\0')
    u.reg_write(UC_ARM_REG_SP, 0x2001f000)
    u.reg_write(UC_ARM_REG_LR, 0x101)
    run_until(u, 0x12fd8, {0x100}, budget=20000)
    return bytes(u.mem_read(TX, 64))


def review(label, image, keys):
    u = machine(image)
    rows = []
    for opcode, expected in [(0x66, 1), (0x67, 0), (0x66, 1)]:
        command(u, opcode)
        assert u.mem_read(SIM, 1)[0] == expected
        assert u.mem_read(CAL, 1)[0] == 0
        assert u.mem_read(ACK, 1)[0] == 1
        rows.append(dict(opcode=opcode, simulation=expected, calibration=0, ack=1))
    # Unknown68 acknowledges by echoing the request body. A released-key sample
    # is valid zero, but an unchanged request is not independent analog proof.
    info = command(u, 0x10, 24)
    requested = b''.join(bytes([key])+bytes(7) for key in [k for k in keys if k>0][:3])
    status = command(u, 0x68, 24, payload=requested)
    assert status[8:32] == requested
    rows.append(dict(opcode=0x68, generic_ack=True, request_echo=True,
                     zero_depth_fields_are_not_proof=True, usable_depth=False))
    # Verify fixed ROM windows used for a private running-code fingerprint.
    # GET10 replaces payload14..18 with MCU identification/zero: never hash/log it.
    fingerprints = []
    for start in (0x13446, 0x13138):
        reply = command(u, 0x10, 24, start-0x9000)
        mask = [i for i in range(24) if not 14 <= i < 19]
        actual = bytes(reply[8+i] for i in mask)
        expected = bytes(image[start+i] for i in mask)
        assert actual == expected
        fingerprints.append(dict(address=hex(start), bytes_hashed=len(actual),
                                 sha256=digest(actual), mcu_word_excluded=True))
    # Current calibrated travel is converted before this exact tail boundary.
    # Synthetic channel/key association is supplied, not an ADC/peripheral model.
    producer = []
    for simulation in (0, 1):
        for key in keys:
            for travel in (0, 1, 170, 340):
                m = machine(image)
                m.mem_write(CAL, b'\0');m.mem_write(SIM, bytes([simulation]))
                m.mem_write(0x2000039b, b'\1')
                m.mem_write(0x200000b5, b'\0')
                m.mem_write(0x20002ede, struct.pack('<H', travel))
                m.reg_write(UC_ARM_REG_R4, key)
                stop = run_until(m, 0x7742, {0x77de, 0x77e4}, budget=300)
                assert stop == 0x77de  # ordinary processing call remains reachable
                assert bytes(m.mem_read(0x2000015e, 1)) == b'\0'
                assert bytes(m.mem_read(0x2000390e, 14)) == bytes(14)
        producer.append(dict(simulation=simulation, positions=len(keys), depths=4,
                             ordinary_continuation_reached=True, fb_records=0))
    return dict(image=label, sha256=digest(image), dispatcher_entry='0x12fd8',
                commands=rows, fingerprints=fingerprints, producer=producer,
                producer_entry='0x7742', producer_stop='0x77de',
                normal_typing_hid_delivery_tested=False, radio_peripheral_tested=False)


def main():
    if not __debug__:
        raise RuntimeError('Assertions are required')
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    args = p.parse_args()
    if args.output.exists():
        raise FileExistsError('Output must be new')
    geometry = json.loads((PRIVATE/'hfd-array-y.json').read_text(encoding='utf-8'))
    keys = sorted(k['keyValue'] for k in geometry)
    assert len(keys) == 80 and len(set(keys)) == 80
    inputs = {Path(__file__): 'audit-source', ROOT/'tools/firmware_replay.py': 'audit-source',
              PRIVATE/'hub-hfd-sdk.es-CwIM3CGY.js': 'producer-code',
              PRIVATE/'hub-HFD-DSWOC1e8.js': 'producer-code'}
    results = []
    for label, filename, expected in IMAGES:
        path = PRIVATE/filename
        image = path.read_bytes()
        assert digest(image) == expected
        inputs[path] = 'firmware-input'
        results.append(review(label, image, keys))
    args.output.mkdir(parents=True)
    assets = {}
    originals = {}
    for n, (path, role) in enumerate(inputs.items()):
        data = path.read_bytes();originals[path] = data
        name = str(n)+'-'+path.name
        with (args.output/name).open('xb') as stream:
            stream.write(data)
        assets[path.name] = dict(path=name, sha256=digest(data), role=role)
    raw = (json.dumps(dict(schema=1, results=results), indent=2)+'\n').encode()
    with (args.output/'raw.json').open('xb') as stream:
        stream.write(raw)
    assets['raw'] = dict(path='raw.json', sha256=digest(raw), role='raw-result')
    ev = lambda asset, locator, level='static': [dict(asset=asset, locator=locator, level=level)]
    findings = {k: dict(state='unknown', detail='Not established on the tester keyboard or receiver.', evidence=[]) for k in CHECKS}
    for k in ('simultaneous_keys', 'release'):
        findings[k] = dict(state='restricted', detail='The reviewed normal post-conversion producer emits no FB data with66 alone, including zero releases. No usable analog from these fragments is inferred.', evidence=ev('raw', '/results/*/producer', 'branch_replay'))
    findings['physical_mapping'] = dict(state='established', detail='Manufacturer geometry supplies80 physical positions; legacy/new Fn and right Ctrl geometry differs.', evidence=ev('hub-HFD-DSWOC1e8.js', 'arrays o/y and keyboard dictionary t'))
    findings['persistent_settings'] = dict(state='established', detail='66/67 only change volatile simulation state in the exact reviewed dispatchers. Get10 and unknown68 do not enter settings/calibration writes.', evidence=ev('raw', '/results/*/commands', 'branch_replay'))
    findings['normal_exit'] = dict(state='established', detail='67 clears the volatile66 flag in both exact images. Physical USB cleanup remains a host/device test concern.', evidence=ev('raw', '/results/*/commands', 'branch_replay'))
    findings['depth_scale']['detail'] = 'FB builder supplies stroke34 and the reviewed current-depth conversion uses a float scale before the tail. New68 status units are a family-format compatibility assumption until actual supported firmware data is observed; do not claim exact target calibration.'
    report = dict(schema=1, model='AULA F75 HE legacy2729 and new2742',
        scope='Exact acquired keyboard images1.55/1.03; current host SDK; not a receiver firmware dump or physical test.',
        assets=assets, execution=dict(outcome='pass', entry_boundary='12FD8 dispatcher;7742 after calibrated depth conversion',
            stop_boundary='Explicit LR100;77DE before normal-processing call', mocked='Synthetic RAM, key/channel index, converted travel and explicit read-only MCU identification register00400180.',
            not_modeled='ADC/calibration/boot/interrupts, radio firmware and timing, physical USB or normal HID delivery.'),
        findings=findings, candidates=[
            dict(id='status68', disposition='unresolved', reason='New SDK documents multi-key ADC/currentStroke requests, but acquired images return generic ACK with an echoed request body. Trial must distinguish the request echo from a live zero sample.', evidence=ev('hub-hfd-sdk.es-CwIM3CGY.js', 'Ae / GET_MAGNETIC_AXIS_STATUS104')+ev('raw', '/results/*/commands/3', 'branch_replay')),
            dict(id='simulation66', disposition='unresolved', reason='Reversible monitor request is acknowledged but the reviewed producer does not emit depth with that flag alone. Other actual firmware remains unmeasured.', evidence=ev('raw', '/results/*/producer', 'branch_replay')),
            dict(id='calibration64', disposition='rejected', reason='Calibration-gated FB generation is not a normal gameplay path; it alters calibration/ordinary processing behavior and is not an authorized substitute.', evidence=ev('new-resource-4000.bin', '7756..77E2 calibration checks and normal-processing continuation')),
            dict(id='flash_read', disposition='rejected', reason='Reviewed10..1C getters select fixed flash windows, not the current depth RAM cache; no arbitrary RAM-read command has been established.', evidence=ev('new-resource-4000.bin', '13036..13168'))],
        selection=dict(candidate=None, rationale='No acquired-image unrestricted analog path is established. Implement safe compatibility attempts and useful ordinary diagnostics without declaring working support.', coverage_limits='Bounded dispatcher/post-conversion replay and producer API inspection; not exhaustive command/peripheral execution or proof that later firmware cannot implement68/66.'))
    result = validate(report, args.output)
    for name, data in [('assessment.json', json.dumps(report, indent=2)+'\n'), ('REPORT.md', render(report, result))]:
        with (args.output/name).open('x', encoding='utf-8') as stream:
            stream.write(data)
    if any(path.read_bytes() != data for path, data in originals.items()):
        raise RuntimeError('Input changed during audit')
    print('AULA_F75_REPLAY=PASS images=2 producer_cases=1280 echo_ack_rejected=1 verdict='+result['verdict'])


if __name__ == '__main__':
    main()
