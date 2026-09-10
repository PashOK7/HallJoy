#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "drunkdeer_backend.h"
#include "drunkdeer_protocol.h"
#include "analog_key_codes.h"
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
#include <cwchar>
#include <mutex>
#include <process.h>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr std::uint16_t kVendorId = 0x352d;
constexpr DWORD kIoTimeoutMs = 250;
constexpr DWORD kReconnectMs = 1000;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr DWORD kRollupMs = 5000;
constexpr DWORD kTransientRetryDelayMs = 5;
constexpr unsigned kTransientFailuresBeforeReopen = 12;
constexpr std::uint64_t kStaleNeutralizeMs = 350;

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
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
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
    bool hasReportId4 = false;
};

struct Proof
{
    drunkdeer::TrackingFrame frame{};
};

struct StreamDiagnostic
{
    std::uint64_t ignoredReports = 0;
    std::uint64_t sequenceRestarts = 0;
    std::uint64_t outOfOrderReports = 0;
};

struct CellDiagnostic
{
    std::uint64_t presses = 0;
    std::uint64_t releases = 0;
    std::uint64_t totalChangedSamples = 0;
    std::uint64_t pressStartedMs = 0;
    std::uint64_t pressFrames = 0;
    std::uint64_t pressChanges = 0;
    std::uint64_t levelMask = 0;
    std::uint8_t pressMin = 0xff;
    std::uint8_t pressMax = 0;
    std::uint8_t overallMin = 0xff;
    std::uint8_t overallMax = 0;
    std::uint32_t aboveMaskRange = 0;
};

struct HeaderDiagnostic
{
    bool initialized = false;
    std::array<std::uint8_t, drunkdeer::kHeaderBytes *
        drunkdeer::kReportCount> first{};
    std::array<std::uint8_t, drunkdeer::kHeaderBytes *
        drunkdeer::kReportCount> latest{};
    std::uint64_t changes = 0;
};

struct PayloadDiagnostic
{
    bool initialized = false;
    drunkdeer::Payload first{};
    drunkdeer::Payload latest{};
    std::array<std::uint64_t, drunkdeer::kPayloadBytes> changes{};
    std::array<std::uint8_t, drunkdeer::kPayloadBytes> minimum{};
    std::array<std::uint8_t, drunkdeer::kPayloadBytes> maximum{};
    std::uint64_t changedFrames = 0;
};

std::atomic<bool> g_prepared{ false }, g_running{ false }, g_stop{ false };
std::atomic<bool> g_present{ false }, g_connected{ false };
std::mutex g_serviceMutex, g_handleMutex, g_signalMutex;
std::mutex g_payloadSnapshotMutex;
HANDLE g_thread = nullptr, g_wake = nullptr;
HANDLE g_activeHandle = INVALID_HANDLE_VALUE;
drunkdeer::Payload g_payloadSnapshot{};
std::uint64_t g_payloadSnapshotGeneration = 0;
std::uint64_t g_payloadSnapshotMs = 0;
std::array<std::atomic<std::uint16_t>, halljoy::keycode::kCount> g_milli{};
std::array<std::atomic<std::uint8_t>, halljoy::keycode::kCount> g_owned{};
std::atomic<std::uint16_t> g_productId{ 0 };
std::atomic<std::uint16_t> g_usagePage{ 0 }, g_usage{ 0 };
std::atomic<std::uint32_t> g_inputBytes{ 0 }, g_outputBytes{ 0 };
std::atomic<std::uint32_t> g_active{ 0 }, g_hz10{ 0 };
std::atomic<std::uint32_t> g_avgUs{ 0 }, g_maxUs{ 0 };
std::atomic<std::uint64_t> g_frames{ 0 }, g_failures{ 0 }, g_lastMs{ 0 };
std::atomic<halljoy::worker::WorkerExceptionKind> g_fault{
    halljoy::worker::WorkerExceptionKind::None };

