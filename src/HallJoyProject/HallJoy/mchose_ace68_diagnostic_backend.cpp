#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "mchose_ace68_diagnostic_backend.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "worker_join_policy.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <map>
#include <mutex>
#include <process.h>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr std::uint16_t kVendorId = 0x41E4;
constexpr std::uint16_t kProductId = 0x2114;
constexpr USAGE kUsagePage = 0x0001;
constexpr USAGE kUsage = 0x0000;
constexpr std::size_t kReportBytes = 64;
constexpr DWORD kReadSliceMs = 100;
constexpr DWORD kCommandAckMs = 700;
constexpr DWORD kPassiveListenMs = 2500;
constexpr DWORD kObservationMs = 300000;
constexpr DWORD kReconnectMs = 1000;
constexpr DWORD kStopTimeoutMs = 3000;

using Report = std::array<std::uint8_t, kReportBytes>;
enum class Transport { InterruptWrite, OutputReport };
enum class AckResult { Accepted, Rejected, Timeout, SendFailed, UnexpectedAck };

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

struct Candidate { std::wstring path; HIDD_ATTRIBUTES attributes{}; HIDP_CAPS caps{}; };
struct Strategy { const wchar_t* name; std::uint8_t header; std::uint8_t xorKey; Transport transport; };
struct ReadRequest { const wchar_t* name; std::uint8_t opcode; std::uint16_t offset; std::uint8_t responseBytes; };
struct DescriptorStats { std::uint64_t packets = 0; std::uint16_t minimum = 0xffff; std::uint16_t maximum = 0; };
struct SessionStats
{
    std::uint64_t reports = 0, a0 = 0, aa = 0, ab = 0, other = 0, writes = 0, failures = 0;
    std::map<std::array<std::uint8_t, 3>, DescriptorStats> descriptorStats;
};

std::atomic<bool> g_prepared{false}, g_running{false}, g_stop{false}, g_present{false}, g_connected{false};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0};
std::atomic<std::uint64_t> g_reports{0}, g_a0{0}, g_failures{0}, g_lastMs{0};
std::mutex g_serviceMutex, g_handleMutex;
std::mutex g_rawKeyboardMutex;
std::unordered_map<std::uintptr_t, bool> g_rawKeyboardIdentity;
HANDLE g_thread = nullptr, g_wake = nullptr, g_activeHandle = INVALID_HANDLE_VALUE;

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) { hash ^= static_cast<std::uint16_t>(towlower(ch)); hash *= 1099511628211ull; }
    return hash;
}

bool IsMchoseRawKeyboard(std::uintptr_t rawDevice)
{
    const auto cached = g_rawKeyboardIdentity.find(rawDevice);
    if (cached != g_rawKeyboardIdentity.end()) return cached->second;
    const HANDLE device = reinterpret_cast<HANDLE>(rawDevice);
    UINT chars = 0;
    bool matched = false;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars) != static_cast<UINT>(-1) && chars)
    {
        std::wstring path(chars + 1, L'\0');
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, path.data(), &chars) != static_cast<UINT>(-1))
        {
            std::transform(path.begin(), path.end(), path.begin(), towupper);
            matched = path.find(L"VID_41E4&PID_2114") != std::wstring::npos;
        }
    }
    g_rawKeyboardIdentity.emplace(rawDevice, matched);
    DebugLog_Write(L"[mchose.ace68.raw_key.identity] device=%p matched=%d", device, matched ? 1 : 0);
    return matched;
}

std::wstring Hex(const std::uint8_t* bytes, std::size_t count)
{
    std::wstring text; text.reserve(count * 3); wchar_t cell[4]{};
    for (std::size_t i = 0; i < count; ++i)
    {
        _snwprintf_s(cell, _countof(cell), _TRUNCATE, L"%02X", bytes[i]);
        if (i) text.push_back(L' '); text.append(cell);
    }
    return text;
}

bool TimedIo(HANDLE handle, bool write, void* data, DWORD bytes, DWORD timeout, DWORD* transferred)
{
    if (transferred) *transferred = 0;
    HidIoOperation operation(handle); DWORD error = 0;
    const auto start = write ? operation.StartWrite(data, bytes, &error) : operation.StartRead(data, bytes, &error);
    if (start == HidIoOperation::StartResult::Failed) { SetLastError(error); return false; }
    if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = operation.Wait(timeout);
        if (wait == WAIT_OBJECT_0)
        {
            const bool ok = operation.Finish(transferred, &error, false); if (!ok) SetLastError(error); return ok;
        }
        const DWORD waitError = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
        operation.CancelAndDrain(transferred, &error); SetLastError(waitError ? waitError : ERROR_GEN_FAILURE); return false;
    }
    const bool ok = operation.Finish(transferred, &error, false); if (!ok) SetLastError(error); return ok;
}

