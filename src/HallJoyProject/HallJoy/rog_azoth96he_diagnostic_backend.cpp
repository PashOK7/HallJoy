#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "rog_azoth96he_diagnostic_backend.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"

#include <algorithm>
#include <array>
#include <atomic>
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
constexpr std::uint16_t kVendorId = 0x0B05;
constexpr std::uint16_t kProductId = 0x1C10;
constexpr USAGE kControlUsagePage = 0xFF00;
constexpr USAGE kEventUsagePage = 0xFFC0;
constexpr USAGE kVendorUsage = 0x0001;
constexpr std::size_t kControlReportBytes = 64;
constexpr std::size_t kEventReportBytes = 21; // report ID 03 plus 20-byte descriptor payload
constexpr std::uint8_t kEventReportId = 0x03;
constexpr std::uint8_t kTravelEvent = 0x7E;
constexpr DWORD kReadSliceMs = 100;
constexpr DWORD kStopTimeoutMs = 3000;

using ControlReport = std::array<std::uint8_t, kControlReportBytes>;
using EventReport = std::array<std::uint8_t, kEventReportBytes>;

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE next) : value(next) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
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
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
};

std::atomic<bool> g_prepared{false};
std::atomic<bool> g_present{false};
std::atomic<bool> g_running{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_stop{false};
std::atomic<std::uint64_t> g_eventCount{0};
std::atomic<std::uint64_t> g_failureCount{0};
std::atomic<std::uint64_t> g_lastEventMs{0};
std::atomic<std::uint32_t> g_averageIntervalUs{0};
std::atomic<std::uint32_t> g_maximumIntervalUs{0};
std::atomic<std::uint32_t> g_controlInputBytes{0};
std::atomic<std::uint32_t> g_controlOutputBytes{0};
std::atomic<std::uint32_t> g_eventInputBytes{0};
std::mutex g_serviceMutex;
std::mutex g_handleMutex;
HANDLE g_thread = nullptr;
HANDLE g_activeControl = INVALID_HANDLE_VALUE;
HANDLE g_activeEvent = INVALID_HANDLE_VALUE;

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value)
    {
        hash ^= static_cast<std::uint16_t>(std::towlower(ch));
        hash *= 1099511628211ull;
    }
    return hash;
}

bool TimedRead(HANDLE handle, void* data, DWORD bytes, DWORD timeout, DWORD* transferred)
{
    if (transferred) *transferred = 0;
    HidIoOperation operation(handle);
    DWORD error = 0;
    const auto started = operation.StartRead(data, bytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
    {
        SetLastError(error);
        return false;
    }
    if (started == HidIoOperation::StartResult::Pending)
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

std::vector<Candidate> EnumerateExact(USAGE usagePage, std::uint16_t expectedInput, std::uint16_t expectedOutput)
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
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0, &required, nullptr);
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail, required, nullptr, nullptr)) continue;

        Handle metadata(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;
        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(metadata.value, &attributes) ||
            attributes.VendorID != kVendorId || attributes.ProductID != kProductId) continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        HIDP_CAPS caps{};
        const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) continue;

        const bool exact = caps.UsagePage == usagePage && caps.Usage == kVendorUsage &&
            caps.InputReportByteLength == expectedInput && caps.OutputReportByteLength == expectedOutput;
        DebugLog_Write(L"[rog.azoth96he.enumeration] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u expected=%04X:%04X/%u/%u exact=%d",
            static_cast<unsigned long long>(HashPath(detail->DevicePath)), attributes.VendorID,
            attributes.ProductID, attributes.VersionNumber, caps.UsagePage, caps.Usage,
            caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength,
            usagePage, kVendorUsage, expectedInput, expectedOutput, exact ? 1 : 0);
        if (exact) result.push_back({detail->DevicePath, attributes, caps});
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

