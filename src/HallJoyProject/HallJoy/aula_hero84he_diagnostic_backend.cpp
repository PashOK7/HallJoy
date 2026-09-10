#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "aula_hero84he_diagnostic_backend.h"
#include "aula_hero84he_diagnostic_protocol.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <process.h>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
using namespace aula_hero84he_diagnostic;
using Clock = std::chrono::steady_clock;

constexpr DWORD kReadSliceMs = 50;
constexpr DWORD kCommandTimeoutMs = 200;
constexpr DWORD kPassiveListenMs = 500;
constexpr DWORD kStopTimeoutMs = 3000;
constexpr std::array<std::uint8_t, 6> kExpectedUuid{{0x11, 0, 0, 0, 0, 0x05}};
// Factory positions only.  The immediately preceding 83 read is evidence, not
// permission to translate them into input or rewrite the map.
constexpr std::array<std::uint16_t, 4> kMovementPositions{{30, 43, 44, 45}};

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    Handle() = default;
    explicit Handle(HANDLE next) : value(next) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other)
        {
            if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = other.value; other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const { return value != INVALID_HANDLE_VALUE; }
};

struct Candidate
{
    std::wstring path;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
    bool reportId9 = false;
};

struct Stage
{
    const wchar_t* name;
    std::uint32_t hertz;
    std::uint32_t milliseconds;
};

struct Range { std::uint16_t minimum = 0xffff; std::uint16_t maximum = 0; };
struct Stats
{
    std::uint64_t passiveReports = 0, requests = 0, completions = 0;
    std::uint64_t failures = 0, scheduleMisses = 0;
    std::uint64_t rttTotalUs = 0, rttMinimumUs = UINT64_MAX, rttMaximumUs = 0;
    std::array<Range, kMaxPositions> current{}, minima{};
};

std::atomic<bool> g_prepared{false}, g_present{false}, g_connected{false}, g_running{false}, g_stop{false};
std::atomic<bool> g_rawInputReady{false};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0};
std::atomic<std::uint64_t> g_successful{0}, g_failures{0}, g_lastMs{0};
std::mutex g_serviceMutex, g_handleMutex, g_rawMutex;
HANDLE g_thread = nullptr, g_wake = nullptr, g_activeHandle = INVALID_HANDLE_VALUE;
std::unordered_map<std::uintptr_t, bool> g_rawTargets;

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const wchar_t ch : value) { hash ^= static_cast<std::uint16_t>(towlower(ch)); hash *= 1099511628211ull; }
    return hash;
}

std::wstring Hex(const std::uint8_t* bytes, std::size_t count)
{
    std::wstring text; text.reserve(count * 3); wchar_t cell[4]{};
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i) text.push_back(L' ');
        _snwprintf_s(cell, _countof(cell), _TRUNCATE, L"%02X", bytes[i]); text.append(cell);
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