std::vector<Candidate> Enumerate(bool verbose)
{
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    std::vector<Candidate> result;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{}; interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &interfaceData))
        { if (GetLastError() == ERROR_NO_MORE_ITEMS) break; continue; }
        DWORD required = 0; SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0, &required, nullptr);
        if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data()); detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail, required, nullptr, nullptr)) continue;
        Handle metadata(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;
        HIDD_ATTRIBUTES attributes{}; attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(metadata.value, &attributes) || attributes.VendorID != kVendorId || attributes.ProductID != kProductId) continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr; if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        HIDP_CAPS caps{}; const NTSTATUS status = HidP_GetCaps(preparsed, &caps); HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) continue;
        const bool exact = caps.UsagePage == kUsagePage && caps.Usage == kUsage && caps.InputReportByteLength == kReportBytes && caps.OutputReportByteLength == kReportBytes;
        if (verbose) DebugLog_Write(L"[mchose.ace68.enumeration] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u exact_vendor_hub=%d", static_cast<unsigned long long>(HashPath(detail->DevicePath)), attributes.VendorID, attributes.ProductID, attributes.VersionNumber, caps.UsagePage, caps.Usage, caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, exact ? 1 : 0);
        if (exact) result.push_back({detail->DevicePath, attributes, caps});
    }
    SetupDiDestroyDeviceInfoList(set); return result;
}

class Session
{
public:
    explicit Session(const Candidate& candidate, bool writable = true) : candidate_(candidate), writable_(writable) {}
    ~Session() { ReleaseActive(); }
    bool Open()
    {
        const DWORD access = GENERIC_READ | (writable_ ? GENERIC_WRITE : 0);
        handle_ = Handle(CreateFileW(candidate_.path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false; HidD_SetNumInputBuffers(handle_.value, 256);
        std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = handle_.value; active_ = true; return true;
    }
    bool Send(const Report& report, Transport transport)
    {
        DebugLog_WriteBuffered(L"[mchose.ace68.tx] transport=%ls bytes=64 data=%ls", transport == Transport::InterruptWrite ? L"WriteFile" : L"HidD_SetOutputReport", Hex(report.data(), report.size()).c_str());
        if (transport == Transport::OutputReport) return HidD_SetOutputReport(handle_.value, const_cast<std::uint8_t*>(report.data()), static_cast<ULONG>(report.size())) != FALSE;
        Report copy = report; DWORD sent = 0; return TimedIo(handle_.value, true, copy.data(), static_cast<DWORD>(copy.size()), kReadSliceMs, &sent) && sent == copy.size();
    }
    bool Read(Report* report, DWORD timeout)
    {
        if (!report) return false; report->fill(0); DWORD received = 0;
        if (!TimedIo(handle_.value, false, report->data(), static_cast<DWORD>(report->size()), timeout, &received)) return false;
        if (received != report->size()) { SetLastError(ERROR_BAD_LENGTH); return false; } return true;
    }
private:
    void ReleaseActive()
    {
        if (!active_) return; std::lock_guard<std::mutex> lock(g_handleMutex);
        if (g_activeHandle == handle_.value) g_activeHandle = INVALID_HANDLE_VALUE; active_ = false;
    }
    Candidate candidate_; Handle handle_{}; bool writable_ = true; bool active_ = false;
};

Report BuildReadRequest(const ReadRequest& request, const Strategy& strategy)
{
    // M HUB's _simpleGetCommand uses no payload. Byte 4 is requested response
    // length, 5..6 are little-endian offset, and byte 3 is their byte sum.
    Report report{}; report[0] = strategy.header; report[1] = request.opcode;
    report[4] = request.responseBytes; report[5] = static_cast<std::uint8_t>(request.offset); report[6] = static_cast<std::uint8_t>(request.offset >> 8);
    report[3] = static_cast<std::uint8_t>(report[4] + report[5] + report[6] + report[7]);
    if (strategy.header == 0x55) { report[2] = strategy.xorKey; if (strategy.xorKey) for (std::size_t index = 3; index <= 7; ++index) report[index] ^= strategy.xorKey; }
    return report;
}

void ProcessReport(const Report& report, SessionStats* stats)
{
    if (!stats) return; ++stats->reports; g_reports.fetch_add(1, std::memory_order_relaxed); g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
    DebugLog_WriteBuffered(L"[mchose.ace68.rx] bytes=64 data=%ls", Hex(report.data(), report.size()).c_str());
    if (report[0] == 0xA0)
    {
        ++stats->a0; g_a0.fetch_add(1, std::memory_order_relaxed);
        const std::array<std::uint8_t, 3> key{report[1], report[2], report[3]}; const std::uint16_t raw = static_cast<std::uint16_t>((report[4] << 8) | report[5]);
        auto& value = stats->descriptorStats[key]; ++value.packets; value.minimum = std::min(value.minimum, raw); value.maximum = std::max(value.maximum, raw);
        DebugLog_WriteBuffered(L"[mchose.ace68.a0] descriptor=%02X %02X %02X raw_be16=%u mode_bytes=%02X %02X packet=%llu", key[0], key[1], key[2], raw, report[6], report[7], static_cast<unsigned long long>(stats->a0));
    }
    else if (report[0] == 0xAA) { ++stats->aa; DebugLog_WriteBuffered(L"[mchose.ace68.response] kind=AA opcode=%02X", report[1]); }
    else if (report[0] == 0xAB) { ++stats->ab; DebugLog_WriteBuffered(L"[mchose.ace68.response] kind=AB opcode=%02X", report[1]); }
    else { ++stats->other; DebugLog_WriteBuffered(L"[mchose.ace68.response] kind=OTHER first=%02X", report[0]); }
}

bool Listen(Session& session, SessionStats* stats, DWORD durationMs)
{
    const auto deadline = GetTickCount64() + durationMs;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        Report report{}; const auto remaining = static_cast<DWORD>(deadline - GetTickCount64());
        if (session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining))) ProcessReport(report, stats);
        else if (GetLastError() != WAIT_TIMEOUT && GetLastError() != ERROR_OPERATION_ABORTED)
        { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mchose.ace68.rx_error] win32=%lu", GetLastError()); return false; }
    }
    return true;
}

