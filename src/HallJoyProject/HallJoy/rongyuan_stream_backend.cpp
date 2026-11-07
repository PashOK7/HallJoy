#define WIN32_LEAN_AND_MEAN

#define NOMINMAX
#include "support_log.h"
#include "keyboard_communication_health.h"
#include "support_notice_catalog.h"
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
#include <avrt.h>
#include "rongyuan_stream_backend.h"
#include "debug_log.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "input_trace.h"
#include "native_analog_routing.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
#include "rongyuan_identity_budget.h"
#include "rongyuan_press_timing.h"
#include "rongyuan_stream_reconcile.h"
#include "rongyuan_stream_protocol.h"
#include "rongyuan_receiver_client.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cwctype>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "avrt.lib")
namespace {
namespace mg = halljoy::ry_stream;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = ~ULONGLONG{0}; // Delta values persist until release/disconnect.
std::atomic<bool> g_running{false}, g_stop{false}, g_present{false},
    g_connected{false}, g_mmcss{false};
std::atomic<std::uint64_t> g_token{0}, g_ok{0}, g_bad{0}, g_last{0};
std::atomic<std::uint32_t> g_avgUs{0}, g_maxUs{0};
std::atomic<unsigned> g_board{0}, g_units{0}, g_mapped{0}, g_vid{0}, g_pid{0};
std::atomic<bool> g_receiver{false};
halljoy::ry_receiver::Liveness g_rfLiveness;
std::atomic<unsigned> g_topology{0}; // device changes; resets identity retries
std::mutex g_service, g_activeLock;
HANDLE g_thread = nullptr, g_wake = nullptr, g_active = INVALID_HANDLE_VALUE;
halljoy::physical_analog::Publication g_factory, g_assigned;
std::array<std::uint16_t, 128> g_slotFactory{}, g_slotAssigned{}; // worker thread only

// Press timing of keys bound to the gamepad (rongyuan_press_timing.h). Windows key events
// of this keyboard go from the UI thread to the reader through a single-producer ring;
// every finished press is one evidence line, slow presses are also kept whole.
struct OsKey { std::uint16_t hid; bool down; LONGLONG us; unsigned uiMs; };
std::array<OsKey, 128> g_osKeys{};
std::atomic<unsigned> g_osHead{0}, g_osTail{0};
HANDLE OsKeyEvent() {
  static const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  return event;
}
constexpr std::int64_t kSlowUs = 30000; // either edge slower than this: kept as a slow press
constexpr unsigned kBacklogSlow = 16, kSlowLines = 256, kReaderLines = 16;
halljoy::ry_stream::PressTiming g_timing; // worker thread only
struct PressStats {
  unsigned presses = 0, windowsOnly = 0, slow = 0, slowLines = 0, nextSummary = 1;
  unsigned maxBacklog = 0, maxUiMs = 0;
  std::int64_t maxDownWaitUs = 0, maxUpWaitUs = 0;
} g_pressStats; // worker thread only, whole run
LONGLONG NowUs() {
  static const LONGLONG freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
  LARGE_INTEGER t; QueryPerformanceCounter(&t);
  return static_cast<LONGLONG>(static_cast<long double>(t.QuadPart) * 1000000.0L / static_cast<long double>(freq));
}
double Ms(std::int64_t us) { return static_cast<double>(us) / 1000.0; }
void WritePress(const halljoy::ry_stream::PressRecord &r) {
  auto &s = g_pressStats;
  ++s.presses;
  if (r.os && !r.stream) ++s.windowsOnly;
  s.maxDownWaitUs = std::max(s.maxDownWaitUs, r.downWaitUs);
  s.maxUpWaitUs = std::max(s.maxUpWaitUs, r.upWaitUs);
  s.maxBacklog = std::max(s.maxBacklog, r.backlog);
  s.maxUiMs = std::max(s.maxUiMs, r.uiDelayMs);
  const bool slow = (r.os && !r.stream) || r.downWaitUs >= kSlowUs || r.upWaitUs >= kSlowUs ||
                    r.backlog >= kBacklogSlow;
  if (slow) ++s.slow;
  char line[352]{};
  int at = _snprintf_s(line, _TRUNCATE,
      "evidence.rongyuan press hid=%02x os=%d stream=%d lead_ms=%.1f down_wait_ms=%.1f up_wait_ms=%.1f "
      "samples=%u max_gap_ms=%.1f peak=%u dur_ms=%.1f backlog=%u ui_ms=%u slow=%d",
      r.hid, r.os ? 1 : 0, r.stream ? 1 : 0, Ms(r.leadUs), Ms(r.downWaitUs), Ms(r.upWaitUs), r.samples,
      Ms(r.maxGapUs), r.peak, Ms(r.durationUs), r.backlog, r.uiDelayMs, slow ? 1 : 0);
  if (at > 0 && r.afterReleaseUs >= 0)
    _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, " after_release_ms=%.1f", Ms(r.afterReleaseUs));
  SupportLog_Evidence(line, slow && s.slowLines++ < kSlowLines);
  if (s.presses != s.nextSummary) return;
  s.nextSummary *= 2;
  _snprintf_s(line, _TRUNCATE,
      "evidence.rongyuan press_summary presses=%u slow=%u windows_only=%u max_down_wait_ms=%.1f "
      "max_up_wait_ms=%.1f max_backlog=%u max_ui_ms=%u",
      s.presses, s.slow, s.windowsOnly, Ms(s.maxDownWaitUs), Ms(s.maxUpWaitUs), s.maxBacklog, s.maxUiMs);
  SupportLog_Evidence(line, true);
}
// Stream/table reconciliation (rongyuan_stream_reconcile.h). The reconciler thread reads the
// keyboard's travel table (E5 FE) through the control collection; the reader thread applies
// the corrections, so publication and press timing stay on one thread.
halljoy::ry_stream::Reconcile g_reconcile;
std::mutex g_correctionLock;
std::vector<halljoy::ry_stream::Reconcile::Correction> g_corrections;
HANDLE ReconcileEvent() {  // reader -> reconciler: a key got a non-zero sample
  static const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  return event;
}
HANDLE CorrectionEvent() {  // reconciler -> reader: corrections are queued
  static const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  return event;
}
constexpr unsigned kReconcileKeptLines = 64;
unsigned g_reconcileLines = 0;  // reader thread only, whole run

void DrainOsKeys() {
  unsigned tail = g_osTail.load(std::memory_order_acquire);
  const unsigned head = g_osHead.load(std::memory_order_acquire);
  for (; tail != head; ++tail) {
    const OsKey key = g_osKeys[tail % g_osKeys.size()];
    halljoy::ry_stream::PressRecord record{};
    if (g_timing.OsKey(key.hid, key.down, key.us, key.uiMs, record)) WritePress(record);
  }
  g_osTail.store(tail, std::memory_order_release);
}
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
  bool receiver=false;
};

