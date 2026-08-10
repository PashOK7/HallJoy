#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "aula_w669_backend.h"
#include "aula_w669_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_join_policy.h"
#include "worker_exception_barrier.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr std::uint16_t kKnownVendor = 0x2e3c;
constexpr std::uint16_t kUsagePage = 0xff1b;
constexpr std::uint16_t kUsage = 0x0091;
constexpr DWORD kIoTimeoutMs = 120;
constexpr DWORD kIdentityTimeoutMs = 400;
constexpr DWORD kProofTimeoutMs = 1200;
constexpr DWORD kReconnectMs = 1000;
constexpr DWORD kStopTimeoutMs = 3000;

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other)
        {
            if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};

struct Candidate
{
    std::wstring path;
    std::wstring manufacturer;
    std::wstring product;
    std::wstring serial;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
};

struct Proof
{
    aula_w669::DeviceInfo deviceInfo{};
    aula_w669::FactoryLayoutProfile factoryProfile =
        aula_w669::FactoryLayoutProfile::Unknown;
    bool firmwareIdentity = false;
    aula_w669::TravelInfo travel{};
    aula_w669::PositionToHid map{};
    std::size_t mapped = 0;
    bool useControlWrite = false;
    bool exclusive = false;
};

std::atomic<bool> g_prepared{ false }, g_running{ false }, g_stop{ false };
std::atomic<bool> g_present{ false }, g_connected{ false };
std::mutex g_serviceMutex, g_routeMutex, g_handleMutex, g_signalMutex;
std::vector<std::uint16_t> g_routedPids;
HANDLE g_thread = nullptr, g_wake = nullptr, g_activeHandle = INVALID_HANDLE_VALUE;
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint8_t>, 256> g_owned{};
std::atomic<std::uint16_t> g_vid{ 0 }, g_pid{ 0 }, g_maxTravel{ 0 };
std::atomic<std::uint32_t> g_inputBytes{ 0 }, g_outputBytes{ 0 }, g_mapped{ 0 };
std::atomic<std::uint32_t> g_active{ 0 }, g_hz10{ 0 }, g_avgUs{ 0 }, g_maxUs{ 0 };
std::atomic<std::uint64_t> g_updates{ 0 }, g_failures{ 0 }, g_lastMs{ 0 };
std::atomic<std::uint64_t> g_liveEvents{ 0 };
std::atomic<halljoy::worker::WorkerExceptionKind> g_fault{ halljoy::worker::WorkerExceptionKind::None };

std::uint64_t NowUs()
{
    static const std::uint64_t frequency = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    LARGE_INTEGER n{}; QueryPerformanceCounter(&n);
    return static_cast<std::uint64_t>(n.QuadPart) * 1000000ull / frequency;
}

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) { hash ^= static_cast<std::uint16_t>(towlower(ch)); hash *= 1099511628211ull; }
    return hash;
}

std::wstring HidString(HANDLE h, BOOLEAN (__stdcall *getter)(HANDLE, PVOID, ULONG))
{
    wchar_t text[256]{};
    return getter(h, text, sizeof(text)) ? std::wstring(text) : std::wstring();
}

void TraceReport(const wchar_t* direction, const std::uint8_t* bytes, std::size_t count)
{
#if defined(HALLJOY_DIAGNOSTIC)
    wchar_t hex[aula_w669::kReportBytes * 3 + 1]{};
    std::size_t used = 0;
    for (std::size_t i = 0; i < std::min(count, aula_w669::kReportBytes); ++i)
        used += static_cast<std::size_t>(_snwprintf_s(hex + used, _countof(hex) - used,
            _TRUNCATE, L"%02X%ls", bytes[i], i + 1 == count ? L"" : L" "));
    DebugLog_WriteBuffered(L"[aula.w669.raw] dir=%ls bytes=%llu data=%ls", direction,
        static_cast<unsigned long long>(count), hex);
#else
    (void)direction; (void)bytes; (void)count;
#endif
}

