#define WIN32_LEAN_AND_MEAN

#define NOMINMAX
#include "support_log.h"
// clang-format off: Windows HID headers require this dependency order.
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <winioctl.h>
#include <hidclass.h>
#include <initguid.h>
#include <devpkey.h>
// clang-format on
#include "rongyuan_stream_backend.h"
#include "debug_log.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
#include "rongyuan_stream_protocol.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
namespace {
namespace mg = halljoy::ry_stream;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = ~ULONGLONG{0}; // Delta values persist until release/disconnect.
std::atomic<bool> g_running{false}, g_stop{false}, g_present{false},
    g_connected{false};
std::atomic<std::uint64_t> g_token{0}, g_ok{0}, g_bad{0}, g_last{0};
std::atomic<std::uint32_t> g_avgUs{0}, g_maxUs{0};
std::atomic<unsigned> g_board{0}, g_units{0}, g_mapped{0}, g_vid{0}, g_pid{0};
std::mutex g_service, g_activeLock;
HANDLE g_thread = nullptr, g_wake = nullptr, g_active = INVALID_HANDLE_VALUE;
halljoy::physical_analog::Publication g_factory, g_assigned;
bool Remapped() {
  return halljoy::native_layout::UsesRemapping(g_token.load());
}
std::uint64_t HashPath(const std::wstring &s) {
  std::uint64_t h = 1469598103934665603ull;
  for (auto c : s) {
    h ^= static_cast<std::uint16_t>(towlower(c));
    h *= 1099511628211ull;
  }
  return h;
}
struct Handle {
  HANDLE v = INVALID_HANDLE_VALUE;
  Handle() = default;
  explicit Handle(HANDLE x) : v(x) {}
  ~Handle() {
    if (v != INVALID_HANDLE_VALUE)
      CloseHandle(v);
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  Handle(Handle &&x) noexcept : v(x.v) { x.v = INVALID_HANDLE_VALUE; }
  Handle &operator=(Handle &&x) noexcept {
    if (this != &x) {
      if (v != INVALID_HANDLE_VALUE)
        CloseHandle(v);
      v = x.v;
      x.v = INVALID_HANDLE_VALUE;
    }
    return *this;
  }
  explicit operator bool() const { return v != INVALID_HANDLE_VALUE; }
};
struct Candidate {
  std::wstring path, inputPath;
  GUID container{};
  bool control=false, input=false;
  HIDD_ATTRIBUTES attributes{};
  HIDP_CAPS caps{};
};

bool Feature(HANDLE h, bool write, mg::Report &data) {
  HidIoOperation op(h);
  DWORD error = 0, done = 0;
  const auto start = op.StartControl(
      write ? IOCTL_HID_SET_FEATURE : IOCTL_HID_GET_FEATURE, data.data(), 65,
      write ? nullptr : data.data(), write ? 0 : 65, &error);
  if (start == HidIoOperation::StartResult::Failed)
    return false;
  if (start == HidIoOperation::StartResult::Pending &&
      op.Wait(50) != WAIT_OBJECT_0) {
    op.CancelAndDrain(&done, &error);
    return false;
  }
  return op.Finish(&done, &error, false) &&
         (write || (done == 64 && data[0] == 0));
}

std::vector<Candidate> Enumerate(bool log) {
  GUID guid{};
  HidD_GetHidGuid(&guid);
  HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (set == INVALID_HANDLE_VALUE)
    return {};
  std::vector<Candidate> out;
  for (DWORD i = 0;; ++i) {
    SP_DEVICE_INTERFACE_DATA iface{};
    iface.cbSize = sizeof(iface);
    if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &iface)) {
      if (GetLastError() == ERROR_NO_MORE_ITEMS)
        break;
      continue;
    }
    DWORD bytes = 0;
    SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &bytes, nullptr);
    if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
      continue;
    std::vector<std::uint8_t> mem(bytes);
    auto *detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(mem.data());
    detail->cbSize = sizeof(*detail);
    SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);
    if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, bytes, nullptr,
                                          &dev))
      continue;
    Handle meta(CreateFileW(detail->DevicePath, 0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!meta)
      continue;
    HIDD_ATTRIBUTES a{};
    a.Size = sizeof(a);
    if (!HidD_GetAttributes(meta.v, &a) || !mg::Candidate(a.VendorID,a.ProductID))
      continue;
    PHIDP_PREPARSED_DATA pp = nullptr;
    if (!HidD_GetPreparsedData(meta.v, &pp))
      continue;
    HIDP_CAPS caps{};
    const NTSTATUS status = HidP_GetCaps(pp, &caps);
    bool input=false;
    if(status==HIDP_STATUS_SUCCESS && caps.UsagePage==0xffff && caps.Usage==1 && caps.InputReportByteLength==32 && caps.NumberInputValueCaps==1) {
      HIDP_VALUE_CAPS value{};USHORT count=1;
      input=HidP_GetValueCaps(HidP_Input,&value,&count,pp)==HIDP_STATUS_SUCCESS && count==1 && value.ReportID==5 && value.BitSize==8 && value.ReportCount==31;
    }
    HidD_FreePreparsedData(pp);
    const bool exact = status == HIDP_STATUS_SUCCESS &&
                       (caps.UsagePage == 0xffff || caps.UsagePage == 0xff00) &&
                       caps.Usage == 2 && caps.FeatureReportByteLength == 65;
    if (log)
      DebugLog_Write(
          L"[rongyuan.stream.enumerate] path_hash=%016llX version=%04X "
          L"exact=%d",
          static_cast<unsigned long long>(HashPath(detail->DevicePath)),
          a.VersionNumber, exact ? 1 : 0);
    GUID container{}; DEVPROPTYPE type=0;DWORD required=0;
    if((exact || input) && SetupDiGetDevicePropertyW(set,&dev,&DEVPKEY_Device_ContainerId,&type,reinterpret_cast<PBYTE>(&container),sizeof(container),&required,0) && type==DEVPROP_TYPE_GUID && !IsEqualGUID(container,GUID{}))
      out.push_back({detail->DevicePath,L"",container,exact,input,a,caps});
  }
  SetupDiDestroyDeviceInfoList(set);
  std::vector<Candidate> paired;
  for(auto c:out)if(c.control) {
    unsigned matches=0;
    for(const auto& i:out)if(i.input && i.attributes.VendorID==c.attributes.VendorID && i.attributes.ProductID==c.attributes.ProductID && IsEqualGUID(i.container,c.container)) {c.inputPath=i.path;++matches;}
    if(matches==1)paired.push_back(c);
  }
  return paired;
}

