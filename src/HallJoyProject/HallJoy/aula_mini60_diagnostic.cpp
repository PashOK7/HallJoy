#if defined(HALLJOY_AULA_MINI60_DIAGNOSTIC) || defined(HALLJOY_AULA_MINI60_NATIVE)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "aula_mini60_diagnostic.h"
#include "aula_mini60_diagnostic_protocol.h"
#include "hid_io_operation.h"
#include "stability_trace.h"
#include "debug_log.h"
#include "aula_mini60_native_model.h"
#include "native_analog_backend.h"
#include "native_analog_backend_registry.h"
#include "configured_xusb_builder.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
#include "realtime_loop.h"
#pragma comment(lib,"setupapi.lib")
#pragma comment(lib,"hid.lib")
namespace {
using namespace halljoy::mini60diag;
struct Handle {
    HANDLE h=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v=INVALID_HANDLE_VALUE):h(v){}
    ~Handle(){if(h && h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    explicit operator bool()const{return h && h!=INVALID_HANDLE_VALUE;}
};
std::atomic<bool> parentStop{false},collecting{false};
std::atomic<unsigned> uiState{0},keyCount{0},releaseCount{0},maximumHeld{0};
std::thread supervisor;
// Anonymous inherited mapping: counters only, never ordered key events.
struct Shared {
    volatile LONG keys=0,released=0,held=0,lastChange=0,topology=0;
    volatile LONG nativeReady=0,mapReady=0,mapRevision=0,heartbeat=0,productId=0;
    volatile LONG assigned[126]{};
    alignas(8) volatile LONG64 samples[126]{};
};
bool liveWorker=false;
std::atomic<bool> nativeConnected{false};
std::atomic<unsigned> nativeMapped{0},nativeProduct{0};
std::uint64_t NativeToken(){return halljoy::mini60::Token(nativeProduct.load());}
std::atomic<std::uint64_t> nativeUpdates{0},nativeExpired{0};
halljoy::physical_analog::Publication factoryValues,assignedValues;
void PumpNative();
void ClearNative();
Handle sharedMapping;
Shared* shared=nullptr;
void CreateShared(){
    if(shared)return;
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    sharedMapping.h=CreateFileMappingW(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(Shared),nullptr);
    if(!sharedMapping)throw 1;
    shared=static_cast<Shared*>(MapViewOfFile(sharedMapping.h,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
    if(!shared)throw 1;
    InterlockedExchange(&shared->lastChange,static_cast<LONG>(GetTickCount()));
}
LONG Load(volatile LONG& value){return InterlockedCompareExchange(&value,0,0);}
HANDLE childCancel=nullptr;
bool Stop(){return childCancel && WaitForSingleObject(childCancel,0)==WAIT_OBJECT_0;}
void Line(const std::string& s){DWORD n=0;std::string l=s+"\n";WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),l.data(),static_cast<DWORD>(l.size()),&n,nullptr);}
void Event(const char* name,unsigned error=0){Line(std::string(name)+" error="+std::to_string(error));}
std::wstring ThisExe(){wchar_t b[32768]{};auto n=GetModuleFileNameW(nullptr,b,_countof(b));if(!n || n>=_countof(b))throw 1;return {b,n};}
std::wstring Quote(const std::wstring& s){return L"\""+s+L"\"";}
struct Device {std::wstring path;unsigned productId=0;};
std::vector<Device> Find(){
    std::vector<Device> result;unsigned receiver=0,wired=0,rejected=0;
    GUID guid{};HidD_GetHidGuid(&guid);
    HDEVINFO list=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(list==INVALID_HANDLE_VALUE){Event("enumeration_failed",GetLastError());return result;}
    struct Guard{HDEVINFO h;~Guard(){SetupDiDestroyDeviceInfoList(h);}} guard{list};
    for(DWORD i=0;i<256 && !Stop();++i){
        SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof(item);
        if(!SetupDiEnumDeviceInterfaces(list,nullptr,&guid,i,&item))break;
        DWORD size=0;SetupDiGetDeviceInterfaceDetailW(list,&item,nullptr,0,&size,nullptr);
        if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size>65536)continue;
        std::vector<unsigned char> memory(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(memory.data());detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(list,&item,detail,size,nullptr,nullptr))continue;
        std::wstring path=detail->DevicePath;std::wstring lower=path;
        std::transform(lower.begin(),lower.end(),lower.begin(),towlower);
        if(lower.find(L"vid_0c45&pid_8032")==std::wstring::npos && lower.find(L"vid_0c45&pid_80a2")==std::wstring::npos && lower.find(L"vid_0c45&pid_80a1")==std::wstring::npos && lower.find(L"vid_0c45&pid_fefe")==std::wstring::npos && lower.find(L"vid_0c45&pid_fefc")==std::wstring::npos)continue;
        Handle meta(CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
        if(!meta){Event("metadata_open_failed",GetLastError());continue;}
        HIDD_ATTRIBUTES attr{};attr.Size=sizeof(attr);
        if(!HidD_GetAttributes(meta.h,&attr) || attr.VendorID!=0x0c45)continue;
        PHIDP_PREPARSED_DATA prep=nullptr;HIDP_CAPS caps{};
        if(!HidD_GetPreparsedData(meta.h,&prep)){Event("descriptor_failed",GetLastError());continue;}
        auto status=HidP_GetCaps(prep,&caps);HidD_FreePreparsedData(prep);
        if(status!=HIDP_STATUS_SUCCESS){Event("caps_failed",static_cast<unsigned>(status));continue;}
        if(attr.ProductID==0xfefe || attr.ProductID==0xfefc){++receiver;continue;}
        if(!halljoy::mini60::SupportedProduct(attr.ProductID))continue;
        ++wired;
        char line[300];sprintf_s(line,"descriptor vid=0c45 pid=%04x bcd=%04x usage=%04x:%04x in=%u out=%u feature=%u",attr.ProductID,attr.VersionNumber,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength,caps.FeatureReportByteLength);Line(line);
        if(caps.UsagePage==0xff68 && caps.Usage==0x61 && caps.InputReportByteLength==65 && caps.OutputReportByteLength==65)result.push_back({path,attr.ProductID});else ++rejected;
    }
    Line("inventory wired_collections="+std::to_string(wired)+" receiver_collections="+std::to_string(receiver)+" rejected_collections="+std::to_string(rejected)+" eligible="+std::to_string(result.size())+" receiver_commands=0");
    return result;
}
struct Session {
    Handle device;Metrics metrics,chords;bool outputControl=false,modeAttempted=false,ioBroken=false;
    unsigned phase=0,readErrors=0,timeouts=0,acks=0;std::array<unsigned,256> unknownCommands{};unsigned chordPackets=0;std::map<unsigned,unsigned> malformedShapes;
    explicit Session(const std::wstring& path):device(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr)){
        if(!device)Event("data_open_failed",GetLastError());
        else {bool ok=HidD_SetNumInputBuffers(device.h,512)!=FALSE;Line("input_queue requested=512 ok="+std::to_string(ok));}
    }
    bool Send(unsigned cmd,unsigned len=0,unsigned off=0){
        auto r=Request(cmd,len,off);if(r[1]!=0xaa){Event("command_rejected");return false;}
        DWORD error=0,got=0;bool ok=false;
        if(outputControl){ok=HidD_SetOutputReport(device.h,r.data(),static_cast<ULONG>(r.size()))!=FALSE;if(!ok)error=GetLastError();}
        else {HidIoOperation op(device.h);auto started=op.StartWrite(r.data(),static_cast<DWORD>(r.size()),&error);
            if(started!=HidIoOperation::StartResult::Failed){
                if(started==HidIoOperation::StartResult::Pending && op.Wait(250)!=WAIT_OBJECT_0){op.CancelAndDrain(nullptr,&error);error=WAIT_TIMEOUT;}
                else ok=op.Finish(&got,&error,false) && got==r.size();}}
        Line("tx command="+std::to_string(cmd)+" length="+std::to_string(len)+" offset="+std::to_string(off)+" route="+(outputControl?"control":"write")+" ok="+std::to_string(ok)+" error="+std::to_string(error));return ok;
    }
    bool Read(std::array<std::uint8_t,65>& r,DWORD& got,unsigned wait=20){
        DWORD error=0;HidIoOperation op(device.h);auto started=op.StartRead(r.data(),static_cast<DWORD>(r.size()),&error);
        if(started==HidIoOperation::StartResult::Failed){++readErrors;ioBroken=true;if(readErrors<=3)Event("read_start_failed",error);return false;}
        if(started==HidIoOperation::StartResult::Pending && op.Wait(wait)!=WAIT_OBJECT_0){op.CancelAndDrain(&got,&error);++timeouts;
            if(error==ERROR_SUCCESS && got)return true; // Preserve a completion racing cancellation.
            if(error!=ERROR_OPERATION_ABORTED && error!=ERROR_SUCCESS){++readErrors;ioBroken=true;Event("read_cancel_failed",error);}
            return false;}
        if(!op.Finish(&got,&error,false)){++readErrors;ioBroken=true;if(readErrors<=3)Event("read_failed",error);return false;}
        return true;
    }
    bool Pump(unsigned expected=0,unsigned len=0,unsigned off=0,std::array<std::uint8_t,64>* reply=nullptr,unsigned wait=20){
        std::array<std::uint8_t,65> r{};DWORD got=0;if(!Read(r,got,wait))return false;
        auto raw=std::span<const std::uint8_t>(r.data(),got);auto p=Payload(raw);
        if(p.empty() && malformedShapes.size()<16){unsigned shape=(got<<16)|(got?r[0]<<8:0)|(got>1?r[1]:0);++malformedShapes[shape];}
        if(!p.empty() && p[1]==expected && Reply(p,expected,len,off)){
            ++acks;if(reply)std::copy(p.begin(),p.end(),reply->begin());return true;}
        if(!p.empty() && p[1]!=0xfb)++unknownCommands[p[1]];
        if(metrics.Add(raw,GetTickCount64(),phase)){
            if(shared && Load(shared->held)>=2){++chordPackets;chords.Add(raw,GetTickCount64(),phase);}
            if(liveWorker && modeAttempted && shared && Load(shared->mapReady)){
                Sample sample{};
                if(Decode(raw,sample) && halljoy::mini60::Factory[sample.key]){
                    InterlockedExchange64(&shared->samples[sample.key],static_cast<LONG64>(halljoy::mini60::Pack(sample.travel,sample.stroke,GetTickCount())));
                    InterlockedExchange(&shared->nativeReady,1);
                }
            }
        }
        return false;
    }
    bool Exchange(unsigned cmd,unsigned len=0,unsigned off=0,std::array<std::uint8_t,64>* reply=nullptr,bool cleanup=false){
        if(!Send(cmd,len,off))return false;
        const auto end=GetTickCount64()+450;
        while(GetTickCount64()<end && (!Stop() || cleanup) && !ioBroken)if(Pump(cmd,len,off,reply))return true;
        Line("reply_missing command="+std::to_string(cmd)+" transport_broken="+std::to_string(ioBroken));return false;
    }
    bool Cleanup(){
        if(!modeAttempted)return true;
        // The SDK's stop operation only requires transmission; record ACK separately.
        bool sent=Send(0x67),ack=false;auto end=GetTickCount64()+180;
        while(sent && GetTickCount64()<end && !ioBroken)if(Pump(0x67)){ack=true;break;}
        if(!sent){outputControl=!outputControl;sent=Send(0x67);}
        Line("cleanup tx_ok="+std::to_string(sent)+" ack="+std::to_string(ack)+" unplug_may_require_reconnect="+std::to_string(!sent));modeAttempted=false;return sent;
    }
    ~Session(){if(modeAttempted){try{Cleanup();}catch(...){}}}
    void Summary(bool final){
        Line(std::string(final?"summary":"checkpoint")+" packets="+std::to_string(metrics.packets)+" keys="+std::to_string(metrics.Seen())+" varying="+std::to_string(metrics.Varying())+" recent50_max="+std::to_string(metrics.recentKeysMax)+" malformed="+std::to_string(metrics.malformed)+" unknown="+std::to_string(metrics.unknown)+" read_errors="+std::to_string(readErrors)+" read_timeouts="+std::to_string(timeouts));
        if(!final)return;
        for(unsigned i=0;i<metrics.keys.size();++i){const auto& k=metrics.keys[i];if(!k.count)continue;
            char b[1000];sprintf_s(b,"key index=%u samples=%llu travel_min=%u travel_max=%u adc_min=%u adc_max=%u high_min=%u high_max=%u low_min=%u low_max=%u stroke_min=%u stroke_max=%u status_mask=%u zero=%llu increases=%llu decreases=%llu equal=%llu max_gap_ms=%llu gaps_gt50=%llu gaps_gt250=%llu silent_tail_ms=%llu",i,k.count,k.minTravel,k.maxTravel,k.minAdc,k.maxAdc,k.minHigh,k.maxHigh,k.minLow,k.maxLow,k.minStroke,k.maxStroke,k.statusMask,k.zero,k.increase,k.decrease,k.equal,k.maxGap,k.gap50,k.gap250,GetTickCount64()-k.last);Line(b);}
        for(const auto& [shape,count]:malformedShapes)Line("malformed_shape length="+std::to_string(shape>>16)+" prefix0="+std::to_string((shape>>8)&255)+" prefix1="+std::to_string(shape&255)+" count="+std::to_string(count));
        for(unsigned i=0;i<4;++i)Line("phase_summary phase="+std::to_string(i)+" packets="+std::to_string(metrics.phases[i]));
        for(unsigned i=0;i<256;++i)if(unknownCommands[i])Line("other_report command="+std::to_string(i)+" count="+std::to_string(unknownCommands[i]));
    }
};
int Capture(bool cleanupOnly){
    if(liveWorker){InterlockedExchange(&shared->nativeReady,0);InterlockedExchange(&shared->mapReady,0);}

    const auto begin=GetTickCount64();std::vector<Device> devices;
    do {devices=Find();if(!devices.empty() || cleanupOnly || Stop())break;
        Line("status state=1");for(unsigned i=0;i<10 && !Stop();++i)Sleep(100);
    }while(!Stop());
    if(devices.size()!=1){Line(devices.empty()?"no_wired_device connect_usb_required=1":"multiple_wired_devices commands_skipped=1");return devices.empty()?2:3;}
    // The same-session path is never logged or persisted.
    unsigned attempt=0;
    while(attempt<3 && (!Stop() || cleanupOnly)){
        Session s(devices[0].path);if(!s.device){if(cleanupOnly || GetTickCount64()-begin>45000)return 6;Line("status state=11");Sleep(500);continue;}
        if(cleanupOnly){s.modeAttempted=true;return s.Cleanup()?0:6;}
        if(liveWorker){
            InterlockedExchange(&shared->nativeReady,0);InterlockedExchange(&shared->mapReady,0);
            for(auto& value:shared->samples)InterlockedExchange64(&value,0);
        }
        Line("connection attempt="+std::to_string(attempt++));Line("status state=2");
        bool info=false;
        for(unsigned route=0;route<2 && !Stop();++route){
            s.outputControl=route!=0;std::array<std::uint8_t,64> reply{};
            for(unsigned retry=0;retry<2 && !Stop();++retry){
                if(s.Exchange(0x10,56,0,&reply)){
                    const auto vid=U16(reply.data()+12),pid=U16(reply.data()+14);
                    if(!halljoy::mini60::Identity(devices[0].productId,vid,pid,U16(reply.data()+20),U16(reply.data()+22))){Line("identity_mismatch commands_stopped=1");return 7;}
                    if(liveWorker)InterlockedExchange(&shared->productId,static_cast<LONG>(pid));
                    char b[250];sprintf_s(b,"device_info vid=%04x pid=%04x version_bytes=%02x:%02x manufacturer=%u product=%u work_mode=%u",vid,pid,reply[16],reply[17],U16(reply.data()+20),U16(reply.data()+22),reply[24]);Line(b);info=true;break;
                }
                if(s.ioBroken)break;
            }
            if(info || s.ioBroken)break;
        }
        if(!info)Line("device_info_unavailable identity=usb_descriptor_only firmware_version_unknown=1");
        if(liveWorker && !info){Line("native_identity_required retry=1");if(Stop())return 4;continue;}
        // Read only key assignments. Never read macro bodies or user text.
        if(info && !Stop()){
            std::array<std::uint8_t,512> map{};std::array<bool,512> valid{};
            for(unsigned off=0;off<512 && !Stop() && !s.ioBroken;off+=56){
                unsigned n=std::min(56u,512-off);std::array<std::uint8_t,64> reply{};bool ok=false;
                for(unsigned retry=0;retry<2 && !Stop();++retry)if(s.Exchange(0x12,n,off,&reply)){ok=true;break;}
                if(ok)for(unsigned j=0;j<n;++j){map[off+j]=reply[8+j];valid[off+j]=true;}
            }
            for(unsigned k=0;k<126;++k){unsigned o=k*4;if(!valid[o] || !valid[o+3])continue;
                Line("assignment index="+std::to_string(k)+" type="+std::to_string(map[o])+" usage="+std::to_string(map[o]==2?map[o+2]:0)+" modifiers="+std::to_string(map[o]==2?map[o+1]:0));}
            Line("keymap bytes_read="+std::to_string(std::count(valid.begin(),valid.end(),true)));
            if(liveWorker && std::all_of(valid.begin(),valid.end(),[](bool v){return v;})){
                unsigned mapped=0,unsupported=0;
                for(unsigned k=0;k<126;++k){
                    auto usage=halljoy::mini60::Assigned(k,map.data()+k*4);
                    InterlockedExchange(&shared->assigned[k],usage);mapped+=usage!=0;
                    unsupported+=halljoy::mini60::Factory[k] && !usage;
                }
                InterlockedIncrement(&shared->mapRevision);InterlockedExchange(&shared->mapReady,1);
                Line("native_mapping factory=61 assigned="+std::to_string(mapped)+" unsupported_assignments="+std::to_string(unsupported));
            }
        }
        if(Stop())return 4;
        if(liveWorker && !Load(shared->mapReady)){Line("native_map_incomplete retry=1");continue;}
        if(s.ioBroken){s.Summary(true);Sleep(200);devices=Find();if(devices.size()!=1)return 6;continue;}
        s.modeAttempted=true;Line("mode_attempted value=1");bool ack=s.Exchange(0x66);
        if(!ack && !s.metrics.packets && !s.ioBroken && !Stop()){if(!info)s.outputControl=!s.outputControl;ack=s.Exchange(0x66);}
        Line("start_ack value="+std::to_string(ack));
        if(liveWorker){
            InterlockedExchange(&shared->heartbeat,static_cast<LONG>(GetTickCount()));
            if(ack)InterlockedExchange(&shared->nativeReady,1);
            Line(Load(shared->nativeReady)?"status state=12":"status state=2");auto next=GetTickCount64(),detail=next+30000,proofDeadline=next+2000;
            while(!Stop() && !s.ioBroken){
                InterlockedExchange(&shared->heartbeat,static_cast<LONG>(GetTickCount()));
                s.Pump();
                auto now=GetTickCount64();
                if(!Load(shared->nativeReady) && now>=proofDeadline){Line("native_no_start_proof retry=1");break;}
                if(now>=next){s.Summary(false);next=now+2000;}
                if(now>=detail){s.Summary(true);detail=now+30000;}
            }
            InterlockedExchange(&shared->nativeReady,0);
            s.Summary(true);const bool clean=s.Cleanup();
            Line("native_session_end clean="+std::to_string(clean)+" cancelled="+std::to_string(Stop()));
            return Stop()?4:6;
        }
        auto start=GetTickCount64(),next=start;unsigned previous=99;Coverage coverage;auto activity=start;unsigned wakeRetries=0;auto nextWake=start+1500;LONG lastChange=Load(shared->lastChange);
        while(!Stop() && !s.ioBroken && !coverage.complete){
            auto now=GetTickCount64();auto changed=Load(shared->lastChange);
            if(changed!=lastChange){activity=now;lastChange=changed;}
            unsigned quietMs=GetTickCount()-static_cast<DWORD>(changed);
            coverage.Update(s.metrics.Seen(),s.metrics.Varying(),Load(shared->keys),Load(shared->released),Load(shared->held),quietMs,s.chordPackets,s.chords.Seen(),s.chords.Varying(),now);
            s.phase=coverage.phase;
            if(!s.metrics.packets && Load(shared->keys)>0 && now>=nextWake && wakeRetries<2){
                ++wakeRetries;Line("wake_retry attempt="+std::to_string(wakeRetries));
                s.Exchange(0x66);nextWake=GetTickCount64()+1500;
            }
            if(now-activity>=45000){Line("inactivity_limit reached=1 partial_data_preserved=1");break;}
            if(now-start>5000 && Load(shared->keys)>=8 && Load(shared->released)>=8 && Load(shared->held)==0 && quietMs>=1500 && !s.metrics.packets){Line("no_analog_despite_digital_input result_is_informative=1");break;}
            if(now-start>15000 && s.metrics.Seen()>=8 && Load(shared->keys)==0){Line("raw_input_missing analog_data_preserved=1");break;}
            if(s.phase!=previous){previous=s.phase;Line("status state="+std::to_string(3+s.phase));}
            s.Pump();
            if(GetTickCount64()>=next){s.Summary(false);Line("digital_checkpoint keys="+std::to_string(Load(shared->keys))+" released="+std::to_string(Load(shared->released))+" held="+std::to_string(Load(shared->held))+" chord_packets="+std::to_string(s.chordPackets));next=GetTickCount64()+2000;}
        }
        Line("chord_summary keys="+std::to_string(s.chords.Seen())+" varying="+std::to_string(s.chords.Varying())+" packets="+std::to_string(s.chordPackets));
        s.Summary(true);bool clean=s.Cleanup();
        if(s.ioBroken && !Stop() && attempt<3){
            Line("reconnect_attempt partial_data_preserved=1");for(unsigned retry=0;retry<5 && !Stop();++retry){Sleep(400);devices=Find();if(devices.size()==1)break;}if(devices.size()==1)continue;
        }
        Line("coverage keys="+std::to_string(s.metrics.Seen())+" varying="+std::to_string(s.metrics.Varying())+" clean="+std::to_string(clean)+" complete="+std::to_string(!Stop() && !s.ioBroken && coverage.complete));
        return Stop()?4:(!clean || s.ioBroken)?6:0;
    }
    return Stop()?4:6;
}

unsigned coverageKeys=0,coverageVarying=0,coverageClean=0,coverageComplete=0;
bool attemptedMode=false,pipeTestSeen=false;
void ReceiveLine(const std::string& line){
    if(line=="waiting_for_device")return; // Liveness only; no idle log growth.
    if(line.size()>2000){StabilityTrace_AppendPlain(L"[mini60] oversized_worker_record dropped=1");return;}
    std::wstring wide(line.begin(),line.end());StabilityTrace_AppendPlain((L"[mini60] "+wide).c_str());
    unsigned state=0;
    if(sscanf_s(line.c_str(),"status state=%u",&state)==1 && state>=1 && (state<=6 || state==11 || state==12))uiState=state;
    if(line=="mode_attempted value=1")attemptedMode=true;
    if(line.starts_with("cleanup tx_ok=1 ack=1"))attemptedMode=false;
    if(line=="pipe_test complete_record=1")pipeTestSeen=true;
    if(line.starts_with("coverage "))sscanf_s(line.c_str(),"coverage keys=%u varying=%u clean=%u complete=%u",&coverageKeys,&coverageVarying,&coverageClean,&coverageComplete);
}
static DWORD RunChild(const std::wstring& mode,DWORD budget,bool allowCancelled=false) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE r=nullptr,w=nullptr;
    if(!CreatePipe(&r,&w,&security,65536))return 100;
    Handle read(r),write(w),cancel(CreateEventW(&security,TRUE,FALSE,nullptr));
    if(!cancel || !SetHandleInformation(read.h,HANDLE_FLAG_INHERIT,0))return 100;
    Handle nullInput(CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr));
    if(!nullInput)return 100;
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> attributeBytes(bytes);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBytes.data());
    if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes))return 100;
    HANDLE inherit[]={write.h,cancel.h,nullInput.h,sharedMapping.h};
    bool ready=UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,sizeof(inherit),nullptr,nullptr)!=FALSE;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdOutput=write.h;
    startup.StartupInfo.hStdError=write.h;startup.StartupInfo.hStdInput=nullInput.h;
    auto exe=ThisExe();auto command=Quote(exe)+L" --halljoy-mini60-worker "+Quote(mode)+L" "+
        std::to_wstring(reinterpret_cast<uintptr_t>(cancel.h))+L" "+std::to_wstring(reinterpret_cast<uintptr_t>(sharedMapping.h));
    PROCESS_INFORMATION pi{};
    const bool created=ready && CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&pi);
    const DWORD creationError=created?0:GetLastError();DeleteProcThreadAttributeList(attributes);
    if(!created){DebugLog_Write(L"[mini60] child_start_error=%lu",creationError);return 100;}
    Handle process(pi.hProcess),thread(pi.hThread);
    // Kill-on-close prevents an orphaned probe from retaining a keyboard handle
    // after a parent crash. Assign before sending any runtime commands below.
    Handle job(CreateJobObjectW(nullptr,nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job || !SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limit,sizeof(limit)) || !AssignProcessToJobObject(job.h,process.h)) {
        TerminateProcess(process.h,100);WaitForSingleObject(process.h,2000);return 100;
    }
    if(ResumeThread(thread.h)==DWORD(-1)){TerminateProcess(process.h,100);WaitForSingleObject(process.h,2000);return 100;}
    CloseHandle(write.h);write.h=INVALID_HANDLE_VALUE;
    std::string pending;auto end=GetTickCount64()+budget;ULONGLONG cancelledAt=0;bool forced=false;auto lastOutput=GetTickCount64();
    for(;;){
        DWORD available=0;
        if(PeekNamedPipe(read.h,nullptr,0,nullptr,&available,nullptr) && available){
            lastOutput=GetTickCount64();
            char buffer[8192];DWORD got=0;
            if(ReadFile(read.h,buffer,std::min<DWORD>(available,sizeof(buffer)),&got,nullptr))pending.append(buffer,got);
            size_t newline;
            while((newline=pending.find('\n'))!=std::string::npos){ReceiveLine(pending.substr(0,newline));pending.erase(0,newline+1);}
            if(pending.size()>65536){ReceiveLine("{\"kind\":\"oversized_record\"}");pending.clear();}
            // Recheck deadlines even under an uninterrupted input stream.
        }
        else if(WaitForSingleObject(process.h,0)==WAIT_OBJECT_0)break;
        if(mode==L"play")PumpNative();
        const auto now=GetTickCount64();
        if(mode==L"capture" && !StabilityTrace_IsEnabled())parentStop=true;
        if(!cancelledAt && (now>=end || ((mode==L"capture" || mode==L"play") && now-lastOutput>8000) || (parentStop && !allowCancelled))){SetEvent(cancel.h);cancelledAt=now;}
        if(cancelledAt && now-cancelledAt>=1200 && WaitForSingleObject(process.h,0)!=WAIT_OBJECT_0){
            TerminateProcess(process.h,101);forced=true;
            if(WaitForSingleObject(process.h,1000)!=WAIT_OBJECT_0){
                DebugLog_Write(L"[mini60] child_unreaped stop_retrying_device=1");return 103;
            }
        }
        Sleep(mode==L"play"?1:2);
    }
    if(!pending.empty())ReceiveLine(pending);
    DWORD code=100;GetExitCodeProcess(process.h,&code);
    DebugLog_Write(L"[mini60] worker mode=%s exit=%lu forced=%d",mode.c_str(),code,forced?1:0);
    return code;
}


