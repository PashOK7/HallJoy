#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "aula_win60he_backend.h"

#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"
#include "aula_win60he_client.h"
#include "aula_win60he_protocol.h"
#include "aula_win60he_session_policy.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr DWORD kWriteTimeoutMs = 1000;
constexpr DWORD kReconnectWaitMs = 1000;
constexpr DWORD kInitialAttemptWaitMarginMs = 3000;
constexpr DWORD kInitialAttemptWaitMs =
    static_cast<DWORD>(aula_win60he::kCapabilityTransactions *
        (kWriteTimeoutMs + aula_win60he::kMatrixResponseTimeoutMs) +
        kInitialAttemptWaitMarginMs);
constexpr DWORD kPollPauseMs = 1;
constexpr DWORD kStopJoinTimeoutMs = 3000;
constexpr ULONGLONG kActiveMapRefreshIntervalMs = 2000;
constexpr ULONG kRequestedInputBuffers = 64;
constexpr ULONG kMinimumInputBuffers =
    static_cast<ULONG>(aula_win60he::kMaximumResponseReports);

struct ScopedHandle
{
    HANDLE value = INVALID_HANDLE_VALUE;

    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : value(handle) {}
    ~ScopedHandle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : value(other.value)
    {
        other.value = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (value && value != INVALID_HANDLE_VALUE)
                CloseHandle(value);
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
    std::wstring instanceId;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
};

struct EnumerationResult
{
    bool exactVidPidPathSeen = false;
    std::vector<Candidate> candidates;
};

struct RetainedDeviceIdentity
{
    std::wstring path;
    std::wstring instanceId;
    std::array<char, 17> firmwareSerial{};
    bool valid = false;
    bool hasFirmwareSerialEvidence = false;
};

struct ProbeResult
{
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
    std::uint16_t inputReportBytes = 0;
    std::uint16_t outputReportBytes = 0;
    aula_win60he::CapabilityProof capability{};
};

struct ClaimState
{
    bool validated = false;
    RetainedDeviceIdentity identity{};
    ProbeResult proof{};
};

struct Session
{
    ScopedHandle handle;
    ScopedHandle readEvent;
    ScopedHandle writeEvent;
    Candidate candidate{};

    explicit operator bool() const
    {
        return handle && readEvent && writeEvent;
    }
};

std::atomic<bool> g_routingPrepared{ false };
std::mutex g_claimMutex;
ClaimState g_claim{};

std::atomic<bool> g_running{ false };
std::atomic<bool> g_stop{ false };
std::atomic<bool> g_deviceChanged{ false };
std::atomic<bool> g_candidatePresent{ false };
std::atomic<bool> g_protocolPresent{ false };
std::atomic<bool> g_connected{ false };
std::atomic<bool> g_ambiguousSelection{ false };
std::atomic<bool> g_invalidEnumeration{ false };
std::atomic<bool> g_retainedIdentityMissing{ false };
std::atomic<bool> g_firmwareSerialMismatch{ false };
std::atomic<std::uint32_t> g_candidateCount{ 0 };
std::atomic<std::uint32_t> g_lastOpenError{ ERROR_SUCCESS };
std::mutex g_serviceMutex;
std::mutex g_signalMutex;
std::mutex g_activeSessionMutex;
HANDLE g_threadHandle = nullptr;
HANDLE g_wakeEvent = nullptr;
HANDLE g_initialAttemptEvent = nullptr;
HANDLE g_activeSessionHandle = INVALID_HANDLE_VALUE;
std::atomic<halljoy::worker::WorkerExceptionKind> g_workerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
halljoy::worker::WorkerExceptionRecord g_workerFaultRecord{};

std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint8_t>, 256> g_owned{};
std::atomic<std::uint16_t> g_vendorId{ 0 };
std::atomic<std::uint16_t> g_productId{ 0 };
std::atomic<std::uint16_t> g_usagePage{ 0 };
std::atomic<std::uint16_t> g_maximumTravelUm{ 0 };
std::atomic<std::uint16_t> g_precisionUm{ 0 };
std::atomic<std::uint32_t> g_mappedKeys{ 0 };
std::atomic<std::uint32_t> g_activeKeys{ 0 };
std::atomic<std::uint32_t> g_inputReportBytes{ 0 };
std::atomic<std::uint32_t> g_outputReportBytes{ 0 };
std::atomic<std::uint64_t> g_successfulUpdates{ 0 };
std::atomic<std::uint64_t> g_failedUpdates{ 0 };
std::atomic<std::uint64_t> g_lastMatrixUs{ 0 };
std::atomic<std::uint32_t> g_averageIntervalUs{ 0 };
std::atomic<std::uint32_t> g_maximumIntervalUs{ 0 };
std::atomic<ULONGLONG> g_lastUpdateMs{ 0 };

class ActiveSessionRegistration final
{
public:
    explicit ActiveSessionRegistration(Session& session) noexcept
        : handle_(session.handle.value)
    {
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        if (!g_stop.load(std::memory_order_acquire) &&
            handle_ && handle_ != INVALID_HANDLE_VALUE)
        {
            g_activeSessionHandle = handle_;
            active_ = true;
        }
    }

    ~ActiveSessionRegistration()
    {
        if (!active_)
            return;
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        if (g_activeSessionHandle == handle_)
            g_activeSessionHandle = INVALID_HANDLE_VALUE;
    }

    ActiveSessionRegistration(const ActiveSessionRegistration&) = delete;
    ActiveSessionRegistration& operator=(const ActiveSessionRegistration&) = delete;

