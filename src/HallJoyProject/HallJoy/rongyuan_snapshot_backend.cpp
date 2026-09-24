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
namespace mg = halljoy::rongyuan;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = 150;
std::atomic<bool> g_running{false}, g_stop{false}, g_present{false},
    g_connected{false};
std::atomic<std::uint64_t> g_token{0}, g_ok{0}, g_bad{0}, g_last{0};
std::atomic<std::uint32_t> g_avgUs{0}, g_maxUs{0};
std::atomic<unsigned> g_board{0}, g_units{0}, g_mapped{0};
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
  std::wstring path;
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
        a.ProductID != mg::kProductId)
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
  bool Travel(unsigned page, mg::Values &values) {
    mg::Report r{};
    return Query(
        mg::Request(0xe5, 0xfe, 1, static_cast<std::uint8_t>(page)), r,
        [&](const auto &x) { return mg::ParseTravel(x, units, values); });
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
  mg::Report r{};
  if (!s.Query(mg::Request(0x8f), r,
               [](const auto &x) { return mg::Find(mg::Board(x)); }))
    return false;
  s.model = mg::Find(mg::Board(r));
  const unsigned version = r[8] | (r[9] << 8);
  mg::Report features{};
  // Old firmware has no E6 reply marker: use its documented version scale.
  s.Query(
      mg::Request(0xe6), features,
      [](const auto &x) { return x[1] == 0xe6 && x[2] == 0xaa; }, true);
  s.units = mg::Units(version, features);
  if (!s.units)
    return false;
  for (unsigned page = 0; page < 4; ++page) {
    mg::Values v{};
    if (!s.Travel(page, v))
      return false;
  }
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
    const auto factory = mg::Decode(s.model->matrix->data() + slot * 4);
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
      halljoy::layout_identity::Token("rongyuan-snapshot", s.model->product);
  if (!halljoy::native_layout::Publish(token, keys.data(), count))
    return false;
  g_token.store(token);
  g_board.store(s.model->board);
  g_units.store(s.units);
  g_mapped.store(count);
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
  g_last.store(now);
  ++g_ok;
}
bool Run(const Candidate &c) {
  Clear();
  Session s(c);
  const char *phase = "open";
  const bool ready = s.Open() && (phase = "identity proof", Proof(s)) &&
                     (phase = "assignments", Map(s));
  if (!ready) {
    ++g_bad;
    DebugLog_Write(L"[rongyuan.snapshot] admission failed phase=%hs error=%lu",
                   phase, GetLastError());
    Clear();
    return false;
  }
  if (!NativeAnalogRouting_Claim(mg::kVendorId, mg::kProductId, c.path.c_str(),
                                 NativeAnalogProtocol::RongYuanSnapshot) &&
      !NativeAnalogRouting_IsClaimedBy(
          c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot)) {
    Clear();
    return false;
  }
  g_present.store(true);
  g_connected.store(true);
  DebugLog_Write(L"[rongyuan.snapshot] connected board=%u keys=%u "
                 L"units_per_mm=%u experimental=1",
                 s.model->board, g_mapped.load(), s.units);
  auto logAt = GetTickCount64() + 5000;
  auto last = GetTickCount64();
  unsigned half = 0;
  while (!g_stop.load()) {
    mg::Values values{};
    if (!s.Travel(half, values)) {
      ++g_bad;
      break;
    }
    const auto now = GetTickCount64();
    Publish(half, values, now);
    const auto us = static_cast<std::uint32_t>(
        std::min<ULONGLONG>(0xffffffffull, (now - last) * 1000));
    last = now;
    const auto prev = g_avgUs.load();
    g_avgUs.store(prev ? (prev * 7 + us) / 8 : us);
    g_maxUs.store(std::max(g_maxUs.load(), us));
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
    if (g_wake)
      WaitForSingleObject(g_wake, 1);
  }
  DebugLog_Write(
      L"[rongyuan.snapshot] session ended stop=%d error=%lu failures=%llu",
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
                c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot))
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
    DebugLog_Write(L"[rongyuan.snapshot] worker exception; input cleared");
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
            c.path.c_str(), NativeAnalogProtocol::RongYuanSnapshot))
      continue;
    Session s(c);
    if (!s.Open() || !Proof(s))
      continue;
    any =
        NativeAnalogRouting_Claim(mg::kVendorId, mg::kProductId, c.path.c_str(),
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
  out->vendorId = mg::kVendorId;
  out->productId = mg::kProductId;
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
} // namespace
const NativeAnalogBackendDescriptor &
RongYuanSnapshot_GetNativeBackendDescriptor() {
  static const NativeAnalogBackendDescriptor d{
      kNativeAnalogBackendAbiVersion,
      sizeof(NativeAnalogBackendDescriptor),
      "rongyuan-snapshot",
      L"MonsGeek / EPOMAKER USB (experimental)",
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
      &Telemetry};
  return d;
}