AckResult SendReadAndAwait(Session& session, SessionStats* stats, const Strategy& strategy, const ReadRequest& request)
{
    const auto packet = BuildReadRequest(request, strategy);
    DebugLog_Write(L"[mchose.ace68.read] begin name=%ls strategy=%ls opcode=%02X offset=%u response_bytes=%u header=%02X xor=%02X", request.name, strategy.name, request.opcode, request.offset, request.responseBytes, strategy.header, strategy.xorKey);
    if (!session.Send(packet, strategy.transport)) { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mchose.ace68.read] send_failed name=%ls strategy=%ls opcode=%02X win32=%lu", request.name, strategy.name, request.opcode, GetLastError()); return AckResult::SendFailed; }
    ++stats->writes; const auto deadline = GetTickCount64() + kCommandAckMs;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        Report report{}; const auto remaining = static_cast<DWORD>(deadline - GetTickCount64());
        if (!session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining)))
        {
            if (GetLastError() == WAIT_TIMEOUT) continue;
            ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mchose.ace68.read] receive_failed name=%ls strategy=%ls opcode=%02X win32=%lu", request.name, strategy.name, request.opcode, GetLastError()); return AckResult::Timeout;
        }
        ProcessReport(report, stats);
        if (report[0] == 0xAB) { DebugLog_Write(L"[mchose.ace68.read] REJECTED name=%ls strategy=%ls opcode=%02X; stopping all active probes", request.name, strategy.name, request.opcode); return AckResult::Rejected; }
        if (report[0] == 0xAA)
        {
            if (report[1] == request.opcode) { DebugLog_Write(L"[mchose.ace68.read] ACK name=%ls strategy=%ls opcode=%02X", request.name, strategy.name, request.opcode); return AckResult::Accepted; }
            DebugLog_Write(L"[mchose.ace68.read] unexpected_AA name=%ls strategy=%ls expected=%02X actual=%02X", request.name, strategy.name, request.opcode, report[1]); return AckResult::UnexpectedAck;
        }
    }
    DebugLog_Write(L"[mchose.ace68.read] timeout name=%ls strategy=%ls opcode=%02X", request.name, strategy.name, request.opcode); return AckResult::Timeout;
}

void LogSummary(const SessionStats& stats)
{
    DebugLog_Write(L"[mchose.ace68.summary] reports=%llu a0=%llu aa=%llu ab=%llu other=%llu writes=%llu failures=%llu unique_descriptors=%llu", static_cast<unsigned long long>(stats.reports), static_cast<unsigned long long>(stats.a0), static_cast<unsigned long long>(stats.aa), static_cast<unsigned long long>(stats.ab), static_cast<unsigned long long>(stats.other), static_cast<unsigned long long>(stats.writes), static_cast<unsigned long long>(stats.failures), static_cast<unsigned long long>(stats.descriptorStats.size()));
    for (const auto& [descriptor, value] : stats.descriptorStats) DebugLog_Write(L"[mchose.ace68.summary.a0] descriptor=%02X %02X %02X packets=%llu min=%u max=%u", descriptor[0], descriptor[1], descriptor[2], static_cast<unsigned long long>(value.packets), value.minimum, value.maximum);
}

