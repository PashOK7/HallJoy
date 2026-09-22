#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "aula_hero84he_backend.h"
#include "aula_hero84he_diagnostic_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "physical_analog_state.h"
#include "aula_hero84he_factory.h"
#include "aula_hero_family.h"
#include "configured_xusb_builder.h"
#include "native_layout_state.h"
#include "generated/layout_pipeline/identities.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
namespace hero = aula_hero84he_diagnostic;
using Clock = std::chrono::steady_clock;
constexpr DWORD kSliceMs = 50, kCommandTimeoutMs = 200, kStopTimeoutMs = 3000;
constexpr ULONGLONG kFreshMs = 750, kDemandMs = 300;
const halljoy::hero_family::Model* g_model=&halljoy::hero_family::models[0];
// Complete physical poll domain; live layer-0 assignments are kept separate
// from factory identities and selected only after automatic layout succeeds.
constexpr std::array<std::uint16_t, 84> kPositions{
    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,  25,  26,  27, 28,
     29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,  55,  56, 57,
     58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 95, 98, 99, 100, 101, 102, 103}};
constexpr std::array<std::uint16_t, 5> kMovement{{30, 43, 44, 45, 70}};

struct Handle
{
    HANDLE v = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE x) : v(x)
    {
    }
    ~Handle()
    {
        if (v != INVALID_HANDLE_VALUE)
            CloseHandle(v);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&x) noexcept : v(x.v)
    {
        x.v = INVALID_HANDLE_VALUE;
    }
    Handle &operator=(Handle &&x) noexcept
    {
        if (this != &x)
        {
            if (v != INVALID_HANDLE_VALUE)
                CloseHandle(v);
            v = x.v;
            x.v = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const
    {
        return v != INVALID_HANDLE_VALUE;
    }
};
struct Candidate
{
    std::wstring path;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
};

std::atomic<bool> g_prepared{false}, g_present{false}, g_connected{false}, g_running{false}, g_stop{false},
    g_mapReady{false};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0}, g_mapped{0};
std::atomic<std::uint64_t> g_ok{0}, g_bad{0};
std::atomic<ULONGLONG> g_last{};
std::array<std::atomic<std::uint16_t>, 256> g_hidAt{};
constexpr std::size_t kHidCount = halljoy::physical_analog::kHidCount;
std::array<std::atomic<bool>, kHidCount> g_has{};
std::atomic<std::uint64_t> g_layoutToken{0};
std::array<std::atomic<ULONGLONG>, kHidCount> g_demand{};
halljoy::physical_analog::Publication g_physical, g_factory;
std::array<std::uint16_t,256> FactoryMap() {
    std::array<std::uint16_t,256> result{};
    for (const auto& key : halljoy::hero84::factory) result[key.position]=key.hid;
    return result;
}
auto kFactoryAt=FactoryMap();
bool Remapped() { return halljoy::native_layout::UsesRemapping(g_layoutToken.load()); }
std::array<std::atomic<std::uint16_t>, 256> g_top{}, g_bottom{};
std::atomic<std::uint16_t> g_cursor{1};
std::mutex g_service, g_activeLock;
HANDLE g_thread = nullptr, g_wake = nullptr, g_active = INVALID_HANDLE_VALUE;

std::uint64_t HashPath(const std::wstring &s)
{
    std::uint64_t h = 1469598103934665603ull;
    for (wchar_t c : s)
    {
        h ^= static_cast<std::uint16_t>(towlower(c));
        h *= 1099511628211ull;
    }
    return h;
}

