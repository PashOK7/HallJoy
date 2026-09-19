#if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
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
#include <cstdarg>
#include <cwctype>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "attackshark_pro_diagnostic.h"
#include "attackshark_pro_diagnostic_model.h"
#include "stability_trace.h"
namespace {
using namespace halljoy::sharkdiag;
struct Handle {
 HANDLE h=INVALID_HANDLE_VALUE;
 explicit Handle(HANDLE value=INVALID_HANDLE_VALUE):h(value){}
 ~Handle(){if(h && h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 explicit operator bool()const{return h && h!=INVALID_HANDLE_VALUE;}
};
struct Shared {volatile LONG topology=0,presses=0,releases=0,held=0,wasd=0,command=0,page=0,ioPhase=0;};
Shared* shared=nullptr;Handle sharedMapping;
std::atomic<bool> parentStop{false},collecting{false};std::atomic<unsigned> uiState{0};
std::thread supervisor;HANDLE childCancel=nullptr;bool pipeTestSeen=false;
LONG Load(volatile LONG& v){return InterlockedCompareExchange(&v,0,0);}
void CreateShared(){
 if(shared)return;SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
 sharedMapping.h=CreateFileMappingW(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(Shared),nullptr);
 if(!sharedMapping)throw 1;
 shared=static_cast<Shared*>(MapViewOfFile(sharedMapping.h,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
 if(!shared)throw 1;
}
bool Stop(){return childCancel && WaitForSingleObject(childCancel,0)==WAIT_OBJECT_0;}
void Line(const std::string& value){DWORD n=0;auto text=value+"\n";WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),static_cast<DWORD>(text.size()),&n,nullptr);}
void Event(const char* name,unsigned code){Line(std::string(name)+" error="+std::to_string(code));}
std::wstring ThisExe(){wchar_t path[32768]{};auto n=GetModuleFileNameW(nullptr,path,_countof(path));if(!n || n>=_countof(path))throw 1;return {path,n};}
std::wstring Quote(const std::wstring& s){return L"\""+s+L"\"";}
struct Device {std::wstring path;unsigned pid=0,usage=0;};
std::vector<Device> Find(){
 std::vector<Device> result;unsigned vendorCollections=0,metadataErrors=0,rejected=0;GUID guid{};HidD_GetHidGuid(&guid);
 auto list=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(list==INVALID_HANDLE_VALUE){Event("inventory_failed",GetLastError());return result;}
 struct Guard{HDEVINFO h;~Guard(){SetupDiDestroyDeviceInfoList(h);}} guard{list};
 for(DWORD i=0;i<512 && !Stop();++i){
  SP_DEVICE_INTERFACE_DATA item{};item.cbSize=sizeof(item);
  if(!SetupDiEnumDeviceInterfaces(list,nullptr,&guid,i,&item))break;
  DWORD size=0;SetupDiGetDeviceInterfaceDetailW(list,&item,nullptr,0,&size,nullptr);
  if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size>65536)continue;
  std::vector<unsigned char> memory(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(memory.data());detail->cbSize=sizeof(*detail);
  if(!SetupDiGetDeviceInterfaceDetailW(list,&item,detail,size,nullptr,nullptr))continue;
  std::wstring path=detail->DevicePath,lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),towlower);
  if(lower.find(L"vid_3151&pid_")==std::wstring::npos)continue;
  ++vendorCollections;
  Handle meta(CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
  if(!meta){++metadataErrors;Event("metadata_open_failed",GetLastError());continue;}
  HIDD_ATTRIBUTES a{};a.Size=sizeof(a);if(!HidD_GetAttributes(meta.h,&a)){++metadataErrors;Event("attributes_failed",GetLastError());continue;}if(a.VendorID!=0x3151)continue;
  PHIDP_PREPARSED_DATA prep=nullptr;HIDP_CAPS caps{};if(!HidD_GetPreparsedData(meta.h,&prep)){++metadataErrors;Event("preparsed_data_failed",GetLastError());continue;}
  auto status=HidP_GetCaps(prep,&caps);HidD_FreePreparsedData(prep);if(status!=HIDP_STATUS_SUCCESS){++metadataErrors;Event("caps_failed",static_cast<unsigned>(status));continue;}
  wchar_t product[256]{};bool receiver=false;
  if(HidD_GetProductString(meta.h,product,sizeof(product))){std::wstring name=product;std::transform(name.begin(),name.end(),name.begin(),towlower);receiver=name.find(L"receiver")!=std::wstring::npos || name.find(L"dongle")!=std::wstring::npos;}
  char line[256];sprintf_s(line,"descriptor vid=3151 pid=%04x bcd=%04x usage=%04x:%04x in=%u out=%u feature=%u receiver=%u",a.ProductID,a.VersionNumber,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength,caps.FeatureReportByteLength,receiver?1:0);Line(line);
  const char* reason=receiver?"receiver":(a.ProductID!=0x502f && a.ProductID!=0x5030)?"unknown_pid":caps.UsagePage!=0xffff?"usage_page":caps.Usage!=2?"usage":caps.FeatureReportByteLength!=65?"feature_length":nullptr;
  if(reason){++rejected;Line(std::string("collection_rejected reason=")+reason);}else result.push_back({path,a.ProductID,caps.Usage});
 }
 std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.usage>b.usage;});
 Line("inventory eligible="+std::to_string(result.size())+" vendor_collections="+std::to_string(vendorCollections)+" metadata_errors="+std::to_string(metadataErrors)+" rejected="+std::to_string(rejected)+" paths_logged=0");return result;
}
struct Session {
 Handle device;unsigned delay=1,errors=0,identity=0;std::uint64_t calls=0,totalUs=0,maxUs=0;ULONGLONG began=GetTickCount64();
 explicit Session(const Device& d):device(CreateFileW(d.path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr)){
  if(!device)Event("exclusive_open_failed",GetLastError());
 }
 bool Exchange(unsigned command,unsigned page,Report& reply){
  const auto request=Request(command,page);if(!request[1])return false;
  LARGE_INTEGER start{},end{},freq{};QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);
  // Match the vendor send delay as well as its separate read delay.
  if(WaitForSingleObject(childCancel,delay)==WAIT_OBJECT_0)return false;
  InterlockedExchange(&shared->command,command);InterlockedExchange(&shared->page,page);InterlockedExchange(&shared->ioPhase,1);
  if(!HidD_SetFeature(device.h,const_cast<std::uint8_t*>(request.data()),65)){++errors;Event("set_feature_failed",GetLastError());return false;}
  if(WaitForSingleObject(childCancel,delay)==WAIT_OBJECT_0)return false;
  reply.fill(0xff);reply[0]=0;
  InterlockedExchange(&shared->ioPhase,2);
  if(!HidD_GetFeature(device.h,reply.data(),65)){++errors;Event("get_feature_failed",GetLastError());return false;}
  InterlockedExchange(&shared->ioPhase,0);
  QueryPerformanceCounter(&end);auto us=static_cast<std::uint64_t>((end.QuadPart-start.QuadPart)*1000000/freq.QuadPart);
  ++calls;totalUs+=us;maxUs=std::max(maxUs,us);return true;
 }
 bool Identify(const Device& d,unsigned& usb){
  for(unsigned attempt=0;attempt<3;++attempt){delay=10;Report a{},b{};
   if(Stop())return false;
   if(Exchange(0x8f,0,a) && Exchange(0x8f,0,b) && Identity(a)==Identity(b) && Known(Identity(a),d.pid)){
    identity=Identity(a);usb=unsigned(b[8])|(unsigned(b[9])<<8);return true;}
   Line("identity_attempt delay_ms="+std::to_string(delay)+" id="+std::to_string(Identity(a))+" repeated_id="+std::to_string(Identity(b))+" reply_command="+std::to_string(a[1])+" repeated_command="+std::to_string(b[1])+" report_id="+std::to_string(b[0]));
  }return false;
 }
};
void Summary(const Metrics& m,const Session& s,bool detail){
 Line(std::string(detail?"summary":"checkpoint")+" dev_id="+std::to_string(s.identity)+" elapsed_ms="+std::to_string(GetTickCount64()-s.began)+" page0="+std::to_string(m.pages[0])+" page1="+std::to_string(m.pages[1])+" page2="+std::to_string(m.pages[2])+" page3="+std::to_string(m.pages[3])+" varying="+std::to_string(m.Varying())+" max_page0_active="+std::to_string(m.peakPage0)+" digital_chord_frames="+std::to_string(m.chordFrames)+" independent_changes="+std::to_string(m.independentChanges)+" malformed="+std::to_string(m.invalid)+" io_errors="+std::to_string(s.errors)+" avg_exchange_us="+std::to_string(s.calls?s.totalUs/s.calls:0)+" max_exchange_us="+std::to_string(s.maxUs)+" delay_ms="+std::to_string(s.delay)+" digital_presses="+std::to_string(Load(shared->presses))+" digital_releases="+std::to_string(Load(shared->releases))+" digital_held="+std::to_string(Load(shared->held)));
 for(unsigned i=0;i<128;++i){const auto& k=m.keys[i];if(!k.count || (!detail && !k.positive && !k.changed))continue;
  Line("key slot="+std::to_string(i)+" samples="+std::to_string(k.count)+" min="+std::to_string(k.low)+" max="+std::to_string(k.high)+" last="+std::to_string(k.last)+" positive="+std::to_string(k.positive)+" zero="+std::to_string(k.zero)+" changed="+std::to_string(k.changed)+" rises="+std::to_string(k.increase)+" falls="+std::to_string(k.decrease)+" releases="+std::to_string(k.released));
 }
}
int Capture(){
 for(;;){
  const auto topology=Load(shared->topology);auto devices=Find();
  if(devices.empty()){
   Line("status state=1");auto heartbeat=GetTickCount64();
   while(!Stop() && Load(shared->topology)==topology){Sleep(100);if(GetTickCount64()-heartbeat>=1000){Line("waiting_for_device");heartbeat=GetTickCount64();}}
   if(Stop()){Line("session_end outcome=no_eligible_device analog_samples=0 settings_unchanged=1");return 4;}continue;
  }
  for(const auto& d:devices){
   if(Stop())return 4;Session s(d);if(!s.device){Line("status state=5");continue;}
   unsigned usb=0;if(!s.Identify(d,usb)){Line("identity_rejected no_depth_commands=1");continue;}
   unsigned rf=0;Report version{};if(s.Exchange(0x80,0,version) && s.Exchange(0x80,0,version) && version[1]==0x80)rf=unsigned(version[2])|(unsigned(version[3])<<8);
   unsigned selected=rf?rf:usb;unsigned units=selected>=0x500?200:selected>=0x300?100:10;
   Line("identity dev_id="+std::to_string(s.identity)+" pid="+std::to_string(d.pid)+" usb_version="+std::to_string(usb)+" rf_version="+std::to_string(rf)+" driver_units_per_mm="+std::to_string(units)+" exclusive=1 discard_first_page_reply=1 digital_scope=vidpid stream_command_sent=0 calibration_sent=0 writes_to_settings=0");
   s.delay=1;Line("status state=2");Metrics m;unsigned cycle=0,invalidStreak=0,lastEscalation=0;bool sufficient=false;
   auto checkpoint=GetTickCount64(),detail=checkpoint+15000;
   while(!Stop()){
    // WASD page is revisited between the other pages. Never treat four pages as an atomic snapshot.
    const unsigned pages[]={0,1,0,2,0,3};unsigned page=pages[cycle%6];Report response{};
    // Raw replies have no page tag. Discard the first reply after changing pages,
    // then request the same page again to avoid accepting a one-request-late page.
    if(!s.Exchange(0xe5,page,response) || !s.Exchange(0xe5,page,response)){if(Stop())break;Summary(m,s,true);Line("status state=6");return 6;}
    if(Identity(response) && Known(Identity(response),d.pid)){
     ++m.invalid;++invalidStreak;Line("stale_identity_reply page="+std::to_string(page));
    }else if(m.Add(page,response,static_cast<unsigned>(Load(shared->wasd))))invalidStreak=0;else {
     ++invalidStreak;
     if(invalidStreak<=3){unsigned maxRaw=0,bad=0;for(unsigned i=0;i<32;++i){unsigned v=unsigned(response[1+2*i])|(unsigned(response[2+2*i])<<8);maxRaw=std::max(maxRaw,v);if(v>4096)++bad;}
      Line("page_rejected page="+std::to_string(page)+" report_id="+std::to_string(response[0])+" max_raw="+std::to_string(maxRaw)+" implausible_slots="+std::to_string(bad));}
    }
    // Retry only documented timing, never stream/calibration modes. Zero alone is not a fault.
    const auto presses=static_cast<unsigned>(Load(shared->presses));
    if(invalidStreak>=3 || (!m.Varying() && presses>=lastEscalation+4)){
     if(s.delay<10){s.delay=s.delay<5?5:10;lastEscalation=presses;invalidStreak=0;Line("timing_retry delay_ms="+std::to_string(s.delay));}
     else if(invalidStreak>=3){Summary(m,s,true);Line("status state=6");return 7;}
    }
    if(!sufficient && m.Enough(presses,static_cast<unsigned>(Load(shared->releases)))){sufficient=true;Line("status state=3");}
    if((++cycle%120)==0){Report proof{};const unsigned depthDelay=s.delay;s.delay=10;const bool valid=s.Exchange(0x8f,0,proof) && s.Exchange(0x8f,0,proof) && Identity(proof)==s.identity;s.delay=depthDelay;if(!valid){if(Stop())break;Summary(m,s,true);Line("identity_lost");return 8;}}
    const auto now=GetTickCount64();if(now>=checkpoint){Summary(m,s,false);checkpoint=now+2000;}
    if(now>=detail){Summary(m,s,true);detail=now+15000;}
   }
   Summary(m,s,true);Line("session_end cancelled=1 settings_unchanged=1 sufficient="+std::to_string(sufficient));return 4;
  }
  Line("status state=5");Line("candidates_unavailable retry_on_close_driver_or_reconnect=1");
  for(unsigned i=0;i<30 && !Stop();++i)Sleep(100);if(Stop())return 4;
 }
}
void ReceiveLine(const std::string& line){
 if(line=="waiting_for_device")return;
 if(line.size()>2000){StabilityTrace_AppendPlain(L"[shark] oversized_worker_record dropped=1");return;}
 std::wstring wide(line.begin(),line.end());StabilityTrace_AppendPlain((L"[shark] "+wide).c_str());
 unsigned state=0;if(sscanf_s(line.c_str(),"status state=%u",&state)==1 && state<=6)uiState=state;
 if(line=="pipe_test complete_record=1")pipeTestSeen=true;
}

