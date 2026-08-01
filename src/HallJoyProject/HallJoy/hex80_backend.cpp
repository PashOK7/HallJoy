#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "hex80_backend.h"
#include "hex80_protocol.h"
#include "native_analog_routing.h"
#include "hid_io_operation.h"
#include "realtime_loop.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <mutex>
#include <process.h>
#include <string>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr DWORD kIoWriteTimeoutMs = 40;
constexpr DWORD kProbeReadTimeoutMs = 120;
constexpr DWORD kPollReadTimeoutMs = 30;
constexpr DWORD kReconnectWaitMs = 1500;
constexpr DWORD kStopJoinTimeoutMs = 3000;
constexpr unsigned kMaxConsecutiveFailures = 8;

struct Candidate
{
    std::wstring path;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
};

struct ScopedHandle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : value(handle) {}
    ~ScopedHandle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
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

std::atomic<bool> g_routingPrepared{ false };
std::mutex g_routingMutex;
std::vector<std::uint16_t> g_routedProductIds;

std::atomic<bool> g_running{ false };
std::atomic<bool> g_stop{ false };
std::atomic<bool> g_present{ false };
std::atomic<bool> g_connected{ false };
HANDLE g_threadHandle = nullptr;
HANDLE g_wakeEvent = nullptr;
std::atomic<halljoy::worker::WorkerExceptionKind> g_workerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
halljoy::worker::WorkerExceptionRecord g_workerFaultRecord{};
std::mutex g_serviceMutex;
std::mutex g_signalMutex;
std::mutex g_activeSessionMutex;
HANDLE g_activeSessionHandle = INVALID_HANDLE_VALUE;

std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint16_t>, 256> g_travel{};
std::array<std::atomic<std::uint8_t>, 256> g_observed{};
std::atomic<std::uint32_t> g_observedCount{ 0 };

std::atomic<std::uint16_t> g_detectedPid{ 0 };
std::atomic<std::uint16_t> g_activePid{ 0 };
std::atomic<std::uint16_t> g_activeVersion{ 0 };
std::atomic<std::uint16_t> g_travelMax{ 0 };
std::atomic<std::uint32_t> g_inputReportBytes{ 0 };
std::atomic<std::uint32_t> g_outputReportBytes{ 0 };
std::atomic<ULONGLONG> g_lastPacketMs{ 0 };
std::atomic<std::uint64_t> g_pollAttempts{ 0 };
std::atomic<std::uint64_t> g_pollSuccess{ 0 };
std::atomic<std::uint64_t> g_pollFail{ 0 };
std::atomic<std::uint64_t> g_matrixCycles{ 0 };
std::atomic<std::uint64_t> g_lastChunkUs{ 0 };
std::atomic<std::uint32_t> g_avgChunkIntervalUs{ 0 };
std::atomic<std::uint32_t> g_avgTransactionUs{ 0 };
std::atomic<std::uint32_t> g_maxTransactionUs{ 0 };
std::atomic<std::uint64_t> g_lastMatrixUs{ 0 };
std::atomic<std::uint32_t> g_avgMatrixIntervalUs{ 0 };
std::atomic<std::uint32_t> g_maxMatrixIntervalUs{ 0 };

bool InjectStopTimeout() noexcept
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine &&
        wcsstr(commandLine, L"--halljoy-test-hex80-stop-timeout");
#else
    return false;
#endif
}

class ScopedActiveSessionHandle
{
public:
    explicit ScopedActiveSessionHandle(HANDLE handle) noexcept : handle_(handle)
    {
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        g_activeSessionHandle = handle_;
    }

    ~ScopedActiveSessionHandle() noexcept
    {
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        if (g_activeSessionHandle == handle_)
            g_activeSessionHandle = INVALID_HANDLE_VALUE;
    }