bool HasReportId(PHIDP_PREPARSED_DATA preparsed, const HIDP_CAPS& caps, std::uint8_t wanted)
{
    const auto hasButtons = [&](HIDP_REPORT_TYPE type, USHORT count) {
        if (!count) return false;
        std::vector<HIDP_BUTTON_CAPS> values(count);
        if (HidP_GetButtonCaps(type, values.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) return false;
        return std::any_of(values.begin(), values.begin() + count, [wanted](const HIDP_BUTTON_CAPS& value) { return value.ReportID == wanted; });
    };
    const auto hasValues = [&](HIDP_REPORT_TYPE type, USHORT count) {
        if (!count) return false;
        std::vector<HIDP_VALUE_CAPS> values(count);
        if (HidP_GetValueCaps(type, values.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) return false;
        return std::any_of(values.begin(), values.begin() + count, [wanted](const HIDP_VALUE_CAPS& value) { return value.ReportID == wanted; });
    };
    return (hasButtons(HidP_Input, caps.NumberInputButtonCaps) || hasValues(HidP_Input, caps.NumberInputValueCaps)) &&
        (hasButtons(HidP_Output, caps.NumberOutputButtonCaps) || hasValues(HidP_Output, caps.NumberOutputValueCaps));
}

std::vector<Candidate> Enumerate(bool verbose)
{
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    std::vector<Candidate> candidates;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &iface))
        { if (GetLastError() == ERROR_NO_MORE_ITEMS) break; continue; }
        DWORD bytes = 0; SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &bytes, nullptr);
        if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data()); detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, bytes, nullptr, nullptr)) continue;
        Handle metadata(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;
        HIDD_ATTRIBUTES attributes{}; attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(metadata.value, &attributes) || attributes.VendorID != kVendorId || attributes.ProductID != kProductId) continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        HIDP_CAPS caps{}; const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        const bool reportId9 = status == HIDP_STATUS_SUCCESS && HasReportId(preparsed, caps, kReportId);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) continue;
        const bool exact = caps.UsagePage == kUsagePage && caps.Usage == kUsage &&
            caps.InputReportByteLength == kReportBytes && caps.OutputReportByteLength == kReportBytes && reportId9;
        if (verbose) DebugLog_Write(L"[aula.hero84.enumerate] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u report09_bidir=%d exact=%d", static_cast<unsigned long long>(HashPath(detail->DevicePath)), attributes.VendorID, attributes.ProductID, attributes.VersionNumber, caps.UsagePage, caps.Usage, caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, reportId9 ? 1 : 0, exact ? 1 : 0);
        if (exact) candidates.push_back({detail->DevicePath, attributes, caps, reportId9});
    }
    SetupDiDestroyDeviceInfoList(set);
    return candidates;
}

class Session
{
public:
    explicit Session(const Candidate& candidate, bool writable) : candidate_(candidate), writable_(writable) {}
    ~Session() { ReleaseActive(); }
    bool Open()
    {
        const DWORD access = GENERIC_READ | (writable_ ? GENERIC_WRITE : 0);
        handle_ = Handle(CreateFileW(candidate_.path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));
        if (!handle_) return false;
        HidD_SetNumInputBuffers(handle_.value, 256);
        if (writable_) { std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = handle_.value; active_ = true; }
        return true;
    }
    bool Send(const Report& report)
    {
        DebugLog_WriteBuffered(L"[aula.hero84.tx] provenance=%ls bytes=64 data=%ls", report[1] == 0x94 ? L"firmware-derived-read-only-candidate" : L"official-client-read", Hex(report.data(), report.size()).c_str());
        Report copy = report; DWORD sent = 0;
        return TimedIo(handle_.value, true, copy.data(), static_cast<DWORD>(copy.size()), kReadSliceMs, &sent) && sent == copy.size();
    }
    bool Read(Report* out, DWORD timeout)
    {
        if (!out) return false;
        out->fill(0); DWORD received = 0;
        if (!TimedIo(handle_.value, false, out->data(), static_cast<DWORD>(out->size()), timeout, &received)) return false;
        if (received != out->size()) { SetLastError(ERROR_BAD_LENGTH); return false; }
        DebugLog_WriteBuffered(L"[aula.hero84.rx] bytes=64 data=%ls", Hex(out->data(), out->size()).c_str());
        g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
        return true;
    }
private:
    void ReleaseActive()
    {
        if (!active_) return;
        std::lock_guard<std::mutex> lock(g_handleMutex);
        if (g_activeHandle == handle_.value) g_activeHandle = INVALID_HANDLE_VALUE;
        active_ = false;
    }
    Candidate candidate_; bool writable_ = false, active_ = false; Handle handle_{};
};

bool PassiveListen(Session& session, Stats* stats)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(kPassiveListenMs);
    while (!g_stop.load(std::memory_order_acquire) && Clock::now() < deadline)
    {
        Report report{};
        if (session.Read(&report, kReadSliceMs)) { ++stats->passiveReports; continue; }
        if (GetLastError() != WAIT_TIMEOUT && GetLastError() != ERROR_OPERATION_ABORTED)
        { DebugLog_Write(L"[aula.hero84.passive] read_error=%lu", GetLastError()); return false; }
    }
    return !g_stop.load(std::memory_order_acquire);
}