    [[nodiscard]] bool IsActive() const noexcept { return active_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool active_ = false;
};

std::uint64_t NowUs()
{
    static const std::uint64_t frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<std::uint64_t>(value.QuadPart > 0 ? value.QuadPart : 1);
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return (static_cast<std::uint64_t>(now.QuadPart) * 1000000ull) / frequency;
}

void AtomicMaximum(std::atomic<std::uint32_t>& target, std::uint32_t value)
{
    std::uint32_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
        !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

void UpdateSmoothed(std::atomic<std::uint32_t>& target, std::uint32_t value)
{
    std::uint32_t current = target.load(std::memory_order_relaxed);
    for (;;)
    {
        const std::uint32_t next = current == 0 ? value :
            (value > current
                ? current + (value - current + 3u) / 4u
                : current - (current - value + 3u) / 4u);
        if (target.compare_exchange_weak(current, next, std::memory_order_relaxed))
            return;
    }
}

void RecordSuccessfulMatrix()
{
    const std::uint64_t now = NowUs();
    const std::uint64_t previous = g_lastMatrixUs.exchange(now, std::memory_order_relaxed);
    if (previous != 0 && now > previous)
    {
        const auto interval = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(now - previous, 0xffffffffull));
        UpdateSmoothed(g_averageIntervalUs, interval);
        AtomicMaximum(g_maximumIntervalUs, interval);
    }
    g_lastUpdateMs.store(GetTickCount64(), std::memory_order_relaxed);
    g_successfulUpdates.fetch_add(1, std::memory_order_relaxed);
}

bool IsSupportedReportLength(std::uint16_t length)
{
    return length == aula_win60he::kWindowsHidReportBytes;
}

bool IsFingerprint(const Candidate& candidate)
{
    return candidate.attributes.VendorID == aula_win60he::kAulaVendorId &&
        candidate.attributes.ProductID == aula_win60he::kAulaProductId &&
        candidate.caps.UsagePage == aula_win60he::kAulaUsagePage &&
        candidate.caps.Usage == aula_win60he::kAulaUsage &&
        IsSupportedReportLength(candidate.caps.InputReportByteLength) &&
        IsSupportedReportLength(candidate.caps.OutputReportByteLength);
}

bool EqualWindowsIdentity(
    const std::wstring& left,
    const std::wstring& right) noexcept
{
    if (left.empty() || right.empty() || left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::towlower(left[index]) != std::towlower(right[index]))
            return false;
    }
    return true;
}

bool SameDevicePath(const wchar_t* left, const std::wstring& right) noexcept
{
    if (!left)
        return false;
    return EqualWindowsIdentity(std::wstring(left), right);
}

bool PathContainsExactVidPid(const wchar_t* path)
{
    if (!path)
        return false;
    std::wstring lower(path);
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
    return lower.find(L"vid_1ca2&pid_1902") != std::wstring::npos;
}

bool ReadMetadata(HANDLE handle, Candidate* inOut)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || !inOut)
        return false;

    Candidate candidate = *inOut;
    candidate.attributes = HIDD_ATTRIBUTES{};
    candidate.attributes.Size = sizeof(candidate.attributes);
    if (!HidD_GetAttributes(handle, &candidate.attributes))
        return false;

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed))
        return false;
    const NTSTATUS status = HidP_GetCaps(preparsed, &candidate.caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS || !IsFingerprint(candidate))
        return false;

    *inOut = std::move(candidate);
    return true;
}

bool ReadInstanceId(
    HDEVINFO info,
    SP_DEVINFO_DATA* deviceInfo,
    std::wstring* out)
{
    if (out) out->clear();
    if (info == INVALID_HANDLE_VALUE || !deviceInfo || !out)
        return false;

    DWORD required = 0;
    (void)SetupDiGetDeviceInstanceIdW(
        info, deviceInfo, nullptr, 0, &required);
    if (required == 0)
        return false;

    std::vector<wchar_t> buffer(required + 1u, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(
            info, deviceInfo, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr))
        return false;
    *out = buffer.data();
    return !out->empty();
}

bool QueryCurrentIdentityForPath(
    const std::wstring& path,
    std::wstring* optionalInstanceId)
{
    if (optionalInstanceId) optionalInstanceId->clear();
    if (path.empty() || !optionalInstanceId)
        return false;

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO info = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return false;

    std::wstring matchedInstanceId;
    std::size_t matches = 0;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(
                info, nullptr, &hidGuid, index, &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD detailBytes = 0;
        (void)SetupDiGetDeviceInterfaceDetailW(
            info, &interfaceData, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;

        std::vector<std::uint8_t> storage(detailBytes, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
            storage.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(
                info, &interfaceData, detail, detailBytes, nullptr, &deviceInfo) ||
            !SameDevicePath(detail->DevicePath, path))
            continue;

        ++matches;
        std::wstring instanceId;
        if (ReadInstanceId(info, &deviceInfo, &instanceId))
            matchedInstanceId = std::move(instanceId);
    }

    SetupDiDestroyDeviceInfoList(info);
    if (matches != 1u)
        return false;
    *optionalInstanceId = std::move(matchedInstanceId);
    return true;
}

EnumerationResult EnumerateCandidates()
{
    EnumerationResult result{};
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO info = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return result;

    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &hidGuid, index, &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD detailBytes = 0;
        (void)SetupDiGetDeviceInterfaceDetailW(
            info, &interfaceData, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;

        std::vector<std::uint8_t> storage(detailBytes, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(
                info, &interfaceData, detail, detailBytes, nullptr, &deviceInfo))
            continue;

        if (!PathContainsExactVidPid(detail->DevicePath))
            continue;
        result.exactVidPidPathSeen = true;

        const bool claimedByAula = NativeAnalogRouting_IsClaimedBy(
            detail->DevicePath, NativeAnalogProtocol::AulaWin60He);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !claimedByAula)
            continue;

        Candidate candidate{};
        candidate.path = detail->DevicePath;
        // SetupAPI instance ID is stronger identity evidence when available,
        // but some valid HID stacks do not expose it. Exact path correlation,
        // exclusive open and the full live capability proof remain mandatory.
        (void)ReadInstanceId(info, &deviceInfo, &candidate.instanceId);

        // This metadata handle is never used for protocol traffic. The command
        // channel is opened later with shareMode=0 and retained through proof
        // and polling.
        ScopedHandle metadata(CreateFileW(
            candidate.path.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!metadata || !ReadMetadata(metadata.value, &candidate))
            continue;
        result.candidates.push_back(std::move(candidate));
    }

    SetupDiDestroyDeviceInfoList(info);
    std::sort(result.candidates.begin(), result.candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (left.instanceId != right.instanceId)
                return left.instanceId < right.instanceId;
            return left.path < right.path;
        });
    return result;
}