    ScopedActiveSessionHandle(const ScopedActiveSessionHandle&) = delete;
    ScopedActiveSessionHandle& operator=(const ScopedActiveSessionHandle&) = delete;

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

void CancelActiveSessionIo()
{
    std::lock_guard<std::mutex> lock(g_activeSessionMutex);
    if (g_activeSessionHandle && g_activeSessionHandle != INVALID_HANDLE_VALUE)
        (void)CancelIoEx(g_activeSessionHandle, nullptr);
}

bool ActiveSessionHandleIsRegistered()
{
    std::lock_guard<std::mutex> lock(g_activeSessionMutex);
    return g_activeSessionHandle && g_activeSessionHandle != INVALID_HANDLE_VALUE;
}

constexpr std::array<std::uint8_t, 256> BuildOwnedHids()
{
    std::array<std::uint8_t, 256> owned{};
    for (const auto hid : hex80::kSlotToHid)
        if (hid < owned.size()) owned[hid] = hid != 0 ? 1u : 0u;
    return owned;
}
constexpr auto kOwnedHids = BuildOwnedHids();

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

LONGLONG NowQpc()
{
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

void AtomicMax(std::atomic<std::uint32_t>& target, std::uint32_t value)
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
        std::uint32_t next = current == 0 ? value :
            (value > current
                ? current + (value - current + 3u) / 4u
                : current - (current - value + 3u) / 4u);
        if (target.compare_exchange_weak(current, next, std::memory_order_relaxed))
            break;
    }
}

void RecordChunkTiming(std::uint64_t transactionUs)
{
    const auto tx = static_cast<std::uint32_t>(std::min<std::uint64_t>(transactionUs, 0xffffffffull));
    UpdateSmoothed(g_avgTransactionUs, tx);
    AtomicMax(g_maxTransactionUs, tx);

    const std::uint64_t now = NowUs();
    const std::uint64_t previous = g_lastChunkUs.exchange(now, std::memory_order_relaxed);
    if (previous != 0 && now > previous)
    {
        const auto interval = static_cast<std::uint32_t>(std::min<std::uint64_t>(now - previous, 0xffffffffull));
        UpdateSmoothed(g_avgChunkIntervalUs, interval);
    }
}

void RecordMatrixCycle()
{
    const std::uint64_t now = NowUs();
    const std::uint64_t previous = g_lastMatrixUs.exchange(now, std::memory_order_relaxed);
    if (previous != 0 && now > previous)
    {
        const auto interval = static_cast<std::uint32_t>(std::min<std::uint64_t>(now - previous, 0xffffffffull));
        UpdateSmoothed(g_avgMatrixIntervalUs, interval);
        AtomicMax(g_maxMatrixIntervalUs, interval);
    }
    g_matrixCycles.fetch_add(1, std::memory_order_relaxed);
}

std::vector<Candidate> EnumerateCandidates(bool routedOnly)
{
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO info = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return {};

    std::vector<Candidate> candidates;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &hidGuid, index, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD detailBytes = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &iface, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(detailBytes, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &iface, detail, detailBytes, nullptr, nullptr))
            continue;

        const bool routedToHex80 = NativeAnalogRouting_IsClaimedBy(
            detail->DevicePath, NativeAnalogProtocol::Hex80);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !routedToHex80)
            continue;
        if (routedOnly && !routedToHex80)
            continue;

        ScopedHandle metadata(CreateFileW(
            detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;

        Candidate candidate{};
        candidate.path = detail->DevicePath;
        candidate.attributes.Size = sizeof(candidate.attributes);
        if (!HidD_GetAttributes(metadata.value, &candidate.attributes) ||
            candidate.attributes.VendorID != hex80::kVendorId)
            continue;

        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        const NTSTATUS capsStatus = HidP_GetCaps(preparsed, &candidate.caps);
        HidD_FreePreparsedData(preparsed);
        if (capsStatus != HIDP_STATUS_SUCCESS) continue;

        candidate.usagePage = candidate.caps.UsagePage;
        candidate.usage = candidate.caps.Usage;
        const bool exactInterface = candidate.usagePage == hex80::kUsagePage &&
            candidate.usage == hex80::kUsage;
        const bool reportSizes = candidate.caps.InputReportByteLength >= hex80::kPayloadBytes &&
            candidate.caps.OutputReportByteLength >= hex80::kPayloadBytes + 1u;
        if (exactInterface && reportSizes)
            candidates.push_back(std::move(candidate));
    }
    SetupDiDestroyDeviceInfoList(info);
    return candidates;
}