bool TimedIo(HANDLE h, bool write, void *data, DWORD size, DWORD timeout, DWORD *done)
{
    if (done)
        *done = 0;
    HidIoOperation op(h);
    DWORD error = 0;
    const auto start = write ? op.StartWrite(data, size, &error) : op.StartRead(data, size, &error);
    if (start == HidIoOperation::StartResult::Failed)
    {
        SetLastError(error);
        return false;
    }
    if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = op.Wait(timeout);
        if (wait == WAIT_OBJECT_0)
        {
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

bool HasId(PHIDP_PREPARSED_DATA pp, const HIDP_CAPS &caps, HIDP_REPORT_TYPE type, bool button)
{
    USHORT count = button ? (type == HidP_Input ? caps.NumberInputButtonCaps : caps.NumberOutputButtonCaps)
                          : (type == HidP_Input ? caps.NumberInputValueCaps : caps.NumberOutputValueCaps);
    if (!count)
        return false;
    if (button)
    {
        std::vector<HIDP_BUTTON_CAPS> v(count);
        if (HidP_GetButtonCaps(type, v.data(), &count, pp) != HIDP_STATUS_SUCCESS)
            return false;
        return std::any_of(v.begin(), v.begin() + count, [](const auto &x) { return x.ReportID == hero::kReportId; });
    }
    std::vector<HIDP_VALUE_CAPS> v(count);
    if (HidP_GetValueCaps(type, v.data(), &count, pp) != HIDP_STATUS_SUCCESS)
        return false;
    return std::any_of(v.begin(), v.begin() + count, [](const auto &x) { return x.ReportID == hero::kReportId; });
}

std::vector<Candidate> Enumerate(bool log)
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
        return {};
    std::vector<Candidate> out;
    for (DWORD i = 0;; ++i)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }
        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &bytes, nullptr);
        if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;
        std::vector<std::uint8_t> mem(bytes);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(mem.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, bytes, nullptr, nullptr))
            continue;
        Handle meta(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!meta)
            continue;
        HIDD_ATTRIBUTES a{};
        a.Size = sizeof(a);
        if (!HidD_GetAttributes(meta.v, &a) || a.VendorID != hero::kVendorId || a.ProductID != hero::kProductId)
            continue;
        PHIDP_PREPARSED_DATA pp = nullptr;
        if (!HidD_GetPreparsedData(meta.v, &pp))
            continue;
        HIDP_CAPS caps{};
        const NTSTATUS status = HidP_GetCaps(pp, &caps);
        const bool id9 = status == HIDP_STATUS_SUCCESS &&
                         (HasId(pp, caps, HidP_Input, true) || HasId(pp, caps, HidP_Input, false)) &&
                         (HasId(pp, caps, HidP_Output, true) || HasId(pp, caps, HidP_Output, false));
        HidD_FreePreparsedData(pp);
        const bool exact = status == HIDP_STATUS_SUCCESS && caps.UsagePage == hero::kUsagePage &&
                           caps.Usage == hero::kUsage && caps.InputReportByteLength == hero::kReportBytes &&
                           caps.OutputReportByteLength == hero::kReportBytes && id9;
        if (log)
            DebugLog_Write(L"[aula.hero84.production.enumerate] path_hash=%016llX version=%04X "
                           L"exact=%d",
                           static_cast<unsigned long long>(HashPath(detail->DevicePath)), a.VersionNumber,
                           exact ? 1 : 0);
        if (exact)
            out.push_back({detail->DevicePath, a, caps});
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