LONG nativeRevision=-1;
std::array<std::uint64_t,126> publishedSamples{};
std::array<unsigned,126> previousValues{};
void ClearNative(){
    const bool was=nativeConnected.exchange(false);
    if(!was && nativeRevision==-1 && !nativeProduct.load())return;
    factoryValues.Clear();assignedValues.Clear();nativeMapped=0;nativeRevision=-1;
    publishedSamples.fill(0);previousValues.fill(0);
    halljoy::native_layout::Clear(NativeToken());nativeProduct=0;
    RealtimeLoop_NotifyInputChanged();
}
void PumpNative(){
    if(!shared)return;
    const auto heartbeat=static_cast<DWORD>(Load(shared->heartbeat));
    const auto now=GetTickCount64();const auto tick=static_cast<DWORD>(now);
    if(!Load(shared->nativeReady) || !Load(shared->mapReady) ||
        DWORD(tick-heartbeat)>1000){ClearNative();return;}
    const auto product=static_cast<unsigned>(Load(shared->productId));
    if(!halljoy::mini60::SupportedProduct(product)){ClearNative();return;}
    LONG revision=Load(shared->mapRevision);
    if(revision!=nativeRevision || product!=nativeProduct.load()){
        ClearNative();nativeProduct=product;std::array<halljoy::native_layout::Key,61> map{};unsigned count=0;
        for(unsigned i=0;i<126;++i)if(auto hid=halljoy::mini60::Factory[i]){
            auto assigned=static_cast<std::uint16_t>(Load(shared->assigned[i]));
            if(assigned>=halljoy::native_layout::kHidCount)assigned=0;
            factoryValues.Bind(static_cast<std::uint8_t>(i+1),hid);
            if(assigned)assignedValues.Bind(static_cast<std::uint8_t>(i+1),assigned);
            map[count++]={hid,assigned};
        }
        if(count!=map.size() || !halljoy::native_layout::Publish(NativeToken(),map.data(),count))return;
        nativeMapped=count;nativeRevision=revision;
        StabilityTrace_Write(L"INFO",L"mini60",L"native_map_published",L"keys=%u revision=%ld normalization=travel_div_stroke_times10 stale_ms=50",count,revision);
    }
    bool changed=false;
    for(unsigned i=0;i<126;++i)if(halljoy::mini60::Factory[i]){
        auto packed=static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->samples[i],0,0));
        // Read the clock after the shared sample. A producer update at a tick
        // boundary must not look like an ancient sample through unsigned wrap.
        const auto sampleNow=GetTickCount64();const auto sampleTick=static_cast<DWORD>(sampleNow);
        const auto value=halljoy::mini60::Read(packed,sampleTick);
        if(previousValues[i] && !value && packed && DWORD(sampleTick-DWORD(packed>>32))>halljoy::mini60::FreshMs)++nativeExpired;
        if(packed!=publishedSamples[i]){++nativeUpdates;publishedSamples[i]=packed;}
        // Also publish expiry zeros to wake the common realtime consumer.
        if(value!=previousValues[i] || value){
            const auto age=packed?DWORD(sampleTick-DWORD(packed>>32)):0;
            const auto stamp=value?sampleNow-age:sampleNow;
            factoryValues.Publish(static_cast<std::uint8_t>(i+1),static_cast<std::uint16_t>(value),stamp);
            assignedValues.Publish(static_cast<std::uint8_t>(i+1),static_cast<std::uint16_t>(value),stamp);
            changed=changed || value!=previousValues[i];previousValues[i]=value;
        }
    }
    nativeConnected=true;uiState=12;
    static std::uint64_t nextLog=0;
    if(now>=nextLog){
        StabilityTrace_Write(L"INFO",L"mini60",L"native_checkpoint",L"updates=%llu stale_releases=%llu active=%u assigned_layout=%u",nativeUpdates.load(),nativeExpired.load(),factoryValues.Active(now),halljoy::native_layout::UsesRemapping(NativeToken())?1u:0u);
        nextLog=now+5000;
    }
    if(changed)RealtimeLoop_NotifyInputChanged();
}
bool NativeStart(){Mini60Diagnostic_Start();return supervisor.joinable();}
halljoy::lifecycle::StopResult NativeStop(halljoy::lifecycle::GenerationId generation){
    parentStop=true;collecting=false;
    if(supervisor.joinable()){
        const auto wait=WaitForSingleObject(supervisor.native_handle(),6000);
        if(wait!=WAIT_OBJECT_0)return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,wait);
    }
    Mini60Diagnostic_Stop();return NativeAnalogBackendStopJoined(generation);
}
bool NativeConnected(){return nativeConnected.load() && !parentStop.load();}
bool NativeOwns(std::uint16_t hid){
    return NativeConnected() && (halljoy::native_layout::UsesRemapping(NativeToken())?assignedValues:factoryValues).Owns(hid);
}
std::uint16_t NativeGet(std::uint16_t hid){
    if(!NativeOwns(hid))return 0;
    return (halljoy::native_layout::UsesRemapping(NativeToken())?assignedValues:factoryValues).Read(hid,GetTickCount64(),halljoy::mini60::FreshMs).milli;
}
void NativeTelemetry(NativeAnalogBackendTelemetry* out){
    if(!out)return;*out={};out->present=NativeConnected();out->connected=NativeConnected();
    out->vendorId=0x0c45;out->productId=static_cast<std::uint16_t>(nativeProduct.load());out->usagePage=0xff68;out->usage=0x61;
    out->mappedKeys=nativeMapped.load();out->activeKeys=factoryValues.Active(GetTickCount64());
    out->inputReportBytes=65;out->outputReportBytes=65;out->nominalRawLevels=341;
    out->successfulUpdates=nativeUpdates.load();
    if(out->connected)out->verifiedLayoutToken=NativeToken();
    wcscpy_s(out->status,nativeProduct.load()==0x8032?L"MINI60 HE: wired analog; per-key freshness 50 ms":nativeProduct.load()==0x80a1?L"MINI60 HE MAX: wired analog; per-key freshness 50 ms":L"MINI60 HE Pro: wired analog; per-key freshness 50 ms");
}