bool RunTimedIo(HANDLE handle, bool write, void* buffer, DWORD bytes,
    DWORD timeoutMs, DWORD* outTransferred, DWORD* outError)
{
    if (outTransferred) *outTransferred = 0;
    if (outError) *outError = ERROR_SUCCESS;
    HidIoOperation operation(handle);
    if (!operation.IsValid())
    {
        if (outError) *outError = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }

    DWORD error = ERROR_SUCCESS;
    const auto started = write
        ? operation.StartWrite(buffer, bytes, &error)
        : operation.StartRead(buffer, bytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
    {
        if (outError) *outError = error;
        return false;
    }

    if (started == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = operation.Wait(timeoutMs);
        if (wait != WAIT_OBJECT_0)
        {
            DWORD transferred = 0;
            DWORD finalError = ERROR_SUCCESS;
            operation.CancelAndDrain(&transferred, &finalError);
            if (outTransferred) *outTransferred = transferred;
            if (outError) *outError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : finalError;
            return false;
        }
    }

    DWORD transferred = 0;
    const bool ok = operation.Finish(&transferred, &error, false);
    if (outTransferred) *outTransferred = transferred;
    if (outError) *outError = error;
    return ok;
}

class Session
{
public:
    explicit Session(const Candidate& candidate)
        : candidate_(candidate),
          writeBuffer_(candidate.caps.OutputReportByteLength, 0),
          readBuffer_(candidate.caps.InputReportByteLength, 0)
    {
    }

    bool Open()
    {
        handle_ = ScopedHandle(CreateFileW(
            candidate_.path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false;
        if (!HidD_SetNumInputBuffers(handle_.value, 128))
            DebugLog_Write(L"[backend.hex80] input-buffer tuning unavailable err=%lu", GetLastError());
        return true;
    }

    bool SendOnly(const std::array<std::uint8_t, hex80::kPayloadBytes>& payload)
    {
        std::fill(writeBuffer_.begin(), writeBuffer_.end(), std::uint8_t{ 0 });
        if (writeBuffer_.size() < payload.size() + 1u) return false;
        std::copy(payload.begin(), payload.end(), writeBuffer_.begin() + 1u);
        DWORD sent = 0, error = ERROR_SUCCESS;
        return RunTimedIo(handle_.value, true, writeBuffer_.data(),
            static_cast<DWORD>(writeBuffer_.size()), kIoWriteTimeoutMs, &sent, &error) &&
            sent == writeBuffer_.size();
    }

    bool Request(const std::array<std::uint8_t, hex80::kPayloadBytes>& payload,
        std::uint8_t expectedOperation, std::uint8_t expectedSubcommand,
        DWORD timeoutMs, const std::uint8_t** outData, std::size_t* outBytes)
    {
        if (outData) *outData = nullptr;
        if (outBytes) *outBytes = 0;
        if (!SendOnly(payload)) return false;

        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        while (true)
        {
            if (g_stop.load(std::memory_order_acquire)) return false;
            const ULONGLONG now = GetTickCount64();
            if (now >= deadline) break;
            std::fill(readBuffer_.begin(), readBuffer_.end(), std::uint8_t{ 0 });
            const DWORD remaining = static_cast<DWORD>(std::max<ULONGLONG>(1, deadline - now));
            DWORD got = 0, error = ERROR_SUCCESS;
            if (!RunTimedIo(handle_.value, false, readBuffer_.data(),
                static_cast<DWORD>(readBuffer_.size()), remaining, &got, &error))
            {
                if (error == WAIT_TIMEOUT || error == ERROR_OPERATION_ABORTED)
                    continue;
                return false;
            }
            if (got == 0) continue;
            if (!hex80::FindPayload(readBuffer_.data(), got,
                expectedOperation, expectedSubcommand, nullptr))
                continue;
            if (outData) *outData = readBuffer_.data();
            if (outBytes) *outBytes = got;
            return true;
        }
        return false;
    }

    const Candidate& Device() const { return candidate_; }
    HANDLE Handle() const { return handle_.value; }

private:
    Candidate candidate_{};
    ScopedHandle handle_{};
    std::vector<std::uint8_t> writeBuffer_;
    std::vector<std::uint8_t> readBuffer_;
};

bool ProbeCandidate(const Candidate& candidate)
{
    Session session(candidate);
    if (!session.Open()) return false;

    const std::uint8_t* data = nullptr;
    std::size_t bytes = 0;
    std::uint16_t travelMax = 0;
    if (!session.Request(hex80::BuildTravelInfoPayload(), hex80::kGetValue,
        hex80::kTravelInfo, kProbeReadTimeoutMs, &data, &bytes) ||
        !hex80::DecodeTravelInfo(data, bytes, travelMax))
        return false;

    std::array<hex80::TravelEntry, hex80::kChunkSize> entries{};
    std::size_t count = 0;
    if (!session.Request(hex80::BuildTravelBufferPayload(0, 4), hex80::kGetValue,
        hex80::kTravelBuffer, kProbeReadTimeoutMs, &data, &bytes) ||
        !hex80::DecodeTravelChunk(data, bytes, 0, 4, travelMax, entries, count) ||
        count != 4)
        return false;

    return true;
}

void ResetTelemetry()
{
    g_activePid.store(0, std::memory_order_relaxed);
    g_activeVersion.store(0, std::memory_order_relaxed);
    g_travelMax.store(0, std::memory_order_relaxed);
    g_inputReportBytes.store(0, std::memory_order_relaxed);
    g_outputReportBytes.store(0, std::memory_order_relaxed);
    g_lastPacketMs.store(0, std::memory_order_relaxed);
    g_pollAttempts.store(0, std::memory_order_relaxed);
    g_pollSuccess.store(0, std::memory_order_relaxed);
    g_pollFail.store(0, std::memory_order_relaxed);
    g_matrixCycles.store(0, std::memory_order_relaxed);
    g_lastChunkUs.store(0, std::memory_order_relaxed);
    g_avgChunkIntervalUs.store(0, std::memory_order_relaxed);
    g_avgTransactionUs.store(0, std::memory_order_relaxed);
    g_maxTransactionUs.store(0, std::memory_order_relaxed);
    g_lastMatrixUs.store(0, std::memory_order_relaxed);
    g_avgMatrixIntervalUs.store(0, std::memory_order_relaxed);
    g_maxMatrixIntervalUs.store(0, std::memory_order_relaxed);
    g_observedCount.store(0, std::memory_order_relaxed);
    for (auto& value : g_observed) value.store(0, std::memory_order_relaxed);
}

void ClearPublishedValues() noexcept
{
    bool changed = false;
    for (std::size_t hid = 1; hid < g_milli.size(); ++hid)
    {
        if (!kOwnedHids[hid]) continue;
        if (g_milli[hid].exchange(0, std::memory_order_relaxed) != 0)
            changed = true;
        g_travel[hid].store(0, std::memory_order_relaxed);
    }
    if (changed) RealtimeLoop_NotifyInputChanged();
}

bool RunSession(const Candidate& candidate)
{
    Session session(candidate);
    if (!session.Open()) return false;
    ScopedActiveSessionHandle activeSession(session.Handle());

    // Re-prove the exact path with GET-only operations after every reconnect.
    // The PID may have been routed earlier, but no SET is sent to a newly opened
    // interface until both the travel scale and a matrix chunk validate again.
    const std::uint8_t* data = nullptr;
    std::size_t bytes = 0;
    std::uint16_t travelMax = 0;
    if (!session.Request(hex80::BuildTravelInfoPayload(), hex80::kGetValue,
        hex80::kTravelInfo, kProbeReadTimeoutMs, &data, &bytes) ||
        !hex80::DecodeTravelInfo(data, bytes, travelMax))
        return false;

    std::array<hex80::TravelEntry, hex80::kChunkSize> proofEntries{};
    std::size_t proofCount = 0;
    if (!session.Request(hex80::BuildTravelBufferPayload(0, 4), hex80::kGetValue,
        hex80::kTravelBuffer, kProbeReadTimeoutMs, &data, &bytes) ||
        !hex80::DecodeTravelChunk(data, bytes, 0, 4, travelMax, proofEntries, proofCount) ||
        proofCount != 4)
        return false;

    // The reference protocol defines 03 96 19 as an idempotent recovery from
    // calibration mode. It is sent only after the currently opened path has
    // passed both GET-only protocol proofs above.
    if (!session.SendOnly(hex80::BuildCalibrationFinishPayload()))
        return false;

    g_activePid.store(candidate.attributes.ProductID, std::memory_order_relaxed);
    g_activeVersion.store(candidate.attributes.VersionNumber, std::memory_order_relaxed);
    g_travelMax.store(travelMax, std::memory_order_relaxed);
    g_inputReportBytes.store(candidate.caps.InputReportByteLength, std::memory_order_relaxed);
    g_outputReportBytes.store(candidate.caps.OutputReportByteLength, std::memory_order_relaxed);
    g_connected.store(true, std::memory_order_release);
    DebugLog_Write(L"[backend.hex80] connected vid=%04X pid=%04X version=%04X travel_max=%u slots=104 mapped=%u in=%u out=%u",
        (unsigned)candidate.attributes.VendorID,
        (unsigned)candidate.attributes.ProductID,
        (unsigned)candidate.attributes.VersionNumber,
        (unsigned)travelMax,
        (unsigned)hex80::MappedKeyCount(),
        (unsigned)candidate.caps.InputReportByteLength,
        (unsigned)candidate.caps.OutputReportByteLength);

    unsigned consecutiveFailures = 0;
    while (!g_stop.load(std::memory_order_acquire))
    {
        bool completeCycle = true;
        for (std::uint16_t offset = 0; offset < hex80::kTotalSlots;
            offset = static_cast<std::uint16_t>(offset + hex80::kChunkSize))
        {
            if (g_stop.load(std::memory_order_acquire)) return true;
            const std::uint8_t size = static_cast<std::uint8_t>(
                std::min<std::size_t>(hex80::kChunkSize, hex80::kTotalSlots - offset));
            g_pollAttempts.fetch_add(1, std::memory_order_relaxed);
            const std::uint64_t startedUs = NowUs();
            const bool received = session.Request(
                hex80::BuildTravelBufferPayload(offset, size),
                hex80::kGetValue, hex80::kTravelBuffer,
                kPollReadTimeoutMs, &data, &bytes);
            const std::uint64_t finishedUs = NowUs();
            if (g_stop.load(std::memory_order_acquire)) return true;

            std::array<hex80::TravelEntry, hex80::kChunkSize> entries{};
            std::size_t count = 0;
            const bool valid = received && hex80::DecodeTravelChunk(
                data, bytes, offset, size, travelMax, entries, count);
            if (!valid)
            {
                g_pollFail.fetch_add(1, std::memory_order_relaxed);
                completeCycle = false;
                if (++consecutiveFailures >= kMaxConsecutiveFailures)
                    return false;
                continue;
            }

            consecutiveFailures = 0;
            g_pollSuccess.fetch_add(1, std::memory_order_relaxed);
            RecordChunkTiming(finishedUs >= startedUs ? finishedUs - startedUs : 0);
            g_lastPacketMs.store(GetTickCount64(), std::memory_order_relaxed);
            const LONGLONG receivedQpc = NowQpc();
            bool changed = false;
            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& entry = entries[index];
                if (entry.hid == 0 || entry.hid >= 256) continue;
                g_travel[entry.hid].store(entry.travel, std::memory_order_relaxed);
                const auto previous = g_milli[entry.hid].exchange(entry.milli, std::memory_order_relaxed);
                if (previous != entry.milli) changed = true;
                if (g_observed[entry.hid].exchange(1, std::memory_order_relaxed) == 0)
                    g_observedCount.fetch_add(1, std::memory_order_relaxed);
            }
            if (changed)
                RealtimeLoop_NotifyInputChangedAt(receivedQpc);
        }
        if (completeCycle) RecordMatrixCycle();
        SwitchToThread();
    }
    return true;
}

std::uint32_t Hex80WorkerBody()
{
    const HANDLE wakeEvent = g_wakeEvent;
    if (!wakeEvent)
        throw std::runtime_error("Hex80 worker started without wake event");
#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (InjectStopTimeout())
    {
        StabilityTrace_Write(L"INFO", L"hex80", L"test.start", L"simulator_only=1");
        while (!g_stop.load(std::memory_order_acquire))
            WaitForSingleObject(wakeEvent, 100);
        StabilityTrace_Write(L"WARN", L"hex80", L"test.stop_timeout.injected",
            L"simulator_only=1");
        WaitForSingleObject(GetCurrentProcess(), INFINITE);
    }
#endif

    while (!g_stop.load(std::memory_order_acquire))
    {
        const auto candidates = EnumerateCandidates(true);
        g_present.store(!candidates.empty(), std::memory_order_release);
        g_detectedPid.store(candidates.empty() ? 0 : candidates.front().attributes.ProductID,
            std::memory_order_relaxed);
        if (candidates.empty())
        {
            g_connected.store(false, std::memory_order_release);
            g_detectedPid.store(0, std::memory_order_relaxed);
            ClearPublishedValues();
            WaitForSingleObject(wakeEvent, kReconnectWaitMs);
            ResetEvent(wakeEvent);
            continue;
        }

        bool opened = false;
        for (const auto& candidate : candidates)
        {
            if (RunSession(candidate))
            {
                opened = true;
                break;
            }
        }
        (void)opened;
        g_connected.store(false, std::memory_order_release);
        g_activePid.store(0, std::memory_order_relaxed);
        g_activeVersion.store(0, std::memory_order_relaxed);
        ClearPublishedValues();
        if (!g_stop.load(std::memory_order_acquire))
        {
            WaitForSingleObject(wakeEvent, 250);
            ResetEvent(wakeEvent);
        }
    }

    g_connected.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    g_detectedPid.store(0, std::memory_order_relaxed);
    ClearPublishedValues();
    g_running.store(false, std::memory_order_release);
    return 0u;
}

void Hex80WorkerOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_workerFaultRecord = record;
    g_workerFaultKind.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    g_detectedPid.store(0, std::memory_order_relaxed);
    g_activePid.store(0, std::memory_order_relaxed);
    g_activeVersion.store(0, std::memory_order_relaxed);
    ClearPublishedValues();
    StabilityTrace_WriteCritical(L"ERROR", L"hex80", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
    OutputDebugStringA("[HallJoy] Hex80 native worker exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

void Hex80WorkerOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"hex80", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

unsigned __stdcall Hex80WorkerEntry(void*) noexcept
{
    StabilityTrace_Write(L"INFO", L"hex80", L"worker.start");
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return Hex80WorkerBody(); },
        Hex80WorkerOnFault,
        Hex80WorkerOnCompletion,
        0xE0520002u));
}
}

