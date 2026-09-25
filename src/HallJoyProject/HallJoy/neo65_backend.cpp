#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>
#include "neo65_backend.h"
#include "neo65_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "generated/layout_pipeline/identities.h"

namespace {
namespace tp=halljoy::neo65;

constexpr std::uint64_t kHold=100;
std::atomic<unsigned> g_pid{0},g_mapped{0};
std::atomic<bool> g_stop{false},g_running{false},g_present{false},g_connected{false};
std::atomic<std::uint64_t> g_ok{0},g_bad{0},g_last{0};
std::atomic<unsigned> g_bytes{0},g_page{0},g_usage{0};
std::mutex g_service;
HANDLE g_thread=nullptr,g_wake=nullptr;
halljoy::physical_analog::Publication g_values;
struct Handle {
 HANDLE v=INVALID_HANDLE_VALUE;
 explicit Handle(HANDLE h):v(h){}
 ~Handle(){if(v!=INVALID_HANDLE_VALUE)CloseHandle(v);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 explicit operator bool() const{return v!=INVALID_HANDLE_VALUE;}
};
struct Candidate {std::wstring path; HIDP_CAPS caps{};std::uint16_t pid=0;};
std::vector<Candidate> Enumerate() {
 std::vector<Candidate> out;
 GUID guid{};HidD_GetHidGuid(&guid);
 HDEVINFO set=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(set==INVALID_HANDLE_VALUE)return out;
 for(DWORD i=0;;++i) {
  SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
  if(!SetupDiEnumDeviceInterfaces(set,nullptr,&guid,i,&iface)) {
   if(GetLastError()==ERROR_NO_MORE_ITEMS)break;
   continue;
  }
  DWORD bytes=0;SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&bytes,nullptr);
  if(bytes<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))continue;
  std::vector<unsigned char> mem(bytes);
  auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(mem.data());detail->cbSize=sizeof(*detail);
  if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,bytes,nullptr,nullptr))continue;
  Handle meta(CreateFileW(detail->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));
  if(!meta)continue;
  HIDD_ATTRIBUTES a{};a.Size=sizeof(a);
  if(!HidD_GetAttributes(meta.v,&a) || a.VendorID!=tp::kVid || (a.ProductID!=tp::kAnsi && a.ProductID!=tp::kIso))continue;
  PHIDP_PREPARSED_DATA pp=nullptr;if(!HidD_GetPreparsedData(meta.v,&pp))continue;
  HIDP_CAPS caps{};bool valid=false;
  valid=HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS && caps.InputReportByteLength==33 &&
        caps.OutputReportByteLength==33 && caps.UsagePage==0xff60 && caps.Usage==0x61;
  HidD_FreePreparsedData(pp);
  if(valid)out.push_back({detail->DevicePath,caps,a.ProductID});
 }
 SetupDiDestroyDeviceInfoList(set);return out;
}
void Clear() {
 g_connected.store(false);g_values.Clear();g_last.store(0);
}
bool Exchange(HANDLE handle,const tp::Report& request,tp::Report& reply,unsigned echoBytes) {
 // One outstanding transaction; exact echo prevents cross-page replies being misattributed.
 HidIoOperation write(handle),read(handle);DWORD done=0,error=0;
 auto finish=[&](HidIoOperation& op,HidIoOperation::StartResult start,std::uint64_t deadline) {
  if(start==HidIoOperation::StartResult::Failed)return false;
  if(start==HidIoOperation::StartResult::Pending) {
   HANDLE events[]={op.Event(),g_wake};
   while(!g_stop.load()) {
    const auto now=GetTickCount64();if(now>=deadline)break;
    const auto wait=WaitForMultipleObjects(2,events,FALSE,static_cast<DWORD>(deadline-now));
    if(wait==WAIT_OBJECT_0)return op.Finish(&done,&error,false);
    if(wait==WAIT_FAILED || wait==WAIT_TIMEOUT)break;
   }
   op.CancelAndDrain(&done,&error);return false;
  }
  return op.Finish(&done,&error,false);
 };
 const auto deadline=GetTickCount64()+100;
 if(g_stop.load() || !finish(write,write.StartWrite(request.data(),33,&error),deadline) || done!=33)return false;
 for(unsigned attempts=0;attempts<16 && !g_stop.load() && GetTickCount64()<deadline;++attempts) {
  if(!finish(read,read.StartRead(reply.data(),33,&error),deadline) || done!=33)return false;
  if(std::equal(request.begin(),request.begin()+echoBytes,reply.begin()))return true;
 }
 return false;
}
void Run(const Candidate& c) {
 Clear();
 // Exclusive vendor collection prevents another configurator changing ranges mid-session.
 Handle input(CreateFileW(c.path.c_str(),GENERIC_READ|GENERIC_WRITE,0,
                         nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
 if(!input) {++g_bad;DebugLog_Write(L"[neo65] open failed error=%lu",GetLastError());return;}
 if(!HidD_FlushQueue(input.v)) {++g_bad;return;}
 tp::Report reply{};
 auto request=tp::Request(0xa9);
 if(!Exchange(input.v,request,reply,3)) {++g_bad;return;}
 const unsigned stride=reply[3];
 if(stride<9 || stride>28) {++g_bad;return;}
 tp::Ranges ranges{};
 for(unsigned start=0;start<80;) {
  const unsigned count=std::min(80-start,28/stride);request=tp::Request(0xaa,start,count);
  if(!Exchange(input.v,request,reply,5) || !tp::ParseRanges(reply,start,count,stride,ranges)) {++g_bad;return;}
  start+=count;
 }
 const auto& map=tp::Factory(c.pid);unsigned mapped=0;
 for(unsigned i=0;i<80;++i)if(map[i]) {
  if(ranges[i].high<=ranges[i].low || !g_values.Bind(static_cast<std::uint8_t>(i+1),map[i])) {++g_bad;Clear();return;}
  ++mapped;
 }
 g_pid.store(c.pid);g_mapped.store(mapped);
 g_bytes.store(33);g_page.store(c.caps.UsagePage);g_usage.store(c.caps.Usage);
 while(!g_stop.load()) {
  tp::Matrix values{};bool valid=true;
  const auto frameStart=GetTickCount64();
  for(unsigned start=0;start<80;) {
   const unsigned count=std::min(80-start,14u);request=tp::Request(0xa6,start,count);
   if(!Exchange(input.v,request,reply,5) || !tp::ParseDepth(reply,start,count,values)) {valid=false;break;}
   start+=count;
  }
  if(!valid || GetTickCount64()-frameStart>100) {++g_bad;break;}
  if(!g_connected.load()) {
   if(!NativeAnalogRouting_Claim(tp::kVid,c.pid,c.path.c_str(),NativeAnalogProtocol::Neo65) &&
      !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::Neo65))break;
   DebugLog_Write(L"[neo65] matrix verified pid=%04x keys=%u; read-only analog commands",c.pid,mapped);
  }
  const auto now=GetTickCount64();
  for(unsigned i=0;i<80;++i)if(map[i])g_values.Publish(static_cast<std::uint8_t>(i+1),tp::Normalize(values[i],ranges[i]),now);
  g_last.store(now);++g_ok;g_connected.store(true);
  // USB replies pace the six-page snapshot; avoid an unbounded immediate-completion loop.
  if(now==frameStart)WaitForSingleObject(g_wake,1);
 }
 Clear();
}
unsigned __stdcall Worker(void*) {
 try {
  while(!g_stop.load()) {
   const auto devices=Enumerate();g_present.store(!devices.empty());
   for(const auto& c:devices) {
    if(g_stop.load())break;
    if(NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::Neo65))continue;
    Run(c);
   }
   if(!g_stop.load())WaitForSingleObject(g_wake,devices.empty()?INFINITE:1000);
  }
 }catch(...) {++g_bad;DebugLog_Write(L"[neo65] worker exception; input cleared");}
 Clear();g_running.store(false);return 0;
}
bool Prepare() {
 std::lock_guard<std::mutex> lock(g_service);
 if(!g_thread)g_present.store(!Enumerate().empty());
 // Claim only after range validation and a complete depth snapshot.
 return g_present.load();
}
bool Start() {
 std::lock_guard<std::mutex> lock(g_service);
 if(g_thread)return g_running.load();
 g_stop.store(false);g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(!g_wake)return false;
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
void Notify(){std::lock_guard<std::mutex> lock(g_service);if(g_wake)SetEvent(g_wake);}
bool Present(){return g_present.load();}
bool Connected(){return g_connected.load();}
bool Owns(std::uint16_t hid){return Connected() && g_values.Owns(hid);}
std::uint16_t Get(std::uint16_t hid){return Owns(hid)?g_values.Read(hid,GetTickCount64(),kHold).milli:0;}
void Telemetry(NativeAnalogBackendTelemetry* out) {
 if(!out)return;*out={};
 out->present=Present();out->connected=Connected();out->verifiedLayoutToken=Connected()?halljoy::layout_identity::Token("neo65",g_pid.load()==tp::kIso?"ISO":"ANSI"):0;
 out->vendorId=tp::kVid;out->productId=static_cast<std::uint16_t>(g_pid.load());out->nominalRawLevels=0;
 out->mappedKeys=Connected()?g_mapped.load():0;out->outputReportBytes=33;out->inputReportBytes=g_bytes.load();
 out->usagePage=static_cast<std::uint16_t>(g_page.load());out->usage=static_cast<std::uint16_t>(g_usage.load());
 const auto now=GetTickCount64(),last=g_last.load();
 if(Connected())for(auto hid:tp::Factory(static_cast<std::uint16_t>(g_pid.load())))if(g_values.Read(hid,now,kHold).milli)++out->activeKeys;
 out->successfulUpdates=g_ok.load();out->failedUpdates=g_bad.load();
 out->lastUpdateAgeMs=last && now>=last?static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff,now-last)):0;
 wcscpy_s(out->deviceName,L"Neo65 SONIC HE+");
 wcscpy_s(out->status,Connected()?L"Neo65: full matrix; factory positions; hardware test pending":
                              L"Neo65: waiting for validated ranges and full matrix");
}
}
const NativeAnalogBackendDescriptor& Neo65_GetNativeBackendDescriptor() {
 static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
 "neo65",L"Neo65 SONIC HE+ (experimental)",NativeAnalogProtocol::Neo65,
 NativeAnalogStartPhase::BeforeUap,NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
 &Prepare,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};
 return d;
}