class Session {
public:
  const mg::Model *model = nullptr;
  unsigned units = 0;
  explicit Session(const Candidate &c) : candidate(c) {}
  ~Session() {
    if(enabled) {auto disable=mg::Request(0x1b,0);Feature(handle.v,true,disable);}
    std::lock_guard<std::mutex> lock(g_activeLock);
    if (g_active == handle.v)
      g_active = INVALID_HANDLE_VALUE;
  }
  bool Open() {
    handle = Handle(CreateFileW(
        candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    if (!handle)
      return false;
    std::lock_guard<std::mutex> lock(g_activeLock);
    g_active = handle.v;
    return true;
  }
  template <class Validate>
  bool Query(const mg::Report &request, mg::Report &reply, Validate valid,
             bool optional = false) {
    if (poisoned || g_stop.load())
      return false;
    auto tx = request;
    if (!Feature(handle.v, true, tx))
      return poisoned = true, false;
    const auto deadline = GetTickCount64() + 50;
    do {
      reply = {};
      if (!Feature(handle.v, false, reply))
        return poisoned = true, false;
      if (valid(reply))
        return true;
      if (g_wake)
        WaitForSingleObject(g_wake, 1);
    } while (!g_stop.load() && GetTickCount64() < deadline);
    if (!optional)
      poisoned = true;
    return false;
  }
  bool OpenInput() {
    input=Handle(CreateFileW(candidate.inputPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,nullptr));
    return input && HidD_SetNumInputBuffers(input.v,128) && HidD_FlushQueue(input.v);
  }
  bool Enable() {
    auto command=mg::Request(0x1b,1);
    enabled=true; // Also restore if a timed-out write reached the device.
    return Feature(handle.v,true,command);
  }
  HANDLE Input() const {return input.v;}
  unsigned VendorId() const {return candidate.attributes.VendorID;}
  unsigned ProductId() const {return candidate.attributes.ProductID;}
  bool Match(const mg::Report& r) {
    const auto board=mg::Board(r);
    const bool match=mg::Find(board,candidate.attributes.VendorID,candidate.attributes.ProductID)!=nullptr;
    if(board && !match && board!=lastRejectedBoard){lastRejectedBoard=board;
      SupportLog_Event("rongyuan.identity_pair_rejected",board,(unsigned(candidate.attributes.VendorID)<<16)|candidate.attributes.ProductID);
    }
    return match;
  }
  const mg::Model* Model(const mg::Report& r) {return mg::Find(mg::Board(r),candidate.attributes.VendorID,candidate.attributes.ProductID);}

private:
  Candidate candidate;
  unsigned lastRejectedBoard=0;
  Handle handle,input;
  bool enabled=false;
  bool poisoned = false;
};
void Clear() {
  g_connected.store(false);
  halljoy::native_layout::Clear(g_token.exchange(0));
  g_factory.Clear();
  g_assigned.Clear();
  g_last.store(0);
}
bool Proof(Session &s) {
  mg::Report r{};
  if (!s.Query(mg::Request(0x8f), r,
               [&](const auto &x) { return s.Match(x); }))
    return false;
  s.model = s.Model(r);
  unsigned version = r[8] | (r[9] << 8);
  mg::Report rf{};
  // Manufacturer scaling uses the radio/controller firmware when present,
  // even on wired USB. Do not derive travel units from USB bcdDevice.
  if(s.Query(mg::Request(0x80),rf,[](const auto& x){return x[1]==0x80;},true)) {
    const unsigned radio=rf[2] | (rf[3]<<8);
    if(radio && radio!=65535)version=radio;
  }
  mg::Report features{};
  // Old firmware has no E6 reply marker: use its documented version scale.
  s.Query(
      mg::Request(0xe6), features,
      [](const auto &x) { return x[1] == 0xe6 && x[2] == 0xaa; }, true);
  s.units = mg::Units(version, features, s.model->precisionEnum);
  if (!s.units)
    return false;
  return true;
}
bool Map(Session &s) {
  mg::Report r{};
  if (!s.Query(mg::Request(0x84, 255), r,
               [](const auto &x) { return x[1] == 0x84 && x[2] < 8; }))
    return false;
  const auto profile = r[2];
  mg::Matrix assigned{};
  for (unsigned page = 0; page < 8; ++page) {
    if (!s.Query(
            mg::Request(0x8a, profile, 255, static_cast<std::uint8_t>(page)), r,
            mg::ValidAssignments))
      return false;
    std::copy(r.begin() + 1, r.end(), assigned.begin() + 64 * page);
  }
  std::array<halljoy::native_layout::Key, 128> keys{};
  unsigned count = 0;
  for (unsigned slot = 0; slot < 128; ++slot) {
    const auto factory = mg::Decode(s.model->matrix.data() + slot * 4);
    if (!factory)
      continue;
    const auto action = mg::Decode(assigned.data() + slot * 4);
    const auto id = static_cast<std::uint8_t>(slot + 1);
    if (!g_factory.Bind(id, factory) ||
        (action && !g_assigned.Bind(id, action)))
      return false;
    keys[count++] = {factory, action};
  }
  const auto token =
      halljoy::layout_identity::Token("rongyuan-stream", s.model->product);
  if (token && !halljoy::native_layout::Publish(token, keys.data(), count))
    return false;
  g_token.store(token);
  g_board.store(s.model->board);
  g_vid.store(s.VendorId());g_pid.store(s.ProductId());
  g_units.store(s.units);
  g_mapped.store(count);
  return true;
}
bool Publish(const mg::Sample& sample, const mg::Model& model, std::uint64_t now) {
  if(sample.raw>6*g_units.load())return false;
  if(!mg::Decode(model.matrix.data()+sample.slot*4))return true; // Known non-key/encoder slots never become analog keys.
  const auto id=static_cast<std::uint8_t>(sample.slot+1);
  const auto value=mg::Normalize(sample.raw,g_units.load(),model.rangeUm);
  g_factory.Publish(id,value,now);g_assigned.Publish(id,value,now);
  g_last.store(now);++g_ok;return true;
}
bool Run(const Candidate &c) {
  Clear();
  Session s(c);
  const char *phase = "open";
  const bool ready = s.Open() && (phase = "identity proof", Proof(s)) &&
                     (phase = "assignments", Map(s)) &&
                     (phase = "input open", s.OpenInput());
  if (!ready) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.stream] admission failed phase=%hs error=%lu",
                   phase, GetLastError());
    Clear();
    return false;
  }
  if (!NativeAnalogRouting_Claim(c.attributes.VendorID, c.attributes.ProductID, c.path.c_str(),
                                 NativeAnalogProtocol::RongYuanStream) &&
      !NativeAnalogRouting_IsClaimedBy(
          c.path.c_str(), NativeAnalogProtocol::RongYuanStream)) {
    Clear();
    return false;
  }
  if(!NativeAnalogRouting_Claim(c.attributes.VendorID,c.attributes.ProductID,c.inputPath.c_str(),NativeAnalogProtocol::RongYuanStream) &&
     !NativeAnalogRouting_IsClaimedBy(c.inputPath.c_str(),NativeAnalogProtocol::RongYuanStream)) {Clear();return false;}
  if(!s.Enable()) {++g_bad;DebugLog_Write(L"[rongyuan.stream] start command failed error=%lu",GetLastError());Clear();return false;}
  g_present.store(true);
  g_connected.store(true);
  DebugLog_Write(L"[rongyuan.stream] connected board=%u keys=%u "
                 L"units_per_mm=%u experimental=1",
                 s.model->board, g_mapped.load(), s.units);
  std::array<unsigned char,32> report{};
  HidIoOperation read(s.Input());
  while (!g_stop.load()) {
    DWORD error=0,done=0;
    const auto start=read.StartRead(report.data(),static_cast<DWORD>(report.size()),&error);
    if(start==HidIoOperation::StartResult::Failed) {++g_bad;break;}
    bool failed=false;
    if(start==HidIoOperation::StartResult::Pending) {
      HANDLE events[]={read.Event(),g_wake};
      while(!g_stop.load()) {
        const auto wait=WaitForMultipleObjects(2,events,FALSE,1000);
        if(wait==WAIT_OBJECT_0)break;
        if(wait==WAIT_FAILED) {failed=true;break;}
      }
    }
    if(g_stop.load() || failed) {read.CancelAndDrain(&done,&error);break;}
    if(!read.Finish(&done,&error,false)) {++g_bad;break;}
    // RID5 also carries vendor notifications unrelated to key travel.
    if(done==32 && report[0]==5 && report[1]!=0x1b)continue;
    mg::Sample sample{};
    if(!mg::Parse(report.data(),done,sample) || !Publish(sample,*s.model,GetTickCount64())) {
      ++g_bad;DebugLog_Write(L"[rongyuan.stream] invalid input bytes=%lu; session stopped",done);break;
    }
  }
  DebugLog_Write(
      L"[rongyuan.stream] session ended stop=%d error=%lu failures=%llu",
      g_stop.load() ? 1 : 0, GetLastError(),
      static_cast<unsigned long long>(g_bad.load()));
  Clear();
  return false;
}
unsigned __stdcall Worker(void *) {
  try {
    while (!g_stop.load()) {
      const auto candidates = Enumerate(false);
      g_present.store(!candidates.empty());
      for (const auto &c : candidates) {
        if (g_stop.load())
          break;
        if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
            !NativeAnalogRouting_IsClaimedBy(
                c.path.c_str(), NativeAnalogProtocol::RongYuanStream))
          continue;
        Run(c);
        if (g_stop.load())
          break;
      }
      if (g_wake && !g_stop.load())
        WaitForSingleObject(g_wake, candidates.empty() ? INFINITE : 1000);
    }
  } catch (...) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.stream] worker exception; input cleared");
  }
  Clear();
  g_running.store(false);
  return 0;
}
bool Prepare() {
  std::lock_guard<std::mutex> lock(g_service);
  if (g_thread)
    return g_present.load();
  g_stop.store(false);
  bool any = false;
  for (const auto &c : Enumerate(true)) {
    if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
        !NativeAnalogRouting_IsClaimedBy(
            c.path.c_str(), NativeAnalogProtocol::RongYuanStream))
      continue;
    Session s(c);
    if (!s.Open() || !Proof(s))
      continue;
    any =
        NativeAnalogRouting_Claim(c.attributes.VendorID, c.attributes.ProductID, c.path.c_str(),
                                  NativeAnalogProtocol::RongYuanStream) ||
        any;
    NativeAnalogRouting_Claim(c.attributes.VendorID,c.attributes.ProductID,c.inputPath.c_str(),NativeAnalogProtocol::RongYuanStream);
  }
  g_present.store(any);
  return any;
}
bool Start() {
  std::lock_guard<std::mutex> lock(g_service);
  if (g_thread)
    return g_running.load();
  g_stop.store(false);
  g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!g_wake)
    return false;
  g_running.store(true);
  unsigned id = 0;
  g_thread = reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, Worker, nullptr, 0, &id));
  if (!g_thread) {
    g_running.store(false);
    CloseHandle(g_wake);
    g_wake = nullptr;
    return false;
  }
  return true;
}
halljoy::lifecycle::StopResult
Stop(halljoy::lifecycle::GenerationId generation) {
  std::lock_guard<std::mutex> lock(g_service);
  g_stop.store(true);
  if (!g_thread) {
    Clear();
    return NativeAnalogBackendStopJoined(generation);
  }
  if (g_wake)
    SetEvent(g_wake);
  {
    std::lock_guard<std::mutex> active(g_activeLock);
    if (g_active != INVALID_HANDLE_VALUE)
      CancelIoEx(g_active, nullptr);
  }
  const auto wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
  if (wait != WAIT_OBJECT_0)
    return NativeAnalogBackendStopFailed(
        generation, halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,
        wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
  CloseHandle(g_thread);
  g_thread = nullptr;
  CloseHandle(g_wake);
  g_wake = nullptr;
  Clear();
  return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
  std::lock_guard<std::mutex> lock(g_service);
  if (g_wake)
    SetEvent(g_wake);
}
bool Present() { return g_present.load(); }
bool Connected() { return g_connected.load(); }
bool Owns(std::uint16_t hid) {
  return Connected() && (Remapped() ? g_assigned : g_factory).Owns(hid);
}
std::uint16_t Get(std::uint16_t hid) {
  return Owns(hid) ? (Remapped() ? g_assigned : g_factory)
                         .Read(hid, GetTickCount64(), kFreshMs)
                         .milli
                   : 0;
}
void Telemetry(NativeAnalogBackendTelemetry *out) {
  if (!out)
    return;
  *out = {};
  out->present = Present();
  out->connected = Connected();
  out->verifiedLayoutToken = Connected() ? g_token.load() : 0;
  out->vendorId = static_cast<std::uint16_t>(g_vid.load());
  out->productId = static_cast<std::uint16_t>(g_pid.load());
  out->usagePage = 0xffff;
  out->usage = 2;
  out->mappedKeys = Connected() ? g_mapped.load() : 0;
  for(unsigned hid=1;hid<halljoy::physical_analog::kHidCount;++hid)if(g_factory.Read(static_cast<std::uint16_t>(hid),GetTickCount64(),kFreshMs).milli)++out->activeKeys;
  const auto *model = mg::Find(g_board.load(),g_vid.load(),g_pid.load());
  out->nominalRawLevels =
      model ? model->rangeUm * g_units.load() / 1000 + 1 : 0;
  out->inputReportBytes = 32;
  out->outputReportBytes = 0;
  out->successfulUpdates = g_ok.load();
  out->failedUpdates = g_bad.load();
  out->averageIntervalUs = g_avgUs.load();
  out->maximumIntervalUs = g_maxUs.load();
  out->updateHz10 = g_avgUs.load() ? 10000000 / g_avgUs.load() : 0;
  const auto last = g_last.load(), now = GetTickCount64();
  out->lastUpdateAgeMs =
      last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                0xffffffffull, now - last))
                          : 0;
  _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
               L"%s: USB analog stream; hardware/range validation pending",
               model ? model->name : L"RongYuan");
}
} // namespace
const NativeAnalogBackendDescriptor &
RongYuanStream_GetNativeBackendDescriptor() {
  static const NativeAnalogBackendDescriptor d{
      kNativeAnalogBackendAbiVersion,
      sizeof(NativeAnalogBackendDescriptor),
      "rongyuan-stream",
      L"RongYuan USB stream (experimental)",
      NativeAnalogProtocol::RongYuanStream,
      NativeAnalogStartPhase::BeforeUap,
      NativeAnalogBackendFlag_ReadOnlyProbe,
      &Prepare,
      &Start,
      &Stop,
      &Notify,
      &Present,
      &Connected,
      &Owns,
      &Get,
      &Telemetry};
  return d;
}