std::uint64_t HashInsensitive(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value)
    {
        hash ^= static_cast<std::uint16_t>(towlower(ch));
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint64_t HashBytes(const std::uint8_t* bytes, std::size_t count)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t index = 0; index < count; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

const wchar_t* KnownModel(std::uint16_t productId)
{
    switch (productId)
    {
    case 0x2382: return L"G65";
    case 0x2383: return L"A75";
    case 0x2384: return L"G60";
    case 0x2386: return L"G75-ANSI";
    case 0x2391: return L"G75-JP";
    default: return L"unknown";
    }
}

std::wstring HidString(HANDLE handle,
    BOOLEAN (__stdcall *getter)(HANDLE, PVOID, ULONG))
{
    wchar_t text[256]{};
    return getter(handle, text, sizeof(text)) ? std::wstring(text) :
        std::wstring();
}

bool HasReportId(PHIDP_PREPARSED_DATA preparsed, const HIDP_CAPS& caps,
    std::uint8_t wanted)
{
    const auto buttonHas = [&](HIDP_REPORT_TYPE type, USHORT count) {
        if (!count) return false;
        std::vector<HIDP_BUTTON_CAPS> values(count);
        if (HidP_GetButtonCaps(type, values.data(), &count, preparsed) !=
            HIDP_STATUS_SUCCESS)
            return false;
        return std::any_of(values.begin(), values.begin() + count,
            [wanted](const HIDP_BUTTON_CAPS& value) {
                return value.ReportID == wanted;
            });
    };
    const auto valueHas = [&](HIDP_REPORT_TYPE type, USHORT count) {
        if (!count) return false;
        std::vector<HIDP_VALUE_CAPS> values(count);
        if (HidP_GetValueCaps(type, values.data(), &count, preparsed) !=
            HIDP_STATUS_SUCCESS)
            return false;
        return std::any_of(values.begin(), values.begin() + count,
            [wanted](const HIDP_VALUE_CAPS& value) {
                return value.ReportID == wanted;
            });
    };
    return buttonHas(HidP_Input, caps.NumberInputButtonCaps) ||
        valueHas(HidP_Input, caps.NumberInputValueCaps) ||
        buttonHas(HidP_Output, caps.NumberOutputButtonCaps) ||
        valueHas(HidP_Output, caps.NumberOutputValueCaps) ||
        buttonHas(HidP_Feature, caps.NumberFeatureButtonCaps) ||
        valueHas(HidP_Feature, caps.NumberFeatureValueCaps);
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
        const DWORD waitError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT :
            GetLastError();
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
            detail->DevicePath, NativeAnalogProtocol::DrunkDeerMatrixB6);
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
            candidate.attributes.VendorID != kVendorId)
            continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        const NTSTATUS capsStatus = HidP_GetCaps(preparsed, &candidate.caps);
        if (capsStatus == HIDP_STATUS_SUCCESS)
            candidate.hasReportId4 = HasReportId(preparsed, candidate.caps,
                drunkdeer::kReportId);
        HidD_FreePreparsedData(preparsed);
        if (capsStatus != HIDP_STATUS_SUCCESS) continue;

        const bool protocolShape = candidate.hasReportId4 &&
            candidate.caps.InputReportByteLength == drunkdeer::kReportBytes &&
            candidate.caps.OutputReportByteLength == drunkdeer::kReportBytes;
        if (verbose)
        {
            candidate.manufacturer = HidString(metadata.value,
                HidD_GetManufacturerString);
            candidate.product = HidString(metadata.value,
                HidD_GetProductString);
            const std::wstring serial = HidString(metadata.value,
                HidD_GetSerialNumberString);
            DebugLog_Write(L"[drunkdeer.enumeration] path_hash=%016llX vid=%04X pid=%04X soup_model=%ls version=%04X usage=%04X:%04X in=%u out=%u feature=%u report_id_04=%d manufacturer=%ls product=%ls serial_present=%d serial_hash=%016llX protocol_shape=%d routed=%d",
                static_cast<unsigned long long>(HashInsensitive(candidate.path)),
                candidate.attributes.VendorID, candidate.attributes.ProductID,
                KnownModel(candidate.attributes.ProductID),
                candidate.attributes.VersionNumber, candidate.caps.UsagePage,
                candidate.caps.Usage, candidate.caps.InputReportByteLength,
                candidate.caps.OutputReportByteLength,
                candidate.caps.FeatureReportByteLength,
                candidate.hasReportId4 ? 1 : 0,
                candidate.manufacturer.empty() ? L"-" :
                    candidate.manufacturer.c_str(),
                candidate.product.empty() ? L"-" : candidate.product.c_str(),
                serial.empty() ? 0 : 1,
                static_cast<unsigned long long>(serial.empty() ? 0 :
                    HashInsensitive(serial)), protocolShape ? 1 : 0,
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
    explicit Session(const Candidate& candidate) : candidate_(candidate) {}
    ~Session()
    {
        (void)StopTracking();
        ReleaseActive();
    }

    bool Open()
    {
        handle_ = Handle(CreateFileW(candidate_.path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false;
        HidD_SetNumInputBuffers(handle_.value, 128);
        protocolMutex_ = Handle(CreateMutexW(nullptr, FALSE, L"DrunkDeerMtx"));
        RegisterActive();
        return true;
    }

    bool StopTracking()
    {
        if (!tracking_) return true;
        const DWORD mutexWait = protocolMutex_
            ? WaitForSingleObject(protocolMutex_.value, kIoTimeoutMs)
            : WAIT_OBJECT_0;
        if (mutexWait != WAIT_OBJECT_0 && mutexWait != WAIT_ABANDONED)
        {
            SetLastError(mutexWait == WAIT_TIMEOUT ? WAIT_TIMEOUT :
                ERROR_LOCK_FAILED);
            tracking_ = false;
            return false;
        }
        const bool releaseMutex = protocolMutex_ &&
            (mutexWait == WAIT_OBJECT_0 || mutexWait == WAIT_ABANDONED);
        const bool stopped = SendTrackingControlUnlocked(false);
        if (releaseMutex) ReleaseMutex(protocolMutex_.value);
        tracking_ = false;
        return stopped;
    }

    bool RequestTrackingFrame(drunkdeer::Reports* out,
        StreamDiagnostic* diagnostic)
    {
        if (!out || !handle_) return false;
        *out = {};
        const DWORD mutexWait = protocolMutex_
            ? WaitForSingleObject(protocolMutex_.value, kIoTimeoutMs)
            : WAIT_OBJECT_0;
        if (mutexWait != WAIT_OBJECT_0 && mutexWait != WAIT_ABANDONED)
        {
            SetLastError(mutexWait == WAIT_TIMEOUT ? WAIT_TIMEOUT :
                ERROR_LOCK_FAILED);
            return false;
        }
        const bool releaseMutex = protocolMutex_ &&
            (mutexWait == WAIT_OBJECT_0 || mutexWait == WAIT_ABANDONED);
        HidD_FlushQueue(handle_.value);
        if (!SendTrackingControlUnlocked(true))
        {
            if (releaseMutex) ReleaseMutex(protocolMutex_.value);
            return false;
        }
        tracking_ = true;
        drunkdeer::TrackingFrameAssembler assembler;
        unsigned reportsExamined = 0;
        while (reportsExamined < 128)
        {
            ++reportsExamined;
            drunkdeer::Report report{};
            DWORD received = 0;
            if (!TimedIo(handle_.value, false, report.data(),
                    static_cast<DWORD>(report.size()), kIoTimeoutMs,
                    &received) || received != report.size())
            {
                if (releaseMutex) ReleaseMutex(protocolMutex_.value);
                return false;
            }

            const auto result = assembler.Consume(report);
            if (result == drunkdeer::TrackingConsumeResult::Ignored)
            {
                if (diagnostic) ++diagnostic->ignoredReports;
                continue;
            }
            if (result == drunkdeer::TrackingConsumeResult::Restarted)
            {
                if (diagnostic) ++diagnostic->sequenceRestarts;
                continue;
            }
            if (result == drunkdeer::TrackingConsumeResult::OutOfOrder)
            {
                if (diagnostic) ++diagnostic->outOfOrderReports;
                continue;
            }
            if (result == drunkdeer::TrackingConsumeResult::Complete)
            {
                *out = assembler.OrderedReports();
                if (releaseMutex) ReleaseMutex(protocolMutex_.value);
                return true;
            }
        }
        if (releaseMutex) ReleaseMutex(protocolMutex_.value);
        SetLastError(ERROR_INVALID_DATA);
        return false;
    }

private:
    bool SendTrackingControlUnlocked(bool enabled)
    {
        auto request = drunkdeer::BuildTrackingRequest(enabled);
        DWORD sent = 0;
        const bool ok = TimedIo(handle_.value, true, request.data(),
            static_cast<DWORD>(request.size()), kIoTimeoutMs, &sent) &&
            sent == request.size();
        return ok;
    }

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
    Handle handle_{};
    Handle protocolMutex_{};
    bool active_ = false;
    bool tracking_ = false;
};

std::wstring HeadersHex(const drunkdeer::Reports& reports)
{
    wchar_t text[128]{};
    std::size_t used = 0;
    for (std::size_t report = 0; report < reports.size(); ++report)
    {
        for (std::size_t byte = 0; byte < drunkdeer::kHeaderBytes; ++byte)
        {
            used += static_cast<std::size_t>(_snwprintf_s(text + used,
                _countof(text) - used, _TRUNCATE, L"%02X%s",
                reports[report][byte], byte + 1 == drunkdeer::kHeaderBytes
                    ? (report + 1 == reports.size() ? L"" : L"|") : L" "));
        }
    }
    return text;
}

std::wstring PayloadChunkHex(const drunkdeer::Payload& payload,
    std::size_t chunk)
{
    wchar_t text[drunkdeer::kPayloadBytesPerReport * 3 + 1]{};
    std::size_t used = 0;
    const std::size_t begin = chunk * drunkdeer::kPayloadBytesPerReport;
    const std::size_t end = begin + drunkdeer::kPayloadBytesPerReport;
    for (std::size_t index = begin; index < end; ++index)
    {
        used += static_cast<std::size_t>(_snwprintf_s(text + used,
            _countof(text) - used, _TRUNCATE, L"%02X%s", payload[index],
            index + 1 == end ? L"" : L" "));
    }
    return text;
}

void LogPayloadSnapshotCritical(const wchar_t* reason, std::uint16_t hid,
    const drunkdeer::Payload& payload, std::uint64_t generation,
    std::uint64_t ageMs)
{
    const auto chunk0 = PayloadChunkHex(payload, 0);
    const auto chunk1 = PayloadChunkHex(payload, 1);
    const auto chunk2 = PayloadChunkHex(payload, 2);
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
        L"payload.snapshot",
        L"reason=%ls hid=%04X generation=%llu age_ms=%llu bytes=177 chunk0=%ls chunk1=%ls chunk2=%ls",
        reason ? reason : L"unknown", hid,
        static_cast<unsigned long long>(generation),
        static_cast<unsigned long long>(ageMs), chunk0.c_str(),
        chunk1.c_str(), chunk2.c_str());
}

std::uint64_t PublishPayloadSnapshot(const drunkdeer::Payload& payload)
{
    std::lock_guard<std::mutex> lock(g_payloadSnapshotMutex);
    g_payloadSnapshot = payload;
    g_payloadSnapshotMs = GetTickCount64();
    return ++g_payloadSnapshotGeneration;
}

void CopyPayloadSnapshot(drunkdeer::Payload* payload,
    std::uint64_t* generation, std::uint64_t* ageMs)
{
    if (!payload || !generation || !ageMs) return;
    std::lock_guard<std::mutex> lock(g_payloadSnapshotMutex);
    *payload = g_payloadSnapshot;
    *generation = g_payloadSnapshotGeneration;
    const auto now = GetTickCount64();
    *ageMs = g_payloadSnapshotMs && now >= g_payloadSnapshotMs
        ? now - g_payloadSnapshotMs : 0;
}


void FlattenHeaders(const drunkdeer::Reports& reports,
    std::array<std::uint8_t, drunkdeer::kHeaderBytes *
        drunkdeer::kReportCount>* out)
{
    if (!out) return;
    std::size_t destination = 0;
    for (const auto& report : reports)
        for (std::size_t byte = 0; byte < drunkdeer::kHeaderBytes; ++byte)
            (*out)[destination++] = report[byte];
}

bool Prove(const Candidate& candidate, Proof* out)
{
    Session session(candidate);
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer", L"proof.begin",
        L"path_hash=%016llX pid=%04X model_hint=%ls request=04-B6-03-01 semantics=request_one_matrix_frame",
        static_cast<unsigned long long>(HashInsensitive(candidate.path)),
        candidate.attributes.ProductID, KnownModel(candidate.attributes.ProductID));
    if (!session.Open())
    {
        DebugLog_Write(L"[drunkdeer.proof] open_failed win32=%lu", GetLastError());
        return false;
    }
    Proof proof{};
    StreamDiagnostic stream{};
    drunkdeer::Reports reports{};
    if (!session.RequestTrackingFrame(&reports, &stream) ||
        !drunkdeer::DecodeTrackingFrame(reports, &proof.frame))
    {
        StabilityTrace_WriteCritical(L"ERROR", L"drunkdeer",
            L"proof.response_failed",
            L"native_error=%lu expected=request_then_B7_chunks_0_1_2 requests=1",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer", L"proof.pass",
        L"headers=%ls payload_bytes=%llu matrix_bytes=59+59+8 request_commands=1 ignored_reports=%llu sequence_restarts=%llu out_of_order=%llu",
        HeadersHex(proof.frame.reports).c_str(),
        static_cast<unsigned long long>(proof.frame.payload.size()),
        static_cast<unsigned long long>(stream.ignoredReports),
        static_cast<unsigned long long>(stream.sequenceRestarts),
        static_cast<unsigned long long>(stream.outOfOrderReports));
    LogPayloadSnapshotCritical(L"proof", 0, proof.frame.payload, 0, 0);
    if (out) *out = proof;
    return true;
}

bool EnsureClaim(const Candidate& candidate, Proof* proof)
{
    Proof validated{};
    if (!Prove(candidate, &validated)) return false;
    if (!NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),
            NativeAnalogProtocol::DrunkDeerMatrixB6) &&
        !NativeAnalogRouting_Claim(candidate.attributes.VendorID,
            candidate.attributes.ProductID, candidate.path.c_str(),
            NativeAnalogProtocol::DrunkDeerMatrixB6))
        return false;
    if (proof) *proof = validated;
    return true;
}

