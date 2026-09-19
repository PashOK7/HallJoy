"""Offline checks of IO V1.17 command parameters and the application RX bridge."""
import json
import review_io_type84_firmware as fw
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R3, UC_ARM_REG_PC, UC_ARM_REG_LR


def dispatch(machine, request):
    assert len(request)==64
    machine.u.mem_write(0x2000637c,request)
    machine.b(0x2000036f,1)
    machine.call(0xd7c4)
    return bytes(machine.u.mem_read(0x2000633c,64))


def main():
    results=[]
    for color in fw.HASHES:
        m=fw.Machine(color)
        count=0
        for position in range(2,64):
            request=bytearray(64);request[:2]=bytes([0xaa,0x66]);request[position]=255
            m.b(0x200000ff,7);m.h(0x2000010a,167)
            dispatch(m,bytes(request))
            assert m.rb(0x200000ff)==7 and m.rh(0x2000010a)==167 and m.rb(0x2000036d)==1
            count+=1
        results.append({'color':color,'case':'66 payload position variations preserve selected key and travel','variations':count,'result':'PASS'})
        bases={0x11:0x9200,0x12:0x9600,0x14:0x9a00,0x15:0x9c00,0x16:0xb000,0x17:0xb600,0x18:0xb200,0x1c:0xbc00}
        for command,base in bases.items():
            replies=[]
            for fill in (0x11,0xee):
                m.u.mem_write(0x2000313a,bytes([fill])*0x100)
                m.u.mem_write(0x2000343a,bytes([fill])*0x100)
                request=bytes([0xaa,command,4,0,0])+bytes(59)
                replies.append(dispatch(m,request))
            assert replies[0]==replies[1]
            assert replies[0][8:12]==bytes(m.u.mem_read(base,4))
            results.append({'color':color,'case':'configuration read unaffected by live depths','command':hex(command),'base':hex(base),'result':'PASS'})
        m=fw.Machine(color);channels=[]
        def receive(u,address,size,data):
            if address!=0x1062c:return
            channel=u.reg_read(UC_ARM_REG_R0);channels.append(channel)
            payload=bytes([2])+bytes(63) if channel==0 else bytes([0xaa,0x66])+bytes(62)
            u.mem_write(u.reg_read(UC_ARM_REG_R1),payload)
            u.mem_write(u.reg_read(UC_ARM_REG_R3),(64).to_bytes(4,'little'))
            u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
        m.u.hook_add(UC_HOOK_CODE,receive)
        m.call(0x12fe8)
        assert channels==[0,2] and m.rb(0x200062e4)==2 and m.rb(0x2000036f)==1
        m.call(0xd7c4);assert m.rb(0x2000036d)==1
        results.append({'color':color,'case':'application RX bridge feeds vendor dispatcher','channels':channels,'result':'PASS'})
    output={'scope':'Pinned V1.17 components only; receive helper is stubbed. Not a USB control-transfer or whole-firmware reachability proof. Command 10 excluded because its reader has a conditional flash-write side effect.','checks':results,'status':'PASS'}
    print(json.dumps(output,indent=2))

if __name__=='__main__':main()