bool RunCandidate(const Candidate& candidate)
{
    SessionStats stats{};
    DebugLog_Write(L"[mchose.ace68.session] begin vid=41E4 pid=2114 version=%04X usage=0001:0000 in=64 out=64 plan=passive_then_official_read_only_queries_then_observe", candidate.attributes.VersionNumber);
    DebugLog_Write(L"[mchose.ace68.instructions] release_all_keys=1; Raw Input key events are correlated with A0 descriptors during observation_ms=%lu", kObservationMs);
    {
        Session readOnly(candidate, false);
        if (readOnly.Open())
        {
            DebugLog_Write(L"[mchose.ace68.phase] name=read_only_passive duration_ms=%lu", kPassiveListenMs);
            (void)Listen(readOnly, &stats, kPassiveListenMs);
        }
        else DebugLog_Write(L"[mchose.ace68.phase] name=read_only_passive open_failed win32=%lu", GetLastError());
    }
    Session session(candidate, true);
    if (!session.Open()) { DebugLog_Write(L"[mchose.ace68.session] read_write_open_failed path_hash=%016llX win32=%lu", static_cast<unsigned long long>(HashPath(candidate.path)), GetLastError()); LogSummary(stats); return false; }
    g_connected.store(true, std::memory_order_release); g_inputBytes.store(candidate.caps.InputReportByteLength); g_outputBytes.store(candidate.caps.OutputReportByteLength);
    DebugLog_Write(L"[mchose.ace68.phase] name=read_write_passive duration_ms=%lu", kPassiveListenMs);
    if (!Listen(session, &stats, kPassiveListenMs)) { LogSummary(stats); g_connected.store(false); return false; }
    constexpr std::array<Strategy, 2> strategies = {{{L"55-official-WriteFile",0x55,0x00,Transport::InterruptWrite},{L"55-official-OutputReport",0x55,0x00,Transport::OutputReport}}};
    std::vector<ReadRequest> requests{{L"get_info",0x03,0,56},{L"get_base",0x04,0,56},{L"get_function_config_0",0x05,0,56},{L"get_function_config_0_tail",0x05,56,8}};
    for (std::uint16_t offset = 0; offset < 216; offset += 56) requests.push_back({L"get_key_matrix",0x08,offset,static_cast<std::uint8_t>(std::min<std::uint16_t>(56, 216 - offset))});
    for (std::uint16_t offset = 0; offset < 1024; offset += 56) requests.push_back({L"get_key_trigger_profile_0",0xA0,offset,static_cast<std::uint8_t>(std::min<std::uint16_t>(56, 1024 - offset))});
    bool rejected = false, unexpectedAck = false;
    for (const auto& request : requests)
    {
        bool accepted = false;
        for (const auto& strategy : strategies)
        {
            if (g_stop.load(std::memory_order_acquire)) break;
            const auto result = SendReadAndAwait(session, &stats, strategy, request);
            if (result == AckResult::Accepted) { accepted = true; break; }
            if (result == AckResult::Rejected || result == AckResult::UnexpectedAck) { rejected = result == AckResult::Rejected; unexpectedAck = result == AckResult::UnexpectedAck; break; }
        }
        if (rejected || unexpectedAck || g_stop.load(std::memory_order_acquire)) break;
        if (!accepted) DebugLog_Write(L"[mchose.ace68.read] no_ack_after_safe_transports name=%ls opcode=%02X", request.name, request.opcode);
    }
    DebugLog_Write(L"[mchose.ace68.plan] official_read_complete requests=%llu rejected=%d unexpected_ack=%d; entering long raw capture", static_cast<unsigned long long>(requests.size()), rejected ? 1 : 0, unexpectedAck ? 1 : 0);
    // AB means no more host writes, not no more evidence. Continue capturing
    // the device's unmodified vendor traffic for the full observation window.
    (void)Listen(session, &stats, kObservationMs);
    LogSummary(stats); g_connected.store(false, std::memory_order_release); return !g_stop.load(std::memory_order_acquire);
}

