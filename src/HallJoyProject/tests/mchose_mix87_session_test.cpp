// Production worker session with fake transport and hash-provider seam; no USB access.
// Fingerprint matching itself is separately checked against the pinned private image.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <bcrypt.h>
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstring>
static std::array<unsigned char,64> req{};
static std::array<unsigned char,8192> settings{};
static std::array<unsigned char,8> base{0,1,0,0,0,0,0,0};
static bool pending=false;static unsigned writes=0,stream=0,hashIndex=0,scenario=0;
void StreamStep(unsigned char*);void StopTest();
static HANDLE FakeCreate(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){return CreateEventW(nullptr,TRUE,FALSE,nullptr);}
static BOOL FakeWrite(HANDLE,LPCVOID buf,DWORD n,LPDWORD,LPOVERLAPPED o){
    assert(n==65);const auto* p=static_cast<const unsigned char*>(buf);assert(p[0]==0);
    std::memcpy(req.data(),p+1,64);o->InternalHigh=n;pending=true;
    assert(req[1]==3 || req[1]==0xe0 || req[1]==6);
    if(req[1]==6){
        ++writes;assert(req[4]==1 && req[5]==7);
        if(scenario==8 && !(req[8]&8)){pending=false;SetLastError(ERROR_DEVICE_NOT_CONNECTED);return FALSE;}
        settings[7]=req[8];
        if(scenario==7 && (req[8]&8))StopTest();
    }
    return TRUE;
}
static BOOL FakeResult(HANDLE,LPOVERLAPPED o,LPDWORD n,BOOL){*n=static_cast<DWORD>(o->InternalHigh);return TRUE;}
static BOOL FakeRead(HANDLE,LPVOID buf,DWORD n,LPDWORD,LPOVERLAPPED o){
    assert(n==65);auto* wire=static_cast<unsigned char*>(buf);std::memset(wire,0,n);o->InternalHigh=n;auto* p=wire+1;
    if(!pending){StreamStep(p);if(!p[0]){SetLastError(WAIT_TIMEOUT);return FALSE;}return TRUE;}
    pending=false;std::memcpy(p,req.data(),64);p[0]=0xaa;
    if(req[1]==3){p[8]=scenario==2?0x23:scenario==10?0x14:0x22;p[9]=1;}
    if(req[1]==0xe0){
        const unsigned address=req[5]|req[6]<<8|req[7]<<16;
        for(unsigned i=0;i<req[4];++i){
            const auto at=address+i;
            if(at>=0x2a000 && at<0x2a008)p[8+i]=base[at-0x2a000];
            else if(at>=0x2c000 && at<0x2e000)p[8+i]=settings[at-0x2c000];
            else if(at>=0x15da6 && at<0x15da6+276){
                const unsigned j=at-0x15da6,key=j/3,part=j%3;
                p[8+i]=key<78?(part==0?16:part==2?static_cast<unsigned char>(key+4):0):
                    key<86?(part==0?16:part==1?static_cast<unsigned char>(1u<<(key-78)):0):0;
            }
        }
    }
    unsigned sum=p[4];for(unsigned i=5;i<8u+p[4];++i)sum+=p[i];p[3]=static_cast<unsigned char>(sum);
    if(req[1]==6 && scenario==4 && (req[8]&8))p[0]=0xab; // write happened, acknowledgement lost/rejected
    return TRUE;
}
static NTSTATUS FakeHash(BCRYPT_ALG_HANDLE,PUCHAR,ULONG,PUCHAR,ULONG,PUCHAR,ULONG);
#define CreateFileW FakeCreate
#define WriteFile FakeWrite
#define ReadFile FakeRead
#define GetOverlappedResult FakeResult
#define BCryptHash FakeHash
#include "../HallJoy/mchose_mix87_backend.cpp"
#undef CreateFileW
#undef WriteFile
#undef ReadFile
#undef GetOverlappedResult
#undef BCryptHash
static NTSTATUS FakeHash(BCRYPT_ALG_HANDLE,PUCHAR,ULONG,PUCHAR,ULONG,PUCHAR out,ULONG n){
    assert(n==32 && hashIndex<std::size(fingerprints));
    const char* hex=fingerprints[hashIndex++].digest;
    auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(unsigned i=0;i<32;++i)out[i]=static_cast<unsigned char>(digit(hex[2*i])*16+digit(hex[2*i+1]));
    if(scenario==3)out[0]^=1;return 0;
}
void StreamStep(unsigned char* p){
    ++stream;
    assert(Connected()); // live before the first key report (idle keyboard)
    if(stream==1 && scenario==5){StopTest();return;}
    if(stream==1 && scenario==6){p[0]=0xa0;p[1]=16;p[3]=26;p[6]=0xff;return;}
    if(stream==1 && scenario==9){base[2]=1;StopTest();return;}
    p[0]=0xa0;p[1]=16;p[3]=26;p[14]=1;p[15]=0x55;
    if(stream==1){p[6]=1;p[7]=0x55;Notify();} // unrelated device change mid-session
    if(stream==2)assert(Connected() && Get(26)==1000);
    if(stream==2 && scenario==10){
        // Ace 75 8K: model keys, identity and layout token; no fingerprint hashing.
        assert(Owns(0x4d) && Owns(0x4a) && !Owns(0x49) && !Owns(0x46) && hashIndex==0);
        NativeAnalogBackendTelemetry t{};Telemetry(&t);
        assert(t.connected && t.vendorId==0x3837 && t.productId==0x303c && t.mappedKeys==80);
        assert(!wcscmp(t.deviceName,L"MCHOSE Ace 75 8K"));
        assert(t.verifiedLayoutToken && t.verifiedLayoutToken==halljoy::layout_identity::Token("mchose-ace75","ACE758K-3837-303C"));
    }
    if(stream==2 && scenario==2){
        // Mix87 III on another firmware version: factory key list, no hashing.
        assert(Owns(0x65) && Owns(0x48) && !Owns(0x32) && hashIndex==0);
        NativeAnalogBackendTelemetry t{};Telemetry(&t);
        assert(t.connected && t.productId==0x300d && t.mappedKeys==86);
        assert(t.verifiedLayoutToken==halljoy::layout_identity::Token("mchose-mix87","MIX87III-3837-300D"));
    }
    if(stream==2 && scenario==0){
        NativeAnalogBackendTelemetry t{};Telemetry(&t);
        assert(t.productId==0x300d && t.mappedKeys==86 && !wcscmp(t.deviceName,L"MCHOSE Mix87 III"));
    }
    if(stream==3){assert(Get(26)==0);StopTest();}
}
void StopTest(){g_stop.store(true);}
void SupportLog_Event(const char*,std::uint64_t,SupportLogDetail) noexcept{}
namespace halljoy::keyboard_support {void ReportCommunicationAnomaly(unsigned) noexcept{}}
bool NativeAnalogRouting_Claim(std::uint16_t,std::uint16_t,const wchar_t*,NativeAnalogProtocol){return true;}
bool NativeAnalogRouting_IsClaimedBy(const wchar_t*,NativeAnalogProtocol){return false;}
int main(){
    for(scenario=0;scenario<10;++scenario){
        pending=false;writes=stream=hashIndex=0;g_autoAttempted=false;
        base={0,1,0,0,0,0,0,0};settings.fill(0xff);std::fill_n(settings.begin(),256,0);
        settings[7]=scenario==0?8:0;settings[8]=0x55;
        g_stop.store(false);Candidate candidate{L"synthetic"};Run(candidate);
        assert(!Connected() && Get(26)==0 && settings[8]==0x55);
        if(scenario==0 || scenario==1 || scenario==2 || scenario==8)assert(stream==3); // survived the device change
        if(scenario==3){assert(writes==0);}
        else if(scenario==0){assert(writes==1 && !(settings[7]&8));}
        else if(scenario==9){assert(writes==1 && (settings[7]&8));}
        else {assert(writes==2);assert(bool(settings[7]&8)==(scenario==8));}
        if(scenario==2)assert(hashIndex==0);
        if(scenario==3)assert(hashIndex==1);
        auto stale=MchoseMix87_GetMode();assert(!MchoseMix87_RequestMode(stale,true));
        if(scenario==1){
            // Recovery is allowed to read, never repeatedly cycle flash.
            pending=false;writes=stream=hashIndex=0;g_stop.store(false);Run(candidate);
            assert(writes==0 && !(settings[7]&8));
        }
    }
    {   // Ace 75 8K (non-fingerprinted ARM model): any version, same lifecycle.
        scenario=10;pending=false;writes=stream=hashIndex=0;g_autoAttempted=false;
        base={0,1,0,0,0,0,0,0};settings.fill(0xff);std::fill_n(settings.begin(),256,0);settings[8]=0x55;
        g_stop.store(false);Candidate candidate{L"synthetic",halljoy::mix87::FindModel(0x3837,0x303c)};Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3 && hashIndex==0);
        g_model.store(&halljoy::mix87::Models[0]);
    }
    { Session session(Candidate{L"synthetic"});std::uint8_t value=0;
      pending=false;assert(!session.ReadBytes(0x2a000,&value,1,true,GetTickCount64()));
      assert(GetLastError()==WAIT_TIMEOUT && !pending); }
    puts("Mix87 lifecycle PASS: auto-enable, pre-enabled no redundant enable, normal/early stop cleanup, ambiguous enable cleanup, other version admitted, bad hash, stream rejection, stop during write, disconnect, profile race, no flash retry loop, Ace 75 8K model");
}
