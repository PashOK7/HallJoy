"""Exact BOOM68 stock RAM getter review; no hardware or updater execution."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R5, UC_ARM_REG_R11
from firmware_replay import thumb_machine, run_until
from firmware_behavior import CHECKS, validate, render

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / '.local/research/aula-boom68-feedback-20261114'
HASH = '4d44b59daeef15271826043715ff5126324c4d84bf19c6407de0511398a86f25'
RX, TX, STATE = 0x200059a8, 0x20005968, 0x20000302
RAM, DEPTH = 0x20002442, 0x20002a42


def sha(b): return hashlib.sha256(b).hexdigest()


def cpu(image):
    u = thumb_machine(image, 0)
    u.flash_calls = []
    u.hook_add(UC_HOOK_CODE, lambda m, a, n, _: m.flash_calls.append(a) if a == 0x3e88 else None)
    return u


def command(u, op, size=0, offset=0, payload=None, stop_flash=False):
    assert 0 <= size <= 56
    request = bytearray([0xa5] * 64)
    request[:8] = bytes([0xaa, op, size, offset & 255, offset >> 8, 0, 1, 0])
    if payload is not None: request[8:8+len(payload)] = payload
    u.mem_write(RX, bytes(request)); u.mem_write(STATE+4, b'\1'); u.mem_write(STATE+3, b'\0')
    u.reg_write(UC_ARM_REG_SP, 0x2001f000); u.reg_write(UC_ARM_REG_LR, 0x101)
    end = run_until(u, 0xd214, {0x100, 0x3e88} if stop_flash else {0x100}, budget=20000)
    return end, bytes(u.mem_read(TX, 64))


def main():
    if not __debug__: raise RuntimeError('Assertions required')
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('output', type=Path); args = p.parse_args()
    assert not args.output.exists()
    image = (INPUT/'resource-4000.bin').read_bytes(); assert sha(image) == HASH
    producer = (INPUT/'layout-classic-QUpyWhzQ.js').read_bytes()
    text = producer.decode(); start = text.index('Cu={vendorId:3141,productId:32949'); end = text.index('},hu=Object.freeze', start)
    keys = [(int(v), int(h)) for v, h in re.findall(r'value:(\d+),name:(?:"(?:[^"\\]|\\.)*"|`[^`]*`),keyCode:(\d+)', text[start:end])]
    assert len(keys) == 67 and len({v for v, _ in keys}) == 67
    channel_map = image[0x1358c:0x1360c]
    assert all(channel_map.count(v) == 1 for v, _ in keys)
    slots = {v: channel_map.index(v) for v, _ in keys}
    u = cpu(image); rows = []
    # Read-only ROM evidence does not use GET10, whose old-marker conversion writes flash.
    for address, length in [(0xd214, 32), (0xd354, 12), (0xd634, 4), (0x1358c, 56)]:
        _, r = command(u, 0x12, length, address-0x9600)
        assert r[8:8+length] == image[address:address+length]
    assert not u.flash_calls
    _, echo = command(u, 0x68, 24, payload=bytes([17])+bytes(7)+bytes([49])+bytes(7)+bytes([83])+bytes(7))
    assert echo[8:32] == bytes([17])+bytes(7)+bytes([49])+bytes(7)+bytes([83])+bytes(7)
    command(u, 0x66); assert u.mem_read(STATE+2, 1) == b'\1' and u.mem_read(STATE+1, 1) == b'\0'
    command(u, 0x67); assert u.mem_read(STATE+2, 1) == b'\0'
    before = bytes(u.mem_read(STATE, 16))
    # Actual machine-code getter over synthetic already-converted scanner RAM.
    # Each phase represents an independent update by the real scan producer.
    for phase in range(5):
        values = [0] * 128
        for v, _ in keys:
            values[slots[v]] = 0 if phase in (0, 4) or (phase == 2 and v == 17) else 1 if phase == 3 else 25 + v % 250
        raw = struct.pack('<128H', *values); u.mem_write(DEPTH, raw)
        captured = bytearray()
        writes = []
        hook = u.hook_add(UC_HOOK_MEM_WRITE, lambda m,a,addr,size,value,_: writes.append((addr,size)))
        for offset in range(0, 256, 56):
            count = min(56, 256-offset)
            _, r = command(u, 0x16, count, DEPTH-RAM+offset)
            assert r[:2] == b'\x55\x16' and r[2] == count
            captured.extend(r[8:8+count])
        u.hook_del(hook)
        assert bytes(captured) == raw and not u.flash_calls
        assert bytes(u.mem_read(STATE, 3)) == before[:3]
        # Getter writes only reply/ready flags and its own stack; not sensor/settings state.
        assert all(TX <= a < RX+64 or STATE+3 <= a < STATE+5 or 0x2001e000 <= a < 0x2001f000 for a,n in writes)
        rows.append(dict(phase=phase, simultaneous=sum(x>0 for x in values), minimum=min(values[slots[v]] for v,_ in keys), maximum=max(values), exact_matrix=True, settings_unchanged=True))
    assert [r['simultaneous'] for r in rows] == [0, 67, 66, 67, 0]
    # Per-switch clamp selected by the real scanner switch table. Do not guess from a label.
    ends = []
    for axis in range(8):
        m = cpu(image); m.reg_write(UC_ARM_REG_R11, axis)
        run_until(m, 0x1fea, {0x202c}, budget=80)
        ends.append(m.reg_read(UC_ARM_REG_R5))
    assert ends == [330,330,330,330,340,340,330,340]
    # GET10's hidden flash marker conversion is demonstrated and excluded from host requests.
    m = cpu(image); stop, _ = command(m, 0x10, 24, stop_flash=True); assert stop == 0x3e88
    raw_result = dict(schema=1, firmware_sha256=HASH, entry='0xd214', tx=hex(TX), ram_getter_base=hex(RAM), depth=hex(DEPTH), channels=slots, phases=rows, axis_bottom=ends, get10_flash_call_excluded=True, normal_getters_flash_calls=0, status68_echo=True, monitor66_volatile_only=True)
    args.output.mkdir(parents=True); assets = {}
    for key, path, data, role in [('firmware',INPUT/'resource-4000.bin',image,'firmware-input'),('producer',INPUT/'layout-classic-QUpyWhzQ.js',producer,'producer-code'),('audit',Path(__file__),Path(__file__).read_bytes(),'audit-source')]:
        name = key+'-'+path.name
        with (args.output/name).open('xb') as f: f.write(data)
        assets[key] = dict(path=name,sha256=sha(data),role=role)
    raw = (json.dumps(raw_result,indent=2)+'\n').encode()
    with (args.output/'raw.json').open('xb') as f: f.write(raw)
    assets['raw'] = dict(path='raw.json',sha256=sha(raw),role='raw-result')
    ev = lambda loc, level='branch_replay': [dict(asset='raw',locator=loc,level=level)]
    findings = {k:dict(state='unknown',detail='Physical USB, interrupts and actual tester hardware have not been exercised.',evidence=[]) for k in CHECKS}
    for name in ('simultaneous_keys','release'):
        findings[name]=dict(state='established',detail='GET16 copies all independent converted channels, including all-zero release, from the existing normal-scanner cache. Five modeled scanner phases cover67 simultaneous channels and independent release; not physical ADC/USB proof.',evidence=ev('/phases'))
    findings['persistent_settings']=dict(state='established',detail='Reviewed GET12 and GET16 write only response/ack/ready state and stack. GET10 can write flash and is excluded. No calibration, monitor, configuration or flash write is needed.',evidence=ev('/get10_flash_call_excluded'))
    findings['normal_exit']=dict(state='established',detail='Read-only route enables no mode and owns no persistent or volatile monitor flag; stop closes host handle and clears host input.',evidence=ev('/phases'))
    findings['depth_scale']=dict(state='established',detail='Read the actual calibrated depth cache used by normal scanner. Its per-axis selected bottom is330 or340 hundredths of a millimeter. Other physical sensor properties remain unmeasured.',evidence=ev('/axis_bottom'))
    findings['physical_mapping']=dict(state='established',detail='Exact ROM hardware-channel map contains each of the manufacturer67 physical key values exactly once, includingFn.',evidence=ev('/channels'))
    findings['normal_typing']=dict(state='established',detail='The selected getter does not touch scanner/calibration/settings state or divert the normal processor. Real concurrent HID scheduling and gameplay are unmeasured.',evidence=ev('/phases'))
    findings['shallow_input']=dict(state='restricted',detail='The source is the firmware calibrated/filtered cache, not raw ADC. Its own shallow zero cutoff and bottom snap remain. A supplied cache value1 is copied exactly; no host cutoff is added.',evidence=ev('/phases/3'))
    findings['temporal_filtering']=dict(state='restricted',detail='Existing scanner curve/hysteresis retained; no host smoothing or polling cap. Pages form independently fresh replies, not one atomic whole-board sampling instant.',evidence=[dict(asset='firmware',locator='202C..2196 calibrated depth cache producer',level='static')])
    candidates=[dict(id='ram16',disposition='limited',reason='Read-only full depth cache through the proven RAM getter; requires matching code structure/addresses before reading RAM. Retains firmware filtering; hardware validation pending.',evidence=ev('/phases')),dict(id='status68',disposition='unresolved',reason='This exact image echoes68. A newer implementing image could expose the documented independent status getter; retain only a proven response.',evidence=ev('/status68_echo')),dict(id='monitor66',disposition='rejected',reason='Only toggles simulation state; sole reviewed FB builder is calibration-gated and no full ordinary producer is established.',evidence=ev('/monitor66_volatile_only')),dict(id='calibration64',disposition='rejected',reason='Calibration changes normal scan/settings and is not an analog substitute.',evidence=[dict(asset='firmware',locator='D510..D524 and2306..2332',level='static')])]
    report=dict(schema=1,model='LANGTU BOOM68-USB stock1.10 0C45:80B5',scope='Exact stock firmware, synthetic normal-scanner cache and actual dispatcher execution; no physical hardware claim.',assets=assets,execution=dict(outcome='pass',entry_boundary='D214 actual dispatcher;1FEA scanner range switch',stop_boundary='LR100;202C;GET10 stopped before flash helper3E88',mocked='Initialized RAM/current calibrated depths; no fabricated analog in production.',not_modeled='ADC, boot, physical USB timing, interrupts, real concurrent typing/gameplay, configurator contention.'),findings=findings,candidates=candidates,selection=dict(candidate='ram16',rationale='Direct independent current matrix without monitor or calibration, avoids loss-prone events and GET10 flash mutation.',coverage_limits='Pinned dispatcher/producer fragments and modeled depth updates, not exhaustive firmware or peripheral execution.'))
    result=validate(report,args.output)
    for name, data in [('assessment.json',json.dumps(report,indent=2)+'\n'),('REPORT.md',render(report,result))]:
        with (args.output/name).open('x',encoding='utf-8') as f:f.write(data)
    print('BOOM68_FIRMWARE=PASS channels=67 phases=5 getters_readonly=1 verdict='+result['verdict'])


if __name__ == '__main__': main()
