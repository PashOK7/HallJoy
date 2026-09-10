#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "irok_nd75_backend.h"
#include "irok_nd75_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"

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
constexpr std::uint16_t kVendorId = 0x0416;
constexpr std::uint16_t kProductId = 0x7372;
constexpr std::uint16_t kUsagePage = 0xff1b;
constexpr std::uint16_t kUsage = 0x0091;
constexpr DWORD kIoTimeoutMs = 120;
constexpr DWORD kProofTimeoutMs = 1200;
constexpr DWORD kReconnectMs = 1000;
constexpr DWORD kStopTimeoutMs = 3000;

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value)
    {
        other.value = INVALID_HANDLE_VALUE;
    }
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
    explicit operator bool() const
    {
        return value && value != INVALID_HANDLE_VALUE;
    }
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
    irok_nd75::DeviceInfo identity{};
    irok_nd75::CapabilityInfo capability{};
    irok_nd75::PositionToHid map = irok_nd75::FactoryMap();
    bool useControlWrite = false;
    bool exclusive = false;
};

std::atomic<bool> g_prepared{ false }, g_running{ false }, g_stop{ false };
std::atomic<bool> g_present{ false }, g_connected{ false };
std::mutex g_serviceMutex, g_handleMutex, g_signalMutex;
HANDLE g_thread = nullptr, g_wake = nullptr;
HANDLE g_activeHandle = INVALID_HANDLE_VALUE;
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint8_t>, 256> g_owned{};
std::atomic<std::uint32_t> g_inputBytes{ 0 }, g_outputBytes{ 0 };
std::atomic<std::uint32_t> g_active{ 0 }, g_hz10{ 0 };
std::atomic<std::uint32_t> g_avgUs{ 0 }, g_maxUs{ 0 };
std::atomic<std::uint64_t> g_updates{ 0 }, g_failures{ 0 }, g_lastMs{ 0 };
std::atomic<std::uint64_t> g_liveEvents{ 0 }, g_outOfRange{ 0 };
std::atomic<halljoy::worker::WorkerExceptionKind> g_fault{
    halljoy::worker::WorkerExceptionKind::None };

std::uint64_t NowUs()
{
    static const std::uint64_t frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<std::uint64_t>(value.QuadPart);
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (!frequency) return GetTickCount64() * 1000ull;
    const auto ticks = static_cast<std::uint64_t>(now.QuadPart);
    return (ticks / frequency) * 1000000ull +
        ((ticks % frequency) * 1000000ull) / frequency;
}

bool IsDeviceLossError(DWORD error) noexcept
{
    switch (error)
    {
    case ERROR_DEVICE_NOT_CONNECTED:
    case ERROR_INVALID_HANDLE:
    case ERROR_GEN_FAILURE:
    case ERROR_BAD_COMMAND:
    case ERROR_NO_SUCH_DEVICE:
    case ERROR_NOT_READY:
    case ERROR_OPERATION_ABORTED:
        return true;
    default:
        return false;
    }
}

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value)
    {
        hash ^= static_cast<std::uint16_t>(towlower(ch));
        hash *= 1099511628211ull;
    }
    return hash;
}

std::wstring HidString(HANDLE handle,
    BOOLEAN (__stdcall *getter)(HANDLE, PVOID, ULONG))
{
    wchar_t text[256]{};
    return getter(handle, text, sizeof(text)) ? std::wstring(text) : std::wstring();
}

void TraceReport(const wchar_t* direction, const std::uint8_t* bytes,
    std::size_t count)
{
#if defined(HALLJOY_DIAGNOSTIC)
    wchar_t hex[irok_nd75::kReportBytes * 3 + 1]{};
    std::size_t used = 0;
    const auto limit = std::min(count, irok_nd75::kReportBytes);
    for (std::size_t index = 0; index < limit; ++index)
        used += static_cast<std::size_t>(_snwprintf_s(hex + used,
            _countof(hex) - used, _TRUNCATE, L"%02X%ls", bytes[index],
            index + 1 == limit ? L"" : L" "));
    DebugLog_WriteBuffered(L"[irok.nd75.raw] dir=%ls bytes=%llu data=%ls",
        direction, static_cast<unsigned long long>(count), hex);
#else
    (void)direction;
    (void)bytes;
    (void)count;
#endif
}

