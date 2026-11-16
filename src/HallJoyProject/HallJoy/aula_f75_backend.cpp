#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off: Windows HID dependency order.
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <devpkey.h>
// clang-format on
#include "aula_f75_backend.h"
#include "aula_f75_client.h"
#include "bindings.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "native_layout_state.h"
#include "support_log.h"
#include "input_trace.h"
#include "sonix_trial_evidence.h"
#include "boom68_fixture.h"
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
namespace f75=halljoy::f75;
std::mutex serviceMutex,activeMutex,dataMutex;
HANDLE worker=nullptr,wake=nullptr,cancelEvent=nullptr,active=INVALID_HANDLE_VALUE;
std::atomic<bool> stop{false},running{false},present{false},connected{false},collision{false};
std::atomic<std::uint64_t> topology{0},token{0};
std::atomic<std::uint32_t> referenceIdentity{0},referenceGeneration{0};
std::atomic<bool> accessBusy{false},boomMode{false};
halljoy::physical_analog::Publication factoryValues,assignedValues;
f75::Assignments assignments=f75::Factory;
f75::Candidate liveCandidate;
struct Stats {
    unsigned vid=0,pid=0,page=0,input=0,output=0,firmware=0,infoVid=0,infoPid=0;
    unsigned precision=0,frame=0,route=0,metadata=0,mapped=0,maximumActive=0;
    unsigned rawPeak=0,adcMin=65535,adcMax=0,strokeMin=65535,strokeMax=0,meanUs=0,maxUs=0;
    std::uint64_t queries=0,replies=0,statusQueries=0,batches=0,samples=0,releases=0,changes=0,ignored=0;
    std::uint64_t monitorOn=0,monitorOff=0,lastUpdate=0;
    unsigned long long referenceDowns=0,referenceUps=0;
    unsigned referenceHeld=0,referencePeakHeld=0;
    std::array<bool,256> referenceKeys{};
    std::array<unsigned long long,static_cast<std::size_t>(f75::MonitorFrame::Count)> monitorFrames{};
    std::array<unsigned,128> last{};
    std::array<bool,128> seen{},positive{},released{};
    std::array<std::uint64_t,2> fingerprints{};
    std::chrono::steady_clock::time_point batchStamp{};
    bool identity=false,firstZero=false;
} stats;
void Record(const char* stage,const char* reason,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0, bool boom=false) noexcept {
    char text[448]{};
    sprintf_s(text,"evidence.%s.stage revision=2 stage=%s reason=%s a=%u b=%u c=%u d=%u",(boom || boomMode.load())?"boom68":"aula_f75",stage,reason,a,b,c,d);
    SupportLog_Evidence(text,false);
}
void Evidence() noexcept {
    Stats s;{std::lock_guard<std::mutex> lock(dataMutex);s=stats;}
    if(!s.vid && !s.queries)return;
    unsigned seen=0,positive=0,released=0,held=0;
    for(unsigned i=0;i<128;++i){seen+=s.seen[i];positive+=s.positive[i];released+=s.released[i];held+=s.last[i]>0;}
    EmitSonixTrialEvidence(s,connected.load(),seen,positive,released,held,[](const char* text){SupportLog_Evidence(text,false);});
    if(s.monitorOn && (f75::Wired(s.vid,s.pid) || (s.vid==0x0c45 && s.pid==0xfefe))) {
        const auto count=[&](f75::MonitorFrame frame){return s.monitorFrames[static_cast<std::size_t>(frame)];};
        char line[384]{};
        sprintf_s(line,"evidence.aula_f75.monitor revision=3 ack=%llu depth=%llu invalid_slot=%llu invalid_range=%llu",
            count(f75::MonitorFrame::Ack),count(f75::MonitorFrame::Depth),count(f75::MonitorFrame::InvalidSlot),
            count(f75::MonitorFrame::InvalidRange));
        SupportLog_Evidence(line,false);
        sprintf_s(line,"evidence.aula_f75.other_frames revision=3 other_opcode=%llu other_header=%llu framing=%llu disconnect=%llu",
            count(f75::MonitorFrame::OtherOpcode),count(f75::MonitorFrame::OtherHeader),
            count(f75::MonitorFrame::Framing),count(f75::MonitorFrame::Disconnect));
        SupportLog_Evidence(line,false);
    }
    if(f75::Wired(s.vid,s.pid) || (s.vid==0x0c45 && s.pid==0xfefe)) {
        char line[256]{};
        sprintf_s(line,"evidence.aula_f75.digital_reference revision=3 enabled=%u downs=%llu ups=%llu held=%u max_simultaneous=%u analog_samples=%llu reference_only=1",
            f75::Wired(s.vid,s.pid)?1u:0u,s.referenceDowns,s.referenceUps,s.referenceHeld,s.referencePeakHeld,s.samples);
        SupportLog_Evidence(line,false);
    }
}
void Neutral() noexcept {
    connected=false;factoryValues.Clear();assignedValues.Clear();
}
void Clear() noexcept {
    referenceIdentity=0;++referenceGeneration;
    Neutral();halljoy::native_layout::Clear(token.exchange(0));
}
void RecordDigitalReference(unsigned hid,bool down,std::uint32_t identity,std::uint32_t generation) {
    std::lock_guard<std::mutex> lock(dataMutex);
    if(!identity || identity!=referenceIdentity.load() || generation!=referenceGeneration.load())return;
    if(hid>=stats.referenceKeys.size() || stats.referenceKeys[hid]==down)return;
    stats.referenceKeys[hid]=down;
    if(down) {++stats.referenceDowns;++stats.referenceHeld;}
    else {++stats.referenceUps;--stats.referenceHeld;}
    stats.referencePeakHeld=std::max(stats.referencePeakHeld,stats.referenceHeld);
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
        const bool usbPath=path.find(L"vid_0c45&pid_80b5")!=std::wstring::npos ||
            (f75::kStockF75TrialEnabled && (path.find(L"vid_38a6&pid_2729")!=std::wstring::npos || path.find(L"vid_38a6&pid_2742")!=std::wstring::npos));
        const bool radioPath=f75::kStockF75TrialEnabled && path.find(L"vid_0c45&pid_fefe")!=std::wstring::npos;
        if(!usbPath && !radioPath)continue;
        wchar_t busName[256]{};DEVPROPTYPE property=0;DWORD required=0;
        const bool bus=SetupDiGetDevicePropertyW(devices,&dev,&DEVPKEY_Device_BusReportedDeviceDesc,&property,
            reinterpret_cast<PBYTE>(busName),sizeof(busName),&required,0)!=FALSE;
        const bool exactBus=bus && property==DEVPROP_TYPE_STRING && f75::Lower(busName)==L"aula f75 he dongle";
        Handle meta(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        if(!meta) {
            // A held exclusive session can deny a second metadata handle. The
            // present SetupDi interface path still proves it has not left USB.
            if(retained && retained->path==detail->DevicePath)candidates.push_back(*retained);
            else if(usbPath || exactBus) {
                const auto error=GetLastError();if(blocked)*blocked=true;
                accessBusy=error==ERROR_ACCESS_DENIED || error==ERROR_SHARING_VIOLATION;
                if(log)Record("metadata_open","failed_retry_without_commands",error,usbPath?1:0,exactBus?1:0);
            }
            continue;
        }
        HIDD_ATTRIBUTES attr{};attr.Size=sizeof(attr);
        if(!HidD_GetAttributes(meta.h,&attr) || (!f75::Wired(attr.VendorID,attr.ProductID) && !(attr.VendorID==0x0c45 && (attr.ProductID==0xfefe || attr.ProductID==0x80b5))))continue;
        f75::Candidate c;c.path=detail->DevicePath;c.vid=attr.VendorID;c.pid=attr.ProductID;c.version=attr.VersionNumber;
        wchar_t name[256]{};const bool product=HidD_GetProductString(meta.h,name,sizeof(name))!=FALSE;
        if(product)c.name=name;
        if(exactBus || (bus && property==DEVPROP_TYPE_STRING && f75::Lower(busName)==L"boom68-usb" && c.pid==0x80b5 && c.vid==0x0c45))c.name=busName;
        PHIDP_PREPARSED_DATA pp=nullptr;HIDP_CAPS caps{};
        const bool parsed=HidD_GetPreparsedData(meta.h,&pp)!=FALSE;
        NTSTATUS status=parsed?HidP_GetCaps(pp,&caps):0;
        if(pp)HidD_FreePreparsedData(pp);
        c.page=caps.UsagePage;c.usage=caps.Usage;c.inputBytes=caps.InputReportByteLength;c.outputBytes=caps.OutputReportByteLength;
        if(log) {
            Record("collection_identity",f75::IdentityCandidate(c)?"model_match":"foreign_or_unknown_product",c.vid,c.pid,product?1:0,bus?1:0,c.vid==0x0c45 && c.pid==0x80b5);
            Record("collection_caps",!parsed || status!=HIDP_STATUS_SUCCESS?"unavailable":f75::Collection(c)?"eligible":"wrong_framing",c.page,c.usage,c.inputBytes,c.outputBytes,c.vid==0x0c45 && c.pid==0x80b5);
        }
        if(parsed && status==HIDP_STATUS_SUCCESS && f75::RuntimeCollection(c))candidates.push_back(std::move(c));
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
    bool switch_=false;
    bool Refresh() {
        const auto generation=topology.load();
        if(generation==generation_)return !switch_;
        const auto candidates=inventory_?inventory_(&candidate_):Enumerate(false,&candidate_);
        generation_=generation;switch_=f75::NeedsSwitch(candidate_,candidates);
        if(switch_)Record("transport","topology_requires_switch",candidate_.vid,candidate_.pid,unsigned(candidates.size()));
        return !switch_;
    }
    bool Transfer(bool write,void* data,unsigned length,unsigned timeout,unsigned* bytes=nullptr,bool cleanup=false) {
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
        auto copy=frame;return Transfer(true,copy.data(),bytes,timeout,nullptr,frame[2]==0x67);
    }
    bool Receive(f75::Report& frame,unsigned& bytes,unsigned timeout) {
        return Transfer(false,frame.data(),input_,timeout,&bytes);
    }
    void Close() noexcept {
        std::lock_guard<std::mutex> lock(activeMutex);
        if(active==handle_)active=INVALID_HANDLE_VALUE;
        if(handle_!=INVALID_HANDLE_VALUE){CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;}
    }
    bool Stopping()const{return stop.load() || collision.load() || switch_;}
    std::uint64_t Now()const{return GetTickCount64();}
    unsigned Error()const{return error_;}
};
struct Output {
    bool MayOpen(const f75::Candidate& c) const {
        return !NativeAnalogRouting_IsClaimed(c.path.c_str()) ||
            NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::AulaF75);
    }
    void Reset() {
        Clear();boomMode=false;std::lock_guard<std::mutex> lock(dataMutex);stats={};
    }
    void Event(const char* stage,const char* reason,unsigned a,unsigned b,unsigned c,unsigned d) noexcept {Record(stage,reason,a,b,c,d);}
    void Identity(const f75::Candidate& c,const f75::Info& info) {
        liveCandidate=c;boomMode=f75::Boom(c);token=f75::Token(c);
        // FEFE is shared by multiple receivers. Only the exact wired F75 tuple
        // identifies the standard keyboard collection for this reference.
        if(f75::Wired(c.vid,c.pid))referenceIdentity=(c.vid<<16)|c.pid;
        std::lock_guard<std::mutex> lock(dataMutex);
        stats.identity=true;stats.vid=c.vid;stats.pid=c.pid;stats.page=c.page;stats.input=c.inputBytes;stats.output=c.outputBytes;
        stats.firmware=info.version;stats.infoVid=info.vid;stats.infoPid=info.pid;stats.precision=info.precision;stats.frame=info.frame;
    }
    void Map(const f75::Assignments& map,bool metadata) {
        assignments=map;factoryValues.Clear();assignedValues.Clear();
        const auto& physical=f75::FactoryFor(liveCandidate);
        std::array<halljoy::native_layout::Key,128> keys{};unsigned count=0;
        for(unsigned i=0;i<128;++i)if(physical[i]) {
            const auto id=std::uint8_t(i+1);
            factoryValues.Bind(id,physical[i]);
            if(map[i])assignedValues.Bind(id,map[i]);
            keys[count++]={physical[i],map[i]};
        }
        const bool cached=halljoy::native_layout::Publish(token.load(),keys.data(),count);
        Record("layout",cached?"cache_published":"cache_unavailable_continue",count);
        std::lock_guard<std::mutex> lock(dataMutex);stats.metadata=metadata?1:0;stats.mapped=count;
    }
    void Sample(unsigned slot,unsigned raw,unsigned adc,unsigned stroke,unsigned route) {
        if(!connected.load()) {
            if(!NativeAnalogRouting_Claim(std::uint16_t(liveCandidate.vid),std::uint16_t(liveCandidate.pid),liveCandidate.path.c_str(),NativeAnalogProtocol::AulaF75) &&
               !NativeAnalogRouting_IsClaimedBy(liveCandidate.path.c_str(),NativeAnalogProtocol::AulaF75)) {
                Record("routing","ownership_collision");collision=true;return;
            }
            connected=true;Record("depth","first_valid_sample",route,raw==0?1:0);
        }
        const auto milli=std::uint16_t(f75::Milli(raw,stroke));const auto now=GetTickCount64();
        factoryValues.Publish(std::uint8_t(slot+1),milli,now,true);
        assignedValues.Publish(std::uint8_t(slot+1),milli,now,true);
        const auto hid=Remapped()?assignments[slot]:f75::FactoryFor(liveCandidate)[slot];
        if(hid)InputTrace_Source(boomMode.load()?"boom68-ram":"aula-f75",0,slot,raw,milli,hid);
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
        const auto hid=Remapped()?assignments[slot]:f75::FactoryFor(liveCandidate)[slot];
        return hid && Bindings_IsHidBound(hid);
    }
    void Requested(unsigned cmd) {
        std::lock_guard<std::mutex> lock(dataMutex);++stats.queries;
        stats.statusQueries+=cmd==0x68;stats.monitorOn+=cmd==0x66;stats.monitorOff+=cmd==0x67;
    }
    void Received(unsigned,unsigned) {std::lock_guard<std::mutex> lock(dataMutex);++stats.replies;}
    void MonitorReceived(f75::MonitorFrame frame) {
        std::lock_guard<std::mutex> lock(dataMutex);
        ++stats.monitorFrames[static_cast<std::size_t>(frame)];
    }
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
    SupportLog_SetAulaF75EvidenceProvider(&Evidence);
    bool blocked=false;const auto candidates=Enumerate(true,nullptr,&blocked);present=!candidates.empty() || blocked;return present.load();
}
unsigned __stdcall Worker(void*) {
    try {
        std::vector<std::wstring> declined;
        while(!stop.load()) {
            const auto generation=topology.load();
            bool blocked=false;const auto candidates=Enumerate(false,nullptr,&blocked);present=!candidates.empty() || blocked;
            declined.erase(std::remove_if(declined.begin(),declined.end(),[&](const auto& path){
                return std::none_of(candidates.begin(),candidates.end(),[&](const auto& c){return c.path==path;});
            }),declined.end());
            for(const auto& c:candidates) {
                if(stop.load() || generation!=topology.load())break;
                if(std::find(declined.begin(),declined.end(),c.path)!=declined.end())continue;
                if(NativeAnalogRouting_IsClaimed(c.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::AulaF75)){
                    Record("routing","ownership_collision_before_open",c.vid,c.pid,0,0,f75::Boom(c));declined.push_back(c.path);continue;
                }
                collision=false;HidTransport io(generation);Output output;f75::Client<HidTransport,Output> client(io,output);
                if(client.Run(c)==f75::Result::Foreign)declined.push_back(c.path);
            }
            if(!stop.load() && generation==topology.load() && wake)WaitForSingleObject(wake,candidates.empty() && !blocked?INFINITE:1000);
        }
    }catch(...){Record("worker","exception_input_cleared");}
    Clear();running=false;return 0;
}
bool Start() {
    std::lock_guard<std::mutex> lock(serviceMutex);
    if(worker)return running.load();
    stop=false;collision=false;SupportLog_SetAulaF75EvidenceProvider(&Evidence);
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
        const auto result=WaitForSingleObject(worker,3000);
        if(result!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,result==WAIT_TIMEOUT?WAIT_TIMEOUT:GetLastError());
        CloseHandle(worker);worker=nullptr;
    }
    if(wake){CloseHandle(wake);wake=nullptr;}
    if(cancelEvent){CloseHandle(cancelEvent);cancelEvent=nullptr;}
    Clear();SupportLog_SetAulaF75EvidenceProvider(nullptr);
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
    out->vendorId=std::uint16_t(s.vid);out->productId=std::uint16_t(s.pid);out->usagePage=std::uint16_t(s.page);out->usage=0x61;
    out->mappedKeys=s.mapped;
    if(out->connected)for(auto raw:s.last)out->activeKeys+=raw>0;
    out->nominalRawLevels=341;
    out->inputReportBytes=s.input;out->outputReportBytes=s.output;out->successfulUpdates=s.samples;
    out->averageIntervalUs=s.meanUs;out->maximumIntervalUs=s.maxUs;out->updateHz10=s.meanUs?10000000/s.meanUs:0;
    const auto now=GetTickCount64();out->lastUpdateAgeMs=s.lastUpdate && now>=s.lastUpdate?unsigned(std::min<std::uint64_t>(0xffffffffull,now-s.lastUpdate)):0;
    // Only the hardware-confirmed BOOM68 matrix route suppresses the trial notice.
    const bool boom=s.vid==0x0c45 && s.pid==0x80b5;
    const bool confirmed=boom && s.route==3;
    out->genericProtocol=out->connected && !confirmed;
    wcscpy_s(out->deviceName,boom?L"LANGTU BOOM68-USB":L"AULA F75 HE");
    wcscpy_s(out->status,boom?(out->connected?(confirmed?L"BOOM68-USB: matrix analog":L"BOOM68-USB: status analog; hardware validation pending"):L"BOOM68-USB identified; checking analog path"):(out->connected?L"AULA F75 HE: analog trial; hardware validation pending":s.identity?L"AULA F75 HE identified; waiting for analog data":L"AULA F75 HE: searching USB and 2.4 GHz"));
}
#include "aula_f75_self_test.h"
#include "boom68_self_test.h"
} // namespace
const NativeAnalogBackendDescriptor& AulaF75_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "aula-f75",L"Sonix AULA F75 HE / LANGTU BOOM68-USB (trial)",NativeAnalogProtocol::AulaF75,NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReversibleControlProbe|NativeAnalogBackendFlag_DynamicVidPid,
        &Prepare,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry,nullptr,
        +[](halljoy::observation::Sink sink){(Remapped()?assignedValues:factoryValues).Observations(sink);}};
    return d;
}
void AulaF75_ObserveRawKey(void* device,std::uint16_t hid,bool down) noexcept {
    const auto identity=referenceIdentity.load();
    if(!identity || !hid || hid>=256 || !device)return;
    const auto generation=referenceGeneration.load();
    static void* lastDevice=nullptr;
    static std::uint32_t lastIdentity=0,lastGeneration=0;
    static bool matches=false;
    if(device!=lastDevice || identity!=lastIdentity || generation!=lastGeneration) {
        lastDevice=device;lastIdentity=identity;lastGeneration=generation;matches=false;
        wchar_t name[512]{};UINT count=static_cast<UINT>(std::size(name));
        if(GetRawInputDeviceInfoW(static_cast<HANDLE>(device),RIDI_DEVICENAME,name,&count)!=static_cast<UINT>(-1)) {
            wchar_t tuple[32]{};
            swprintf_s(tuple,L"vid_%04x&pid_%04x",identity>>16,identity&0xffffu);
            matches=f75::Lower(name).find(tuple)!=std::wstring::npos;
        }
    }
    if(matches && identity==referenceIdentity.load() && generation==referenceGeneration.load())
        RecordDigitalReference(hid,down,identity,generation);
}
bool AulaF75_TryRunSelfTest(int& result) noexcept {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    const bool boom=argv && argc==2 && wcscmp(argv[1],L"--halljoy-boom68-self-test")==0;
    const bool selected=boom || (argv && argc==2 && wcscmp(argv[1],L"--halljoy-aula-f75-self-test")==0);
    if(argv)LocalFree(argv);
    if(!selected)return false;
    bool pass=false;try {pass=boom?BoomSelfTest():F75SelfTest();}catch(...){pass=false;}
    if(boom){printf("BOOM68_SELF_TEST=%s scenarios=18 negatives=14 physical_channels=67 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 readonly=1 device_info_commands=0 monitor_commands=0 log_fields_retained=1 hardware_opened=0\n",pass?"PASS":"FAIL");result=pass?0:10;return true;}
    printf("AULA_F75_SELF_TEST=%s scenarios=%zu negatives=%zu physical_channels=80 proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 hotplug_cancel=1 wired_priority=1 unrelated_topology=1 monitor_frame_evidence=1 digital_reference_only=1 hardware_opened=0\n",pass?"PASS":"FAIL",std::size(f75Positive),std::size(f75Negative));
    result=pass?0:10;return true;
}