void Trace(const wchar_t* format,...){wchar_t line[1024]{};va_list ap;va_start(ap,format);_vsnwprintf_s(line,_countof(line),_TRUNCATE,format,ap);va_end(ap);StabilityTrace_AppendPlain(line);}
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
    auto exe=ThisExe();auto command=Quote(exe)+L" --halljoy-shark-worker "+Quote(mode)+L" "+
        std::to_wstring(reinterpret_cast<uintptr_t>(cancel.h))+L" "+std::to_wstring(reinterpret_cast<uintptr_t>(sharedMapping.h));
    PROCESS_INFORMATION pi{};
    const bool created=ready && CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&pi);
    const DWORD creationError=created?0:GetLastError();DeleteProcThreadAttributeList(attributes);
    if(!created){Trace(L"[shark] child_start_error=%lu",creationError);return 100;}
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
        const auto now=GetTickCount64();
        if(mode==L"capture" && !StabilityTrace_IsEnabled())parentStop=true;
        if(!cancelledAt && (now>=end || ((mode==L"capture" || mode==L"play") && now-lastOutput>8000) || (parentStop && !allowCancelled))){SetEvent(cancel.h);cancelledAt=now;}
        if(cancelledAt && now-cancelledAt>=1200 && WaitForSingleObject(process.h,0)!=WAIT_OBJECT_0){
            TerminateProcess(process.h,101);forced=true;
            if(WaitForSingleObject(process.h,1000)!=WAIT_OBJECT_0){
                Trace(L"[shark] child_unreaped stop_retrying_device=1");return 103;
            }
        }
        Sleep(mode==L"play"?1:2);
    }
    if(!pending.empty())ReceiveLine(pending);
    DWORD code=100;GetExitCodeProcess(process.h,&code);
    Trace(L"[shark] worker mode=%s exit=%lu forced=%d",mode.c_str(),code,forced?1:0);
    return code;
}