bool TimedIo(HANDLE handle, bool write, void* data, DWORD bytes,
    DWORD timeout, DWORD* transferred)
{
    if (transferred) *transferred = 0;
    HidIoOperation operation(handle);
    DWORD error = 0;
    const auto start = write
        ? operation.StartWrite(data, bytes, &error)
        : operation.StartRead(data, bytes, &error);
    if (start == HidIoOperation::StartResult::Failed)
    {
        SetLastError(error);
        return false;
    }
    if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = operation.Wait(timeout);
        if (wait == WAIT_OBJECT_0)
        {
            const bool ok = operation.Finish(transferred, &error, false);
            if (!ok) SetLastError(error);
            return ok;
        }
        const DWORD waitError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        operation.CancelAndDrain(transferred, &error);
        SetLastError(waitError ? waitError : ERROR_GEN_FAILURE);
        return false;
    }
    const bool ok = operation.Finish(transferred, &error, false);
    if (!ok) SetLastError(error);
    return ok;
}

std::vector<Candidate> Enumerate(bool routedOnly, bool verbose)
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};

    std::vector<Candidate> result;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index,
                &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0,
            &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
            storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail,
                needed, nullptr, nullptr))
            continue;

        const bool routed = NativeAnalogRouting_IsClaimedBy(
            detail->DevicePath, NativeAnalogProtocol::IrokNd75M484);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !routed)
            continue;
        if (routedOnly && !routed) continue;

        Handle metadata(CreateFileW(detail->DevicePath, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;
        Candidate candidate{};
        candidate.path = detail->DevicePath;
        candidate.attributes.Size = sizeof(candidate.attributes);
        if (!HidD_GetAttributes(metadata.value, &candidate.attributes) ||
            candidate.attributes.VendorID != kVendorId ||
            candidate.attributes.ProductID != kProductId)
            continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        const NTSTATUS status = HidP_GetCaps(preparsed, &candidate.caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) continue;
        const bool protocolShape = candidate.caps.UsagePage == kUsagePage &&
            candidate.caps.Usage == kUsage &&
            candidate.caps.InputReportByteLength == irok_nd75::kReportBytes &&
            candidate.caps.OutputReportByteLength == irok_nd75::kReportBytes;
        if (verbose)
        {
            candidate.manufacturer = HidString(metadata.value,
                HidD_GetManufacturerString);
            candidate.product = HidString(metadata.value, HidD_GetProductString);
            candidate.serial = HidString(metadata.value, HidD_GetSerialNumberString);
            DebugLog_Write(L"[irok.nd75.enumeration] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u manufacturer=%ls product=%ls serial_present=%d protocol_shape=%d routed=%d",
                static_cast<unsigned long long>(HashPath(candidate.path)),
                candidate.attributes.VendorID, candidate.attributes.ProductID,
                candidate.attributes.VersionNumber, candidate.caps.UsagePage,
                candidate.caps.Usage, candidate.caps.InputReportByteLength,
                candidate.caps.OutputReportByteLength,
                candidate.caps.FeatureReportByteLength,
                candidate.manufacturer.empty() ? L"-" : candidate.manufacturer.c_str(),
                candidate.product.empty() ? L"-" : candidate.product.c_str(),
                candidate.serial.empty() ? 0 : 1, protocolShape ? 1 : 0,
                routed ? 1 : 0);
        }
        if (protocolShape) result.push_back(std::move(candidate));
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

class Session
{
public:
    Session(const Candidate& candidate, bool exclusive, bool control)
        : candidate_(candidate), exclusive_(exclusive), control_(control) {}
    ~Session() { ReleaseActive(); }

    bool Open()
    {
        const DWORD share = exclusive_ ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
        handle_ = Handle(CreateFileW(candidate_.path.c_str(),
            GENERIC_READ | GENERIC_WRITE, share, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false;
        HidD_SetNumInputBuffers(handle_.value, 256);
        RegisterActive();
        return true;
    }

    bool Send(const irok_nd75::Report& report)
    {
        TraceReport(L"TX", report.data(), report.size());
        auto copy = report;
        bool ok = false;
        if (control_)
            ok = HidD_SetOutputReport(handle_.value, copy.data(),
                static_cast<ULONG>(copy.size())) != FALSE;
        else
        {
            DWORD sent = 0;
            ok = TimedIo(handle_.value, true, copy.data(),
                static_cast<DWORD>(copy.size()), kIoTimeoutMs, &sent) &&
                sent == copy.size();
        }
        if (!ok) g_failures.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }

    bool Read(irok_nd75::Report* out, DWORD timeout)
    {
        if (!out) return false;
        out->fill(0);
        DWORD received = 0;
        if (!TimedIo(handle_.value, false, out->data(),
                static_cast<DWORD>(out->size()), timeout, &received))
            return false;
        if (received < out->size())
        {
            SetLastError(ERROR_BAD_LENGTH);
            return false;
        }
        TraceReport(L"RX", out->data(), received);
        return true;
    }

    HANDLE Native() const { return handle_.value; }

private:
    void RegisterActive()
    {
        std::lock_guard<std::mutex> lock(g_handleMutex);
        g_activeHandle = handle_.value;
        active_ = true;
    }

    void ReleaseActive()
    {
        if (!active_) return;
        std::lock_guard<std::mutex> lock(g_handleMutex);
        if (g_activeHandle == handle_.value)
            g_activeHandle = INVALID_HANDLE_VALUE;
        active_ = false;
    }

    Candidate candidate_;
    bool exclusive_ = false;
    bool control_ = false;
    bool active_ = false;
    Handle handle_{};
};

bool ReceiveIdentity(Session& session, irok_nd75::DeviceInfo* out)
{
    if (g_stop.load(std::memory_order_acquire)) return false;
    if (!session.Send(irok_nd75::BuildIdentityRequest())) return false;
    const auto deadline = GetTickCount64() + kProofTimeoutMs;
    irok_nd75::Report report{};
    while (!g_stop.load(std::memory_order_acquire) &&
        GetTickCount64() < deadline)
        if (session.Read(&report, 100) && irok_nd75::DecodeDeviceInfo(
                report.data(), report.size(), out))
            return irok_nd75::IsExpectedDevice(*out);
    return false;
}

bool ReceiveCapability(Session& session, irok_nd75::CapabilityInfo* out)
{
    if (g_stop.load(std::memory_order_acquire)) return false;
    if (!session.Send(irok_nd75::BuildCapabilityRequest())) return false;
    const auto deadline = GetTickCount64() + kProofTimeoutMs;
    irok_nd75::Report report{};
    while (!g_stop.load(std::memory_order_acquire) &&
        GetTickCount64() < deadline)
        if (session.Read(&report, 100) && irok_nd75::DecodeCapabilityInfo(
                report.data(), report.size(), out))
            return true;
    return false;
}

bool Prove(const Candidate& candidate, Proof* out)
{
    for (bool exclusive : { false, true })
    {
        for (bool control : { false, true })
        {
            if (g_stop.load(std::memory_order_acquire)) return false;
            Session session(candidate, exclusive, control);
            DebugLog_Write(L"[irok.nd75.proof] begin path_hash=%016llX exclusive=%d write=%ls",
                static_cast<unsigned long long>(HashPath(candidate.path)),
                exclusive ? 1 : 0,
                control ? L"HidD_SetOutputReport" : L"WriteFile");
            if (!session.Open())
            {
                DebugLog_Write(L"[irok.nd75.proof] open_failed win32=%lu",
                    GetLastError());
                continue;
            }
            Proof proof{};
            proof.exclusive = exclusive;
            proof.useControlWrite = control;
            if (!ReceiveIdentity(session, &proof.identity))
            {
                DebugLog_Write(L"[irok.nd75.proof] identity_failed expected=M484/X86HERGB");
                continue;
            }
            if (!ReceiveCapability(session, &proof.capability))
            {
                DebugLog_Write(L"[irok.nd75.proof] capability_failed expected=21/04 firmware=%hs",
                    proof.identity.firmware.data());
                continue;
            }
            const auto mapped = irok_nd75::MappedKeyCount(proof.map);
            if (mapped != 81u)
            {
                DebugLog_Write(L"[irok.nd75.proof] factory_map_invalid mapped=%llu",
                    static_cast<unsigned long long>(mapped));
                continue;
            }
            DebugLog_Write(L"[irok.nd75.proof] pass controller=%hs product=%hs firmware=%hs sensitivity=%u mapped=%llu map=official_x86hergb_6x22 scale=0..40",
                proof.identity.controller.data(), proof.identity.product.data(),
                proof.identity.firmware.data(), proof.capability.sensitivity,
                static_cast<unsigned long long>(mapped));
            if (out) *out = proof;
            return true;
        }
    }
    return false;
}

bool EnsureClaim(const Candidate& candidate, Proof* proof)
{
    if (NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),
            NativeAnalogProtocol::IrokNd75M484))
        return Prove(candidate, proof);
    Proof validated{};
    if (!Prove(candidate, &validated)) return false;
    if (!NativeAnalogRouting_Claim(candidate.attributes.VendorID,
            candidate.attributes.ProductID, candidate.path.c_str(),
            NativeAnalogProtocol::IrokNd75M484))
        return false;
    if (proof) *proof = validated;
    return true;
}

void Clear()
{
    bool changed = false;
    for (auto& value : g_milli)
        if (value.exchange(0, std::memory_order_relaxed)) changed = true;
    for (auto& owned : g_owned) owned.store(0, std::memory_order_relaxed);
    g_active.store(0, std::memory_order_relaxed);
    if (changed) RealtimeLoop_NotifyInputChanged();
}

void PublishOwnership(const irok_nd75::PositionToHid& map)
{
    for (const auto hid : map)
        if (hid != 0) g_owned[hid].store(1, std::memory_order_relaxed);
}

void Publish(const irok_nd75::LiveEvent& event, const Proof& proof,
    LONGLONG qpc)
{
    const std::size_t position = std::size_t(event.row) *
        irok_nd75::kColumns + event.column;
    const std::uint8_t hid = proof.map[position];
    if (hid == 0) return;
    if (event.travel > irok_nd75::kNominalTravelMaximum)
        g_outOfRange.fetch_add(1, std::memory_order_relaxed);
    const auto milli = irok_nd75::ToMilli(event.travel);
    const auto old = g_milli[hid].exchange(milli, std::memory_order_relaxed);
    if ((old == 0) != (milli == 0))
    {
        if (milli) g_active.fetch_add(1, std::memory_order_relaxed);
        else g_active.fetch_sub(1, std::memory_order_relaxed);
    }
    g_updates.fetch_add(1, std::memory_order_relaxed);
    g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
    if (old != milli) RealtimeLoop_NotifyInputChangedAt(qpc);
}

bool Run(const Candidate& candidate, Proof proof)
{
    Session session(candidate, proof.exclusive, proof.useControlWrite);
    if (!session.Open()) return false;
    irok_nd75::DeviceInfo sessionIdentity{};
    irok_nd75::CapabilityInfo sessionCapability{};
    if (!ReceiveIdentity(session, &sessionIdentity) ||
        std::strcmp(sessionIdentity.controller.data(),
            proof.identity.controller.data()) != 0 ||
        std::strcmp(sessionIdentity.product.data(), proof.identity.product.data()) != 0 ||
        !ReceiveCapability(session, &sessionCapability))
    {
        DebugLog_Write(L"[irok.nd75.session] capability_changed_after_claim=1");
        return false;
    }
    proof.identity = sessionIdentity;
    proof.capability = sessionCapability;
    g_inputBytes.store(candidate.caps.InputReportByteLength);
    g_outputBytes.store(candidate.caps.OutputReportByteLength);
    Clear();
    PublishOwnership(proof.map);
    const auto mask = irok_nd75::SubscriptionMask(proof.map);
    if (!session.Send(irok_nd75::BuildSubscriptionRequest(mask)))
    {
        Clear();
        return false;
    }
    g_connected.store(true, std::memory_order_release);
    DebugLog_Write(L"[irok.nd75.session] connected vid=0416 pid=7372 controller=%hs product=%hs firmware=%hs mapped=81 nominal_max=40 sensitivity=%u mode=%ls exclusive=%d strategy=live_subscription_only",
        proof.identity.controller.data(), proof.identity.product.data(),
        proof.identity.firmware.data(), proof.capability.sensitivity,
        proof.useControlWrite ? L"control" : L"write",
        proof.exclusive ? 1 : 0);

    std::uint64_t previousUs = 0, intervalSum = 0, intervalCount = 0;
    std::uint64_t maxInterval = 0, lastRollup = GetTickCount64();
    std::uint64_t lastRollupLive = g_liveEvents.load(std::memory_order_relaxed);
    std::uint32_t consecutiveReadErrors = 0;
    bool transportLost = false;
#if defined(HALLJOY_DIAGNOSTIC)
    std::array<std::uint64_t, 256> diagnosticEvents{};
    std::array<std::uint64_t, 256> diagnosticReleases{};
    std::array<std::uint8_t, 256> diagnosticMin{};
    std::array<std::uint8_t, 256> diagnosticMax{};
    diagnosticMin.fill(0xff);
    std::uint64_t diagnosticPositiveEdges = 0, diagnosticZeroEdges = 0;
    std::uint32_t diagnosticUnique = 0, diagnosticPeakActive = 0;
    const auto diagnosticStartedMs = GetTickCount64();
    const auto diagnosticStartedLive = g_liveEvents.load(std::memory_order_relaxed);
    const auto diagnosticStartedOutOfRange = g_outOfRange.load(std::memory_order_relaxed);
#endif

    while (!g_stop.load(std::memory_order_acquire))
    {
        irok_nd75::Report report{};
        const bool received = session.Read(&report, 100);
        if (!received)
        {
            const DWORD readError = GetLastError();
            const bool expectedStopCancellation =
                g_stop.load(std::memory_order_acquire) &&
                (readError == ERROR_OPERATION_ABORTED ||
                    readError == ERROR_INVALID_HANDLE);
            if (expectedStopCancellation)
            {
                StabilityTrace_Write(L"INFO", L"irok-nd75",
                    L"protocol.cancelled", L"operation=read win32=%lu reason=stop",
                    readError);
                break;
            }
            const bool timedOut = readError == WAIT_TIMEOUT;
            if (timedOut)
            {
                consecutiveReadErrors = 0;
            }
            else
            {
                ++consecutiveReadErrors;
                g_failures.fetch_add(1, std::memory_order_relaxed);
                const bool deviceLost = IsDeviceLossError(readError);
                DebugLog_WriteBuffered(L"[irok.nd75.transport] read_failed win32=%lu consecutive=%u device_lost=%d",
                    readError, consecutiveReadErrors, deviceLost ? 1 : 0);
                if (irok_nd75::ClassifyReadFailure(
                        g_stop.load(std::memory_order_acquire), false,
                        deviceLost, consecutiveReadErrors) ==
                    irok_nd75::ReadFailureAction::EndSession)
                {
                    transportLost = true;
                    break;
                }
                WaitForSingleObject(g_wake, 10);
            }
        }
        else
        {
            consecutiveReadErrors = 0;
        }
        irok_nd75::LiveEvent event{};
        if (received && irok_nd75::DecodeLiveEvent(report.data(),
                report.size(), &event))
        {
            const auto nowUs = NowUs();
            if (previousUs && nowUs > previousUs)
            {
                const auto interval = nowUs - previousUs;
                intervalSum += interval;
                ++intervalCount;
                maxInterval = std::max(maxInterval, interval);
            }
            previousUs = nowUs;
            LARGE_INTEGER qpc{};
            QueryPerformanceCounter(&qpc);
#if defined(HALLJOY_DIAGNOSTIC)
            const std::size_t position = std::size_t(event.row) *
                irok_nd75::kColumns + event.column;
            const std::uint8_t diagnosticHid = proof.map[position];
            const std::uint16_t diagnosticOld = diagnosticHid
                ? g_milli[diagnosticHid].load(std::memory_order_relaxed) : 0;
#endif
            Publish(event, proof, qpc.QuadPart);
            g_liveEvents.fetch_add(1, std::memory_order_relaxed);
#if defined(HALLJOY_DIAGNOSTIC)
            if (diagnosticHid)
            {
                ++diagnosticEvents[diagnosticHid];
                if (event.travel && diagnosticMax[diagnosticHid] == 0)
                    ++diagnosticUnique;
                diagnosticMin[diagnosticHid] = std::min(
                    diagnosticMin[diagnosticHid], event.travel);
                diagnosticMax[diagnosticHid] = std::max(
                    diagnosticMax[diagnosticHid], event.travel);
                const auto diagnosticMilli = irok_nd75::ToMilli(event.travel);
                if (diagnosticOld == 0 && diagnosticMilli != 0)
                    ++diagnosticPositiveEdges;
                if (diagnosticOld != 0 && diagnosticMilli == 0)
                {
                    ++diagnosticZeroEdges;
                    ++diagnosticReleases[diagnosticHid];
                }
                diagnosticPeakActive = std::max(diagnosticPeakActive,
                    g_active.load(std::memory_order_relaxed));
            }
#endif
        }

        const auto nowMs = GetTickCount64();
        if (nowMs - lastRollup >= 1000)
        {
            const auto live = g_liveEvents.load(std::memory_order_relaxed);
            const auto delta = live - lastRollupLive;
            g_hz10.store(static_cast<std::uint32_t>(delta * 10000ull /
                std::max<ULONGLONG>(1, nowMs - lastRollup)));
            if (intervalCount)
            {
                g_avgUs.store(static_cast<std::uint32_t>(
                    intervalSum / intervalCount));
                g_maxUs.store(static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(maxInterval, 0xffffffffull)));
            }
            DebugLog_WriteBuffered(L"[irok.nd75.telemetry] live_event_hz=%.1f live_events=%llu publications=%llu active=%u mapped=81 avg_event_interval_us=%u max_event_interval_us=%u out_of_range=%llu failures=%llu last_age_ms=%llu",
                g_hz10.load() / 10.0, static_cast<unsigned long long>(live),
                static_cast<unsigned long long>(g_updates.load()), g_active.load(),
                g_avgUs.load(), g_maxUs.load(),
                static_cast<unsigned long long>(g_outOfRange.load()),
                static_cast<unsigned long long>(g_failures.load()),
                static_cast<unsigned long long>(g_lastMs.load()
                    ? nowMs - g_lastMs.load() : 0));
#if defined(HALLJOY_DIAGNOSTIC)
            DebugLog_WriteBuffered(L"[irok.nd75.diagnostic] elapsed_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu out_of_range=%llu active_now=%u",
                static_cast<unsigned long long>(nowMs - diagnosticStartedMs),
                diagnosticUnique, diagnosticPeakActive,
                static_cast<unsigned long long>(diagnosticPositiveEdges),
                static_cast<unsigned long long>(diagnosticZeroEdges),
                static_cast<unsigned long long>(g_outOfRange.load() -
                    diagnosticStartedOutOfRange), g_active.load());
#endif
            lastRollup = nowMs;
            lastRollupLive = live;
        }
    }

    if (transportLost)
    {
        g_connected.store(false, std::memory_order_release);
        Clear();
        StabilityTrace_Write(L"WARN", L"irok-nd75",
            L"transport.disconnected",
            L"neutralized=1 reconnect_pending=1 failures=%llu",
            static_cast<unsigned long long>(g_failures.load()));
    }

#if defined(HALLJOY_DIAGNOSTIC)
    DebugLog_Write(L"[irok.nd75.session_summary] duration_ms=%llu unique_pressed_keys=%u peak_simultaneous=%u positive_edges=%llu release_to_zero_edges=%llu out_of_range=%llu active_at_stop=%u session_live_events=%llu failures=%llu",
        static_cast<unsigned long long>(GetTickCount64() - diagnosticStartedMs),
        diagnosticUnique, diagnosticPeakActive,
        static_cast<unsigned long long>(diagnosticPositiveEdges),
        static_cast<unsigned long long>(diagnosticZeroEdges),
        static_cast<unsigned long long>(g_outOfRange.load() -
            diagnosticStartedOutOfRange), g_active.load(),
        static_cast<unsigned long long>(g_liveEvents.load() -
            diagnosticStartedLive),
        static_cast<unsigned long long>(g_failures.load()));
    for (std::size_t hid = 0; hid < diagnosticEvents.size(); ++hid)
        if (diagnosticEvents[hid])
            DebugLog_Write(L"[irok.nd75.coverage] hid=%02X events=%llu releases=%llu min_raw=%u max_raw=%u final_milli=%u",
                static_cast<unsigned>(hid),
                static_cast<unsigned long long>(diagnosticEvents[hid]),
                static_cast<unsigned long long>(diagnosticReleases[hid]),
                diagnosticMin[hid], diagnosticMax[hid],
                g_milli[hid].load(std::memory_order_relaxed));
#endif
    if (!transportLost)
        session.Send(irok_nd75::BuildUnsubscribeRequest());
    return true;
}

std::uint32_t WorkerBody()
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        const auto candidates = Enumerate(false, true);
        bool provenPresent = false;
        bool ran = false;
        for (const auto& candidate : candidates)
        {
            Proof proof{};
            if (!EnsureClaim(candidate, &proof)) continue;
            provenPresent = true;
            g_present.store(true, std::memory_order_release);
            ran = Run(candidate, proof);
            if (ran) break;
        }
        if (!provenPresent)
            g_present.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(g_handleMutex);
            g_activeHandle = INVALID_HANDLE_VALUE;
        }
        Clear();
        if (g_stop.load(std::memory_order_acquire)) break;
        WaitForSingleObject(g_wake, ran ? 200 : kReconnectMs);
        ResetEvent(g_wake);
    }
    g_present.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    return 0;
}

