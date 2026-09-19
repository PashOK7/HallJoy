"""Pinned IO Type84 firmware component review; synthetic RAM, no device I/O."""
from pathlib import Path
import sys, hashlib, json, struct, argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/ipi-reverse-deps'))
from intelhex import IntelHex
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_OP_MEM
DIR=ROOT/'docs/firmware/io-type84-magnetic'
HASHES={'Black':'4f2e8b8b406a72ed4a4d4b34502478aa9b8b5d874c4a3cdfa1381acaea00ec44','White':'8f5ef507771c6795258eb7521cfc1b46269a5b301548767ae87a3f1a83a50d45'}
def load(color):
    p=DIR/f'IO_Type_84_Magnetic_{color}_V1.17.hex'
    assert hashlib.sha256(p.read_bytes()).hexdigest()==HASHES[color]
    return IntelHex(str(p))
class Machine:
    def __init__(self,color='Black'):
        h=load(color);self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
        self.u.mem_map(0,0x80000);self.u.mem_map(0x20000000,0x10000)
        self.u.mem_write(0,bytes(h.tobinarray(start=0,end=0x16263)))
        self.u.reg_write(UC_ARM_REG_FPEXC,0x40000000)
        self.sent=[];self.calls=[];self.normal=[]
        self.u.hook_add(UC_HOOK_CODE,self.hook)
    def b(self,a,v):self.u.mem_write(a,bytes([v]))
    def h(self,a,v):self.u.mem_write(a,struct.pack('<H',v))
    def rb(self,a):return self.u.mem_read(a,1)[0]
    def rh(self,a):return int.from_bytes(self.u.mem_read(a,2),'little')
    def hook(self,u,a,size,data):
        if a==0x18000:u.emu_stop()
        if a==0xbf0:self.calls.append(u.reg_read(UC_ARM_REG_R0))
        if a==0x107dc:
            self.sent.append(bytes(u.mem_read(u.reg_read(UC_ARM_REG_R1),u.reg_read(UC_ARM_REG_R2))))
            u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
        # Analog input filter is replaced by identity; no ADC or USB is emulated.
        if a==0x6c14:u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
        if a==0x43fc:
            self.normal.append(a);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
    def call(self,a,*args):
        u=self.u;u.reg_write(UC_ARM_REG_SP,0x2000f000);u.reg_write(UC_ARM_REG_LR,0x18001)
        for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):u.reg_write(reg,value)
        for i,value in enumerate(args[4:]):u.mem_write(0x2000f000+i*4,struct.pack('<I',value))
        u.emu_start(a|1,0x18000,count=200000)
        assert u.reg_read(UC_ARM_REG_PC)==0x18000,hex(u.reg_read(UC_ARM_REG_PC))
    def command(self,cmd):
        self.u.mem_write(0x2000637c,bytes([0xaa,cmd])+bytes(62));self.b(0x2000036f,1)
        self.call(0xd7c4)
    def scan(self,index,adc):
        self.h(0x2000222a+(index%8)*32+(index//8)*2,adc)
        self.call(0x3aa8,index%8,index//8)
        return self.rh(0x2000313a+index*2)
def main():
    rows=[]
    black,white=load('Black'),load('White')
    assert black.addresses()==white.addresses()
    differences=[a for a in black.addresses() if black[a]!=white[a]]
    assert differences==list(range(0x10ba8,0x10bad))
    for color in HASHES:
        for calibration in (0,1):
            m=Machine(color);m.b(0x2000036c,calibration)
            for cmd,expected in [(0x66,1),(0x67,0),(0x60,0),(0x68,0)]:
                m.command(cmd)
                assert m.rb(0x2000036d)==expected
                assert m.rb(0x2000036c)==calibration
                assert bytes(m.u.mem_read(0x2000633c,64))==bytes([0x55,cmd])+bytes(62)
                rows.append({'color':color,'command':hex(cmd),'test_flag':expected,'calibration_flag':calibration,'result':'PASS'})
        m=Machine(color)
        m.call(0xbf0,7,2200,0x876c,2050,170,34)
        packet=bytes(m.u.mem_read(0x20004482,64))
        assert packet[:14]==bytes.fromhex('55fb070198086c870208aa002200') and packet[14:]==bytes(50)
        m.b(0x20000101,1);m.call(0x12620)
        assert m.sent==[packet] and m.rb(0x20000101)==0
        rows.append({'color':color,'case':'serializer and scheduler','bytes':64,'prefix':packet[:14].hex(),'result':'PASS'})
        m.sent.clear()
        m.call(0xbf0,7,2200,1900,2050,170,34);m.b(0x20000101,1)
        m.call(0xbf0,16,2200,1900,2000,167,34)
        latest=bytes(m.u.mem_read(0x20004482,64));m.call(0x12620)
        assert m.sent==[latest] and latest[2]==16
        rows.append({'color':color,'case':'pending mailbox overwritten before send','received_keys':[x[2] for x in m.sent],'result':'PASS'})
        m=Machine(color);m.command(0x66);m.b(0x200002b8,1)
        for i in range(128):
            m.h(0x2000353a+i*2,2200);m.h(0x2000363a+i*2,1900)
        trace=[]
        for index,adc,expected in [(0,2100,[0,0]),(1,2000,[16,16]),(0,2150,[]),(1,2000,[16,16])]:
            before=len(m.calls);normal_before=len(m.normal);travel=m.scan(index,adc)
            serialized=m.calls[before:]
            assert serialized==expected and len(m.normal)>normal_before
            trace.append({'matrix_index':index,'adc':adc,'travel':travel,'selected_index':m.rb(0x200000ff),'serialized_keys':serialized})
        assert trace[0]['travel']==93 and trace[2]['travel']==43
        m.command(0x67);before=len(m.calls);normal_before=len(m.normal);m.scan(1,2000)
        assert len(m.calls)==before and len(m.normal)>normal_before
        rows.append({'color':color,'case':'weaker key depth changes without serialization; normal path reached on and off','trace':trace,'result':'PASS'})
    result={'firmware_sha256':HASHES,'mapped_difference_addresses':[hex(x) for x in differences],
            'scope':'Synthetic component execution. ADC filter is identity; USB send and normal-key processing are stubs. No physical keyboard, timing or full typing test.',
            'checks':rows,'status':'PASS'}
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path);args=parser.parse_args()
    output=json.dumps(result,indent=2)+'\n'
    if args.output:
        with args.output.open('x',encoding='utf-8',newline='\n') as stream:stream.write(output)
    print(output)
if __name__=='__main__':main()