class Session
{
  public:
    explicit Session(const Candidate &c) : candidate(c)
    {
    }
    ~Session()
    {
        std::lock_guard<std::mutex> lock(g_activeLock);
        if (g_active == handle.v)
            g_active = INVALID_HANDLE_VALUE;
    }
    bool Open()
    {
        handle =
            Handle(CreateFileW(candidate.path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle)
            return false;
        HidD_SetNumInputBuffers(handle.v, 256);
        std::lock_guard<std::mutex> lock(g_activeLock);
        g_active = handle.v;
        return true;
    }
    bool Exchange(const hero::Report &request, hero::Report *response, std::uint32_t *rtt)
    {
        if (!response || !rtt)
            return false;
        const auto begin = Clock::now();
        hero::Report tx = request;
        DWORD sent = 0;
        if (!TimedIo(handle.v, true, tx.data(), static_cast<DWORD>(tx.size()), kSliceMs, &sent) || sent != tx.size())
            return false;
        response->fill(0);
        DWORD received = 0;
        if (!TimedIo(handle.v, false, response->data(), static_cast<DWORD>(response->size()), kCommandTimeoutMs,
                     &received) ||
            received != response->size())
            return false;
        *rtt = static_cast<std::uint32_t>(std::min<std::int64_t>(
            0xffffffffll, std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count()));
        return true;
    }

  private:
    Candidate candidate;
    Handle handle{};
};

void Clear()
{
    g_mapReady.store(false);
    halljoy::native_layout::Clear(g_layoutToken.exchange(0));
    g_physical.Clear();
    g_factory.Clear();
    g_mapped.store(0);
    g_last.store(0);
    for (auto& has : g_has) has.store(false);
    for (auto& demand : g_demand) demand.store(0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        g_hidAt[i].store(0);

        g_top[i].store(0);
        g_bottom[i].store(0);

    }
}
bool Identity(Session &s)
{
    hero::Report q{}, r{};
    std::uint32_t us = 0;
    std::array<std::uint8_t, 6> uuid{};
    if (!hero::BuildIdentityRead(&q) || !s.Exchange(q, &r, &us) || !hero::ParseIdentityResponse(r, &uuid)) return false;
    std::uint64_t id=0; for(auto b:uuid)id=(id<<8)|b;
    const auto* model=halljoy::hero_family::Find(id);
    if(!model)return false;
    g_model=model;return true;
}
bool InstallMap(const std::array<std::uint32_t,256>& assignments)
{
    Clear();
    std::array<halljoy::native_layout::Key,256> remaps{};
    kFactoryAt.fill(0);
    std::size_t count=0;
    for (const auto& key : g_model->keys) {
        kFactoryAt[key.position]=key.hid;
        const auto hid=halljoy::hero84::DecodeAssignment(assignments[key.position]);
        g_hidAt[key.position].store(hid);
        g_factory.Bind(static_cast<std::uint8_t>(key.position),key.hid);
        if (hid) { g_physical.Bind(static_cast<std::uint8_t>(key.position),hid); g_has[hid].store(true); }
        remaps[count++]={key.hid,hid};
    }
    const auto token=g_model->token;
    if (!halljoy::native_layout::Publish(token,remaps.data(),count)) { Clear(); return false; }
    g_layoutToken.store(token);
    g_mapped.store(static_cast<std::uint32_t>(count));
    g_mapReady.store(true);
    return true;
}
bool Map(Session &s)
{
    std::array<std::uint32_t,256> assignments{};
    std::vector<std::uint16_t> positions;
    for(const auto& key:g_model->keys)positions.push_back(key.position);
    for (std::size_t base=0;base<positions.size();base+=hero::kMaxPositions) {
        const auto n=std::min(hero::kMaxPositions,positions.size()-base);
        hero::Report q{},r{}; std::uint32_t us=0;
        std::array<hero::Assignment,hero::kMaxPositions> values{};
        if (!hero::BuildAssignmentRead(0,positions.data()+base,n,&q) || !s.Exchange(q,&r,&us) ||
            !hero::ParseAssignmentResponse(r,0,positions.data()+base,n,&values)) return false;
        for (std::size_t i=0;i<n;++i) assignments[values[i].position]=values[i].value;
    }
    return InstallMap(assignments);
}
std::size_t Plan(std::array<std::uint16_t, hero::kMaxPositions> *out)
{
    if (!out)
        return 0;
    std::size_t n = 0;
    const auto add = [&](std::uint16_t p) {
        if (!p || n == out->size())
            return;
        for (std::size_t i = 0; i < n; ++i)
            if ((*out)[i] == p)
                return;
        (*out)[n++] = p;
    };
    for (auto p : kMovement)
        add(p);
    const ULONGLONG now = GetTickCount64();
    const auto first = g_cursor.fetch_add(1);
    for (std::size_t offset = 0; offset < 255 && n < out->size(); ++offset)
    {
        const std::size_t p = (static_cast<std::size_t>(first) + offset - 1) % 255 + 1;
        const auto hid = Remapped() ? g_hidAt[p].load() : kFactoryAt[p];
        const auto demand = hid ? g_demand[hid].load() : 0;
        if (demand && now >= demand && now - demand <= kDemandMs)
            add(static_cast<std::uint16_t>(p));
    }
    return n;
}
void Publish(const hero::DirectSample &x)
{
    if (x.position >= kFactoryAt.size() || !kFactoryAt[x.position] || !x.current)
        return;
    const auto raw = x.current;
    const auto position = x.position;
    auto top = g_top[position].load();
    if (!top || raw > top)
    {
        top = raw;
        g_top[position].store(top);
    }
    auto bottom = g_bottom[position].load();
    if (!bottom || raw < bottom)
    {
        bottom = raw;
        g_bottom[position].store(bottom);
    }
    std::uint16_t milli = 0;
    // Firmware exports episode-minimum with current; its scanner path and the
    // related family implementation both indicate pressing reduces raw value.
    // Until an observed span exists, fail neutral rather than assume a range.
    if (top > bottom + 32 && raw < top)
    {
        const auto span = static_cast<std::uint32_t>(top - bottom);
        milli = static_cast<std::uint16_t>(std::min<std::uint32_t>(1000, ((top - raw) * 1000u + span / 2u) / span));
        if (milli < 8)
            milli = 0;
    }
    const auto now=GetTickCount64();
    g_physical.Publish(static_cast<std::uint8_t>(position), milli, now);
    g_factory.Publish(static_cast<std::uint8_t>(position), milli, now);
}
bool Run(const Candidate &c)
{
    Clear();
    Session s(c);
    if (!s.Open() || !Identity(s))
    {
        ++g_bad;
        return false;
    }
    g_connected.store(true);
    g_inputBytes.store(c.caps.InputReportByteLength);
    g_outputBytes.store(c.caps.OutputReportByteLength);
    if (!Map(s))
    {
        ++g_bad;
        g_connected.store(false);
        return false;
    }
    DebugLog_Write(L"[aula.hero84.production] session begin path_hash=%016llX "
                   L"allowed=82/01,83,94/02 poll=selected outstanding=1",
                   static_cast<unsigned long long>(HashPath(c.path)));
    auto next = Clock::now();
    std::uint32_t failures = 0;
    while (!g_stop.load())
    {
        std::array<std::uint16_t, hero::kMaxPositions> positions{};
        const auto count = Plan(&positions);
        hero::Report q{}, r{};
        std::uint32_t us = 0;
        if (!count || !hero::BuildDirectRead(positions.data(), count, &q) || !s.Exchange(q, &r, &us))
        {
            ++g_bad;
            if (++failures >= 3)
                break;
        }
        else
        {
            std::array<hero::DirectSample, hero::kMaxPositions> samples{};
            if (!hero::ParseDirectResponse(r, positions.data(), count, &samples))
            {
                ++g_bad;
                if (++failures >= 3)
                    break;
            }
            else
            {
                failures = 0;
                for (std::size_t i = 0; i < count; ++i)
                    Publish(samples[i]);
                g_last.store(GetTickCount64());
                ++g_ok;
            }
        }
        next += std::chrono::milliseconds(1);
        const auto now = Clock::now();
        if (next > now)
            Sleep(static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count()));
        else
            next = now;
    }
    g_connected.store(false);
    Clear();
    return false;
}
unsigned __stdcall Worker(void *)
{
    while (!g_stop.load())
    {
        bool found = false;
        for (const auto &c : Enumerate(false))
        {
            if (!NativeAnalogRouting_IsClaimedBy(c.path.c_str(), NativeAnalogProtocol::AulaHero84He))
                continue;
            found = true;
            (void)Run(c);
            break;
        }
        if (g_stop.load())
            break;
        if (!found)
            g_present.store(false);
        if (g_wake)
            WaitForSingleObject(g_wake, 1000);
        if (g_wake)
            ResetEvent(g_wake);
    }
    g_connected.store(false);
    g_running.store(false);
    return 0;
}
bool Prepare()
{
    bool any = false;
    for (const auto &c : Enumerate(true))
    {
        if (NativeAnalogRouting_IsClaimed(c.path.c_str()) &&
            !NativeAnalogRouting_IsClaimedBy(c.path.c_str(), NativeAnalogProtocol::AulaHero84He))
            continue;
        Session s(c);
        if (!s.Open() || !Identity(s))
            continue;
        const bool claim = NativeAnalogRouting_Claim(hero::kVendorId, hero::kProductId, c.path.c_str(),
                                                     NativeAnalogProtocol::AulaHero84He);
        any = any || claim || NativeAnalogRouting_IsClaimedBy(c.path.c_str(), NativeAnalogProtocol::AulaHero84He);
        DebugLog_Write(L"[aula.hero84.production.prepare] identity_proven "
                       L"path_hash=%016llX claimed=%d",
                       static_cast<unsigned long long>(HashPath(c.path)), claim ? 1 : 0);
    }
    g_prepared.store(true);
    g_present.store(any);
    return any;
}
bool Start()
{
    std::lock_guard<std::mutex> lock(g_service);
    if (!g_prepared.load())
        (void)Prepare();
    if (g_thread)
        return g_running.load();
    if (!g_present.load())
        return false;
    g_stop.store(false);
    g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake)
        return false;
    g_running.store(true);
    unsigned id = 0;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &id));
    if (!g_thread)
    {
        g_running.store(false);
        CloseHandle(g_wake);
        g_wake = nullptr;
        return false;
    }
    DebugLog_Write(L"[aula.hero84.production.start] worker=%u experimental=1", id);
    return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_service);
    if (!g_thread)
        return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true);
    if (g_wake)
        SetEvent(g_wake);
    {
        std::lock_guard<std::mutex> active(g_activeLock);
        if (g_active != INVALID_HANDLE_VALUE)
            CancelIoEx(g_active, nullptr);
    }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0)
        return NativeAnalogBackendStopFailed(generation, halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,
                                             wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread);
    g_thread = nullptr;
    if (g_wake)
        CloseHandle(g_wake);
    g_wake = nullptr;
    g_connected.store(false);
    Clear();
    return NativeAnalogBackendStopJoined(generation);
}
void Notify()
{
    if (g_wake)
        SetEvent(g_wake);
}
bool Present()
{
    return g_present.load();
}
bool Connected()
{
    return g_connected.load() && g_mapReady.load();
}
bool Owns(std::uint16_t hid)
{
    if (!hid || hid >= kHidCount || !Connected())
        return false;
    g_demand[hid].store(GetTickCount64());
    // Ownership is a session property; a stale sample must not fall back to digital input.
    return (Remapped() ? g_physical : g_factory).Owns(hid);
}
std::uint16_t Get(std::uint16_t hid)
{
    return Owns(hid) ? (Remapped() ? g_physical : g_factory).Read(hid, GetTickCount64(), kFreshMs).milli : 0;
}
void Telemetry(NativeAnalogBackendTelemetry *out)
{
    if (!out)
        return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
    out->verifiedLayoutToken = out->connected ? g_layoutToken.load() : 0;
    out->vendorId = hero::kVendorId;
    out->productId = hero::kProductId;
    out->usagePage = hero::kUsagePage;
    out->usage = hero::kUsage;
    out->mappedKeys = g_mapped.load();
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->successfulUpdates = g_ok.load();
    out->failedUpdates = g_bad.load();
    const auto last = g_last.load(), now = GetTickCount64();
    out->lastUpdateAgeMs =
        last && now >= last ? static_cast<std::uint32_t>(std::min<ULONGLONG>(0xffffffffull, now - last)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
                 L"AULA HERO family experimental: 82/01 + 83 + read-only 94/02; "
                 L"adaptive observed range");
}
} // namespace