bool NeutralizeValues()
{
    bool changed = false;
    for (auto& value : g_milli)
        if (value.exchange(0, std::memory_order_relaxed)) changed = true;
    g_active.store(0, std::memory_order_relaxed);
    if (changed) RealtimeLoop_NotifyInputChanged();
    return changed;
}

void Clear()
{
    (void)NeutralizeValues();
    for (auto& owned : g_owned) owned.store(0, std::memory_order_relaxed);
}

bool IsDefinitiveDisconnect(DWORD error) noexcept
{
    return error == ERROR_DEVICE_NOT_CONNECTED ||
        error == ERROR_INVALID_HANDLE || error == ERROR_GEN_FAILURE ||
        error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

void PublishOwnership(const drunkdeer::PositionToHid& map)
{
    for (const auto code : map)
        if (code && code < g_owned.size())
            g_owned[code].store(1, std::memory_order_relaxed);
}

void FinishPress(std::size_t position, std::uint16_t mappedCode,
    CellDiagnostic& diagnostic, bool interrupted, std::uint64_t nowMs)
{
    const auto row = position / drunkdeer::kColumns;
    const auto column = position % drunkdeer::kColumns;
        DebugLog_WriteBuffered(L"[drunkdeer.raw_cell.release] row=%llu column=%llu compiled_map_code=%04X duration_ms=%llu frames=%llu changed_samples=%llu min_raw=%u max_raw=%u levels_0_63=%016llX above_63=%u interrupted=%d",
        static_cast<unsigned long long>(row),
        static_cast<unsigned long long>(column), mappedCode,
        static_cast<unsigned long long>(nowMs - diagnostic.pressStartedMs),
        static_cast<unsigned long long>(diagnostic.pressFrames),
        static_cast<unsigned long long>(diagnostic.pressChanges),
        diagnostic.pressMin == 0xff ? 0 : diagnostic.pressMin,
        diagnostic.pressMax,
        static_cast<unsigned long long>(diagnostic.levelMask),
        diagnostic.aboveMaskRange, interrupted ? 1 : 0);
    if (!interrupted) ++diagnostic.releases;
    diagnostic.pressStartedMs = 0;
    diagnostic.pressFrames = 0;
    diagnostic.pressChanges = 0;
    diagnostic.levelMask = 0;
    diagnostic.pressMin = 0xff;
    diagnostic.pressMax = 0;
    diagnostic.aboveMaskRange = 0;
}

void ProcessMatrix(const drunkdeer::Matrix& matrix,
    const drunkdeer::PositionToHid& map, drunkdeer::Matrix& previous,
    std::array<CellDiagnostic, drunkdeer::kCellCount>& diagnostics)
{
    bool changedForRealtime = false;
    std::uint32_t activeCells = 0;
    const auto nowMs = GetTickCount64();
    for (std::size_t position = 0; position < matrix.size(); ++position)
    {
        const auto raw = matrix[position];
        const auto oldRaw = previous[position];
        const auto code = map[position];
        auto& diagnostic = diagnostics[position];
        if (raw) ++activeCells;

        if (raw && !oldRaw)
        {
            ++diagnostic.presses;
            diagnostic.pressStartedMs = nowMs;
            diagnostic.pressFrames = 1;
            diagnostic.pressChanges = 1;
            diagnostic.pressMin = raw;
            diagnostic.pressMax = raw;
            diagnostic.overallMin = std::min(diagnostic.overallMin, raw);
            diagnostic.overallMax = std::max(diagnostic.overallMax, raw);
            if (raw < 64) diagnostic.levelMask |= 1ull << raw;
            else ++diagnostic.aboveMaskRange;
            DebugLog_WriteBuffered(L"[drunkdeer.raw_cell.press] row=%llu column=%llu raw=%u compiled_map_code=%04X mapped=%d",
                static_cast<unsigned long long>(position /
                    drunkdeer::kColumns),
                static_cast<unsigned long long>(position %
                    drunkdeer::kColumns), raw, code, code ? 1 : 0);
        }
        else if (raw)
        {
            ++diagnostic.pressFrames;
            diagnostic.pressMin = std::min(diagnostic.pressMin, raw);
            diagnostic.pressMax = std::max(diagnostic.pressMax, raw);
            diagnostic.overallMin = std::min(diagnostic.overallMin, raw);
            diagnostic.overallMax = std::max(diagnostic.overallMax, raw);
            if (raw < 64) diagnostic.levelMask |= 1ull << raw;
            else ++diagnostic.aboveMaskRange;
            if (raw != oldRaw)
            {
                ++diagnostic.pressChanges;
                ++diagnostic.totalChangedSamples;
            }
        }
        else if (oldRaw)
        {
            ++diagnostic.totalChangedSamples;
            FinishPress(position, code, diagnostic, false, nowMs);
        }

        if (code && code < g_milli.size())
        {
            const auto milli = drunkdeer::ToMilli(raw);
            const auto oldMilli = g_milli[code].exchange(milli,
                std::memory_order_relaxed);
            if (oldMilli != milli) changedForRealtime = true;
        }
        previous[position] = raw;
    }
    g_active.store(activeCells, std::memory_order_relaxed);
    g_lastMs.store(nowMs, std::memory_order_relaxed);
    if (changedForRealtime) RealtimeLoop_NotifyInputChanged();
}

void ObserveHeaders(const drunkdeer::Reports& reports,
    HeaderDiagnostic& diagnostic)
{
    std::array<std::uint8_t, drunkdeer::kHeaderBytes *
        drunkdeer::kReportCount> current{};
    FlattenHeaders(reports, &current);
    if (!diagnostic.initialized)
    {
        diagnostic.initialized = true;
        diagnostic.first = current;
        diagnostic.latest = current;
        DebugLog_Write(L"[drunkdeer.report_headers.initial] data=%ls",
            HeadersHex(reports).c_str());
        return;
    }
    if (current != diagnostic.latest)
    {
        ++diagnostic.changes;
        diagnostic.latest = current;
    }
}

void ObservePayload(const drunkdeer::Payload& payload,
    PayloadDiagnostic& diagnostic)
{
    const auto generation = PublishPayloadSnapshot(payload);
    if (!diagnostic.initialized)
    {
        diagnostic.initialized = true;
        diagnostic.first = payload;
        diagnostic.latest = payload;
        diagnostic.minimum = payload;
        diagnostic.maximum = payload;
        LogPayloadSnapshotCritical(L"session_initial", 0, payload,
            generation, 0);
        return;
    }

    wchar_t delta[1400]{};
    std::size_t used = 0;
    std::size_t changed = 0;
    std::size_t listed = 0;
    std::size_t nonzero = 0;
    std::size_t firstChanged = payload.size();
    std::size_t lastChanged = 0;
    std::uint8_t maximum = 0;
    for (std::size_t index = 0; index < payload.size(); ++index)
    {
        const auto value = payload[index];
        if (value) ++nonzero;
        maximum = std::max(maximum, value);
        diagnostic.minimum[index] = std::min(diagnostic.minimum[index], value);
        diagnostic.maximum[index] = std::max(diagnostic.maximum[index], value);
        if (value == diagnostic.latest[index]) continue;
        ++changed;
        ++diagnostic.changes[index];
        firstChanged = std::min(firstChanged, index);
        lastChanged = index;
        if (listed < 48 && used + 28 < _countof(delta))
        {
            const int appended = _snwprintf_s(delta + used,
                _countof(delta) - used, _TRUNCATE, L"%s%llu:%u>%u",
                listed ? L"," : L"",
                static_cast<unsigned long long>(index),
                diagnostic.latest[index], value);
            if (appended > 0) used += static_cast<std::size_t>(appended);
            ++listed;
        }
    }
    if (changed)
    {
        ++diagnostic.changedFrames;
        DebugLog_WriteBuffered(L"[drunkdeer.payload.delta] generation=%llu changed=%llu listed=%llu nonzero=%llu max=%u first_offset=%llu last_offset=%llu payload_hash=%016llX values=%ls",
            static_cast<unsigned long long>(generation),
            static_cast<unsigned long long>(changed),
            static_cast<unsigned long long>(listed),
            static_cast<unsigned long long>(nonzero), maximum,
            static_cast<unsigned long long>(firstChanged),
            static_cast<unsigned long long>(lastChanged),
            static_cast<unsigned long long>(HashBytes(payload.data(),
                payload.size())), delta);
        diagnostic.latest = payload;
    }
}

void FinishPayloadDiagnostic(const PayloadDiagnostic& diagnostic)
{
    if (!diagnostic.initialized) return;
    std::size_t changedOffsets = 0;
    for (std::size_t index = 0; index < diagnostic.changes.size(); ++index)
    {
        if (!diagnostic.changes[index]) continue;
        ++changedOffsets;
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"payload.offset_summary",
            L"offset=%llu chunk=%llu chunk_offset=%llu changes=%llu min=%u max=%u",
            static_cast<unsigned long long>(index),
            static_cast<unsigned long long>(
                index / drunkdeer::kPayloadBytesPerReport),
            static_cast<unsigned long long>(
                index % drunkdeer::kPayloadBytesPerReport),
            static_cast<unsigned long long>(diagnostic.changes[index]),
            diagnostic.minimum[index], diagnostic.maximum[index]);
    }
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
        L"payload.summary",
        L"bytes=%llu changed_frames=%llu changed_offsets=%llu first_hash=%016llX latest_hash=%016llX",
        static_cast<unsigned long long>(diagnostic.latest.size()),
        static_cast<unsigned long long>(diagnostic.changedFrames),
        static_cast<unsigned long long>(changedOffsets),
        static_cast<unsigned long long>(HashBytes(diagnostic.first.data(),
            diagnostic.first.size())),
        static_cast<unsigned long long>(HashBytes(diagnostic.latest.data(),
            diagnostic.latest.size())));
}

