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
#include "steelseries_apex_backend.h"
#include "steelseries_apex_protocol.h"
#include "steelseries_apex_gen3_protocol.h"
#include "debug_log.h"
#include "support_log.h"
#include "hid_io_operation.h"
#include "physical_analog_state.h"
#include "native_layout_state.h"
#include "generated/layout_pipeline/identities.h"


namespace {
namespace tp=halljoy::apex;
namespace tg=halljoy::apex::gen3;
std::atomic<std::uint64_t> g_layoutToken{0};
std::atomic<unsigned> g_maxIntervalUs{0},g_averageIntervalUs{0};

constexpr std::uint64_t kHold=100;
std::atomic<unsigned> g_pid{0},g_mapped{0};
std::atomic<bool> g_stop{false},g_running{false},g_present{false},g_connected{false};
// The last exclusive open failed because another program holds the interface.
std::atomic<bool> g_busy{false};
std::atomic<std::uint64_t> g_ok{0},g_bad{0},g_last{0};
std::atomic<unsigned> g_bytes{0},g_page{0},g_usage{0};
std::mutex g_service;
HANDLE g_thread=nullptr,g_wake=nullptr;
halljoy::physical_analog::Publication g_values;
halljoy::physical_analog::Publication g_assigned[2];
std::atomic<unsigned> g_assignedIndex{0};
// Enumeration is serialized by the service lifecycle. Log descriptor shapes once,
// not every polling iteration, and never put key activity into the support log.
std::vector<std::uint64_t> g_loggedCaps;
std::uint64_t FirmwareBytes(const tp::Report& p) {
 std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(p[i+1])<<(8*i);return v;
}
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
  if(!HidD_GetAttributes(meta.v,&a) || !tp::SupportedIdentity(a.VendorID,a.ProductID))continue;
  PHIDP_PREPARSED_DATA pp=nullptr;if(!HidD_GetPreparsedData(meta.v,&pp))continue;
  HIDP_CAPS caps{};bool valid=false;
  valid=HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS && (a.ProductID==tg::kPid ? tg::Collection(caps.UsagePage,caps.Usage,
        caps.InputReportByteLength,caps.OutputReportByteLength,caps.FeatureReportByteLength) : tp::SupportedCollection(caps.UsagePage,caps.Usage,
        caps.InputReportByteLength,caps.OutputReportByteLength,caps.FeatureReportByteLength));
  HidD_FreePreparsedData(pp);
  const std::uint64_t shape=(std::uint64_t(caps.UsagePage)<<48)|(std::uint64_t(caps.InputReportByteLength)<<32)|
      (std::uint64_t(caps.OutputReportByteLength)<<16)|caps.FeatureReportByteLength;
  if(g_loggedCaps.size()<32 && std::find(g_loggedCaps.begin(),g_loggedCaps.end(),shape)==g_loggedCaps.end()) {
   g_loggedCaps.push_back(shape);SupportLog_Event("apex.hid_caps_page_in_out_feature",shape,SupportLog_Data(caps.Usage));
   SupportLog_Event(valid?"apex.collection_accepted":"apex.collection_rejected",shape);
  }
  if(valid)out.push_back({detail->DevicePath,caps,a.ProductID});
 }
 SetupDiDestroyDeviceInfoList(set);return out;
}
void Clear() {
 g_connected.store(false);halljoy::native_layout::Clear(g_layoutToken.exchange(0));
 g_values.Clear();for(auto& values:g_assigned)values.Clear();g_last.store(0);g_maxIntervalUs.store(0);g_averageIntervalUs.store(0);
}
bool Exchange(HANDLE handle,const tp::Report& request,tp::Report& reply,bool modern=false) {
 // Legacy replies have no command/bank echo. Exclusive access, one transaction,
 // exact length and immediate disconnect on any failure prevent page misattribution.
 if(!(modern?tg::ReadOnlyRequest(request):tp::ReadOnlyRequest(request)))return false;
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
   op.CancelAndDrain(&done,&error);if(!g_stop.load())error=ERROR_TIMEOUT;return false;
  }
  return op.Finish(&done,&error,false);
 };
 const auto deadline=GetTickCount64()+100;
 if(g_stop.load())return false;
 if(!finish(write,write.StartWrite(request.data(),65,&error),deadline) || done!=65) {
  SupportLog_Event("apex.write_failed",(std::uint64_t(request[1])<<48)|(std::uint64_t(request[2])<<32)|done,SupportLog_Win32(error));
  DebugLog_Write(L"[steelseries_apex] write failed cmd=%02x arg=%u bytes=%lu error=%lu",request[1],request[2],done,error);return false;
 }
 if(!finish(read,read.StartRead(reply.data(),65,&error),deadline) || done!=65 || reply[0]!=0) {
  SupportLog_Event("apex.reply_failed",(std::uint64_t(request[1])<<48)|(std::uint64_t(request[2])<<32)|done,SupportLog_Win32(error));
  DebugLog_Write(L"[steelseries_apex] reply failed cmd=%02x arg=%u bytes=%lu error=%lu",request[1],request[2],done,error);return false;
 }
 return true;
}
bool ReadRanges(HANDLE h,tp::Ranges& ranges) {
 tp::Ranges next{};tp::Report reply{};
 for(unsigned start=0;start<68;start+=12) {
  const auto count=std::min(12u,68-start);
  if(!Exchange(h,tp::RangeRequest(start,count),reply) || !tp::ParseRanges(reply,start,count,next)) {
   SupportLog_Event("apex.calibration_reply_rejected",start,SupportLog_Data(count));
   DebugLog_Write(L"[steelseries_apex] calibration read rejected start=%u count=%u first=%02x %02x %02x %02x %02x",start,count,reply[1],reply[2],reply[3],reply[4],reply[5]);return false;
  }
 }
 // The main alphanumeric section must have meaningful firmware calibration.
 for(unsigned i=14;i<40;++i)if(next[i].high<=next[i].low) {
  SupportLog_Event("apex.calibration_unusable",0);
  DebugLog_Write(L"[steelseries_apex] missing calibration sensor=%u hid=%u",i,tp::kMap[i]);return false;
 }
 ranges=next;return true;
}
std::uint64_t Gen3NowUs(){
 static const std::uint64_t frequency=[](){LARGE_INTEGER f{};return QueryPerformanceFrequency(&f)?static_cast<std::uint64_t>(f.QuadPart):std::uint64_t{0};}();
 LARGE_INTEGER t{};if(!frequency || !QueryPerformanceCounter(&t))return GetTickCount64()*1000;
 const auto ticks=static_cast<std::uint64_t>(t.QuadPart);
 return (ticks/frequency)*1000000+(ticks%frequency)*1000000/frequency;
}
bool Gen3Ranges(HANDLE h,unsigned layout,tp::Ranges& ranges) {
 tp::Ranges next{};tp::Report reply{};
 for(unsigned start=0;start<68;start+=tg::kRangeCount){
  const auto count=std::min(tg::kRangeCount,68-start);
  if(!Exchange(h,tg::RangeRequest(start,count),reply,true))return false;
  const auto why=tg::ParseRanges(reply,start,count,next);
  if(why!=tg::Refusal::None){
   SupportLog_Event("apex.gen3_ranges_refused",(std::uint64_t(start)<<32)|unsigned(why),SupportLog_Protocol(reply[2]));
   DebugLog_Write(L"[steelseries_apex] Gen3 calibration rejected start=%u count=%u reason=%S status=%u",start,count,tg::RefusalName(why),reply[2]);return false;
  }
 }
 tg::PhysicalRanges(layout,next);
 for(unsigned i=0;i<68;++i)if(tp::kMap[i]>=4 && tp::kMap[i]<=29 && next[i].high<=next[i].low){SupportLog_Event("apex.gen3_calibration_unusable",i);return false;}
 ranges=next;return true;
}
bool Gen3Mappings(HANDLE h,tg::Mappings& maps){
 tg::Mappings next{};tp::Report reply{};
 for(unsigned start=0;start<68;start+=tg::kMappingCount){
  const auto count=std::min(tg::kMappingCount,68-start);
  if(!Exchange(h,tg::MappingRequest(start,count),reply,true))return false;
  const auto why=tg::ParseMappings(reply,start,count,next);
  if(why!=tg::Refusal::None){
   SupportLog_Event("apex.gen3_mapping_refused",(std::uint64_t(start)<<32)|unsigned(why),SupportLog_Protocol(reply[2]));
   DebugLog_Write(L"[steelseries_apex] Gen3 mapping rejected start=%u count=%u reason=%S status=%u",start,count,tg::RefusalName(why),reply[2]);return false;
  }
 }
 maps=next;return true;
}
bool Gen3PublishMappings(std::uint64_t token,const tg::Mappings& maps,const tp::Ranges& ranges,const tp::Matrix& values,std::uint64_t now,unsigned layout){
 const auto index=1u-g_assignedIndex.load();auto& assigned=g_assigned[index];assigned.Clear();
 std::array<halljoy::native_layout::Key,68> keys{};
 unsigned remapped=0,special=0,keyCount=0;
 for(unsigned i=0;i<68;++i){
  if(!tg::PhysicalKey(layout,tp::kMap[i]))continue;
  auto& key=keys[keyCount++];
  key.factory=static_cast<std::uint16_t>(tg::FactoryHid(i));key.assigned=maps[i].assigned;
  if(maps[i].kind!=tg::MappingKind::Keyboard)wcscpy_s(key.label.data(),key.label.size(),tg::MappingLabel(maps[i].kind));
  remapped+=maps[i].assigned!=key.factory;special+=maps[i].kind!=tg::MappingKind::Keyboard;
  if(ranges[i].high>ranges[i].low){
   if(!assigned.Bind(static_cast<std::uint8_t>(i+1),maps[i].assigned))return false;
   assigned.Publish(static_cast<std::uint8_t>(i+1),tp::Normalize(values[i],ranges[i]),now);
  }
 }
 if(!halljoy::native_layout::Publish(token,keys.data(),keyCount))return false;
 g_layoutToken.store(token);g_assignedIndex.store(index);SupportLog_Event("apex.gen3_remap_ready",remapped,SupportLog_Data(special));return true;
}
void RunGen3(HANDLE h,const Candidate& c) {
 SupportLog_Event("apex.gen3_revision",1);
 SupportLog_Event("apex.gen3_stage",1);
 tp::Report reply{};
 if(!Exchange(h,tp::VersionRequest(),reply,true)){++g_bad;return;}
 std::uint64_t version=0;for(unsigned i=0;i<8;++i)version|=std::uint64_t(reply[i+3])<<(i*8);
 SupportLog_Event("apex.gen3_firmware_ascii_le",version,SupportLog_Protocol(reply[2]));
 if(!tg::Version(reply)){SupportLog_Event("apex.gen3_firmware_not_admitted",version);++g_bad;return;}
 SupportLog_Event("apex.gen3_stage",2);
 unsigned layout=255,region=255;
 if(!Exchange(h,tg::LayoutRequest(),reply,true) || !tg::Layout(reply,layout)){
  SupportLog_Event("apex.gen3_layout_refused",reply[2]);++g_bad;return;
 }
 if(!Exchange(h,tg::RegionRequest(),reply,true) || !tg::Region(reply,region)){
  SupportLog_Event("apex.gen3_region_refused",reply[2]);++g_bad;return;
 }
 const auto token=halljoy::layout_identity::Token("steelseries-apex",tg::LayoutProduct(layout));
 if(!token){SupportLog_Event("apex.gen3_layout_missing",layout);++g_bad;return;}
 SupportLog_Event("apex.gen3_layout_region",(std::uint64_t(layout)<<32)|region);
 SupportLog_Event("apex.gen3_stage",3);
 tp::Ranges ranges{};if(!Gen3Ranges(h,layout,ranges)){++g_bad;return;}
 SupportLog_Event("apex.gen3_stage",4);
 tg::Mappings maps{};if(!Gen3Mappings(h,maps)){++g_bad;return;}
 bool mappingChanged=true;
 unsigned mapped=0,spanMin=4096,spanMax=0;
 for(unsigned i=0;i<68;++i)if(ranges[i].high>ranges[i].low){
  if(!g_values.Bind(static_cast<std::uint8_t>(i+1),static_cast<std::uint16_t>(tg::FactoryHid(i)))){++g_bad;return;}
  ++mapped;const unsigned span=ranges[i].high-ranges[i].low;spanMin=std::min(spanMin,span);spanMax=std::max(spanMax,span);
 }
 SupportLog_Event("apex.gen3_calibrated_keys",mapped,SupportLog_Data((std::uint64_t(spanMin)<<32)|spanMax));
 g_mapped.store(mapped);auto summary=GetTickCount64(),nextRefresh=summary+2000,previousPublishUs=std::uint64_t{0};
 std::uint64_t frames=0,changes=0,releases=0,intervalSumUs=0;unsigned refreshPart=0;
 tp::Ranges pendingRanges{};tg::Mappings pendingMaps{};
 unsigned maxUs=0,observedMin=4096,observedMax=0;tp::Matrix previous{};
 auto log=[&]{
  SupportLog_Event("apex.gen3_frames",frames,SupportLog_Data(maxUs));
  SupportLog_Event("apex.gen3_sensor_changes",changes,SupportLog_Data(releases));
  SupportLog_Event("apex.gen3_observed_raw_minmax",observedMin,SupportLog_Data(observedMax));
  SupportLog_Event("apex.gen3_update_interval_us",g_averageIntervalUs.load(),SupportLog_Data(maxUs));
  DebugLog_Write(L"[steelseries_apex] Gen3 frames=%llu sensor_changes=%llu returned_to_low=%llu average_update_interval_us=%u max_update_interval_us=%u",frames,changes,releases,g_averageIntervalUs.load(),maxUs);
 };
 while(!g_stop.load()){
  const auto startMs=GetTickCount64();
  tp::Matrix values{};bool valid=true;
  for(unsigned start=0;start<68;start+=tg::kDepthCount){
   const auto count=std::min(tg::kDepthCount,68-start);
   if(!Exchange(h,tg::DepthRequest(start,count),reply,true)){valid=false;break;}
   const auto why=tg::ParseDepth(reply,start,count,values);
   if(why!=tg::Refusal::None){
    SupportLog_Event("apex.gen3_depth_refused",(std::uint64_t(start)<<32)|unsigned(why),SupportLog_Protocol(reply[2]));
    DebugLog_Write(L"[steelseries_apex] Gen3 depth rejected start=%u count=%u reason=%S status=%u",start,count,tg::RefusalName(why),reply[2]);valid=false;break;
   }
  }
  const auto now=GetTickCount64(),nowUs=Gen3NowUs();const auto elapsed=static_cast<unsigned>(std::min<std::uint64_t>(0xffffffff,now-startMs));
  if(!valid || elapsed>100){if(elapsed>100)SupportLog_Event("apex.gen3_frame_expired",elapsed);++g_bad;break;}
  const auto sensorFaults=tg::SensorFaults(layout,values,ranges);
  if(sensorFaults){SupportLog_Event("apex.gen3_sensor_faults",sensorFaults);++g_bad;break;}
  if(mappingChanged){if(!Gen3PublishMappings(token,maps,ranges,values,now,layout)){++g_bad;break;}mappingChanged=false;}
  if(!g_connected.load()){
   if(!NativeAnalogRouting_Claim(tp::kVid,c.pid,c.path.c_str(),NativeAnalogProtocol::SteelSeriesApex) &&
      !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::SteelSeriesApex))break;
   g_layoutToken.store(token);SupportLog_Event("apex.gen3_stage",5);SupportLog_Event("apex.gen3_analog_connected",mapped);
   DebugLog_Write(L"[steelseries_apex] exact wired Gen3 1038:1642 firmware 1.19.7; live sensor reads, layout=%u region=%u keys=%u",layout,region,mapped);
  }
  for(unsigned i=0;i<68;++i){
   const auto value=tp::Normalize(values[i],ranges[i]);
   if(ranges[i].high>ranges[i].low){observedMin=std::min(observedMin,unsigned(values[i]));observedMax=std::max(observedMax,unsigned(values[i]));changes+=values[i]!=previous[i];releases+=previous[i]>ranges[i].low && values[i]<=ranges[i].low;}
   g_values.Publish(static_cast<std::uint8_t>(i+1),value,now);
   g_assigned[g_assignedIndex.load()].Publish(static_cast<std::uint8_t>(i+1),value,now);
  }
  previous=values;
  if(previousPublishUs){
   const auto interval=static_cast<unsigned>(std::min<std::uint64_t>(0xffffffff,nowUs-previousPublishUs));
   intervalSumUs+=interval;maxUs=std::max(maxUs,interval);
   g_averageIntervalUs.store(static_cast<unsigned>(intervalSumUs/frames));
  }
  previousPublishUs=nowUs;g_maxIntervalUs.store(maxUs);g_last.store(now);++g_ok;++frames;g_connected.store(true);
  if(frames==1 || frames==100 || frames==1000 || now-summary>=30000){log();summary=now;}
  // Publish input first. Amortize a read-only metadata sweep over 14 input
  // frames, one extra transaction per frame, instead of a 14-request pause.
  if(now>=nextRefresh){
   constexpr unsigned rangeParts=(68+tg::kRangeCount-1)/tg::kRangeCount;
   constexpr unsigned mapParts=(68+tg::kMappingCount-1)/tg::kMappingCount;
   tg::Refusal why=tg::Refusal::None;bool exchanged=false;
   if(refreshPart<rangeParts){
    const unsigned start=refreshPart*tg::kRangeCount,count=std::min(tg::kRangeCount,68-start);
    exchanged=Exchange(h,tg::RangeRequest(start,count),reply,true);
    if(exchanged)why=tg::ParseRanges(reply,start,count,pendingRanges);
   }else{
    const unsigned start=(refreshPart-rangeParts)*tg::kMappingCount,count=std::min(tg::kMappingCount,68-start);
    exchanged=Exchange(h,tg::MappingRequest(start,count),reply,true);
    if(exchanged)why=tg::ParseMappings(reply,start,count,pendingMaps);
   }
   if(!exchanged || why!=tg::Refusal::None){
    SupportLog_Event("apex.gen3_refresh_refused",refreshPart,SupportLog_Protocol(unsigned(why)));++g_bad;break;
   }
   if(++refreshPart==rangeParts+mapParts){
    tg::PhysicalRanges(layout,pendingRanges);bool same=true,usable=true;
    for(unsigned i=0;i<68;++i){
     if((ranges[i].high>ranges[i].low)!=(pendingRanges[i].high>pendingRanges[i].low))same=false;
     if(tp::kMap[i]>=4 && tp::kMap[i]<=29 && pendingRanges[i].high<=pendingRanges[i].low)usable=false;
    }
    if(!same || !usable){SupportLog_Event("apex.gen3_population_changed",mapped);++g_bad;break;}
    ranges=pendingRanges;mappingChanged=maps!=pendingMaps;maps=pendingMaps;
    refreshPart=0;nextRefresh=GetTickCount64()+2000;
   }
  }
 }
 log();SupportLog_Event("apex.gen3_session_end",g_stop.load()?1:0);
}
void Run(const Candidate& c) {
 Clear();g_pid.store(c.pid);g_bytes.store(65);g_page.store(c.caps.UsagePage);g_usage.store(c.caps.Usage);
 Handle input(CreateFileW(c.path.c_str(),GENERIC_READ|GENERIC_WRITE,0,
                         nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
 const DWORD openError=input?ERROR_SUCCESS:GetLastError();
 g_busy.store(!input && NativeAnalog_IsAccessBusyError(openError));
 if(!input) {
  // Once per distinct error: the worker retries every second.
  static std::atomic<DWORD> lastError{ERROR_SUCCESS};
  if(lastError.exchange(openError)!=openError)SupportLog_Event("apex.open_failed",0,SupportLog_Win32(openError));
  ++g_bad;DebugLog_Write(L"[steelseries_apex] exclusive vendor collection open failed error=%lu; another HID client may own it",openError);return;
 }
 // Revalidate the opened collection before any output after re-enumeration.
 HIDD_ATTRIBUTES live{};live.Size=sizeof(live);
 if(!HidD_GetAttributes(input.v,&live) || !tp::SupportedIdentity(live.VendorID,live.ProductID) || live.ProductID!=c.pid) {
  SupportLog_Event("apex.identity_changed",c.pid);++g_bad;return;
 }
 PHIDP_PREPARSED_DATA openedPp=nullptr;HIDP_CAPS openedCaps{};
 const bool gotCaps=HidD_GetPreparsedData(input.v,&openedPp)!=FALSE;
 const bool validCaps=gotCaps && HidP_GetCaps(openedPp,&openedCaps)==HIDP_STATUS_SUCCESS &&
  (c.pid==tg::kPid?tg::Collection(openedCaps.UsagePage,openedCaps.Usage,openedCaps.InputReportByteLength,openedCaps.OutputReportByteLength,openedCaps.FeatureReportByteLength):
                   tp::SupportedCollection(openedCaps.UsagePage,openedCaps.Usage,openedCaps.InputReportByteLength,openedCaps.OutputReportByteLength,openedCaps.FeatureReportByteLength));
 if(openedPp)HidD_FreePreparsedData(openedPp);
 if(!validCaps){SupportLog_Event("apex.opened_caps_refused",c.pid);++g_bad;return;}
 SupportLog_Event("apex.device",c.pid);
 if(!HidD_FlushQueue(input.v)) {SupportLog_Event("apex.flush_failed",0,SupportLog_Win32(GetLastError()));++g_bad;DebugLog_Write(L"[steelseries_apex] input queue flush failed error=%lu",GetLastError());return;}
 if(c.pid==tg::kPid){RunGen3(input.v,c);Clear();return;}
 tp::Report reply{};
 SupportLog_Event("apex.version_query",0x90);
 if(!Exchange(input.v,tp::VersionRequest(),reply)) {++g_bad;return;}
 DebugLog_Write(L"[steelseries_apex] firmware reply=%02x %02x %02x %02x %02x %02x %02x %02x",reply[1],reply[2],reply[3],reply[4],reply[5],reply[6],reply[7],reply[8]);
 SupportLog_Event("apex.firmware_ascii_le",FirmwareBytes(reply));
 if(!tp::KnownVersion(reply)) {SupportLog_Event("apex.firmware_not_admitted",FirmwareBytes(reply));++g_bad;DebugLog_Write(L"[steelseries_apex] firmware differs from reviewed 4.16.8; analog commands withheld");return;}
 tp::Ranges ranges{};
 if(!ReadRanges(input.v,ranges)) {++g_bad;return;}
 unsigned mapped=0;
 for(unsigned i=0;i<68;++i)if(ranges[i].high>ranges[i].low && tp::kMap[i]!=240) {
  if(!g_values.Bind(static_cast<std::uint8_t>(i+1),tp::kMap[i])) {++g_bad;Clear();return;}
  ++mapped;
  DebugLog_Write(L"[steelseries_apex] calibration sensor=%u hid=%u min=%u max=%u",i,tp::kMap[i],ranges[i].low,ranges[i].high);
 }
 SupportLog_Event("apex.calibration_ready",mapped);
 g_mapped.store(mapped);auto refreshed=GetTickCount64();
 while(!g_stop.load()) {
  tp::Matrix values{};bool valid=true;
  const auto frameStart=GetTickCount64();
  // Refresh learned extrema without changing any keyboard setting. A changed
  // populated-key set restarts admission instead of silently leaving stale aliases.
  if(frameStart-refreshed>=2000) {
   tp::Ranges next{};
   if(!ReadRanges(input.v,next)) {++g_bad;break;}
   for(unsigned i=0;i<68;++i)if((ranges[i].high>ranges[i].low)!=(next[i].high>next[i].low))valid=false;
   if(!valid){++g_bad;break;}
   ranges=next;refreshed=GetTickCount64();
  }
  for(unsigned bank=1;bank<=5;++bank) {
   if(!Exchange(input.v,tp::DepthRequest(bank),reply) || !tp::ParseDepth(reply,bank,values)) {
    SupportLog_Event("apex.depth_reply_rejected",bank);
    DebugLog_Write(L"[steelseries_apex] depth rejected bank=%u first=%02x %02x filtered=%02x %02x",bank,reply[1],reply[2],reply[29],reply[30]);valid=false;break;
   }
  }
  if(!valid || GetTickCount64()-frameStart>100) {++g_bad;break;}
  if(!g_connected.load()) {
   if(!NativeAnalogRouting_Claim(tp::kVid,c.pid,c.path.c_str(),NativeAnalogProtocol::SteelSeriesApex) &&
      !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::SteelSeriesApex))break;
   SupportLog_Event("apex.analog_connected",mapped);
   DebugLog_Write(L"[steelseries_apex] ADC matrix verified keys=%u W=%u A=%u S=%u D=%u; read-only 4.16.8 path",mapped,values[16],values[29],values[30],values[31]);
  }
  const auto now=GetTickCount64();
  for(unsigned i=0;i<68;++i)g_values.Publish(static_cast<std::uint8_t>(i+1),tp::Normalize(values[i],ranges[i]),now);
  g_last.store(now);++g_ok;g_connected.store(true);
  if(now==frameStart)WaitForSingleObject(g_wake,1);
 }
 SupportLog_Event("apex.session_end",g_stop.load()?1:0);
 Clear();
}
unsigned __stdcall Worker(void*) {
 try {
  while(!g_stop.load()) {
   const auto devices=Enumerate();g_present.store(!devices.empty());
   for(const auto& c:devices) {
    if(g_stop.load())break;
    if(NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
       !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::SteelSeriesApex))continue;
    Run(c);
   }
   // A transient metadata/open failure must not strand an already attached device.
   if(!g_stop.load())WaitForSingleObject(g_wake,devices.empty()?2000:1000);
  }
 }catch(...) {++g_bad;DebugLog_Write(L"[steelseries_apex] worker exception; input cleared");}
 Clear();g_busy.store(false);g_running.store(false);return 0;
}
bool Prepare() {
 std::lock_guard<std::mutex> lock(g_service);
 if(!g_thread)g_present.store(!Enumerate().empty());
 // Claim only after range validation and a complete depth snapshot.
 return g_present.load();
}
bool Start() {
 std::lock_guard<std::mutex> lock(g_service);
 SupportLog_Event("apex.backend_start",3);
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
const halljoy::physical_analog::Publication& ActiveValues(){
 return halljoy::native_layout::UsesRemapping(g_layoutToken.load())?g_assigned[g_assignedIndex.load()]:g_values;
}
bool Owns(std::uint16_t hid){return Connected() && ActiveValues().Owns(hid);}
std::uint16_t Get(std::uint16_t hid){return Owns(hid)?ActiveValues().Read(hid,GetTickCount64(),kHold).milli:0;}
void Telemetry(NativeAnalogBackendTelemetry* out) {
 if(!out)return;*out={};
 out->present=Present();out->connected=Connected();
 out->accessBusy=out->present && !out->connected && g_busy.load();
 out->vendorId=tp::kVid;out->productId=static_cast<std::uint16_t>(g_pid.load());out->nominalRawLevels=0;
 out->mappedKeys=Connected()?g_mapped.load():0;out->outputReportBytes=65;out->inputReportBytes=g_bytes.load();
 out->usagePage=static_cast<std::uint16_t>(g_page.load());out->usage=static_cast<std::uint16_t>(g_usage.load());
 const auto now=GetTickCount64(),last=g_last.load();
 if(Connected())out->activeKeys=g_values.Active(now);
 out->successfulUpdates=g_ok.load();out->failedUpdates=g_bad.load();
 out->lastUpdateAgeMs=last && now>=last?static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff,now-last)):0;
 wcscpy_s(out->deviceName,tp::ModelName(out->productId));
 out->maximumIntervalUs=g_maxIntervalUs.load();
 out->averageIntervalUs=g_averageIntervalUs.load();
 out->updateHz10=out->averageIntervalUs?10000000u/out->averageIntervalUs:0;
 if(Connected() && out->productId==tg::kPid)out->verifiedLayoutToken=g_layoutToken.load();
 // Only the original full-size Apex Pro has a verified layout (official GG geometry).
 if(Connected() && out->productId==tp::kPid)
  out->verifiedLayoutToken=halljoy::layout_identity::Token("steelseries-apex","APEXPRO-1038-1610");
 wcscpy_s(out->status,Connected()?L"Apex Pro: analog sensors connected":
                              out->productId==tg::kPid?L"Apex Pro TKL Gen 3: waiting for reviewed firmware 1.19.7 and live sensors":
                              L"Apex Pro: waiting for firmware 4.16.8 and calibrated sensor data");
}
}
const NativeAnalogBackendDescriptor& SteelSeriesApex_GetNativeBackendDescriptor() {
 static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,sizeof(NativeAnalogBackendDescriptor),
 "steelseries_apex",L"SteelSeries Apex Pro",NativeAnalogProtocol::SteelSeriesApex,
 NativeAnalogStartPhase::BeforeUap,NativeAnalogBackendFlag_PolledTransport|NativeAnalogBackendFlag_ReadOnlyProbe,
 &Prepare,&Start,&Stop,&Notify,&Present,&Connected,&Owns,&Get,&Telemetry};
 return d;
}