unsigned __stdcall Worker(void*)
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        bool ran = false;
        for (const auto& candidate : Enumerate(false))
        {
            if (NativeAnalogRouting_IsClaimed(candidate.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(), NativeAnalogProtocol::MchoseAce68Diagnostic)) continue;
            NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.path.c_str(), NativeAnalogProtocol::MchoseAce68Diagnostic); g_present.store(true, std::memory_order_release);
            ran = RunCandidate(candidate) || ran; if (g_stop.load(std::memory_order_acquire)) break;
        }
        if (!ran && !g_stop.load(std::memory_order_acquire)) { g_present.store(false, std::memory_order_release); if (g_wake) { WaitForSingleObject(g_wake, kReconnectMs); ResetEvent(g_wake); } }
    }
    g_connected.store(false, std::memory_order_release); g_running.store(false, std::memory_order_release); return 0;
}

bool Prepare()
{
    bool any = false;
    for (const auto& candidate : Enumerate(true))
    {
        const bool claimed = NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.path.c_str(), NativeAnalogProtocol::MchoseAce68Diagnostic);
        DebugLog_Write(L"[mchose.ace68.prepare] path_hash=%016llX exact_shape=1 claimed=%d", static_cast<unsigned long long>(HashPath(candidate.path)), claimed ? 1 : 0);
        any = claimed || NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(), NativeAnalogProtocol::MchoseAce68Diagnostic) || any;
    }
    g_prepared.store(true, std::memory_order_release); g_present.store(any, std::memory_order_release); return any;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_prepared.load(std::memory_order_acquire)) (void)Prepare(); if (g_thread) return g_running.load(std::memory_order_acquire);
    g_stop.store(false, std::memory_order_release); g_running.store(true, std::memory_order_release); g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake) { g_running.store(false, std::memory_order_release); return false; }
    unsigned threadId = 0; g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &threadId));
    if (!g_thread) { CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); return false; }
    DebugLog_Write(L"[mchose.ace68.start] diagnostic_worker=%u", threadId); return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release); if (g_wake) SetEvent(g_wake);
    { std::lock_guard<std::mutex> active(g_handleMutex); if (g_activeHandle != INVALID_HANDLE_VALUE) CancelIoEx(g_activeHandle, nullptr); }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0) return halljoy::lifecycle::ObserveWorkerJoin(generation, wait == WAIT_TIMEOUT ? halljoy::lifecycle::JoinWaitStatus::TimedOut : halljoy::lifecycle::JoinWaitStatus::Failed, wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread); g_thread = nullptr; if (g_wake) CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); g_connected.store(false, std::memory_order_release); return NativeAnalogBackendStopJoined(generation);
}

void Notify() { if (g_wake) SetEvent(g_wake); }
bool Present() { return g_present.load(std::memory_order_acquire); }
bool Connected() { return g_connected.load(std::memory_order_acquire); }
bool Owns(std::uint16_t) { return false; }
std::uint16_t Get(std::uint16_t) { return 0; }
void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return; *out = {}; out->present = Present(); out->connected = Connected(); out->vendorId = kVendorId; out->productId = kProductId; out->usagePage = kUsagePage; out->usage = kUsage; out->inputReportBytes = g_inputBytes.load(); out->outputReportBytes = g_outputBytes.load(); out->successfulUpdates = g_reports.load(); out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64(); out->lastUpdateAgeMs = last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last, 0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE, L"MCHOSE Ace 68 diagnostic: raw=%llu A0=%llu, no gameplay ownership", static_cast<unsigned long long>(g_reports.load()), static_cast<unsigned long long>(g_a0.load()));
}
}

#if defined(HALLJOY_MCHOSE_ACE68_DIAGNOSTIC)
void MchoseAce68Diagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice, std::uint16_t hidUsage,
    std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey)
{
    std::lock_guard<std::mutex> lock(g_rawKeyboardMutex);
    if (!IsMchoseRawKeyboard(rawDevice)) return;
    const bool isDown = (flags & RI_KEY_BREAK) == 0;
    DebugLog_WriteBuffered(L"[mchose.ace68.raw_key] device=%p hid=%02X down=%d make=%02X flags=%04X vkey=%02X",
        reinterpret_cast<void*>(rawDevice), hidUsage, isDown ? 1 : 0,
        makeCode, flags, virtualKey);
}
#endif

const NativeAnalogBackendDescriptor& MchoseAce68Diagnostic_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), "mchose-ace68-diagnostic", L"MCHOSE Ace 68 diagnostic (no input ownership)", NativeAnalogProtocol::MchoseAce68Diagnostic, NativeAnalogStartPhase::BeforeUap, NativeAnalogBackendFlag_StreamTransport | NativeAnalogBackendFlag_ReversibleControlProbe, &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return descriptor;
}