bool Feature(HANDLE h, bool write, mg::Report &data) {
  HidIoOperation op(h);
  DWORD error = 0, done = 0;
  const auto start = op.StartControl(
      write ? IOCTL_HID_SET_FEATURE : IOCTL_HID_GET_FEATURE, data.data(), 65,
      write ? nullptr : data.data(), write ? 0 : 65, &error);
  if (start == HidIoOperation::StartResult::Failed) {
    SetLastError(error); return false;
  }
  if (start == HidIoOperation::StartResult::Pending &&
      op.Wait(50) != WAIT_OBJECT_0) {
    op.CancelAndDrain(&done, &error);
    SetLastError(ERROR_TIMEOUT); return false;
  }
  if (!op.Finish(&done, &error, false)) {SetLastError(error);return false;}
  const bool valid=write || (done == 64 && data[0] == 0);
  SetLastError(valid ? ERROR_SUCCESS : ERROR_INVALID_DATA);
  return valid;
}

// `visible` counts matching HID interfaces whose metadata could be read: a keyboard that
// is visible but not paired (a collection busy in another program) must be retried.
std::vector<Candidate> Enumerate(bool log, unsigned* visible = nullptr) {
  GUID guid{};
  HidD_GetHidGuid(&guid);
  HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (set == INVALID_HANDLE_VALUE)
    return {};
  std::vector<Candidate> out;
  // Support diagnostics: a matching keyboard that never pairs is otherwise silent.
  unsigned matching = 0, exactCount = 0, inputCount = 0;
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
    if (!HidD_GetAttributes(meta.v, &a))continue;
    const bool receiver=a.VendorID==0x3151 && a.ProductID==0x5038;
    if (!receiver && !mg::Candidate(a.VendorID,a.ProductID))continue;
    ++matching;
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
    exactCount += exact ? 1u : 0u;
    inputCount += input ? 1u : 0u;
    if (log)
      DebugLog_Write(
          L"[rongyuan.stream.enumerate] path_hash=%016llX version=%04X "
          L"exact=%d",
          static_cast<unsigned long long>(HashPath(detail->DevicePath)),
          a.VersionNumber, exact ? 1 : 0);
    if(receiver) {
      std::wstring path=detail->DevicePath;
      std::transform(path.begin(),path.end(),path.begin(),[](wchar_t ch){return static_cast<wchar_t>(towlower(ch));});
      const unsigned interfaceNumber=path.find(L"mi_02")!=std::wstring::npos?2u:~0u;
      const bool admitted=status==HIDP_STATUS_SUCCESS && halljoy::ry_receiver::Identity(
          a.VendorID,a.ProductID,caps.UsagePage,caps.Usage,caps.FeatureReportByteLength,interfaceNumber);
      static std::atomic<bool> logged{false};
      if(admitted && !logged.exchange(true))SupportLog_Evidence(
          "evidence.rongyuan.rf discovery revision=2 vid=3151 pid=5038 interface=2 feature=65 remote_identity_required=1",true);
      GUID container{};DEVPROPTYPE type=0;DWORD bytesNeeded=0;
      const bool grouped=SetupDiGetDevicePropertyW(set,&dev,&DEVPKEY_Device_ContainerId,&type,
          reinterpret_cast<PBYTE>(&container),sizeof(container),&bytesNeeded,0) && type==DEVPROP_TYPE_GUID;
      if(admitted || (input && grouped))out.push_back({detail->DevicePath,L"",grouped?container:GUID{},admitted,input,a,caps,true});
      continue;
    }
    GUID container{}; DEVPROPTYPE type=0;DWORD required=0;
    if((exact || input) && SetupDiGetDevicePropertyW(set,&dev,&DEVPKEY_Device_ContainerId,&type,reinterpret_cast<PBYTE>(&container),sizeof(container),&required,0) && type==DEVPROP_TYPE_GUID && !IsEqualGUID(container,GUID{}))
      out.push_back({detail->DevicePath,L"",container,exact,input,a,caps});
  }
  SetupDiDestroyDeviceInfoList(set);
  std::vector<Candidate> paired;
  for(auto c:out)if(c.control && !c.receiver) {
    unsigned matches=0;
    for(const auto& i:out)if(i.input && i.attributes.VendorID==c.attributes.VendorID && i.attributes.ProductID==c.attributes.ProductID && IsEqualGUID(i.container,c.container)) {c.inputPath=i.path;++matches;}
    if(matches==1)paired.push_back(c);
  }
  // Prefer USB whenever both the keyboard and its idle receiver are present.
  for(auto c:out)if(c.receiver && c.control) {
    unsigned matches=0;
    if(!IsEqualGUID(c.container,GUID{}))for(const auto& input:out)
      if(input.receiver && input.input && IsEqualGUID(input.container,c.container)) {c.inputPath=input.path;++matches;}
    if(matches!=1)c.inputPath.clear();
    static unsigned lastMatches=~0u;
    if(matches!=lastMatches){lastMatches=matches;char evidence[128]{};
      _snprintf_s(evidence,_TRUNCATE,"evidence.rongyuan.rf input revision=2 exact_stream_pairs=%u",matches);
      SupportLog_Evidence(evidence,true);
    }
    paired.push_back(c);
  }
  // Logged only when the counts change; the worker repeats this scan every second.
  if(matching) {
    static std::atomic<std::uint64_t> lastSignature{~0ull};
    const std::uint64_t signature=(std::uint64_t(matching)<<48)|(std::uint64_t(exactCount)<<32)|(std::uint64_t(inputCount)<<16)|std::uint64_t(paired.size());
    if(lastSignature.exchange(signature)!=signature) {
      SupportLog_Event("rongyuan.enumerate",matching,SupportLog_Data(exactCount));
      SupportLog_Event("rongyuan.enumerate_pairing",inputCount,SupportLog_Data(static_cast<unsigned>(paired.size())));
    }
  }
  if (visible) *visible = matching;
  return paired;
}

