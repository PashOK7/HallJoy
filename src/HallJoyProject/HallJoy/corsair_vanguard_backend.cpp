#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "corsair_vanguard_backend.h"
#include "corsair_vanguard_client.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "support_log.h"
#include "realtime_loop.h"

namespace {
namespace cv=halljoy::corsair_vanguard;
constexpr std::uint64_t VanguardLayoutToken(unsigned region) noexcept {
    using halljoy::layout_identity::Token;
    switch(region) {
    case 1: return Token("corsair_vanguard", "VANGUARDPRO96-1B1C-2B0E-ANSI");
    case 2: return Token("corsair_vanguard", "VANGUARDPRO96-1B1C-2B0E-ISO");
    case 4: return Token("corsair_vanguard", "VANGUARDPRO96-1B1C-2B0E-JIS");
    default: return 0;
    }
}
static_assert(VanguardLayoutToken(1) && VanguardLayoutToken(2) && VanguardLayoutToken(4));
std::atomic<bool> g_stop{false},g_running{false},g_present{false},g_connected{false},g_busy{false};
std::atomic<std::uint64_t> g_good{0},g_bad{0},g_last{0},g_topology{0};
std::atomic<unsigned> g_page{0},g_usage{0},g_inputBytes{0},g_outputBytes{0};
std::atomic<std::uint64_t> g_attempt{0};
std::atomic<cv::Stage> g_stage{cv::Stage::Idle};
std::atomic<const cv::Layout*> g_layout{nullptr};
halljoy::physical_analog::Publication g_values;
std::mutex g_service;
HANDLE g_thread=nullptr,g_wake=nullptr;

// Retained by the ordinary evidence store even after history rolls over and
// Open log rewrites its snapshot. This does not enable continuous logging.
void CorsairEvent(const char* category,std::uint64_t value,SupportLogDetail detail={}) noexcept {
    const char* kind=detail.kind==SupportLogDetailKind::Win32?"win32":
        detail.kind==SupportLogDetailKind::Protocol?"protocol":detail.kind==SupportLogDetailKind::Data?"data":"none";
    char line[384]{};
    _snprintf_s(line,sizeof(line),_TRUNCATE,"evidence.%s revision=2 attempt=%llu value=%llu detail_kind=%s detail=%llu",
        category,g_attempt.load(),value,kind,detail.value);
    SupportLog_Evidence(line,false);
}
struct Discovery {unsigned products=0,controls=0,errors=0;DWORD firstError=0;};
void DiscoveryEvidence(const Discovery& d) {
    static Discovery previous{};static bool first=true;
    if(!first && d.products==previous.products && d.controls==previous.controls &&
        d.errors==previous.errors && d.firstError==previous.firstError)return;
    first=false;previous=d;
    char line[384]{};
    _snprintf_s(line,sizeof(line),_TRUNCATE,"evidence.corsair.discovery revision=2 product_collections=%u control_if2_collections=%u metadata_errors=%u first_win32=%lu metadata_only=1",
        d.products,d.controls,d.errors,d.firstError);
    SupportLog_Evidence(line,false);
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h):value(h){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    explicit operator bool() const{return value && value!=INVALID_HANDLE_VALUE;}
};
void Clear() {
    const bool wasConnected=g_connected.exchange(false,std::memory_order_acq_rel);
    const bool changed=g_values.Clear();g_layout.store(nullptr);g_last.store(0);
    if(wasConnected || changed)RealtimeLoop_NotifyInputChanged();
}
std::wstring Lower(std::wstring text) {
    for(auto& c:text)c=static_cast<wchar_t>(towlower(c));
    return text;
}
std::vector<std::wstring> Enumerate(Discovery& scan) {
    std::vector<std::wstring> result;
    const auto error=[&](DWORD code) {++scan.errors;if(!scan.firstError)scan.firstError=code;};
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE){error(GetLastError());DiscoveryEvidence(scan);return result;}
    struct Guard {HDEVINFO value;~Guard(){SetupDiDestroyDeviceInfoList(value);}} guard{set};
    for(DWORD i=0;i<512;++i) {
        SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&iface)) {
            const auto code=GetLastError();if(code==ERROR_NO_MORE_ITEMS)break;
            error(code);continue;
        }
        DWORD bytes=0;SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&bytes,nullptr);
        if(bytes<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || bytes>65536){error(ERROR_BAD_LENGTH);continue;}
        std::vector<unsigned char> memory(bytes);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(memory.data());
        detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,bytes,nullptr,nullptr)){error(GetLastError());continue;}
        const auto path=Lower(detail->DevicePath);
        if(path.find(L"vid_1b1c&pid_2b0e")==std::wstring::npos)continue;
        ++scan.products;
        // Exact official product and IF2; verify actual opened attributes/caps.
        if(path.find(L"&mi_02")==std::wstring::npos)continue;
        ++scan.controls;result.emplace_back(detail->DevicePath);
    }
    DiscoveryEvidence(scan);return result;
}

