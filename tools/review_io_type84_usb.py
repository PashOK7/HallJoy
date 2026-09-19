"""Pinned IO Type84 USB class component tests; no physical USB or firmware writes."""
import json
import struct
import review_io_type84_firmware as fw
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm_const import *


class UsbMachine(fw.Machine):
    def __init__(self,color):
        super().__init__(color)
        h=fw.load(color)
        for address in (0x15c28,0x15c38):
            args=struct.unpack('<IIII',bytes(h.tobinarray(start=address,end=address+15)))
            self.call(args[3],*args[:3])
        self.call(0x1076c,0x20001168)
        self.callbacks=[self.word(0x200066a0+i) for i in (0x80,0x84,0x88)]
        assert self.callbacks==[0,0x12c95,0x1376d]
        self.call(0x10608,0x200003e8)
        self.u.mem_write(0x200003f8,struct.pack('<I',0x20006f38))
        # Synthetic enumerated interface state; sizes come from initialized config.
        for n in range(4):
            address=0x20006730+0xc0+n*328
            self.b(address,n);self.b(address+1,n)
            self.h(address+0xf2,self.rh(0x200066a4+n*24+8))
        self.transfers=[];self.stalls=0;self.incoming=bytes(64);self.depth_reads=[]
        self.u.hook_add(UC_HOOK_CODE,self.usb_hook)
        self.u.hook_add(UC_HOOK_MEM_READ,self.read_hook)
    def word(self,address):return int.from_bytes(self.u.mem_read(address,4),'little')
    def read_hook(self,u,access,address,size,value,data):
        if 0x2000313a<=address<0x2000323a or 0x2000343a<=address<0x2000353a:
            self.depth_reads.append(address)
    def usb_hook(self,u,address,size,data):
        if address==0xfb28:
            self.transfers.append(bytes(u.mem_read(u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2))))
        elif address==0xfad4:
            length=u.reg_read(UC_ARM_REG_R2)
            u.mem_write(u.reg_read(UC_ARM_REG_R1),self.incoming[:length])
        elif address==0xfacc:self.stalls+=1
        else:return
        u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    def setup(self,interface,report_id,report_type=3,write=False,vendor=False):
        request=struct.pack('<BBHHH',0x40 if vendor else (0x21 if write else 0xa1),9 if write else 1,(report_type<<8)|report_id,interface,64)
        self.u.mem_write(0x2000c000,request)
        self.call(0x10864,0x200003e8,0x2000c000)
        return self.u.reg_read(UC_ARM_REG_R0)


def main():
    rows=[]
    for color in fw.HASHES:
        m=UsbMachine(color);pattern=bytes(range(64));m.u.mem_write(0x200062f8,pattern)
        accepted=[];count=0
        for interface in range(4):
            for report_type in (1,2,3):
                for report_id in range(256):
                    before=len(m.transfers);status=m.setup(interface,report_id,report_type)
                    expected=interface in (0,3) and report_type==3 and report_id in (0,2)
                    assert status==(0 if expected else 1),(interface,report_type,report_id,status)
                    assert len(m.transfers)==before+int(expected)
                    if expected:
                        assert m.transfers[-1]==pattern
                        accepted.append([interface,report_type,report_id])
                    count+=1
        assert not m.depth_reads
        rows.append({'color':color,'case':'GET_REPORT class dispatch','requests':count,'accepted_interface_type_id':accepted,'live_depth_reads':0,'callbacks':[hex(x) for x in m.callbacks],'result':'PASS'})
        for fill in (0x11,0xee):
            m.u.mem_write(0x2000313a,bytes([fill])*256);m.u.mem_write(0x2000343a,bytes([fill])*256)
            assert m.setup(0,0)==0 and m.transfers[-1]==pattern
        m.incoming=bytes([0xaa,0x66])+bytes(range(2,64))
        assert m.setup(0,0,write=True)==0
        m.call(0x1032c,0x200003e8)
        assert bytes(m.u.mem_read(0x200062f8,64))==m.incoming
        assert m.rb(0x2000036d)==0
        assert m.setup(0,0)==0 and m.transfers[-1]==m.incoming
        assert not m.depth_reads
        rows.append({'color':color,'case':'feature SET then GET echoes payload without starting simulation; independent of live depths','result':'PASS'})
        initialized=bytes(m.u.mem_read(0x20000000,0x1200))
        pointers=[]
        for offset in range(len(initialized)-3):
            value=int.from_bytes(initialized[offset:offset+4],'little')
            if 0x200044c2<=value<0x20004582 or 0x2000022e<=value<0x20000232:
                pointers.append([offset,value])
        assert not pointers
        rows.append({'color':color,'case':'no pointers to extra transmit buffers or flags in initialized data','result':'PASS'})
        scanner=UsbMachine(color);scanner.command(0x66);scanner.b(0x200002b8,1)
        for index in range(128):
            scanner.h(0x2000353a+index*2,2200);scanner.h(0x2000363a+index*2,1900)
        trace=[]
        for index,adc,travel,keys in [(0,2100,93,[0,0]),(1,2000,167,[16,16]),(0,2150,43,[]),(1,2000,167,[16,16])]:
            before=len(scanner.calls)
            assert scanner.scan(index,adc)==travel and scanner.calls[before:]==keys
            trace.append({'index':index,'travel':travel,'serialized_keys':keys})
        rows.append({'color':color,'case':'selected-key counterexample survives firmware RAM initialization','trace':trace,'result':'PASS'})
        assert m.setup(0,0,vendor=True)==1
        rows.append({'color':color,'case':'vendor-type setup rejected by this HID class handler','result':'PASS'})
    print(json.dumps({'scope':'Original RAM scatter initialization and USB class code; synthetic enumeration state. Control send, receive and stall helpers are stubs. No hardware or USB timing test.','checks':rows,'status':'PASS'},indent=2))

if __name__=='__main__':main()
