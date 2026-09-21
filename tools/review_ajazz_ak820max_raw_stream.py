"""Exercise SG8994HE command 0x23 through dispatcher, matrix loop and scheduler.

Synthetic ADC/RAM; no hardware or interrupt timing claims. No calibration mode.
"""
import runpy
import struct
from pathlib import Path
from unicorn.arm_const import UC_ARM_REG_R0

s=runpy.run_path(str(Path(__file__).with_name('review_ajazz_ak820max_routes.py')))
cpu,image,call,base=s['cpu'],s['image'],s['call'],s['base']
flags,output=0x20000400,0x200032e4
request=s['request']
def enable(value):
    r=bytearray(64);r[0]=1;r[1]=0x23;r[6]=value
    cpu.mem_write(request,bytes(r));cpu.reg_write(UC_ARM_REG_R0,request)
    call(0xa5c8)

def snapshot():
    cpu.mem_write(flags,bytes(4))
    cpu.mem_write(0x200032b4+0xb8,struct.pack('<I',1<<20))
    call(0x83b4,0x8ad0)
    assert struct.unpack('<I',cpu.mem_read(flags,4))[0] & (1<<24)
    expected=struct.unpack('<132H',cpu.mem_read(0x20002798,264))
    actual=[]
    for row in range(6):
        cpu.mem_write(0x200032b4+0xb8,struct.pack('<I',1<<20))
        call(0xa5de)
        r=bytes(cpu.mem_read(output,64))
        assert r[:2]==bytes([1,0x23]) and r[3:6]==bytes([0,row+1,44]),r.hex()
        actual.extend(struct.unpack('>22H',r[6:50]))
    assert tuple(actual)==expected
    assert not struct.unpack('<I',cpu.mem_read(flags,4))[0] & (1<<24)
    return actual

cpu.mem_write(flags,bytes(4));cpu.mem_write(0x200032b4+0xb8,bytes(4))
enable(1)
assert struct.unpack('<I',cpu.mem_read(0x200032b4+0xb8,4))[0]==1<<20
assert cpu.mem_read(base+0xdf,1)==b'\0' and cpu.mem_read(base+0xe4,1)==b'\0'
first=snapshot();second=snapshot()
assert first==second
keys=s['keys']
for (row,col,lr,lc),value in zip(keys,[390,370,350,330]):
    cpu.mem_write(0x20002070+row*32+col*2,struct.pack('<H',value*8))
third=snapshot()
for (row,col,lr,lc),value in zip(keys,[390,370,350,330]):
    assert third[lr*22+lc]==value*8
enable(0)
cpu.mem_write(flags,bytes(4))
call(0x83b4,0x8ad0)
assert not struct.unpack('<I',cpu.mem_read(flags,4))[0] & (1<<24)
print('RAW_STREAM=PASS: six rows/132 raw ADC slots, repeated stationary snapshot, four independent values, disable, calibration flags unchanged.')
