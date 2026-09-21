"""Pinned MG75 Pro serializer and V2 travel selector regression (no USB access).
Requires capstone, dnfile and unicorn. Updaters are parsed, never executed.
"""
from pathlib import Path
import hashlib
import struct
import dnfile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / '.local/research/irok-mg75-fn'
STOP = 0x080ff000

def machine(image, address):
    u = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    u.mem_map(0x08000000, 0x100000)
    u.mem_map(0x20000000, 0x100000)
    u.mem_write(address, image)
    u.reg_write(UC_ARM_REG_SP, 0x200ff000)
    u.reg_write(UC_ARM_REG_LR, STOP | 1)
    return u

def run():
    pro = (DATA / '356aa3a6c6c77fde51f32d041c9aaa5b145aca183e8024e085410fdd20544feb.bin').read_bytes()
    assert hashlib.sha256(pro).hexdigest() == '356aa3a6c6c77fde51f32d041c9aaa5b145aca183e8024e085410fdd20544feb'
    table = 0x14756
    assert 0x08054756 + 2 * struct.unpack_from('<H', pro, table + 0x12 * 2)[0] == 0x0805502c
    cases = 0
    for values in ([0]*63, [i*53 for i in range(63)], [3500 if i%2 else 750 for i in range(63)]):
        u = machine(pro, 0x08040000)
        u.mem_write(0x20080000, struct.pack('<63H', *values))
        for reg, val in [(UC_ARM_REG_R0,0x20090000),(UC_ARM_REG_R1,0),(UC_ARM_REG_R2,2),(UC_ARM_REG_R3,0x20080000)]: u.reg_write(reg,val)
        u.emu_start(0x0804951d, STOP, count=50000)
        packet = bytes(u.mem_read(0x20090000,132))
        assert packet[:3] == bytes.fromhex('5c8092')
        assert packet[4:6] == bytes([0,2])
        assert struct.unpack('<63H',packet[6:]) == tuple(values)
        assert u.reg_read(UC_ARM_REG_R0) == 132
        cases += 1
    print(f'PRO_ACTUAL_FIRMWARE_SERIALIZER=PASS cases={cases} independent_values=63 response_bytes=132')
    factory = struct.unpack_from('<126H', pro, 0x1f720)
    slots = [i for i, key in enumerate(factory) if key]
    assert len(slots) == 81
    for first in (0, 2, 4):
        u = machine(pro, 0x08040000)
        for reg, val in [(UC_ARM_REG_R0,0x20090000),(UC_ARM_REG_R1,0),(UC_ARM_REG_R2,first),(UC_ARM_REG_R3,0x0805f720+42*first)]: u.reg_write(reg,val)
        u.mem_write(0x200ff000, struct.pack('<II',first+1,0x0805f720+42*(first+1)))
        u.emu_start(0x08048d11,STOP,count=10000)
        packet = bytes(u.mem_read(0x20090000,49))
        assert packet[:3] == bytes.fromhex('5c2dab') and packet[5] == first and packet[27] == first+1
        for row, offset in [(first,6),(first+1,28)]:
            assert packet[offset:offset+21] == bytes(1 if key==0xf001 else key for key in factory[row*21:(row+1)*21])
    # Execute the command-23 read branch with a seeded live base layer.
    # The actual firmware emits all fourteen records, including invalid padding.
    for remapped in (False, True):
        live = list(factory)
        if remapped: live[44],live[64],live[116] = 4,0,0xf001
        for start in range(0,len(slots),14):
            selected = slots[start:start+14]
            u = machine(pro,0x08040000)
            sp = 0x200ee000
            u.reg_write(UC_ARM_REG_SP,sp)
            u.mem_write(0x20000698,struct.pack('<126H',*live))
            request = bytearray(64)
            request[:5] = bytes([0x5c,57,0x23,0,0])
            for j, slot in enumerate(selected): request[5+4*j] = 1 if factory[slot]==0xf001 else factory[slot]
            request[3] = (0x35+request[0]+request[1]+request[2]+request[60])&255
            u.mem_write(sp+0xc4,bytes(request))
            u.emu_start(0x0805540b,0x080565c6,count=100000)
            packet = bytes(u.mem_read(sp+0xc4,61))
            assert packet[:3] == bytes.fromhex('5c39a3')
            assert packet[3] == (0x35+packet[0]+packet[1]+packet[2]+packet[60])&255
            for j, slot in enumerate(selected):
                assert packet[5+4*j:9+4*j] == bytes([request[5+4*j],0])+struct.pack('<H',live[slot])
            assert bytes(u.mem_read(0x20000698,252)) == struct.pack('<126H',*live)
    print('PRO_ACTUAL_FIRMWARE_MAP=PASS rows=6 keys=81 remap_unassigned_fn=PASS no_live_map_mutation=PASS')

    exe = DATA / 'MG75V2-20260713-v1.21-A.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == '2d1b128d671aa0f9b3206ca95a1e60472af01b0be547ae7714df673984262b7a'
    pe = dnfile.dnPE(str(exe))
    resource = next(r.data for r in pe.net.resources if str(r.name) == 'IAP_Demo.Resources.data.bin')
    load, size = struct.unpack_from('<II',resource,4)
    assert load == 0x08006000 and size == 0x602b0
    image = resource[64:64+size]
    assert image == (DATA/'mg75v2-resource-data.bin').read_bytes()[64:64+size]
    print('V2_PAYLOAD=PASS bytes=',len(image),'sha256=',hashlib.sha256(image).hexdigest())
    cases = 0
    # Execute the real selector; substitute only calibrated travel conversion
    # and USB transmission. This is not a live sensor or USB timing test.
    for depths in ({}, {0:500,1:1500,2:1000}, {0:1800,1:700}, {0:600,1:600}):
        u = machine(image,load)
        u.mem_write(0x2000048d,bytes([0x20]))
        u.mem_write(0x08040000,bytes(range(1,91)))
        captured=[]
        def hook(uc, address, size, data):
            if address == 0x08008818:
                i=uc.reg_read(UC_ARM_REG_R0)*6+uc.reg_read(UC_ARM_REG_R1)
                uc.reg_write(UC_ARM_REG_R0,depths.get(i,0))
                uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
            elif address == 0x08017bf0:
                captured.append(bytes(uc.mem_read(0x2000bc28,6)))
                uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
        u.hook_add(UC_HOOK_CODE,hook)
        u.emu_start(0x0801584d,STOP,count=100000)
        assert len(captured)==1
        packet=captured[0]
        expected=max(depths.values(),default=0)
        key=next((i+1 for i in range(90) if depths.get(i,0)==expected),0) if expected else 0
        assert packet[0]==0xa0 and packet[1]==key
        assert int.from_bytes(packet[4:6],'big')==expected
        cases+=1
    print(f'V2_ACTUAL_FIRMWARE_SELECTOR=PASS cases={cases} single_maximum_only=1')

if __name__ == '__main__': run()
