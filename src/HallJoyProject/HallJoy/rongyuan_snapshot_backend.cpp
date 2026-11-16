#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off: Windows HID headers require this dependency order.
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <winioctl.h>
#include <hidclass.h>
// clang-format on
#include "rongyuan_snapshot_backend.h"
#include "debug_log.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
#include "rongyuan_snapshot_protocol.h"
#include "support_log.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <chrono>
#include <cstdio>
#include <shellapi.h>
#include <thread>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
namespace {
namespace mg = halljoy::rongyuan;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = 150;
std::atomic<bool> g_running{false}, g_stop{false}, g_present{false},
    g_connected{false};
// The last exclusive open failed because another program holds the interface.
std::atomic<bool> g_busy{false};
std::atomic<std::uint64_t> g_token{0}, g_ok{0}, g_bad{0}, g_last{0};
std::atomic<std::uint32_t> g_avgUs{0}, g_maxUs{0};
std::atomic<unsigned> g_board{0}, g_units{0}, g_mapped{0};
std::atomic<std::uint64_t> g_topology{0};
std::mutex g_service, g_activeLock;
HANDLE g_thread = nullptr, g_wake = nullptr, g_active = INVALID_HANDLE_VALUE;
halljoy::physical_analog::Publication g_factory, g_assigned;
#include "rongyuan_snapshot_evidence.h"
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
  std::wstring path;
  HIDD_ATTRIBUTES attributes{};
  HIDP_CAPS caps{};
  bool privatePipe = false; // Explicit isolated self-test transport only.
};

bool Feature(HANDLE h, bool write, mg::Report &data, bool privatePipe, DWORD& error) {
  HidIoOperation op(h);
  DWORD done = 0;
  error = ERROR_SUCCESS;
  const auto start = privatePipe
      ? (write ? op.StartWrite(data.data(),65,&error) : op.StartRead(data.data(),65,&error))
      : op.StartControl(write ? IOCTL_HID_SET_FEATURE : IOCTL_HID_GET_FEATURE,
                        data.data(),65,write ? nullptr : data.data(),write ? 0 : 65,&error);
  if (start == HidIoOperation::StartResult::Failed)
    return false;
  if (start == HidIoOperation::StartResult::Pending &&
      op.Wait(50) != WAIT_OBJECT_0) {
    op.CancelAndDrain(&done, &error);
    error=WAIT_TIMEOUT;
    return false;
  }
  if(!op.Finish(&done,&error,false)) return false;
  if(!write && (done != (privatePipe?65u:64u) || data[0]!=0)) {
    Record("feature_frame","short_or_report_id",0,0,ERROR_INVALID_DATA,done|(unsigned(data[0])<<16));
    error=ERROR_INVALID_DATA;return false;
  }
  return true;
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
    if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, bytes, nullptr,
                                          nullptr))
      continue;
    Handle meta(CreateFileW(detail->DevicePath, 0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!meta)
      continue;
    HIDD_ATTRIBUTES a{};
    a.Size = sizeof(a);
    if (!HidD_GetAttributes(meta.v, &a) || a.VendorID != mg::kVendorId ||
        (a.ProductID != mg::kProductId && a.ProductID != 0x5029))
      continue;
    PHIDP_PREPARSED_DATA pp = nullptr;
    if (!HidD_GetPreparsedData(meta.v, &pp))
      continue;
    HIDP_CAPS caps{};
    const NTSTATUS status = HidP_GetCaps(pp, &caps);
    HidD_FreePreparsedData(pp);
    const bool exact = status == HIDP_STATUS_SUCCESS &&
                       (caps.UsagePage == 0xffff || caps.UsagePage == 0xff00) &&
                       caps.Usage == 2 && caps.FeatureReportByteLength == 65;
    if (log)
      DebugLog_Write(
          L"[rongyuan.snapshot.enumerate] path_hash=%016llX version=%04X "
          L"exact=%d",
          static_cast<unsigned long long>(HashPath(detail->DevicePath)),
          a.VersionNumber, exact ? 1 : 0);
    if (a.ProductID==0x5029 && log)
      Record("collection",exact?"eligible":"wrong_collection",0,0,0,
             (unsigned(caps.FeatureReportByteLength)<<16)|caps.Usage);
    if (exact)
      out.push_back({detail->DevicePath, a, caps});
  }
  SetupDiDestroyDeviceInfoList(set);
  return out;
}