bool Hex80_PrepareProtocolRouting()
{
    if (g_routingPrepared.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> lock(g_routingMutex);
        return !g_routedProductIds.empty();
    }

    const auto candidates = EnumerateCandidates(false);
    g_present.store(!candidates.empty(), std::memory_order_release);
    g_detectedPid.store(candidates.empty() ? 0 : candidates.front().attributes.ProductID,
        std::memory_order_relaxed);
    std::vector<std::uint16_t> routed;
    for (const auto& candidate : candidates)
    {
        const auto pid = candidate.attributes.ProductID;
        if (ProbeCandidate(candidate) &&
            NativeAnalogRouting_Claim(candidate.attributes.VendorID, pid,
                candidate.path.c_str(), NativeAnalogProtocol::Hex80))
        {
            routed.push_back(pid);
            DebugLog_Write(L"[backend.hex80.route] validated GET-only protocol vid=%04X pid=%04X usage=FF60:0061",
                (unsigned)candidate.attributes.VendorID, (unsigned)pid);
        }
    }

    std::sort(routed.begin(), routed.end());
    routed.erase(std::unique(routed.begin(), routed.end()), routed.end());
    {
        std::lock_guard<std::mutex> lock(g_routingMutex);
        g_routedProductIds = routed;
    }
    g_routingPrepared.store(true, std::memory_order_release);
    return !routed.empty();
}