bool TimedIo(HANDLE h, bool write, void* data, DWORD bytes, DWORD timeout, DWORD* transferred)
{
    if (transferred) *transferred = 0;
    HidIoOperation io(h);
    DWORD error = 0;
    const auto start = write ? io.StartWrite(data, bytes, &error) : io.StartRead(data, bytes, &error);
    if (start == HidIoOperation::StartResult::Failed)
    {
        SetLastError(error);
        return false;
    }
    if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = io.Wait(timeout);
        if (wait == WAIT_OBJECT_0)
        {
            const bool ok = io.Finish(transferred, &error, false);
            if (!ok) SetLastError(error);
            return ok;
        }

        // CancelAndDrain normally ends a timed-out request with
        // ERROR_OPERATION_ABORTED.  That is the cancellation result, not the
        // reason the caller stopped waiting.  Preserve WAIT_TIMEOUT so an
        // ordinary event-stream idle period is not reported as a transport
        // failure.
        const DWORD waitError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        io.CancelAndDrain(transferred, &error);
        SetLastError(waitError ? waitError : ERROR_GEN_FAILURE);
        return false;
    }
    const bool ok = io.Finish(transferred, &error, false);
    if (!ok) SetLastError(error);
    return ok;
}

std::vector<Candidate> Enumerate(bool routedOnly, bool verbose)
{
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    std::vector<Candidate> result;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &iface))
        { if (GetLastError() == ERROR_NO_MORE_ITEMS) break; continue; }
        DWORD needed = 0; SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, nullptr, nullptr)) continue;
        const bool routed = NativeAnalogRouting_IsClaimedBy(detail->DevicePath, NativeAnalogProtocol::AulaW669);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !routed) continue;
        if (routedOnly && !routed) continue;
        Handle meta(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!meta) continue;
        Candidate c{}; c.path = detail->DevicePath; c.attributes.Size = sizeof(c.attributes);
        if (!HidD_GetAttributes(meta.value, &c.attributes)) continue;
        PHIDP_PREPARSED_DATA pp = nullptr;
        if (!HidD_GetPreparsedData(meta.value, &pp)) continue;
        const NTSTATUS status = HidP_GetCaps(pp, &c.caps); HidD_FreePreparsedData(pp);
        if (status != HIDP_STATUS_SUCCESS) continue;
        const bool protocolShape = c.caps.UsagePage == kUsagePage && c.caps.Usage == kUsage &&
            c.caps.InputReportByteLength >= aula_w669::kReportBytes &&
            c.caps.OutputReportByteLength >= aula_w669::kReportBytes;
        if (verbose && (c.attributes.VendorID == kKnownVendor || protocolShape))
        {
            c.manufacturer = HidString(meta.value, HidD_GetManufacturerString);
            c.product = HidString(meta.value, HidD_GetProductString);
            c.serial = HidString(meta.value, HidD_GetSerialNumberString);
            DebugLog_Write(L"[aula.w669.enumeration] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u manufacturer=%ls product=%ls serial_present=%d serial_chars=%llu protocol_shape=%d",
                static_cast<unsigned long long>(HashPath(c.path)), c.attributes.VendorID,
                c.attributes.ProductID, c.attributes.VersionNumber, c.caps.UsagePage, c.caps.Usage,
                c.caps.InputReportByteLength, c.caps.OutputReportByteLength, c.caps.FeatureReportByteLength,
                c.manufacturer.empty() ? L"-" : c.manufacturer.c_str(),
                c.product.empty() ? L"-" : c.product.c_str(), c.serial.empty() ? 0 : 1,
                static_cast<unsigned long long>(c.serial.size()), protocolShape ? 1 : 0);
        }
        if (protocolShape) result.push_back(std::move(c));
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

