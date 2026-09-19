"""Execute IPI stored-calibration read serializers with synthetic helper values."""
import hashlib, json, re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.local/ipi-reverse-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *
from audit_ipi_firmware import md, BASE, SOURCE
RAM=0x20000000; META=0x200d0000; PACKET=0x200d1000; TEMP=0x200d2000; OUTPUT=0x200e0000; STACK=0x200f0000

def run(row, ids):
    data=(SOURCE/row['file']).read_bytes()
    assert hashlib.sha256(data).hexdigest()==row['sha256']
    a=int(row['travel_handler'],16)
    table=next(x.address+4 for x in md.disasm(data[a-BASE:a-BASE+32],a) if x.mnemonic=='tbb')
    start=table+2*data[table-BASE+5]
    body=[]
    for x in md.disasm(data[start-BASE:start-BASE+512],start):
        body.append(x)
        if len(body)>2 and body[-2].mnemonic=='blo' and 'lsr #1' in body[-3].op_str: break
    calls=[int(x.op_str[1:],16) for x in body if x.mnemonic=='bl']
    assert len(calls)==3 and calls[0]==int(row['id_lookup'],16),(row['file'],calls)
    text='\n'.join(x.mnemonic+' '+x.op_str for x in body)
    names={'r'+str(i):globals()['UC_ARM_REG_R'+str(i)] for i in range(13)}
    names.update(sb=UC_ARM_REG_R9,sl=UC_ARM_REG_R10,fp=UC_ARM_REG_R11)
    regs={UC_ARM_REG_SP:STACK,UC_ARM_REG_R8:OUTPUT,UC_ARM_REG_R9:TEMP}
    packetreg=re.findall(r'add.w r0, (\w+), r[01], lsl #6',text)[-1]
    metareg=re.findall(r'ldrb r[01], \[(\w+), #7\]',text)[-1]
    for name,value in [(packetreg,PACKET),(metareg,META)]:
        alias=re.search(r'mov '+name+r', (\w+)\n',text)
        regs[names[alias[1] if alias else name]]=value
    tempreg=re.findall(r'strb(?:.w)? r1, \[(\w+), #1\]',text)[0]
    if tempreg!='r7': regs[names[tempreg]]=TEMP
    uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB)
    uc.mem_map(0x08000000,0x100000);uc.mem_write(BASE,data);uc.mem_map(RAM,0x100000)
    for r,v in regs.items():uc.reg_write(r,v)
    packet=bytearray(64);packet[:3]=bytes([9,0x94,5]);packet[4]=1;packet[6]=len(ids)*2
    for i,k in enumerate(ids):packet[7+i*2:9+i*2]=k.to_bytes(2,'big')
    packet[63]=(255-sum(packet[:63]))&255
    uc.mem_write(PACKET,bytes(packet));values=[];finished=[];addresses={x.address for x in body}
    def hook(u,pc,size,user):
        if pc in calls[1:]:
            matrix=u.reg_read(UC_ARM_REG_R0)
            value=(11000 if pc==calls[1] else 1700)+matrix
            values.append((pc,matrix,value));u.reg_write(UC_ARM_REG_R0,value)
            u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
        if pc not in addresses and not calls[0]<=pc<calls[0]+128:
            finished.append(pc);u.emu_stop()
    uc.hook_add(UC_HOOK_CODE,hook);uc.emu_start(start|1,0,count=30000)
    assert finished and len(values)==len(ids)*2,(row['file'],values)
    expected=b''
    for i,k in enumerate(ids):
        maximum,minimum=values[2*i:2*i+2]
        assert maximum[0]==calls[1] and minimum[0]==calls[2] and maximum[1]==minimum[1]
        expected+=k.to_bytes(2,'big')+maximum[2].to_bytes(2,'big')+minimum[2].to_bytes(2,'big')
    assert bytes(uc.mem_read(RAM,0x100000)).find(expected)>=0,row['file']
    return dict(ids=ids,records_hex=expected.hex(),handler=hex(start),max_getter=hex(calls[1]),min_getter=hex(calls[2]))

def main():
    rows=json.loads((SOURCE/'firmware-audit.json').read_bytes())
    results=[dict(file=r['file'],sha256=r['sha256'],cases=[run(r,r['physical_ids'][i:i+9]) for i in range(0,len(r['physical_ids']),9)]) for r in rows]
    blob=(json.dumps(results,indent=2)+'\n').encode();p=SOURCE/'calibration-record-emulation.json'
    if p.exists():assert p.read_bytes()==blob
    else:
        with p.open('xb') as f:f.write(blob)
    print('IPI_CALIBRATION_RECORDS=PASS images='+str(len(rows))+' batches='+str(sum(len(r['cases']) for r in results)))
    print('Scope: real ID lookup and calibration serializer; getter values synthetic; no USB or calibration writes.')
if __name__=='__main__':main()