class Session {
public:
  const mg::Model *model = nullptr;
  unsigned units = 0;
  bool rejectedIdentity = false;
  bool privatePipe() const {return candidate.privatePipe;}
  unsigned pid() const {return candidate.attributes.ProductID;}
  bool Healthy() const {return !poisoned;}
  explicit Session(const Candidate &c) : candidate(c) {}
  ~Session() {
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
    if (poisoned || g_stop.load()) {
      Record("request",g_stop.load()?"cancelled":"transport_poisoned",request[1],request[4]);
      return false;
    }
    auto tx = request;
    DWORD error=0;
    if (!Feature(handle.v, true, tx,candidate.privatePipe,error)) {
      Record(optional?"metadata_set":"feature_set","io_failed",request[1],request[4],error);
      if(!optional) poisoned=true;
      return false;
    }
    const auto deadline = GetTickCount64() + 50;
    do {
      reply = {};
      if (!Feature(handle.v, false, reply,candidate.privatePipe,error)) {
        Record(optional?"metadata_get":"feature_get","io_failed",request[1],request[4],error);
        if(!optional) poisoned=true;
        return false;
      }
      if (valid(reply))
        return true;
      // Retry only an observed unready shared-buffer reply, not valid samples.
      if (g_wake && !candidate.privatePipe)
        WaitForSingleObject(g_wake, 1);
    } while (!g_stop.load() && GetTickCount64() < deadline);
    Record(optional?"metadata_reply":"protocol_reply",g_stop.load()?"cancelled":"invalid_or_unready",
           request[1],request[4],0,unsigned(reply[0])|(unsigned(reply[1])<<8));
    if (!optional) poisoned = true;
    return false;
  }
  static unsigned slotForEvidence(unsigned page,unsigned i) {return page*32+i;}
  bool Travel(unsigned page, mg::Values &values) {
    mg::Report r{};bool reported=false;
    return Query(
        mg::Request(0xe5, 0xfe, 1, static_cast<std::uint8_t>(page)), r,
        [&](const auto &x) {
          if(model && mg::PhysicalMetadata(model->board)) {
            if(x[0] || !units || page>=4) return false;
            mg::Values next{};
            for(unsigned i=0;i<32;++i) {
              next[i]=std::uint16_t(x[1+2*i]|(x[2+2*i]<<8));
              if(mg::Decode(model->matrix->data()+(page*32+i)*4) && next[i]>6*units) {
                if(!reported) {Record("depth_frame","physical_value_out_of_scale_or_unready",0xe5,page,0,slotForEvidence(page,i));reported=true;}
                return false;
              }
            }
            values=next;return true;
          }
          return mg::ParseTravel(x,units,values);
        });
  }

private:
  Candidate candidate;
  Handle handle;
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
  Record("identity","attempt",0x8f);
  mg::Report r{};
  if (!s.Query(mg::Request(0x8f),r,[](const auto& x){return mg::Board(x)!=0;})) return false;
  s.model=mg::Find(mg::Board(r));
  if(!s.model || s.model->pid!=s.pid()) {
    s.rejectedIdentity=true;
    Record("identity","foreign_board_or_pid",0x8f,0,0,mg::Board(r));return false;
  }
  const unsigned version=r[8]|(unsigned(r[9])<<8);
  if(mg::PhysicalMetadata(s.model->board)) {
    mg::Report repeated{};
    if(!s.Query(mg::Request(0x8f),repeated,[](const auto& x){return mg::Board(x)!=0;}) ||
       mg::Board(repeated)!=s.model->board) {
      s.rejectedIdentity=true;
      Record("identity","inconsistent_board",0x8f,0,0,mg::Board(repeated));return false;
    }
    std::lock_guard<std::mutex> lock(g_evidenceLock);
    const auto revision=g_evidence.revision+1;g_evidence={};g_evidence.revision=revision;
    g_evidence.board=s.model->board;g_evidence.pid=s.pid();g_evidence.usb=version;
  }
  mg::Report features{};
  const bool featureReply=s.Query(mg::Request(0xe6),features,
      [](const auto& x){return x[0]==0 && x[1]==0xe6 && x[2]==0xaa;},true);
  if(!featureReply) features={};
  s.units=mg::Units(version,features);
  Record("precision",featureReply?"feature_reply":"documented_legacy_version",0xe6,0,0,s.units);
  if(!s.units) {s.rejectedIdentity=true;Record("precision","unsupported_units",0xe6,0,0,features[3]);return false;}
  if(mg::PhysicalMetadata(s.model->board)) {
    std::lock_guard<std::mutex> lock(g_evidenceLock);g_evidence.units=s.units;
  }
  for(unsigned page=0;page<4;++page) {
    mg::Values v{};Record("proof_page","attempt",0xe5,page);
    if(!s.Travel(page,v)) return false;
    Record("proof_page","valid_including_zero",0xe5,page);
  }
  Record("proof","ready",0xe5,0,0,s.model->board);return true;
}
bool Map(Session &s) {
  const bool permissive=mg::PhysicalMetadata(s.model->board);
  mg::Report r{};mg::Matrix assigned{};
  bool available=s.Query(mg::Request(0x84,255),r,
      [](const auto& x){return x[0]==0 && x[1]==0x84 && x[2]<8;},permissive);
  if(available) {
    const auto profile=r[2];
    for(unsigned page=0;page<8;++page) {
      if(!s.Query(mg::Request(0x8a,profile,255,static_cast<std::uint8_t>(page)),r,
                  mg::ValidAssignments,permissive)) {available=false;break;}
      std::copy(r.begin()+1,r.end(),assigned.begin()+64*page);
    }
  }
  if(!available && !permissive) return false;
  if(!available) assigned=*s.model->matrix;
  Record("map_policy",available?"assignments_available":"physical_factory_continue",0x8a);
  std::array<halljoy::native_layout::Key,128> keys{};unsigned count=0;
  for(unsigned slot=0;slot<128;++slot) {
    const auto factory=mg::Decode(s.model->matrix->data()+slot*4);
    if(!factory) continue;
    const auto action=mg::Decode(assigned.data()+slot*4);
    const auto id=static_cast<std::uint8_t>(slot+1);
    if(!g_factory.Bind(id,factory) || (action && !g_assigned.Bind(id,action))) {
      Record("publication","invalid_physical_binding",0,0,0,slot);return false;
    }
    keys[count++]={factory,action};
  }
  const auto token=halljoy::layout_identity::Token("rongyuan-snapshot",s.model->product);
  const bool cached=halljoy::native_layout::Publish(token,keys.data(),count);
  if(!cached && !permissive) return false;
  Record("layout",cached?"published":"cache_unavailable_continue",0,0,0,count);
  g_token.store(token);g_board.store(s.model->board);g_units.store(s.units);g_mapped.store(count);
  if(permissive) {g_avgUs=0;g_maxUs=0;}
  if(permissive) {
    std::lock_guard<std::mutex> lock(g_evidenceLock);
    g_evidence.mapped=count;g_evidence.metadata=available?1:0;g_evidence.layout=cached?1:0;
  }
  return true;
}
void Publish(unsigned page, const mg::Values &values, std::uint64_t now) {
  const auto *model = mg::Find(g_board.load());
  if (!model)
    return;
  for (unsigned i = 0; i < 32; ++i) {
    const auto slot = page * 32 + i;
    if (!mg::Decode(model->matrix->data() + slot * 4))
      continue;
    const auto id = static_cast<std::uint8_t>(slot + 1);
    const auto value = mg::Normalize(values[i], g_units.load(), model->rangeUm);
    g_factory.Publish(id, value, now);
    g_assigned.Publish(id, value, now);
  }
  if(mg::PhysicalMetadata(model->board)) ObservePage(page,values,*model);
  g_last.store(now);
  ++g_ok;
}
bool Run(const Candidate &c) {
  Clear();
  if(g_stop.load()) {Record("request","cancelled");return false;}
  if(c.attributes.VendorID!=mg::kVendorId ||
     (c.attributes.ProductID!=mg::kProductId && c.attributes.ProductID!=0x5029) ||
     (c.caps.UsagePage!=0xffff && c.caps.UsagePage!=0xff00) ||
      c.caps.Usage!=2 || c.caps.FeatureReportByteLength!=65) {
    Record("collection","wrong_collection");return false;
  }
  Session s(c);
  const char *phase = "open";
  const bool opened = s.Open();
  const DWORD openError = opened ? ERROR_SUCCESS : GetLastError();
  g_busy.store(!opened && NativeAnalog_IsAccessBusyError(openError));
  Record("open",opened?"success":"failed",0,0,openError);
  if (!opened) {
    // Once per distinct error: the worker retries every second.
    static std::atomic<DWORD> lastError{ERROR_SUCCESS};
    if (lastError.exchange(openError) != openError)
      SupportLog_Event("rongyuan_snapshot.open_failed", 0, SupportLog_Win32(openError));
  }
  const bool ready = opened && (phase = "identity proof", Proof(s)) &&
                     (phase = "assignments", Map(s));
  if (!ready) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.snapshot] admission failed phase=%hs error=%lu",
                   phase, opened ? GetLastError() : openError);
    Clear();
    Record("cleanup","input_cleared_handle_closes");
    return s.rejectedIdentity;
  }
  if (!NativeAnalogRouting_Claim(mg::kVendorId, c.attributes.ProductID, c.path.c_str(),
                                 NativeAnalogProtocol::RongYuanSnapshot) &&
      !NativeAnalogRouting_IsClaimedBy(
          c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot)) {
    Record("routing","ownership_collision");
    Clear();
    return false;
  }
  g_present.store(true);
  g_connected.store(true);
  Record("polling","started",0xe5,0,0,s.model->board);
  DebugLog_Write(L"[rongyuan.snapshot] connected board=%u keys=%u "
                 L"units_per_mm=%u experimental=1",
                 s.model->board, g_mapped.load(), s.units);
  auto logAt = GetTickCount64() + 5000;
  auto last = GetTickCount64();
  unsigned half = 0;
  auto sweepStart=std::chrono::steady_clock::now();
  while (!g_stop.load()) {
    mg::Values values{};
    if (!s.Travel(half, values)) {
      ++g_bad;
      {std::lock_guard<std::mutex> lock(g_evidenceLock);++g_evidence.rejected;}
      Record("depth","read_failed",0xe5,half);break;
    }
    const auto now = GetTickCount64();
    Publish(half, values, now);
    if(mg::PhysicalMetadata(s.model->board)) {
      if(half==3) {
        const auto stamp=std::chrono::steady_clock::now();
        const auto us=static_cast<std::uint32_t>(std::max<std::int64_t>(1,
          std::chrono::duration_cast<std::chrono::microseconds>(stamp-sweepStart).count()));
        const auto prev=g_avgUs.load();g_avgUs.store(prev?(prev*7+us)/8:us);
        g_maxUs.store(std::max(g_maxUs.load(),us));sweepStart=stamp;
      }
    } else {
      const auto us=static_cast<std::uint32_t>(std::min<ULONGLONG>(0xffffffffull,(now-last)*1000));
      const auto prev=g_avgUs.load();g_avgUs.store(prev?(prev*7+us)/8:us);
      g_maxUs.store(std::max(g_maxUs.load(),us));
    }
    last=now;
    if (now >= logAt) {
      DebugLog_Write(
          L"[rongyuan.snapshot] health half_updates=%llu failures=%llu "
          L"avg_us=%u max_us=%u",
          static_cast<unsigned long long>(g_ok.load()),
          static_cast<unsigned long long>(g_bad.load()), g_avgUs.load(),
          g_maxUs.load());
      logAt = now + 5000;
    }
    half = (half + 1) % 4;
    if (g_wake && !mg::PhysicalMetadata(s.model->board))
      WaitForSingleObject(g_wake, 1);
  }
  DebugLog_Write(
      L"[rongyuan.snapshot] session ended stop=%d error=%lu failures=%llu",
      g_stop.load() ? 1 : 0, GetLastError(),
      static_cast<unsigned long long>(g_bad.load()));
  WriteSnapshotEvidence();
  Clear();
  Record("cleanup","input_cleared_handle_closes");
  return false;
}
unsigned __stdcall Worker(void *) {
  try {
    std::vector<std::wstring> declined;
    auto topology=g_topology.load();
    while (!g_stop.load()) {
      if(topology!=g_topology.load()) {topology=g_topology.load();declined.clear();}
      const auto candidates = Enumerate(false);
      g_present.store(!candidates.empty());
      for (const auto &c : candidates) {
        if (g_stop.load())
          break;
        if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
            !NativeAnalogRouting_IsClaimedBy(
                c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot))
          continue;
        if(std::find(declined.begin(),declined.end(),c.path)!=declined.end()) continue;
        // An unrelated identity is not reopened once per second while on the bus.
        if(Run(c)) declined.push_back(c.path);
        if (g_stop.load())
          break;
      }
      if (g_wake && !g_stop.load())
        WaitForSingleObject(g_wake, candidates.empty() ? INFINITE : 1000);
    }
  } catch (...) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.snapshot] worker exception; input cleared");
  }
  Clear();
  g_busy.store(false);
  g_running.store(false);
  return 0;
}
bool Prepare() {
  std::lock_guard<std::mutex> lock(g_service);
  if (g_thread)
    return g_present.load();
  g_stop.store(false);
  SupportLog_SetRongyuanEvidenceProvider(&WriteSnapshotEvidence);
  bool any = false;
  for (const auto &c : Enumerate(true)) {
    if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
        !NativeAnalogRouting_IsClaimedBy(
            c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot))
      continue;
    Session s(c);
    if (!s.Open() || !Proof(s))
      continue;
    any =
        NativeAnalogRouting_Claim(mg::kVendorId, c.attributes.ProductID, c.path.c_str(),
                                  NativeAnalogProtocol::RongYuanSnapshot) ||
        any;
  }
  g_present.store(any);
  return any;
}
bool Start() {
  std::lock_guard<std::mutex> lock(g_service);
  if (g_thread)
    return g_running.load();
  g_stop.store(false);
  SupportLog_SetRongyuanEvidenceProvider(&WriteSnapshotEvidence);
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
  SupportLog_SetRongyuanEvidenceProvider(nullptr);
  return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
  ++g_topology;
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
  out->accessBusy = out->present && !out->connected && g_busy.load();
  out->verifiedLayoutToken = Connected() ? g_token.load() : 0;
  out->vendorId = mg::kVendorId;
  const auto* actualModel=mg::Find(g_board.load());
  out->productId = actualModel?actualModel->pid:mg::kProductId;
  out->usagePage = 0xffff;
  out->usage = 2;
  out->mappedKeys = Connected() ? g_mapped.load() : 0;
  out->activeKeys = g_factory.Active(GetTickCount64());
  const auto *model = mg::Find(g_board.load());
  out->nominalRawLevels =
      model ? model->rangeUm * g_units.load() / 1000 + 1 : 0;
  out->inputReportBytes = 0;
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
               L"%s: USB matrix snapshots; hardware/range validation pending",
               model ? model->name : L"RongYuan");
}
#include "rongyuan_snapshot_self_test.h"
} // namespace
const NativeAnalogBackendDescriptor &
RongYuanSnapshot_GetNativeBackendDescriptor() {
  static const NativeAnalogBackendDescriptor d{
      kNativeAnalogBackendAbiVersion,
      sizeof(NativeAnalogBackendDescriptor),
      "rongyuan-snapshot",
      L"Akko / MonsGeek / EPOMAKER USB (experimental)",
      NativeAnalogProtocol::RongYuanSnapshot,
      NativeAnalogStartPhase::BeforeUap,
      NativeAnalogBackendFlag_PolledTransport |
          NativeAnalogBackendFlag_ReadOnlyProbe,
      &Prepare,
      &Start,
      &Stop,
      &Notify,
      &Present,
      &Connected,
      &Owns,
      &Get,
      &Telemetry, nullptr, +[](halljoy::observation::Sink sink) { (Remapped() ? g_assigned : g_factory).Observations(sink); }};
  return d;
}
bool RongYuanSnapshot_TryRunSelfTest(int& result) noexcept {
  int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
  const bool mod007=argv && argc==2 && wcscmp(argv[1],L"--halljoy-akko3113-self-test")==0;
  const bool selected=mod007 || (argv && argc==2 && wcscmp(argv[1],L"--halljoy-akko3131-self-test")==0);
  if(argv) LocalFree(argv);
  if(!selected) return false;
  bool pass=false;
  try {pass=SnapshotSelfTest(mod007?3113u:3131u);} catch(...) {pass=false;}
  wprintf(L"%ls=%ls scenarios=14 negatives=11 physical_channels=%u proof_to_subscribe=1 first_sample=1 multikey=1 release=1 cleanup=1 metadata_optional=1 read_only=1 hardware_opened=0\n",mod007?L"AKKO3113_SELF_TEST":L"AKKO3131_SELF_TEST",pass?L"PASS":L"FAIL",mod007?80u:87u);
  result=pass?0:10;return true;
}
