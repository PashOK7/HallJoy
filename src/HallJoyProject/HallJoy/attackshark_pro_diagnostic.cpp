#include "input_path_diagnostics.h"
#if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC) || defined(HALLJOY_ATTACKSHARK_NATIVE)
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
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cwctype>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "attackshark_pro_diagnostic.h"
#include "attackshark_pro_diagnostic_model.h"
#include "attackshark_pro_native_model.h"
#include "attackshark_pro_layout_identity.h"
#include "native_analog_backend.h"
#include "native_analog_backend_registry.h"
#include "configured_xusb_builder.h"
#include "support_log.h"
#include "settings.h"
#include "physical_analog_state.h"
#include "realtime_loop.h"
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
struct SharedPage {volatile LONG sequence=0,tick=0;volatile LONG values[32]{};};
struct Shared {volatile LONG topology=0,presses=0,releases=0,held=0,wasd=0,command=0,page=0,ioPhase=0;
 volatile LONG identity=0,usb=0,units=100,freshMs=150;SharedPage pages[4]{};};
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
// High-resolution waitable timers avoid rounding each 1 ms wait to a system tick.
Handle PreciseTimer(){return Handle(CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS));}
void Event(const char* name,unsigned code);
bool Delay(HANDLE timer,HANDLE cancel,unsigned ms){
 if(timer && timer!=INVALID_HANDLE_VALUE){LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(ms)*10000;
  if(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)){
   HANDLE handles[]={timer,cancel};const auto result=WaitForMultipleObjects(cancel?2:1,handles,FALSE,INFINITE);
   if(result==WAIT_OBJECT_0)return true;
   if(cancel && result==WAIT_OBJECT_0+1)return false;
   const auto error=result==WAIT_FAILED?GetLastError():ERROR_INVALID_FUNCTION;Event("timer_wait_fallback",error);
  }else {const auto error=GetLastError();Event("timer_arm_fallback",error);}
 }
 // Timer availability must not gate protocol traffic; retain cooperative cancel.
 if(!cancel){Sleep(ms);return true;}
 const auto result=WaitForSingleObject(cancel,ms);
 if(result==WAIT_TIMEOUT)return true;
 if(result==WAIT_FAILED){const auto error=GetLastError();Event("cancel_wait_failed",error);}
 return false;
}
halljoy::physical_analog::Publication nativeValues,fnValues;
halljoy::sharkplay::LayerState layerState;
std::array<unsigned,128> latestRaw{};
std::atomic<const halljoy::sharkplay::Profile*> activeProfile{nullptr};
std::atomic<bool> nativeReady{false};std::atomic<ULONGLONG> nativeLast{0};
std::atomic<unsigned long long> nativeUpdates{0},nativeExpired{0};
std::array<LONG,4> seenPages{};std::array<DWORD,4> pageTicks{};std::array<bool,4> pageExpired{};
void ClearNative(){
 nativeReady=false;activeProfile=nullptr;nativeLast=0;nativeValues.Clear();fnValues.Clear();layerState={};latestRaw.fill(0);seenPages.fill(0);pageTicks.fill(0);pageExpired.fill(false);
 RealtimeLoop_NotifyInputChanged();
}
unsigned FreshBudget(){return shared?std::clamp(static_cast<unsigned>(Load(shared->freshMs)),150u,300u):150u;}
void PumpNative(){
 if(!shared || !halljoy::sharkplay::Supported(Load(shared->identity),Load(shared->usb)))return;
 const auto* profile=halljoy::sharkplay::Find(Load(shared->identity));if(!profile)return;
 if(!nativeReady){activeProfile=profile;
  for(unsigned i=0;i<128;++i){
   if(profile->factory[i])nativeValues.Bind(static_cast<std::uint8_t>(i+1),profile->factory[i]);
   if(auto fn=profile->fn[i])fnValues.Bind(static_cast<std::uint8_t>(i+1),static_cast<std::uint16_t>(fn));
  }
  nativeReady=true;
 }
 bool dirty=false,changed=false;const auto now=GetTickCount64();const DWORD tick=GetTickCount();
 // First collect complete pages, including Fn. Then route all affected keys.
 for(unsigned page=0;page<4;++page){
  auto& input=shared->pages[page];const LONG seq=Load(input.sequence);
  if(seq && !(seq&1) && seq!=seenPages[page]){
   const DWORD stamp=static_cast<DWORD>(Load(input.tick));std::array<unsigned,32> values{};
   for(unsigned i=0;i<32;++i)values[i]=static_cast<unsigned>(Load(input.values[i]));
   if(seq!=Load(input.sequence))continue;
   seenPages[page]=seq;pageTicks[page]=stamp;pageExpired[page]=false;dirty=true;
   std::copy(values.begin(),values.end(),latestRaw.begin()+page*32);
   if(halljoy::sharkplay::Fresh(stamp,tick,FreshBudget())){nativeLast=now;++nativeUpdates;}
  }
  if(seenPages[page] && !pageExpired[page] && !halljoy::sharkplay::Fresh(pageTicks[page],tick,FreshBudget())){
   pageExpired[page]=true;++nativeExpired;dirty=true;
  }
 }
 if(dirty){
  const unsigned fnPage=profile->fnSlot/32;
  const bool fnFresh=seenPages[fnPage] && !pageExpired[fnPage];
  const bool fnDown=fnFresh && latestRaw[profile->fnSlot]>0;
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
  const bool blocking=Settings_GetBlockBoundKeys();
  halljoy::input_path::Add(blocking,halljoy::input_path::SourceFrames);
  if(!fnFresh)halljoy::input_path::Add(blocking,halljoy::input_path::FnUnavailable);
  unsigned positive=0;
#endif
  for(unsigned slot=0;slot<128;++slot){
   const unsigned page=slot/32;
   const bool fresh=fnFresh && seenPages[page] && !pageExpired[page];
   const unsigned raw=fresh?latestRaw[slot]:0;
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
   if(raw)++positive;
#endif
   const unsigned routed=layerState.Route(slot,raw,fnDown,*profile);
   const auto milli=static_cast<std::uint16_t>(halljoy::sharkplay::Milli(raw,static_cast<unsigned>(Load(shared->units))));
   const auto stamp=fresh?now-DWORD(tick-pageTicks[page]):now;
   changed=nativeValues.Publish(static_cast<std::uint8_t>(slot+1),routed==profile->factory[slot]?milli:0,stamp)||changed;
   changed=fnValues.Publish(static_cast<std::uint8_t>(slot+1),routed==profile->fn[slot]?milli:0,stamp)||changed;
  }
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
  if(positive)halljoy::input_path::Add(blocking,halljoy::input_path::SourcePositive);
#endif
 }
 if(changed)RealtimeLoop_NotifyInputChanged();
 static ULONGLONG nextLog=0;if(now>=nextLog){
  SupportLog_Event("shark.native_updates",nativeUpdates.load());
  SupportLog_Event("shark.expired_pages",nativeExpired.load());
  StabilityTrace_Write(L"INFO",L"shark",L"native_checkpoint",L"updates=%llu expired_pages=%llu active=%u full_travel_um=3500 freshness_ms=%u fn_layer=1",nativeUpdates.load(),nativeExpired.load(),nativeValues.Active(now)+fnValues.Active(now),FreshBudget());nextLog=now+5000;
 }
}
void PublishPage(unsigned page,const Report& response){
 std::array<unsigned,32> values{};if(!Decode(response,values))return;auto& output=shared->pages[page];
 InterlockedIncrement(&output.sequence);
 for(unsigned i=0;i<32;++i)InterlockedExchange(&output.values[i],static_cast<LONG>(values[i]));
 InterlockedExchange(&output.tick,static_cast<LONG>(GetTickCount()));InterlockedIncrement(&output.sequence);
}
void Line(const std::string& value){DWORD n=0;auto text=value+"\n";WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),static_cast<DWORD>(text.size()),&n,nullptr);}
void Event(const char* name,unsigned code){Line(std::string(name)+" error="+std::to_string(code));}
std::wstring ThisExe(){wchar_t path[32768]{};auto n=GetModuleFileNameW(nullptr,path,_countof(path));if(!n || n>=_countof(path))throw 1;return {path,n};}
std::wstring Quote(const std::wstring& s){return L"\""+s+L"\"";}
// R85 diagnostics deliberately retain every byte of a bounded wire sample.
// The ordinary build never records these reports.
std::string WireRecord(const char* direction,unsigned command,unsigned page,unsigned delay,const Report& report){
 static constexpr char hex[]="0123456789abcdef";std::string bytes;bytes.reserve(130);
 for(auto v:report){bytes+=hex[v>>4];bytes+=hex[v&15];}
 return std::string("r85_wire direction=")+direction+" command="+std::to_string(command)+" page="+std::to_string(page)+" delay_ms="+std::to_string(delay)+" bytes="+bytes;
}
bool DiagnosticPid(unsigned pid){
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
 return pid==0x5029;
#else
 return CandidatePid(pid);
#endif
}
struct Device {std::wstring path;unsigned pid=0,usage=0,ordinal=0;};
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
  auto status=HidP_GetCaps(prep,&caps);
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
  if(status==HIDP_STATUS_SUCCESS && a.ProductID==0x5029){
   Line("r85_collection ordinal="+std::to_string(i)+" feature_value_caps="+std::to_string(caps.NumberFeatureValueCaps)+" feature_button_caps="+std::to_string(caps.NumberFeatureButtonCaps));
   USHORT count=std::min<USHORT>(caps.NumberFeatureValueCaps,128);std::vector<HIDP_VALUE_CAPS> values(count);
   if(count && HidP_GetValueCaps(HidP_Feature,values.data(),&count,prep)==HIDP_STATUS_SUCCESS)
    for(unsigned k=0;k<count;++k)Line("r85_feature ordinal="+std::to_string(i)+" report_id="+std::to_string(values[k].ReportID)+" bit_size="+std::to_string(values[k].BitSize)+" report_count="+std::to_string(values[k].ReportCount));
  }
