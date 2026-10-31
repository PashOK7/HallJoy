#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <bcrypt.h>
#include <process.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>
#include "mchose_mix87_backend.h"
#include "mchose_mix87_protocol.h"
#include "hid_io_operation.h"
#include "support_log.h"
#include "keyboard_support_status.h"
#include "generated/layout_pipeline/identities.h"
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")
#pragma comment(lib,"bcrypt.lib")

namespace {
namespace tp=halljoy::mix87;
using tp::Report;
struct Handle {
    HANDLE v=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h=INVALID_HANDLE_VALUE):v(h){}
    ~Handle(){if(v && v!=INVALID_HANDLE_VALUE)CloseHandle(v);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    explicit operator bool()const{return v && v!=INVALID_HANDLE_VALUE;}
};
struct Candidate {std::wstring path;const tp::Model* model=&tp::Models[0];};
std::atomic<bool> g_stop{false},g_running{false},g_present{false},g_connected{false};
std::atomic<std::uint64_t> g_good{0},g_bad{0},g_last{0};
std::array<std::atomic<std::uint16_t>,256> g_values{};
std::array<std::atomic<bool>,256> g_owned{};
std::mutex g_service,g_control;
HANDLE g_thread=nullptr,g_wake=nullptr;
// Model of the last enumerated keyboard (telemetry name, layout, key count).
std::atomic<const tp::Model*> g_model{&tp::Models[0]};
std::atomic<unsigned> g_keyCount{0};
Mix87ModeSnapshot g_mode{Mix87ModeState::Absent,0,0};
tp::Base g_base{};
int g_request=-1;
// At most one automatic enable attempt per worker generation. Recovery reads
// may continue, but repeated faults must never become a flash-write loop.
bool g_autoAttempted=false;
void Clear(){g_connected.store(false);for(auto& v:g_values)v.store(0);}
void SetState(Mix87ModeState state){std::lock_guard<std::mutex> l(g_control);g_mode.state=state;}
void Failure(const char* stage,SupportLogDetail detail){++g_bad;SupportLog_Event(stage,0,detail);halljoy::keyboard_support::ReportCommunicationAnomaly(27);}
std::vector<Candidate> Enumerate(){
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE)return {};
    struct FreeSet{HDEVINFO s;~FreeSet(){SetupDiDestroyDeviceInfoList(s);}} freeSet{set};
    std::vector<Candidate> out;
    for(DWORD i=0;;++i){
        SP_DEVICE_INTERFACE_DATA d{};d.cbSize=sizeof(d);
        if(!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&d))break;
        DWORD needed=0;SetupDiGetDeviceInterfaceDetailW(set,&d,nullptr,0,&needed,nullptr);
        if(needed<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))continue;
        std::vector<unsigned char> bytes(needed);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(set,&d,detail,needed,nullptr,nullptr))continue;
        Handle h(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!h)continue;
        HIDD_ATTRIBUTES a{};a.Size=sizeof(a);
        if(!HidD_GetAttributes(h.v,&a))continue;
        const auto* model=tp::FindModel(a.VendorID,a.ProductID);if(!model)continue;
        PHIDP_PREPARSED_DATA data=nullptr;if(!HidD_GetPreparsedData(h.v,&data))continue;
        HIDP_CAPS caps{};const auto status=HidP_GetCaps(data,&caps);HidD_FreePreparsedData(data);
        if(status!=HIDP_STATUS_SUCCESS)continue;
        // Stock descriptor: unnumbered 64-byte IN/OUT => Windows includes ID0.
        SupportLog_Event("mix87.hid_caps",(static_cast<std::uint64_t>(caps.UsagePage)<<48)|(static_cast<std::uint64_t>(caps.Usage)<<32)|(caps.InputReportByteLength<<16)|caps.OutputReportByteLength);
        if(caps.UsagePage==1 && caps.Usage==0 && caps.InputReportByteLength==65 && caps.OutputReportByteLength==65)
            out.push_back({detail->DevicePath,model});
    }
    return out;
}
bool Io(HANDLE handle,bool write,void* bytes,DWORD count,DWORD timeout){
    HidIoOperation op(handle);DWORD error=0,done=0;
    const auto start=write?op.StartWrite(bytes,count,&error):op.StartRead(bytes,count,&error);
    if(start==HidIoOperation::StartResult::Failed){SetLastError(error);return false;}
    if(start==HidIoOperation::StartResult::Pending && op.Wait(timeout)!=WAIT_OBJECT_0){
        op.CancelAndDrain(&done,&error);
        // Keep a report that completed between the timeout and CancelIoEx.
        if(error!=ERROR_SUCCESS || done==0){SetLastError(WAIT_TIMEOUT);return false;}
        if(done!=count){SetLastError(ERROR_BAD_LENGTH);return false;}
        return true;
    }
    if(!op.Finish(&done,&error,false)){SetLastError(error);return false;}
    if(done!=count){SetLastError(ERROR_BAD_LENGTH);return false;}return true;
}
class Session {
    Handle handle;
public:
    explicit Session(const Candidate& c):handle(CreateFileW(c.path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr)){}
    bool Valid()const{return bool(handle);}
    bool Receive(Report& report,DWORD timeout){
        std::array<std::uint8_t,65> wire{};
        if(!Io(handle.v,false,wire.data(),65,timeout))return false;
        if(wire[0]){SetLastError(ERROR_INVALID_DATA);return false;}
        std::copy(wire.begin()+1,wire.end(),report.begin());return true;
    }
    bool Exchange(const Report& request,Report& reply){
        std::array<std::uint8_t,65> wire{};std::copy(request.begin(),request.end(),wire.begin()+1);
        if(!Io(handle.v,true,wire.data(),65,300)){Failure("mix87.send_failed",SupportLog_Win32(GetLastError()));return false;}
        const auto end=GetTickCount64()+800;
        while(GetTickCount64()<end){
            if(!Receive(reply,50)){
                if(GetLastError()==WAIT_TIMEOUT)continue;
                Failure("mix87.reply_failed",SupportLog_Win32(GetLastError()));return false;
            }
            if(tp::Reply(request,reply))return true;
            if(reply[0]==0xab || reply[0]==0xaa){Failure("mix87.reply_mismatch",SupportLog_Protocol(request[1]));return false;}
            // A0 may have been queued before a configuration transaction. It is
            // intentionally ignored while the published gamepad input is neutral.
        }
        Failure("mix87.ack_timeout",SupportLog_Protocol(request[1]));return false;
    }
    bool ReadBytes(std::uint32_t address,std::uint8_t* target,std::size_t size,bool completeWrite=false,ULONGLONG deadline=0){
        for(std::size_t pos=0;pos<size;){
            if(deadline && GetTickCount64()>=deadline){SetLastError(WAIT_TIMEOUT);return false;}
            if(g_stop.load() && !completeWrite)return false;
            const auto count=static_cast<std::uint8_t>(std::min<std::size_t>(56,size-pos));
            Report reply{};
            if(!Exchange(tp::Read(address+static_cast<std::uint32_t>(pos),count),reply))return false;
            std::copy_n(reply.data()+8,count,target+pos);pos+=count;
        }return true;
    }
};
struct Fingerprint{std::uint32_t offset,size;const char* digest;};
// Hashes only: vendor firmware bytes are not included in the application.
constexpr Fingerprint fingerprints[]={
    {0x131f0,1952,"afcfb5972f2d2d51eb5930c7c143a588dc0d0be6de8645878bdf9ae4daeaaf9e"},
    {0xe868,320,"922a79dccbf1deafc0ec061174e2e8d639d830f844a6cf33baa661883c894b79"},
    {0xf406,464,"3cae1b70df6ae61f1dc126af4cf875c3a2df89996eae5e0e7afca11df49edd6a"},
    {0x8754,96,"58f2f1cf98a8fa48982db7298efcad0cff02e09eb143728185b6be868c80e822"},
    {0x8f08,256,"d77daa8354436078bd43e56a18d45754874cc6ef416e557b1cfce857de2e9490"},
    {0x13b85,93,"86a41775ae9ae2022ec05d65441857088c3f9a45383ba9bf1e1916910d143896"},
    {0x15da6,276,"4a6bcfa47be09b1ae0342008d3c44a6ea2727fa8dbbef94bc9fe36c81cc36533"}
};
bool MatchesHash(std::vector<std::uint8_t>& bytes,const char* expected){
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    std::array<unsigned char,32> digest{};
    const auto result=BCryptHash(algorithm,nullptr,0,bytes.data(),static_cast<ULONG>(bytes.size()),digest.data(),32);
    BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return false;
    constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i)if(expected[i*2]!=hex[digest[i]>>4] || expected[i*2+1]!=hex[digest[i]&15])return false;
    return true;
}
bool Verify(Session& session,const tp::Model& model,std::array<bool,256>& map,bool& exact){
    exact=false;
    Report info{},reply{};info[0]=0x55;info[1]=3;info[4]=31;info[3]=tp::Checksum(info);
    // Not version-bound (owner decision for the M HUB family, 2026-10-02):
    // the protocol answer is required, the version is logged, and safety
    // comes from the settings/base checks, readback and A0 validation.
    // Mix87 III 1.22 keeps its exact fingerprint path; other versions use the
    // factory key list (a tester's Mix87 III was refused on 2026-10-29).
    if(!session.Exchange(info,reply))return false;
    const unsigned version=reply[8]|(reply[9]<<8);
    SupportLog_Event("mix87.model",model.pid);
    SupportLog_Event("mix87.firmware_version",version);
    if(!model.fingerprinted || version!=0x0122){
        for(std::size_t i=0;i<model.keyCount;++i)map[model.keys[i]]=true;
        return true;
    }
    for(const auto& f:fingerprints){
        std::vector<std::uint8_t> bytes(f.size);
        if(!session.ReadBytes(f.offset,bytes.data(),bytes.size()))return false;
        if(!MatchesHash(bytes,f.digest)){SupportLog_Event("mix87.firmware_mismatch",f.offset);return false;}
        if(f.offset==0x15da6){
            for(unsigned i=0;i<92;++i){
                const auto hid=tp::Hid(bytes[i*3],bytes[i*3+1],bytes[i*3+2]);
                if(hid){if(map[hid])return false;map[hid]=true;}
            }
        }
    }
    exact=true;
    return std::count(map.begin(),map.end(),true)==86;
}
bool LoadMode(Session& session,tp::Base& base,bool& enabled,unsigned& profile){
    tp::Settings settings{};
    if(!session.ReadBytes(tp::BaseAddress,base.data(),base.size()) || !tp::Profile(base,profile) ||
       !session.ReadBytes(tp::SettingsAddress,settings.data(),settings.size()))return false;
    tp::Base check{};
    if(!session.ReadBytes(tp::BaseAddress,check.data(),check.size()) || check!=base)return false;
    enabled=(settings[profile*64+7]&8)!=0;return true;
}
tp::ChangeResult ChangeMode(Session& session,const tp::Base& base,bool enabled,bool cleanup){
    const auto deadline=GetTickCount64()+10000;
    return tp::ChangeFlag(base,enabled,
        [&](std::uint32_t a,std::uint8_t* p,std::size_t n){return GetTickCount64()<deadline && session.ReadBytes(a,p,n,true,deadline);},
        [&](const Report& q,Report& r){if(!cleanup && g_stop.load())return false;return session.Exchange(q,r);});
}
struct ModeLease {
    Session& session; tp::Base base; bool armed=false;
    ~ModeLease(){
        if(!armed)return;
        Clear(); // No gamepad input while the keyboard changes mode.
        const auto result=ChangeMode(session,base,false,true);
        SupportLog_Event("mix87.cleanup_mode_result",static_cast<unsigned>(result));
        if(result!=tp::ChangeResult::Verified && result!=tp::ChangeResult::Unchanged)
            Failure("mix87.mode_may_remain_enabled",SupportLog_Protocol(static_cast<unsigned>(result)));
    }
};
void Run(const Candidate& c){
    Clear();for(auto& owned:g_owned)owned.store(false);
    {std::lock_guard<std::mutex> l(g_control);++g_mode.session;g_mode.state=Mix87ModeState::Checking;g_request=-1;}
    struct End{~End(){Clear();std::lock_guard<std::mutex> l(g_control);g_request=-1;++g_mode.session;g_mode.state=Mix87ModeState::Failed;}} end;
    const auto& model=*c.model;g_model.store(c.model);g_keyCount.store(0);
    Session session(c);if(!session.Valid()){Failure("mix87.open_failed",SupportLog_Win32(GetLastError()));return;}
    std::array<bool,256> map{};bool exact341=false;
    if(!Verify(session,model,map,exact341)){SupportLog_Event("mix87.admission_failed",1);return;}
    g_keyCount.store(static_cast<unsigned>(std::count(map.begin(),map.end(),true)));
    if(!NativeAnalogRouting_Claim(model.vid,model.pid,c.path.c_str(),NativeAnalogProtocol::MchoseMix87) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::MchoseMix87))return;
    bool enabled=false;unsigned profile=0;tp::Base base{};
    if(!LoadMode(session,base,enabled,profile))return;
    ModeLease lease{session,base,enabled};
    if(!enabled){
        if(g_stop.load())return;
        if(g_autoAttempted){SupportLog_Event("mix87.auto_enable_retry_blocked",profile);return;}
        g_autoAttempted=true;lease.armed=true;SetState(Mix87ModeState::Busy);
        const auto result=ChangeMode(session,base,true,false);
        SupportLog_Event("mix87.auto_enable_result",static_cast<unsigned>(result));
        if(result!=tp::ChangeResult::Verified && result!=tp::ChangeResult::Unchanged)return;
        enabled=true;
    }
    // If a pre-enabled session later fails, do not repeatedly cycle flash during recovery.
    g_autoAttempted=true;
    for(unsigned i=0;i<256;++i)g_owned[i].store(map[i]);
    {std::lock_guard<std::mutex> l(g_control);g_base=base;g_mode.profile=profile;g_mode.state=enabled?Mix87ModeState::Enabled:Mix87ModeState::Disabled;}
    SupportLog_Event("mix87.mode_enabled",enabled);
    SupportLog_Event("mix87.profile",profile);
    // The stream is event-only: an untouched keyboard sends nothing. An
    // admitted session with the mode on is a live source with all keys at 0.
    g_connected.store(enabled);
    // Device-change notifications are system-wide (another USB device, our own
    // virtual gamepad appearing) and do not end a healthy session: removal of
    // this keyboard fails the next read (stream_failed) instead.
    while(!g_stop.load()){
        int request=-1;tp::Base consent{};
        {std::lock_guard<std::mutex> l(g_control);request=g_request;g_request=-1;consent=g_base;}
        if(request>=0){
            Clear();
            if(g_stop.load())return;
            const auto deadline=GetTickCount64()+10000;
            const auto result=tp::ChangeFlag(consent,request!=0,
                [&](std::uint32_t a,std::uint8_t* p,std::size_t n){return GetTickCount64()<deadline && session.ReadBytes(a,p,n,true,deadline);},
                [&](const Report& q,Report& r){if(g_stop.load())return false;return session.Exchange(q,r);});
            SupportLog_Event("mix87.explicit_mode_result",static_cast<unsigned>(result));
            if(result!=tp::ChangeResult::Verified && result!=tp::ChangeResult::Unchanged)return;
            enabled=request!=0;SetState(enabled?Mix87ModeState::Enabled:Mix87ModeState::Disabled);
            g_connected.store(enabled);
        }
        Report report{};
        if(!session.Receive(report,50)){
            if(GetLastError()==WAIT_TIMEOUT)continue; // idle is valid, never expire held keys
            Failure("mix87.stream_failed",SupportLog_Win32(GetLastError()));return;
        }
        if(report[0]==0xa2 || report[0]==0xa3 || report[0]==0xaa || report[0]==0xab){
            // Profile/configurator activity may stop debug output without a USB
            // disconnect. Neutralize and require a new admission (pause/resume).
            Failure("mix87.configuration_changed",SupportLog_Protocol(report[0]));return;
        }
        if(!enabled)continue;
        tp::Sample sample{};
        if(tp::Decode(report,map,sample,exact341)){
            g_values[sample.hid].store(sample.milli);g_last.store(GetTickCount64());++g_good;g_connected.store(true);
        }else if(report[0]==0xa0 && report[1]==0x10){Failure("mix87.invalid_analog",SupportLog_Protocol(1));return;}
    }
}
unsigned __stdcall Worker(void*) noexcept{
    try{
        while(!g_stop.load()){
            const auto candidates=Enumerate();g_present.store(!candidates.empty());
            if(candidates.size()==1)Run(candidates[0]);
            else if(candidates.size()>1){g_model.store(candidates[0].model);SetState(Mix87ModeState::Failed);SupportLog_Event("mix87.multiple_devices",candidates.size());}
            else SetState(Mix87ModeState::Absent);
            if(!g_stop.load())WaitForSingleObject(g_wake,candidates.empty()?INFINITE:5000);
        }
    }catch(...){Failure("mix87.worker_exception",SupportLog_Protocol(1));SetState(Mix87ModeState::Failed);}
    Clear();g_running.store(false);return 0;
}
bool Start(){
    std::lock_guard<std::mutex> l(g_service);if(g_thread)return g_running.load();
    g_autoAttempted=false;g_stop.store(false);g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!g_wake)return false;
    g_running.store(true);unsigned id=0;g_thread=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Worker,nullptr,0,&id));
    if(!g_thread){g_running.store(false);CloseHandle(g_wake);g_wake=nullptr;return false;}return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation){
    std::lock_guard<std::mutex> l(g_service);g_stop.store(true);if(!g_thread)return NativeAnalogBackendStopJoined(generation);
    SetEvent(g_wake);const auto wait=WaitForSingleObject(g_thread,25000);
    if(wait!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,WAIT_TIMEOUT);
    CloseHandle(g_thread);g_thread=nullptr;CloseHandle(g_wake);g_wake=nullptr;Clear();return NativeAnalogBackendStopJoined(generation);
}
void Notify(){std::lock_guard<std::mutex> l(g_service);if(g_wake)SetEvent(g_wake);}
bool Present(){return g_present.load();}bool Connected(){return g_connected.load();}
bool Owns(std::uint16_t hid){return hid<256 && Connected() && g_owned[hid].load();}
std::uint16_t Get(std::uint16_t hid){return Owns(hid)?g_values[hid].load():0;}
void Telemetry(NativeAnalogBackendTelemetry* out){
    if(!out)return;
    *out={};const auto& model=*g_model.load();
    out->present=Present();out->connected=Connected();out->vendorId=model.vid;out->productId=model.pid;out->usagePage=1;
    out->inputReportBytes=65;out->outputReportBytes=65;out->successfulUpdates=g_good.load();out->failedUpdates=g_bad.load();
    out->nominalRawLevels=342;out->mappedKeys=Connected()?g_keyCount.load():0;
    for(unsigned i=0;i<256;++i)if(Get(static_cast<std::uint16_t>(i)))++out->activeKeys;
    const auto last=g_last.load();out->lastUpdateAgeMs=last?static_cast<unsigned>(std::min<ULONGLONG>(0xffffffff,GetTickCount64()-last)):0;
    wcscpy_s(out->deviceName,model.name);
    // Published only after firmware admission; selects the official M HUB layout.
    if(out->connected)out->verifiedLayoutToken=halljoy::layout_identity::Token(model.layoutProtocol,model.layoutProduct);
    const auto state=MchoseMix87_GetMode().state;
    swprintf_s(out->status,L"%ls: %ls",model.shortName,state==Mix87ModeState::Enabled?L"analog active; automatically disabled on pause or exit":
        state==Mix87ModeState::Disabled?L"analog disabled":
        state==Mix87ModeState::Failed?L"admission/communication failed; see log; pause and resume to retry activation":L"checking firmware / preparing analog");
}
}
Mix87ModeSnapshot MchoseMix87_GetMode(){std::lock_guard<std::mutex> l(g_control);return g_mode;}
bool MchoseMix87_RequestMode(Mix87ModeSnapshot expected,bool enabled){
    std::lock_guard<std::mutex> l(g_control);
    if(g_stop.load() || expected.session!=g_mode.session || expected.profile!=g_mode.profile || expected.state!=g_mode.state ||
       (g_mode.state!=Mix87ModeState::Enabled && g_mode.state!=Mix87ModeState::Disabled))return false;
    g_request=enabled?1:0;g_mode.state=Mix87ModeState::Busy;return true;
}
const NativeAnalogBackendDescriptor& MchoseMix87_GetNativeBackendDescriptor(){
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "mchose_mix87",L"MCHOSE M HUB (Mix87 III family) analog",NativeAnalogProtocol::MchoseMix87,
        NativeAnalogStartPhase::AfterRealtime,NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};return d;
}
