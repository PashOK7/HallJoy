#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off: Windows HID headers require this dependency order.
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
// clang-format on
#include "mg75_pro_backend.h"
#include "debug_log.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "mg75_pro_protocol.h"
#include "jingtai_v1_profiles.h"
#include "native_analog_routing.h"
#include "native_layout_state.h"
#include "physical_analog_state.h"
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
namespace mg = halljoy::mg75pro;
namespace jt = halljoy::jingtai_v1;
constexpr jt::Model kLegacyModel{"MG75PRO-1CA5-0807",L"IROK MG75 Pro",mg::kFactoryActions,81,3500};
std::atomic<unsigned> g_vid{0}, g_pid{0}, g_mapped{0}, g_range{0};
constexpr DWORD kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = 150;
std::atomic<bool> g_running{false}, g_stop{false}, g_present{false},
    g_connected{false};
std::atomic<std::uint64_t> g_token{0}, g_ok{0}, g_bad{0}, g_last{0};
std::atomic<std::uint32_t> g_avgUs{0}, g_maxUs{0};
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
  const jt::Model* model = &kLegacyModel;
};

bool TimedIo(HANDLE h, bool write, void *data, DWORD size, DWORD timeout,
             DWORD *done) {
  if (done)
    *done = 0;
  HidIoOperation op(h);
  DWORD error = 0;
  const auto start = write ? op.StartWrite(data, size, &error)
                           : op.StartRead(data, size, &error);
  if (start == HidIoOperation::StartResult::Failed) {
    SetLastError(error);
    return false;
  }
  if (start == HidIoOperation::StartResult::Pending) {
    const DWORD wait = op.Wait(timeout);
    if (wait == WAIT_OBJECT_0) {
      const bool ok = op.Finish(done, &error, false);
      if (!ok)
        SetLastError(error);
      return ok;
    }
    const DWORD saved = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
    op.CancelAndDrain(done, &error);
    SetLastError(saved ? saved : ERROR_GEN_FAILURE);
    return false;
  }
  const bool ok = op.Finish(done, &error, false);
  if (!ok)
    SetLastError(error);
  return ok;
}

bool HasId(PHIDP_PREPARSED_DATA pp, const HIDP_CAPS &caps,
           HIDP_REPORT_TYPE type, bool button) {
  USHORT count = button ? (type == HidP_Input ? caps.NumberInputButtonCaps
                                              : caps.NumberOutputButtonCaps)
                        : (type == HidP_Input ? caps.NumberInputValueCaps
                                              : caps.NumberOutputValueCaps);
  if (!count)
    return false;
  if (button) {
    std::vector<HIDP_BUTTON_CAPS> v(count);
    if (HidP_GetButtonCaps(type, v.data(), &count, pp) != HIDP_STATUS_SUCCESS)
      return false;
    return std::any_of(v.begin(), v.begin() + count,
                       [](const auto &x) { return x.ReportID == 0; });
  }
  std::vector<HIDP_VALUE_CAPS> v(count);
  if (HidP_GetValueCaps(type, v.data(), &count, pp) != HIDP_STATUS_SUCCESS)
    return false;
  return std::any_of(v.begin(), v.begin() + count,
                     [](const auto &x) { return x.ReportID == 0; });
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
    if (!HidD_GetAttributes(meta.v, &a) ||
        !((a.VendorID == mg::kVendorId && a.ProductID == mg::kProductId) ||
          jt::Candidate(a.VendorID,a.ProductID)))
      continue;
    PHIDP_PREPARSED_DATA pp = nullptr;
    if (!HidD_GetPreparsedData(meta.v, &pp))
      continue;
    HIDP_CAPS caps{};
    const NTSTATUS status = HidP_GetCaps(pp, &caps);
    const bool unnumbered = status == HIDP_STATUS_SUCCESS &&
                            (HasId(pp, caps, HidP_Input, true) ||
                             HasId(pp, caps, HidP_Input, false)) &&
                            (HasId(pp, caps, HidP_Output, true) ||
                             HasId(pp, caps, HidP_Output, false));
    HidD_FreePreparsedData(pp);
    const bool exact =
        status == HIDP_STATUS_SUCCESS && caps.UsagePage == mg::kUsagePage &&
        caps.Usage == mg::kUsage &&
        caps.InputReportByteLength == mg::kReportBytes &&
        caps.OutputReportByteLength == mg::kReportBytes && unnumbered;
    if (log)
      DebugLog_Write(
          L"[irok.mg75pro.enumerate] path_hash=%016llX version=%04X "
          L"exact=%d",
          static_cast<unsigned long long>(HashPath(detail->DevicePath)),
          a.VersionNumber, exact ? 1 : 0);
    wchar_t product[128]{};
    if (!HidD_GetProductString(meta.v, product, sizeof(product)))
      continue;
    std::wstring name(product);
    while (!name.empty() && iswspace(name.back()))
      name.pop_back();
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t ch) {
      return static_cast<wchar_t>(towupper(ch));
    });
    const auto* model = mg::ExactModel(a.VendorID,a.ProductID,name)
        ? &kLegacyModel : jt::Find(a.VendorID,a.ProductID,name);
    if (exact && model)
      out.push_back({detail->DevicePath, a, caps, model});
  }
  SetupDiDestroyDeviceInfoList(set);
  return out;
}