bool EnsureInputQueueCapacity(HANDLE handle, DWORD* error)
{
    if (error) *error = ERROR_SUCCESS;
    ULONG current = 0;
    if (!HidD_GetNumInputBuffers(handle, &current))
    {
        if (error) *error = GetLastError();
        return false;
    }

    if (current < kRequestedInputBuffers)
    {
        // Enlargement is optional once at least one complete three-report
        // response already fits. It is mandatory when the current queue is too
        // small to hold one complete response.
        const bool mustGrow = current < kMinimumInputBuffers;
        if (!HidD_SetNumInputBuffers(handle, kRequestedInputBuffers))
        {
            if (mustGrow)
            {
                if (error) *error = GetLastError();
                return false;
            }
            return true;
        }

        ULONG verified = 0;
        if (!HidD_GetNumInputBuffers(handle, &verified) ||
            verified < kMinimumInputBuffers)
        {
            if (error)
            {
                *error = verified < kMinimumInputBuffers
                    ? ERROR_INSUFFICIENT_BUFFER
                    : GetLastError();
            }
            return false;
        }
    }
    return true;
}

bool OpenSession(const Candidate& candidate, Session* out, DWORD* openError)
{
    if (out) *out = Session{};
    if (openError) *openError = ERROR_SUCCESS;
    if (!out || !IsFingerprint(candidate) || candidate.path.empty())
    {
        if (openError) *openError = ERROR_INVALID_PARAMETER;
        return false;
    }

    Session session{};
    session.candidate = candidate;
    session.handle = ScopedHandle(CreateFileW(
        candidate.path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, // protocol has no transaction ID: the command channel must be exclusive
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr));
    if (!session.handle)
    {
        if (openError) *openError = GetLastError();
        return false;
    }

    // Close the enumerate/open TOCTOU window. A recycled path must not inherit
    // the trust assigned to the SetupAPI instance observed before CreateFileW.
    std::wstring openedInstanceId;
    if (!QueryCurrentIdentityForPath(candidate.path, &openedInstanceId) ||
        (!candidate.instanceId.empty() &&
            !EqualWindowsIdentity(openedInstanceId, candidate.instanceId)))
    {
        if (openError) *openError = ERROR_INVALID_DATA;
        return false;
    }
    // Identity may strengthen after the exclusive open if SetupAPI starts
    // returning an instance ID. It may never weaken or contradict an ID that
    // was present during enumeration.
    session.candidate.instanceId = std::move(openedInstanceId);

    // Re-read the full HID fingerprint through the exact exclusive handle that
    // will perform proof and polling.
    if (!ReadMetadata(session.handle.value, &session.candidate))
    {
        if (openError) *openError = ERROR_INVALID_DATA;
        return false;
    }
    if (!EnsureInputQueueCapacity(session.handle.value, openError))
        return false;

    session.readEvent = ScopedHandle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    session.writeEvent = ScopedHandle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!session)
    {
        if (openError) *openError = GetLastError();
        return false;
    }

    *out = std::move(session);
    return true;
}

bool FinishIo(HidIoOperation& operation, DWORD timeoutMs, DWORD* transferred, DWORD* error)
{
    const DWORD wait = operation.Wait(timeoutMs);
    if (wait == WAIT_OBJECT_0)
        return operation.Finish(transferred, error, false);
    operation.CancelAndDrain(transferred, error);
    if (error && wait == WAIT_TIMEOUT)
        *error = WAIT_TIMEOUT;
    return false;
}