bool Run(const Candidate& candidate)
{
    Session session(candidate);
    if (!session.Open()) return false;
    StreamDiagnostic stream{};
    drunkdeer::Reports firstReports{};
    drunkdeer::TrackingFrame firstFrame{};
    unsigned startupFailures = 0;
    while (!g_stop.load(std::memory_order_acquire))
    {
        const bool requested =
            session.RequestTrackingFrame(&firstReports, &stream);
        const bool decoded = requested &&
            drunkdeer::DecodeTrackingFrame(firstReports, &firstFrame);
        if (decoded) break;
        const DWORD error = requested ? ERROR_INVALID_DATA : GetLastError();
        ++startupFailures;
        StabilityTrace_WriteCritical(L"WARN", L"drunkdeer",
            L"transport.startup_retry",
            L"attempt=%u native_error=%lu same_handle=1",
            startupFailures, static_cast<unsigned long>(error));
        if (IsDefinitiveDisconnect(error) ||
            startupFailures >= kTransientFailuresBeforeReopen)
            return false;
        WaitForSingleObject(g_wake, kTransientRetryDelayMs);
    }
    if (g_stop.load(std::memory_order_acquire)) return true;

    const auto map = drunkdeer::MapForProduct(candidate.attributes.ProductID);
    const bool isG65 = candidate.attributes.ProductID == 0x2382;
    const wchar_t* mapName = isG65
        ? L"g65_antler_nav_v3" : L"generic_uap_unverified";
    const wchar_t* mapSelection = isG65
        ? L"pid_2382" : L"diagnostic_fallback";
    g_productId.store(candidate.attributes.ProductID);
    g_usagePage.store(candidate.caps.UsagePage);
    g_usage.store(candidate.caps.Usage);
    g_inputBytes.store(candidate.caps.InputReportByteLength);
    g_outputBytes.store(candidate.caps.OutputReportByteLength);
    Clear();
    PublishOwnership(map);
    g_connected.store(true, std::memory_order_release);
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
        L"session.connected",
        L"vid=352D pid=%04X model_hint=%ls usage=%04X:%04X path_hash=%016llX cells=6x21 mapped=%llu scale=0..40 transport=per_frame_uap_transaction request_commands_per_frame=1 matrix_chunks=59+59+8 map=%ls map_selection=%ls digital_mapping_dependency=0",
        candidate.attributes.ProductID, KnownModel(candidate.attributes.ProductID),
        candidate.caps.UsagePage, candidate.caps.Usage,
        static_cast<unsigned long long>(HashInsensitive(candidate.path)),
        static_cast<unsigned long long>(drunkdeer::MappedKeyCount(map)),
        mapName, mapSelection);

    drunkdeer::Matrix previous{};
    std::array<CellDiagnostic, drunkdeer::kCellCount> diagnostics{};
    HeaderDiagnostic headers{};
    PayloadDiagnostic payloads{};
    ObserveHeaders(firstReports, headers);
    ObservePayload(firstFrame.payload, payloads);
    ProcessMatrix(firstFrame.matrix, map, previous, diagnostics);

    std::uint64_t successful = 1;
    std::uint64_t failed = 0;
    std::uint64_t frameRequests = 1;
    std::uint64_t lastRollup = GetTickCount64();
    std::uint64_t rollupFrames = 0;
    std::uint64_t intervalSumUs = 0;
    std::uint64_t intervalCount = 0;
    std::uint64_t maximumIntervalUs = 0;
    LARGE_INTEGER frequency{}, previousQpc{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previousQpc);
    unsigned consecutiveFailures = 0;
    std::uint64_t failureStartedMs = 0;
    std::uint64_t sameHandleRecoveries = 0;
    std::uint64_t staleNeutralizations = 0;
    DWORD reopenError = ERROR_SUCCESS;

    while (!g_stop.load(std::memory_order_acquire))
    {
        drunkdeer::Reports reports{};
        drunkdeer::TrackingFrame frame{};
        ++frameRequests;
        const bool requested = session.RequestTrackingFrame(&reports, &stream);
        const bool decoded = requested &&
            drunkdeer::DecodeTrackingFrame(reports, &frame);
        if (!decoded)
        {
            const DWORD error = requested ? ERROR_INVALID_DATA : GetLastError();
            if (g_stop.load(std::memory_order_acquire) &&
                (error == ERROR_OPERATION_ABORTED ||
                    error == ERROR_INVALID_HANDLE))
                break;
            ++failed;
            ++consecutiveFailures;
            const auto failureNowMs = GetTickCount64();
            if (!failureStartedMs) failureStartedMs = failureNowMs;
            g_failures.fetch_add(1, std::memory_order_relaxed);
            const auto lastGoodMs = g_lastMs.load(std::memory_order_relaxed);
            if (lastGoodMs && failureNowMs >= lastGoodMs &&
                failureNowMs - lastGoodMs >= kStaleNeutralizeMs &&
                NeutralizeValues())
            {
                ++staleNeutralizations;
                StabilityTrace_WriteCritical(L"WARN", L"drunkdeer",
                    L"transport.stale_neutralized",
                    L"age_ms=%llu consecutive_failures=%u handle_kept=1 ownership_kept=1",
                    static_cast<unsigned long long>(failureNowMs - lastGoodMs),
                    consecutiveFailures);
            }
            const bool definitive = IsDefinitiveDisconnect(error);
            StabilityTrace_WriteCritical(L"WARN", L"drunkdeer",
                L"transport.retry",
                L"native_error=%lu consecutive_failures=%u definitive_disconnect=%d same_handle=1 retry_delay_ms=%lu",
                static_cast<unsigned long>(error), consecutiveFailures,
                definitive ? 1 : 0,
                static_cast<unsigned long>(kTransientRetryDelayMs));
            if (definitive ||
                consecutiveFailures >= kTransientFailuresBeforeReopen)
            {
                reopenError = error;
                StabilityTrace_WriteCritical(L"ERROR", L"drunkdeer",
                    L"transport.reopen_required",
                    L"native_error=%lu consecutive_failures=%u definitive_disconnect=%d transient_limit=%u",
                    static_cast<unsigned long>(error), consecutiveFailures,
                    definitive ? 1 : 0, kTransientFailuresBeforeReopen);
                break;
            }
            WaitForSingleObject(g_wake, kTransientRetryDelayMs);
            continue;
        }
        if (consecutiveFailures)
        {
            ++sameHandleRecoveries;
            const auto recoveredMs = GetTickCount64();
            StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
                L"transport.recovered",
                L"failed_attempts=%u recovery_ms=%llu same_handle=1 reopen=0",
                consecutiveFailures,
                static_cast<unsigned long long>(failureStartedMs &&
                    recoveredMs >= failureStartedMs
                    ? recoveredMs - failureStartedMs : 0));
        }
        consecutiveFailures = 0;
        failureStartedMs = 0;
        ++successful;
        ++rollupFrames;
        g_frames.fetch_add(1, std::memory_order_relaxed);
        LARGE_INTEGER nowQpc{};
        QueryPerformanceCounter(&nowQpc);
        if (frequency.QuadPart > 0 && previousQpc.QuadPart > 0)
        {
            const auto intervalUs = static_cast<std::uint64_t>(
                (nowQpc.QuadPart - previousQpc.QuadPart) * 1000000ll /
                    frequency.QuadPart);
            intervalSumUs += intervalUs;
            ++intervalCount;
            maximumIntervalUs = std::max(maximumIntervalUs, intervalUs);
        }
        previousQpc = nowQpc;
        ObserveHeaders(reports, headers);
        ObservePayload(frame.payload, payloads);
        ProcessMatrix(frame.matrix, map, previous, diagnostics);

        const auto nowMs = GetTickCount64();
        if (nowMs - lastRollup >= kRollupMs)
        {
            const auto elapsed = std::max<std::uint64_t>(1, nowMs - lastRollup);
            g_hz10.store(static_cast<std::uint32_t>(
                rollupFrames * 10000ull / elapsed));
            if (intervalCount)
            {
                g_avgUs.store(static_cast<std::uint32_t>(
                    intervalSumUs / intervalCount));
                g_maxUs.store(static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(maximumIntervalUs, 0xffffffffull)));
            }
            DebugLog_WriteBuffered(L"[drunkdeer.rollup] elapsed_ms=%llu frame_hz=%.1f successful=%llu failed=%llu active_cells=%u header_changes=%llu latest_header_hash=%016llX payload_changed_frames=%llu payload_hash=%016llX ignored_reports=%llu sequence_restarts=%llu out_of_order=%llu avg_interval_us=%u max_interval_us=%u",
                static_cast<unsigned long long>(elapsed), g_hz10.load() / 10.0,
                static_cast<unsigned long long>(successful),
                static_cast<unsigned long long>(failed), g_active.load(),
                static_cast<unsigned long long>(headers.changes),
                static_cast<unsigned long long>(HashBytes(headers.latest.data(),
                    headers.latest.size())),
                static_cast<unsigned long long>(payloads.changedFrames),
                static_cast<unsigned long long>(HashBytes(payloads.latest.data(),
                    payloads.latest.size())),
                static_cast<unsigned long long>(stream.ignoredReports),
                static_cast<unsigned long long>(stream.sequenceRestarts),
                static_cast<unsigned long long>(stream.outOfOrderReports),
                g_avgUs.load(), g_maxUs.load());
            lastRollup = nowMs;
            rollupFrames = 0;
            intervalSumUs = intervalCount = maximumIntervalUs = 0;
        }
    }

    const auto endedMs = GetTickCount64();
    for (std::size_t position = 0; position < diagnostics.size(); ++position)
    {
        auto& diagnostic = diagnostics[position];
        if (diagnostic.pressStartedMs)
            FinishPress(position, map[position], diagnostic, true, endedMs);
        if (diagnostic.presses)
            DebugLog_WriteBuffered(L"[drunkdeer.raw_cell.summary] row=%llu column=%llu compiled_map_code=%04X presses=%llu releases=%llu changed_samples=%llu overall_min_raw=%u overall_max_raw=%u",
                static_cast<unsigned long long>(position /
                    drunkdeer::kColumns),
                static_cast<unsigned long long>(position %
                    drunkdeer::kColumns), map[position],
                static_cast<unsigned long long>(diagnostic.presses),
                static_cast<unsigned long long>(diagnostic.releases),
                static_cast<unsigned long long>(diagnostic.totalChangedSamples),
                diagnostic.overallMin == 0xff ? 0 : diagnostic.overallMin,
                diagnostic.overallMax);
    }
    FinishPayloadDiagnostic(payloads);
    StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
        L"session.summary",
        L"pid=%04X frame_requests=%llu successful=%llu failed=%llu header_changes=%llu first_header_hash=%016llX latest_header_hash=%016llX ignored_reports=%llu sequence_restarts=%llu out_of_order=%llu same_handle_recoveries=%llu stale_neutralizations=%llu stop_requested=%d reopen_error=%lu",
        candidate.attributes.ProductID,
        static_cast<unsigned long long>(frameRequests),
        static_cast<unsigned long long>(successful),
        static_cast<unsigned long long>(failed),
        static_cast<unsigned long long>(headers.changes),
        static_cast<unsigned long long>(HashBytes(headers.first.data(),
            headers.first.size())),
        static_cast<unsigned long long>(HashBytes(headers.latest.data(),
            headers.latest.size())),
        static_cast<unsigned long long>(stream.ignoredReports),
        static_cast<unsigned long long>(stream.sequenceRestarts),
        static_cast<unsigned long long>(stream.outOfOrderReports),
        static_cast<unsigned long long>(sameHandleRecoveries),
        static_cast<unsigned long long>(staleNeutralizations),
        g_stop.load(std::memory_order_acquire) ? 1 : 0,
        static_cast<unsigned long>(reopenError));
    if (!session.StopTracking())
        StabilityTrace_WriteCritical(L"ERROR", L"drunkdeer",
            L"tracking.stop_failed", L"native_error=%lu",
            static_cast<unsigned long>(GetLastError()));
    else
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"tracking.stopped", L"request=04-B6-03-00");
    return true;
}