bool Hex80_Start()
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
    const bool injectStopTimeout = InjectStopTimeout();
    if (!injectStopTimeout && !g_routingPrepared.load(std::memory_order_acquire))
        Hex80_PrepareProtocolRouting();
    if (!injectStopTimeout)
    {
        std::lock_guard<std::mutex> lock(g_routingMutex);
        if (g_routedProductIds.empty()) return false;
    }
    if (g_threadHandle)
        return g_running.load(std::memory_order_acquire);

    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return true;

    g_workerFaultRecord = {};
    g_workerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_stop.store(false, std::memory_order_release);
    ResetTelemetry();
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        g_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_wakeEvent)
        {
            const DWORD error = GetLastError();
            g_running.store(false, std::memory_order_release);
            StabilityTrace_WriteCritical(L"ERROR", L"hex80", L"start.failed", L"stage=create_event win32=%lu", error);
            return false;
        }
    }

    unsigned threadId = 0;
    const uintptr_t thread = _beginthreadex(
        nullptr, 0, Hex80WorkerEntry, nullptr, 0, &threadId);
    if (thread == 0)
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"hex80", L"start.failed", L"stage=create_thread");
        return false;
    }
    g_threadHandle = reinterpret_cast<HANDLE>(thread);
    if (WaitForSingleObject(g_threadHandle, 0) == WAIT_OBJECT_0 &&
        !g_running.load(std::memory_order_acquire))
    {
        CloseHandle(g_threadHandle);
        g_threadHandle = nullptr;
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        StabilityTrace_WriteCritical(L"ERROR", L"hex80", L"start.failed", L"stage=worker_early_exit");
        return false;
    }
    StabilityTrace_Write(L"INFO", L"hex80", L"start.ok", L"thread_id=%u", threadId);
    return true;
}