bool WriteReport(Session& session, const aula_win60he::Report& report)
{
    const DWORD reportBytes = session.candidate.caps.OutputReportByteLength;
    if (!IsSupportedReportLength(static_cast<std::uint16_t>(reportBytes)))
        return false;

    aula_win60he::HidWireReport buffer{};
    std::size_t wireBytes = 0;
    if (!aula_win60he::EncodeHidReport(report, reportBytes, &buffer, &wireBytes) ||
        wireBytes != reportBytes)
        return false;

    DWORD error = ERROR_SUCCESS;
    DWORD transferred = 0;
    HidIoOperation operation(session.handle.value, session.writeEvent.value);
    const auto started = operation.StartWrite(buffer.data(), reportBytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
        return false;
    if (started == HidIoOperation::StartResult::Completed)
        return operation.Finish(&transferred, &error, false) && transferred == reportBytes;
    return FinishIo(operation, kWriteTimeoutMs, &transferred, &error) &&
        transferred == reportBytes;
}

bool ReadProtocolBlock(Session& session, DWORD timeoutMs, aula_win60he::Report* out)
{
    if (out) out->fill(0);
    if (!out)
        return false;

    const DWORD reportBytes = session.candidate.caps.InputReportByteLength;
    if (!IsSupportedReportLength(static_cast<std::uint16_t>(reportBytes)))
        return false;

    aula_win60he::HidWireReport buffer{};
    DWORD error = ERROR_SUCCESS;
    DWORD transferred = 0;
    HidIoOperation operation(session.handle.value, session.readEvent.value);
    const auto started = operation.StartRead(buffer.data(), reportBytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
        return false;

    bool complete = false;
    if (started == HidIoOperation::StartResult::Completed)
        complete = operation.Finish(&transferred, &error, false);
    else
        complete = FinishIo(operation, timeoutMs, &transferred, &error);
    if (!complete || transferred != reportBytes)
        return false;

    return aula_win60he::DecodeHidReport(buffer.data(), reportBytes, out);
}

class WindowsReportTransport final : public aula_win60he::ReportTransport
{
public:
    explicit WindowsReportTransport(Session& session) noexcept : session_(session) {}

    bool FlushInput() override
    {
        return session_.handle && HidD_FlushQueue(session_.handle.value) != FALSE;
    }

    bool WriteReport(const aula_win60he::Report& report) override
    {
        return ::WriteReport(session_, report);
    }

    bool ReadReport(std::uint32_t timeoutMs, aula_win60he::Report* out) override
    {
        return ReadProtocolBlock(session_, static_cast<DWORD>(timeoutMs), out);
    }

    std::uint64_t NowMilliseconds() const noexcept override
    {
        return static_cast<std::uint64_t>(GetTickCount64());
    }

private:
    Session& session_;
};

struct ClientTraceContext
{
    std::uint32_t emitted = 0;
    std::uint32_t maximum = 0;
    aula_win60he::Report lastTransmit{};
    aula_win60he::Report lastReceive{};
    bool hasTransmit = false;
    bool hasReceive = false;
};

[[maybe_unused]] void TraceClientReport(
    void* context,
    aula_win60he::TraceKind kind,
    const char* label,
    const aula_win60he::Report* report) noexcept
{
#if defined(HALLJOY_DIAGNOSTIC)
    auto* state = static_cast<ClientTraceContext*>(context);
    const bool error = kind == aula_win60he::TraceKind::Error;
    if (state && report)
    {
        if (kind == aula_win60he::TraceKind::Transmit)
        {
            state->lastTransmit = *report;
            state->hasTransmit = true;
        }
        else if (kind == aula_win60he::TraceKind::Receive)
        {
            state->lastReceive = *report;
            state->hasReceive = true;
        }
    }
    if (!error && state && state->maximum != 0 && state->emitted >= state->maximum)
        return;
    if (state && !error)
        ++state->emitted;

    wchar_t wideLabel[64]{};
    if (label)
    {
        std::size_t i = 0;
        for (; label[i] != '\0' && i + 1u < (sizeof(wideLabel) / sizeof(wideLabel[0])); ++i)
            wideLabel[i] = static_cast<unsigned char>(label[i]);
    }

    const wchar_t* direction = L"ERR";
    if (kind == aula_win60he::TraceKind::Transmit) direction = L"TX";
    else if (kind == aula_win60he::TraceKind::Receive) direction = L"RX";

    if (!report)
    {
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he.trace] %ls %ls",
            direction, wideLabel);
        if (state && state->hasTransmit)
            TraceClientReport(nullptr, aula_win60he::TraceKind::Transmit,
                "last-tx-before-error", &state->lastTransmit);
        if (state && state->hasReceive)
            TraceClientReport(nullptr, aula_win60he::TraceKind::Receive,
                "last-rx-before-error", &state->lastReceive);
        return;
    }

    wchar_t hex[aula_win60he::kWireReportBytes * 3u + 1u]{};
    std::size_t pos = 0;
    for (std::size_t i = 0; i < report->size() && pos + 3u < (sizeof(hex) / sizeof(hex[0])); ++i)
    {
        const int written = _snwprintf_s(
            hex + pos, (sizeof(hex) / sizeof(hex[0])) - pos, _TRUNCATE,
            i + 1u == report->size() ? L"%02X" : L"%02X ",
            static_cast<unsigned>((*report)[i]));
        if (written <= 0)
            break;
        pos += static_cast<std::size_t>(written);
    }
    DebugLog_WriteBuffered(
        L"[backend.aula_win60he.trace] %ls %ls | %ls",
        direction, wideLabel, hex);
#else
    (void)context;
    (void)kind;
    (void)label;
    (void)report;
#endif
}

aula_win60he::TraceSink MakeTraceSink(ClientTraceContext* context) noexcept
{
#if defined(HALLJOY_DIAGNOSTIC)
    if (DebugLog_IsDiagnosticBuild())
        return aula_win60he::TraceSink{ &TraceClientReport, context };
#else
    (void)context;
#endif
    return {};
}


bool BuildProbeResult(
    const Session& session,
    const aula_win60he::CapabilityProof& capability,
    ProbeResult* out)
{
    if (out) *out = ProbeResult{};
    if (!out || !IsFingerprint(session.candidate))
        return false;
    ProbeResult result{};
    result.vendorId = session.candidate.attributes.VendorID;
    result.productId = session.candidate.attributes.ProductID;
    result.usagePage = session.candidate.caps.UsagePage;
    result.usage = session.candidate.caps.Usage;
    result.inputReportBytes = session.candidate.caps.InputReportByteLength;
    result.outputReportBytes = session.candidate.caps.OutputReportByteLength;
    result.capability = capability;
    *out = result;
    return true;
}

void ClearPublishedValues(bool clearOwnership)
{
    bool changed = false;
    for (std::size_t hid = 0; hid < g_milli.size(); ++hid)
    {
        if (g_milli[hid].exchange(0, std::memory_order_relaxed) != 0)
            changed = true;
        if (clearOwnership)
            g_owned[hid].store(0, std::memory_order_relaxed);
    }
    g_activeKeys.store(0, std::memory_order_relaxed);
    if (changed)
        RealtimeLoop_NotifyInputChanged();
}

void PublishProof(const ProbeResult& proof)
{
    ClearPublishedValues(true);
    for (const auto& row : proof.capability.keyMap)
        for (const auto hid : row)
            if (aula_win60he::IsPublishableKeyboardUsage(hid))
                g_owned[hid].store(1, std::memory_order_relaxed);

    g_vendorId.store(proof.vendorId, std::memory_order_relaxed);
    g_productId.store(proof.productId, std::memory_order_relaxed);
    g_usagePage.store(proof.usagePage, std::memory_order_relaxed);
    g_maximumTravelUm.store(
        proof.capability.precision.maximumTravelUm, std::memory_order_relaxed);
    g_precisionUm.store(
        proof.capability.precision.precisionUm, std::memory_order_relaxed);
    g_mappedKeys.store(
        static_cast<std::uint32_t>(proof.capability.mappedKeys),
        std::memory_order_relaxed);
    g_inputReportBytes.store(proof.inputReportBytes, std::memory_order_relaxed);
    g_outputReportBytes.store(proof.outputReportBytes, std::memory_order_relaxed);
}

bool PublishMatrix(
    const aula_win60he::KeyMap& map,
    const aula_win60he::TravelMatrix& travel,
    std::uint16_t maximumTravelUm)
{
    aula_win60he::SnapshotResult snapshot{};
    aula_win60he::BuildHidMilliSnapshot(map, travel, maximumTravelUm, &snapshot);

    bool changed = false;
    for (std::size_t hid = 1; hid < snapshot.milli.size(); ++hid)
    {
        if (g_owned[hid].load(std::memory_order_relaxed) == 0)
            continue;
        const auto next = snapshot.milli[hid];
        if (g_milli[hid].exchange(next, std::memory_order_relaxed) != next)
            changed = true;
    }
    g_activeKeys.store(snapshot.activeKeys, std::memory_order_relaxed);
    if (changed)
        RealtimeLoop_NotifyInputChanged();
    return changed;
}

