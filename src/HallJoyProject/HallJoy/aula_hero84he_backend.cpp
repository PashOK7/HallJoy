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
constexpr std::array<std::uint8_t, 6> kExpectedUuid{{0x11, 0, 0, 0, 0, 0x05}};
// AULA_2829's complete default physical layout.  Live layer-0 `83`, not this
// list, determines which standard HID usages become ownable.
constexpr std::array<std::uint16_t, 83> kPositions{
    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,  25,  26,  27, 28,
     29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,  54,  55,  56, 57,
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
std::array<std::atomic<std::uint8_t>, 256> g_hidAt{};
std::array<std::atomic<bool>, 256> g_has{};
std::array<std::atomic<ULONGLONG>, 256> g_demand{}, g_sample{};
std::array<std::atomic<std::uint16_t>, 256> g_milli{}, g_top{}, g_bottom{};
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
    g_mapped.store(0);
    g_last.store(0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        g_hidAt[i].store(0);
        g_has[i].store(false);
        g_milli[i].store(0);
        g_top[i].store(0);
        g_bottom[i].store(0);
        g_sample[i].store(0);
    }
}
bool Identity(Session &s)
{
    hero::Report q{}, r{};
    std::uint32_t us = 0;
    std::array<std::uint8_t, 6> uuid{};
    return hero::BuildIdentityRead(&q) && s.Exchange(q, &r, &us) && hero::ParseIdentityResponse(r, &uuid) &&
           uuid == kExpectedUuid;
}
bool Map(Session &s)
{
    std::array<std::uint8_t, 256> nextAt{};
    std::array<bool, 256> nextHas{};
    std::uint32_t count = 0;
    for (std::size_t base = 0; base < kPositions.size(); base += hero::kMaxPositions)
    {
        const std::size_t n = std::min(hero::kMaxPositions, kPositions.size() - base);
        hero::Report q{}, r{};
        std::uint32_t us = 0;
        std::array<hero::Assignment, hero::kMaxPositions> values{};
        if (!hero::BuildAssignmentRead(0, kPositions.data() + base, n, &q) || !s.Exchange(q, &r, &us) ||
            !hero::ParseAssignmentResponse(r, 0, kPositions.data() + base, n, &values))
            return false;
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto &a = values[i];
            const auto hid = static_cast<std::uint8_t>(a.value & 0xffu);
            if ((a.value & 0xffffff00u) != 0 || !hid || hid > 0xe7 || nextHas[hid])
            {
                DebugLog_Write(L"[aula.hero84.production.map] skip pos=%04X assignment=%08X", a.position, a.value);
                continue;
            }
            nextAt[a.position] = hid;
            nextHas[hid] = true;
            ++count;
        }
    }
    if (!count)
        return false;
    Clear();
    for (std::size_t i = 0; i < 256; ++i)
    {
        g_hidAt[i].store(nextAt[i]);
        g_has[i].store(nextHas[i]);
    }
    g_mapped.store(count);
    g_mapReady.store(true);
    DebugLog_Write(L"[aula.hero84.production.map] accepted layer=0 mapped=%u "
                   L"source=read_only_83",
                   count);
    return true;
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
        const auto hid = g_hidAt[p].load();
        const auto demand = hid ? g_demand[hid].load() : 0;
        if (demand && now >= demand && now - demand <= kDemandMs)
            add(static_cast<std::uint16_t>(p));
    }
    return n;
}
void Publish(const hero::DirectSample &x)
{
    const auto hid = g_hidAt[x.position].load();
    if (!hid || !g_has[hid].load() || !x.current)
        return;
    const auto raw = x.current;
    auto top = g_top[hid].load();
    if (!top || raw > top)
    {
        top = raw;
        g_top[hid].store(top);
    }
    auto bottom = g_bottom[hid].load();
    if (!bottom || raw < bottom)
    {
        bottom = raw;
        g_bottom[hid].store(bottom);
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
    g_milli[hid].store(milli);
    g_sample[hid].store(GetTickCount64());
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
    Clear();
    g_connected.store(false);
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
    Clear();
    g_connected.store(false);
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
    if (!hid || hid >= 256 || !Connected())
        return false;
    g_demand[hid].store(GetTickCount64());
    const auto sample = g_sample[hid].load(), now = GetTickCount64();
    return g_has[hid].load() && sample && now >= sample && now - sample <= kFreshMs;
}
std::uint16_t Get(std::uint16_t hid)
{
    return Owns(hid) ? g_milli[hid].load() : 0;
}
void Telemetry(NativeAnalogBackendTelemetry *out)
{
    if (!out)
        return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
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
                 L"AULA HERO84 HE experimental: 82/01 + 83 + read-only 94/02; "
                 L"adaptive observed range");
}
} // namespace

const NativeAnalogBackendDescriptor &AulaHero84He_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion,
                                                 sizeof(NativeAnalogBackendDescriptor),
                                                 "aula-hero84he-9402-experimental",
                                                 L"AULA HERO84 HE (experimental native analogue)",
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