bool Prepare()
{
    const auto controls = EnumerateExact(kControlUsagePage, kControlReportBytes, kControlReportBytes);
    const auto events = EnumerateExact(kEventUsagePage, kEventReportBytes, 0);
    bool claimed = false;
    if (!controls.empty() && !events.empty())
    {
        // The catalog is dedicated to M901 diagnostic mode, so both exact paths
        // can be reserved without competing with an ordinary HallJoy backend.
        // Keep the pair all-or-nothing: a partial reservation would make UAP
        // skip one M901 interface although this diagnostic cannot start.
        const bool controlClaimed = NativeAnalogRouting_Claim(kVendorId, kProductId,
            controls.front().path.c_str(), NativeAnalogProtocol::RogAzoth96HeDiagnostic);
        const bool eventClaimed = controlClaimed && NativeAnalogRouting_Claim(kVendorId,
            kProductId, events.front().path.c_str(), NativeAnalogProtocol::RogAzoth96HeDiagnostic);
        claimed = controlClaimed && eventClaimed;
        if (!claimed && controlClaimed)
        {
            // This target has a dedicated one-backend catalog, so Reset cannot
            // release a claim belonging to any unrelated native backend.
            NativeAnalogRouting_Reset();
        }
    }
    g_prepared.store(true, std::memory_order_release);
    g_present.store(claimed, std::memory_order_release);
    return claimed;
}

bool SendEnableTravel(HANDLE control)
{
    // Report ID is absent on FF00, therefore byte 0 is the documented opcode.
    // Do not add fallback commands and never send the calibration opcode 80 26.
    ControlReport report{};
    report[0] = 0x51;
    report[1] = 0x61;
    return HidD_SetOutputReport(control, report.data(), static_cast<ULONG>(report.size())) != FALSE;
}

void RecordTravel(const EventReport& report, std::uint64_t* lastQpc, std::uint64_t frequency)
{
    if (report[0] != kEventReportId || report[1] != kTravelEvent) return;
    const std::uint16_t firmwareKey = static_cast<std::uint16_t>(report[2]) |
        (static_cast<std::uint16_t>(report[3]) << 8);
    const std::uint16_t travel = static_cast<std::uint16_t>(report[4]) |
        (static_cast<std::uint16_t>(report[5]) << 8);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    if (*lastQpc != 0 && frequency != 0)
    {
        const std::uint64_t deltaUs = (static_cast<std::uint64_t>(now.QuadPart) - *lastQpc) * 1000000ull / frequency;
        const auto prior = g_averageIntervalUs.load(std::memory_order_relaxed);
        g_averageIntervalUs.store(prior == 0 ? static_cast<std::uint32_t>(deltaUs) :
            static_cast<std::uint32_t>((static_cast<std::uint64_t>(prior) * 7 + deltaUs) / 8),
            std::memory_order_relaxed);
        g_maximumIntervalUs.store(std::max(g_maximumIntervalUs.load(std::memory_order_relaxed),
            static_cast<std::uint32_t>(std::min<std::uint64_t>(deltaUs, 0xFFFFFFFFull))),
            std::memory_order_relaxed);
    }
    *lastQpc = static_cast<std::uint64_t>(now.QuadPart);
    const auto sequence = g_eventCount.fetch_add(1, std::memory_order_relaxed) + 1;
    g_lastEventMs.store(GetTickCount64(), std::memory_order_relaxed);
    DebugLog_WriteBuffered(L"[rog.azoth96he.travel] seq=%llu firmware_key=%04X raw_le16=%u report_id=03 event=7E",
        static_cast<unsigned long long>(sequence), firmwareKey, travel);
}

