// Production worker session with a fake Jet 75 II transport; no USB access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstring>
static std::array<unsigned char,64> req{};
static std::array<unsigned char,256> settings{};
static std::array<unsigned char,6> base{0,1,0,0,0,0};
static bool pending=false,rebooting=false;static unsigned writes=0,stream=0,scenario=0;
void StreamStep(unsigned char*);void StopTest();
static HANDLE FakeCreate(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){return CreateEventW(nullptr,TRUE,FALSE,nullptr);}
static BOOL FakeWrite(HANDLE,LPCVOID buf,DWORD n,LPDWORD,LPOVERLAPPED o){
    assert(n==65);const auto* p=static_cast<const unsigned char*>(buf);assert(p[0]==0);
    std::memcpy(req.data(),p+1,64);o->InternalHigh=n;pending=true;
    assert(req[1]==3 || req[1]==4 || req[1]==5 || req[1]==6);
    if(req[1]==6){
        ++writes;assert(req[4]==1 && req[5]==(scenario==15?2*64+7:7) && req[6]==0);
        if(scenario==8 && !(req[8]&8)){pending=false;SetLastError(ERROR_DEVICE_NOT_CONNECTED);return FALSE;}
        settings[req[5]]=req[8];
        if((settings[0]&1) && scenario!=14){pending=false;rebooting=true;} // the keyboard reboots: no reply, no reports
        if(scenario==7 && (req[8]&8))StopTest();
    }
    return TRUE;
}
static BOOL FakeResult(HANDLE,LPOVERLAPPED o,LPDWORD n,BOOL){*n=static_cast<DWORD>(o->InternalHigh);return TRUE;}
static BOOL FakeRead(HANDLE,LPVOID buf,DWORD n,LPDWORD,LPOVERLAPPED o){
    assert(n==65);auto* wire=static_cast<unsigned char*>(buf);std::memset(wire,0,n);o->InternalHigh=n;auto* p=wire+1;
    if(!pending){if(!rebooting)StreamStep(p);if(!p[0]){SetLastError(WAIT_TIMEOUT);return FALSE;}return TRUE;}
    pending=false;std::memcpy(p,req.data(),64);p[0]=0xaa;
    if(req[1]==3 && scenario==13){p[8]=0x21;p[9]=1;std::memcpy(p+10,"Nov  8 2025,15:59:23",20);}
    else if(req[1]==3){p[8]=0x16;p[9]=scenario==11?2:1;std::memcpy(p+10,"Aug 28 2025,10:45:06",20);}
    if(req[1]==4)for(unsigned i=0;i<req[4];++i)p[8+i]=req[5]+i<6?base[req[5]+i]:0;
    if(req[1]==5)for(unsigned i=0;i<req[4];++i)p[8+i]=settings[req[5]+i];
    unsigned sum=p[4];for(unsigned i=5;i<8u+p[4];++i)sum+=p[i];p[3]=static_cast<unsigned char>(sum);
    if(req[1]==6 && scenario==4 && (req[8]&8))p[0]=0xab; // write happened, acknowledgement lost/rejected
    if(req[1]==3 && scenario==2)p[0]=0xab;              // not the M HUB protocol: no admission
    if(req[1]==5 && scenario==12)p[8+2]=0;              // foreign settings layout (profile 0 magic)
    return TRUE;
}
#define CreateFileW FakeCreate
#define WriteFile FakeWrite
#define ReadFile FakeRead
#define GetOverlappedResult FakeResult
#include "../HallJoy/mchose_jet75_backend.cpp"
#undef CreateFileW
#undef WriteFile
#undef ReadFile
#undef GetOverlappedResult
void StreamStep(unsigned char* p){
    ++stream;
    assert(Connected()); // live before the first key report (idle keyboard)
    if(scenario==16){
        // A key outside the reviewed set is ignored; the session and later keys continue.
        p[0]=0xa0;p[1]=16;p[14]=1;p[15]=0x55;
        if(stream==1){p[3]=0x48;p[7]=10;return;}
        if(stream==2){p[3]=26;p[6]=1;p[7]=0x55;return;}
        assert(Get(26)==1000 && !Owns(0x48));StopTest();return;
    }
    if(stream==1 && scenario==5){StopTest();return;}
    if(stream==1 && scenario==6){p[0]=0xa0;p[1]=16;p[3]=26;p[6]=0xff;return;}
    if(stream==1 && scenario==9){p[0]=0xa1;return;} // profile switched on the keyboard
    p[0]=0xa0;p[1]=16;p[3]=26;p[14]=1;p[15]=0x55;
    if(stream==1){p[6]=1;p[7]=0x55;Notify();} // unrelated device change mid-session
    if(stream==2)assert(Connected() && Get(26)==1000 && Owns(0xe5) && !Owns(0x48));
    if(stream==2 && scenario==13){
        // Ace68-II: its own key set, identity and layout token.
        assert(Owns(0x49) && Owns(0xe6) && !Owns(0x45) && !Owns(0x4a));
        NativeAnalogBackendTelemetry t{};Telemetry(&t);
        assert(t.connected && t.vendorId==0x41e4 && t.productId==0x2116 && t.mappedKeys==67);
        assert(!wcscmp(t.deviceName,L"MCHOSE Ace 68 (Ace68-II)"));
        assert(t.verifiedLayoutToken && t.verifiedLayoutToken==halljoy::layout_identity::Token("mchose-ace68","ACE68II-41E4-2116"));
    }
    if(stream==2 && scenario==0){
        NativeAnalogBackendTelemetry t{};Telemetry(&t);
        assert(t.productId==0x211a && t.mappedKeys==79 && !wcscmp(t.deviceName,L"MCHOSE Jet 75 II") && !Owns(0xe6));
    }
    if(stream==3){assert(Get(26)==0);StopTest();}
}
void StopTest(){g_stop.store(true);}
void SupportLog_Event(const char*,std::uint64_t,SupportLogDetail) noexcept{}
namespace halljoy::keyboard_support {void ReportCommunicationAnomaly(unsigned) noexcept{}}
bool NativeAnalogRouting_Claim(std::uint16_t,std::uint16_t,const wchar_t*,NativeAnalogProtocol){return true;}
bool NativeAnalogRouting_IsClaimedBy(const wchar_t*,NativeAnalogProtocol){return false;}
bool NativeAnalogRouting_IsClaimed(const wchar_t*){return false;}
static void Reset(){
    pending=rebooting=false;writes=stream=0;base={0,1,0,0,0,0};
    for(unsigned i=0;i<256;++i)settings[i]=static_cast<unsigned char>(i*29+3);
    for(unsigned p=0;p<4;++p){settings[p*64+2]=0xaa;settings[p*64+3]=0xbb;}
    settings[0]&=0xfe;settings[7]=0;settings[8]=0x55;
}
int main(){
    // 0 pre-enabled, 1 auto, 2 protocol not answered, 3 normal stop, 4 lost enable ACK,
    // 5 stop before any report, 6 invalid analog, 7 stop during enable write,
    // 8 disconnect during cleanup, 9 profile switch report.
    for(scenario=0;scenario<10;++scenario){
        Reset();g_autoAttempted=false;settings[7]=scenario==0?8:0;
        g_stop.store(false);Candidate candidate{L"synthetic"};Run(candidate);
        assert(!Connected() && Get(26)==0 && settings[8]==0x55 && settings[2]==0xaa);
        if(scenario==0 || scenario==1 || scenario==3 || scenario==8)assert(stream==3); // survived the device change
        if(scenario==2)assert(writes==0);
        else if(scenario==0)assert(writes==1 && !(settings[7]&8));
        else {assert(writes==2);assert(bool(settings[7]&8)==(scenario==8));}
        if(scenario==1){
            // Recovery is allowed to read, never repeatedly cycle flash.
            pending=false;writes=stream=0;g_stop.store(false);Run(candidate);
            assert(writes==0 && !(settings[7]&8));
        }
    }
    {   // Reboot-on-write state: enable once, session ends; the reconnected session
        // finds the flag on, streams, and owns the cleanup (which reboots again).
        scenario=10;Reset();settings[0]|=1;g_autoAttempted=false;g_stop.store(false);
        Candidate candidate{L"synthetic"};Run(candidate);
        assert(writes==1 && (settings[7]&8) && stream==0);
        pending=rebooting=false;stream=0;g_stop.store(false);Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3);
    }
    {   // A later firmware version is admitted and works when the protocol is unchanged.
        scenario=11;Reset();g_autoAttempted=false;g_stop.store(false);
        Candidate candidate{L"synthetic"};Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3);
        // A firmware with a different settings layout is never written to.
        scenario=12;Reset();g_autoAttempted=false;g_stop.store(false);Run(candidate);
        assert(writes==0 && stream==0);
    }
    {   // Ace68-II (41E4:2116, stock 1.21): same transaction and lifecycle.
        scenario=13;Reset();g_autoAttempted=false;g_stop.store(false);
        Candidate candidate{L"synthetic",&halljoy::jet75::Models[1]};Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3 && settings[8]==0x55);
        g_model.store(&halljoy::jet75::Models[0]);
    }
    {   // Jet 75 I (older writer): the fresh-flash bit does not reboot it, so the
        // session verifies by readback and streams without a reconnect.
        scenario=14;Reset();settings[0]|=1;g_autoAttempted=false;g_stop.store(false);
        Candidate candidate{L"synthetic",halljoy::jet75::FindModel(0x41e4,0x2118)};Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3);
        g_model.store(&halljoy::jet75::Models[0]);
    }
    {   // Ace 60 Pro with a base the firmware accepts but the old strict check did not
        // (count 0, current 1 -> table[1] = profile 2): enabled on profile 2 and streams.
        scenario=15;Reset();base={1,0,0,2,0,0};g_autoAttempted=false;g_stop.store(false);
        for(unsigned p=0;p<4;++p)settings[p*64+7]=0;
        Candidate candidate{L"synthetic",halljoy::jet75::FindModel(0x41e4,0x2103)};Run(candidate);
        assert(writes==2 && !(settings[2*64+7]&8) && stream==3);
        // A base no firmware rule can resolve is never written to.
        scenario=15;Reset();base={0,1,7,0,0,0};g_autoAttempted=false;g_stop.store(false);
        Candidate older{L"synthetic",halljoy::jet75::FindModel(0x41e4,0x2118)};Run(older);
        assert(writes==0 && stream==0);
        g_model.store(&halljoy::jet75::Models[0]);
    }
    {   scenario=16;Reset();g_autoAttempted=false;g_stop.store(false);
        Candidate candidate{L"synthetic"};Run(candidate);
        assert(writes==2 && !(settings[7]&8) && stream==3);
    }
    {   Session session(Candidate{L"synthetic"});halljoy::jet75::Base value{};
        pending=false;assert(!session.ReadBase(value,true,GetTickCount64()));
        assert(GetLastError()==WAIT_TIMEOUT && !pending); }
    puts("Jet75 lifecycle PASS: auto-enable, pre-enabled no redundant enable, normal/early stop cleanup, ambiguous enable cleanup, protocol not answered, newer firmware admitted, foreign layout not written, stream rejection, stop during write, disconnect, profile switch, reboot-on-write reconnect, no flash retry loop, Ace68-II model, older writer without reboot, Ace 60 Pro firmware base rule, unresolvable base not written, unknown key ignored");
}
