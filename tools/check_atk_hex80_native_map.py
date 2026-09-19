"""Validate the native Hex80 matrix against locked official driver semantics."""
from pathlib import Path
import gzip, hashlib, json, re
import prepare_atk_hex80_layout as layout
ROOT=Path(__file__).resolve().parents[1]
def expected():
    for name,digest in layout.LOCKS.items():
        assert hashlib.sha256((layout.SOURCE/name).read_bytes()).hexdigest()==digest
    text=gzip.decompress((layout.SOURCE/'atk-index-M8vylvoC.js.gz').read_bytes()).decode()
    assert 'aUt={layer:4,row:6,col:17,' in text
    assert '"SIZE",32),Et(A,"baseOffset",1),A)' in text
    assert 'class e extends WFt{get size()' in text
    assert 'sendReport(t,e)' in text
    assert 'GNt.getAdcTripCompStatusBuffer(4*o,4)' in text
    assert 'const{position:[r,i]}=e' in text
    assert 'cBt.chunk(t.map(([e,t,n])=>({adc:e,currentValue:Number(t.toFixed(1)),isSuccess:!!n,isAutomaticCalibration:!1})),this.col)' in text
    data=json.loads((layout.SOURCE/'atk-hex80-demo.json').read_bytes())
    result=[0]*104;seen=set()
    for row in data['keyActions'][0]:
        for key in row:
            r,c=key['position'];assert 0<=r<6 and 0<=c<17
            slot=r*17+c;assert slot not in seen;seen.add(slot)
            code=key['defaultKey']
            if code==[0,168]:assert key['label']=='extraFunction.mute';continue
            assert code[0]==0 or code==[82,33]
            result[slot]=0x409 if code==[82,33] else code[1]
    assert len(seen)==88 and sum(bool(x) for x in result)==87
    assert len(set(result)-{0})==87
    return result

def main():
    text=(ROOT/'src/HallJoyProject/HallJoy/hex80_protocol.h').read_text()
    body=text.split('kSlotToHid{{',1)[1].split('}};',1)[0]
    body=re.sub(r'//[^\n]*','',body)
    values=[int(v.strip(),0) for v in body.split(',') if v.strip()]
    assert values==expected(),'Native Hex80 slot matrix differs from source-locked physical positions'
    report=layout.prepare()
    assert set(values)-{0}=={k['hid'] for k in report['keys']}
    print('ATK_NATIVE_MATRIX=PASS slots=104 physical_keys=87 fn=1 source_position_semantics=1')
if __name__=='__main__':main()