class Session {
public:
  explicit Session(const Candidate &c) : candidate(c) {}
  ~Session() {
    std::lock_guard<std::mutex> lock(g_activeLock);
    if (g_active == handle.v)
      g_active = INVALID_HANDLE_VALUE;
  }
  bool Open() {
    // Exclusive ownership of this vendor interface prevents untagged half
    // replies from another application mixing with this session's replies.
    handle = Handle(CreateFileW(
        candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    if (!handle)
      return false;
    if (!HidD_SetNumInputBuffers(handle.v, 64) || !HidD_FlushQueue(handle.v))
      return false;
    std::lock_guard<std::mutex> lock(g_activeLock);
    g_active = handle.v;
    return true;
  }
  bool Exchange(const mg::Report &request, mg::Frame &response) {
    if (poisoned || g_stop.load())
      return false;
    auto tx = request;
    DWORD sent = 0;
    if (!TimedIo(handle.v, true, tx.data(), static_cast<DWORD>(tx.size()), 50,
                 &sent) ||
        sent != tx.size())
      return poisoned = true, false;
    response = {};
    const auto deadline = GetTickCount64() + 120;
    while (!response.Complete() && !g_stop.load()) {
      const auto now = GetTickCount64();
      if (now >= deadline)
        return poisoned = true, false;
      mg::Report rx{};
      DWORD n = 0;
      if (!TimedIo(handle.v, false, rx.data(), static_cast<DWORD>(rx.size()),
                   static_cast<DWORD>(deadline - now), &n) ||
          !response.Push(rx, n, request[3]))
        return poisoned = true, false;
    }
    if (!response.Complete())
      return poisoned = true, false;
    return true;
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
  g_vid.store(0); g_pid.store(0); g_mapped.store(0); g_range.store(0);
}
bool Proof(Session &s, const jt::Model& model = kLegacyModel) {
  // Keep firmware-derived factory proof for the existing MG75 Pro path.
  // Legacy V1 peers use pinned vendor maps; their client does not establish2B.
  if (&model == &kLegacyModel) for (unsigned row = 0; row < 6; row += 2) {
    mg::Frame f;
    if (!s.Exchange(mg::Factory(row), f) || !mg::MatchFactory(f, row))
      return false;
  }
  for (unsigned half = 1; half <= 2; ++half) {
    mg::Frame f;
    mg::Values v;
    if (!s.Exchange(mg::Travel(half), f) || !mg::ParseTravel(f, v))
      return false;
  }
  return true;
}
bool InstallMap(const std::array<std::uint16_t, mg::kSlots> &assigned,
                const jt::Model& model = kLegacyModel) {
  std::array<halljoy::native_layout::Key, mg::kSlots> keys{};
  std::size_t count = 0;
  for (std::size_t slot = 0; slot < mg::kSlots; ++slot) {
    const auto factory = mg::Decode(model.actions[slot]);
    if (!factory)
      continue;
    if (count >= keys.size())
      return false;
    const auto id = static_cast<std::uint8_t>(slot + 1);
    if (!g_factory.Bind(id, factory))
      return false;
    if (assigned[slot] && !g_assigned.Bind(id, assigned[slot]))
      return false;
    keys[count++] = {factory, assigned[slot]};
  }
  const auto token =
      halljoy::layout_identity::Token("irok-mg75-pro", model.identity);
  if (count != model.count ||
      (token && !halljoy::native_layout::Publish(token, keys.data(), count)))
    return false;
  g_token.store(token);
  g_mapped.store(static_cast<unsigned>(count));
  return true;
}
bool ReadMap(Session &s, const jt::Model& model,
             std::array<std::uint16_t, mg::kSlots>& assigned) {
  std::size_t slot = 0;
  while (slot < mg::kSlots) {
    mg::Keys keys{};
    std::array<std::size_t, 14> slots{};
    std::size_t count = 0;
    for (; slot < mg::kSlots && count < keys.size(); ++slot) {
      if (!model.actions[slot])
        continue;
      slots[count] = slot;
      keys[count++] = model.actions[slot] == 0xf001 ? 1 :
          static_cast<std::uint8_t>(model.actions[slot]);
    }
    if (!count)
      break;
    mg::Frame f;
    mg::Assignments values{};
    if (!s.Exchange(mg::Layout(keys), f) || !mg::ParseLayout(f, keys, values))
      return false;
    for (std::size_t i = 0; i < count; ++i)
      assigned[slots[i]] = values[i];
  }
  return true;
}
bool Map(Session &s, const jt::Model& model) {
  std::array<std::uint16_t, mg::kSlots> assigned{};
  return ReadMap(s,model,assigned) && InstallMap(assigned,model);
}
void Publish(unsigned half, const mg::Values &values, std::uint64_t now,
             const jt::Model& model = kLegacyModel) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    const auto slot = (half - 1) * 63 + i;
    if (!model.actions[slot])
      continue;
    const auto id = static_cast<std::uint8_t>(slot + 1);
    const auto value = mg::Normalize(values[i],model.range);
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
  const bool ready = s.Open() && (phase = "identity proof", Proof(s,*c.model)) &&
                     (phase = "assignments", Map(s,*c.model));
  if (!ready) {
    ++g_bad;
    DebugLog_Write(L"[irok.mg75pro] admission failed phase=%hs error=%lu",
                   phase, GetLastError());
    Clear();
    return false;
  }
  if (!NativeAnalogRouting_Claim(c.attributes.VendorID, c.attributes.ProductID, c.path.c_str(),
                                 NativeAnalogProtocol::IrokMg75Pro) &&
      !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),
                                       NativeAnalogProtocol::IrokMg75Pro)) {
    Clear();
    return false;
  }
  g_present.store(true);
  g_vid.store(c.attributes.VendorID); g_pid.store(c.attributes.ProductID); g_range.store(c.model->range);
  g_connected.store(true);
  DebugLog_Write(L"[irok.mg75pro] connected path_hash=%016llX model=%ls mapped=%u "
                 L"range_um=%u experimental=1 calibration=0",
                 static_cast<unsigned long long>(HashPath(c.path)),c.model->name,c.model->count,c.model->range);
  auto logAt = GetTickCount64() + 5000;
  auto last = GetTickCount64();
  unsigned half = 1;
  while (!g_stop.load()) {
    mg::Frame f;
    mg::Values values{};
    if (!s.Exchange(mg::Travel(half), f) || !mg::ParseTravel(f, values)) {
      ++g_bad;
      break;
    }
    const auto now = GetTickCount64();
    Publish(half, values, now,*c.model);
    const auto us = static_cast<std::uint32_t>(
        std::min<ULONGLONG>(0xffffffffull, (now - last) * 1000));
    last = now;
    const auto prev = g_avgUs.load();
    g_avgUs.store(prev ? (prev * 7 + us) / 8 : us);
    g_maxUs.store(std::max(g_maxUs.load(), us));
    if (now >= logAt) {
      DebugLog_Write(L"[irok.mg75pro] health half_updates=%llu failures=%llu "
                     L"avg_us=%u max_us=%u",
                     static_cast<unsigned long long>(g_ok.load()),
                     static_cast<unsigned long long>(g_bad.load()),
                     g_avgUs.load(), g_maxUs.load());
      logAt = now + 5000;
    }
    half = 3 - half;
    if (g_wake)
      WaitForSingleObject(g_wake, 1);
  }
  DebugLog_Write(
      L"[irok.mg75pro] session ended stop=%d error=%lu failures=%llu",
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
            !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),
                                             NativeAnalogProtocol::IrokMg75Pro))
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
    DebugLog_Write(L"[irok.mg75pro] worker exception; input cleared");
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
        !NativeAnalogRouting_IsClaimedBy(c.path.c_str(),
                                         NativeAnalogProtocol::IrokMg75Pro))
      continue;
    Session s(c);
    std::array<std::uint16_t,mg::kSlots> assigned{};
    if (!s.Open() || !Proof(s,*c.model) || !ReadMap(s,*c.model,assigned))
      continue;
    any =
        NativeAnalogRouting_Claim(c.attributes.VendorID, c.attributes.ProductID, c.path.c_str(),
                                  NativeAnalogProtocol::IrokMg75Pro) ||
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
  out->vendorId = static_cast<std::uint16_t>(g_vid.load());
  out->productId = static_cast<std::uint16_t>(g_pid.load());
  out->usagePage = mg::kUsagePage;
  out->usage = mg::kUsage;
  out->mappedKeys = Connected() ? g_mapped.load() : 0;
  out->activeKeys = g_factory.Active(GetTickCount64());
  out->nominalRawLevels = g_range.load() ? g_range.load()+1 : 0;
  out->inputReportBytes = 65;
  out->outputReportBytes = 65;
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
               L"JingTai V1 experimental: independent 6x21 travel, model-specific "
               L"range; hardware testing pending");
}
} // namespace
const NativeAnalogBackendDescriptor &Mg75Pro_GetNativeBackendDescriptor() {
  static const NativeAnalogBackendDescriptor d{
      kNativeAnalogBackendAbiVersion,
      sizeof(NativeAnalogBackendDescriptor),
      "irok-mg75-pro",
      L"JingTai V1 (experimental)",
      NativeAnalogProtocol::IrokMg75Pro,
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
#if defined(HALLJOY_ANALOG_SIMULATOR)
bool Mg75Pro_TestPublication(int *line) {
  const auto fail = [&](int x) {
    if (line)
      *line = x;
    return false;
  };
  if (g_thread || Connected())
    return fail(__LINE__);
  struct Reset {
    bool enabled = halljoy::native_layout::enabled.load();
    std::uint64_t token = halljoy::native_layout::activeToken.load();
    ~Reset() {
      Clear();
      halljoy::native_layout::enabled.store(enabled);
      halljoy::native_layout::activeToken.store(token);
    }
  } reset;
  // Test every production map and missing-preset fallback without HID I/O.
  const auto checkModel = [&](const jt::Model& model) {
    Clear();
    std::array<std::uint16_t, mg::kSlots> assigned{};
    for (std::size_t i=0;i<assigned.size();++i)
      assigned[i]=mg::Decode(model.actions[i]);
    if (!InstallMap(assigned,model) || g_mapped.load()!=model.count) return false;
    g_connected.store(true);
    halljoy::native_layout::enabled.store(false);
    mg::Values full{}; full.fill(static_cast<std::uint16_t>(model.range));
    const auto time=GetTickCount64();
    Publish(1,full,time,model); Publish(2,full,time,model);
    for (const auto hid:assigned) if (hid && Get(hid)!=1000) return false;
    return true;
  };
  for (const auto& identity:jt::identities)
    if (!checkModel(*identity.model)) return fail(__LINE__);
  auto manual=jt::model_K; manual.identity="TEST-NO-VISUAL-PRESET";
  if (!checkModel(manual) || g_token.load()!=0) return fail(__LINE__);
  Clear();
  std::array<std::uint16_t, mg::kSlots> map{};
  for (std::size_t i = 0; i < map.size(); ++i)
    map[i] = mg::Decode(mg::kFactoryActions[i]);
  map[44] = 4;
  map[64] = 4;
  if (!InstallMap(map))
    return fail(__LINE__);
  g_connected.store(true);
  halljoy::native_layout::enabled.store(true);
  halljoy::native_layout::activeToken.store(g_token.load());
  mg::Values a{}, b{};
  a[44] = 1750;
  b[1] = 3500;
  b[53] = 875;
  const auto now = GetTickCount64();
  Publish(1, a, now);
  Publish(2, b, now);
  if (Get(4) != 1000 || Get(mg::kFn) != 250)
    return fail(__LINE__);
  b[1] = 0;
  Publish(2, b, now);
  if (Get(4) != 500)
    return fail(__LINE__);
  halljoy::native_layout::enabled.store(false);
  if (Get(26) != 500 || Get(4) != 0)
    return fail(__LINE__);
  if (g_factory.Read(26, now + kFreshMs + 1, kFreshMs).milli != 0)
    return fail(__LINE__);
  a.fill(0);
  b.fill(0);
  Publish(1, a, now);
  Publish(2, b, now);
  if (Get(26) || Get(mg::kFn))
    return fail(__LINE__);
  Clear();
  if (Owns(26) || Get(26))
    return fail(__LINE__);
  return true;
}
#endif