RetainedDeviceIdentity PreferredIdentity()
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    return g_claim.identity;
}

bool OpenSelectedSession(Session* out)
{
    if (out) *out = Session{};
    if (!out)
        return false;

    const EnumerationResult enumeration = EnumerateCandidates();
    g_candidatePresent.store(
        enumeration.exactVidPidPathSeen || !enumeration.candidates.empty(),
        std::memory_order_release);
    g_candidateCount.store(
        static_cast<std::uint32_t>(enumeration.candidates.size()),
        std::memory_order_relaxed);
    g_ambiguousSelection.store(false, std::memory_order_relaxed);
    g_invalidEnumeration.store(false, std::memory_order_relaxed);
    g_retainedIdentityMissing.store(false, std::memory_order_relaxed);
    g_firmwareSerialMismatch.store(false, std::memory_order_relaxed);

    std::vector<std::wstring> paths;
    std::vector<std::wstring> instanceIds;
    paths.reserve(enumeration.candidates.size());
    instanceIds.reserve(enumeration.candidates.size());
    for (const auto& candidate : enumeration.candidates)
    {
        paths.push_back(candidate.path);
        instanceIds.push_back(candidate.instanceId);
    }

    const RetainedDeviceIdentity preferred = PreferredIdentity();
    const aula_win60he::DeviceSelectionPlan selection =
        aula_win60he::PlanDeviceSelection(
            paths, instanceIds,
            preferred.valid ? preferred.path : std::wstring{},
            preferred.valid ? preferred.instanceId : std::wstring{});
    if (selection.invalidEnumeration)
    {
        g_invalidEnumeration.store(true, std::memory_order_relaxed);
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] contradictory SetupAPI enumeration evidence; refusing device selection");
        return false;
    }
    if (selection.ambiguous)
    {
        g_ambiguousSelection.store(true, std::memory_order_relaxed);
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] multiple or contradictory exact-fingerprint candidates; refusing implicit device selection");
        return false;
    }
    if (selection.retainedIdentityMissing)
    {
        g_retainedIdentityMissing.store(true, std::memory_order_relaxed);
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] retained Aula identity is absent; refusing path, instance or serial rebind until backend restart");
        return false;
    }
    if (selection.candidateIndices.size() != 1u ||
        selection.candidateIndices[0] >= enumeration.candidates.size())
        return false;

    const Candidate& candidate =
        enumeration.candidates[selection.candidateIndices[0]];
    DWORD openError = ERROR_SUCCESS;
    if (!OpenSession(candidate, out, &openError))
    {
        g_lastOpenError.store(openError, std::memory_order_relaxed);
        if (openError == ERROR_SHARING_VIOLATION)
        {
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] vendor HID is busy; exclusive access is required because replies have no transaction ID");
        }
        else
        {
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] exact exclusive open/identity validation failed error=%lu",
                static_cast<unsigned long>(openError));
        }
        return false;
    }
    g_lastOpenError.store(ERROR_SUCCESS, std::memory_order_relaxed);
    return true;
}

bool MatchesRetainedProofIdentity(
    const Session& session,
    const aula_win60he::CapabilityProof& capability)
{
    const RetainedDeviceIdentity retained = PreferredIdentity();
    if (!retained.valid)
        return true;
    return aula_win60he::MatchesRetainedDeviceIdentity(
        retained.path,
        retained.instanceId,
        retained.firmwareSerial,
        retained.hasFirmwareSerialEvidence,
        session.candidate.path,
        session.candidate.instanceId,
        capability.sync.serial);
}

void SaveValidatedClaim(const Session& session, const ProbeResult& proof)
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    g_claim.validated = true;
    g_claim.identity.path = session.candidate.path;
    g_claim.identity.instanceId = session.candidate.instanceId;
    g_claim.identity.firmwareSerial = proof.capability.sync.serial;
    g_claim.identity.hasFirmwareSerialEvidence =
        aula_win60he::IsMeaningfulDeviceSerial(proof.capability.sync.serial);
    g_claim.identity.valid = true;
    g_claim.proof = proof;
}

void WaitForReconnect(DWORD timeoutMs)
{
    if (g_deviceChanged.exchange(false, std::memory_order_acq_rel))
    {
        if (g_wakeEvent)
            ResetEvent(g_wakeEvent);
        return;
    }
    if (g_wakeEvent)
    {
        const DWORD wait = WaitForSingleObject(g_wakeEvent, timeoutMs);
        if (wait == WAIT_OBJECT_0)
            ResetEvent(g_wakeEvent);
    }
    g_deviceChanged.store(false, std::memory_order_release);
}

void SignalInitialAttempt()
{
    if (g_initialAttemptEvent)
        SetEvent(g_initialAttemptEvent);
}