const NativeAnalogBackendDescriptor &AulaHero84He_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,
                                                 sizeof(NativeAnalogBackendDescriptor),
                                                 "aula-hero84he-9402-experimental",
                                                 L"AULA HERO family (experimental native analogue)",
                                                 NativeAnalogProtocol::AulaHero84He,
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

bool AulaHero84He_TestPublication(int* failedLine) {
    const auto fail=[&](int line) { if(failedLine) *failedLine=line; return false; };
    // Production publication functions, synthetic samples only; no HID or worker.
    if(g_thread || g_connected.load()) return fail(__LINE__);
    struct Reset { bool enabled=halljoy::native_layout::enabled.load(); std::uint64_t token=halljoy::native_layout::activeToken.load();
        ~Reset(){g_connected.store(false);Clear();halljoy::native_layout::enabled.store(enabled);halljoy::native_layout::activeToken.store(token);} } reset;
    Clear();
    g_hidAt[30].store(26);g_hidAt[43].store(26);g_has[26].store(true);
    g_physical.Bind(30,26);g_physical.Bind(43,26);
    const auto token=halljoy::layout_identity::Token("aula-hero84", "110000000005");
    g_layoutToken.store(token);halljoy::native_layout::enabled.store(true);halljoy::native_layout::activeToken.store(token);
    g_mapReady.store(true);g_connected.store(true);
    Publish({30,10000,0,false});Publish({43,12000,0,false});
    if(!Owns(26) || Get(26)!=0) return fail(__LINE__);
    Publish({30,5000,0,false});Publish({43,6000,0,false});
    if(Get(26)!=1000) return fail(__LINE__);
    Publish({30,10000,0,false});
    if(Get(26)!=1000) return fail(__LINE__);
    Publish({43,12000,0,false});
    if(Get(26)!=0) return fail(__LINE__);
    Publish({30,7500,0,false});
    if(Get(26)!=500) return fail(__LINE__);
    // Position 53 is the factory apostrophe key, including release publication.
    if (std::find(kPositions.begin(), kPositions.end(), 53) == kPositions.end()) return fail(__LINE__);
    g_hidAt[53].store(0x34); g_has[0x34].store(true); g_physical.Bind(53,0x34);
    Publish({53,10000,0,false}); Publish({53,5000,0,false});
    if (!Owns(0x34) || Get(0x34)!=1000) return fail(__LINE__);
    Publish({53,10000,0,false});
    if (Get(0x34)!=0) return fail(__LINE__);
    const auto now=GetTickCount64();
    g_physical.Publish(30,1000,now>kFreshMs ? now-kFreshMs-1 : 0);
    g_physical.Publish(43,1000,now>kFreshMs ? now-kFreshMs-1 : 0);
    if(!Owns(26) || Get(26)!=0) return fail(__LINE__);
    g_connected.store(false);Clear();
    if (Owns(26) || Get(26)!=0) return fail(__LINE__);
    std::array<std::uint32_t,256> assignments{};
    for (const auto& key : halljoy::hero84::factory) assignments[key.position]=key.hid;
    assignments[55]=0x00020000; assignments[72]=0x0D000000;
    assignments[30]=4; assignments[43]=4; assignments[53]=0x02000001;
    if (!InstallMap(assignments)) return fail(__LINE__);
    g_connected.store(true);
    halljoy::native_layout::activeToken.store(token);
    if (!Owns(0x409) || !Owns(0xE1) || Owns(0x34)) return fail(__LINE__);
    Publish({72,10000,0,false});Publish({72,5000,0,false});
    if (Get(0x409)!=1000) return fail(__LINE__);
    Publish({53,10000,0,false});Publish({53,5000,0,false});
    Publish({30,10000,0,false});Publish({30,5000,0,false});
    if (Get(4)!=1000 || Owns(26)) return fail(__LINE__);
    halljoy::native_layout::activeToken.store(0);
    if (Get(26)!=1000) return fail(__LINE__);
    if (Get(4)!=0) return fail(__LINE__);
    if (Get(0x34)!=1000) return fail(__LINE__);
    if (!halljoy::native_layout::Read(token).complete) return fail(__LINE__);
    std::array<bool,256> requested{};
    for (const auto& key:halljoy::hero84::factory) if (!Owns(key.hid)) return fail(__LINE__);
    for (unsigned pass=0;pass<255;++pass) {
        std::array<std::uint16_t,hero::kMaxPositions> request{};
        const auto count=Plan(&request);
        for (std::size_t i=0;i<count;++i) requested[request[i]]=true;
    }
    for (const auto& key:halljoy::hero84::factory) if (!requested[key.position]) return fail(__LINE__);

    for (unsigned bit=0;bit<8;++bit)
        if (halljoy::hero84::DecodeAssignment(0x10000u<<bit)!=0xE0+bit) return fail(__LINE__);
    if (halljoy::hero84::DecodeAssignment(0x30000)!=0 || halljoy::hero84::DecodeAssignment(0x01000004)!=0) return fail(__LINE__);
    g_connected.store(false);Clear();
    for(const auto& model:halljoy::hero_family::models) {
        g_model=&model;
        assignments.fill(0);
        for(const auto& key:model.keys) {
            assignments[key.position]=key.hid==0x409 ? 0x0D000000 : key.hid;
            if(key.hid>=224 && key.hid<=231) assignments[key.position]=0x10000u<<(key.hid-224);
        }
        if(!InstallMap(assignments))return fail(__LINE__);
        g_connected.store(true);halljoy::native_layout::activeToken.store(model.token);
        if(!halljoy::native_layout::Read(model.token).complete || g_mapped.load()!=model.keys.size()) return fail(__LINE__);
        for(const auto& key:model.keys) {
            if(!Owns(key.hid))return fail(__LINE__);
            Publish({key.position,10000,0,false});Publish({key.position,5000,0,false});
            if(Get(key.hid)!=1000)return fail(__LINE__);
            Publish({key.position,10000,0,false});if(Get(key.hid)!=0)return fail(__LINE__);
        }
        Publish({30,7500,0,false});Publish({43,5000,0,false});
        halljoy::configured_xusb::PadConfiguration config{};config.axes[0]={4,7};config.axes[1]={22,26};
        halljoy::configured_xusb::InputValues input{};
        for(unsigned hid:{4u,7u,22u,26u})input.filtered[hid]=Get(static_cast<std::uint16_t>(hid))/1000.0f;
        halljoy::configured_xusb::BuilderState state{};
        const auto frame=halljoy::configured_xusb::BuildReport(config,input,state);
        if(frame.leftStickX!=-32767 || frame.leftStickY!=16384)return fail(__LINE__);
        g_connected.store(false);Clear();
    }
    g_model=&halljoy::hero_family::models[0];kFactoryAt=FactoryMap();
    return !Owns(0x409) && !halljoy::native_layout::Read(token).complete;
}
