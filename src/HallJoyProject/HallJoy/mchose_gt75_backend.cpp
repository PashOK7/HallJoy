#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off: Windows HID dependency order.
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <devpkey.h>
// clang-format on
#include "mchose_gt75_backend.h"
#include "mchose_gt75_client.h"
#include "bindings.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "native_layout_state.h"
#include "support_log.h"
#include "input_trace.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <process.h>
#include <shellapi.h>
#include <thread>
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")
namespace {
namespace f75=halljoy::gt75;
std::mutex serviceMutex,activeMutex,dataMutex;
HANDLE worker=nullptr,wake=nullptr,cancelEvent=nullptr,active=INVALID_HANDLE_VALUE;
std::atomic<bool> stop{false},running{false},present{false},connected{false},collision{false};
std::atomic<std::uint64_t> topology{0},token{0};
std::atomic<bool> accessBusy{false};
halljoy::physical_analog::Publication factoryValues,assignedValues;
f75::Assignments assignments=f75::Factory;
f75::Candidate liveCandidate;
struct Stats {
    unsigned vid=0,pid=0,page=0,input=0,output=0,firmware=0,infoVid=0,infoPid=0;
    unsigned precision=0,frame=0,route=0,metadata=0,mapped=0,maximumActive=0;
    unsigned rawPeak=0,adcMin=65535,adcMax=0,strokeMin=65535,strokeMax=0,meanUs=0,maxUs=0;
    std::uint64_t queries=0,replies=0,statusQueries=0,batches=0,samples=0,releases=0,changes=0,ignored=0;
    std::uint64_t monitorOn=0,monitorOff=0,lastUpdate=0;
    std::array<unsigned,128> last{};
    std::array<bool,128> seen{},positive{},released{};
    std::array<std::uint64_t,2> fingerprints{};
    std::chrono::steady_clock::time_point batchStamp{};
    bool identity=false,firstZero=false;
} stats;
void Record(const char* stage,const char* reason,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0) noexcept {
    char text[448]{};
    sprintf_s(text,"evidence.mchose_gt75.stage revision=1 stage=%s reason=%s a=%u b=%u c=%u d=%u",stage,reason,a,b,c,d);
    SupportLog_Evidence(text,false);
}
void Evidence() noexcept {
    Stats s;{std::lock_guard<std::mutex> lock(dataMutex);s=stats;}
    if(!s.vid && !s.queries)return;
    unsigned seen=0,positive=0,released=0,held=0;
    for(unsigned i=0;i<128;++i){seen+=s.seen[i];positive+=s.positive[i];released+=s.released[i];held+=s.last[i]>0;}
    char text[1024]{};
    sprintf_s(text,"evidence.mchose_gt75.current revision=1 vid=%04X pid=%04X page=%04X input=%u output=%u identity=%u firmware_bcd=%04X mapped=%u queries=%llu replies=%llu settings_reads=%llu flag_write_attempts=%llu connected=%u no_analog_yet=%u factory_assignments=1",
        s.vid,s.pid,s.page,s.input,s.output,s.identity,s.firmware,s.mapped,s.queries,s.replies,s.statusQueries,s.monitorOn,connected.load()?1:0,s.samples?0:1);
    SupportLog_Evidence(text,false);
    sprintf_s(text,"evidence.mchose_gt75.depth revision=1 samples=%llu observed_channels=%u positive_channels=%u released_channels=%u release_events=%llu changes=%llu held_last=%u max_activity=%u raw_peak=%u declared_max_min=%u declared_max_max=%u first_sample_zero=%u mean_interval_us=%u max_interval_us=%u physical_channels=81 fn_included=1 source=firmware_a0 source_cutoff_le9=1 source_bottom_snap5=1 release_observation_pending=%u",
        s.samples,seen,positive,released,s.releases,s.changes,held,s.maximumActive,s.rawPeak,s.strokeMin==65535?0:s.strokeMin,s.strokeMax,s.firstZero,s.meanUs,s.maxUs,(!positive || released<positive)?1:0);
    SupportLog_Evidence(text,false);
}
void Neutral() noexcept {
    connected=false;factoryValues.Clear();assignedValues.Clear();
}
void Clear() noexcept {
    Neutral();halljoy::native_layout::Clear(token.exchange(0));
}
bool Remapped() {return halljoy::native_layout::UsesRemapping(token.load());}
struct Handle {
    HANDLE h=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value=INVALID_HANDLE_VALUE):h(value){}
    ~Handle(){if(h && h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    explicit operator bool()const{return h && h!=INVALID_HANDLE_VALUE;}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::vector<f75::Candidate> Enumerate(bool log,const f75::Candidate* retained=nullptr,bool* blocked=nullptr) {
    if(blocked)*blocked=false;
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO devices=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(devices==INVALID_HANDLE_VALUE)return {};
    std::vector<f75::Candidate> candidates;
    for(DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(devices,nullptr,&guid,i,&iface))break;
        DWORD bytes=0;SetupDiGetDeviceInterfaceDetailW(devices,&iface,nullptr,0,&bytes,nullptr);
        if(bytes<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))continue;
        std::vector<std::uint8_t> memory(bytes);
        auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(memory.data());detail->cbSize=sizeof(*detail);
        SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);
        if(!SetupDiGetDeviceInterfaceDetailW(devices,&iface,detail,bytes,nullptr,&dev))continue;
        const auto path=f75::Lower(detail->DevicePath);
        const bool usbPath=path.find(L"vid_3837&pid_302d")!=std::wstring::npos;
        if(!usbPath)continue;
        Handle meta(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        if(!meta) {
            // A held exclusive session can deny a second metadata handle. The
            // present SetupDi interface path still proves it has not left USB.
            if(retained && retained->path==detail->DevicePath)candidates.push_back(*retained);
            else if(usbPath) {
                const auto error=GetLastError();if(blocked)*blocked=true;
                accessBusy=error==ERROR_ACCESS_DENIED || error==ERROR_SHARING_VIOLATION;
                if(log)Record("metadata_open","failed_retry_without_commands",error,1,0);
            }
            continue;
        }
        HIDD_ATTRIBUTES attr{};attr.Size=sizeof(attr);
        if(!HidD_GetAttributes(meta.h,&attr) || (attr.VendorID!=0x3837 || attr.ProductID!=0x302d))continue;
        f75::Candidate c;c.path=detail->DevicePath;c.vid=attr.VendorID;c.pid=attr.ProductID;
        wchar_t name[256]{};const bool product=HidD_GetProductString(meta.h,name,sizeof(name))!=FALSE;
        if(product)c.name=name;
        PHIDP_PREPARSED_DATA pp=nullptr;HIDP_CAPS caps{};
        const bool parsed=HidD_GetPreparsedData(meta.h,&pp)!=FALSE;
        NTSTATUS status=parsed?HidP_GetCaps(pp,&caps):0;
        if(pp)HidD_FreePreparsedData(pp);
        c.page=caps.UsagePage;c.usage=caps.Usage;c.inputBytes=caps.InputReportByteLength;c.outputBytes=caps.OutputReportByteLength;
        if(log) {
            Record("collection_identity",f75::IdentityCandidate(c)?"model_match":"different_or_unknown_receiver",c.vid,c.pid,product?1:0,0);
            Record("collection_caps",parsed && status==HIDP_STATUS_SUCCESS?"observed":"unavailable",c.page,c.usage,c.inputBytes,c.outputBytes);
        }
        if(parsed && status==HIDP_STATUS_SUCCESS && f75::Collection(c))candidates.push_back(std::move(c));
    }
    SetupDiDestroyDeviceInfoList(devices);f75::PreferWired(candidates);return candidates;
}
class HidTransport {
public:
    using Inventory=std::vector<f75::Candidate>(*)(const f75::Candidate*);
private:
    HANDLE handle_=INVALID_HANDLE_VALUE;
    DWORD error_=0;
    std::uint64_t generation_;
    f75::Candidate candidate_;
    Inventory inventory_=nullptr;
    bool switch_=false,cleanup_=false;
    bool Refresh() {
        const auto generation=topology.load();
        if(generation==generation_)return !switch_;
        const auto candidates=inventory_?inventory_(&candidate_):Enumerate(false,&candidate_);
        generation_=generation;switch_=f75::NeedsSwitch(candidate_,candidates);
        if(switch_)Record("transport","topology_requires_switch",candidate_.vid,candidate_.pid,unsigned(candidates.size()));
        return !switch_;
    }
    bool Transfer(bool write,void* data,unsigned length,unsigned timeout,unsigned* bytes=nullptr,bool cleanup=false) {
        cleanup=cleanup || cleanup_;
        if(!cleanup && (!Refresh() || Stopping())){error_=ERROR_OPERATION_ABORTED;return false;}
        HidIoOperation op(handle_);DWORD done=0,error=0;
        const auto started=write?op.StartWrite(data,length,&error):op.StartRead(data,length,&error);
        if(started==HidIoOperation::StartResult::Failed){error_=error;return false;}
        if(started==HidIoOperation::StartResult::Pending) {
            const auto deadline=GetTickCount64()+timeout;
            for(;;) {
                const HANDLE waits[]={op.Event(),cancelEvent,wake};
                const auto now=GetTickCount64();
                const auto remaining=timeout==INFINITE?INFINITE:now>=deadline?0:DWORD(deadline-now);
                const auto result=cancelEvent && !cleanup?WaitForMultipleObjects(wake?3:2,waits,FALSE,remaining):op.Wait(remaining);
                if(result==WAIT_OBJECT_0)break;
                if(result==WAIT_OBJECT_0+2 && Refresh() && !Stopping())continue;
                op.CancelAndDrain(&done,&error);
                error_=(result==WAIT_OBJECT_0+1 || result==WAIT_OBJECT_0+2)?ERROR_OPERATION_ABORTED:result==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError();
                return false;
            }
        }
        if(!op.Finish(&done,&error,false)){error_=error;return false;}
        if(bytes)*bytes=done;
        if(write && done!=length){error_=ERROR_INVALID_DATA;return false;}
        error_=ERROR_SUCCESS;return true;
    }
    unsigned input_=0;
public:
    explicit HidTransport(std::uint64_t generation,Inventory inventory=nullptr):generation_(generation),inventory_(inventory){}
    ~HidTransport(){Close();}
    bool Open(const f75::Candidate& c) {
        input_=c.inputBytes;candidate_=c;
        handle_=CreateFileW(c.path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        error_=handle_==INVALID_HANDLE_VALUE?GetLastError():0;
        accessBusy=error_==ERROR_SHARING_VIOLATION || error_==ERROR_ACCESS_DENIED;
        if(handle_==INVALID_HANDLE_VALUE)return false;
        std::lock_guard<std::mutex> lock(activeMutex);active=handle_;return true;
    }
    bool Send(const f75::Report& frame,unsigned bytes,unsigned timeout) {
        auto copy=frame;return Transfer(true,copy.data(),bytes,timeout,nullptr,cleanup_);
    }
    bool Receive(f75::Report& frame,unsigned& bytes,unsigned timeout) {
        return Transfer(false,frame.data(),input_,timeout,&bytes);
    }
    void Close() noexcept {
        std::lock_guard<std::mutex> lock(activeMutex);
        if(active==handle_)active=INVALID_HANDLE_VALUE;
        if(handle_!=INVALID_HANDLE_VALUE){CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;}
    }
    void CleanupMode(bool value){cleanup_=value;}
    bool Stopping()const{return stop.load() || collision.load() || switch_;}
    std::uint64_t Now()const{return GetTickCount64();}
    unsigned Error()const{return error_;}
};
struct Output {
    void Reset() {
        Clear();std::lock_guard<std::mutex> lock(dataMutex);stats={};
    }
    void Event(const char* stage,const char* reason,unsigned a,unsigned b,unsigned c,unsigned d) noexcept {Record(stage,reason,a,b,c,d);}
    void Identity(const f75::Candidate& c,const f75::Info& info) {
        liveCandidate=c;token=f75::Token(c);
        std::lock_guard<std::mutex> lock(dataMutex);
        stats.identity=true;stats.vid=c.vid;stats.pid=c.pid;stats.page=c.page;stats.input=c.inputBytes;stats.output=c.outputBytes;
        stats.firmware=info.version;stats.infoVid=c.vid;stats.infoPid=c.pid;
    }
    void Map(const f75::Assignments& map,bool metadata) {
        assignments=map;factoryValues.Clear();assignedValues.Clear();
        std::array<halljoy::native_layout::Key,128> keys{};unsigned count=0;
        for(unsigned i=0;i<128;++i)if(f75::Factory[i]) {
            const auto id=std::uint8_t(i+1);
            factoryValues.Bind(id,f75::Factory[i]);
            if(map[i])assignedValues.Bind(id,map[i]);
            keys[count++]={f75::Factory[i],map[i]};
        }
        const bool cached=halljoy::native_layout::Publish(token.load(),keys.data(),count);
        Record("layout",cached?"cache_published":"cache_unavailable_continue",count);
        std::lock_guard<std::mutex> lock(dataMutex);stats.metadata=metadata?1:0;stats.mapped=count;
    }
    void Sample(unsigned slot,unsigned raw,unsigned adc,unsigned stroke,unsigned route) {
        if(!connected.load()) {
            if(!NativeAnalogRouting_Claim(std::uint16_t(liveCandidate.vid),std::uint16_t(liveCandidate.pid),liveCandidate.path.c_str(),NativeAnalogProtocol::MchoseGt75) &&
               !NativeAnalogRouting_IsClaimedBy(liveCandidate.path.c_str(),NativeAnalogProtocol::MchoseGt75)) {
                Record("routing","ownership_collision");collision=true;return;
            }
            connected=true;Record("depth","first_valid_sample",route,raw==0?1:0);
        }
        const auto milli=std::uint16_t(f75::Milli(raw,stroke));const auto now=GetTickCount64();
        factoryValues.Publish(std::uint8_t(slot+1),milli,now,true);
        assignedValues.Publish(std::uint8_t(slot+1),milli,now,true);
        const auto hid=Remapped()?assignments[slot]:f75::Factory[slot];
        if(hid)InputTrace_Source("mchose-gt75",0,slot,raw,milli,hid);
        std::lock_guard<std::mutex> lock(dataMutex);
        if(!stats.samples)stats.firstZero=raw==0;
        if(stats.seen[slot] && stats.last[slot]!=raw)++stats.changes;
        if(stats.seen[slot] && stats.last[slot] && !raw){++stats.releases;stats.released[slot]=true;}
        stats.seen[slot]=true;stats.positive[slot]=stats.positive[slot]||raw>0;stats.last[slot]=raw;
        ++stats.samples;stats.route=route;stats.rawPeak=std::max(stats.rawPeak,raw);
        stats.strokeMin=std::min(stats.strokeMin,stroke);stats.strokeMax=std::max(stats.strokeMax,stroke);
        stats.adcMin=std::min(stats.adcMin,adc);stats.adcMax=std::max(stats.adcMax,adc);stats.lastUpdate=now;
        unsigned held=0;for(auto v:stats.last)held+=v>0;stats.maximumActive=std::max(stats.maximumActive,held);
    }
    bool Bound(unsigned slot)const {
        const auto hid=Remapped()?assignments[slot]:f75::Factory[slot];
        return hid && Bindings_IsHidBound(hid);
    }
    void Requested(unsigned cmd) {
        std::lock_guard<std::mutex> lock(dataMutex);++stats.queries;
        stats.statusQueries+=cmd==5;stats.monitorOn+=cmd==6;
    }
    void Received(unsigned,unsigned) {std::lock_guard<std::mutex> lock(dataMutex);++stats.replies;}
    void IgnoredSlot() {std::lock_guard<std::mutex> lock(dataMutex);++stats.ignored;}
    void Batch(unsigned,bool) {
        std::lock_guard<std::mutex> lock(dataMutex);++stats.batches;
        const auto now=std::chrono::steady_clock::now();
        if(stats.batchStamp.time_since_epoch().count()) {
            const auto us=unsigned(std::max<std::int64_t>(1,std::chrono::duration_cast<std::chrono::microseconds>(now-stats.batchStamp).count()));
            stats.meanUs=stats.meanUs?(stats.meanUs*7+us)/8:us;stats.maxUs=std::max(stats.maxUs,us);
        }
        stats.batchStamp=now;
    }
    void Fingerprint(unsigned index,std::uint64_t hash) {
        std::lock_guard<std::mutex> lock(dataMutex);stats.fingerprints[index]=hash;
    }
    void Neutral() noexcept {::Neutral();}
    void Closed() noexcept {Evidence();Clear();}
};
bool Prepare() {
    SupportLog_SetMchoseGt75EvidenceProvider(&Evidence);
    bool blocked=false;const auto candidates=Enumerate(true,nullptr,&blocked);present=!candidates.empty() || blocked;return present.load();
}
unsigned __stdcall Worker(void*) {
    try {
        std::vector<std::wstring> declined,written;
        while(!stop.load()) {
            const auto generation=topology.load();
            bool blocked=false;const auto candidates=Enumerate(false,nullptr,&blocked);present=!candidates.empty() || blocked;
            declined.erase(std::remove_if(declined.begin(),declined.end(),[&](const auto& path){
                return std::none_of(candidates.begin(),candidates.end(),[&](const auto& c){return c.path==path;});
            }),declined.end());
            written.erase(std::remove_if(written.begin(),written.end(),[&](const auto& path){
                return std::none_of(candidates.begin(),candidates.end(),[&](const auto& c){return c.path==path;});
            }),written.end());
            for(const auto& c:candidates) {
                if(stop.load() || generation!=topology.load())break;
                if(std::find(declined.begin(),declined.end(),c.path)!=declined.end())continue;
                if(NativeAnalogRouting_IsClaimed(c.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::MchoseGt75))continue;
                collision=false;HidTransport io(generation);Output output;f75::Client<HidTransport,Output> client(io,output,std::find(written.begin(),written.end(),c.path)==written.end());
                if(client.Run(c)==f75::Result::Foreign)declined.push_back(c.path);
                if(client.WasWritten() && std::find(written.begin(),written.end(),c.path)==written.end())written.push_back(c.path);
            }
            if(!stop.load() && generation==topology.load() && wake)WaitForSingleObject(wake,candidates.empty() && !blocked?INFINITE:1000);
        }
    }catch(...){Record("worker","exception_input_cleared");}
    Clear();running=false;return 0;
}
bool Start() {
    std::lock_guard<std::mutex> lock(serviceMutex);
    if(worker)return running.load();
    stop=false;collision=false;SupportLog_SetMchoseGt75EvidenceProvider(&Evidence);
    wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake)return false;
    cancelEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!cancelEvent){CloseHandle(wake);wake=nullptr;return false;}
    running=true;unsigned id=0;worker=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Worker,nullptr,0,&id));
    if(!worker){running=false;CloseHandle(wake);wake=nullptr;CloseHandle(cancelEvent);cancelEvent=nullptr;return false;}return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard<std::mutex> lock(serviceMutex);stop=true;
    if(wake)SetEvent(wake);
    if(cancelEvent)SetEvent(cancelEvent);
    {std::lock_guard<std::mutex> activeLock(activeMutex);if(active!=INVALID_HANDLE_VALUE)CancelIoEx(active,nullptr);}
    if(worker) {
        const auto result=WaitForSingleObject(worker,10000);
        if(result!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,result==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError());
        CloseHandle(worker);worker=nullptr;
    }
    if(wake){CloseHandle(wake);wake=nullptr;}
    if(cancelEvent){CloseHandle(cancelEvent);cancelEvent=nullptr;}
    Clear();SupportLog_SetMchoseGt75EvidenceProvider(nullptr);
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
    ++topology;std::lock_guard<std::mutex> lock(serviceMutex);
    if(wake)SetEvent(wake);
    // The I/O worker checks which collection changed. An unrelated mouse/pad
    // arrival must not clear held keyboard depths or restart a healthy session.
}
bool Present(){return present.load();}
bool Connected(){return connected.load();}
bool Owns(std::uint16_t hid){return connected.load() && (Remapped()?assignedValues:factoryValues).Owns(hid);}
std::uint16_t Get(std::uint16_t hid) {return Owns(hid)?(Remapped()?assignedValues:factoryValues).Read(hid,GetTickCount64(),500,true).milli:0;}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if(!out)return;*out={};Stats s;{std::lock_guard<std::mutex> lock(dataMutex);s=stats;}
    out->present=present.load();out->connected=connected.load();out->verifiedLayoutToken=token.load();
    out->accessBusy=accessBusy.load();
    out->vendorId=std::uint16_t(s.vid);out->productId=std::uint16_t(s.pid);out->usagePage=std::uint16_t(s.page);out->usage=0;
    out->mappedKeys=s.mapped;
    if(out->connected)for(auto raw:s.last)out->activeKeys+=raw>0;
    out->nominalRawLevels=s.strokeMax+1;
    out->inputReportBytes=s.input;out->outputReportBytes=s.output;out->successfulUpdates=s.samples;
    out->averageIntervalUs=s.meanUs;out->maximumIntervalUs=s.maxUs;out->updateHz10=s.meanUs?10000000/s.meanUs:0;
    const auto now=GetTickCount64();out->lastUpdateAgeMs=s.lastUpdate && now>=s.lastUpdate?unsigned(std::min<std::uint64_t>(0xffffffffull,now-s.lastUpdate)):0;
    // This compatibility trial is not an entry in the confirmed model catalog.
    out->genericProtocol=out->connected;
    wcscpy_s(out->deviceName,L"MCHOSE GT75");
    wcscpy_s(out->status,out->connected?L"MCHOSE GT75: analog trial; hardware validation pending":s.identity?L"MCHOSE GT75 identified; waiting for analog data":L"MCHOSE GT75: searching USB");
}
#include "mchose_gt75_self_test.h"
} // namespace
const NativeAnalogBackendDescriptor& MchoseGt75_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "mchose-gt75",L"MCHOSE GT75 USB (trial)",NativeAnalogProtocol::MchoseGt75,NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReversibleControlProbe,
        &Prepare,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry,nullptr,
        +[](halljoy::observation::Sink sink){(Remapped()?assignedValues:factoryValues).Observations(sink);}};
    return d;
}
bool MchoseGt75_TryRunSelfTest(int& result) noexcept {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    const bool selected=argv && argc==2 && wcscmp(argv[1],L"--halljoy-mchose-gt75-self-test")==0;
    if(argv)LocalFree(argv);
    if(!selected)return false;
    bool pass=false;try {pass=Gt75SelfTest();}catch(...){pass=false;}
    printf("MCHOSE_GT75_SELF_TEST=%s scenarios=18 negatives=9 physical_channels=81 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hotplug_cancel=1 unrelated_topology=1 hardware_opened=0\n",pass?"PASS":"FAIL");
    result=pass?0:10;return true;
}