void WorkerFault(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_fault.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    Clear();
    StabilityTrace_WriteCritical(L"ERROR", L"irok-nd75", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerComplete(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(
        record.kind == halljoy::worker::WorkerExceptionKind::None
            ? L"INFO" : L"ERROR",
        L"irok-nd75", L"worker.exit", L"fault_kind=%u",
        static_cast<unsigned>(record.kind));
}

unsigned __stdcall Worker(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerBody(); }, WorkerFault, WorkerComplete, 0xE0416737u));
}

bool Prepare()
{
    if (g_prepared.exchange(true, std::memory_order_acq_rel))
        return NativeAnalogRouting_ProtocolHasClaims(
            NativeAnalogProtocol::IrokNd75M484);
    bool claimed = false;
    for (const auto& candidate : Enumerate(false, true))
    {
        Proof proof{};
        if (EnsureClaim(candidate, &proof)) claimed = true;
    }
    g_present.store(claimed, std::memory_order_release);
    return claimed;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_prepared.load(std::memory_order_acquire)) Prepare();
    if (g_thread) return g_running.load(std::memory_order_acquire);
    g_stop.store(false, std::memory_order_release);
    g_running.store(true, std::memory_order_release);
    g_fault.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake)
    {
        g_running.store(false, std::memory_order_release);
        return false;
    }
    unsigned id = 0;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker,
        nullptr, 0, &id));
    if (!g_thread)
    {
        CloseHandle(g_wake);
        g_wake = nullptr;
        g_running.store(false, std::memory_order_release);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"irok-nd75", L"start.ok",
        L"thread_id=%u hotplug_ready=1", id);
    return true;
}