bool Exchange(Session& session, const Report& request, Report* response, std::uint64_t* rttUs)
{
    if (!response || !rttUs) return false;
    const auto begin = Clock::now();
    if (!session.Send(request)) { DebugLog_Write(L"[aula.hero84.exchange] send_failed command=%02X sub=%02X win32=%lu", request[1], request[2], GetLastError()); return false; }
    Report report{};
    if (!session.Read(&report, kCommandTimeoutMs)) { DebugLog_Write(L"[aula.hero84.exchange] timeout_or_read_failure command=%02X sub=%02X win32=%lu", request[1], request[2], GetLastError()); return false; }
    *rttUs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count());
    *response = report;
    return true;
}

void AddSample(Stats* stats, std::size_t index, const DirectSample& sample, std::uint64_t rttUs)
{
    auto& current = stats->current[index]; auto& minimum = stats->minima[index];
    current.minimum = std::min(current.minimum, sample.current); current.maximum = std::max(current.maximum, sample.current);
    minimum.minimum = std::min(minimum.minimum, sample.minimum); minimum.maximum = std::max(minimum.maximum, sample.minimum);
    ++stats->completions; ++g_successful;
    stats->rttTotalUs += rttUs; stats->rttMinimumUs = std::min(stats->rttMinimumUs, rttUs); stats->rttMaximumUs = std::max(stats->rttMaximumUs, rttUs);
}

bool RunStage(Session& session, const Stage& stage, Stats* stats)
{
    Report request{};
    if (!BuildDirectRead(kMovementPositions.data(), kMovementPositions.size(), &request)) return false;
    const auto period = std::chrono::microseconds(1000000u / stage.hertz);
    const auto deadline = Clock::now() + std::chrono::milliseconds(stage.milliseconds);
    auto next = Clock::now();
    const std::uint64_t requestsBefore = stats->requests, completionsBefore = stats->completions;
    DebugLog_Write(L"[aula.hero84.stage] begin name=%ls requested_hz=%u duration_ms=%u keys=W,A,S,D outstanding_limit=1", stage.name, stage.hertz, stage.milliseconds);
    while (!g_stop.load(std::memory_order_acquire) && Clock::now() < deadline)
    {
        while (!g_stop.load(std::memory_order_acquire) && Clock::now() < next) Sleep(0);
        const auto started = Clock::now();
        if (started > next + period) ++stats->scheduleMisses;
        Report response{}; std::uint64_t rttUs = 0; ++stats->requests;
        if (!Exchange(session, request, &response, &rttUs)) { ++stats->failures; ++g_failures; return false; }
        std::array<DirectSample, kMaxPositions> samples{};
        if (!ParseDirectResponse(response, kMovementPositions.data(), kMovementPositions.size(), &samples))
        {
            ++stats->failures; ++g_failures;
            DebugLog_Write(L"[aula.hero84.stage] semantic_reject name=%ls expected=94/02/checksum/correlated_records count=4; stopping", stage.name);
            return false;
        }
        for (std::size_t index = 0; index < kMovementPositions.size(); ++index)
        {
            AddSample(stats, index, samples[index], rttUs);
            DebugLog_WriteBuffered(L"[aula.hero84.sample] stage=%ls pos=%04X current=%u minimum=%u scanner_lock=%d rtt_us=%llu", stage.name, samples[index].position, samples[index].current, samples[index].minimum, samples[index].scannerLock ? 1 : 0, static_cast<unsigned long long>(rttUs));
        }
        next += period;
        if (Clock::now() > next + period) next = Clock::now();
    }
    DebugLog_Write(L"[aula.hero84.stage] complete name=%ls requests=%llu replies=%llu schedule_misses=%llu", stage.name, static_cast<unsigned long long>(stats->requests - requestsBefore), static_cast<unsigned long long>(stats->completions - completionsBefore), static_cast<unsigned long long>(stats->scheduleMisses));
    return !g_stop.load(std::memory_order_acquire);
}