struct ReceiverIO {
  HANDLE& handle;
  bool Write(halljoy::ry_receiver::Report r){return Feature(handle,true,r);}
  bool Read(halljoy::ry_receiver::Report& r){return Feature(handle,false,r);}
  bool cleanup=false;
  bool Check(){if(!cleanup && g_stop.load()){SetLastError(ERROR_OPERATION_ABORTED);return false;}return true;}
  std::uint64_t ClockUs(){return static_cast<std::uint64_t>(NowUs());}
  void YieldBusy(){SwitchToThread();}
  void Status(bool online){g_rfLiveness.Status(GetTickCount64(),online);}
};
class Session {
public:
  const mg::Model *model = nullptr;
  unsigned board = 0; // reported by the keyboard; an alias may use another board's profile
  unsigned units = 0;
  explicit Session(const Candidate &c) : candidate(c) {}
  ~Session() {
    DisableStream();
    std::lock_guard<std::mutex> lock(g_activeLock);
    if (g_active == handle.v)
      g_active = INVALID_HANDLE_VALUE;
  }
  bool Open() {
    handle = Handle(CreateFileW(
        candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    // Games (Unity opens every HID interface) and vendor tools often keep the
    // control collection open. Exclusive ownership is preferred, but a shared
    // handle (HIDAPI convention) still works: every reply is validated by command
    // and the identity proof, and the stream arrives on its own input collection.
    const DWORD exclusiveError = handle ? ERROR_SUCCESS : GetLastError();
    // A radio channel must not be interleaved with another configurator's F6/FC.
    if (!candidate.receiver && (exclusiveError==ERROR_SHARING_VIOLATION || exclusiveError==ERROR_ACCESS_DENIED)) {
      // Once per device: the worker retries every second while a session fails.
      static std::atomic<unsigned> loggedPid{0};
      if (loggedPid.exchange(candidate.attributes.ProductID)!=candidate.attributes.ProductID)
        SupportLog_Event("rongyuan.shared_open",candidate.attributes.ProductID,SupportLog_Win32(exclusiveError));
      handle = Handle(CreateFileW(
          candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    }
    if (!handle) {
      const DWORD error=GetLastError();
      SupportLog_Event("rongyuan.exclusive_open_failed",candidate.attributes.ProductID,SupportLog_Win32(error));
      halljoy::keyboard_support::ReportCommunicationAccessFailure(
          static_cast<unsigned>(NativeAnalogProtocol::RongYuanStream),error);
      SetLastError(error);
      return false;
    }
    std::lock_guard<std::mutex> lock(g_activeLock);
    g_active = handle.v;
    return true;
  }
  template <class Validate>
  bool Query(const mg::Report &request, mg::Report &reply, Validate valid,
             bool optional = false) {
    if (poisoned || g_stop.load())
      return false;
    if(candidate.receiver) {
      lastCommand=request[1];SetLastError(ERROR_SUCCESS);
      const bool ok=radio.Query(request,reply,valid);
      if(!ok && GetLastError()==ERROR_SUCCESS &&
         radio.stage!=halljoy::ry_receiver::Stage::Offline && radio.stage!=halljoy::ry_receiver::Stage::Bootloader)
        SetLastError(ERROR_INVALID_DATA);
      return ok;
    }
    auto tx = request;
    if (!Send(tx))
      return poisoned = true, false;
    const auto deadline = GetTickCount64() + 100;
    do {
      // Manufacturer driver timing (commonMsg -> readMsg): 10 ms before each
      // GET_FEATURE. RY5088 firmware stalls the control pipe (USB timeout,
      // ERROR_SEM_TIMEOUT) when a reply is read while it is still building it.
      Sleep(kExchangeGapMs);
      reply = {};
      if (!Feature(handle.v, false, reply))
        return poisoned = true, false;
      if (valid(reply))
        return true;
    } while (!g_stop.load() && GetTickCount64() < deadline);
    if (!optional)
      poisoned = true;
    SetLastError(g_stop.load() ? ERROR_OPERATION_ABORTED : ERROR_INVALID_DATA);
    return false;
  }
  bool OpenInput() {
    input=Handle(CreateFileW(candidate.inputPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,nullptr));
    return input && HidD_SetNumInputBuffers(input.v,128) && HidD_FlushQueue(input.v);
  }
  bool Enable() {
    auto command=mg::Request(0x1b,1);
    enabled=true; // Also restore if a timed-out write reached the device.
    return Send(command);
  }
  void DisableStream(){
    if(!enabled)return;
    const bool cleanup=radioIo.cleanup;const unsigned timeout=radio.timeoutUs;
    radioIo.cleanup=true;radio.timeoutUs=100000;auto disable=mg::Request(0x1b,0);
    const bool restored=Send(disable);if(restored)enabled=false;
    radioIo.cleanup=cleanup;radio.timeoutUs=timeout;
    if(candidate.receiver)SupportLog_Evidence(restored?"evidence.rongyuan.rf stream_cleanup sent=1":"evidence.rongyuan.rf stream_cleanup sent=0",true);
  }
  HANDLE Input() const {return input.v;}
  unsigned VendorId() const {return candidate.attributes.VendorID;}
  unsigned ProductId() const {return candidate.attributes.ProductID;}
  bool Match(const mg::Report& r) {
    const auto answered=mg::Board(r);
    const bool match=(candidate.receiver?halljoy::ry_receiver::Model(answered):
        mg::Find(answered,candidate.attributes.VendorID,candidate.attributes.ProductID))!=nullptr;
    if(answered && !match) identityRejected=true;
    if(answered && !match && answered!=lastRejectedBoard){lastRejectedBoard=answered;
      SupportLog_Event("rongyuan.identity_pair_rejected",answered,SupportLog_Data((unsigned(candidate.attributes.VendorID)<<16)|candidate.attributes.ProductID));
    }
    return match;
  }
  // The keyboard answered with a board this table does not admit on its VID/PID.
  bool IdentityRejected() const {return identityRejected;}
  // A control transfer failed; every later query of this session fails too.
  bool Poisoned() const {return poisoned;}
  // Command of the last feature request sent (support log on admission failure).
  unsigned LastCommand() const {return lastCommand;}
  const mg::Model* Model(const mg::Report& r) {return candidate.receiver?halljoy::ry_receiver::Model(mg::Board(r)):
      mg::Find(mg::Board(r),candidate.attributes.VendorID,candidate.attributes.ProductID);}
  bool Receiver() const{return candidate.receiver;}
  auto& Radio(){return radio;}

private:
  // Manufacturer driver timing (commonMsg -> sendMsg): 10 ms before each SET_FEATURE.
  static constexpr DWORD kExchangeGapMs = 10;
  bool Send(mg::Report &request) {
    if(candidate.receiver)return radio.SendKeyboard(request);
    Sleep(kExchangeGapMs);
    lastCommand = request[1];
    return Feature(handle.v, true, request);
  }
  Candidate candidate;
  unsigned lastRejectedBoard=0;
  unsigned lastCommand=0;
  bool identityRejected=false;
  Handle handle,input;
  ReceiverIO radioIo{handle.v};
  halljoy::ry_receiver::Client<ReceiverIO> radio{radioIo};
  bool enabled=false;
  bool poisoned = false;
};
void Clear() {
  g_connected.store(false);
  g_receiver.store(false);g_rfLiveness.Reset();g_avgUs.store(0);g_maxUs.store(0);
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
  s.board = mg::Board(r);
  const unsigned usbVersion = r[8] | (r[9] << 8);
  unsigned version=usbVersion,radioVersion=0;
  mg::Report rf{};
  // Manufacturer scaling uses the radio/controller firmware when present,
  // even on wired USB. Do not derive travel units from USB bcdDevice.
  if(s.Query(mg::Request(0x80),rf,[](const auto& x){return x[1]==0x80;},true)) {
    const unsigned radio=rf[2] | (rf[3]<<8);
    if(radio && radio!=65535)version=radioVersion=radio;
  }
  mg::Report features{};
  // Old firmware has no E6 reply marker: use its documented version scale.
  s.Query(
      mg::Request(0xe6), features,
      [](const auto &x) { return x[1] == 0xe6 && x[2] == 0xaa; }, true);
  s.units = mg::ModelUnits(*s.model, version, features);
  char evidence[240]{};
  _snprintf_s(evidence,_TRUNCATE,
      "evidence.rongyuan identity revision=4 board=%u vid=%04x pid=%04x firmware=%04x usb=%04x rf=%04x feature_cmd=%02x marker=%02x precision=%u units_per_mm=%u range_um=%u",
      s.board,s.VendorId(),s.ProductId(),version,usbVersion,radioVersion,features[1],features[2],features[3],s.units,s.model->rangeUm);
  SupportLog_Evidence(evidence,true);
  if (!s.units) {SetLastError(ERROR_INVALID_DATA);return false;}
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
  g_slotFactory = {};
  g_slotAssigned = {};
  for (unsigned slot = 0; slot < 128; ++slot) {
    const auto factory = mg::Decode(s.model->matrix.data() + slot * 4);
    if (!factory)
      continue;
    const auto action = mg::Decode(assigned.data() + slot * 4);
    const auto id = static_cast<std::uint8_t>(slot + 1);
    if (!g_factory.Bind(id, factory) ||
        (action && !g_assigned.Bind(id, action)))
      return false;
    g_slotFactory[slot] = factory;
    g_slotAssigned[slot] = action;
    keys[count++] = {factory, action};
  }
  const auto token =
      halljoy::layout_identity::Token("rongyuan-stream", s.model->product);
  if (token && !halljoy::native_layout::Publish(token, keys.data(), count))
    return false;
  g_token.store(token);
  g_board.store(s.board); // Telemetry resolves the model with the device's own identity.
  g_vid.store(s.VendorId());g_pid.store(s.ProductId());
  g_units.store(s.units);
  g_mapped.store(count);
  return true;
}
// `hid` receives the key the sample belongs to (0 for non-key slots), `milli` its depth.
bool Publish(const mg::Sample& sample, const mg::Model& model, std::uint64_t now, std::uint16_t& hid, unsigned& milli, bool stream=true, bool retainWhileOnline=false) {
  hid=0;milli=0;
  if(sample.raw>6*g_units.load())return false;
  if(!mg::Decode(model.matrix.data()+sample.slot*4))return true; // Known non-key/encoder slots never become analog keys.
  const auto id=static_cast<std::uint8_t>(sample.slot+1);
  const auto value=mg::Normalize(sample.raw,g_units.load(),model.rangeUm);
  g_factory.Publish(id,value,now,retainWhileOnline);
  g_assigned.Publish(id,value,now,retainWhileOnline);
  if(stream) {
    g_reconcile.Stream(sample.slot,static_cast<std::uint16_t>(sample.raw));
    if(sample.raw && ReconcileEvent())SetEvent(ReconcileEvent());
  }
  hid=(Remapped()?g_slotAssigned:g_slotFactory)[sample.slot];
  milli=std::min<unsigned>(value,1000);
  g_last.store(now);++g_ok;return true;
}
struct ReconcilerContext { Session* session; unsigned units; unsigned board; HANDLE stop; };
// Reads the table pages that hold a non-zero, unconfirmed key (E5 FE, one page = 32 slots),
// paced only by the vendor's command exchange; sleeps while every key is idle or confirmed.
// A board that does not answer E5 FE keeps the plain stream for the rest of the session.
unsigned __stdcall Reconciler(void* arg) {
  auto& ctx=*static_cast<ReconcilerContext*>(arg);
  const HANDLE waits[]={ctx.stop,ReconcileEvent()};
  constexpr unsigned kNeverAnswered=3;
  unsigned reads=0,failures=0,failuresInRow=0,failureLines=0;
  while(!g_stop.load() && WaitForSingleObject(ctx.stop,0)!=WAIT_OBJECT_0) {
    const int page=g_reconcile.NextPage();
    if(page<0) {
      if(!waits[1] || WaitForMultipleObjects(2,waits,FALSE,INFINITE)==WAIT_OBJECT_0)break;
      continue;
    }
    const auto seqs=g_reconcile.Before(static_cast<unsigned>(page));
    mg::Report reply{};halljoy::rongyuan::Values table{};
    if(!ctx.session->Query(mg::Request(0xe5,0xfe,1,static_cast<std::uint8_t>(page)),reply,
        [&](const auto& x){return halljoy::rongyuan::ParseTravel(x,ctx.units,table);},true)) {
      // Tester log 2026-10-21: after 29 s of good reads one reply was not ready within the
      // exchange window (error 13). That is the firmware being busy, not a missing command:
      // the keys stay unconfirmed and the next pass reads the page again. Only a board that
      // never answered gives up (three failures before any good read).
      const DWORD error=GetLastError();
      ++failures;++failuresInRow;
      char line[192]{};
      if(failureLines<16 || (!reads && failures>=kNeverAnswered)) {
        ++failureLines;
        _snprintf_s(line,_TRUNCATE,"evidence.rongyuan reconcile_failed board=%u page=%d error=%lu reads=%u failures=%u",
                    ctx.board,page,error,reads,failures);
        SupportLog_Evidence(line,true);
      }
      if(!reads && failures>=kNeverAnswered) {
        _snprintf_s(line,_TRUNCATE,"evidence.rongyuan reconcile_unavailable board=%u page=%d error=%lu",ctx.board,page,error);
        SupportLog_Evidence(line,true);
        break;
      }
      if(ctx.session->Poisoned())break;  // the control pipe failed: the session is ending anyway
      continue;
    }
    ++reads;
    if(failuresInRow) {
      char line[160]{};
      _snprintf_s(line,_TRUNCATE,"evidence.rongyuan reconcile_resumed board=%u page=%d after_failures=%u reads=%u",
                  ctx.board,page,failuresInRow,reads);
      if(failureLines<16)SupportLog_Evidence(line,true);
      failuresInRow=0;
    }
    std::vector<halljoy::ry_stream::Reconcile::Correction> found;
    g_reconcile.Table(static_cast<unsigned>(page),seqs,table.data(),found);
    if(found.empty())continue;
    {
      std::lock_guard<std::mutex> lock(g_correctionLock);
      g_corrections.insert(g_corrections.end(),found.begin(),found.end());
    }
    if(CorrectionEvent())SetEvent(CorrectionEvent());
  }
  return 0;
}
// Reader thread: applies queued table values whose key got no newer stream sample.
void ApplyCorrections(const mg::Model& model) {
  std::vector<halljoy::ry_stream::Reconcile::Correction> pending;
  {
    std::lock_guard<std::mutex> lock(g_correctionLock);
    pending.swap(g_corrections);
  }
  for(const auto& c:pending) {
    if(!g_reconcile.Accept(c))continue;
    const LONGLONG sampleUs=NowUs();
    mg::Sample sample{};sample.slot=c.slot;sample.raw=c.raw;
    std::uint16_t hid=0;unsigned milli=0;
    if(!Publish(sample,model,GetTickCount64(),hid,milli))continue;
    if(hid) {
      DrainOsKeys();
      halljoy::ry_stream::PressRecord record{};
      if((!milli || InputTrace_IsBound(hid)) && g_timing.Sample(hid,milli,sampleUs,0,record))WritePress(record);
    }
    // Every applied correction is evidence of a report the keyboard did not deliver.
    char line[160]{};
    _snprintf_s(line,_TRUNCATE,"evidence.rongyuan reconcile slot=%u hid=%02x stream=%u table=%u",
                c.slot,hid,c.stream,c.raw);
    SupportLog_Evidence(line,g_reconcileLines++<kReconcileKeptLines);
  }
}

void ReceiverEvidence(Session& s,unsigned phase,DWORD error) {
  static unsigned attempts=0,lastPhase=~0u,lastStage=~0u;
  const unsigned attempt=++attempts,stage=static_cast<unsigned>(s.Radio().stage);
  if(phase==lastPhase && stage==lastStage && (attempt&(attempt-1)) && attempt%30)return;
  lastPhase=phase;lastStage=stage;
  char line[320]{};
  _snprintf_s(line,_TRUNCATE,
      "evidence.rongyuan.rf attempt=%u phase=%u stage=%u reason=%s command=%02x error=%lu board=%u units=%u online=%u send=%u read=%u boot=%u exchanges=%u polls=%u page=%u snapshot_reason=%u devices=%u keyboard_state=%u",
      attempt,phase,stage,halljoy::ry_receiver::StageName(s.Radio().stage),s.Radio().command,error,s.board,s.units,s.Radio().lastStatus.online?1:0,
      s.Radio().lastStatus.send?1:0,s.Radio().lastStatus.read?1:0,s.Radio().lastStatus.boot?1:0,
      s.Radio().exchanges,s.Radio().statusReads,s.Radio().page,s.Radio().snapshotProblem,
      s.Radio().lastStatus.devices,s.Radio().lastStatus.keyboardState);
  SupportLog_Evidence(line,true);
}

// A pending interrupt read is consumed on its own thread. Feature/table traffic
// never sits between a received depth packet and publication to the gamepad.
class ReceiverStreamPump {
 public:
  std::mutex publication;
  halljoy::ry_receiver::StreamOrder order;
  std::atomic<unsigned> packets{0},malformed{0},ioError{0},maxGapUs{0};
  explicit ReceiverStreamPump(Session& session):s(session){}
  ~ReceiverStreamPump(){Stop();}
  bool Start(){
    stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stop || !s.OpenInput())return false;
    thread=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Entry,this,0,nullptr));
    if(!thread)return false;
    // Session remembers even an ambiguous enable so its destructor disables it.
    return s.Enable();
  }
  void Stop(){
    if(stop)SetEvent(stop);
    if(thread){CancelIoEx(s.Input(),nullptr);WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}
    if(stop){CloseHandle(stop);stop=nullptr;}
  }
 private:
  Session& s;
  HANDLE stop=nullptr,thread=nullptr;
  static unsigned __stdcall Entry(void* arg){return static_cast<ReceiverStreamPump*>(arg)->Read();}
  unsigned Read(){
    DWORD task=0;const HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Games",&task);
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
    std::array<unsigned char,32> data{};HidIoOperation read(s.Input());
    LONGLONG previous=0;ULONGLONG nextSummary=0;
    while(!g_stop.load() && WaitForSingleObject(stop,0)!=WAIT_OBJECT_0){
      DWORD error=0,done=0;
      const auto started=read.StartRead(data.data(),static_cast<DWORD>(data.size()),&error);
      if(started==HidIoOperation::StartResult::Failed){ioError.store(error);break;}
      if(started==HidIoOperation::StartResult::Pending){
        const HANDLE waits[]={read.Event(),stop};
        if(WaitForMultipleObjects(2,waits,FALSE,INFINITE)!=WAIT_OBJECT_0){read.CancelAndDrain(&done,&error);break;}
      }
      if(!read.Finish(&done,&error,false)){if(!g_stop.load())ioError.store(error);break;}
      if(done==32 && data[0]==5 && data[1]!=0x1b)continue;
      mg::Sample sample{};
      if(!mg::Parse(data.data(),done,sample) || sample.raw>6*s.units){++malformed;continue;}
      const auto now=NowUs();unsigned received=0;
      {std::lock_guard<std::mutex> lock(publication);
        std::uint16_t hid=0;unsigned milli=0;
        const auto tick=GetTickCount64();
        if(g_slotFactory[sample.slot] && Publish(sample,*s.model,tick,hid,milli,false,true)){
          order.Event(sample.slot,sample.raw);
          g_rfLiveness.Status(tick,true);
          received=++packets;
          if(received==1)g_rfLiveness.ConfirmStream();
          g_connected.store(true);
          if(previous)maxGapUs.store(std::max(maxGapUs.load(),static_cast<unsigned>(std::min<LONGLONG>(0xffffffffll,now-previous))));
          previous=now;
        }
      }
      const auto tick=GetTickCount64();
      if(received && (received==1 || received==8 || received==64 || tick>=nextSummary)){
        char line[192]{};
        _snprintf_s(line,_TRUNCATE,"evidence.rongyuan.rf stream revision=3 packets=%u malformed=%u io_error=%u max_gap_us=%u",
                    received,malformed.load(),ioError.load(),maxGapUs.load());
        SupportLog_Evidence(line,true);
        SupportLog_Event("rongyuan.rf_stream",received,SupportLog_Data(ioError.load()));
        nextSummary=tick+30000;
      }
    }
    if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
    return 0;
  }
};

bool RunReceiver(const Candidate& c,bool& identityRejected) {
  Clear();Session s(c);unsigned phase=1;
  const bool ready=s.Open() && (phase=2,Proof(s)) && (phase=3,Map(s));
  if(!ready) {
    ReceiverEvidence(s,phase,GetLastError());identityRejected=s.IdentityRejected();Clear();return false;
  }
  if(!NativeAnalogRouting_Claim(c.attributes.VendorID,c.attributes.ProductID,c.path.c_str(),NativeAnalogProtocol::RongYuanStream) &&
     !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),NativeAnalogProtocol::RongYuanStream)){Clear();return false;}
  g_receiver.store(true);g_avgUs.store(0);g_maxUs.store(0);
  const unsigned topology=g_topology.load();
  ReceiverStreamPump stream(s);
  const bool streamEligible=!c.inputPath.empty();
  bool streamActive=false,streamAttempted=false;
  // Seed all keys from genuine depth BEFORE entering delta mode. Otherwise
  // streaming can occupy the command channel needed for initial table admission.
  SupportLog_Evidence("evidence.rongyuan.rf bootstrap revision=5 initial_depth_before_stream=1 fixed_delay_us=0",true);
  unsigned pages=0,sweeps=0,peak=0,releases=0,everPositive=0,varying=0,transients=0;
  unsigned page=0,tableAdjustments=0,zeroFixes=0,superseded=0,zeroReports=0,reportedZeros=0;
  ULONGLONG nextSummary=0,nextZeroReport=0;
  std::uint64_t sweepStart=static_cast<std::uint64_t>(NowUs()),sumSweepUs=0;
  std::array<std::uint16_t,128> previous{};
  std::array<bool,128> positive{},moved{};
  unsigned initialMappedMin=0xffff,initialNonkeyPositive=0,initialRepeatedPages=0;
  while(!g_stop.load() && topology==g_topology.load()) {
    if(streamActive && (stream.ioError.load() || stream.malformed.load()>=16)){
      stream.Stop();g_rfLiveness.StopStream();s.DisableStream();streamActive=false;
      ++g_bad;halljoy::keyboard_support::ReportCommunicationAnomaly(static_cast<unsigned>(NativeAnalogProtocol::RongYuanStream));
      char reason[144]{};_snprintf_s(reason,_TRUNCATE,"evidence.rongyuan.rf stream_fallback io_error=%u malformed=%u",stream.ioError.load(),stream.malformed.load());
      SupportLog_Evidence(reason,true);
    }
    halljoy::ry_receiver::StreamOrder::Ticket ticket{};
    bool retainTable=false;
    halljoy::rongyuan::Values values{};
    SetLastError(ERROR_SUCCESS);
    if(!s.Radio().Page(page,s.units,values,[&]{
      std::lock_guard<std::mutex> lock(stream.publication);
      ticket=stream.order.Before(page);
      retainTable=g_rfLiveness.Streaming();
    })) {
      ReceiverEvidence(s,4,GetLastError());
      if(halljoy::ry_receiver::Client<ReceiverIO>::Transient(s.Radio().stage)) {
        // Ready=0 is normal flow control, not a failed analog packet. Malformed
        // status is still an error; either case retains the bounded live session.
        if(!halljoy::ry_receiver::Client<ReceiverIO>::Busy(s.Radio().stage))++g_bad;
        ++transients;continue;
      }
      ++g_bad;break;
    }
    if(g_stop.load() || topology!=g_topology.load())break;
    const auto now=GetTickCount64();
    if(pages<4){
      for(unsigned earlier=0;earlier<page;++earlier){
        if(std::equal(values.begin(),values.end(),previous.begin()+32*earlier)){
          ++initialRepeatedPages;
          break;
        }
      }
      std::copy(values.begin(),values.end(),previous.begin()+32*page);
      for(unsigned i=0;i<32;++i){
        if(g_slotFactory[page*32+i])initialMappedMin=std::min<unsigned>(initialMappedMin,values[i]);
        else if(values[i])++initialNonkeyPositive;
      }
    }
    {std::lock_guard<std::mutex> lock(stream.publication);
      for(unsigned i=0;i<32;++i) {
        const unsigned slot=page*32+i,value=values[i];
        if(!mg::Decode(s.model->matrix.data()+4*slot))continue;
        if(!stream.order.Accept(page,i,ticket)){++superseded;continue;}
        const unsigned published=stream.order.Current(slot);
        if(stream.packets.load() && published!=value){++tableAdjustments;if(published && !value)++zeroFixes;}
        peak=std::max(peak,value);
        if(value && !positive[slot]){positive[slot]=true;++everPositive;}
        if(pages>=4 && value!=previous[slot] && !moved[slot]){moved[slot]=true;++varying;}
        if(published && !value)++releases;
        std::uint16_t hid=0;unsigned milli=0;
        Publish({slot,value},*s.model,now,hid,milli,false,retainTable);
        stream.order.Table(slot,value);
        previous[slot]=static_cast<std::uint16_t>(value);
      }
      ++pages;
      if(pages>=4){g_last.store(now);g_connected.store(true);}
    }
    if(zeroFixes!=reportedZeros && (zeroReports<8 || now>=nextZeroReport)){
      ++zeroReports;reportedZeros=zeroFixes;nextZeroReport=now+30000;
      SupportLog_Event("rongyuan.rf_zero_corrections",zeroFixes,SupportLog_Data(tableAdjustments));
    }
    page=(page+1)%4;
    if(!page){
      ++sweeps;const auto us=static_cast<std::uint64_t>(NowUs());
      const unsigned duration=static_cast<unsigned>(std::min<std::uint64_t>(0xffffffffu,us-sweepStart));
      sweepStart=us;sumSweepUs+=duration;
      g_avgUs.store(g_avgUs.load()?(g_avgUs.load()*7+duration)/8:duration);
      g_maxUs.store(std::max(g_maxUs.load(),duration));
    }
    if(!streamAttempted && pages>=4){
      streamAttempted=true;SetLastError(ERROR_SUCCESS);
      streamActive=streamEligible && stream.Start();
      const DWORD streamError=streamActive?ERROR_SUCCESS:GetLastError();
      // Sending 1B does not prove the remote stream. The reader confirms it.
      if(!streamActive){stream.Stop();g_rfLiveness.StopStream();s.DisableStream();}
      char initial[220]{};
      _snprintf_s(initial,_TRUNCATE,
          "evidence.rongyuan.rf initial_table mapped_positive=%u mapped_min=%u mapped_max=%u nonkey_positive=%u repeated_pages=%u",
          everPositive,initialMappedMin,peak,initialNonkeyPositive,initialRepeatedPages);
      SupportLog_Evidence(initial,true);
      char mode[288]{};
      _snprintf_s(mode,_TRUNCATE,"evidence.rongyuan.rf mode revision=5 fixed_delay_us=0 bootstrap_pages=4 stream_eligible=%u stream_requested=%u stream_start_error=%lu table_mode=per_page liveness=f7 stream_hold=confirmed_delta table_order=send_time",
                  streamEligible?1:0,streamActive?1:0,streamError);
      SupportLog_Evidence(mode,true);
    }
    if(!page && sweeps && (sweeps==1 || sweeps==8 || now>=nextSummary)) {
      char line[360]{};
      _snprintf_s(line,_TRUNCATE,
          "evidence.rongyuan.rf travel revision=2 board=%u sweeps=%u pages=%u avg_sweep_us=%llu max_sweep_us=%u stream_packets=%u stream_bad=%u stream_io=%u max_stream_gap_us=%u transients=%u positive_slots=%u varying_slots=%u releases=%u peak_raw=%u",
          s.board,sweeps,pages,static_cast<unsigned long long>(sumSweepUs/sweeps),g_maxUs.load(),stream.packets.load(),
          stream.malformed.load(),stream.ioError.load(),stream.maxGapUs.load(),transients,everPositive,varying,releases,peak);
      SupportLog_Evidence(line,true);
      SupportLog_Event("rongyuan.rf_sweep_us",static_cast<unsigned>(sumSweepUs/sweeps),SupportLog_Data(g_maxUs.load()));
      SupportLog_Event("rongyuan.rf_stream",stream.packets.load(),SupportLog_Data(stream.ioError.load()));
      SupportLog_Event("rongyuan.rf_pages",pages,SupportLog_Data(transients));
      char reconcile[192]{};
      _snprintf_s(reconcile,_TRUNCATE,"evidence.rongyuan.rf reconcile revision=4 adjustments=%u zeros=%u superseded=%u drains=%u",
                  tableAdjustments,zeroFixes,superseded,s.Radio().drains);
      SupportLog_Evidence(reconcile,true);
      SupportLog_Event("rongyuan.rf_reconcile",tableAdjustments,SupportLog_Data(superseded));
      SupportLog_Event("rongyuan.rf_drains",s.Radio().drains,SupportLog_Data(zeroFixes));
      nextSummary=now+30000;
    }
  }
  stream.Stop(); // join/cancel the input request before maps/session can disappear
  g_rfLiveness.StopStream();
  s.DisableStream();
  char end[224]{};
  _snprintf_s(end,_TRUNCATE,"evidence.rongyuan.rf end revision=2 board=%u sweeps=%u pages=%u stream_packets=%u transients=%u releases=%u stop=%u topology_changed=%u",
      s.board,sweeps,pages,stream.packets.load(),transients,releases,g_stop.load()?1:0,topology!=g_topology.load()?1:0);
  SupportLog_Evidence(end,true);
  Clear();return false;
}