halljoy::lifecycle::StopResult Stop(
    halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release);
    if (g_wake) SetEvent(g_wake);
    {
        std::lock_guard<std::mutex> active(g_handleMutex);
        if (g_activeHandle != INVALID_HANDLE_VALUE)
            CancelIoEx(g_activeHandle, nullptr);
    }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0)
        return halljoy::lifecycle::ObserveWorkerJoin(generation,
            wait == WAIT_TIMEOUT
                ? halljoy::lifecycle::JoinWaitStatus::TimedOut
                : halljoy::lifecycle::JoinWaitStatus::Failed,
            wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread);
    g_thread = nullptr;
    if (g_wake) CloseHandle(g_wake);
    g_wake = nullptr;
    g_running.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    Clear();
    return NativeAnalogBackendStopJoined(generation);
}

void Notify()
{
    std::lock_guard<std::mutex> lock(g_signalMutex);
    if (g_wake) SetEvent(g_wake);
}

bool Present()
{
    return g_present.load(std::memory_order_acquire);
}

bool Connected()
{
    return g_connected.load(std::memory_order_acquire);
}

bool Owns(std::uint16_t hid)
{
    return hid < g_owned.size() && Connected() &&
        g_owned[hid].load(std::memory_order_relaxed) != 0;
}