class Session
{
public:
    Session(const Candidate& c, bool exclusive, bool control)
        : candidate_(c), exclusive_(exclusive), control_(control) {}
    bool Open()
    {
        const DWORD share = exclusive_ ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
        handle_ = Handle(CreateFileW(candidate_.path.c_str(), GENERIC_READ | GENERIC_WRITE, share,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false;
        HidD_SetNumInputBuffers(handle_.value, 256);
        return true;
    }
    bool Send(const aula_w669::Report& report)
    {
        TraceReport(L"TX", report.data(), report.size());
        if (control_)
        {
            auto copy = report;
            const bool ok = HidD_SetOutputReport(handle_.value, copy.data(), static_cast<ULONG>(copy.size())) != FALSE;
            if (!ok) g_failures.fetch_add(1, std::memory_order_relaxed);
            return ok;
        }
        auto copy = report; DWORD sent = 0;
        const bool ok = TimedIo(handle_.value, true, copy.data(), static_cast<DWORD>(copy.size()), kIoTimeoutMs, &sent) && sent == copy.size();
        if (!ok) g_failures.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }
    bool Read(aula_w669::Report* out, DWORD timeout)
    {
        if (!out) return false; out->fill(0); DWORD got = 0;
        if (!TimedIo(handle_.value, false, out->data(), static_cast<DWORD>(out->size()), timeout, &got)) return false;
        if (got < out->size()) { SetLastError(ERROR_BAD_LENGTH); return false; }
        TraceReport(L"RX", out->data(), got); return true;
    }
    HANDLE Native() const { return handle_.value; }
private:
    Candidate candidate_; bool exclusive_ = false, control_ = false; Handle handle_{};
};

bool ReceiveTravel(Session& s, aula_w669::TravelInfo* out)
{
    if (!s.Send(aula_w669::BuildTravelInfoRequest())) return false;
    const auto deadline = GetTickCount64() + kProofTimeoutMs; aula_w669::Report r{};
    while (GetTickCount64() < deadline)
        if (s.Read(&r, 100) && aula_w669::DecodeTravelInfo(r.data(), r.size(), out)) return true;
    return false;
}

const wchar_t* ProfileName(aula_w669::FactoryLayoutProfile profile)
{
    switch (profile)
    {
    case aula_w669::FactoryLayoutProfile::Si2825Win60: return L"si2825_win60_61";
    case aula_w669::FactoryLayoutProfile::Si2828Win68: return L"si2828_win68_68";
    case aula_w669::FactoryLayoutProfile::Si2851KpTe153Uk: return L"si2851_kp_te153_uk_69";
    case aula_w669::FactoryLayoutProfile::K673Br: return L"redragon_k673_br_81";
    case aula_w669::FactoryLayoutProfile::K673Uk: return L"redragon_k673_uk_81";
    case aula_w669::FactoryLayoutProfile::K673Us: return L"redragon_k673_us_80";
    default: return L"unknown_explicit_only";
    }
}

aula_w669::FactoryLayoutProfile DescriptorFallbackProfile(
    const Candidate& candidate)
{
    std::wstring product = candidate.product;
    while (!product.empty() && iswspace(product.back())) product.pop_back();
    while (!product.empty() && iswspace(product.front())) product.erase(product.begin());
    std::transform(product.begin(), product.end(), product.begin(), towupper);
    if (product == L"WIN 60 HE")
        return aula_w669::FactoryLayoutProfile::Si2825Win60;
    if (product == L"WIN 68 HE")
        return aula_w669::FactoryLayoutProfile::Si2828Win68;
    if (product == L"KP-TE153")
        return aula_w669::FactoryLayoutProfile::Si2851KpTe153Uk;
    return aula_w669::FactoryLayoutProfile::Unknown;
}

bool ResolveFactoryProfile(Session& s, const Candidate& candidate,
    aula_w669::DeviceInfo* info,
    aula_w669::FactoryLayoutProfile* profile,
    bool* firmwareIdentity)
{
    if (!info || !profile || !firmwareIdentity) return false;
    *info = {};
    *profile = aula_w669::FactoryLayoutProfile::Unknown;
    *firmwareIdentity = false;

    if (s.Send(aula_w669::BuildDeviceInfoRequest()))
    {
        const auto deadline = GetTickCount64() + kIdentityTimeoutMs;
        aula_w669::Report report{};
        while (GetTickCount64() < deadline)
        {
            if (!s.Read(&report, 80)) continue;
            if (!aula_w669::DecodeDeviceInfo(
                    report.data(), report.size(), info))
                continue;
            *firmwareIdentity = true;
            *profile = aula_w669::FactoryProfileForProduct(
                info->product.data());
            DebugLog_Write(L"[aula.w669.identity] source=read_only_0x0D firmware_product=%hs profile=%ls",
                info->product.data(), ProfileName(*profile));
            return true;
        }
    }

    *profile = DescriptorFallbackProfile(candidate);
    DebugLog_Write(L"[aula.w669.identity] source=hid_product_fallback product=%ls profile=%ls firmware_identity_timeout=1",
        candidate.product.empty() ? L"-" : candidate.product.c_str(),
        ProfileName(*profile));
    return true;
}

bool ReceiveMap(Session& s, aula_w669::FactoryLayoutProfile profile,
    aula_w669::PositionToHid* map)
{
    if (!map) return false;
    *map = aula_w669::FactoryMap(profile);
    if (!s.Send(aula_w669::BuildKeyMapRequest())) return false;
    std::array<bool, 10> received{}; const auto deadline = GetTickCount64() + kProofTimeoutMs;
    aula_w669::Report r{};
    while (GetTickCount64() < deadline)
    {
        if (!s.Read(&r, 100)) continue;
        aula_w669::DecodeKeyMapFragment(r.data(), r.size(), map, &received);
        if (std::all_of(received.begin(), received.end(), [](bool v) { return v; }))
            return aula_w669::MappedKeyCount(*map) >= 20;
    }
    return false;
}

void QueryPollRate(Session& s)
{
    if (!s.Send(aula_w669::BuildPollRateQuery())) return;
    const auto deadline = GetTickCount64() + 500; aula_w669::Report r{};
    while (GetTickCount64() < deadline)
    {
        if (!s.Read(&r, 100)) continue;
        std::uint8_t code = 0; std::uint16_t hz = 0;
        if (aula_w669::DecodePollRate(r.data(), r.size(), &code, &hz))
        {
            if (hz)
                DebugLog_Write(L"[aula.w669.capability] configured_poll_code=%u nominal_poll_hz=%u source=read_only_0x21_0x0A",
                    code, hz);
            else
                DebugLog_Write(L"[aula.w669.capability] configured_poll_code=0 nominal_poll_hz=unspecified mode=firmware_default source=read_only_0x21_0x0A");
            return;
        }
    }
    DebugLog_Write(L"[aula.w669.capability] configured_poll_rate=unknown response_timeout=1 non_blocking=1");
}

bool Prove(const Candidate& c, Proof* out)
{
    for (bool exclusive : { false, true }) for (bool control : { false, true })
    {
        Session s(c, exclusive, control);
        DebugLog_Write(L"[aula.w669.proof] begin path_hash=%016llX exclusive=%d write=%ls",
            static_cast<unsigned long long>(HashPath(c.path)), exclusive ? 1 : 0,
            control ? L"HidD_SetOutputReport" : L"WriteFile");
        if (!s.Open()) { DebugLog_Write(L"[aula.w669.proof] open_failed win32=%lu", GetLastError()); continue; }
        Proof proof{}; proof.exclusive = exclusive; proof.useControlWrite = control;
        if (!ResolveFactoryProfile(s, c, &proof.deviceInfo,
                &proof.factoryProfile, &proof.firmwareIdentity))
        { DebugLog_Write(L"[aula.w669.proof] identity_failed"); continue; }
        if (!ReceiveTravel(s, &proof.travel)) { DebugLog_Write(L"[aula.w669.proof] travel_failed"); continue; }
        if (!ReceiveMap(s, proof.factoryProfile, &proof.map)) { DebugLog_Write(L"[aula.w669.proof] map_failed"); continue; }
        proof.mapped = aula_w669::MappedKeyCount(proof.map);
        DebugLog_Write(L"[aula.w669.proof] pass max=%u unit=%u format=%u mapped=%llu map_source=%ls identity_source=%ls",
            proof.travel.maximum, proof.travel.unitCode, proof.travel.formatCode,
            static_cast<unsigned long long>(proof.mapped),
            ProfileName(proof.factoryProfile),
            proof.firmwareIdentity ? L"firmware_0x0D" : L"hid_descriptor_fallback");
        if (out) *out = proof; return true;
    }
    return false;
}

void Publish(std::uint8_t row, std::uint8_t column, std::uint16_t travel,
    const Proof& proof, LONGLONG qpc)
{
    const std::size_t position = std::size_t(row) * aula_w669::kColumns + column;
    const std::uint8_t hid = proof.map[position];
    if (hid == 0) return;
    const auto milli = aula_w669::ToMilli(travel, proof.travel.maximum);
    const auto old = g_milli[hid].exchange(milli, std::memory_order_relaxed);
    g_owned[hid].store(1, std::memory_order_relaxed);
    if ((old == 0) != (milli == 0))
    {
        if (milli) g_active.fetch_add(1, std::memory_order_relaxed);
        else g_active.fetch_sub(1, std::memory_order_relaxed);
    }
    g_updates.fetch_add(1, std::memory_order_relaxed); g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
    if (old != milli) RealtimeLoop_NotifyInputChangedAt(qpc);
}

void Clear()
{
    bool changed = false;
    for (auto& v : g_milli) if (v.exchange(0, std::memory_order_relaxed)) changed = true;
    for (auto& v : g_owned) v.store(0, std::memory_order_relaxed);
    g_active.store(0, std::memory_order_relaxed);
    if (changed) RealtimeLoop_NotifyInputChanged();
}

bool Run(const Candidate& c)
{
    Proof proof{};
    if (!Prove(c, &proof)) return false;
    Session s(c, proof.exclusive, proof.useControlWrite); if (!s.Open()) return false;
    aula_w669::DeviceInfo sessionInfo{};
    aula_w669::FactoryLayoutProfile sessionProfile{};
    bool sessionFirmwareIdentity = false;
    const bool resolved = ResolveFactoryProfile(s, c, &sessionInfo,
        &sessionProfile, &sessionFirmwareIdentity);
    const bool identityStable = resolved &&
        sessionProfile == proof.factoryProfile &&
        (!proof.firmwareIdentity ||
            (sessionFirmwareIdentity && std::strcmp(
                sessionInfo.product.data(), proof.deviceInfo.product.data()) == 0));
    if (!identityStable)
    {
        DebugLog_Write(L"[aula.w669.session] identity_changed proof=%ls session=%ls",
            ProfileName(proof.factoryProfile), ProfileName(sessionProfile));
        return false;
    }
    proof.deviceInfo = sessionInfo;
    proof.firmwareIdentity = sessionFirmwareIdentity;
    if (!ReceiveTravel(s, &proof.travel) ||
        !ReceiveMap(s, proof.factoryProfile, &proof.map)) return false;
    { std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = s.Native(); }
    g_vid.store(c.attributes.VendorID); g_pid.store(c.attributes.ProductID); g_maxTravel.store(proof.travel.maximum);
    g_inputBytes.store(c.caps.InputReportByteLength); g_outputBytes.store(c.caps.OutputReportByteLength);
    g_mapped.store(static_cast<std::uint32_t>(aula_w669::MappedKeyCount(proof.map)));
    QueryPollRate(s);
    std::array<std::uint8_t, aula_w669::kColumns> mask{};
    for (std::size_t pos = 0; pos < proof.map.size(); ++pos)
        if (proof.map[pos]) mask[pos % aula_w669::kColumns] |= static_cast<std::uint8_t>(1u << (pos / aula_w669::kColumns));
    Clear();
    if (!s.Send(aula_w669::BuildSubscriptionRequest(mask))) return false;
    g_connected.store(true, std::memory_order_release);
    DebugLog_Write(L"[aula.w669.session] connected vid=%04X pid=%04X firmware_product=%hs profile=%ls max=%u mapped=%u mode=%ls exclusive=%d strategy=live_subscription_only snapshot_publish=disabled",
        c.attributes.VendorID, c.attributes.ProductID,
        proof.deviceInfo.product.data(), ProfileName(proof.factoryProfile),
        proof.travel.maximum, g_mapped.load(),
        proof.useControlWrite ? L"control" : L"write", proof.exclusive ? 1 : 0);
    std::uint64_t previousUs = 0, intervalSum = 0, intervalCount = 0, maxInterval = 0;
    std::uint64_t lastRollup = GetTickCount64(), lastRollupLive = g_liveEvents.load();
#if defined(HALLJOY_DIAGNOSTIC)
    std::array<std::uint64_t, 256> diagnosticEvents{};
    std::array<std::uint64_t, 256> diagnosticReleases{};
    std::array<std::uint16_t, 256> diagnosticMin{};
    std::array<std::uint16_t, 256> diagnosticMax{};
    diagnosticMin.fill(0xffff);
    std::uint64_t diagnosticPositiveEdges = 0, diagnosticZeroEdges = 0;
    std::uint32_t diagnosticUnique = 0, diagnosticPeakActive = 0;
    std::uint16_t diagnosticMinPositiveRaw = 0xffff, diagnosticMaxRaw = 0;
    const auto diagnosticStartedMs = GetTickCount64();
    const auto diagnosticStartedLive = g_liveEvents.load(std::memory_order_relaxed);
#endif
    while (!g_stop.load(std::memory_order_acquire))
    {
        aula_w669::Report r{};
        const bool received = s.Read(&r, 100);
        if (!received)
        {
            const DWORD readError = GetLastError();
            const bool expectedStopCancellation =
                g_stop.load(std::memory_order_acquire) &&
                (readError == ERROR_OPERATION_ABORTED || readError == ERROR_INVALID_HANDLE);
            if (expectedStopCancellation)
                StabilityTrace_Write(L"INFO", L"aula-w669", L"protocol.cancelled",
                    L"operation=read win32=%lu reason=stop", readError);
            else if (readError != WAIT_TIMEOUT)
                g_failures.fetch_add(1, std::memory_order_relaxed);
        }
        aula_w669::LiveEvent event{};
        if (received && aula_w669::DecodeLiveEvent(r.data(), r.size(), &event))
        {
            const auto nowUs = NowUs();
            if (previousUs && nowUs > previousUs) { const auto d = nowUs - previousUs; intervalSum += d; ++intervalCount; maxInterval = std::max(maxInterval, d); }
            previousUs = nowUs; LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
#if defined(HALLJOY_DIAGNOSTIC)
            const std::size_t position = std::size_t(event.row) * aula_w669::kColumns + event.column;
            const std::uint8_t diagnosticHid = proof.map[position];
            const std::uint16_t diagnosticOld = diagnosticHid ?
                g_milli[diagnosticHid].load(std::memory_order_relaxed) : 0;
#endif
            Publish(event.row, event.column, event.travel, proof, qpc.QuadPart);
            g_liveEvents.fetch_add(1, std::memory_order_relaxed);
#if defined(HALLJOY_DIAGNOSTIC)
            if (diagnosticHid)
            {
                const auto diagnosticMilli = aula_w669::ToMilli(event.travel, proof.travel.maximum);
                ++diagnosticEvents[diagnosticHid];
                if (event.travel && diagnosticMax[diagnosticHid] == 0) ++diagnosticUnique;
                diagnosticMin[diagnosticHid] = std::min(diagnosticMin[diagnosticHid], event.travel);
                diagnosticMax[diagnosticHid] = std::max(diagnosticMax[diagnosticHid], event.travel);
                diagnosticMaxRaw = std::max(diagnosticMaxRaw, event.travel);
                if (event.travel)
                    diagnosticMinPositiveRaw = std::min(diagnosticMinPositiveRaw, event.travel);
                if (diagnosticOld == 0 && diagnosticMilli != 0) ++diagnosticPositiveEdges;
                if (diagnosticOld != 0 && diagnosticMilli == 0)
                {
                    ++diagnosticZeroEdges;
                    ++diagnosticReleases[diagnosticHid];
                }
                diagnosticPeakActive = std::max(diagnosticPeakActive, g_active.load(std::memory_order_relaxed));
            }
#endif
        }
        const auto nowMs = GetTickCount64();
        if (nowMs - lastRollup >= 1000)
        {
            const auto total = g_updates.load(); const auto live = g_liveEvents.load();
            const auto delta = live - lastRollupLive;
            g_hz10.store(static_cast<std::uint32_t>(delta * 10000ull / std::max<ULONGLONG>(1, nowMs - lastRollup)));
            if (intervalCount) { g_avgUs.store(static_cast<std::uint32_t>(intervalSum / intervalCount)); g_maxUs.store(static_cast<std::uint32_t>(std::min<std::uint64_t>(maxInterval, 0xffffffffull))); }
            DebugLog_WriteBuffered(L"[aula.w669.telemetry] live_event_hz=%.1f live_events=%llu publications=%llu active=%u mapped=%u avg_event_interval_us=%u max_event_interval_us=%u failures=%llu last_age_ms=%llu",
                g_hz10.load() / 10.0, static_cast<unsigned long long>(live),
                static_cast<unsigned long long>(total), g_active.load(), g_mapped.load(),
                g_avgUs.load(), g_maxUs.load(), static_cast<unsigned long long>(g_failures.load()),
                static_cast<unsigned long long>(g_lastMs.load() ? nowMs - g_lastMs.load() : 0));
#if defined(HALLJOY_DIAGNOSTIC)
            DebugLog_WriteBuffered(L"[aula.w669.diagnostic] elapsed_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu min_positive_raw=%u max_raw=%u active_now=%u",
                static_cast<unsigned long long>(nowMs - diagnosticStartedMs), diagnosticUnique,
                diagnosticPeakActive, static_cast<unsigned long long>(diagnosticPositiveEdges),
                static_cast<unsigned long long>(diagnosticZeroEdges),
                diagnosticMinPositiveRaw == 0xffff ? 0 : diagnosticMinPositiveRaw,
                diagnosticMaxRaw, g_active.load(std::memory_order_relaxed));
#endif
            lastRollup = nowMs; lastRollupLive = live;
        }
    }
#if defined(HALLJOY_DIAGNOSTIC)
    DebugLog_Write(L"[aula.w669.session_summary] duration_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu min_positive_raw=%u max_raw=%u active_at_stop=%u session_live_events=%llu failures=%llu",
        static_cast<unsigned long long>(GetTickCount64() - diagnosticStartedMs),
        diagnosticUnique, diagnosticPeakActive,
        static_cast<unsigned long long>(diagnosticPositiveEdges),
        static_cast<unsigned long long>(diagnosticZeroEdges),
        diagnosticMinPositiveRaw == 0xffff ? 0 : diagnosticMinPositiveRaw,
        diagnosticMaxRaw, g_active.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(g_liveEvents.load(std::memory_order_relaxed) - diagnosticStartedLive),
        static_cast<unsigned long long>(g_failures.load(std::memory_order_relaxed)));
    for (std::size_t hid = 0; hid < diagnosticEvents.size(); ++hid)
        if (diagnosticEvents[hid])
            DebugLog_Write(L"[aula.w669.coverage] hid=%02X events=%llu releases=%llu min_raw=%u max_raw=%u final_milli=%u",
                static_cast<unsigned>(hid),
                static_cast<unsigned long long>(diagnosticEvents[hid]),
                static_cast<unsigned long long>(diagnosticReleases[hid]),
                diagnosticMin[hid], diagnosticMax[hid],
                g_milli[hid].load(std::memory_order_relaxed));
#endif
    s.Send(aula_w669::BuildUnsubscribeRequest());
    { std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = INVALID_HANDLE_VALUE; }
    return true;
}

std::uint32_t WorkerBody()
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        const auto candidates = Enumerate(true, true); g_present.store(!candidates.empty());
        bool ran = false; for (const auto& c : candidates) if ((ran = Run(c))) break;
        g_connected.store(false); { std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = INVALID_HANDLE_VALUE; }
        Clear(); if (g_stop.load()) break;
        WaitForSingleObject(g_wake, ran ? 200 : kReconnectMs); ResetEvent(g_wake);
    }
    g_present.store(false); g_running.store(false); return 0;
}