class Transport {
    std::uint64_t topology_;
    HidIoOperation read_,write_;
    bool Finish(HidIoOperation& operation,HidIoOperation::StartResult started,
                DWORD& bytes,std::uint64_t deadline,bool cleanup) {
        if(started==HidIoOperation::StartResult::Failed)return false;
        if(started==HidIoOperation::StartResult::Completed)return operation.Finish(&bytes,&error,false);
        for(;;) {
            if(!cleanup && (g_stop.load() || topology_!=g_topology.load())) {error=ERROR_CANCELLED;break;}
            const auto now=GetTickCount64();
            if(now>=deadline) {error=WAIT_TIMEOUT;break;}
            HANDLE events[]={operation.Event(),g_wake};
            const auto wait=cleanup?WaitForSingleObject(events[0],static_cast<DWORD>(deadline-now)):
                WaitForMultipleObjects(2,events,FALSE,static_cast<DWORD>(deadline-now));
            if(wait==WAIT_OBJECT_0)return operation.Finish(&bytes,&error,false);
            if(wait==WAIT_FAILED){error=GetLastError();break;}
            if(wait==WAIT_TIMEOUT){error=WAIT_TIMEOUT;break;}
        }
        const auto failure=error;DWORD ignored=0;
        operation.CancelAndDrain(&bytes,&ignored);error=failure;return false;
    }
public:
    DWORD error=0;
    unsigned protocolError=0,command=0,property=0,subcommand=0,reportId=0;
    unsigned skippedPrefix=0,skippedOpcode=0;
    DWORD actualBytes=0;
    const char* phase="none";
    void ObserveStage(cv::Stage stage){g_stage.store(stage);}
    explicit Transport(HANDLE handle,std::uint64_t topology):topology_(topology),read_(handle),write_(handle){}
    bool Exchange(const cv::Frame& request,cv::Frame& reply,cv::ReplyKind kind,bool cleanup) {
        error=0;protocolError=0;command=request[3];property=command==2?cv::U16(request.data()+4):0;
        subcommand=property==0xc6?request[6]:0;reportId=0;actualBytes=0;skippedPrefix=skippedOpcode=0;phase="write";
        if(!cleanup && (g_stop.load() || topology_!=g_topology.load())) {error=ERROR_CANCELLED;return false;}
        const auto deadline=GetTickCount64()+(cleanup?100:250);
        std::array<unsigned char,cv::kReportBytes> output{},input{};
        std::copy(request.begin(),request.end(),output.begin()+1);
        DWORD bytes=0;
        if(!Finish(write_,write_.StartWrite(output.data(),static_cast<DWORD>(output.size()),&error),bytes,deadline,cleanup)){actualBytes=bytes;return false;}
        actualBytes=bytes;
        if(bytes!=output.size()){error=ERROR_BAD_LENGTH;return false;}
        // A timeout aborts the session. Never let a late calibration answer
        // become another request's data; cleanup matches its own opcode/token.
        for(unsigned ignored=0;ignored<16 && GetTickCount64()<deadline;++ignored) {
            input.fill(0);phase="read";bytes=0;actualBytes=0;
            if(!Finish(read_,read_.StartRead(input.data(),static_cast<DWORD>(input.size()),&error),bytes,deadline,cleanup)){actualBytes=bytes;return false;}
            actualBytes=bytes;reportId=input[0];
            if(bytes!=input.size() || input[0]!=0){error=ERROR_BAD_LENGTH;return false;}
            std::copy(input.begin()+1,input.end(),reply.begin());
            if(reply[0] || reply[2]!=request[2]){++skippedPrefix;continue;}
            if(reply[1]){protocolError=reply[1];return false;}
            if(kind==cv::ReplyKind::Normal && reply[3]!=request[3]){++skippedOpcode;continue;}
            if(kind==cv::ReplyKind::Normal && reply[4]){protocolError=reply[4];return false;}
            return true;
        }
        error=ERROR_INVALID_DATA;return false;
    }
};
void Failure(cv::Stage stage,const Transport& io,const cv::Client<Transport>* client=nullptr) {
    ++g_bad;g_stage.store(stage);
    // Evidence records are 384 bytes including uptime. Keep each record under
    // 330 bytes; split context rather than silently truncate the actual cause.
    char line[330]{};
    _snprintf_s(line,sizeof(line),_TRUNCATE,
        "evidence.corsair.failure revision=2 attempt=%llu stage=%s reason=%s command=%u property=%u subcommand=%u win32=%lu protocol=%u",
        g_attempt.load(),cv::StageName(stage),client?cv::Reason(client->reject):"host_binding",
        io.command,io.property,io.subcommand,io.error,io.protocolError);
    SupportLog_Evidence(line,false);
    _snprintf_s(line,sizeof(line),_TRUNCATE,
        "evidence.corsair.transport revision=2 attempt=%llu phase=%s expected_bytes=%zu actual_bytes=%lu report_id=%u skipped_prefix=%u skipped_opcode=%u",
        g_attempt.load(),io.phase,cv::kReportBytes,io.actualBytes,io.reportId,io.skippedPrefix,io.skippedOpcode);
    SupportLog_Evidence(line,false);
    if(client) {
        _snprintf_s(line,sizeof(line),_TRUNCATE,
            "evidence.corsair.admission revision=2 attempt=%llu vid=%u pid=%u firmware=%u.%u.%u expected_firmware=2.8.171 bragi=%u layout=%u file_id=%u file_bytes=%lu saved_calibration=%u switch_file=%u",
            g_attempt.load(),client->observedVid,client->observedPid,
            client->firmware&255,(client->firmware>>8)&255,client->firmware>>16,
            client->bragi,client->layoutId,client->currentFile,static_cast<unsigned long>(client->fileBytes),
            client->savedCalibration?1:0,client->switchFile?1:0);
        SupportLog_Evidence(line,false);
    }
    CorsairEvent("corsair.admission_or_read_failed",static_cast<unsigned>(stage),SupportLog_Data(io.protocolError));
    if(io.error)CorsairEvent("corsair.io_failed",static_cast<unsigned>(stage),SupportLog_Win32(io.error));
}
enum class CapsResult { Accepted, Retry, Rejected };
CapsResult Caps(HANDLE handle) {
    HIDD_ATTRIBUTES attributes{};attributes.Size=sizeof(attributes);
    if(!HidD_GetAttributes(handle,&attributes)) {CorsairEvent("corsair.attributes_failed",1,SupportLog_Win32(GetLastError()));return CapsResult::Retry;}
    if(attributes.VendorID!=cv::kVid || attributes.ProductID!=cv::kPid) {
        CorsairEvent("corsair.attributes_rejected",(std::uint32_t(attributes.VendorID)<<16)|attributes.ProductID);return CapsResult::Rejected;
    }
    PHIDP_PREPARSED_DATA preparsed=nullptr;
    if(!HidD_GetPreparsedData(handle,&preparsed)) {CorsairEvent("corsair.preparsed_failed",1,SupportLog_Win32(GetLastError()));return CapsResult::Retry;}
    HIDP_CAPS caps{};const auto result=HidP_GetCaps(preparsed,&caps);HidD_FreePreparsedData(preparsed);
    if(result!=HIDP_STATUS_SUCCESS){CorsairEvent("corsair.caps_query_failed",1,SupportLog_Data(static_cast<std::uint32_t>(result)));return CapsResult::Retry;}
    g_page.store(caps.UsagePage);g_usage.store(caps.Usage);
    g_inputBytes.store(caps.InputReportByteLength);g_outputBytes.store(caps.OutputReportByteLength);
    CorsairEvent("corsair.caps",caps.InputReportByteLength,SupportLog_Data(caps.OutputReportByteLength));
    CorsairEvent("corsair.usage",caps.UsagePage,SupportLog_Data(caps.Usage));
    return caps.InputReportByteLength==cv::kReportBytes && caps.OutputReportByteLength==cv::kReportBytes?CapsResult::Accepted:CapsResult::Rejected;
}
// Return true only for a rejected proof; remember it until the path leaves the
// bus or the owner deliberately starts a fresh engine generation with Resume.
bool Run(const std::wstring& path) {
    if(NativeAnalogRouting_IsClaimed(path.c_str()) &&
       !NativeAnalogRouting_IsClaimedBy(path.c_str(),NativeAnalogProtocol::CorsairVanguard)){CorsairEvent("corsair.foreign_path_claim",1);return true;}
    ++g_attempt;Clear();g_stage.store(cv::Stage::Open);
    g_page.store(0);g_usage.store(0);g_inputBytes.store(0);g_outputBytes.store(0);
    static DWORD lastOpenError=0;
    Handle input(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    const auto openError=input?ERROR_SUCCESS:GetLastError();
    g_busy.store(!input && NativeAnalog_IsAccessBusyError(openError));
    if(!input) {
        if(lastOpenError!=openError) {lastOpenError=openError;CorsairEvent("corsair.open_failed",1,SupportLog_Win32(openError));}
        return false;
    }
    g_busy.store(false);lastOpenError=0;
    const auto caps=Caps(input.value);
    if(caps!=CapsResult::Accepted) {++g_bad;CorsairEvent("corsair.caps_rejected",caps==CapsResult::Rejected?1:0);return caps==CapsResult::Rejected;}
    if(!HidD_FlushQueue(input.value)) {CorsairEvent("corsair.flush_failed",1,SupportLog_Win32(GetLastError()));return false;}
    const auto topology=g_topology.load();
    Transport io(input.value,topology);cv::Client<Transport> client(io);
    LARGE_INTEGER counter{};QueryPerformanceCounter(&counter);
    const auto token=static_cast<std::uint32_t>(counter.QuadPart)^GetCurrentProcessId()^GetCurrentThreadId();
    const auto cleanup=[&] {
        Clear();
        const bool ok=client.Close();
        if(!ok)Failure(cv::Stage::Cleanup,io,&client);
        CorsairEvent("corsair.session_cleanup",ok?1:0,SupportLog_Win32(ok?0:io.error));
        if(!ok && io.protocolError)CorsairEvent("corsair.cleanup_reply_failed",1,SupportLog_Protocol(io.protocolError));
    };
    const bool initialized=client.Initialize(token);
    CorsairEvent("corsair.identity",client.firmware,SupportLog_Data(client.layoutId));
    CorsairEvent("corsair.admission_context",static_cast<unsigned>(client.CurrentStage()),
        SupportLog_Data((client.savedCalibration?1u:0u)|(client.switchFile?2u:0u)));
    if(!initialized) {if(!g_stop.load() && topology==g_topology.load())Failure(client.CurrentStage(),io,&client);
        else CorsairEvent("corsair.admission_cancelled",static_cast<unsigned>(client.CurrentStage()));
        cleanup();return !g_stop.load() && topology==g_topology.load();}
    const auto* layout=client.profile.layout;
    CorsairEvent("corsair.calibration",client.savedCalibration?2:0,SupportLog_Data(client.profile.mapped));
    CorsairEvent("corsair.switch_settings",client.switchFile?1:0);
    for(unsigned i=0;i<layout->count;++i)if(layout->hid[i])
        if(!g_values.Bind(static_cast<std::uint8_t>(i+1),layout->hid[i])){Failure(cv::Stage::Layout,io);cleanup();return true;}
    g_layout.store(layout);g_stage.store(cv::Stage::Sampling);
    bool live=false;
    const auto sessionStarted=GetTickCount64();auto nextSummary=sessionStarted+30000;
    std::uint64_t frames=0,rawChanges=0,positiveSamples=0,releases=0,valueChanges=0;
    unsigned maxSimultaneous=0,observedMilestones=0;
    std::array<std::uint16_t,100> previous{};
    const auto summary=[&] {
        char line[512]{};
        _snprintf_s(line,sizeof(line),_TRUNCATE,
            "evidence.corsair.travel revision=2 attempt=%llu frames=%llu raw_changed_slots=%llu positive_samples=%llu value_changes=%llu release_transitions=%llu max_simultaneous=%u elapsed_ms=%llu target_hz=100 measured_hz_unknown=1",
            g_attempt.load(),frames,rawChanges,positiveSamples,valueChanges,releases,maxSimultaneous,GetTickCount64()-sessionStarted);
        SupportLog_Evidence(line,false);
    };
    while(!g_stop.load() && topology==g_topology.load()) {
        const auto began=GetTickCount64();
        std::array<std::uint16_t,100> values{};
        if(!client.Read(values)) {
            if(!g_stop.load() && topology==g_topology.load())Failure(cv::Stage::Sampling,io,&client);
            break;
        }
        if(!live) {
            if(!NativeAnalogRouting_Claim(cv::kVid,cv::kPid,path.c_str(),NativeAnalogProtocol::CorsairVanguard) &&
               !NativeAnalogRouting_IsClaimedBy(path.c_str(),NativeAnalogProtocol::CorsairVanguard)){CorsairEvent("corsair.routing_claim_failed",1);break;}
        }
        const auto now=GetTickCount64();
        bool changed=false;unsigned active=0;++frames;rawChanges+=client.rawChanged;
        for(unsigned i=0;i<layout->count;++i)if(layout->hid[i]) {
            active+=values[i]>0;valueChanges+=previous[i]!=values[i];
            releases+=previous[i]>0 && values[i]==0;previous[i]=values[i];
            changed=g_values.Publish(static_cast<std::uint8_t>(i+1),values[i],now) || changed;
        }
        positiveSamples+=active;maxSimultaneous=std::max(maxSimultaneous,active);
        // Retain useful evidence even when Open log is requested after a
        // short test. Each aggregate milestone can emit only once per session.
        const unsigned milestones=1u|(rawChanges?2u:0u)|(positiveSamples?4u:0u)|
            (releases?8u:0u)|(maxSimultaneous>=2?16u:0u);
        const bool newMilestone=(milestones&~observedMilestones)!=0;
        observedMilestones=milestones;
        if(newMilestone || now>=nextSummary){summary();nextSummary=now+30000;}
        g_last.store(now);++g_good;
        g_connected.store(true,std::memory_order_release);
        if(!live) {live=true;CorsairEvent("corsair.travel_started",layout->id,SupportLog_Data(client.profile.mapped));}
        if(changed)RealtimeLoop_NotifyInputChanged();
        const auto elapsed=GetTickCount64()-began;
        if(elapsed<10)WaitForSingleObject(g_wake,static_cast<DWORD>(10-elapsed));
    }
    summary();cleanup();return !live && !g_stop.load() && topology==g_topology.load();
}
unsigned __stdcall Worker(void*) noexcept {
    std::vector<std::wstring> rejected;
    try {
        while(!g_stop.load()) {
            Discovery scan{};const auto devices=Enumerate(scan);g_present.store(scan.products!=0);
            if(!scan.errors)rejected.erase(std::remove_if(rejected.begin(),rejected.end(),[&](const auto& p){return std::find(devices.begin(),devices.end(),p)==devices.end();}),rejected.end());
            if(devices.size()==1) {
                const auto& path=devices.front();
                if(std::find(rejected.begin(),rejected.end(),path)==rejected.end())
                    if(Run(path)){rejected.push_back(path);CorsairEvent("corsair.retry_policy",1);}
            } else {g_busy.store(false);}
            // Metadata API failures are retryable, unlike rejected protocol proof.
            if(!g_stop.load())WaitForSingleObject(g_wake,devices.empty() && !scan.errors?INFINITE:1000);

        }
    } catch(...) {++g_bad;CorsairEvent("corsair.worker_exception",1);}
    Clear();g_busy.store(false);g_present.store(false);g_running.store(false);return 0;
}
bool Start() {
    std::lock_guard<std::mutex> lock(g_service);
    if(g_thread)return g_running.load();
    g_stop.store(false);g_stage.store(cv::Stage::Idle);g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!g_wake)return false;
    CorsairEvent("corsair.start",2,SupportLog_Data(0x00ab0802));
    g_running.store(true);unsigned id=0;
    g_thread=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Worker,nullptr,0,&id));
    if(!g_thread){g_running.store(false);CloseHandle(g_wake);g_wake=nullptr;return false;}
    return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard<std::mutex> lock(g_service);g_stop.store(true);
    if(!g_thread){Clear();return NativeAnalogBackendStopJoined(generation);}
    SetEvent(g_wake);const auto wait=WaitForSingleObject(g_thread,3000);
    if(wait!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,
        halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,wait==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError());
    CloseHandle(g_thread);g_thread=nullptr;CloseHandle(g_wake);g_wake=nullptr;
    Clear();return NativeAnalogBackendStopJoined(generation);
}
void Notify(){++g_topology;std::unique_lock<std::mutex> lock(g_service,std::try_to_lock);if(lock.owns_lock() && g_wake)SetEvent(g_wake);}
bool Present(){return g_present.load();}
bool Connected(){return g_connected.load(std::memory_order_acquire);}
bool Owns(std::uint16_t hid){return Connected() && g_values.Owns(hid);}
std::uint16_t Get(std::uint16_t hid){return Owns(hid)?g_values.Read(hid,GetTickCount64(),100).milli:0;}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if(!out)return;*out={};out->present=Present();out->connected=Connected();
    out->accessBusy=out->present && !out->connected && g_busy.load();
    out->vendorId=cv::kVid;out->productId=cv::kPid;
    out->usagePage=static_cast<std::uint16_t>(g_page.load());out->usage=static_cast<std::uint16_t>(g_usage.load());
    out->inputReportBytes=g_inputBytes.load();out->outputReportBytes=g_outputBytes.load();
    out->nominalRawLevels=41;out->successfulUpdates=g_good.load();out->failedUpdates=g_bad.load();
    const auto now=GetTickCount64(),last=g_last.load();
    out->lastUpdateAgeMs=last?static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff,now-last)):0;
    const auto* layout=g_layout.load();
    if(out->connected && layout) {
        out->verifiedLayoutToken=VanguardLayoutToken(layout->id);
        out->mappedKeys=layout->id==1?96:layout->id==2?97:98;
        out->activeKeys=g_values.Active(now);
    }
    wcscpy_s(out->deviceName,L"CORSAIR VANGUARD PRO 96");
    swprintf_s(out->status,L"VANGUARD PRO 96: %ls; stage=%u; see HallJoy.log",
        out->connected?L"stock analog; 0.1 mm steps":out->accessBusy?L"close the other keyboard app":
        out->present?L"checking firmware/layout/calibration":L"not present",static_cast<unsigned>(g_stage.load()));
}
}
const NativeAnalogBackendDescriptor& CorsairVanguard_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor descriptor{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "corsair_vanguard",L"CORSAIR VANGUARD PRO 96 (experimental stock analog)",NativeAnalogProtocol::CorsairVanguard,
        NativeAnalogStartPhase::AfterRealtime,NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};
    return descriptor;
}