#endif
  HidD_FreePreparsedData(prep);if(status!=HIDP_STATUS_SUCCESS){++metadataErrors;Event("caps_failed",static_cast<unsigned>(status));continue;}
  wchar_t product[256]{};bool receiver=false;
  if(HidD_GetProductString(meta.h,product,sizeof(product))){std::wstring name=product;std::transform(name.begin(),name.end(),name.begin(),towlower);receiver=name.find(L"receiver")!=std::wstring::npos || name.find(L"dongle")!=std::wstring::npos;}
  char line[256];sprintf_s(line,"descriptor vid=3151 pid=%04x bcd=%04x usage=%04x:%04x in=%u out=%u feature=%u receiver=%u",a.ProductID,a.VersionNumber,caps.UsagePage,caps.Usage,caps.InputReportByteLength,caps.OutputReportByteLength,caps.FeatureReportByteLength,receiver?1:0);Line(line);
  const char* reason=receiver?"receiver":!DiagnosticPid(a.ProductID)?"non_target_pid":caps.UsagePage!=0xffff?"usage_page":caps.Usage!=2?"usage":caps.FeatureReportByteLength!=65?"feature_length":nullptr;
  if(reason){++rejected;Line(std::string("collection_rejected reason=")+reason);}else result.push_back({path,a.ProductID,caps.Usage,i});
 }
 std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.usage>b.usage;});
 Line("inventory eligible="+std::to_string(result.size())+" vendor_collections="+std::to_string(vendorCollections)+" metadata_errors="+std::to_string(metadataErrors)+" rejected="+std::to_string(rejected)+" paths_logged=0");return result;
}
struct Session {
 unsigned wireCount[4]{},wireDelay[4]{};ULONGLONG wireNext[4]{};
 Handle device;Handle timer=PreciseTimer();unsigned delay=1,errors=0,identity=0;std::uint64_t calls=0,totalUs=0,maxUs=0,maxWaitUs=0,maxSetUs=0,maxGetUs=0;ULONGLONG began=GetTickCount64();
 DWORD firstOpenError=0,lastOpenError=0,accessMode=GENERIC_READ|GENERIC_WRITE,shareMode=0;
 explicit Session(const Device& d){
  // All member initialization (including the timer) is already complete.
  // Capture GetLastError immediately; timer creation used to erase it.
  auto open=[&](DWORD access,DWORD share){
   device.h=CreateFileW(d.path.c_str(),access,share,nullptr,OPEN_EXISTING,0,nullptr);
   lastOpenError=device?ERROR_SUCCESS:GetLastError();accessMode=access;shareMode=share;
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
   Line("r85_open collection="+std::to_string(d.ordinal)+" pid="+std::to_string(d.pid)+" access="+std::to_string(access)+" share="+std::to_string(share)+" success="+std::to_string(bool(device))+" error="+std::to_string(lastOpenError));
#endif
  };
  open(GENERIC_READ|GENERIC_WRITE,0);firstOpenError=lastOpenError;
  if(!device)Event("exclusive_open_failed",firstOpenError);
  // Follow HIDAPI's shared RW / feature-only opening convention for this
  // family. Identity and all depth validation remain mandatory.
  if(!device && (lastOpenError==ERROR_SHARING_VIOLATION || lastOpenError==ERROR_ACCESS_DENIED)){
   open(GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE);
   if(!device && (lastOpenError==ERROR_SHARING_VIOLATION || lastOpenError==ERROR_ACCESS_DENIED))open(0,FILE_SHARE_READ|FILE_SHARE_WRITE);
  }
 }
 bool Exchange(unsigned command,unsigned page,Report& reply){
  const auto request=Request(command,page);if(!request[1])return false;

  LARGE_INTEGER start{},end{},freq{};QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);
  // Match the vendor send delay as well as its separate read delay.
  if(!Delay(timer.h,childCancel,delay))return false;
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
  // First 8 exchanges/page, each timing transition, then one pair/second/page.
  const bool wire=command!=0xe5 || wireCount[page]<8 || wireDelay[page]!=delay || GetTickCount64()>=wireNext[page];
  if(wire){Line(WireRecord("tx",command,page,delay,request));if(command==0xe5){++wireCount[page];wireDelay[page]=delay;wireNext[page]=GetTickCount64()+1000;}}
#endif
  LARGE_INTEGER beforeSet{},afterSet{},beforeGet{};QueryPerformanceCounter(&beforeSet);
  InterlockedExchange(&shared->command,command);InterlockedExchange(&shared->page,page);InterlockedExchange(&shared->ioPhase,1);
  if(!HidD_SetFeature(device.h,const_cast<std::uint8_t*>(request.data()),65)){++errors;Event("set_feature_failed",GetLastError());return false;}
  QueryPerformanceCounter(&afterSet);
  if(!Delay(timer.h,childCancel,delay))return false;
  QueryPerformanceCounter(&beforeGet);
  reply.fill(0xff);reply[0]=0;
  InterlockedExchange(&shared->ioPhase,2);
  if(!HidD_GetFeature(device.h,reply.data(),65)){++errors;Event("get_feature_failed",GetLastError());return false;}
  InterlockedExchange(&shared->ioPhase,0);
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
  std::array<unsigned,32> decoded{};
  if(wire || (command==0xe5 && !Decode(reply,decoded)))Line(WireRecord("rx",command,page,delay,reply));
#endif
  QueryPerformanceCounter(&end);auto us=static_cast<std::uint64_t>((end.QuadPart-start.QuadPart)*1000000/freq.QuadPart);
  auto micros=[&](LONGLONG ticks){return static_cast<std::uint64_t>(ticks*1000000/freq.QuadPart);};
  maxWaitUs=std::max(maxWaitUs,micros(beforeSet.QuadPart-start.QuadPart+beforeGet.QuadPart-afterSet.QuadPart));
  maxSetUs=std::max(maxSetUs,micros(afterSet.QuadPart-beforeSet.QuadPart));
  maxGetUs=std::max(maxGetUs,micros(end.QuadPart-beforeGet.QuadPart));
  ++calls;totalUs+=us;maxUs=std::max(maxUs,us);return true;
 }
 bool Identify(const Device& d,unsigned& usb){
  for(unsigned attempt=0;attempt<3;++attempt){delay=10;Report a{},b{};
   if(Stop())return false;
   if(Exchange(0x8f,0,a) && Exchange(0x8f,0,b) && Identity(a)==Identity(b) && Known(Identity(a),d.pid)){
    identity=Identity(a);usb=unsigned(b[8])|(unsigned(b[9])<<8);
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
    if(identity!=3123){Line("r85_identity_mismatch expected=3123 actual="+std::to_string(identity)+" no_depth_commands=1");return false;}
#endif
    return true;}
   Line("identity_attempt delay_ms="+std::to_string(delay)+" id="+std::to_string(Identity(a))+" repeated_id="+std::to_string(Identity(b))+" reply_command="+std::to_string(a[1])+" repeated_command="+std::to_string(b[1])+" report_id="+std::to_string(b[0]));
  }return false;
 }
};
void Summary(const Metrics& m,const Session& s,bool detail){
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
 unsigned peak=0,mappedPositive=0,unmappedPositive=0;std::uint64_t valid=0;
 const auto* profile=halljoy::sharkplay::Find(3123);
 for(unsigned i=0;i<128;++i){peak=std::max(peak,m.keys[i].high);if(m.keys[i].positive){if(profile->factory[i])++mappedPositive;else ++unmappedPositive;}}
 for(auto n:m.pages)valid+=n;
 const char* outcome=!valid?"no_valid_depth_pages":!peak?(Load(shared->presses)?"zero_depth_despite_digital_presses":"zero_depth_no_digital_evidence"):!m.Varying()?"nonzero_depth_not_varying":"varying_depth";
 Line(std::string("r85_depth_evidence outcome=")+outcome+" valid_pages="+std::to_string(valid)+" peak_raw="+std::to_string(peak)+" mapped_positive_slots="+std::to_string(mappedPositive)+" unmapped_positive_slots="+std::to_string(unmappedPositive)+" digital_scope=3151:5029 digital_presses="+std::to_string(Load(shared->presses))+" declared_travel_um=3300 current_normalization_um=3500 range_unverified=1");
#endif

 Line(std::string(detail?"summary":"checkpoint")+" dev_id="+std::to_string(s.identity)+" elapsed_ms="+std::to_string(GetTickCount64()-s.began)+" page0="+std::to_string(m.pages[0])+" page1="+std::to_string(m.pages[1])+" page2="+std::to_string(m.pages[2])+" page3="+std::to_string(m.pages[3])+" varying="+std::to_string(m.Varying())+" max_page0_active="+std::to_string(m.peakPage0)+" digital_chord_frames="+std::to_string(m.chordFrames)+" independent_changes="+std::to_string(m.independentChanges)+" malformed="+std::to_string(m.invalid)+" io_errors="+std::to_string(s.errors)+" avg_exchange_us="+std::to_string(s.calls?s.totalUs/s.calls:0)+" max_exchange_us="+std::to_string(s.maxUs)+" max_wait_us="+std::to_string(s.maxWaitUs)+" max_set_us="+std::to_string(s.maxSetUs)+" max_get_us="+std::to_string(s.maxGetUs)+" delay_ms="+std::to_string(s.delay)+" digital_presses="+std::to_string(Load(shared->presses))+" digital_releases="+std::to_string(Load(shared->releases))+" digital_held="+std::to_string(Load(shared->held)));
#if !defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
 (void)detail;return;
#endif
 for(unsigned i=0;i<128;++i){const auto& k=m.keys[i];if(!k.count || (!detail && !k.positive && !k.changed))continue;
  Line("key slot="+std::to_string(i)+" samples="+std::to_string(k.count)+" min="+std::to_string(k.low)+" max="+std::to_string(k.high)+" last="+std::to_string(k.last)+" positive="+std::to_string(k.positive)+" zero="+std::to_string(k.zero)+" changed="+std::to_string(k.changed)+" rises="+std::to_string(k.increase)+" falls="+std::to_string(k.decrease)+" releases="+std::to_string(k.released));
 }
}
int Capture(){
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
 Line("r85_diagnostic revision=3 expected_vid=3151 expected_pid=5029 expected_id=3123 usage=ffff:0002 feature_bytes=65 physical_keys=79 fn_slot=65 vendor_class=Ry5088_sg9047_1m_8k exact_firmware_unavailable=1 writes_to_settings=0");
 const auto* expected=halljoy::sharkplay::Find(3123);
 for(unsigned slot=0;slot<expected->factory.size();++slot)if(expected->factory[slot])Line("r85_factory slot="+std::to_string(slot)+" factory_usage="+std::to_string(expected->factory[slot]));
#endif
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
   const bool playable=halljoy::sharkplay::Supported(s.identity,usb);
#if !defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
   if(!playable){Line("unsupported_revision no_depth_commands=1");continue;}
#endif
   InterlockedExchange(&shared->usb,usb);InterlockedExchange(&shared->identity,s.identity);
   Line("playback enabled="+std::to_string(playable)+" high_resolution_timer="+std::to_string(bool(s.timer))+" full_travel_um=3500 factory_mapping=1 remap_readback=0");
   unsigned rf=0;Report version{};if(s.Exchange(0x80,0,version) && s.Exchange(0x80,0,version) && version[1]==0x80)rf=unsigned(version[2])|(unsigned(version[3])<<8);
   unsigned selected=rf?rf:usb;unsigned units=halljoy::sharkplay::Units(selected);InterlockedExchange(&shared->units,units);
   Line("identity dev_id="+std::to_string(s.identity)+" pid="+std::to_string(d.pid)+" usb_version="+std::to_string(usb)+" rf_version="+std::to_string(rf)+" driver_units_per_mm="+std::to_string(units)+" exclusive="+std::to_string(s.shareMode==0)+" access="+std::to_string(s.accessMode)+" discard_first_page_reply=1 digital_scope=vidpid stream_command_sent=0 calibration_sent=0 writes_to_settings=0");
   s.delay=1;InterlockedExchange(&shared->freshMs,halljoy::sharkplay::FreshBudget(s.delay));Line("status state=2");Metrics m;unsigned cycle=0,invalidStreak=0,lastEscalation=0;bool sufficient=false;
   auto checkpoint=GetTickCount64(),detail=checkpoint+15000;unsigned previousPage=4;
   while(!Stop()){
    // Prioritize WASD bursts. Never treat independently sampled pages as an atomic snapshot.
    const unsigned pages[]={0,1,0,2,0,3};unsigned page=playable?halljoy::sharkplay::Page(cycle,halljoy::sharkplay::Find(s.identity)->fnSlot/32,halljoy::sharkplay::UsesFourthPage(*halljoy::sharkplay::Find(s.identity))):pages[cycle%6];Report response{};
    // Raw replies have no page tag. Discard the first reply after changing pages,
    // then request the same page again to avoid accepting a one-request-late page.
    const bool flush=!playable || page!=previousPage;
    if((flush && !s.Exchange(0xe5,page,response)) || !s.Exchange(0xe5,page,response)){if(Stop())break;Summary(m,s,true);Line("status state=6");return 6;}
    previousPage=page;
    if(Identity(response) && Known(Identity(response),d.pid)){
     ++m.invalid;++invalidStreak;Line("stale_identity_reply page="+std::to_string(page));
    }else if(m.Add(page,response,static_cast<unsigned>(Load(shared->wasd)))){invalidStreak=0;if(playable)PublishPage(page,response);}else {
     ++invalidStreak;
     if(invalidStreak<=3){unsigned maxRaw=0,bad=0;for(unsigned i=0;i<32;++i){unsigned v=unsigned(response[1+2*i])|(unsigned(response[2+2*i])<<8);maxRaw=std::max(maxRaw,v);if(v>4096)++bad;}
      Line("page_rejected page="+std::to_string(page)+" report_id="+std::to_string(response[0])+" max_raw="+std::to_string(maxRaw)+" implausible_slots="+std::to_string(bad));}
    }
    // Retry only documented timing, never stream/calibration modes. Zero alone is not a fault.
    const auto presses=static_cast<unsigned>(Load(shared->presses));
    if(invalidStreak>=3 || (!m.Varying() && presses>=lastEscalation+4)){
     if(s.delay<10){s.delay=s.delay<5?5:10;InterlockedExchange(&shared->freshMs,halljoy::sharkplay::FreshBudget(s.delay));lastEscalation=presses;invalidStreak=0;Line("timing_retry delay_ms="+std::to_string(s.delay));}
     else if(invalidStreak>=3){Summary(m,s,true);Line("status state=6");return 7;}
    }
    bool enough=m.Enough(presses,static_cast<unsigned>(Load(shared->releases)));
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
    // Coverage is derived from this model's factory map, never X65 slot numbers.
    enough=presses>=4 && Load(shared->releases)>=4;
    const auto* profile=halljoy::sharkplay::Find(3123);
    for(unsigned hid:{26u,4u,22u,7u})for(unsigned slot=0;slot<profile->factory.size();++slot)if(profile->factory[slot]==hid){const auto& k=m.keys[slot];enough=enough && k.positive && k.released && k.increase && k.decrease;}
#endif
    if(!sufficient && enough){sufficient=true;Line("status state=3");}
    if((++cycle%1024)==0){previousPage=4;Report proof{};const unsigned depthDelay=s.delay;s.delay=10;const bool valid=s.Exchange(0x8f,0,proof) && s.Exchange(0x8f,0,proof) && Identity(proof)==s.identity;s.delay=depthDelay;if(!valid){if(Stop())break;Summary(m,s,true);Line("identity_lost");return 8;}}
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
 // Keep ordinary reports useful when discovery fails before any analog samples.
 // Only named structural fields are copied; never arbitrary child text/keys.
 auto number=[&](const char* key){auto pos=line.find(key);return pos==std::string::npos?0ull:std::strtoull(line.c_str()+pos+strlen(key),nullptr,10);};
 if(line.rfind("inventory eligible=",0)==0) {
  SupportLog_Event("shark.eligible",number("eligible="));
  SupportLog_Event("shark.vendor_collections",number("vendor_collections="),number("metadata_errors="));
 }
 if(line.rfind("descriptor ",0)==0) {
  unsigned pid=0,bcd=0,page=0,usage=0,in=0,out=0,feature=0,receiver=0;
  if(sscanf_s(line.c_str(),"descriptor vid=3151 pid=%x bcd=%x usage=%x:%x in=%u out=%u feature=%u receiver=%u",
      &pid,&bcd,&page,&usage,&in,&out,&feature,&receiver)==8) {
   SupportLog_Event("shark.collection_pid",pid,bcd);
   SupportLog_Event("shark.collection_usage",page,usage);
   SupportLog_Event("shark.collection_feature",feature,receiver);
  }
 }
 if(line.rfind("identity_attempt ",0)==0) {
  SupportLog_Event("shark.probe_identity",number(" id="),number("repeated_id="));
  SupportLog_Event("shark.probe_command",number("reply_command="),number("repeated_command="));
 }
 if(line.rfind("identity_rejected ",0)==0)SupportLog_Event("shark.identity_rejected",1);
 const char* failures[]={"inventory_failed","metadata_open_failed","attributes_failed",
  "preparsed_data_failed","caps_failed","exclusive_open_failed","set_feature_failed","get_feature_failed"};
 for(const auto* name:failures) if(line.rfind(std::string(name)+" error=",0)==0) {
  const auto category=std::string("shark.")+name;
  SupportLog_Event(category.c_str(),1,number("error="));break;
 }
 if(line.rfind("status state=",0)==0)SupportLog_Event("shark.state",number("state="));

 if(line.rfind("summary ",0)==0){
  auto field=[&](const char* key){auto pos=line.find(key);return pos==std::string::npos?0ull:std::strtoull(line.c_str()+pos+strlen(key),nullptr,10);};
  SupportLog_Event("shark.page0_samples",field("page0="));
  SupportLog_Event("shark.capture_ms",field("elapsed_ms="));
  SupportLog_Event("shark.exchange_max_us",field("max_exchange_us="));
  SupportLog_Event("shark.wait_max_us",field("max_wait_us="));
  SupportLog_Event("shark.set_max_us",field("max_set_us="));
  SupportLog_Event("shark.get_max_us",field("max_get_us="));
  SupportLog_Event("shark.page1_samples",field("page1="));
  SupportLog_Event("shark.page2_samples",field("page2="));
  SupportLog_Event("shark.io_errors",field("io_errors="));
 }
 if(line.rfind("identity dev_id=",0)==0)SupportLog_Event("shark.identity",std::strtoull(line.c_str()+16,nullptr,10));
 if(line=="unsupported_revision no_depth_commands=1")SupportLog_Event("shark.unsupported_revision",1);
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
    Handle process(pi.hProcess),thread(pi.hThread);Handle parentTimer=PreciseTimer();
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
        PumpNative();
        const auto now=GetTickCount64();
        #if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
        if(mode==L"capture" && !StabilityTrace_IsEnabled())parentStop=true;
#endif
        if(!cancelledAt && (now>=end || ((mode==L"capture" || mode==L"play") && now-lastOutput>8000) || (parentStop && !allowCancelled))){SetEvent(cancel.h);cancelledAt=now;}
        if(cancelledAt && now-cancelledAt>=1200 && WaitForSingleObject(process.h,0)!=WAIT_OBJECT_0){
            TerminateProcess(process.h,101);forced=true;
            if(WaitForSingleObject(process.h,1000)!=WAIT_OBJECT_0){
                Trace(L"[shark] child_unreaped stop_retrying_device=1");return 103;
            }
        }
        if(!Delay(parentTimer.h,nullptr,nativeReady?1:20)){SetEvent(cancel.h);parentStop=true;}
    }
    if(!pending.empty())ReceiveLine(pending);
    DWORD code=100;GetExitCodeProcess(process.h,&code);
    Trace(L"[shark] worker mode=%s exit=%lu forced=%d",mode.c_str(),code,forced?1:0);
    return code;
}