std::uint32_t WorkerBody()
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        bool provenPresent = false;
        bool ran = false;
        for (const auto& candidate : Enumerate(false, true))
        {
            if (!NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(),
                    NativeAnalogProtocol::DrunkDeerMatrixB6))
            {
                Proof proof{};
                if (!EnsureClaim(candidate, &proof)) continue;
            }
            provenPresent = true;
            g_present.store(true, std::memory_order_release);
            ran = Run(candidate);
            if (ran) break;
        }
        if (!provenPresent) g_present.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
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
    StabilityTrace_WriteCritical(L"ERROR", L"drunkdeer", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerComplete(const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_WriteCritical(record.kind == halljoy::worker::WorkerExceptionKind::None
        ? L"INFO" : L"ERROR", L"drunkdeer", L"worker.exit",
        L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

unsigned __stdcall Worker(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerBody(); }, WorkerFault, WorkerComplete,
        0xE0352D04u));
}

bool Prepare()
{
    if (g_prepared.exchange(true, std::memory_order_acq_rel))
        return NativeAnalogRouting_ProtocolHasClaims(
            NativeAnalogProtocol::DrunkDeerMatrixB6);
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
    StabilityTrace_Write(L"INFO", L"drunkdeer", L"start.ok",
        L"thread_id=%u diagnostic_only=1 hotplug_ready=1", id);
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

bool Present() { return g_present.load(std::memory_order_acquire); }
bool Connected() { return g_connected.load(std::memory_order_acquire); }
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
    out->productId = g_productId.load();
    out->usagePage = g_usagePage.load();
    out->usage = g_usage.load();
    out->mappedKeys = static_cast<std::uint32_t>(
        drunkdeer::MappedKeyCount(drunkdeer::MapForProduct(
            g_productId.load())));
    out->activeKeys = g_active.load();
    out->nominalRawLevels = drunkdeer::kNominalTravelMaximum + 1u;
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->updateHz10 = g_hz10.load();
    out->averageIntervalUs = g_avgUs.load();
    out->maximumIntervalUs = g_maxUs.load();
    out->successfulUpdates = g_frames.load();
    out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last,
            0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE,
        L"DrunkDeer native diagnostic, PID %04X, raw 6x21, full static map incl Fn/Menu",
        g_productId.load());
}