std::uint32_t WorkerMain()
{
    bool initialAttemptSignaled = false;
    const auto signalInitialAttempt = [&initialAttemptSignaled] {
        if (!initialAttemptSignaled)
        {
            SignalInitialAttempt();
            initialAttemptSignaled = true;
        }
    };

    while (!g_stop.load(std::memory_order_acquire))
    {
        Session session{};
        if (!OpenSelectedSession(&session))
        {
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForReconnect(kReconnectWaitMs);
            continue;
        }

        ActiveSessionRegistration activeSession(session);
        if (!activeSession.IsActive())
        {
            signalInitialAttempt();
            break;
        }

        WindowsReportTransport transport(session);
        ClientTraceContext traceContext{};
        traceContext.maximum = 64;
        aula_win60he::Client client(transport, MakeTraceSink(&traceContext));
        aula_win60he::CapabilityProof capability{};
        aula_win60he::Failure failure{};
        if (!client.Probe(&capability, &failure))
        {
            g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
            DebugLog_Write(
                L"[backend.aula_win60he] proof failed stage=%u command=%02X selector=%02X index=%u; closing exclusive session",
                static_cast<unsigned>(failure.stage),
                static_cast<unsigned>(failure.command),
                static_cast<unsigned>(failure.selector),
                static_cast<unsigned>(failure.index));
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForReconnect(100);
            continue;
        }

        if (!MatchesRetainedProofIdentity(session, capability))
        {
            g_firmwareSerialMismatch.store(true, std::memory_order_relaxed);
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] proven Aula identity returned different retained path, instance or firmware-serial evidence; refusing session");
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForReconnect(100);
            continue;
        }

        ProbeResult proof{};
        if (!BuildProbeResult(session, capability, &proof) ||
            !NativeAnalogRouting_Claim(
                aula_win60he::kAulaVendorId,
                aula_win60he::kAulaProductId,
                session.candidate.path.c_str(),
                NativeAnalogProtocol::AulaWin60He))
        {
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForReconnect(100);
            continue;
        }

        SaveValidatedClaim(session, proof);
        PublishProof(proof);
        g_candidatePresent.store(true, std::memory_order_release);
        g_protocolPresent.store(true, std::memory_order_release);
        g_connected.store(true, std::memory_order_release);
        g_deviceChanged.store(false, std::memory_order_release);
        if (g_wakeEvent) ResetEvent(g_wakeEvent);
        DebugLog_Write(
            L"[backend.aula_win60he] connected exclusive instance=%ls path=%ls vid=%04X pid=%04X usage=%04X:%04X mapped=%u precision_um=%u max_um=%u in=%u out=%u",
            session.candidate.instanceId.c_str(),
            session.candidate.path.c_str(),
            static_cast<unsigned>(proof.vendorId),
            static_cast<unsigned>(proof.productId),
            static_cast<unsigned>(proof.usagePage),
            static_cast<unsigned>(proof.usage),
            static_cast<unsigned>(capability.mappedKeys),
            static_cast<unsigned>(capability.precision.precisionUm),
            static_cast<unsigned>(capability.precision.maximumTravelUm),
            static_cast<unsigned>(proof.inputReportBytes),
            static_cast<unsigned>(proof.outputReportBytes));
        signalInitialAttempt();

        ULONGLONG nextActiveMapRefreshMs =
            GetTickCount64() + kActiveMapRefreshIntervalMs;
        while (!g_stop.load(std::memory_order_acquire))
        {
            if (GetTickCount64() >= nextActiveMapRefreshMs)
            {
                aula_win60he::ActiveMapSnapshot activeMap{};
                failure = aula_win60he::Failure{};
                if (!client.ReadActiveMap(
                        capability.defaultKeyMap, &activeMap, &failure))
                {
                    g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
                    DebugLog_WriteBuffered(
                        L"[backend.aula_win60he] active-map refresh failed stage=%u command=%02X selector=%02X index=%u; destroying session",
                        static_cast<unsigned>(failure.stage),
                        static_cast<unsigned>(failure.command),
                        static_cast<unsigned>(failure.selector),
                        static_cast<unsigned>(failure.index));
                    break;
                }

                if (activeMap.functions != capability.activeFunctions ||
                    activeMap.keyMap != capability.keyMap ||
                    activeMap.mappedKeys != capability.mappedKeys)
                {
                    capability.activeFunctions = activeMap.functions;
                    capability.keyMap = activeMap.keyMap;
                    capability.mappedKeys = activeMap.mappedKeys;
                    proof.capability = capability;
                    SaveValidatedClaim(session, proof);
                    PublishProof(proof);
                    DebugLog_WriteBuffered(
                        L"[backend.aula_win60he] active Fn0 map refreshed mapped=%u",
                        static_cast<unsigned>(capability.mappedKeys));
                }
                nextActiveMapRefreshMs =
                    GetTickCount64() + kActiveMapRefreshIntervalMs;
            }

            aula_win60he::TravelMatrix matrix{};
            failure = aula_win60he::Failure{};
            if (!client.ReadTravelMatrix(capability, &matrix, &failure))
            {
                g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
                DebugLog_WriteBuffered(
                    L"[backend.aula_win60he] poll failed stage=%u command=%02X selector=%02X index=%u; destroying session before any further request",
                    static_cast<unsigned>(failure.stage),
                    static_cast<unsigned>(failure.command),
                    static_cast<unsigned>(failure.selector),
                    static_cast<unsigned>(failure.index));
                break;
            }

            PublishMatrix(
                capability.keyMap,
                matrix,
                capability.precision.maximumTravelUm);
            RecordSuccessfulMatrix();

            if (g_wakeEvent &&
                WaitForSingleObject(g_wakeEvent, kPollPauseMs) == WAIT_OBJECT_0)
            {
                ResetEvent(g_wakeEvent);
                // WM_DEVICECHANGE is global. It wakes an absent-device retry but
                // never tears down a healthy Aula session by itself. Real removal
                // is detected by the next exclusive read/write failure.
                g_deviceChanged.store(false, std::memory_order_release);
            }
        }

        g_protocolPresent.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
        ClearPublishedValues(false);
        if (!g_stop.load(std::memory_order_acquire))
            WaitForReconnect(100);
    }

    g_connected.store(false, std::memory_order_release);
    g_candidatePresent.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    signalInitialAttempt();
    ClearPublishedValues(false);
    g_running.store(false, std::memory_order_release);
    return 0u;
}

void WorkerOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_workerFaultRecord = record;
    g_workerFaultKind.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    ClearPublishedValues(false);
    StabilityTrace_WriteCritical(L"ERROR", L"aula-win60he", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(
        record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"aula-win60he", L"worker.exit", L"fault_kind=%u",
        static_cast<unsigned>(record.kind));
}

unsigned __stdcall WorkerEntry(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerMain(); },
        WorkerOnFault,
        WorkerOnCompletion,
        0xA0600001u));
}

void FillTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out)
        return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = g_candidatePresent.load(std::memory_order_acquire);
    out->connected = g_connected.load(std::memory_order_acquire);
    out->vendorId = g_vendorId.load(std::memory_order_relaxed);
    out->productId = g_productId.load(std::memory_order_relaxed);
    out->usagePage = g_usagePage.load(std::memory_order_relaxed);
    out->usage = aula_win60he::kAulaUsage;
    out->mappedKeys = g_mappedKeys.load(std::memory_order_relaxed);
    out->activeKeys = g_activeKeys.load(std::memory_order_relaxed);
    const auto precision = g_precisionUm.load(std::memory_order_relaxed);
    const auto maximum = g_maximumTravelUm.load(std::memory_order_relaxed);
    out->nominalRawLevels = precision == 0 ? 0u :
        static_cast<std::uint32_t>(maximum / precision) + 1u;
    out->inputReportBytes = g_inputReportBytes.load(std::memory_order_relaxed);
    out->outputReportBytes = g_outputReportBytes.load(std::memory_order_relaxed);
    out->averageIntervalUs = g_averageIntervalUs.load(std::memory_order_relaxed);
    out->maximumIntervalUs = g_maximumIntervalUs.load(std::memory_order_relaxed);
    if (out->averageIntervalUs != 0)
    {
        out->updateHz10 = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            1000000ull,
            (10000000ull + out->averageIntervalUs / 2u) / out->averageIntervalUs));
    }
    const ULONGLONG last = g_lastUpdateMs.load(std::memory_order_relaxed);
    const ULONGLONG now = GetTickCount64();
    if (last != 0 && now >= last)
    {
        out->lastUpdateAgeMs = static_cast<std::uint32_t>(
            std::min<ULONGLONG>(now - last, 0xffffffffull));
    }
    out->successfulUpdates = g_successfulUpdates.load(std::memory_order_relaxed);
    out->failedUpdates = g_failedUpdates.load(std::memory_order_relaxed);
    if (out->connected)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"exclusive same-handle proof/poll, active Fn0 map, %u mapped, %u um max",
            static_cast<unsigned>(out->mappedKeys),
            static_cast<unsigned>(maximum));
    }
    else if (g_ambiguousSelection.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"multiple exact Aula candidates (%u); selection is fail-closed",
            static_cast<unsigned>(g_candidateCount.load(std::memory_order_relaxed)));
    }
    else if (g_invalidEnumeration.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"contradictory SetupAPI identity evidence; device selection is fail-closed");
    }
    else if (g_retainedIdentityMissing.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"retained Aula identity is absent; restart backend to choose another unit");
    }
    else if (g_firmwareSerialMismatch.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula firmware serial differs from retained proof identity");
    }
    else if (out->present &&
        g_lastOpenError.load(std::memory_order_relaxed) == ERROR_SHARING_VIOLATION)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula vendor HID is busy; waiting for exclusive access");
    }
    else if (out->present)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula candidate present; waiting for exact capability proof (error %u)",
            static_cast<unsigned>(g_lastOpenError.load(std::memory_order_relaxed)));
    }
    else
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"waiting for Aula WIN 60 HE MAX 1CA2:1902");
    }
}
}

bool AulaWin60He_PrepareProtocolRouting()
{
    if (g_routingPrepared.load(std::memory_order_acquire))
    {
        const RetainedDeviceIdentity retained = PreferredIdentity();
        if (retained.valid && NativeAnalogRouting_IsClaimedBy(
                retained.path.c_str(), NativeAnalogProtocol::AulaWin60He))
            return true;
        g_routingPrepared.store(false, std::memory_order_release);
    }

    // The current routing contract forbids VID/PID reservations. Prove the
    // complete read-only protocol on one exclusive exact interface first, then
    // publish only that path to the UAP pre-open exclusion registry.
    Session session{};
    bool proven = false;
    if (OpenSelectedSession(&session))
    {
        WindowsReportTransport transport(session);
        ClientTraceContext traceContext{};
        traceContext.maximum = 64;
        aula_win60he::Client client(transport, MakeTraceSink(&traceContext));
        aula_win60he::CapabilityProof capability{};
        aula_win60he::Failure failure{};
        ProbeResult proof{};
        if (client.Probe(&capability, &failure) &&
            MatchesRetainedProofIdentity(session, capability) &&
            BuildProbeResult(session, capability, &proof) &&
            NativeAnalogRouting_Claim(
                aula_win60he::kAulaVendorId,
                aula_win60he::kAulaProductId,
                session.candidate.path.c_str(),
                NativeAnalogProtocol::AulaWin60He))
        {
            SaveValidatedClaim(session, proof);
            PublishProof(proof);
            g_candidatePresent.store(true, std::memory_order_release);
            g_protocolPresent.store(true, std::memory_order_release);
            proven = true;
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"routing.claimed",
                L"vid=%04X pid=%04X usage=%04X:%04X mapped=%u",
                static_cast<unsigned>(proof.vendorId),
                static_cast<unsigned>(proof.productId),
                static_cast<unsigned>(proof.usagePage),
                static_cast<unsigned>(proof.usage),
                static_cast<unsigned>(proof.capability.mappedKeys));
        }
        else
        {
            g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
            g_protocolPresent.store(false, std::memory_order_release);
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] pre-UAP capability proof rejected stage=%u command=%02X selector=%02X index=%u",
                static_cast<unsigned>(failure.stage),
                static_cast<unsigned>(failure.command),
                static_cast<unsigned>(failure.selector),
                static_cast<unsigned>(failure.index));
        }
    }

    g_routingPrepared.store(true, std::memory_order_release);
    return proven;
}