halljoy::lifecycle::StopResult Hex80_StopGeneration(
    halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
    if (!g_threadHandle)
        return NativeAnalogBackendStopJoined(generation);
    StabilityTrace_Write(L"INFO", L"hex80", L"stop.begin", L"worker_handle=1");
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    ClearPublishedValues();
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) SetEvent(g_wakeEvent);
    }
    CancelActiveSessionIo();

    const DWORD wait = WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs);
    if (wait != WAIT_OBJECT_0)
    {
        const DWORD error = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        StabilityTrace_WriteCritical(L"ERROR", L"hex80", L"stop.incomplete",
            L"wait=%lu native_error=%lu thread_handle_retained=1 wake_event_retained=1 active_hid_retained=%d restart_blocked=1",
            static_cast<unsigned long>(wait), static_cast<unsigned long>(error),
            ActiveSessionHandleIsRegistered() ? 1 : 0);
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
        g_wakeEvent = nullptr;
    }
    g_running.store(false, std::memory_order_release);
    ClearPublishedValues();
    StabilityTrace_Write(L"INFO", L"hex80", L"neutralized", L"reason=stop");
    StabilityTrace_Write(L"INFO", L"hex80", L"stop.joined");
    return NativeAnalogBackendStopJoined(generation);
}

void Hex80_Stop()
{
    (void)Hex80_StopGeneration(halljoy::lifecycle::GenerationId{});
}

