#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include "mchose_jet75_backend.h"
#include "mchose_jet75_protocol.h"
#include "hid_io_operation.h"
#include "support_log.h"
#include "keyboard_support_status.h"
#include "generated/layout_pipeline/identities.h"
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")

// MCHOSE Jet 75 II and Ace68-II (tp::Models): same lifecycle policy as Mix87 III
// (owner decision 2026-09-27): enable the persistent debug flag at start/resume,
// disable it on pause/exit, at most one automatic enable per worker generation.
namespace {
namespace tp=halljoy::jet75;
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
std::mutex g_service;
// Model of the last enumerated keyboard (telemetry name, layout, keys).
std::atomic<const tp::Model*> g_model{&tp::Models[0]};
HANDLE g_thread=nullptr,g_wake=nullptr;
// At most one automatic enable attempt per worker generation. Recovery reads
// may continue, but repeated faults must never become a flash-write loop.
bool g_autoAttempted=false;
void Clear(){g_connected.store(false);for(auto& v:g_values)v.store(0);}
void Failure(const char* stage,SupportLogDetail detail){++g_bad;SupportLog_Event(stage,0,detail);halljoy::keyboard_support::ReportCommunicationAnomaly(29);}
std::vector<Candidate> Enumerate(){
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE)return {};
    struct FreeSet{HDEVINFO s;~FreeSet(){SetupDiDestroyDeviceInfoList(s);}} freeSet{set};
    std::vector<Candidate> out;std::vector<std::uint64_t> seen;
    // Collections of admitted models, logged when the set changes (not every pass).
    static std::vector<std::uint64_t> loggedCaps;
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
        seen.push_back((static_cast<std::uint64_t>(a.ProductID)<<56)|(static_cast<std::uint64_t>(caps.UsagePage)<<40)|(static_cast<std::uint64_t>(caps.Usage)<<24)|(caps.InputReportByteLength<<12)|caps.OutputReportByteLength);
        if(caps.UsagePage==1 && caps.Usage==0 && caps.InputReportByteLength==65 && caps.OutputReportByteLength==65)
            out.push_back({detail->DevicePath,model});
    }
    std::sort(seen.begin(),seen.end());
    if(seen!=loggedCaps){
        // value: pid low byte<<56 | usage page<<40 | usage<<24 | input bytes<<12 | output bytes
        for(const auto v:seen)SupportLog_Event("jet75.hid_caps",v);
        SupportLog_Event("jet75.control_collections",out.size(),SupportLog_Data(seen.size()));
        loggedCaps=seen;
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
        if(!Io(handle.v,true,wire.data(),65,300)){Failure("jet75.send_failed",SupportLog_Win32(GetLastError()));return false;}
        const auto end=GetTickCount64()+800;
        while(GetTickCount64()<end){
            if(!Receive(reply,50)){
                if(GetLastError()==WAIT_TIMEOUT)continue;
                Failure("jet75.reply_failed",SupportLog_Win32(GetLastError()));return false;
            }
            if(tp::Reply(request,reply))return true;
            if(reply[0]==0xab || reply[0]==0xaa){Failure("jet75.reply_mismatch",SupportLog_Protocol(request[1]));return false;}
            // A0 may have been queued before a configuration transaction. It is
            // intentionally ignored while the published gamepad input is neutral.
        }
        Failure("jet75.ack_timeout",SupportLog_Protocol(request[1]));return false;
    }
    bool ReadBytes(std::uint8_t op,std::uint8_t* target,std::size_t size,bool completeWrite=false,ULONGLONG deadline=0){
        for(std::size_t pos=0;pos<size;){
            if(deadline && GetTickCount64()>=deadline){SetLastError(WAIT_TIMEOUT);return false;}
            if(g_stop.load() && !completeWrite)return false;
            const auto count=static_cast<std::uint8_t>(std::min<std::size_t>(56,size-pos));
            Report reply{};
            if(!Exchange(tp::Read(op,static_cast<std::uint8_t>(pos),count),reply))return false;
            std::copy_n(reply.data()+8,count,target+pos);pos+=count;
        }return true;
    }
    bool ReadBase(tp::Base& b,bool completeWrite=false,ULONGLONG deadline=0){return ReadBytes(tp::ReadBaseOp,b.data(),b.size(),completeWrite,deadline);}
    bool ReadSettings(tp::Settings& s,bool completeWrite=false,ULONGLONG deadline=0){return ReadBytes(tp::ReadSettingsOp,s.data(),s.size(),completeWrite,deadline);}
};
// Any firmware version is admitted: the M HUB protocol answer is required,
// and the version is only recorded. Safety comes from the data checks
// (settings layout, readback, A0 validation), not from a version list.
bool Verify(Session& session,const tp::Model& model){
    Report reply{};
    if(!session.Exchange(tp::Info(),reply))return false;
    const auto version=tp::Version(reply);
    SupportLog_Event("jet75.model",model.pid);
    SupportLog_Event("jet75.firmware_version",version);
    if(version!=model.reviewed)SupportLog_Event("jet75.firmware_unreviewed",version,SupportLog_Data(model.reviewed));
    return true;
}
bool LoadMode(Session& session,const tp::Model& model,tp::Base& base,bool& enabled,unsigned& profile){
    tp::Settings settings{};tp::Base check{};
    // The keyboard may rewrite the base while it is read (a profile switch saved
    // late): read until two reads around the settings read agree.
    bool stable=false;
    for(unsigned attempt=0;attempt<3 && !stable;++attempt){
        if(!session.ReadBase(base) || !session.ReadSettings(settings) || !session.ReadBase(check)){
            if(!g_stop.load())SupportLog_Event("jet75.load_failed",attempt,SupportLog_Win32(GetLastError()));
            return false;
        }
        stable=check==base;
        if(!stable)SupportLog_Event("jet75.profile_base_changed",tp::Pack(base),SupportLog_Data(tp::Pack(check)));
    }
    if(!stable)return false;
    // value: the six base bytes {current, count, table[4]}, little-endian;
    // detail: resolved profile, or 0xff when the firmware rule leaves it unknown.
    const bool resolved=tp::Profile(base,profile,model.baseRule);
    SupportLog_Event("jet75.profile_base",tp::Pack(base),SupportLog_Data(resolved?profile:0xffu));
    if(!resolved){SupportLog_Event("jet75.profile_base_unusable",tp::Pack(base),SupportLog_Data(static_cast<unsigned>(model.baseRule)));return false;}
    if(!tp::ProfileValid(settings,profile)){
        // detail: the profile's first eight settings bytes (magic AA BB at 2..3).
        std::uint64_t head=0;for(unsigned i=8;i-->0;)head=(head<<8)|settings[profile*64+i];
        SupportLog_Event("jet75.profile_format",profile,SupportLog_Data(head));return false;
    }
    if(model.freshFlashReboot && tp::WriteReboots(settings))SupportLog_Event("jet75.write_reboots",1);
    enabled=(settings[profile*64+7]&8)!=0;return true;
}
tp::ChangeResult ChangeMode(Session& session,const tp::Model& model,const tp::Base& base,bool enabled,bool cleanup){
    const auto deadline=GetTickCount64()+10000;
    return tp::ChangeFlag(base,enabled,
        [&](tp::Base& b){return GetTickCount64()<deadline && session.ReadBase(b,true,deadline);},
        [&](tp::Settings& s){return GetTickCount64()<deadline && session.ReadSettings(s,true,deadline);},
        [&](const Report& q,Report& r){if(!cleanup && g_stop.load())return false;return session.Exchange(q,r);},
        model.freshFlashReboot,model.baseRule);
}
bool Settled(tp::ChangeResult r){
    return r==tp::ChangeResult::Verified || r==tp::ChangeResult::Unchanged || r==tp::ChangeResult::Rebooting;
}
struct ModeLease {
    Session& session; const tp::Model& model; tp::Base base; bool armed=false;
    ~ModeLease(){
        if(!armed)return;
        Clear(); // No gamepad input while the keyboard changes mode.
        const auto result=ChangeMode(session,model,base,false,true);
        SupportLog_Event("jet75.cleanup_mode_result",static_cast<unsigned>(result));
        if(!Settled(result))Failure("jet75.mode_may_remain_enabled",SupportLog_Protocol(static_cast<unsigned>(result)));
    }
};
void Run(const Candidate& c){
    Clear();for(auto& owned:g_owned)owned.store(false);
    struct End{~End(){Clear();}} end;
    const auto& model=*c.model;g_model.store(c.model);
    Session session(c);if(!session.Valid()){Failure("jet75.open_failed",SupportLog_Win32(GetLastError()));return;}
    if(!Verify(session,model)){SupportLog_Event("jet75.admission_failed",1);return;}
    if(!NativeAnalogRouting_Claim(model.vid,model.pid,c.path.c_str(),NativeAnalogProtocol::MchoseJet75) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::MchoseJet75)){
        SupportLog_Event("jet75.claim_refused",model.pid,SupportLog_Data(NativeAnalogRouting_IsClaimed(c.path.c_str())));return;}
    bool enabled=false;unsigned profile=0;tp::Base base{};
    if(!LoadMode(session,model,base,enabled,profile))return;
    ModeLease lease{session,model,base,enabled};
    if(!enabled){
        if(g_stop.load())return;
        if(g_autoAttempted){SupportLog_Event("jet75.auto_enable_retry_blocked",profile);return;}
        g_autoAttempted=true;lease.armed=true;
        const auto result=ChangeMode(session,model,base,true,false);
        SupportLog_Event("jet75.auto_enable_result",static_cast<unsigned>(result));
        if(result==tp::ChangeResult::Rebooting){
            // The flag is saved; the keyboard reconnects and the next session
            // finds it enabled (and owns its cleanup).
            lease.armed=false;return;
        }
        if(!Settled(result))return;
        enabled=true;
    }
    // If a pre-enabled session later fails, do not repeatedly cycle flash during recovery.
    g_autoAttempted=true;
    const auto allowed=tp::Allowed(model);
    for(unsigned i=0;i<256;++i)g_owned[i].store(allowed[i]);
    SupportLog_Event("jet75.mode_enabled",enabled);
    SupportLog_Event("jet75.profile",profile);
    // The stream is event-only: an untouched keyboard sends nothing. An
    // admitted session with the mode on is a live source with all keys at 0.
    g_connected.store(true);
    std::array<bool,256> unknownLogged{};
    // Device-change notifications are system-wide (another USB device, our own
    // virtual gamepad appearing) and do not end a healthy session: removal of
    // this keyboard fails the next read (stream_failed) instead.
    while(!g_stop.load()){
        Report report{};
        if(!session.Receive(report,50)){
            if(GetLastError()==WAIT_TIMEOUT)continue; // idle is valid, never expire held keys
            Failure("jet75.stream_failed",SupportLog_Win32(GetLastError()));return;
        }
        if(report[0]==0xa1 || report[0]==0xa2 || report[0]==0xa3 || report[0]==0xaa || report[0]==0xab){
            // A profile switch (A1) or configurator activity can stop debug
            // output without a USB disconnect. Neutralize and require a new
            // admission (pause/resume).
            Failure("jet75.configuration_changed",SupportLog_Protocol(report[0]));return;
        }
        tp::Sample sample{};
        if(tp::Decode(report,allowed,sample)){
            g_values[sample.hid].store(sample.milli);g_last.store(GetTickCount64());++g_good;g_connected.store(true);
        }else if(report[0]==0xa0 && report[1]==0x10){
            const auto hid=tp::Hid(report[1],report[2],report[3]);
            const unsigned depth=(report[6]<<8)|report[7],maximum=(report[14]<<8)|report[15];
            const std::uint64_t data=(static_cast<std::uint64_t>(hid)<<32)|(maximum<<16)|depth;
            if(hid && hid<256 && !allowed[hid] && tp::KnownMaximum(maximum) && depth<=maximum){
                // A key outside this model's reviewed set: ignore it, keep the session.
                if(!unknownLogged[hid]){unknownLogged[hid]=true;SupportLog_Event("jet75.unknown_key",hid,SupportLog_Data(data));}
                continue;
            }
            Failure("jet75.invalid_analog",SupportLog_Data(data));return;
        }
    }
}
unsigned __stdcall Worker(void*) noexcept{
    try{
        while(!g_stop.load()){
            const auto candidates=Enumerate();g_present.store(!candidates.empty());
            static std::size_t loggedCount=1;
            if(candidates.size()!=loggedCount && (candidates.size()>1 || loggedCount>1))SupportLog_Event("jet75.multiple_devices",candidates.size());
            loggedCount=candidates.empty()?1:candidates.size();
            // Each collection is tried in turn; one that fails admission returns at once.
            for(const auto& c:candidates){if(g_stop.load())break;Run(c);}
            if(!g_stop.load())WaitForSingleObject(g_wake,candidates.empty()?INFINITE:5000);
        }
    }catch(...){Failure("jet75.worker_exception",SupportLog_Protocol(1));}
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
    out->nominalRawLevels=342;out->mappedKeys=Connected()?static_cast<unsigned>(model.keyCount):0;
    for(unsigned i=0;i<256;++i)if(Get(static_cast<std::uint16_t>(i)))++out->activeKeys;
    const auto last=g_last.load();out->lastUpdateAgeMs=last?static_cast<unsigned>(std::min<ULONGLONG>(0xffffffff,GetTickCount64()-last)):0;
    wcscpy_s(out->deviceName,model.name);
    // Published only after firmware admission; selects the official M HUB layout.
    if(out->connected)out->verifiedLayoutToken=halljoy::layout_identity::Token(model.layoutProtocol,model.layoutProduct);
    swprintf_s(out->status,L"%ls: %ls",model.shortName,Connected()?L"analog active; automatically disabled on pause or exit":
        Present()?L"checking firmware / preparing analog; see log if this persists":L"not connected");
}
}
const NativeAnalogBackendDescriptor& MchoseJet75_GetNativeBackendDescriptor(){
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "mchose_jet75",L"MCHOSE M HUB (Jet 75 II family) analog",NativeAnalogProtocol::MchoseJet75,
        NativeAnalogStartPhase::AfterRealtime,NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};return d;
}