void Supervise() noexcept {
    try{
        coverageKeys=coverageVarying=coverageClean=coverageComplete=0;attemptedMode=false;
#if defined(HALLJOY_AULA_MINI60_NATIVE)
        do {
            attemptedMode=false;
            DWORD result=RunChild(L"play",MAXDWORD);
            ClearNative();
            if(attemptedMode && result!=0 && result!=103)RunChild(L"cleanup",1800,true);
            StabilityTrace_WriteCritical(L"INFO",L"mini60",L"native_session",L"exit=%lu updates=%llu stale_releases=%llu",result,nativeUpdates.load(),nativeExpired.load());
            if(result==103)break;
            uiState=13;
            for(unsigned i=0;i<30 && !parentStop;++i)Sleep(100);
        }while(!parentStop);
        collecting=false;return;
#endif
        DWORD result=RunChild(L"capture",MAXDWORD);
        if(attemptedMode && result!=0 && result!=103){
            DWORD clean=RunChild(L"cleanup",1800,true);
            StabilityTrace_WriteCritical(L"INFO",L"mini60",L"recovery_cleanup",L"exit=%lu reconnect_if_failed=%u",clean,clean!=0);
        }
        collecting=false;
        const bool enough=result==0 && coverageComplete && Enough(coverageKeys,coverageVarying,keyCount,releaseCount,maximumHeld,coverageClean!=0);
        uiState=!StabilityTrace_IsEnabled()?10:result==2?9:enough?7:8;
        StabilityTrace_WriteCritical(L"INFO",L"mini60",L"final",L"exit=%lu sufficient=%u analog_keys=%u varying=%u digital_keys=%u released_keys=%u maximum_held=%u cancelled=%u full_analog_support_proven=0",result,enough,coverageKeys,coverageVarying,keyCount.load(),releaseCount.load(),maximumHeld.load(),parentStop.load());
    }catch(...){ClearNative();collecting=false;uiState=10;StabilityTrace_WriteCritical(L"ERROR",L"mini60",L"supervisor_exception",L"partial_log_retained=1 reconnect_keyboard_if_mode_stuck=1");}
}
struct Digital {
    bool target=false;std::array<bool,1024> held{},seen{},released{};
    unsigned heldCount=0,presses=0,releases=0,repeats=0,orphanReleases=0;
};
std::mutex digitalMutex;std::map<HANDLE,Digital> digital;
void DigitalSummary(){
    std::lock_guard<std::mutex> lock(digitalMutex);unsigned device=0;
    for(const auto& [handle,d]:digital){(void)handle;if(!d.target)continue;
        StabilityTrace_WriteCritical(L"INFO",L"mini60",L"digital_summary",L"device=%u presses=%u releases=%u repeats=%u held_at_close=%u orphan_releases=%u ordered_events_logged=0",++device,d.presses,d.releases,d.repeats,d.heldCount,d.orphanReleases);
    }
}
void Require(bool v){if(!v)throw 1;}
void SelfTest(unsigned product){
    Require(halljoy::mini60::Identity(product,0x0c45,product,0x0166,0x110c));
    Require(!halljoy::mini60::Identity(product,0x0c45,product^1,0x0166,0x110c));
    Require(!halljoy::mini60::Identity(product,0x0c45,product,0,0x110c));
    Require(!halljoy::mini60::SupportedProduct(0xfefc) && !halljoy::mini60::SupportedProduct(0x80b2));
    // Exercise the actual publication/getMilli path without HID or app startup.
    parentStop=false;halljoy::native_layout::activeToken=0;
    InterlockedExchange(&shared->productId,static_cast<LONG>(product));
    for(unsigned i=0;i<126;++i)InterlockedExchange(&shared->assigned[i],halljoy::mini60::Factory[i]);
    auto tick=GetTickCount();InterlockedExchange(&shared->heartbeat,static_cast<LONG>(tick));
    InterlockedExchange(&shared->mapReady,1);InterlockedIncrement(&shared->mapRevision);InterlockedExchange(&shared->nativeReady,1);
    InterlockedExchange64(&shared->samples[34],halljoy::mini60::Pack(170,34,tick));
    InterlockedExchange64(&shared->samples[49],halljoy::mini60::Pack(340,34,tick));
    InterlockedExchange64(&shared->samples[0],halljoy::mini60::Pack(85,34,tick));
    PumpNative();NativeAnalogBackendTelemetry telemetry{};NativeTelemetry(&telemetry);
    Require(telemetry.productId==product && telemetry.verifiedLayoutToken==halljoy::mini60::Token(product));
    Require(NativeToken()==halljoy::mini60::Token(product));Require(NativeOwns(26) && NativeGet(26)==500 && NativeGet(4)==1000 && NativeGet(41)==250);
    Require(NativeAnalogBackends_CatalogIsValid());
    auto routed=NativeAnalogBackends_ReadMilli(26);Require(routed.connected && routed.owned && routed.milli==500);
    halljoy::configured_xusb::PadConfiguration config{};config.axes[0]={4,7};config.axes[1]={22,26};
    halljoy::configured_xusb::InputValues input{};
    for(unsigned hid:{4u,7u,22u,26u})input.filtered[hid]=NativeAnalogBackends_ReadMilli(static_cast<std::uint16_t>(hid)).milli/1000.0f;
    halljoy::configured_xusb::BuilderState gameState{};
    auto frame=halljoy::configured_xusb::BuildReport(config,input,gameState);
    Require(frame.leftStickX==-32767 && frame.leftStickY==16384);
    InterlockedExchange64(&shared->samples[34],halljoy::mini60::Pack(170,34,tick-51));
    PumpNative();Require(NativeToken()==halljoy::mini60::Token(product));Require(NativeOwns(26) && NativeGet(26)==0 && NativeGet(4)==1000);
    InterlockedExchange(&shared->assigned[49],26);InterlockedIncrement(&shared->mapRevision);
    PumpNative();halljoy::native_layout::activeToken=halljoy::mini60::Token(product);
    Require(NativeGet(26)==1000 && !NativeOwns(4));
    InterlockedExchange64(&shared->samples[49],halljoy::mini60::Pack(0,34,GetTickCount()));PumpNative();Require(NativeGet(26)==0);
    InterlockedExchange(&shared->nativeReady,0);PumpNative();Require(!NativeConnected() && NativeGet(26)==0);
    Require(halljoy::mini60::Milli(369,34)==1000 && halljoy::mini60::Milli(170,0)==0 && halljoy::mini60::Milli(65535,34)==0);
    Require(halljoy::mini60::Read(halljoy::mini60::Pack(170,34,0xfffffff0u),10)==500);
    Require(halljoy::mini60::Read(halljoy::mini60::Pack(170,34,0xfffffff0u),100)==0);
    std::array<std::uint8_t,4> assign{2,0,26,0};Require(halljoy::mini60::Assigned(49,assign.data())==26);
    assign={2,1,26,0};Require(halljoy::mini60::Assigned(49,assign.data())==0);
    assign={2,1,0,0};Require(halljoy::mini60::Assigned(49,assign.data())==224);
    assign={6,0,0,0};Require(halljoy::mini60::Assigned(49,assign.data())==0);
    assign={2,0,0xaf,0};Require(halljoy::mini60::Assigned(85,assign.data())==0x409);
    assign={2,1,0xaf,0};Require(halljoy::mini60::Assigned(85,assign.data())==0);
    Coverage c;
    c.Update(7,4,8,8,0,1000,20,2,1,100);Require(c.phase==0);
    c.Update(8,4,8,8,2,1000,20,1,1,200);Require(c.phase==1);
    c.Update(8,4,8,8,2,1000,20,2,1,300);Require(c.phase==2);
    c.Update(8,4,8,8,0,1000,20,2,1,400);Require(c.phase==3 && !c.complete);
    c.Update(8,4,8,8,1,1000,20,2,1,500);Require(c.phase==2 && !c.complete);
    c.Update(8,4,8,8,0,1000,20,2,1,600);c.Update(8,4,8,8,0,1000,20,2,1,1600);Require(c.complete);
    auto r=Request(0x10,56);Require(r[0]==0 && r[1]==0xaa && r[3]==56 && r[7]==1);
    for(unsigned command=0;command<256;++command)if(command!=0x66 && command!=0x67)Require(Request(command)[1]==0);
    Require(Request(0x12,56,500)[1]==0);Require(Request(0x12,56,56)[7]==0);Require(Request(0x12,8,504)[7]==1);
    std::array<std::uint8_t,65> report{};report[1]=0x55;report[2]=0xfb;report[3]=7;
    report[5]=0x98;report[6]=8;report[7]=0x6c;report[8]=0x87;report[9]=0x20;report[10]=8;report[11]=170;report[13]=34;
    Sample sample{};Require(Decode(report,sample) && sample.key==7 && sample.low==1900 && sample.travel==170);
    Require(!Decode(std::span(report).first(14),sample));report[0]=1;Require(!Decode(report,sample));report[0]=0;
    report[3]=126;Require(!Decode(report,sample));report[3]=7;report[4]=2;Require(!Decode(report,sample));report[4]=0;
    Metrics m;Require(m.Add(report,100,0));report[3]=16;report[11]=93;Require(m.Add(report,110,1));Require(m.recentKeysMax==2);
    report[3]=7;report[11]=43;Require(m.Add(report,120,1));report[11]=0;Require(m.Add(report,500,3));
    Require(m.keys[7].decrease==2 && m.keys[7].zero==1 && m.keys[7].gap250==1 && m.Seen()==2 && m.Varying()==1);
    Require(!Enough(8,4,8,8,2,false) && !Enough(8,4,8,7,2,true) && Enough(8,4,8,8,2,true));
    auto info=report;info[2]=0x10;info[3]=56;info[4]=0;info[5]=0;auto payload=Payload(info);
    Require(Reply(payload,0x10,56,0) && !Reply(payload,0x10,55,0) && !Reply(payload,0x10,56,56));
    pipeTestSeen=false;Require(RunChild(L"test-pipe",1500)==0 && pipeTestSeen);
    auto start=GetTickCount64();Require(RunChild(L"test-hang",30)==101);Require(GetTickCount64()-start<5000);
    Require(RunChild(L"test-cancel",30)==0);
    Require(RunChild(L"test-failure",1500)==19);
    Line("MINI60_SELF_TEST=PASS native_publication=1 registry_to_gamepad=1 independent_expiry=1 aliases=1 normalization=1 rollover=1 adaptive_completion=1 protocol=1 aggregate_metrics=1 sufficiency=1 pipe=1 forced_timeout=1 cooperative_cancel=1 child_failure=1 hardware_access=0");
}
} // namespace
bool Mini60Diagnostic_TryRunCommand(int& result) noexcept {
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return false;
    bool worker=argc>=5 && wcscmp(argv[1],L"--halljoy-mini60-worker")==0;
    bool test=argc==2 && wcscmp(argv[1],L"--halljoy-mini60-self-test")==0;
    if(!worker && !test){LocalFree(argv);return false;}
    try{
        if(test){CreateShared();SelfTest(0x80a2);SelfTest(0x80a1);SelfTest(0x8032);result=0;}
        else{
            childCancel=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[3],nullptr,10)));
            DWORD flags=0;if(!childCancel || !GetHandleInformation(childCancel,&flags))throw 1;
            shared=static_cast<Shared*>(MapViewOfFile(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[4],nullptr,10))),FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
            if(!shared)throw 1;
            std::wstring mode=argv[2];
            if(mode==L"test-pipe"){Line("pipe_test complete_record=1");result=0;}
            else if(mode==L"test-hang"){Sleep(INFINITE);result=19;}
            else if(mode==L"test-cancel"){while(!Stop())Sleep(5);result=0;}
            else if(mode==L"test-failure")result=19;
            else if(mode==L"capture" || mode==L"cleanup" || mode==L"play"){liveWorker=mode==L"play";result=Capture(mode==L"cleanup");}
            else result=19;
        }
    }catch(...){Line("worker_exception partial_log_retained=1");result=20;}
    LocalFree(argv);return true;
}
void Mini60Diagnostic_Start() noexcept {
    if(supervisor.joinable())return;
    #if !defined(HALLJOY_AULA_MINI60_NATIVE)
    if(!StabilityTrace_IsEnabled()){uiState=10;return;}
#endif
    parentStop=false;collecting=false;uiState=1;
    StabilityTrace_WriteCritical(L"INFO",L"mini60",L"start",L"schema=2 build=20260919-native-2 mode=continuous_analog no_test_deadline=1 stale_release_ms=50 scope=0c45:80a2,80a1,8032:ff68:0061 wireless_commands=0 gamepad_unchanged=1 firmware_writes=0 calibration=0 typed_text=0");
    try{CreateShared();
        InterlockedExchange(&shared->nativeReady,0);InterlockedExchange(&shared->mapReady,0);
        keyCount=0;releaseCount=0;maximumHeld=0;
        {std::lock_guard<std::mutex> lock(digitalMutex);digital.clear();}
        InterlockedExchange(&shared->keys,0);InterlockedExchange(&shared->released,0);InterlockedExchange(&shared->held,0);
        collecting=true;supervisor=std::thread(Supervise);
    }catch(...){collecting=false;uiState=10;}
}
void Mini60Diagnostic_Stop() noexcept {
    parentStop=true;collecting=false;if(supervisor.joinable())supervisor.join();DigitalSummary();ClearNative();
}
void Mini60Diagnostic_UpdateWindow(HWND window) noexcept {
#if defined(HALLJOY_AULA_MINI60_NATIVE)
    // Normal application title is not owned by a keyboard backend.
    (void)window;return;
#endif
    static unsigned previous=99;unsigned state=uiState.load();
    if(state && !StabilityTrace_IsEnabled())state=10;
    if(state==previous)return;previous=state;
    const wchar_t* titles[]={L"HallJoy",L"HallJoy - MINI60: connect the keyboard by USB (not the receiver)",
        L"HallJoy - MINI60: checking firmware and connection...",
        L"HallJoy - MINI60: press/release 8+ keys at varied depths",
        L"HallJoy - MINI60: hold 2+ keys; vary one key's depth",
        L"HallJoy - MINI60: slowly release ALL keys",
        L"HallJoy - MINI60: checking releases...",
        L"HallJoy - MINI60: sufficient diagnostic data. Close and send HallJoy.log",
        L"HallJoy - MINI60: completed with limitations. Close and send HallJoy.log",
        L"HallJoy - MINI60: wired keyboard not found. Send HallJoy.log",
        L"HallJoy - MINI60: logging or diagnostic error. Use a writable folder and restart",
        L"HallJoy - MINI60: close AULA software to free USB; retrying...",
        L"HallJoy - MINI60: analog active; gameplay log enabled",
        L"HallJoy - MINI60: reconnecting; analog cleared"};
    SetWindowTextW(window,titles[std::min(state,13u)]);
}
void Mini60Diagnostic_DeviceChanged(HANDLE device) noexcept {
    if(!collecting)return;
    std::lock_guard<std::mutex> lock(digitalMutex);auto found=digital.find(device);
    if(found!=digital.end()){
        if(found->second.target)StabilityTrace_Write(L"INFO",L"mini60",L"digital_device_changed",L"held_discarded=%u presses=%u releases=%u",found->second.heldCount,found->second.presses,found->second.releases);
        digital.erase(found);
        unsigned held=0;for(const auto& entry:digital)if(entry.second.target)held+=entry.second.heldCount;
        if(shared){InterlockedExchange(&shared->held,held);InterlockedExchange(&shared->lastChange,static_cast<LONG>(GetTickCount()));}
    }
}
void Mini60Diagnostic_ObserveRawInput(HRAWINPUT input) noexcept {
    if(!collecting)return;
    try{
        RAWINPUT raw{};UINT bytes=sizeof(raw);
        if(GetRawInputData(input,RID_INPUT,&raw,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1) || raw.header.dwType!=RIM_TYPEKEYBOARD || bytes<sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD))return;
        std::lock_guard<std::mutex> lock(digitalMutex);auto found=digital.find(raw.header.hDevice);
        if(found==digital.end()){
            if(digital.size()>=64)return;
            wchar_t path[2048]{};UINT n=_countof(path);Digital d;
            if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,path,&n)!=UINT(-1)){
                std::wstring value=path;std::transform(value.begin(),value.end(),value.begin(),towlower);
                d.target=value.find(L"vid_0c45&pid_8032")!=std::wstring::npos || value.find(L"vid_0c45&pid_80a2")!=std::wstring::npos || value.find(L"vid_0c45&pid_80a1")!=std::wstring::npos;
            }
            found=digital.emplace(raw.header.hDevice,d).first;
        }
        auto& d=found->second;if(!d.target)return;
        const auto& k=raw.data.keyboard;if(k.MakeCode==0 || k.MakeCode==KEYBOARD_OVERRUN_MAKE_CODE)return;
        unsigned index=(k.MakeCode&255u)|((k.Flags&RI_KEY_E0)?256:0)|((k.Flags&RI_KEY_E1)?512:0);
        bool up=(k.Flags&RI_KEY_BREAK)!=0;bool transition=up?d.held[index]:!d.held[index];
        if(up){if(d.held[index]){d.held[index]=false;--d.heldCount;++d.releases;if(!d.released[index]){d.released[index]=true;++releaseCount;}}
            else ++d.orphanReleases;}
        else if(d.held[index])++d.repeats;
        else{d.held[index]=true;++d.heldCount;++d.presses;if(!d.seen[index]){d.seen[index]=true;++keyCount;}maximumHeld=std::max(maximumHeld.load(),d.heldCount);}
        if(shared){
            unsigned held=0;for(const auto& entry:digital)if(entry.second.target)held+=entry.second.heldCount;
            InterlockedExchange(&shared->keys,keyCount.load());InterlockedExchange(&shared->released,releaseCount.load());
            InterlockedExchange(&shared->held,held);
            if(transition)InterlockedExchange(&shared->lastChange,static_cast<LONG>(GetTickCount()));
        }
    }catch(...){StabilityTrace_Write(L"WARN",L"mini60",L"raw_input_error",L"coverage_may_be_incomplete=1");}
}

#if defined(HALLJOY_AULA_MINI60_NATIVE)
const NativeAnalogBackendDescriptor& Mini60_GetNativeBackendDescriptor(){
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
        "aula-mini60-he-pro",L"AULA MINI60 HE / Pro / MAX",NativeAnalogProtocol::AulaMini60HePro,
        NativeAnalogStartPhase::AfterRawInput,
        NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReversibleControlProbe|NativeAnalogBackendFlag_RequiresRawInput,
        nullptr,&NativeStart,&NativeStop,nullptr,&NativeConnected,&NativeConnected,&NativeOwns,&NativeGet,&NativeTelemetry};
    return descriptor;
}
#endif
#endif