void Hex80_NotifyDeviceChange()
{
    std::unique_lock<std::mutex> signalLock(g_signalMutex, std::try_to_lock);
    if (signalLock.owns_lock() && g_wakeEvent)
        SetEvent(g_wakeEvent);
}

bool Hex80_IsRunning()
{
    return g_running.load(std::memory_order_acquire);
}

bool Hex80_IsDevicePresent()
{
    return g_present.load(std::memory_order_acquire);
}

bool Hex80_IsProtocolDevicePresent()
{
    if (!g_routingPrepared.load(std::memory_order_acquire))
        Hex80_PrepareProtocolRouting();
    const auto candidates = EnumerateCandidates(true);
    g_present.store(!candidates.empty(), std::memory_order_release);
    g_detectedPid.store(candidates.empty() ? 0 : candidates.front().attributes.ProductID,
        std::memory_order_relaxed);
    return !candidates.empty();
}

bool Hex80_IsConnected()
{
    return g_connected.load(std::memory_order_acquire);
}

bool Hex80_OwnsHid(std::uint16_t hidUsage)
{
    return hidUsage < kOwnedHids.size() && kOwnedHids[hidUsage] != 0 && Hex80_IsConnected();
}

std::uint16_t Hex80_GetMilli(std::uint16_t hidUsage)
{
    if (!Hex80_OwnsHid(hidUsage)) return 0;
    return g_milli[hidUsage].load(std::memory_order_relaxed);
}