#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
struct RawKeyboardDiagnostic
{
    bool target = false;
    bool identified = false;
    std::uint16_t productId = 0;
    std::uint64_t pathHash = 0;
    std::array<bool, 512> down{};
    std::array<std::uint32_t, 512> repeats{};
};

std::mutex g_rawKeyboardMutex;
std::unordered_map<std::uintptr_t, RawKeyboardDiagnostic> g_rawKeyboards;

bool ContainsInsensitive(const std::wstring& value, const wchar_t* needle)
{
    if (!needle || !*needle) return false;
    const std::size_t needleLength = wcslen(needle);
    for (std::size_t index = 0; index + needleLength <= value.size(); ++index)
        if (_wcsnicmp(value.c_str() + index, needle, needleLength) == 0)
            return true;
    return false;
}

std::uint16_t ParseHexAfter(const std::wstring& value, const wchar_t* marker)
{
    for (std::size_t index = 0; index + wcslen(marker) + 4 <= value.size();
        ++index)
    {
        if (_wcsnicmp(value.c_str() + index, marker, wcslen(marker)) != 0)
            continue;
        wchar_t digits[5]{};
        wcsncpy_s(digits, value.c_str() + index + wcslen(marker), 4);
        return static_cast<std::uint16_t>(wcstoul(digits, nullptr, 16));
    }
    return 0;
}

