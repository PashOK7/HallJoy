"""Compare extracted vendor JS packet generation with HallJoy C++ (offline, no HID)."""
from pathlib import Path
import json, subprocess, tempfile, shutil, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from run_native_backend_checks import find_cxx, compile_and_run
JS=ROOT/'.local/research/attackshark-pro/desktop/resources/app/dist/js'
index=(JS/'index.2e5bd916.js').read_text(encoding='utf-8')
base=(JS/'f9b6af43.js').read_text(encoding='utf-8')
def field(text, start, end):
 return text.split(start,1)[1].split(end,1)[0]
encoder=field(index[index.index('class Tb '):],'___encodeCmd=',';sendMsg=')
pages=field(base,'_getMulitMagnetismCMD=',';_decodeMultiMagnetismTwoByteList=')
decode=field(base,'_decodeMultiMagnetismTwoByteList=',';getMultiMagnetismInfo=')
# Execute only the three reviewed pure/transport-injected methods, never the bundle.
program="""const Wt={Bit7:0,Bit8:1};const h=Wt;const Io=a=>a.reduce((x,y)=>x+y,0);
class Vendor { CONNECT='usb'; FEA_CMD_GET_MULTI_MAGNETISM=229; SEND_TIME=10; READ_TIME=10;
encode=ENCODER;pages=PAGES;decode=DECODE; frames=[];
async commonMsg(a,b,c,d){this.frames.push({payload:Array.from(this.encode(a,b)),send:c,read:d});return new Uint8Array(64);}}
(async()=>{const v=new Vendor();await v.pages(254,4,1,1);
for(const cmd of [143,128])v.frames.push({payload:Array.from(v.encode(new Uint8Array([cmd]),0))});
const values=v.decode(new Uint8Array([0,0,1,0,255,0,0,1,255,15,0,16]));
process.stdout.write(JSON.stringify({frames:v.frames,values}));})();""".replace('ENCODER',encoder).replace('PAGES',pages).replace('DECODE',decode)
vendor=json.loads(subprocess.check_output([shutil.which('node'),'-e',program],text=True))
assert vendor['values']==[0,1,255,256,4095,4096]
assert all(f['send']==f['read']==1 for f in vendor['frames'][:4])
cpp=r'''#include "attackshark_pro_diagnostic_model.h"
#include <cstdio>
#include <cassert>
using namespace halljoy::sharkdiag;
int main(){
for(unsigned i=0;i<6;++i){auto r=Request(i<4?0xe5:i==4?0x8f:0x80,i<4?i:0);
 for(auto b:r){std::printf("%02x",b);}std::puts("");}
Report r{};r[1]=0x8f;r[2]=0x7a;r[3]=0x0b;assert(Identity(r)==2938);
r.fill(0);unsigned values[]={0,1,255,256,4095,4096};
for(unsigned i=0;i<6;++i){r[1+i*2]=values[i]&255;r[2+i*2]=values[i]>>8;}
std::array<unsigned,32> out{};assert(Decode(r,out));for(unsigned i=0;i<6;++i)assert(out[i]==values[i]);
}
'''
with tempfile.TemporaryDirectory(prefix='shark-wire-',dir=ROOT/'build/obj') as folder:
 p=Path(folder);(p/'check.cpp').write_text(cpp,encoding='utf-8');exe=p/'check.exe'
 compile_and_run(find_cxx(),exe,[p/'check.cpp'],ROOT/'src/HallJoyProject/HallJoy')
 lines=subprocess.check_output([str(exe)],text=True).splitlines()
 for frame,line in zip(vendor['frames'],lines,strict=True):
  expected=bytes([0]+frame['payload']+[0]*(64-len(frame['payload'])))
  assert bytes.fromhex(line)==expected
for pid in [20521,20525,20527,20528]:assert f'vendorId:12625,productId:{pid},usage:2,usagePage:65535,interfaceNumber:2' in index
assert 'return e[8]<<8|e[7]' in base
assert 'const n=e[2]<<8|e[1]' in base
assert 'getUint32(1,!0)' in index
print('ATTACKSHARK_WIRE_REVIEW=PASS exact_request_bytes=6 le16_decoder=1 identity_offset=1 vendor_delays=1 feature_usage=2 hardware_access=0')