void Hex80_GetTelemetry(Hex80Telemetry* out)
{
    if (!out) return;
    Hex80Telemetry telemetry{};
    telemetry.present = Hex80_IsDevicePresent();
    telemetry.connected = Hex80_IsConnected();
    telemetry.vendorId = telemetry.present ? hex80::kVendorId : 0;
    telemetry.productId = g_activePid.load(std::memory_order_relaxed);
    if (telemetry.productId == 0)
        telemetry.productId = g_detectedPid.load(std::memory_order_relaxed);
    telemetry.firmwareVersion = g_activeVersion.load(std::memory_order_relaxed);
    telemetry.travelMax = g_travelMax.load(std::memory_order_relaxed);
    telemetry.mappedKeys = static_cast<std::uint32_t>(hex80::MappedKeyCount());
    telemetry.observedKeys = g_observedCount.load(std::memory_order_relaxed);
    telemetry.inputReportBytes = g_inputReportBytes.load(std::memory_order_relaxed);
    telemetry.outputReportBytes = g_outputReportBytes.load(std::memory_order_relaxed);
    telemetry.averageTransactionUs = g_avgTransactionUs.load(std::memory_order_relaxed);
    telemetry.maximumTransactionUs = g_maxTransactionUs.load(std::memory_order_relaxed);
    telemetry.averageMatrixIntervalUs = g_avgMatrixIntervalUs.load(std::memory_order_relaxed);
    telemetry.maximumMatrixIntervalUs = g_maxMatrixIntervalUs.load(std::memory_order_relaxed);
    const auto chunkInterval = g_avgChunkIntervalUs.load(std::memory_order_relaxed);
    if (chunkInterval != 0)
        telemetry.chunkHz10 = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(1000000ull, (10000000ull + chunkInterval / 2u) / chunkInterval));
    if (telemetry.averageMatrixIntervalUs != 0)
        telemetry.matrixHz10 = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(1000000ull,
                (10000000ull + telemetry.averageMatrixIntervalUs / 2u) /
                telemetry.averageMatrixIntervalUs));
    const auto lastPacket = g_lastPacketMs.load(std::memory_order_relaxed);
    const auto now = GetTickCount64();
    if (lastPacket != 0 && now >= lastPacket)
        telemetry.lastPacketAgeMs = static_cast<std::uint32_t>(
            std::min<ULONGLONG>(now - lastPacket, 0xffffffffull));
    telemetry.pollAttempts = g_pollAttempts.load(std::memory_order_relaxed);
    telemetry.pollSuccess = g_pollSuccess.load(std::memory_order_relaxed);
    telemetry.pollFail = g_pollFail.load(std::memory_order_relaxed);
    telemetry.matrixCycles = g_matrixCycles.load(std::memory_order_relaxed);
    for (std::size_t hid = 1; hid < g_milli.size(); ++hid)
        if (kOwnedHids[hid] && g_milli[hid].load(std::memory_order_relaxed) != 0)
            ++telemetry.activeKeys;
    *out = telemetry;
}

namespace
{
void Hex80_FillGenericTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{};
    Hex80Telemetry t{};
    Hex80_GetTelemetry(&t);
    out->present = t.present;
    out->connected = t.connected;
    out->vendorId = t.vendorId;
    out->productId = t.productId;
    out->usagePage = 0xFF60;
    out->usage = 0x0061;
    out->mappedKeys = t.mappedKeys;
    out->activeKeys = t.activeKeys;
    out->nominalRawLevels = t.travelMax == 0 ? 0u : static_cast<std::uint32_t>(t.travelMax) + 1u;
    out->inputReportBytes = t.inputReportBytes;
    out->outputReportBytes = t.outputReportBytes;
    out->updateHz10 = t.matrixHz10;
    out->averageIntervalUs = t.averageMatrixIntervalUs;
    out->maximumIntervalUs = t.maximumMatrixIntervalUs;
    out->lastUpdateAgeMs = t.lastPacketAgeMs;
    out->successfulUpdates = t.pollSuccess;
    out->failedUpdates = t.pollFail;
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"0x96 matrix polling, %u mapped keys, travel_max=%u",
        static_cast<unsigned>(t.mappedKeys), static_cast<unsigned>(t.travelMax));
}
}

const NativeAnalogBackendDescriptor& Hex80_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "hex80-0x96",
        L"ATK x QK Hex80 0x96",
        NativeAnalogProtocol::Hex80,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe |
            NativeAnalogBackendFlag_DynamicVidPid,
        &Hex80_PrepareProtocolRouting,
        &Hex80_Start,
        [](halljoy::lifecycle::GenerationId generation) {
            return Hex80_StopGeneration(generation);
        },
        &Hex80_NotifyDeviceChange,
        &Hex80_IsProtocolDevicePresent,
        &Hex80_IsConnected,
        &Hex80_OwnsHid,
        &Hex80_GetMilli,
        &Hex80_FillGenericTelemetry,
    };
    return descriptor;
}
