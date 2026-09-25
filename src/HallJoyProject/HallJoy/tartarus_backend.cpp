#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>
#include "tartarus_backend.h"
#include "tartarus_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "generated/layout_pipeline/identities.h"

namespace {
namespace tp=halljoy::tartarus;
constexpr auto kToken=halljoy::layout_identity::Token("razer-tartarus","0244");
constexpr std::uint64_t kHold=~std::uint64_t{0};
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
struct Candidate {std::wstring path; HIDP_CAPS caps{};};
bool SynapseRunning() {
 Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
 if(!snapshot)return false;
 PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
 if(!Process32FirstW(snapshot.v,&entry))return false;
 do {
  if(!_wcsicmp(entry.szExeFile,L"RazerAppEngine.exe") ||
     !_wcsicmp(entry.szExeFile,L"Razer Synapse 3.exe"))return true;
 }while(Process32NextW(snapshot.v,&entry));
 return false;
}
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
  if(!HidD_GetAttributes(meta.v,&a) || a.VendorID!=tp::kVid || a.ProductID!=tp::kPid)continue;
  PHIDP_PREPARSED_DATA pp=nullptr;if(!HidD_GetPreparsedData(meta.v,&pp))continue;
  HIDP_CAPS caps{};bool valid=false;
  if(HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS && caps.InputReportByteLength>=21 &&
     caps.InputReportByteLength<=1024 && caps.NumberInputValueCaps && caps.NumberInputValueCaps<=128) {
   std::vector<HIDP_VALUE_CAPS> values(caps.NumberInputValueCaps);USHORT count=caps.NumberInputValueCaps;
   if(HidP_GetValueCaps(HidP_Input,values.data(),&count,pp)==HIDP_STATUS_SUCCESS) {
    unsigned depthBytes=0;bool shape=true;
    for(unsigned n=0;n<count;++n)if(values[n].ReportID==6) {
     if(values[n].BitSize!=8)shape=false;
     depthBytes+=values[n].ReportCount;
    }
    valid=shape && depthBytes>=20;
   }
  }
  HidD_FreePreparsedData(pp);
  if(valid)out.push_back({detail->DevicePath,caps});
 }
 SetupDiDestroyDeviceInfoList(set);return out;
}
void Clear() {
 g_connected.store(false);g_values.Clear();g_last.store(0);
}
bool Bind() {
 for(unsigned i=0;i<tp::kFactoryHids.size();++i)
  if(!g_values.Bind(static_cast<std::uint8_t>(i+1),tp::kFactoryHids[i]))return false;
 return true;
}
void Run(const Candidate& c) {
 Clear();
 // Synapse owns device mode. This backend has no output or feature writes.
 Handle input(CreateFileW(c.path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                         nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
 if(!input) {++g_bad;DebugLog_Write(L"[tartarus] input open failed error=%lu",GetLastError());return;}
 if(!HidD_SetNumInputBuffers(input.v,128) || !Bind()) {++g_bad;Clear();return;}
 g_bytes.store(c.caps.InputReportByteLength);g_page.store(c.caps.UsagePage);g_usage.store(c.caps.Usage);
 std::array<unsigned char,1024> buffer{};
 HidIoOperation read(input.v); // Cancel/drain before buffer and input handle destruction.
 std::uint64_t nextSynapseCheck=0;
 while(!g_stop.load()) {
  DWORD error=0,done=0;
  if(GetTickCount64()>=nextSynapseCheck) {
   if(!SynapseRunning())break;
   nextSynapseCheck=GetTickCount64()+1000;
  }
  const auto start=read.StartRead(buffer.data(),c.caps.InputReportByteLength,&error);
  if(start==HidIoOperation::StartResult::Failed) {++g_bad;break;}
  bool abandon=false;
  if(start==HidIoOperation::StartResult::Pending) {
   HANDLE events[]={read.Event(),g_wake};
   while(!g_stop.load()) {
    const auto wait=WaitForMultipleObjects(2,events,FALSE,1000);
    if(wait==WAIT_OBJECT_0)break;
    if(wait==WAIT_FAILED) {++g_bad;abandon=true;break;}
    if(GetTickCount64()>=nextSynapseCheck) {
     if(!SynapseRunning()) {abandon=true;break;}
     nextSynapseCheck=GetTickCount64()+1000;
    }
   }
  }
  if(abandon || g_stop.load()) {read.CancelAndDrain(&done,&error);break;}
  if(!read.Finish(&done,&error,false)) {++g_bad;break;}
  if(done && buffer[0]!=6)continue;
  tp::Values values{};
  if(!tp::Parse(buffer.data(),done,values)) {++g_bad;break;}
  if(!g_connected.load()) {
   if(!NativeAnalogRouting_Claim(tp::kVid,tp::kPid,c.path.c_str(),NativeAnalogProtocol::TartarusPro) &&
      !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::TartarusPro))break;
   DebugLog_Write(L"[tartarus] analog stream verified keys=20 report=6 Synapse=1");
  }
  const auto now=GetTickCount64();
  for(unsigned i=0;i<values.size();++i)g_values.Publish(static_cast<std::uint8_t>(i+1),values[i],now);
  g_last.store(now);++g_ok;g_connected.store(true);
 }
 Clear();
}
unsigned __stdcall Worker(void*) {
 try {
  while(!g_stop.load()) {
   const auto devices=Enumerate();g_present.store(!devices.empty());
   if(SynapseRunning())for(const auto& c:devices) {
    if(g_stop.load())break;
    if(NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::TartarusPro))continue;
    Run(c);
   }
   if(!g_stop.load())WaitForSingleObject(g_wake,devices.empty()?INFINITE:1000);
  }
 }catch(...) {++g_bad;DebugLog_Write(L"[tartarus] worker exception; input cleared");}
 Clear();g_running.store(false);return 0;
}
bool Prepare() {
 std::lock_guard<std::mutex> lock(g_service);
 if(!g_thread)g_present.store(!Enumerate().empty());
 // Claim only after a valid report in Run; the bundled UAP has no Tartarus path.
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
 out->present=Present();out->connected=Connected();out->verifiedLayoutToken=Connected()?kToken:0;
 out->vendorId=tp::kVid;out->productId=tp::kPid;out->nominalRawLevels=256;
 out->mappedKeys=Connected()?20:0;out->inputReportBytes=g_bytes.load();
 out->usagePage=static_cast<std::uint16_t>(g_page.load());out->usage=static_cast<std::uint16_t>(g_usage.load());
 const auto now=GetTickCount64(),last=g_last.load();
 if(Connected())for(auto hid:tp::kFactoryHids)if(g_values.Read(hid,now,kHold).milli)++out->activeKeys;
 out->successfulUpdates=g_ok.load();out->failedUpdates=g_bad.load();
 out->lastUpdateAgeMs=last && now>=last?static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff,now-last)):0;
 wcscpy_s(out->deviceName,L"Razer Tartarus Pro");
 wcscpy_s(out->status,Connected()?L"Tartarus Pro: 20 physical analog keys; hardware test pending":
                              L"Tartarus Pro: requires Razer Synapse and an analog report");
}
}
const NativeAnalogBackendDescriptor& Tartarus_GetNativeBackendDescriptor() {
 static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
 "razer-tartarus",L"Razer Tartarus Pro (experimental)",NativeAnalogProtocol::TartarusPro,
 NativeAnalogStartPhase::BeforeUap,NativeAnalogBackendFlag_StreamTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
 &Prepare,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};
 return d;
}