void LogSummary(const Stats& stats)
{
    const std::uint64_t mean = stats.completions ? stats.rttTotalUs / stats.completions : 0;
    DebugLog_Write(L"[aula.hero84.summary] passive_reports=%llu requests=%llu completions=%llu failures=%llu schedule_misses=%llu rtt_us_min=%llu mean=%llu max=%llu input_ownership=none", static_cast<unsigned long long>(stats.passiveReports), static_cast<unsigned long long>(stats.requests), static_cast<unsigned long long>(stats.completions), static_cast<unsigned long long>(stats.failures), static_cast<unsigned long long>(stats.scheduleMisses), static_cast<unsigned long long>(stats.rttMinimumUs == UINT64_MAX ? 0 : stats.rttMinimumUs), static_cast<unsigned long long>(mean), static_cast<unsigned long long>(stats.rttMaximumUs));
    for (std::size_t i = 0; i < kMovementPositions.size(); ++i)
        DebugLog_Write(L"[aula.hero84.summary.key] pos=%04X current_min=%u current_max=%u episode_min_min=%u episode_min_max=%u", kMovementPositions[i], stats.current[i].minimum == 0xffff ? 0 : stats.current[i].minimum, stats.current[i].maximum, stats.minima[i].minimum == 0xffff ? 0 : stats.minima[i].minimum, stats.minima[i].maximum);
}

bool RunCandidate(const Candidate& candidate)
{
    Stats stats{};
    DebugLog_Write(L"[aula.hero84.session] begin path_hash=%016llX vid=372E pid=103E usage=FF60:0061 report=09 plan=passive+82/01+83+94/02-staged; forbidden=94/00,94/04,94/05,98,feature,flash,config", static_cast<unsigned long long>(HashPath(candidate.path)));
    { Session passive(candidate, false); if (passive.Open()) { DebugLog_Write(L"[aula.hero84.phase] read_only_passive duration_ms=%lu", kPassiveListenMs); (void)PassiveListen(passive, &stats); } }
    if (g_stop.load(std::memory_order_acquire)) { LogSummary(stats); return false; }
    Session session(candidate, true);
    if (!session.Open()) { DebugLog_Write(L"[aula.hero84.session] read_write_open_failed win32=%lu", GetLastError()); LogSummary(stats); return false; }
    g_connected.store(true, std::memory_order_release); g_inputBytes.store(candidate.caps.InputReportByteLength); g_outputBytes.store(candidate.caps.OutputReportByteLength);
    Report request{}, response{}; std::uint64_t rttUs = 0;
    if (!BuildIdentityRead(&request) || !Exchange(session, request, &response, &rttUs)) { ++stats.failures; ++g_failures; LogSummary(stats); g_connected.store(false); return false; }
    std::array<std::uint8_t, 6> uuid{};
    if (!ParseIdentityResponse(response, &uuid) || uuid != kExpectedUuid)
    {
        ++stats.failures; ++g_failures;
        DebugLog_Write(L"[aula.hero84.admission] reject identity_response_or_uuid uuid=%02X%02X%02X%02X%02X%02X expected=110000000005", uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5]);
        LogSummary(stats); g_connected.store(false); return false;
    }
    if (!NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.path.c_str(), NativeAnalogProtocol::AulaHero84HeDiagnostic))
    {
        DebugLog_Write(L"[aula.hero84.admission] reject routing_conflict"); LogSummary(stats); g_connected.store(false); return false;
    }
    DebugLog_Write(L"[aula.hero84.admission] accepted uuid=110000000005 identity_rtt_us=%llu", static_cast<unsigned long long>(rttUs));
    if (!BuildAssignmentRead(0, kMovementPositions.data(), kMovementPositions.size(), &request) || !Exchange(session, request, &response, &rttUs)) { ++stats.failures; ++g_failures; LogSummary(stats); g_connected.store(false); return false; }
    std::array<Assignment, kMaxPositions> assignments{};
    if (!ParseAssignmentResponse(response, 0, kMovementPositions.data(), kMovementPositions.size(), &assignments))
    {
        ++stats.failures; ++g_failures; DebugLog_Write(L"[aula.hero84.mapping] reject 83 response; stopping before 94/02"); LogSummary(stats); g_connected.store(false); return false;
    }
    for (const auto& assignment : assignments) DebugLog_Write(L"[aula.hero84.mapping] pos=%04X assignment_be32=%08X", assignment.position, assignment.value);
    constexpr std::array<Stage, 5> stages{{{L"25Hz",25,1000},{L"125Hz",125,2000},{L"250Hz",250,2000},{L"500Hz",500,2000},{L"1000Hz",1000,3000}}};
    bool succeeded = true;
    for (const auto& stage : stages) if (!RunStage(session, stage, &stats)) { succeeded = false; break; }
    LogSummary(stats); g_connected.store(false, std::memory_order_release);
    return succeeded && !g_stop.load(std::memory_order_acquire);
}