void Supervise() noexcept {
 try{do{const auto result=RunChild(L"capture",MAXDWORD);Trace(L"[shark] worker_end exit=%lu last_command=%ld last_page=%ld io_phase=%ld",result,Load(shared->command),Load(shared->page),Load(shared->ioPhase));
  if(parentStop || result==103)break;uiState=6;for(unsigned i=0;i<30 && !parentStop;++i)Sleep(100);
 }while(!parentStop);}catch(...){uiState=6;Trace(L"[shark] supervisor_exception partial_log_retained=1");}
 collecting=false;
}
struct Digital {bool target=false;std::array<bool,1024> held{};unsigned count=0,presses=0,releases=0;};
std::mutex digitalMutex;std::map<HANDLE,Digital> digital;
unsigned retiredPresses=0,retiredReleases=0;
void PublishDigital(){
 unsigned held=0,presses=retiredPresses,releases=retiredReleases,wasd=0;
 for(const auto& [handle,d]:digital){(void)handle;if(!d.target)continue;held+=d.count;presses+=d.presses;releases+=d.releases;
  if(d.held[17])wasd|=1;if(d.held[30])wasd|=2;if(d.held[31])wasd|=4;if(d.held[32])wasd|=8;}
 if(shared){InterlockedExchange(&shared->held,held);InterlockedExchange(&shared->presses,presses);InterlockedExchange(&shared->releases,releases);InterlockedExchange(&shared->wasd,wasd);}
}
void Require(bool v){if(!v)throw 1;}
void SelfTest(){
 Require(Request(0x1b)[1]==0 && Request(0x1c)[1]==0 && Request(0xe5,4)[1]==0);
 Require(Request(0xe5,0)[8]==0x1b && Request(0xe5,3)[8]==0x18);
 Require(Known(2308,0x502f) && Known(2938,0x5030) && !Known(2308,0x5030));
 Metrics m;Report r{};auto put=[&](unsigned slot,unsigned value){r[1+2*slot]=static_cast<std::uint8_t>(value);r[2+2*slot]=static_cast<std::uint8_t>(value>>8);};
 put(14,300);put(9,100);Require(m.Add(0,r,3));put(14,320);Require(m.Add(0,r,3));put(14,0);Require(m.Add(0,r,2));
 Require(m.keys[14].released==1 && m.keys[9].last==100 && m.independentChanges==1 && !m.Enough(4,4));
 auto invalid=Request(0xe5);Require(!m.Add(0,invalid,0));invalid.fill(255);Require(!m.Add(0,invalid,0));
 parentStop=false;Require(RunChild(L"test-pipe",1000)==0 && pipeTestSeen);
 Require(RunChild(L"test-failure",1000)==19);
 Require(RunChild(L"test-hang",20)==101);Require(RunChild(L"test-cancel",20)==0);
 Line("SHARK_SELF_TEST=PASS allowlist=1 parser=1 independent_depths=1 release=1 metrics=1 isolated_pipe=1 forced_timeout=1 cooperative_cancel=1 hardware_access=0");
}
}
bool SharkDiagnostic_TryRunCommand(int& result) noexcept {
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return false;
 bool test=argc==2 && wcscmp(argv[1],L"--halljoy-shark-self-test")==0;
 bool worker=argc==5 && wcscmp(argv[1],L"--halljoy-shark-worker")==0;
 if(!test && !worker){LocalFree(argv);return false;}
 try{
  if(test){CreateShared();SelfTest();result=0;}
  else{childCancel=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[3],nullptr,10)));DWORD flags=0;
   if(!childCancel || !GetHandleInformation(childCancel,&flags))throw 1;
   shared=static_cast<Shared*>(MapViewOfFile(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[4],nullptr,10))),FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));if(!shared)throw 1;
   std::wstring mode=argv[2];
   if(mode==L"test-pipe"){Line("pipe_test complete_record=1");result=0;}
   else if(mode==L"test-hang"){Sleep(INFINITE);result=19;}
   else if(mode==L"test-cancel"){while(!Stop())Sleep(5);result=0;}
   else if(mode==L"test-failure")result=19;
   else if(mode==L"capture")result=Capture();else result=19;
  }
 }catch(...){Line("worker_exception partial_log_retained=1");result=20;}
 LocalFree(argv);return true;
}
void SharkDiagnostic_Start() noexcept {
 if(supervisor.joinable())return;if(!StabilityTrace_IsEnabled()){uiState=4;return;}
 try{CreateShared();parentStop=false;collecting=true;uiState=1;
  StabilityTrace_WriteCritical(L"INFO",L"shark",L"start",L"build=20260919-shark-3 no_test_deadline=1 stream=0 calibration=0 firmware_writes=0 ordered_keys=0 analog_output=0");
  supervisor=std::thread(Supervise);
 }catch(...){collecting=false;uiState=6;}
}
void SharkDiagnostic_Stop() noexcept {
 parentStop=true;collecting=false;if(supervisor.joinable())supervisor.join();
 if(shared)StabilityTrace_WriteCritical(L"INFO",L"shark",L"digital_summary",L"presses=%ld releases=%ld held=%ld ordered_keys=0",Load(shared->presses),Load(shared->releases),Load(shared->held));
}
void SharkDiagnostic_DeviceChanged(HANDLE device) noexcept {
 if(!collecting)return;if(shared)InterlockedIncrement(&shared->topology);
 std::lock_guard<std::mutex> lock(digitalMutex);auto it=digital.find(device);if(it!=digital.end()){if(it->second.target){retiredPresses+=it->second.presses;retiredReleases+=it->second.releases;}digital.erase(it);}PublishDigital();
}
void SharkDiagnostic_ObserveRawInput(HRAWINPUT input) noexcept {
 if(!collecting)return;
 try{RAWINPUT raw{};UINT n=sizeof(raw);
  if(GetRawInputData(input,RID_INPUT,&raw,&n,sizeof(RAWINPUTHEADER))==UINT(-1) || n<sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD) || raw.header.dwType!=RIM_TYPEKEYBOARD)return;
  std::lock_guard<std::mutex> lock(digitalMutex);auto it=digital.find(raw.header.hDevice);
  if(it==digital.end()){if(digital.size()>=64)return;Digital d;wchar_t path[2048]{};UINT len=_countof(path);
   if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,path,&len)!=UINT(-1)){std::wstring lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),towlower);d.target=lower.find(L"vid_3151&pid_502f")!=std::wstring::npos || lower.find(L"vid_3151&pid_5030")!=std::wstring::npos;}
   it=digital.emplace(raw.header.hDevice,d).first;
  }
  auto& d=it->second;if(!d.target)return;const auto& k=raw.data.keyboard;if(!k.MakeCode || k.MakeCode==KEYBOARD_OVERRUN_MAKE_CODE)return;
  const unsigned index=(k.MakeCode&255)|((k.Flags&RI_KEY_E0)?256:0)|((k.Flags&RI_KEY_E1)?512:0);bool up=(k.Flags&RI_KEY_BREAK)!=0;
  if(up && d.held[index]){d.held[index]=false;--d.count;++d.releases;}
  if(!up && !d.held[index]){d.held[index]=true;++d.count;++d.presses;}
  PublishDigital();
 }catch(...){StabilityTrace_Write(L"WARN",L"shark",L"digital_error",L"coverage_incomplete=1");}
}
void SharkDiagnostic_UpdateWindow(HWND window) noexcept {
 static unsigned previous=99;auto state=uiState.load();if(state && !StabilityTrace_IsEnabled())state=4;
 if(previous==state)return;previous=state;
 const wchar_t* titles[]={L"HallJoy",L"HallJoy - ATTACK SHARK test: connect by USB; close the vendor driver",
  L"HallJoy - ATTACK SHARK test: press WASD; hold several and vary depth; release all",
  L"HallJoy - ATTACK SHARK: sufficient data; close and send HallJoy.log (capture continues)",
  L"HallJoy - ATTACK SHARK: cannot write HallJoy.log; use a writable folder",
  L"HallJoy - ATTACK SHARK: device busy or revision unrecognized; close driver; log retained",
  L"HallJoy - ATTACK SHARK: read interrupted; retrying; log retained"};
 SetWindowTextW(window,titles[std::min(state,6u)]);
}
#endif