void WorkerFault(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_fault.store(record.kind, std::memory_order_release); g_stop.store(true);
    g_connected.store(false); g_present.store(false); Clear();
    StabilityTrace_WriteCritical(L"ERROR", L"aula-w669", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerComplete(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"aula-w669", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

unsigned __stdcall Worker(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerBody(); }, WorkerFault, WorkerComplete, 0xE0669001u));
}

bool Prepare()
{
    if (g_prepared.load()) { std::lock_guard<std::mutex> lock(g_routeMutex); return !g_routedPids.empty(); }
    auto candidates = Enumerate(false, true); std::vector<std::uint16_t> routed;
    for (const auto& c : candidates)
    {
        Proof proof{};
        if (Prove(c, &proof) && NativeAnalogRouting_Claim(c.attributes.VendorID, c.attributes.ProductID,
            c.path.c_str(), NativeAnalogProtocol::AulaW669)) routed.push_back(c.attributes.ProductID);
    }
    g_present.store(!routed.empty(), std::memory_order_release);
    { std::lock_guard<std::mutex> lock(g_routeMutex); g_routedPids = std::move(routed); }
    g_prepared.store(true); return !g_routedPids.empty();
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_prepared.load()) Prepare();
    { std::lock_guard<std::mutex> route(g_routeMutex); if (g_routedPids.empty()) return false; }
    if (g_thread) return g_running.load(); g_stop.store(false); g_running.store(true);
    g_fault.store(halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
    g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr); if (!g_wake) { g_running.store(false); return false; }
    unsigned id = 0; g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &id));
    if (!g_thread) { CloseHandle(g_wake); g_wake = nullptr; g_running.store(false); return false; }
    StabilityTrace_Write(L"INFO", L"aula-w669", L"start.ok", L"thread_id=%u", id); return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true); if (g_wake) SetEvent(g_wake);
    { std::lock_guard<std::mutex> active(g_handleMutex); if (g_activeHandle != INVALID_HANDLE_VALUE) CancelIoEx(g_activeHandle, nullptr); }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0) return halljoy::lifecycle::ObserveWorkerJoin(generation,
        wait == WAIT_TIMEOUT ? halljoy::lifecycle::JoinWaitStatus::TimedOut : halljoy::lifecycle::JoinWaitStatus::Failed,
        wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread); g_thread = nullptr; if (g_wake) CloseHandle(g_wake); g_wake = nullptr;
    g_running.store(false); g_connected.store(false); Clear(); return NativeAnalogBackendStopJoined(generation);
}