bool Run(const Candidate &c, bool &identityRejected) {
  if(c.receiver)return RunReceiver(c,identityRejected);
  Clear();
  Session s(c);
  const char *phase = "open";
  unsigned phaseCode=1;
  SetLastError(ERROR_SUCCESS);
  const bool ready = s.Open() && (phase = "identity proof", phaseCode=2, Proof(s)) &&
                     (phase = "assignments", phaseCode=3, Map(s)) &&
                     (phase = "input open", phaseCode=4, s.OpenInput());
  if (!ready) {
    const DWORD error=GetLastError();
    ++g_bad;
    SupportLog_Event("rongyuan.admission_failed",phaseCode,SupportLog_Win32(error));
    if (phaseCode >= 2) SupportLog_Event("rongyuan.admission_command",s.LastCommand(),SupportLog_Win32(error));
    identityRejected = s.IdentityRejected();
    DebugLog_Write(L"[rongyuan.stream] admission failed phase=%hs error=%lu",
                   phase, error);
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
  if(!s.Enable()) {++g_bad;SupportLog_Event("rongyuan.start_failed",s.board,SupportLog_Win32(GetLastError()));DebugLog_Write(L"[rongyuan.stream] start command failed error=%lu",GetLastError());Clear();return false;}
  g_present.store(true);
  g_connected.store(true);
  SupportLog_Event("rongyuan.connected",s.board,SupportLog_Data(s.units));
  DebugLog_Write(L"[rongyuan.stream] connected board=%u keys=%u "
                 L"units_per_mm=%u experimental=1",
                 s.model->board, g_mapped.load(), s.units);
  {
    static unsigned readerLines=0;
    if(readerLines++<kReaderLines) {
      char line[128]{};
      _snprintf_s(line,_TRUNCATE,"evidence.rongyuan reader board=%u pid=%04x priority=%d mmcss=%d",
                  s.board,s.ProductId(),GetThreadPriority(GetCurrentThread()),g_mmcss.load()?1:0);
      SupportLog_Evidence(line,true);
    }
  }
  // Press timing starts clean: no record or Windows key event of an earlier session.
  g_timing.Reset();
  g_osTail.store(g_osHead.load(std::memory_order_acquire),std::memory_order_release);
  g_reconcile.Reset();
  {std::lock_guard<std::mutex> lock(g_correctionLock);g_corrections.clear();}
  ReconcilerContext reconcilerContext{&s,s.units,s.board,CreateEventW(nullptr,TRUE,FALSE,nullptr)};
  HANDLE reconciler=reconcilerContext.stop && ReconcileEvent() && CorrectionEvent()
      ? reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Reconciler,&reconcilerContext,0,nullptr)) : nullptr;
  std::array<unsigned char,32> report{};
  HidIoOperation read(s.Input());
  DWORD endError=ERROR_SUCCESS;
  unsigned endReason=0; // 0 stop, 1 read start, 2 wait, 3 read finish, 4 invalid packet
  unsigned backlog=0;   // reports in a row that were already queued when the read started
  while (!g_stop.load()) {
    DWORD error=0,done=0;
    const auto start=read.StartRead(report.data(),static_cast<DWORD>(report.size()),&error);
    if(start==HidIoOperation::StartResult::Failed) {endReason=1;endError=error;++g_bad;break;}
    bool failed=false;
    if(start==HidIoOperation::StartResult::Pending) {
      backlog=0;
      HANDLE events[4]={read.Event(),g_wake};
      DWORD count=2,osAt=MAXDWORD,correctionAt=MAXDWORD;
      if(OsKeyEvent()) {osAt=count;events[count++]=OsKeyEvent();}
      if(reconciler) {correctionAt=count;events[count++]=CorrectionEvent();}
      while(!g_stop.load()) {
        const auto wait=WaitForMultipleObjects(count,events,FALSE,1000);
        if(wait==WAIT_OBJECT_0)break;
        if(wait==WAIT_OBJECT_0+osAt) {DrainOsKeys();continue;}
        if(wait==WAIT_OBJECT_0+correctionAt) {ApplyCorrections(*s.model);continue;}
        if(wait==WAIT_FAILED) {endReason=2;endError=GetLastError();failed=true;break;}
      }
    } else ++backlog;
    if(g_stop.load() || failed) {read.CancelAndDrain(&done,&error);break;}
    if(!read.Finish(&done,&error,false)) {endReason=3;endError=error;++g_bad;break;}
    const LONGLONG sampleUs=NowUs();
    // RID5 also carries vendor notifications unrelated to key travel.
    if(done==32 && report[0]==5 && report[1]!=0x1b)continue;
    mg::Sample sample{};
    std::uint16_t hid=0;unsigned milli=0;
    const bool published=mg::Parse(report.data(),done,sample) && Publish(sample,*s.model,GetTickCount64(),hid,milli);
    if(published && hid) {
      DrainOsKeys(); // Windows events up to now first, so their order against this sample holds.
      halljoy::ry_stream::PressRecord record{};
      if((!milli || InputTrace_IsBound(hid)) && g_timing.Sample(hid,milli,sampleUs,backlog,record))WritePress(record);
    }
    if(!published) {
      endReason=4;endError=ERROR_INVALID_DATA;
      ++g_bad;DebugLog_Write(L"[rongyuan.stream] invalid input bytes=%lu; session stopped",done);break;
    }
  }
  if(reconciler) {
    SetEvent(reconcilerContext.stop);
    WaitForSingleObject(reconciler,INFINITE);
    CloseHandle(reconciler);
  }
  if(reconcilerContext.stop)CloseHandle(reconcilerContext.stop);
  if(!g_stop.load() && endReason==4)halljoy::keyboard_support::ReportCommunicationAnomaly(
      static_cast<unsigned>(NativeAnalogProtocol::RongYuanStream));
  if(!g_stop.load())halljoy::keyboard_support::ReportCommunicationAccessFailure(
      static_cast<unsigned>(NativeAnalogProtocol::RongYuanStream),endError);
  SupportLog_Event("rongyuan.session_end",endReason,SupportLog_Win32(endError));
  DebugLog_Write(
      L"[rongyuan.stream] session ended stop=%d error=%lu failures=%llu",
      g_stop.load() ? 1 : 0, endError,
      static_cast<unsigned long long>(g_bad.load()));
  Clear();
  return false;
}
unsigned __stdcall Worker(void *) {
  // A board this table does not admit is stable for the same device: retry it a few
  // times (a reply can be late after plug-in), then wait for a device change instead
  // of querying the unknown keyboard every second (rongyuan_identity_budget.h).
  halljoy::ry_stream::IdentityBudget rejected;
  unsigned topology = g_topology.load();
  // The reader must not wait behind a busy foreground game (tester report 2026-10-16: wired
  // M1 V5 TMR sometimes lags): the scheduling of the realtime loop that consumes its values,
  // MMCSS "Games" plus highest priority. The thread sleeps in the HID read almost always.
  DWORD mmcssTask = 0;
  const HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Games", &mmcssTask);
  g_mmcss.store(mmcss != nullptr);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  try {
    while (!g_stop.load()) {
      if (g_topology.load() != topology) {topology = g_topology.load(); rejected.Reset();}
      unsigned visible = 0;
      const auto candidates = Enumerate(false, &visible);
      g_present.store(!candidates.empty());
      bool retry = false;
      for (const auto &c : candidates) {
        if (g_stop.load())
          break;
        if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
            !NativeAnalogRouting_IsClaimedBy(
                c.path.c_str(), NativeAnalogProtocol::RongYuanStream)) {
          retry = true; // owned by another protocol now; routing can change later
          continue;
        }
        if (rejected.Exhausted(c.path))
          continue;
        bool identityRejected = false;
        Run(c, identityRejected);
        const unsigned attempts = rejected.Failed(c.path, identityRejected);
        if (!attempts) {retry = true; continue;}
        if (attempts < halljoy::ry_stream::IdentityBudget::kAttempts) retry = true;
        else SupportLog_Event("rongyuan.identity_waiting", attempts, SupportLog_Data(c.attributes.ProductID));
        if (g_stop.load())
          break;
      }
      if (g_wake && !g_stop.load())
        // Nothing usable: sleep until a device change. A visible keyboard whose
        // control/input pair is not usable yet (held by another program, still
        // enumerating) is retried, since releasing a handle raises no device change.
        WaitForSingleObject(g_wake, retry ? 1000 : (candidates.empty() && visible) ? 2000 : INFINITE);
    }
  } catch (...) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.stream] worker exception; input cleared");
  }
  Clear();
  if (mmcss)
    AvRevertMmThreadCharacteristics(mmcss);
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
    if(c.receiver)continue; // Worker owns the complete bounded radio proof, once.
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
  g_topology.fetch_add(1);
  std::lock_guard<std::mutex> lock(g_service);
  if (g_wake)
    SetEvent(g_wake);
}
bool Present() { return g_present.load(); }
bool Connected() {
  if(!g_connected.load())return false;
  const auto last=g_last.load(),now=GetTickCount64();
  return !g_receiver.load() || g_rfLiveness.Connected(now,last);
}
bool Owns(std::uint16_t hid) {
  return Connected() && (Remapped() ? g_assigned : g_factory).Owns(hid);
}
halljoy::physical_analog::Publication::Value ReadPublished(
    const halljoy::physical_analog::Publication& publication,
    std::uint16_t hid, std::uint64_t now) {
  if(g_receiver.load())
    return publication.Read(hid,now,halljoy::ry_receiver::Liveness::kFreshMs,
                            g_rfLiveness.Streaming());
  return publication.Read(hid,now,kFreshMs);
}
std::uint16_t Get(std::uint16_t hid) {
  if(!Owns(hid))return 0;
  return ReadPublished(Remapped() ? g_assigned : g_factory,hid,GetTickCount64()).milli;
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
  if(out->connected)for(unsigned hid=1;hid<halljoy::physical_analog::kHidCount;++hid)if(ReadPublished(g_factory,static_cast<std::uint16_t>(hid),GetTickCount64()).milli)++out->activeKeys;
  const auto *model = g_receiver.load()?halljoy::ry_receiver::Model(g_board.load()):mg::Find(g_board.load(),g_vid.load(),g_pid.load());
  out->nominalRawLevels =
      model ? model->rangeUm * g_units.load() / 1000 + 1 : 0;
  out->inputReportBytes = g_receiver.load() && !g_rfLiveness.Streaming()?65:32;
  out->outputReportBytes = 0;
  out->successfulUpdates = g_ok.load();
  out->failedUpdates = g_bad.load();
  out->averageIntervalUs = g_avgUs.load();
  out->maximumIntervalUs = g_maxUs.load();
  // Table sweeps are not the delta stream polling frequency.
  out->updateHz10 = g_receiver.load() && g_rfLiveness.Streaming()?0:g_avgUs.load() ? 10000000 / g_avgUs.load() : 0;
  const auto last = g_last.load(), now = GetTickCount64();
  out->lastUpdateAgeMs =
      last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                0xffffffffull, now - last))
                          : 0;
  _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
               g_receiver.load()?(g_rfLiveness.Streaming()?L"%s: experimental 2.4 GHz analog stream":L"%s: experimental 2.4 GHz analog snapshots"):
               model && halljoy::keyboard_support::NativeNotice(static_cast<unsigned>(NativeAnalogProtocol::RongYuanStream),g_token.load(),true,g_pid.load())==0
                 ? L"%s: USB analog stream" : L"%s: USB analog stream; hardware/range validation pending",
               model ? model->name : L"RongYuan");
}
} // namespace
// Windows key events of the connected keyboard (UI thread, WM_INPUT) for the press timing.
// Key-downs only of keys bound to the gamepad; key-ups always, so an open press can end.
void RongYuanStream_ObserveRawKey(void *device, std::uint16_t hid, bool down) noexcept {
  if (!hid || g_receiver.load() || !g_connected.load() || (down && !InputTrace_IsBound(hid)))
    return;
  static void *lastDevice = nullptr;
  static unsigned lastIdentity = 0;
  static bool lastMatch = false;
  const unsigned identity = (g_vid.load() << 16) | g_pid.load();
  if (device != lastDevice || identity != lastIdentity) {
    lastDevice = device;
    lastIdentity = identity;
    lastMatch = false;
    wchar_t name[512]{};
    UINT size = static_cast<UINT>(std::size(name));
    if (device && GetRawInputDeviceInfoW(static_cast<HANDLE>(device), RIDI_DEVICENAME, name, &size) != static_cast<UINT>(-1)) {
      for (auto &ch : name) ch = static_cast<wchar_t>(towlower(ch));
      wchar_t want[32]{};
      swprintf_s(want, L"vid_%04x&pid_%04x", identity >> 16, identity & 0xffffu);
      lastMatch = wcsstr(name, want) != nullptr;
    }
  }
  if (!lastMatch)
    return;
  // How long WM_INPUT waited in the UI queue (16 ms tick resolution).
  const LONG queued = static_cast<LONG>(GetTickCount() - static_cast<DWORD>(GetMessageTime()));
  const unsigned head = g_osHead.load();
  if (head - g_osTail.load() >= g_osKeys.size())
    return; // reader behind: drop
  g_osKeys[head % g_osKeys.size()] = OsKey{hid, down, NowUs(), queued > 0 && queued < 60000 ? static_cast<unsigned>(queued) : 0u};
  g_osHead.store(head + 1, std::memory_order_release);
  if (const HANDLE event = OsKeyEvent())
    SetEvent(event);
}
const NativeAnalogBackendDescriptor &
RongYuanStream_GetNativeBackendDescriptor() {
  static const NativeAnalogBackendDescriptor d{
      kNativeAnalogBackendAbiVersion,
      sizeof(NativeAnalogBackendDescriptor),
      "rongyuan-stream",
      L"RongYuan USB / NJ98 V4S receiver (experimental)",
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