std::uint16_t Get(std::uint16_t hid)
{
    return Owns(hid) ? g_milli[hid].load(std::memory_order_relaxed) : 0;
}

void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
    out->vendorId = kVendorId;
    out->productId = kProductId;
    out->usagePage = kUsagePage;
    out->usage = kUsage;
    out->mappedKeys = 81;
    out->activeKeys = g_active.load();
    out->nominalRawLevels = irok_nd75::kNominalTravelMaximum + 1u;
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->updateHz10 = g_hz10.load();
    out->averageIntervalUs = g_avgUs.load();
    out->maximumIntervalUs = g_maxUs.load();
    out->successfulUpdates = g_updates.load();
    out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last,
            0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
        L"IROK ND75 M484 experimental, 81 mapped, raw 0..40, range_errors=%llu",
        static_cast<unsigned long long>(g_outOfRange.load()));
}
}

const NativeAnalogBackendDescriptor& IrokNd75_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "irok-nd75-m484-experimental",
        L"IROK ND75 M484 (experimental)",
        NativeAnalogProtocol::IrokNd75M484,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_StreamTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe |
            NativeAnalogBackendFlag_ReversibleControlProbe,
        &Prepare,
        &Start,
        &Stop,
        &Notify,
        &Present,
        &Connected,
        &Owns,
        &Get,
        &Telemetry,
    };
    return descriptor;
}