bool AulaWin60He_Start()
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
    if (!g_routingPrepared.load(std::memory_order_acquire))
        (void)AulaWin60He_PrepareProtocolRouting();
    if (g_threadHandle)
        return g_running.load(std::memory_order_acquire);

    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return true;

    g_stop.store(false, std::memory_order_release);
    g_deviceChanged.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_ambiguousSelection.store(false, std::memory_order_relaxed);
    g_invalidEnumeration.store(false, std::memory_order_relaxed);
    g_retainedIdentityMissing.store(false, std::memory_order_relaxed);
    g_firmwareSerialMismatch.store(false, std::memory_order_relaxed);
    g_candidateCount.store(0, std::memory_order_relaxed);
    g_lastOpenError.store(ERROR_SUCCESS, std::memory_order_relaxed);
    g_successfulUpdates.store(0, std::memory_order_relaxed);
    g_failedUpdates.store(0, std::memory_order_relaxed);
    g_lastMatrixUs.store(0, std::memory_order_relaxed);
    g_averageIntervalUs.store(0, std::memory_order_relaxed);
    g_maximumIntervalUs.store(0, std::memory_order_relaxed);
    g_lastUpdateMs.store(0, std::memory_order_relaxed);
    g_workerFaultRecord = {};
    g_workerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);

    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        g_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        g_initialAttemptEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_wakeEvent || !g_initialAttemptEvent)
        {
            if (g_wakeEvent) CloseHandle(g_wakeEvent);
            if (g_initialAttemptEvent) CloseHandle(g_initialAttemptEvent);
            g_wakeEvent = nullptr;
            g_initialAttemptEvent = nullptr;
            g_running.store(false, std::memory_order_release);
            return false;
        }
    }

    unsigned threadId = 0;
    const uintptr_t thread = _beginthreadex(
        nullptr, 0, WorkerEntry, nullptr, 0, &threadId);
    if (thread == 0)
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        CloseHandle(g_wakeEvent);
        CloseHandle(g_initialAttemptEvent);
        g_wakeEvent = nullptr;
        g_initialAttemptEvent = nullptr;
        g_running.store(false, std::memory_order_release);
        return false;
    }
    g_threadHandle = reinterpret_cast<HANDLE>(thread);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"worker.start",
        L"thread_id=%u", threadId);

    // Backend_Init queries proof-only presence immediately after this start
    // phase. Wait for exactly one bounded enumeration/proof attempt so an Aula
    // already attached at process start cannot lose a race against optional UAP
    // initialisation. Absence/open failure is signalled promptly.
    const DWORD initialWait = WaitForSingleObject(
        g_initialAttemptEvent, kInitialAttemptWaitMs);
    if (initialWait == WAIT_TIMEOUT)
    {
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] initial discovery/proof gate timed out; continuing worker in retry mode");
    }
    if (WaitForSingleObject(g_threadHandle, 0) == WAIT_OBJECT_0 &&
        !g_running.load(std::memory_order_acquire))
        return false;
    return true;
}

halljoy::lifecycle::StopResult AulaWin60He_Stop(
    halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
    if (!g_threadHandle)
        return NativeAnalogBackendStopJoined(generation);

    g_stop.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) SetEvent(g_wakeEvent);
    }
    {
        std::lock_guard<std::mutex> activeLock(g_activeSessionMutex);
        if (g_activeSessionHandle && g_activeSessionHandle != INVALID_HANDLE_VALUE)
            (void)CancelIoEx(g_activeSessionHandle, nullptr);
    }

    const DWORD wait = WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs);
    if (wait != WAIT_OBJECT_0)
    {
        const DWORD error = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        StabilityTrace_WriteCritical(L"ERROR", L"aula-win60he", L"stop.incomplete",
            L"wait=%lu native_error=%lu resources_retained=1",
            static_cast<unsigned long>(wait), static_cast<unsigned long>(error));
        return halljoy::lifecycle::ObserveWorkerJoin(
            generation,
            wait == WAIT_TIMEOUT
                ? halljoy::lifecycle::JoinWaitStatus::TimedOut
                : halljoy::lifecycle::JoinWaitStatus::Failed,
            error);
    }

    CloseHandle(g_threadHandle);
    g_threadHandle = nullptr;
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        if (g_initialAttemptEvent) CloseHandle(g_initialAttemptEvent);
        g_wakeEvent = nullptr;
        g_initialAttemptEvent = nullptr;
    }
    g_running.store(false, std::memory_order_release);
    g_candidatePresent.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    ClearPublishedValues(false);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"stop.joined");
    return NativeAnalogBackendStopJoined(generation);
}

void AulaWin60He_NotifyDeviceChange()
{
    g_deviceChanged.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> signalLock(g_signalMutex, std::try_to_lock);
    if (signalLock.owns_lock() && g_wakeEvent)
        (void)SetEvent(g_wakeEvent);
}

bool AulaWin60He_IsProtocolDevicePresent()
{
    // Only a completed exact capability proof on the live exclusive handle is
    // protocol presence. Candidate discovery remains telemetry, not trust.
    return g_protocolPresent.load(std::memory_order_acquire);
}

bool AulaWin60He_HasDiscoveryCandidate()
{
    if (g_protocolPresent.load(std::memory_order_acquire) ||
        g_connected.load(std::memory_order_acquire))
        return true;

    const EnumerationResult enumeration = EnumerateCandidates();
    const bool candidate = !enumeration.candidates.empty();
    g_candidatePresent.store(candidate, std::memory_order_release);
    return candidate;
}

bool AulaWin60He_IsConnected()
{
    return g_connected.load(std::memory_order_acquire);
}

bool AulaWin60He_OwnsHid(std::uint16_t hidUsage)
{
    return hidUsage < g_owned.size() &&
        g_connected.load(std::memory_order_acquire) &&
        g_owned[hidUsage].load(std::memory_order_relaxed) != 0;
}

std::uint16_t AulaWin60He_GetMilli(std::uint16_t hidUsage)
{
    if (!AulaWin60He_OwnsHid(hidUsage))
        return 0;
    return g_milli[hidUsage].load(std::memory_order_relaxed);
}

const NativeAnalogBackendDescriptor& AulaWin60He_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "aula-win60he-max",
        L"Aula WIN 60 HE MAX (SparkPlayJoy RM6x21)",
        NativeAnalogProtocol::AulaWin60He,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe,
        &AulaWin60He_PrepareProtocolRouting,
        &AulaWin60He_Start,
        &AulaWin60He_Stop,
        &AulaWin60He_NotifyDeviceChange,
        &AulaWin60He_IsProtocolDevicePresent,
        &AulaWin60He_IsConnected,
        &AulaWin60He_OwnsHid,
        &AulaWin60He_GetMilli,
        &FillTelemetry,
    };
    return descriptor;
}