void Supervise() noexcept {
 try{do{SupportLog_Event("shark.worker_start",1);ClearNative();InterlockedExchange(&shared->identity,0);
  for(auto& page:shared->pages)InterlockedExchange(&page.sequence,0);
  #if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
  const wchar_t* mode=L"capture";
#else
  const wchar_t* mode=L"play";
#endif
  const auto result=RunChild(mode,MAXDWORD);SupportLog_Event("shark.worker_exit",result);ClearNative();Trace(L"[shark] worker_end exit=%lu last_command=%ld last_page=%ld io_phase=%ld",result,Load(shared->command),Load(shared->page),Load(shared->ioPhase));
  if(parentStop || result==103)break;uiState=6;for(unsigned i=0;i<30 && !parentStop;++i)Sleep(100);
 }while(!parentStop);}catch(...){ClearNative();uiState=6;Trace(L"[shark] supervisor_exception partial_log_retained=1");}
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
void RequireAt(bool v,unsigned line){if(!v){Line("self_test_failed source_line="+std::to_string(line));throw 1;}}
#define Require(value) RequireAt((value),__LINE__)
void SharkTelemetry(NativeAnalogBackendTelemetry* out);
void TestOrdinaryFailureLog(){
 // Exercise actual child-record parsing AND ordinary file writer, not just formatting.
 wchar_t temp[MAX_PATH]{},directory[MAX_PATH]{};
 Require(GetTempPathW(MAX_PATH,temp)!=0);
 Require(GetTempFileNameW(temp,L"hjs",0,directory)!=0);
 Require(DeleteFileW(directory) && CreateDirectoryW(directory,nullptr));
 const std::wstring path=std::wstring(directory)+L"\\HallJoy.log";
 Require(SupportLog_Start(directory,directory));
 ReceiveLine("inventory eligible=0 vendor_collections=7 metadata_errors=2 rejected=5 paths_logged=0");
 ReceiveLine("exclusive_open_failed error=32");
 ReceiveLine("identity_attempt delay_ms=10 id=3123 repeated_id=9999 reply_command=143 repeated_command=0 report_id=0");
 ReceiveLine("get_feature_failed error=5");
 ReceiveLine("identity_rejected no_depth_commands=1");
 ReceiveLine("descriptor vid=3151 pid=5029 bcd=0123 usage=ffff:0002 in=0 out=0 feature=65 receiver=0");
 ReceiveLine("key slot=PRIVACY_SENTINEL samples=PRIVATE_VALUES path=PRIVATE_PATH");
 SupportLog_ReportMissingSource();
 std::string text;
 const auto deadline=GetTickCount64()+5000;
 do {
  Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr));
  if(file){char buffer[32768];DWORD got=0;if(ReadFile(file.h,buffer,sizeof(buffer),&got,nullptr))text.assign(buffer,got);}
  if(text.find("shark.collection_feature value=65")!=std::string::npos)break;
  Sleep(20);
 }while(GetTickCount64()<deadline);
 const bool stopped=SupportLog_Stop();
 DeleteFileW(path.c_str());RemoveDirectoryW(directory);
 Require(stopped);
 Require(text.find("shark.eligible value=0")!=std::string::npos);
 Require(text.find("shark.exclusive_open_failed value=1 error=32")!=std::string::npos);
 Require(text.find("shark.get_feature_failed value=1 error=5")!=std::string::npos);
 Require(text.find("shark.probe_identity value=3123 error=9999")!=std::string::npos);
 Require(text.find("shark.identity_rejected value=1")!=std::string::npos);
 Require(text.find("shark.collection_usage value=65535 error=2")!=std::string::npos);
 Require(text.find("PRIVATE_")==std::string::npos && text.find("PRIVACY_SENTINEL")==std::string::npos);
 Line("SHARK_ORDINARY_FAILURE_LOG=PASS actual_file=1 failure_reasons=1 private_payload_excluded=1");
}
void TestDelayFallback(){
 Handle wrongTimer(CreateEventW(nullptr,TRUE,FALSE,nullptr));Handle cancel(CreateEventW(nullptr,TRUE,FALSE,nullptr));Require(bool(wrongTimer) && bool(cancel));
 Require(Delay(wrongTimer.h,cancel.h,1));Require(Delay(nullptr,cancel.h,1));
 Require(SetEvent(cancel.h)!=0);Require(!Delay(wrongTimer.h,cancel.h,1));Require(!Delay(nullptr,cancel.h,1));
 ReceiveLine("SHARK_DELAY_TEST=PASS timer_failure_continues=1 cancellation_preserved=1 hardware_access=0");
}
void TestSessionOpen(){
 TestDelayFallback();
 wchar_t temp[MAX_PATH]{},path[MAX_PATH]{};
 Require(GetTempPathW(MAX_PATH,temp)!=0 && GetTempFileNameW(temp,L"hjo",0,path)!=0);
 {Handle held(CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr));Require(bool(held));
  Session conflict(Device{path,0x5029,2,77});Require(conflict.firstOpenError==ERROR_SHARING_VIOLATION);
  Require(bool(conflict.device) && conflict.shareMode==(FILE_SHARE_READ|FILE_SHARE_WRITE) && conflict.lastOpenError==0);

 }
 Require(DeleteFileW(path)!=0);
 {Session missing(Device{path,0x5029,2,78});Require(!missing.device && missing.firstOpenError==ERROR_FILE_NOT_FOUND && missing.lastOpenError==ERROR_FILE_NOT_FOUND);}
 ReceiveLine("SHARK_OPEN_TEST=PASS real_win32_errors=32,2 hardware_access=0");
}
void SelfTest(){
 TestSessionOpen();
#if !defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
 Require(!StabilityTrace_IsEnabled());
 TestOrdinaryFailureLog();
#endif
 parentStop=false;ClearNative();InterlockedExchange(&shared->identity,2308);InterlockedExchange(&shared->usb,0x314);InterlockedExchange(&shared->units,100);
 Report sample{},fnPage{};PublishPage(2,fnPage);sample[1+2*14]=175;sample[1+2*9]=350&255;sample[2+2*9]=350>>8;
 PublishPage(0,sample);PumpNative();
 Require(NativeAnalogBackends_CatalogIsValid());
 auto routed=NativeAnalogBackends_ReadMilli(26);Require(routed.connected && routed.owned && routed.milli==500);
 halljoy::configured_xusb::PadConfiguration config{};config.axes[0]={4,7};config.axes[1]={22,26};
 halljoy::configured_xusb::InputValues input{};
 for(unsigned hid:{4u,7u,22u,26u})input.filtered[hid]=NativeAnalogBackends_ReadMilli(static_cast<std::uint16_t>(hid)).milli/1000.0f;
 halljoy::configured_xusb::BuilderState gameState{};const auto frame=halljoy::configured_xusb::BuildReport(config,input,gameState);
 Require(frame.leftStickX==-32767 && frame.leftStickY==16384);
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
 Require(Backend_TestSharkConfiguredPath());
 Line("SHARK_CONFIGURED_PATH=PASS real_native_registry=1 real_curves=1 real_bindings=1 block_off_on_identical=1 digital_events=0 hardware_access=0");
#endif
 sample[1+2*14]=0;PublishPage(0,sample);PumpNative();Require(NativeAnalogBackends_ReadMilli(26).milli==0 && NativeAnalogBackends_ReadMilli(4).milli==1000);
 // A torn shared page is not a sample. A stale page cannot refresh held input.
 InterlockedIncrement(&shared->pages[0].sequence);PumpNative();Require(NativeAnalogBackends_ReadMilli(4).milli==1000);
 InterlockedExchange(&shared->pages[0].tick,static_cast<LONG>(GetTickCount()-151));InterlockedIncrement(&shared->pages[0].sequence);
 PumpNative();Require(NativeAnalogBackends_ReadMilli(4).milli==0);
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
 Require(Backend_TestSharkConfiguredPath(0,0));
#endif
 // Fn before digit routes depth exclusively to F1. Releasing Fn first keeps F1
 // until that physical key releases; the next press returns to the number.
 ClearNative();sample.fill(0);fnPage.fill(0);fnPage[3]=100;PublishPage(2,fnPage);PumpNative();
 sample[1+2*7]=175;PublishPage(0,sample);PumpNative();
 Require(NativeAnalogBackends_ReadMilli(58).milli==500 && NativeAnalogBackends_ReadMilli(30).milli==0);
 fnPage[3]=0;PublishPage(2,fnPage);PumpNative();
 Require(NativeAnalogBackends_ReadMilli(58).milli==500 && NativeAnalogBackends_ReadMilli(30).milli==0);
 sample[1+2*7]=0;PublishPage(0,sample);PumpNative();Require(NativeAnalogBackends_ReadMilli(58).milli==0);
 sample[1+2*7]=175;PublishPage(0,sample);PumpNative();Require(NativeAnalogBackends_ReadMilli(30).milli==500);
 fnPage[3]=100;PublishPage(2,fnPage);PumpNative();Require(NativeAnalogBackends_ReadMilli(30).milli==500 && NativeAnalogBackends_ReadMilli(58).milli==0);
 sample[1+2*7]=0;PublishPage(0,sample);PumpNative();
 sample[1+2*7]=175;PublishPage(0,sample);PumpNative();Require(NativeAnalogBackends_ReadMilli(58).milli==500);
 sample[1+2*7]=0;PublishPage(0,sample);PumpNative();Require(NativeAnalogBackends_ReadMilli(58).milli==0);
 fnPage[3]=0;PublishPage(2,fnPage);PumpNative();
 ClearNative();InterlockedExchange(&shared->identity,9999);PumpNative();Require(!NativeAnalogBackends_ReadMilli(26).owned);
 InterlockedExchange(&shared->identity,0);

 Require(Request(0x1b)[1]==0 && Request(0x1c)[1]==0 && Request(0xe5,4)[1]==0);
 Require(Request(0xe5,0)[8]==0x1b && Request(0xe5,3)[8]==0x18);
 Require(Known(2308,0x502f) && Known(2938,0x5030) && !Known(2308,0x5030));
 Metrics m;Report r{};auto put=[&](unsigned slot,unsigned value){r[1+2*slot]=static_cast<std::uint8_t>(value);r[2+2*slot]=static_cast<std::uint8_t>(value>>8);};
 put(14,300);put(9,100);Require(m.Add(0,r,3));put(14,320);Require(m.Add(0,r,3));put(14,0);Require(m.Add(0,r,2));
 Require(m.keys[14].released==1 && m.keys[9].last==100 && m.independentChanges==1 && !m.Enough(4,4));
 auto invalid=Request(0xe5);Require(!m.Add(0,invalid,0));invalid.fill(255);Require(!m.Add(0,invalid,0));
 // Exercise each exact model through the same shared-page/registry path.
 for(const auto& profile:halljoy::sharkplay::Profiles){
  ClearNative();for(auto& page:shared->pages)InterlockedExchange(&page.sequence,0);
  InterlockedExchange(&shared->identity,profile.id);InterlockedExchange(&shared->usb,0x500);InterlockedExchange(&shared->units,200);
  Report empty{},keys{};PublishPage(profile.fnSlot/32,empty);keys[1+2*14]=350&255;keys[2+2*14]=350>>8;
  PublishPage(0,keys);PumpNative();Require(NativeAnalogBackends_ReadMilli(26).milli==500);
  NativeAnalogBackendTelemetry telemetry{};SharkTelemetry(&telemetry);
  Require(telemetry.connected && telemetry.verifiedLayoutToken==halljoy::sharklayout::Token(profile.id));
  if(telemetry.verifiedLayoutToken)Require(halljoy::sharklayout::Match(telemetry.verifiedLayoutToken)!=nullptr);
  // Every mapped position must reach native publication, including page3/numpad.
  for(unsigned slot=0;slot<128;++slot){
   if(!profile.factory[slot])continue;layerState={};
   for(unsigned page=0;page<4;++page){Report one{};if(slot/32==page){one[1+2*(slot%32)]=350&255;one[2+2*(slot%32)]=350>>8;}PublishPage(page,one);}
   PumpNative();Require(NativeAnalogBackends_ReadMilli(profile.factory[slot]).milli==500);
   for(unsigned page=0;page<4;++page)PublishPage(page,Report{});
   PumpNative();Require(NativeAnalogBackends_ReadMilli(profile.factory[slot]).milli==0);
  }
  if(profile.fn[7]){
   empty[1+2*(profile.fnSlot%32)]=100;PublishPage(profile.fnSlot/32,empty);
   keys[1+2*7]=350&255;keys[2+2*7]=350>>8;PublishPage(0,keys);PumpNative();
   Require(NativeAnalogBackends_ReadMilli(58).milli==500 && NativeAnalogBackends_ReadMilli(30).milli==0);
  }
 }
 ClearNative();InterlockedExchange(&shared->identity,0);InterlockedExchange(&shared->units,100);
 NativeAnalogBackendTelemetry disconnected{};SharkTelemetry(&disconnected);Require(!disconnected.verifiedLayoutToken);
 parentStop=false;Require(RunChild(L"test-pipe",1000)==0 && pipeTestSeen);
 Require(NativeAnalogBackends_ReadMilli(26).milli==500);ClearNative();InterlockedExchange(&shared->identity,0);
 Require(RunChild(L"test-failure",1000)==19);
 Require(RunChild(L"test-hang",20)==101);Require(RunChild(L"test-cancel",20)==0);
 Line("SHARK_SELF_TEST=PASS family_native_profiles=37 fn_analog_only=1 fn_release_orders=1 normal_logging_independent=1 native_publication=1 registry_to_gamepad=1 cross_process_page=1 stale_release=1 torn_page=1 exact_revision=1 allowlist=1 parser=1 independent_depths=1 release=1 metrics=1 isolated_pipe=1 forced_timeout=1 cooperative_cancel=1 hardware_access=0");
}
}
bool SharkDiagnostic_TryRunCommand(int& result) noexcept {
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return false;
 bool test=argc==2 && wcscmp(argv[1],L"--halljoy-shark-self-test")==0;
 bool logTest=argc==2 && wcscmp(argv[1],L"--halljoy-r85-log-self-test")==0;
 bool worker=argc==5 && wcscmp(argv[1],L"--halljoy-shark-worker")==0;
 if(!test && !worker && !logTest){LocalFree(argv);return false;}
 try{
  if(logTest){
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
   StabilityTrace_Init();Require(StabilityTrace_IsEnabled());
   TestSessionOpen();
   ReceiveLine("r85_diagnostic revision=3 expected_id=3123 self_test=1 hardware_access=0");
   ReceiveLine(WireRecord("tx",0x8f,0,10,Request(0x8f)));
   Report fixture{};fixture[1]=0x8f;fixture[2]=0x33;fixture[3]=0x0c;
   Require(Identity(fixture)==3123);ReceiveLine(WireRecord("rx",0x8f,0,10,fixture));
   fixture.fill(0);fixture[1+2*14]=200;std::array<unsigned,32> values{};
   Require(Decode(fixture,values) && values[14]==200);ReceiveLine(WireRecord("rx",0xe5,0,1,fixture));
   Require(!Decode(Request(0xe5),values));fixture[0]=1;Require(!Decode(fixture,values));
   ReceiveLine("r85_depth_evidence outcome=zero_depth_despite_digital_presses valid_pages=100 digital_presses=4");
   ReceiveLine("exclusive_open_failed error=32");ReceiveLine("get_feature_failed error=5");
   ReceiveLine("r85_identity_mismatch expected=3123 actual=9999 no_depth_commands=1");
   ReceiveLine("page_rejected page=2 report_id=0 max_raw=65535 implausible_slots=32");
   Require(DiagnosticPid(0x5029) && !DiagnosticPid(0x5030));
   ReceiveLine("R85_LOG_SELF_TEST=PASS");StabilityTrace_Shutdown(0);result=0;
#else
   result=92;
#endif
  }
  else if(test){CreateShared();SelfTest();result=0;}
  else{childCancel=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[3],nullptr,10)));DWORD flags=0;
   if(!childCancel || !GetHandleInformation(childCancel,&flags))throw 1;
   shared=static_cast<Shared*>(MapViewOfFile(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(_wcstoui64(argv[4],nullptr,10))),FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));if(!shared)throw 1;
   std::wstring mode=argv[2];
   if(mode==L"test-pipe"){
    InterlockedExchange(&shared->identity,2308);InterlockedExchange(&shared->usb,0x314);
    Report sample{},fnPage{};PublishPage(2,fnPage);sample[1+2*14]=175;PublishPage(0,sample);
    Line("pipe_test complete_record=1");Sleep(20);result=0;
   }
   else if(mode==L"test-hang"){Sleep(INFINITE);result=19;}
   else if(mode==L"test-cancel"){while(!Stop())Sleep(5);result=0;}
   else if(mode==L"test-failure")result=19;
   else if(mode==L"capture" || mode==L"play")result=Capture();else result=19;
  }
 }catch(...){Line("worker_exception partial_log_retained=1");result=20;}
 LocalFree(argv);return true;
}
void SharkDiagnostic_Start() noexcept {
 if(supervisor.joinable())return;
#if defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
 if(!StabilityTrace_IsEnabled()){uiState=4;return;}
#endif
 try{CreateShared();parentStop=false;collecting=true;uiState=1;
  StabilityTrace_WriteCritical(L"INFO",L"shark",L"start",L"build=20260924-shark-diag-3 no_test_deadline=1 stream=0 calibration=0 firmware_writes=0 ordered_keys=0 analog_output=1 catalog_profiles=37 fn_analog_only=1");
  supervisor=std::thread(Supervise);
 }catch(...){collecting=false;uiState=6;}
}
void SharkDiagnostic_Stop() noexcept {
 parentStop=true;collecting=false;if(supervisor.joinable())supervisor.join();ClearNative();
 if(shared)StabilityTrace_WriteCritical(L"INFO",L"shark",L"digital_summary",L"presses=%ld releases=%ld held=%ld ordered_keys=0",Load(shared->presses),Load(shared->releases),Load(shared->held));
}
void SharkDiagnostic_DeviceChanged(HANDLE device) noexcept {
 if(!collecting)return;if(shared)InterlockedIncrement(&shared->topology);
 std::lock_guard<std::mutex> lock(digitalMutex);auto it=digital.find(device);if(it!=digital.end()){if(it->second.target){retiredPresses+=it->second.presses;retiredReleases+=it->second.releases;}digital.erase(it);}PublishDigital();
}
void SharkDiagnostic_ObserveRawInput(HRAWINPUT input) noexcept {
#if !defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
 (void)input;return;
#endif
 if(!collecting)return;
 try{RAWINPUT raw{};UINT n=sizeof(raw);
  if(GetRawInputData(input,RID_INPUT,&raw,&n,sizeof(RAWINPUTHEADER))==UINT(-1) || n<sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD) || raw.header.dwType!=RIM_TYPEKEYBOARD)return;
  std::lock_guard<std::mutex> lock(digitalMutex);auto it=digital.find(raw.header.hDevice);
  if(it==digital.end()){if(digital.size()>=64)return;Digital d;wchar_t path[2048]{};UINT len=_countof(path);
   if(GetRawInputDeviceInfoW(raw.header.hDevice,RIDI_DEVICENAME,path,&len)!=UINT(-1)){std::wstring lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),towlower);d.target=lower.find(L"vid_3151&pid_502f")!=std::wstring::npos || lower.find(L"vid_3151&pid_5030")!=std::wstring::npos;
#if defined(HALLJOY_ATTACKSHARK_R85_DIAGNOSTIC)
    d.target=lower.find(L"vid_3151&pid_5029")!=std::wstring::npos;
#endif
   }
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
#if !defined(HALLJOY_ATTACKSHARK_PRO_DIAGNOSTIC)
 (void)window;return;