unsigned __stdcall Worker(void*)
{
    if (!g_rawInputReady.load(std::memory_order_acquire))
    {
        DebugLog_Write(L"[aula.hero84.worker] waiting_for_raw_input_registration timeout_ms=10000");
        const auto deadline = GetTickCount64() + 10000;
        while (!g_rawInputReady.load(std::memory_order_acquire) &&
            !g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
        {
            const auto remaining = static_cast<DWORD>(deadline - GetTickCount64());
            if (g_wake) WaitForSingleObject(g_wake, remaining);
            if (g_wake) ResetEvent(g_wake);
        }
    }
    if (!g_rawInputReady.load(std::memory_order_acquire) || g_stop.load(std::memory_order_acquire))
    {
        DebugLog_Write(L"[aula.hero84.worker] raw_input_not_ready; no active diagnostic traffic sent");
        g_running.store(false, std::memory_order_release);
        return 0;
    }
    bool ran = false;
    for (const auto& candidate : Enumerate(false))
    {
        if (!NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(), NativeAnalogProtocol::AulaHero84HeDiagnostic)) continue;
        g_present.store(true, std::memory_order_release);
        ran = RunCandidate(candidate) || ran;
        break; // A diagnostic run is single-shot; it never repeats commands automatically.
    }
    if (!ran) g_present.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release); g_running.store(false, std::memory_order_release);
    DebugLog_Write(L"[aula.hero84.worker] exit diagnostic_ran=%d stop_requested=%d", ran ? 1 : 0, g_stop.load() ? 1 : 0);
    return 0;
}

bool IsTargetRawKeyboard(std::uintptr_t rawDevice)
{
    const auto found = g_rawTargets.find(rawDevice); if (found != g_rawTargets.end()) return found->second;
    UINT chars = 0; const HANDLE device = reinterpret_cast<HANDLE>(rawDevice); bool matched = false;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars) != static_cast<UINT>(-1) && chars)
    {
        std::wstring path(chars + 1, L'\0');
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, path.data(), &chars) != static_cast<UINT>(-1))
        { std::transform(path.begin(), path.end(), path.begin(), towupper); matched = path.find(L"VID_372E&PID_103E") != std::wstring::npos; }
    }
    g_rawTargets.emplace(rawDevice, matched); return matched;
}