unsigned __stdcall Worker(void*)
{
    const auto controls = EnumerateExact(kControlUsagePage, kControlReportBytes, kControlReportBytes);
    const auto events = EnumerateExact(kEventUsagePage, kEventReportBytes, 0);
    if (controls.empty() || events.empty())
    {
        ++g_failureCount;
        g_running.store(false, std::memory_order_release);
        return 0;
    }
    Handle control(CreateFileW(controls.front().path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    Handle event(CreateFileW(events.front().path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
    if (!control || !event)
    {
        ++g_failureCount;
        g_running.store(false, std::memory_order_release);
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_handleMutex);
        g_activeControl = control.value;
        g_activeEvent = event.value;
    }
    g_controlInputBytes.store(controls.front().caps.InputReportByteLength, std::memory_order_release);
    g_controlOutputBytes.store(controls.front().caps.OutputReportByteLength, std::memory_order_release);
    g_eventInputBytes.store(events.front().caps.InputReportByteLength, std::memory_order_release);
    if (!SendEnableTravel(control.value))
    {
        ++g_failureCount;
        DebugLog_Write(L"[rog.azoth96he.enable] failed win32=%lu command=51 61", GetLastError());
    }
    else
    {
        g_connected.store(true, std::memory_order_release);
        DebugLog_Write(L"[rog.azoth96he.enable] sent command=51 61 scope=all_keys; calibration_command_80_26=prohibited");
    }

    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    std::uint64_t lastQpc = 0;
    while (!g_stop.load(std::memory_order_acquire))
    {
        EventReport report{};
        DWORD received = 0;
        if (TimedRead(event.value, report.data(), static_cast<DWORD>(report.size()), kReadSliceMs, &received))
        {
            if (received == report.size()) RecordTravel(report, &lastQpc,
                static_cast<std::uint64_t>(frequency.QuadPart));
            else { ++g_failureCount; DebugLog_Write(L"[rog.azoth96he.rx] bad_length=%lu expected=%llu", received, static_cast<unsigned long long>(report.size())); }
        }
        else if (GetLastError() != WAIT_TIMEOUT && GetLastError() != ERROR_OPERATION_ABORTED)
        {
            ++g_failureCount;
            DebugLog_Write(L"[rog.azoth96he.rx] failed win32=%lu", GetLastError());
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_handleMutex);
        g_activeControl = INVALID_HANDLE_VALUE;
        g_activeEvent = INVALID_HANDLE_VALUE;
    }
    g_connected.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    return 0;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_prepared.load(std::memory_order_acquire) && !Prepare()) return false;
    if (g_thread) return g_running.load(std::memory_order_acquire);
    g_stop.store(false, std::memory_order_release);
    g_running.store(true, std::memory_order_release);
    unsigned threadId = 0;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &threadId));
    if (!g_thread)
    {
        g_running.store(false, std::memory_order_release);
        return false;
    }
    DebugLog_Write(L"[rog.azoth96he.start] diagnostic_worker=%u; no HallJoy input ownership", threadId);
    return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> active(g_handleMutex);
        if (g_activeControl != INVALID_HANDLE_VALUE) CancelIoEx(g_activeControl, nullptr);
        if (g_activeEvent != INVALID_HANDLE_VALUE) CancelIoEx(g_activeEvent, nullptr);
    }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0)
        return NativeAnalogBackendStopFailed(generation,
            wait == WAIT_TIMEOUT ? halljoy::lifecycle::LifecycleErrorCode::StopTimedOut :
            halljoy::lifecycle::LifecycleErrorCode::JoinFailed,
            wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread);
    g_thread = nullptr;
    g_running.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    return NativeAnalogBackendStopJoined(generation);
}

void Notify() {}
bool Present() { return g_present.load(std::memory_order_acquire); }
bool Connected() { return g_connected.load(std::memory_order_acquire); }
bool Owns(std::uint16_t) { return false; }
std::uint16_t Get(std::uint16_t) { return 0; }

void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
    out->vendorId = kVendorId;
    out->productId = kProductId;
    out->usagePage = kEventUsagePage;
    out->usage = kVendorUsage;
    out->nominalRawLevels = 0; // not inferred until physical capture establishes scale
    out->inputReportBytes = g_eventInputBytes.load(std::memory_order_relaxed);
    out->outputReportBytes = g_controlOutputBytes.load(std::memory_order_relaxed);
    out->successfulUpdates = g_eventCount.load(std::memory_order_relaxed);
    out->failedUpdates = g_failureCount.load(std::memory_order_relaxed);
    out->averageIntervalUs = g_averageIntervalUs.load(std::memory_order_relaxed);
    out->maximumIntervalUs = g_maximumIntervalUs.load(std::memory_order_relaxed);
    const auto last = g_lastEventMs.load(std::memory_order_relaxed);
    const auto now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last ? static_cast<std::uint32_t>(
        std::min<std::uint64_t>(now - last, 0xFFFFFFFFull)) : 0;
    if (out->averageIntervalUs != 0) out->updateHz10 =
        static_cast<std::uint32_t>(10000000ull / out->averageIntervalUs);
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
        L"ROG Azoth 96 HE diagnostic: travel events=%llu; no gameplay ownership",
        static_cast<unsigned long long>(out->successfulUpdates));
}
}

const NativeAnalogBackendDescriptor& RogAzoth96HeDiagnostic_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor),
        "rog-azoth96he-diagnostic", L"ROG Azoth 96 HE diagnostic (no input ownership)",
        NativeAnalogProtocol::RogAzoth96HeDiagnostic, NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_StreamTransport,
        &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return descriptor;
}