RawKeyboardDiagnostic IdentifyRawKeyboard(std::uintptr_t rawDevice)
{
    RawKeyboardDiagnostic result{};
    const HANDLE device = reinterpret_cast<HANDLE>(rawDevice);
    UINT chars = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars) ==
            static_cast<UINT>(-1) || !chars)
        return result;
    std::wstring path(chars + 1, L'\0');
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, path.data(), &chars) ==
        static_cast<UINT>(-1))
        return result;
    path.resize(wcsnlen(path.c_str(), path.size()));
    result.identified = true;
    result.target = ContainsInsensitive(path, L"vid_352d");
    result.productId = ParseHexAfter(path, L"pid_");
    result.pathHash = HashInsensitive(path);
    if (result.target)
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"digital.device",
            L"path_hash=%016llX vid=352D pid=%04X model_hint=%ls source=windows_raw_input mapping_dependency=0",
            static_cast<unsigned long long>(result.pathHash), result.productId,
            KnownModel(result.productId));
    return result;
}
#endif
}

const NativeAnalogBackendDescriptor& DrunkDeer_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "drunkdeer-matrix-b6-diagnostic",
        L"DrunkDeer matrix diagnostic (native)",
        NativeAnalogProtocol::DrunkDeerMatrixB6,
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
        &Telemetry,
    };
    return descriptor;
}