#endif
 static unsigned previous=99;auto state=uiState.load();if(state && !StabilityTrace_IsEnabled())state=4;
 const bool playable=shared && halljoy::sharkplay::Supported(Load(shared->identity),Load(shared->usb));
 if((state==2 || state==3) && !playable)state=5;
 if(previous==state)return;previous=state;
 const wchar_t* titles[]={L"HallJoy",L"HallJoy - ATTACK SHARK test: connect by USB; close the vendor driver",
  L"HallJoy - ATTACK SHARK: analog active; diagnostic log recording",
  L"HallJoy - ATTACK SHARK: movement recorded; close and send HallJoy.log",
  L"HallJoy - ATTACK SHARK: cannot write HallJoy.log; use a writable folder",
  L"HallJoy - ATTACK SHARK: device busy or revision unrecognized; close driver; log retained",
  L"HallJoy - ATTACK SHARK: read interrupted; retrying; log retained"};
 SetWindowTextW(window,titles[std::min(state,6u)]);
}

namespace {
bool SharkStart(){SharkDiagnostic_Start();return supervisor.joinable();}
halljoy::lifecycle::StopResult SharkStop(halljoy::lifecycle::GenerationId generation){
 parentStop=true;
 if(supervisor.joinable() && WaitForSingleObject(supervisor.native_handle(),6000)!=WAIT_OBJECT_0)
  return NativeAnalogBackendStopFailed(generation,halljoy::lifecycle::LifecycleErrorCode::StopTimedOut);
 SharkDiagnostic_Stop();return NativeAnalogBackendStopJoined(generation);
}
bool SharkConnected(){const auto last=nativeLast.load();return nativeReady && !parentStop && last && GetTickCount64()-last<=500;}
bool SharkOwns(std::uint16_t hid){return SharkConnected() && (nativeValues.Owns(hid) || fnValues.Owns(hid));}
std::uint16_t SharkGet(std::uint16_t hid){if(!SharkOwns(hid))return 0;const auto now=GetTickCount64();
 return std::max(nativeValues.Read(hid,now,FreshBudget()).milli,fnValues.Read(hid,now,FreshBudget()).milli);}
void SharkTelemetry(NativeAnalogBackendTelemetry* out){
 if(!out)return;*out={};out->connected=SharkConnected();out->present=out->connected;
 const auto* profile=activeProfile.load();out->vendorId=0x3151;out->productId=profile?static_cast<std::uint16_t>(profile->pid):0;out->usagePage=0xffff;out->usage=2;
 if(out->connected && profile)out->verifiedLayoutToken=halljoy::sharklayout::Token(profile->id);
 if(profile)for(auto hid:profile->factory)if(hid)++out->mappedKeys;
 out->activeKeys=nativeValues.Active(GetTickCount64())+fnValues.Active(GetTickCount64());out->nominalRawLevels=shared?static_cast<unsigned>(Load(shared->units))*35/10+1:0;
 out->inputReportBytes=65;out->outputReportBytes=65;out->successfulUpdates=nativeUpdates.load();
 const auto last=nativeLast.load();out->lastUpdateAgeMs=last?static_cast<std::uint32_t>(GetTickCount64()-last):0;
 wcscpy_s(out->status,profile && (profile->id==3123 || profile->id==2308)?L"ATTACK SHARK: wired analog; factory Fn layer":L"ATTACK SHARK: factory Fn layer; provisional 3.5 mm range; testing incomplete");
}
}
const NativeAnalogBackendDescriptor& Shark_GetNativeBackendDescriptor(){
 static const NativeAnalogBackendDescriptor descriptor{
 kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),"attackshark-pro",L"ATTACK SHARK RY5088 magnetic keyboards",
 NativeAnalogProtocol::AttackSharkX65Pro,NativeAnalogStartPhase::AfterRawInput,
 NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe|NativeAnalogBackendFlag_RequiresRawInput,
 nullptr,&SharkStart,&SharkStop,nullptr,&SharkConnected,&SharkConnected,&SharkOwns,&SharkGet,&SharkTelemetry};
 return descriptor;
}
#endif