bool Prepare()
{
    bool any = false;
    for (const auto& candidate : Enumerate(true))
    {
        if (NativeAnalogRouting_IsClaimed(candidate.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(), NativeAnalogProtocol::AulaHero84HeDiagnostic)) continue;
        Session session(candidate, true);
        Report request{}, response{}; std::uint64_t rttUs = 0; std::array<std::uint8_t, 6> uuid{};
        const bool proven = session.Open() && BuildIdentityRead(&request) &&
            Exchange(session, request, &response, &rttUs) &&
            ParseIdentityResponse(response, &uuid) && uuid == kExpectedUuid;
        if (!proven)
        {
            DebugLog_Write(L"[aula.hero84.prepare] reject path_hash=%016llX reason=shape_or_82_identity", static_cast<unsigned long long>(HashPath(candidate.path)));
            continue;
        }
        const bool claimed = NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.path.c_str(), NativeAnalogProtocol::AulaHero84HeDiagnostic);
        DebugLog_Write(L"[aula.hero84.prepare] identity_proven path_hash=%016llX uuid=110000000005 rtt_us=%llu claimed=%d", static_cast<unsigned long long>(HashPath(candidate.path)), static_cast<unsigned long long>(rttUs), claimed ? 1 : 0);
        any = claimed || NativeAnalogRouting_IsClaimedBy(candidate.path.c_str(), NativeAnalogProtocol::AulaHero84HeDiagnostic) || any;
    }
    g_prepared.store(true, std::memory_order_release); g_present.store(any, std::memory_order_release); return any;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_prepared.load(std::memory_order_acquire)) (void)Prepare();
    if (g_thread) return g_running.load(std::memory_order_acquire);
    if (!g_present.load(std::memory_order_acquire)) return false;
    g_stop.store(false, std::memory_order_release); g_rawInputReady.store(false, std::memory_order_release); g_running.store(true, std::memory_order_release);
    g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake) { g_running.store(false, std::memory_order_release); return false; }
    unsigned threadId = 0; g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &threadId));
    if (!g_thread) { CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); return false; }
    DebugLog_Write(L"[aula.hero84.start] worker=%u diagnostic_only=1 active_traffic_waits_for_raw_input=1", threadId); return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex);
    if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release);
    if (g_wake) SetEvent(g_wake);
    { std::lock_guard<std::mutex> active(g_handleMutex); if (g_activeHandle != INVALID_HANDLE_VALUE) CancelIoEx(g_activeHandle, nullptr); }
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0) return NativeAnalogBackendStopFailed(generation, halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread); g_thread = nullptr; if (g_wake) CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); g_connected.store(false, std::memory_order_release);
    return NativeAnalogBackendStopJoined(generation);
}

void Notify() { if (g_wake) SetEvent(g_wake); }
bool Present() { return g_present.load(std::memory_order_acquire); }
bool Connected() { return g_connected.load(std::memory_order_acquire); }
bool Owns(std::uint16_t) { return false; }
std::uint16_t Get(std::uint16_t) { return 0; }
void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return; *out = {}; out->present = Present(); out->connected = Connected(); out->vendorId = kVendorId; out->productId = kProductId; out->usagePage = kUsagePage; out->usage = kUsage; out->mappedKeys = 4; out->inputReportBytes = g_inputBytes.load(); out->outputReportBytes = g_outputBytes.load(); out->successfulUpdates = g_successful.load(); out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64(); out->lastUpdateAgeMs = last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last, 0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE, L"AULA HERO84 HE diagnostic: read-only evidence only; no input ownership");
}
}

#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)
void AulaHero84HeDiagnostic_RecordRawKeyboardEvent(std::uintptr_t rawDevice,
    std::uint16_t hidUsage, std::uint16_t makeCode, std::uint16_t flags,
    std::uint16_t virtualKey)
{
    std::lock_guard<std::mutex> lock(g_rawMutex);
    if (!IsTargetRawKeyboard(rawDevice)) return;
    DebugLog_WriteBuffered(L"[aula.hero84.raw_key] device=%p hid=%02X down=%d make=%02X flags=%04X vkey=%02X", reinterpret_cast<void*>(rawDevice), hidUsage, (flags & RI_KEY_BREAK) == 0 ? 1 : 0, makeCode, flags, virtualKey);
}

void AulaHero84HeDiagnostic_NotifyRawInputReady(bool registered)
{
    g_rawInputReady.store(registered, std::memory_order_release);
    DebugLog_Write(L"[aula.hero84.raw_input] registered=%d", registered ? 1 : 0);
    if (g_wake) SetEvent(g_wake);
}
#endif

const NativeAnalogBackendDescriptor& AulaHero84HeDiagnostic_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), "aula-hero84he-diagnostic", L"AULA HERO84 HE diagnostic (no input ownership)", NativeAnalogProtocol::AulaHero84HeDiagnostic, NativeAnalogStartPhase::BeforeUap, NativeAnalogBackendFlag_PolledTransport | NativeAnalogBackendFlag_ReadOnlyProbe | NativeAnalogBackendFlag_RequiresRawInput, &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return descriptor;
}