#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
void DrunkDeerDiagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice, std::uint16_t hidUsage,
    std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey)
{
    std::lock_guard<std::mutex> lock(g_rawKeyboardMutex);
    auto found = g_rawKeyboards.find(rawDevice);
    if (found == g_rawKeyboards.end())
        found = g_rawKeyboards.emplace(rawDevice,
            IdentifyRawKeyboard(rawDevice)).first;
    auto& device = found->second;
    if (!device.target) return;
    const bool isDown = (flags & RI_KEY_BREAK) == 0;
    const std::size_t key = hidUsage && hidUsage < 256 ? hidUsage :
        256u + (virtualKey & 0xffu);
    if (isDown)
    {
        if (device.down[key])
        {
            ++device.repeats[key];
            return;
        }
        device.down[key] = true;
        device.repeats[key] = 0;
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"digital.press",
            L"path_hash=%016llX hid=%04X make=%04X flags=%04X vkey=%04X",
            static_cast<unsigned long long>(device.pathHash), hidUsage,
            makeCode, flags, virtualKey);
        drunkdeer::Payload snapshot{};
        std::uint64_t generation = 0, ageMs = 0;
        CopyPayloadSnapshot(&snapshot, &generation, &ageMs);
        LogPayloadSnapshotCritical(L"digital_press", hidUsage, snapshot,
            generation, ageMs);
    }
    else
    {
        const bool hadPress = device.down[key];
        device.down[key] = false;
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"digital.release",
            L"path_hash=%016llX hid=%04X make=%04X flags=%04X vkey=%04X repeats_suppressed=%u press_seen=%d",
            static_cast<unsigned long long>(device.pathHash), hidUsage,
            makeCode, flags, virtualKey, device.repeats[key],
            hadPress ? 1 : 0);
        drunkdeer::Payload snapshot{};
        std::uint64_t generation = 0, ageMs = 0;
        CopyPayloadSnapshot(&snapshot, &generation, &ageMs);
        LogPayloadSnapshotCritical(L"digital_release", hidUsage, snapshot,
            generation, ageMs);
        device.repeats[key] = 0;
    }
}

void DrunkDeerDiagnostic_RecordRawDeviceChange(
    std::uintptr_t rawDevice, bool arrived)
{
    std::lock_guard<std::mutex> lock(g_rawKeyboardMutex);
    const auto found = g_rawKeyboards.find(rawDevice);
    if (found != g_rawKeyboards.end() && found->second.target)
        StabilityTrace_WriteCritical(L"INFO", L"drunkdeer",
            L"digital.device_change", L"path_hash=%016llX kind=%ls",
            static_cast<unsigned long long>(found->second.pathHash),
            arrived ? L"arrival" : L"removal");
    g_rawKeyboards.erase(rawDevice);
}
#endif