void Notify() { std::lock_guard<std::mutex> lock(g_signalMutex); if (g_wake) SetEvent(g_wake); }
bool Present() { if (!g_prepared.load()) Prepare(); return g_present.load(); }
bool Connected() { return g_connected.load(); }
bool Owns(std::uint16_t hid) { return hid < g_owned.size() && g_connected.load() && g_owned[hid].load(); }
std::uint16_t Get(std::uint16_t hid) { return Owns(hid) ? g_milli[hid].load() : 0; }
void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return; *out = {}; out->present = g_present.load(); out->connected = g_connected.load();
    out->vendorId = g_vid.load(); out->productId = g_pid.load(); out->usagePage = kUsagePage; out->usage = kUsage;
    out->mappedKeys = g_mapped.load(); out->activeKeys = g_active.load(); out->nominalRawLevels = g_maxTravel.load() + 1u;
    out->inputReportBytes = g_inputBytes.load(); out->outputReportBytes = g_outputBytes.load(); out->updateHz10 = g_hz10.load();
    out->averageIntervalUs = g_avgUs.load(); out->maximumIntervalUs = g_maxUs.load(); out->successfulUpdates = g_updates.load(); out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64(); out->lastUpdateAgeMs = last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now-last, 0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE, L"W669 adaptive stream, %u mapped, max=%u", out->mappedKeys, g_maxTravel.load());
}
}

const NativeAnalogBackendDescriptor& AulaW669_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor d{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), "aula-w669-adaptive",
        L"AULA W669 adaptive", NativeAnalogProtocol::AulaW669, NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_StreamTransport | NativeAnalogBackendFlag_ReadOnlyProbe |
            NativeAnalogBackendFlag_ReversibleControlProbe | NativeAnalogBackendFlag_DynamicVidPid,
        &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry };
    return d;
}
