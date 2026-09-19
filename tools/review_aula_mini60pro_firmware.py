"""Pinned MINI60 HE Pro V1.55 reporting-tail checks; no device I/O."""
from pathlib import Path
import sys,hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/ipi-reverse-deps'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_HOOK_CODE
from unicorn.arm_const import *
IMAGE=ROOT/'.local/aula-mini60-pro-155-resource-4000.bin'
HASH='c070e514ff1bef20a71abff12a6c30b04f152892b0fa22e1b5e0709be63eb7cd'

class Machine:
    def __init__(self):
        data=IMAGE.read_bytes();assert hashlib.sha256(data).hexdigest()==HASH
        self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
        self.u.mem_map(0,0x80000);self.u.mem_map(0x20000000,0x10000);self.u.mem_write(0,data)
        self.stop=0x18000;self.sent=[];self.usb_status=0;self.normal=False
        self.u.hook_add(UC_HOOK_CODE,self.hook)
    def hook(self,u,a,size,data):
        if a==self.stop:u.emu_stop()
        if a==0x111b8:
            self.sent.append(bytes(u.mem_read(u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2))))
            u.reg_write(UC_ARM_REG_R0,self.usb_status);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
        if a==0x521c:self.normal=True;u.emu_stop()
    def byte(self,a,v):self.u.mem_write(a,bytes([v]))
    def half(self,a,v):self.u.mem_write(a,struct.pack('<H',v))
    def run(self,a):
        self.u.reg_write(UC_ARM_REG_SP,0x2000f000);self.u.reg_write(UC_ARM_REG_LR,0x18001)
        self.u.emu_start(a|1,0x18000,count=100000)
        assert self.u.reg_read(UC_ARM_REG_PC) in (self.stop,0x521c)
    def command(self,cmd):
        self.stop=0x18000;self.u.mem_write(0x20005aa8,bytes([0xaa,cmd])+bytes(62));self.byte(0x20000477,1);self.run(0xe344)
    def report(self,index,key,adc,travel):
        self.stop=0x2db6
        for reg,v in [(UC_ARM_REG_R0,index),(UC_ARM_REG_R2,travel),(UC_ARM_REG_R3,0),(UC_ARM_REG_R4,0x200000e4),(UC_ARM_REG_R5,0x20000474),(UC_ARM_REG_R7,key),(UC_ARM_REG_R8,0x200030b2)]:self.u.reg_write(reg,v)
        self.half(0x20002fb2+index*2,2200);self.half(0x200030b2+index*2,1900);self.half(0x20003672+index*2,adc)
        before=len(self.sent);self.run(0x2d74);return self.sent[before:]

def main():
    m=Machine();rows=[]
    for cmd,expected in [(0x66,1),(0x67,0),(0x60,0),(0x68,0),(0x66,1)]:
        m.command(cmd);assert m.u.mem_read(0x20000475,1)[0]==expected
        assert m.u.mem_read(0x20000474,1)[0]==0
        rows.append({'case':'command','command':hex(cmd),'simulation':expected,'calibration':0,'result':'PASS'})
    for index,key,adc,travel,expected in [(0,7,2050,170,True),(1,16,2100,93,True),(0,7,2150,43,True),(0,7,2188,1,True),(0,7,2189,0,False),(0,7,2200,0,False)]:
        packets=m.report(index,key,adc,travel);assert bool(packets)==expected
        if expected:
            p=packets[0];assert len(p)==64 and p[:3]==bytes([0x55,0xfb,key])
            assert int.from_bytes(p[8:10],'little')==adc and int.from_bytes(p[10:12],'little')==travel
        rows.append({'case':'reporting tail','key':key,'adc':adc,'supplied_travel':travel,'send_attempts':len(packets),'result':'PASS'})
    m.usb_status=2;assert len(m.report(1,16,2100,93))==1
    rows.append({'case':'USB busy result returns from reporting tail without retry','result':'PASS'})
    m.stop=0x521c;m.byte(0x200003e5,1);m.byte(0x20000083,1);m.u.reg_write(UC_ARM_REG_R5,0x20000474);m.run(0x2e06)
    assert m.normal
    rows.append({'case':'calibration-off continuation reaches normal processing with simulation enabled','result':'PASS'})
    print(json.dumps({'image_sha256':HASH,'scope':'Real dispatcher and reporting-tail code; synthetic already-computed travel, endpoints and register state. USB send is a stub. Normal processing entry only. No full scanner, ADC, timing, radio, or physical USB validation.','checks':rows,'status':'PASS'},indent=2))
if __name__=='__main__':main()
