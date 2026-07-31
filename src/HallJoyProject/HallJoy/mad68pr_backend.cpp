#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "mad68pr_backend.h"
#include "mad68pr_protocol.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "hid_io_operation.h"
#include "realtime_loop.h"
#include "native_analog_routing.h"
#include "worker_exception_barrier.h"
#include "version.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr USAGE kExpectedUsagePage = 0x0001;
constexpr USAGE kExpectedUsage = 0x0000;
constexpr DWORD kReadSliceMs = 25;
constexpr DWORD kCommandAckMs = 650;
constexpr DWORD kValidationMs = 7000;
constexpr DWORD kPassiveListenMs = 1400;
constexpr DWORD kDigitalAnalogDeadlineMs = 1000;
// A8 schedules three complete 72-slot sweeps. One slot is emitted per ~15 ms,
// so the same key can legitimately take about 1080 ms to appear during this phase.
constexpr DWORD kForcedSweepGraceMs = 4500;
constexpr DWORD kForcedSweepDigitalDeadlineMs = 1600;
// In steady event mode the firmware emits at most one changed slot per service pass.
// If another key keeps the global stream active, allow extra time for a higher slot.
constexpr DWORD kGlobalStreamAliveMs = 350;
constexpr DWORD kSchedulerStarvationDeadlineMs = 3000;
constexpr DWORD kDigitalLeadToleranceMs = 250;
constexpr DWORD kReleaseWaitLogMs = 2000;
constexpr DWORD kAllReleasedStableMs = 500;
constexpr DWORD kReconnectWaitMs = 1000;
constexpr DWORD kSummaryMs = 5000;
constexpr DWORD kStreamSalvageAgeMs = 2500;
constexpr DWORD kRecoveryWindowMs = 60000;
constexpr int kMaxRecoveryCyclesPerWindow = 2;
constexpr std::uint64_t kLogMaxBytes = 12ull * 1024ull * 1024ull;
constexpr std::size_t kMaxRawLogPackets = 128;
constexpr std::size_t kMaxUnknownA0LogPackets = 96;
constexpr std::size_t kMaxMalformedWireLogPackets = 32;
// A lost A8 ACK can be distinguished from a failed command by the firmware's
// characteristic forced sweep: many distinct descriptors arrive within the
// ACK window while all keyboard keys are released. This threshold is high
// enough to reject ordinary event-mode noise but low enough to tolerate loss.
constexpr std::uint32_t kA8SemanticEvidenceMinFresh = 12;
constexpr std::uint16_t kSignificantRawLogDelta = 80;
constexpr wchar_t kBuildName[] = HALLJOY_BUILD_ID_W;

enum class UiState : int
{
    Starting,
    NoDevice,
    Passive,
    UnsupportedFirmware,
    WaitingRelease,
    Activating,
    Validating,
    ActiveEmergencyWasd,
    ActiveFull,
    Recovering,
    Exhausted,
    Stopped,
};


enum class PublishMode : int
{
    None = 0,
    EmergencyWasd = 1,
    Full = 2,
};

enum class SendTransport
{
    InterruptCaps,
    InterruptRaw64,
    ControlCaps,
};

struct Strategy
{
    const wchar_t* name;
    SendTransport transport;
    std::uint8_t framing;
    std::uint8_t xorKey;
    bool requireAck;
    DWORD interCommandDelayMs;
};

constexpr std::array<Strategy, 13> kStrategies = {{
    { L"interrupt-caps-normal-strict",       SendTransport::InterruptCaps,  mad68pr::kNormalRequestHeader, 0x00, true,   0 },
    // The exact hardware-confirmed transaction gets one clean reopen/retry before
    // any speculative framing fallback is attempted.
    { L"interrupt-caps-normal-strict-retry", SendTransport::InterruptCaps,  mad68pr::kNormalRequestHeader, 0x00, true,   0 },
    { L"interrupt-caps-normal-xor-strict",   SendTransport::InterruptCaps,  mad68pr::kNormalRequestHeader, 0x5A, true,   0 },
    { L"interrupt-caps-raw5f-strict",       SendTransport::InterruptCaps,  mad68pr::kRawRequestHeader,    0x00, true,   0 },
    { L"interrupt-raw64-normal-strict",     SendTransport::InterruptRaw64, mad68pr::kNormalRequestHeader, 0x00, true,   0 },
    { L"interrupt-raw64-normal-xor-strict", SendTransport::InterruptRaw64, mad68pr::kNormalRequestHeader, 0x5A, true,   0 },
    { L"interrupt-raw64-raw5f-strict",      SendTransport::InterruptRaw64, mad68pr::kRawRequestHeader,    0x00, true,   0 },
    { L"control-caps-normal-strict",        SendTransport::ControlCaps,    mad68pr::kNormalRequestHeader, 0x00, true,   0 },
    { L"control-caps-normal-xor-strict",    SendTransport::ControlCaps,    mad68pr::kNormalRequestHeader, 0x5A, true,   0 },
    { L"control-caps-raw5f-strict",         SendTransport::ControlCaps,    mad68pr::kRawRequestHeader,    0x00, true,   0 },
    { L"interrupt-caps-normal-delayed",     SendTransport::InterruptCaps,  mad68pr::kNormalRequestHeader, 0x00, false, 250 },
    { L"interrupt-caps-raw5f-delayed",      SendTransport::InterruptCaps,  mad68pr::kRawRequestHeader,    0x00, false, 250 },
    { L"interrupt-raw64-normal-delayed",    SendTransport::InterruptRaw64, mad68pr::kNormalRequestHeader, 0x00, false, 250 },
}};

struct ScopedHandle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE h) : value(h) {}
    ~ScopedHandle() { reset(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
    void reset(HANDLE next = INVALID_HANDLE_VALUE)
    {
        if (*this) CloseHandle(value);
        value = next;
    }
};

struct ScopedPreparsed
{
    PHIDP_PREPARSED_DATA value = nullptr;
    ~ScopedPreparsed() { if (value) HidD_FreePreparsedData(value); }
};

struct HidPath
{
    std::wstring path;
    HIDP_CAPS caps{};
    HIDD_ATTRIBUTES attrs{};
    USAGE usagePage = 0;
    USAGE usage = 0;
    std::wstring manufacturer;
    std::wstring product;
    std::wstring serial;
};

struct SessionStats
{
    std::uint64_t reads = 0;
    std::uint64_t readTimeouts = 0;
    std::uint64_t readErrors = 0;
    std::uint64_t writes = 0;
    std::uint64_t writeErrors = 0;
    std::uint64_t controlResponses = 0;
    std::uint64_t checksumErrors = 0;
    std::uint64_t a0Packets = 0;
    std::uint64_t a0GapOver50Ms = 0;
    std::uint64_t maxA0GapMs = 0;
    std::uint64_t keyPackets = 0;
    std::uint64_t unknownA0 = 0;
    std::uint64_t malformed = 0;
    std::uint64_t digitalFailures = 0;
    std::uint64_t semanticMismatches = 0;
    std::uint64_t completeSnapshots = 0;
};

std::atomic<bool> g_running{false};
std::atomic<bool> g_stop{false};
std::atomic<bool> g_rescanRequested{false};
std::atomic<bool> g_devicePresent{false};
std::atomic<bool> g_streamConnected{false};
std::atomic<int> g_publishMode{static_cast<int>(PublishMode::None)};
std::atomic<std::uint16_t> g_recoveryHid{0};
std::atomic<bool> g_recoveryRequested{false};
std::atomic<int> g_uiState{static_cast<int>(UiState::Stopped)};
std::atomic<int> g_strategyIndex{-1};
std::atomic<ULONGLONG> g_lastA0Ms{0};
std::atomic<ULONGLONG> g_forcedSweepGraceUntilMs{0};
std::atomic<std::uint32_t> g_coverage{0};
std::atomic<std::uint16_t> g_firmwareVersion{0};
std::atomic<std::uint16_t> g_productId{0};
std::mutex g_protocolRoutingMutex;
std::vector<std::uint16_t> g_routedNativePids;
std::atomic<bool> g_protocolRoutingPrepared{false};
// A full ordered snapshot proves the hidden transport exists, but does not prove
// the firmware's post-sweep event branch. Full-matrix ownership is granted only
// after a physical key edge receives a new matching A0 after the startup sweep
// window has ended.
std::atomic<bool> g_steadyStateConfirmed{false};
std::atomic<std::uint32_t> g_orderedSweepCycles{0};
std::atomic<std::uint32_t> g_orderedSweepPosition{0};
std::atomic<ULONGLONG> g_activationEpochMs{0};
std::atomic<ULONGLONG> g_lastOrderedSweepMs{0};

std::array<std::atomic<std::uint16_t>, 256> g_raw{};
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint16_t>, 256> g_threshold{};
std::array<std::atomic<std::uint16_t>, 256> g_baseline{};
std::array<std::atomic<std::uint8_t>, 256> g_keyState{};
std::array<std::atomic<ULONGLONG>, 256> g_sampleMs{};
std::array<std::atomic<std::uint32_t>, 256> g_sampleSeq{};

// Event-driven publication metadata. One bit per HID usage is set whenever a
// decoded A0 sample changes the value visible to HallJoy. The realtime thread
// exchanges these chunks after every wake, so bursts coalesce without losing
// which keys need fresh curve evaluation.
std::array<std::atomic<std::uint64_t>, 4> g_changedHidChunks{};
std::array<std::atomic<std::uint64_t>, 256> g_changeSequenceByHid{};
std::array<std::atomic<LONGLONG>, 256> g_a0ReceivedQpcByHid{};
std::array<std::atomic<LONGLONG>, 256> g_snapshotPublishedQpcByHid{};
std::array<std::atomic<std::uint16_t>, 256> g_traceRawByHid{};
std::atomic<std::uint64_t> g_changeSequence{0};
std::atomic<std::uint64_t> g_pendingChangedSamples{0};

std::array<std::atomic<std::uint16_t>, mad68pr::kPhysicalKeyCount> g_descriptorRaw{};
std::array<std::atomic<std::uint32_t>, mad68pr::kPhysicalKeyCount> g_descriptorSeq{};
std::array<std::atomic<bool>, 256> g_physicalDown{};
std::array<std::atomic<std::uint32_t>, 256> g_digitalSeq{};
std::array<std::atomic<ULONGLONG>, 256> g_digitalMs{};
std::array<std::atomic<bool>, 256> g_digitalDown{};
// Snapshot the analogue state in the Raw Input producer thread before publishing
// the digital sequence. This removes the race where the worker observed a packet
// that arrived between the physical edge and delayed correlation processing.
std::array<std::atomic<std::uint32_t>, 256> g_sampleSeqAtDigitalEvent{};
std::array<std::atomic<std::uint16_t>, 256> g_rawAtDigitalEvent{};
std::atomic<std::uint32_t> g_digitalResetSeq{0};
std::atomic<std::uint64_t> g_rawInputEdges{0};

LONGLONG Mad68QpcNow()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

void PublishAnalogueChange(
    std::uint16_t hid,
    std::uint16_t raw,
    LONGLONG receivedQpc,
    LONGLONG publishedQpc)
{
    if (hid == 0 || hid >= 256)
        return;

    g_a0ReceivedQpcByHid[hid].store(receivedQpc, std::memory_order_relaxed);
    g_snapshotPublishedQpcByHid[hid].store(publishedQpc, std::memory_order_relaxed);
    g_traceRawByHid[hid].store(raw, std::memory_order_relaxed);
    const std::uint64_t sequence =
        g_changeSequence.fetch_add(1u, std::memory_order_acq_rel) + 1u;
    g_changeSequenceByHid[hid].store(sequence, std::memory_order_release);
    g_changedHidChunks[hid / 64u].fetch_or(
        std::uint64_t{1} << (hid % 64u), std::memory_order_release);
    g_pendingChangedSamples.fetch_add(1u, std::memory_order_relaxed);

    // Wake after all analogue state and trace metadata has been published. Raw
    // Input remains independent and is used only by UI/diagnostics.
    RealtimeLoop_NotifyInputChangedAt(publishedQpc);
}

void CaptureAnalogSnapshot(std::uint16_t hid, std::uint32_t& seq, std::uint16_t& raw)
{
    // ProcessPayload publishes raw before incrementing sampleSeq. Retry when a
    // packet completes while the snapshot is being taken. If a writer has
    // published the new raw but not the sequence yet, the next sequence change
    // still makes that exact packet post-edge, which is the conservative result.
    for (unsigned attempt = 0; attempt != 4; ++attempt)
    {
        const std::uint32_t before = g_sampleSeq[hid].load(std::memory_order_acquire);
        const std::uint16_t value = g_raw[hid].load(std::memory_order_acquire);
        const std::uint32_t after = g_sampleSeq[hid].load(std::memory_order_acquire);
        if (before == after)
        {
            seq = after;
            raw = value;
            return;
        }
    }
    seq = g_sampleSeq[hid].load(std::memory_order_acquire);
    raw = g_raw[hid].load(std::memory_order_acquire);
}

HANDLE g_wakeEvent = nullptr;
std::thread g_thread;
std::atomic<halljoy::worker::WorkerExceptionKind> g_workerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
halljoy::worker::WorkerExceptionRecord g_workerFaultRecord{};
std::mutex g_logMutex;
bool g_logStarted = false;
std::atomic<std::uint64_t> g_rawLogCount{0};
std::atomic<std::uint64_t> g_malformedWireLogCount{0};

std::wstring PathNearExe(const wchar_t* name)
{
    std::array<wchar_t, 32768> buffer{};
    const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    std::wstring out(buffer.data(), n);
    const auto slash = out.find_last_of(L"\\/");
    if (slash != std::wstring::npos) out.resize(slash + 1); else out.clear();
    out += name;
    return out;
}

std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), needed, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

void Log(const wchar_t* fmt, ...)
{
#if !defined(HALLJOY_DIAGNOSTIC)
    // Final builds expose live state through Backend_GetAnalogTelemetry and the
    // Configuration/Gamepad Tester UI. Do not create or append per-key files.
    (void)fmt;
    return;
#else
    if (!fmt) return;
    wchar_t body[4096]{};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, _countof(body), _TRUNCATE, fmt, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t line[4400]{};
    _snwprintf_s(line, _countof(line), _TRUNCATE,
        L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, body);

    std::lock_guard<std::mutex> lock(g_logMutex);
    const std::wstring path = PathNearExe(L"HallJoyMAD68ProR.log");
    if (!g_logStarted)
    {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
        {
            const std::uint64_t size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            if (size >= kLogMaxBytes)
                MoveFileExW(path.c_str(), (path + L".1").c_str(), MOVEFILE_REPLACE_EXISTING);
        }
        g_logStarted = true;
    }

    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const std::string utf8 = WideToUtf8(line);
    DWORD written = 0;
    if (!utf8.empty()) WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
#endif
}

void HexLine(const std::uint8_t* data, std::size_t bytes, wchar_t* out, std::size_t chars)
{
    if (!out || chars == 0) return;
    out[0] = L'\0';
    std::size_t used = 0;
    for (std::size_t i = 0; i < bytes && used + 4 < chars; ++i)
    {
        const int n = _snwprintf_s(out + used, chars - used, _TRUNCATE,
            i == 0 ? L"%02X" : L" %02X", static_cast<unsigned>(data[i]));
        if (n <= 0) break;
        used += static_cast<std::size_t>(n);
    }
}

ScopedHandle OpenPath(const std::wstring& path, DWORD access, bool overlapped)
{
    const DWORD flags = FILE_ATTRIBUTE_NORMAL | (overlapped ? FILE_FLAG_OVERLAPPED : 0);
    return ScopedHandle(CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, flags, nullptr));
}

std::wstring ReadHidString(HANDLE handle, BOOLEAN (__stdcall *fn)(HANDLE, PVOID, ULONG))
{
    std::array<wchar_t, 256> buffer{};
    if (!fn || !fn(handle, buffer.data(), static_cast<ULONG>(buffer.size() * sizeof(wchar_t)))) return {};
    return std::wstring(buffer.data());
}

bool PopulateCaps(HANDLE handle, HidPath& out)
{
    out.attrs = {};
    out.attrs.Size = sizeof(out.attrs);
    if (!HidD_GetAttributes(handle, &out.attrs)) return false;
    ScopedPreparsed pp;
    if (!HidD_GetPreparsedData(handle, &pp.value)) return false;
    if (HidP_GetCaps(pp.value, &out.caps) != HIDP_STATUS_SUCCESS) return false;
    out.usagePage = out.caps.UsagePage;
    out.usage = out.caps.Usage;
    out.manufacturer = ReadHidString(handle, HidD_GetManufacturerString);
    out.product = ReadHidString(handle, HidD_GetProductString);
    out.serial = ReadHidString(handle, HidD_GetSerialNumberString);
    return true;
}

bool PathLooksLikeInterface1(const std::wstring& path)
{
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return lower.find(L"&mi_01") != std::wstring::npos || lower.find(L"mi_01#") != std::wstring::npos;
}

bool LooksLikeMad68Family(const HidPath& path)
{
    // The native decoder owns the audited 68-position descriptor table. An A9
    // ACK proves the control framing, but not the physical layout, so an unknown
    // PID must also identify itself as a MAD68-family device before UAP is
    // excluded. Other MADLIONS products remain available to Soup/UAP.
    std::wstring identity = path.manufacturer;
    identity.push_back(L' ');
    identity += path.product;
    std::transform(identity.begin(), identity.end(), identity.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return identity.find(L"mad68") != std::wstring::npos ||
        identity.find(L"mad 68") != std::wstring::npos ||
        identity.find(L"mad-68") != std::wstring::npos;
}

bool IsRoutedNativePid(std::uint16_t pid)
{
    std::lock_guard<std::mutex> lock(g_protocolRoutingMutex);
    return std::find(g_routedNativePids.begin(), g_routedNativePids.end(), pid) !=
        g_routedNativePids.end();
}

void PublishRoutedPidEnvironment(const std::vector<std::uint16_t>& pids)
{
    // Multiple validated VID 373B native protocols share the dedicated UAP
    // pre-open exclusion registry. Do not overwrite another backend's tokens.
    for (const std::uint16_t pid : pids)
        NativeAnalogRouting_Claim(mad68pr::kVid, pid, NativeAnalogProtocol::Mad68A0);
}

std::vector<HidPath> EnumerateBrandCandidates(bool logAll, bool routedOnly)
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO info = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return {};

    std::vector<HidPath> candidates;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &guid, index, &iface))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            continue;
        }

        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &iface, nullptr, 0, &bytes, nullptr);
        if (!bytes) continue;
        std::vector<std::uint8_t> storage(bytes);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &iface, detail, bytes, nullptr, nullptr)) continue;

        HidPath item{};
        item.path = detail->DevicePath;
        auto metadata = OpenPath(item.path, 0, false);
        if (!metadata || !PopulateCaps(metadata.value, item)) continue;
        if (item.attrs.VendorID != mad68pr::kVid) continue;
        const bool routedToMad68 = NativeAnalogRouting_IsClaimedBy(
            item.attrs.VendorID, item.attrs.ProductID, NativeAnalogProtocol::Mad68A0);
        if (NativeAnalogRouting_IsClaimed(item.attrs.VendorID, item.attrs.ProductID) && !routedToMad68)
            continue;
        if (routedOnly && !IsRoutedNativePid(item.attrs.ProductID)) continue;

        if (logAll)
        {
            Log(L"enumerate path=%s usage=%04X:%04X in=%u out=%u feature=%u version=%04X product=%s serial=%s",
                item.path.c_str(), item.usagePage, item.usage,
                item.caps.InputReportByteLength, item.caps.OutputReportByteLength,
                item.caps.FeatureReportByteLength, item.attrs.VersionNumber,
                item.product.c_str(), item.serial.c_str());
        }

        const bool exactUsage = item.usagePage == kExpectedUsagePage && item.usage == kExpectedUsage;
        const bool reportSizes = item.caps.InputReportByteLength == mad68pr::kPayloadBytes + 1u &&
            item.caps.OutputReportByteLength == mad68pr::kPayloadBytes + 1u;
        if (reportSizes && (exactUsage || PathLooksLikeInterface1(item.path)))
            candidates.push_back(std::move(item));
    }
    SetupDiDestroyDeviceInfoList(info);

    std::stable_sort(candidates.begin(), candidates.end(), [](const HidPath& a, const HidPath& b) {
        const bool ae = a.usagePage == kExpectedUsagePage && a.usage == kExpectedUsage;
        const bool be = b.usagePage == kExpectedUsagePage && b.usage == kExpectedUsage;
        return ae && !be;
    });
    return candidates;
}

std::vector<HidPath> EnumerateCandidates(bool logAll)
{
    return EnumerateBrandCandidates(logAll, true);
}

bool ProbePresence()
{
    return !EnumerateCandidates(false).empty();
}

bool ProbeAuditedPresence()
{
    const auto candidates = EnumerateCandidates(false);
    return std::any_of(candidates.begin(), candidates.end(), [](const HidPath& path) {
        return path.attrs.VersionNumber == mad68pr::kAuditedBcdDevice;
    });
}

bool PathStillPresent(const std::wstring& expected)
{
    const auto candidates = EnumerateCandidates(false);
    for (const auto& item : candidates)
        if (_wcsicmp(item.path.c_str(), expected.c_str()) == 0) return true;
    return false;
}

const std::uint8_t* NormalizePayload(
    const std::uint8_t* wire,
    std::size_t transferred,
    std::array<std::uint8_t, mad68pr::kPayloadBytes>& normalized)
{
    if (!wire || transferred == 0) return nullptr;
    auto plausible = [](std::uint8_t h) {
        return h == mad68pr::kStreamHeader || h == mad68pr::kNormalResponseHeader ||
            h == mad68pr::kChecksumErrorHeader || h == mad68pr::kRawRequestHeader;
    };

    std::size_t offset = static_cast<std::size_t>(-1);
    if (transferred >= 2 && wire[0] == 0 && plausible(wire[1])) offset = 1;
    else if (plausible(wire[0])) offset = 0;
    else if (transferred >= 2 && plausible(wire[1])) offset = 1;
    if (offset == static_cast<std::size_t>(-1) || offset >= transferred) return nullptr;

    normalized.fill(0);
    const std::size_t available = std::min<std::size_t>(mad68pr::kPayloadBytes, transferred - offset);
    std::copy(wire + offset, wire + offset + available, normalized.begin());
    return normalized.data();
}

void ClearTargetInputState(const wchar_t* reason)
{
    bool changed = false;
    for (auto& down : g_physicalDown)
        changed = down.exchange(false, std::memory_order_acq_rel) || changed;
    for (auto& down : g_digitalDown)
        changed = down.exchange(false, std::memory_order_acq_rel) || changed;
    if (changed)
        Log(L"target Raw Input key state cleared reason=%s", reason ? reason : L"-");
}

bool AnyKeyboardKeyDown()
{
    // Target-scoped Raw Input is the primary guard. GetAsyncKeyState is retained
    // as a conservative startup fallback so a key held before Raw Input
    // registration cannot be sampled as a held-key service state while A8
    // derives its auxiliary per-key working arrays.
    for (const auto& down : g_physicalDown)
        if (down.load(std::memory_order_acquire)) return true;

    for (int vk = 0x08; vk <= 0xFE; ++vk)
    {
        if (vk >= VK_LBUTTON && vk <= VK_XBUTTON2) continue;
        if ((GetAsyncKeyState(vk) & 0x8000) != 0) return true;
    }
    return false;
}

const wchar_t* UiStateName(UiState state)
{
    switch (state)
    {
    case UiState::Starting: return L"starting";
    case UiState::NoDevice: return L"device not found";
    case UiState::Passive: return L"passive listen";
    case UiState::UnsupportedFirmware: return L"unsupported firmware (passive only)";
    case UiState::WaitingRelease: return L"release all keys";
    case UiState::Activating: return L"activating";
    case UiState::Validating: return L"validating 68-key snapshot";
    case UiState::ActiveEmergencyWasd: return L"EMERGENCY WASD";
    case UiState::ActiveFull: return L"ACTIVE 68/68";
    case UiState::Recovering: return L"auto troubleshooting";
    case UiState::Exhausted: return L"all safe strategies exhausted";
    default: return L"stopped";
    }
}

class Session
{
public:
    explicit Session(const HidPath& path) : path_(path) {}
    ~Session() { Close(); }

    bool Open()
    {
        Close();
        consecutiveReadErrors_ = 0;
        read_ = OpenPath(path_.path, GENERIC_READ | GENERIC_WRITE, true);
        if (!read_) read_ = OpenPath(path_.path, GENERIC_READ, true);
        write_ = OpenPath(path_.path, GENERIC_WRITE, true);
        if (!write_) write_ = OpenPath(path_.path, GENERIC_READ | GENERIC_WRITE, true);
        control_ = OpenPath(path_.path, GENERIC_WRITE, false);
        if (!control_) control_ = OpenPath(path_.path, GENERIC_READ | GENERIC_WRITE, false);
        BOOL buffersOk = FALSE;
        if (read_) buffersOk = HidD_SetNumInputBuffers(read_.value, 256);
        readWire_.assign(std::max<std::size_t>(
            mad68pr::kPayloadBytes + 1u, path_.caps.InputReportByteLength), 0);
        Log(L"handles open read=%d write=%d control=%d input_buffers=256 ok=%d wire_bytes=%u",
            read_ ? 1 : 0, write_ ? 1 : 0, control_ ? 1 : 0, buffersOk ? 1 : 0,
            static_cast<unsigned>(readWire_.size()));
        return static_cast<bool>(read_);
    }

    void Close()
    {
        CancelPendingRead();
        read_.reset();
        write_.reset();
        control_.reset();
        readWire_.clear();
    }

    bool Reopen()
    {
        Log(L"reopening HID handles before strategy");
        Close();
        Sleep(80);
        return Open();
    }

    bool ReadPayload(DWORD timeoutMs, std::array<std::uint8_t, mad68pr::kPayloadBytes>& out)
    {
        if (!read_) return false;
        if (!pendingRead_)
        {
            if (readWire_.empty())
                readWire_.assign(std::max<std::size_t>(
                    mad68pr::kPayloadBytes + 1u, path_.caps.InputReportByteLength), 0);
            std::fill(readWire_.begin(), readWire_.end(), 0);
            pendingRead_ = std::make_unique<HidIoOperation>(read_.value);
            DWORD startError = ERROR_SUCCESS;
            const auto start = pendingRead_->StartRead(
                readWire_.data(), static_cast<DWORD>(readWire_.size()), &startError);
            if (start == HidIoOperation::StartResult::Failed)
            {
                pendingRead_.reset();
                if (startError != ERROR_OPERATION_ABORTED && startError != ERROR_SUCCESS)
                {
                    ++stats_.readErrors;
                    ++consecutiveReadErrors_;
                    Log(L"read start failed err=%lu consecutive=%u", startError, consecutiveReadErrors_);
                }
                return false;
            }
            if (start == HidIoOperation::StartResult::Completed)
                return FinishPendingRead(out);
        }

        DWORD wait = pendingRead_->Wait(timeoutMs);
        if (wait == WAIT_TIMEOUT)
        {
            // Keep the same overlapped ReadFile pending. Cancelling and reissuing it
            // every 25 ms creates gaps where the firmware's event-driven A0 report
            // can be lost, especially during the three-sweep A8 snapshot.
            ++stats_.readTimeouts;
            return false;
        }
        if (wait != WAIT_OBJECT_0)
        {
            DWORD transferred = 0;
            DWORD error = ERROR_SUCCESS;
            pendingRead_->CancelAndDrain(&transferred, &error);
            pendingRead_.reset();
            ++stats_.readErrors;
            ++consecutiveReadErrors_;
            Log(L"read wait failed wait=%lu err=%lu consecutive=%u",
                wait, error, consecutiveReadErrors_);
            return false;
        }

        return FinishPendingRead(out);
    }

    bool Send(const Strategy& strategy, std::uint8_t opcode)
    {
        // Hard safety boundary: no other firmware opcode may leave this backend.
        if (opcode != mad68pr::kArmOpcode && opcode != mad68pr::kRestoreInputOpcode)
        {
            Log(L"SAFETY BLOCK: refused opcode=%02X", opcode);
            return false;
        }

        const auto payload = mad68pr::MakeZeroPayloadRequest(opcode, strategy.framing, strategy.xorKey);
        wchar_t hex[512]{};
        HexLine(payload.data(), 20, hex, _countof(hex));
        Log(L"TX strategy=%s transport=%s opcode=%02X frame=%02X xor=%02X payload20=%s",
            strategy.name, TransportName(strategy.transport), opcode,
            strategy.framing, strategy.xorKey, hex);

        bool ok = false;
        switch (strategy.transport)
        {
        case SendTransport::InterruptCaps:
            ok = SendInterrupt(payload, false);
            break;
        case SendTransport::InterruptRaw64:
            ok = SendInterrupt(payload, true);
            break;
        case SendTransport::ControlCaps:
            ok = SendControl(payload);
            break;
        }
        ++stats_.writes;
        if (!ok) ++stats_.writeErrors;
        return ok;
    }

    SessionStats& Stats() { return stats_; }
    unsigned ConsecutiveReadErrors() const { return consecutiveReadErrors_; }

private:
    bool FinishPendingRead(std::array<std::uint8_t, mad68pr::kPayloadBytes>& out)
    {
        if (!pendingRead_) return false;
        DWORD transferred = 0;
        DWORD error = ERROR_SUCCESS;
        const bool ok = pendingRead_->Finish(&transferred, &error, false);
        pendingRead_.reset();
        if (!ok)
        {
            if (error != ERROR_OPERATION_ABORTED && error != ERROR_SUCCESS && error != ERROR_IO_INCOMPLETE)
            {
                ++stats_.readErrors;
                ++consecutiveReadErrors_;
                Log(L"read completion failed err=%lu consecutive=%u", error, consecutiveReadErrors_);
            }
            return false;
        }

        consecutiveReadErrors_ = 0;
        ++stats_.reads;
        if (NormalizePayload(readWire_.data(), transferred, out) != nullptr)
            return true;

        ++stats_.malformed;
        const std::uint64_t malformedIndex = g_malformedWireLogCount.fetch_add(1, std::memory_order_relaxed);
        if (malformedIndex < kMaxMalformedWireLogPackets)
        {
            wchar_t hex[512]{};
            HexLine(readWire_.data(), std::min<std::size_t>(transferred, 32u), hex, _countof(hex));
            Log(L"malformed HID input report index=%llu transferred=%lu expected_payload=%u wire32=%s",
                static_cast<unsigned long long>(malformedIndex + 1), transferred,
                static_cast<unsigned>(mad68pr::kPayloadBytes), hex);
        }
        return false;
    }

    void CancelPendingRead()
    {
        if (!pendingRead_) return;
        DWORD transferred = 0;
        DWORD error = ERROR_SUCCESS;
        pendingRead_->CancelAndDrain(&transferred, &error);
        pendingRead_.reset();
    }

    static const wchar_t* TransportName(SendTransport mode)
    {
        switch (mode)
        {
        case SendTransport::InterruptCaps: return L"WriteFile/caps";
        case SendTransport::InterruptRaw64: return L"WriteFile/raw64";
        default: return L"HidD_SetOutputReport";
        }
    }

    bool SendInterrupt(const std::array<std::uint8_t, mad68pr::kPayloadBytes>& payload, bool raw64)
    {
        if (!write_) return false;
        const std::size_t wireBytes = raw64
            ? mad68pr::kPayloadBytes
            : std::max<std::size_t>(9u, path_.caps.OutputReportByteLength);
        std::vector<std::uint8_t> wire(wireBytes, 0);
        const std::size_t offset = raw64 ? 0u : 1u;
        const std::size_t copyBytes = std::min<std::size_t>(payload.size(), wire.size() - offset);
        if (copyBytes < 8u) return false;
        std::copy(payload.begin(), payload.begin() + copyBytes, wire.begin() + offset);

        HidIoOperation operation(write_.value);
        DWORD error = ERROR_SUCCESS;
        const auto start = operation.StartWrite(wire.data(), static_cast<DWORD>(wire.size()), &error);
        DWORD transferred = 0;
        if (start == HidIoOperation::StartResult::Completed)
        {
            const bool ok = operation.Finish(&transferred, &error, false);
            Log(L"WriteFile completed ok=%d bytes=%lu/%u err=%lu", ok ? 1 : 0,
                transferred, static_cast<unsigned>(wire.size()), error);
            return ok && transferred == wire.size();
        }
        if (start != HidIoOperation::StartResult::Pending)
        {
            Log(L"WriteFile start failed err=%lu bytes=%u", error, static_cast<unsigned>(wire.size()));
            return false;
        }
        const DWORD wait = operation.Wait(1000);
        if (wait != WAIT_OBJECT_0)
        {
            operation.CancelAndDrain(&transferred, &error);
            Log(L"WriteFile timeout/cancel wait=%lu err=%lu bytes=%lu", wait, error, transferred);
            return false;
        }
        const bool ok = operation.Finish(&transferred, &error, false);
        Log(L"WriteFile reaped ok=%d bytes=%lu/%u err=%lu", ok ? 1 : 0,
            transferred, static_cast<unsigned>(wire.size()), error);
        return ok && transferred == wire.size();
    }

    bool SendControl(const std::array<std::uint8_t, mad68pr::kPayloadBytes>& payload)
    {
        if (!control_) return false;
        const std::size_t wireBytes = std::max<std::size_t>(9u, path_.caps.OutputReportByteLength);
        std::vector<std::uint8_t> wire(wireBytes, 0);
        const std::size_t offset = 1u;
        const std::size_t copyBytes = std::min<std::size_t>(payload.size(), wire.size() - offset);
        if (copyBytes < 8u) return false;
        std::copy(payload.begin(), payload.begin() + copyBytes, wire.begin() + offset);
        const BOOL ok = HidD_SetOutputReport(control_.value, wire.data(), static_cast<ULONG>(wire.size()));
        Log(L"HidD_SetOutputReport ok=%d bytes=%u err=%lu", ok ? 1 : 0,
            static_cast<unsigned>(wire.size()), ok ? ERROR_SUCCESS : GetLastError());
        return ok != FALSE;
    }

    const HidPath& path_;
    ScopedHandle read_;
    ScopedHandle write_;
    ScopedHandle control_;
    std::vector<std::uint8_t> readWire_;
    std::unique_ptr<HidIoOperation> pendingRead_;
    SessionStats stats_{};
    unsigned consecutiveReadErrors_ = 0;
};

bool ProbeNativeControlProtocol(const HidPath& path)
{
    Session session(path);
    if (!session.Open())
        return false;

    const Strategy& strategy = kStrategies[0];
    if (!session.Send(strategy, mad68pr::kRestoreInputOpcode))
        return false;

    const ULONGLONG deadline = GetTickCount64() + 450u;
    while (GetTickCount64() < deadline)
    {
        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        if (!session.ReadPayload(25u, packet))
            continue;
        const mad68pr::ControlResponse response = mad68pr::DecodeControlResponse(
            packet.data(), packet.size(), strategy.framing, mad68pr::kRestoreInputOpcode);
        if (response.kind == mad68pr::ControlResponseKind::Valid)
            return true;
        if (response.kind == mad68pr::ControlResponseKind::ChecksumError ||
            response.kind == mad68pr::ControlResponseKind::Invalid)
            return false;
    }
    return false;
}

struct DigitalWatch
{
    std::array<std::uint32_t, 256> seenSeq{};
    std::array<bool, 256> pending{};
    std::array<bool, 256> expectedDown{};
    std::array<ULONGLONG, 256> eventMs{};
    std::array<std::uint32_t, 256> sampleSeqAtEvent{};
    std::array<std::uint16_t, 256> rawAtEvent{};
    std::array<std::uint8_t, 256> failureStreak{};
    std::array<DWORD, 256> deadlineMs{};
    std::array<bool, 256> liveStreamExtension{};
    std::uint32_t seenResetSeq = 0;
};

void SynchroniseDigitalWatch(DigitalWatch& digital, bool clearFailureStreak)
{
    for (const auto& descriptor : mad68pr::kKeyDescriptors)
    {
        const std::uint16_t hid = descriptor.hid;
        if (hid == 0 || hid >= 256) continue;
        digital.seenSeq[hid] = g_digitalSeq[hid].load(std::memory_order_acquire);
        digital.pending[hid] = false;
        digital.expectedDown[hid] = false;
        digital.eventMs[hid] = 0;
        CaptureAnalogSnapshot(hid, digital.sampleSeqAtEvent[hid], digital.rawAtEvent[hid]);
        digital.deadlineMs[hid] = 0;
        digital.liveStreamExtension[hid] = false;
        if (clearFailureStreak) digital.failureStreak[hid] = 0;
    }
}

PublishMode CurrentPublishMode()
{
    return static_cast<PublishMode>(g_publishMode.load(std::memory_order_acquire));
}

void SetPublishMode(PublishMode mode, const wchar_t* reason)
{
    const PublishMode previous = static_cast<PublishMode>(
        g_publishMode.exchange(static_cast<int>(mode), std::memory_order_acq_rel));
    g_streamConnected.store(mode != PublishMode::None, std::memory_order_release);
    if (mode == PublishMode::Full)
        g_uiState.store(static_cast<int>(UiState::ActiveFull), std::memory_order_release);
    else if (mode == PublishMode::EmergencyWasd)
        g_uiState.store(static_cast<int>(UiState::ActiveEmergencyWasd), std::memory_order_release);
    if (previous != mode)
        Log(L"publish mode %d -> %d reason=%s", static_cast<int>(previous),
            static_cast<int>(mode), reason ? reason : L"-");
}

std::uint32_t CurrentCoverage();
bool HasCompleteSnapshot();

bool ModeOwnsHid(PublishMode mode, std::uint16_t hid)
{
    if (mode == PublishMode::Full) return mad68pr::IsPublishedHid(hid);
    if (mode == PublishMode::EmergencyWasd) return mad68pr::IsWasdHid(hid);
    return false;
}

void ConfirmSteadyState(const wchar_t* reason)
{
    bool expected = false;
    if (g_steadyStateConfirmed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        Log(L"STEADY-STATE A0 CONFIRMED reason=%s ordered_startup_cycles=%u coverage=%u/68",
            reason ? reason : L"-",
            g_orderedSweepCycles.load(std::memory_order_relaxed),
            CurrentCoverage());
        // Ordered cycle tracking is only an activation diagnostic. Once a
        // genuinely post-sweep physical edge has produced a fresh descriptor
        // report, random steady-state events must not be interpreted as another
        // startup cycle.
        g_activationEpochMs.store(0, std::memory_order_release);
        g_orderedSweepPosition.store(0, std::memory_order_release);
    }

    if (HasCompleteSnapshot() && CurrentPublishMode() == PublishMode::EmergencyWasd)
        SetPublishMode(PublishMode::Full,
            L"post-sweep physical edge produced a fresh A0 and 68/68 snapshot is available");
}

void MaybeConfirmSteadyStateFromPhysicalEdge(
    ULONGLONG eventMs, ULONGLONG sampleMs, bool correlatedValue)
{
    const ULONGLONG forcedUntil =
        g_forcedSweepGraceUntilMs.load(std::memory_order_acquire);
    const ULONGLONG lastOrdered =
        g_lastOrderedSweepMs.load(std::memory_order_acquire);
    const bool startupSweepQuiet =
        lastOrdered == 0 || sampleMs < lastOrdered || sampleMs - lastOrdered >= 150;
    if (eventMs >= forcedUntil && sampleMs >= eventMs &&
        startupSweepQuiet && correlatedValue)
    {
        ConfirmSteadyState(
            L"new correlated descriptor packet after a post-sweep physical edge");
    }
}

void MaybeConfirmSteadyStateFromAnalogOnlyEdge(
    std::size_t keyIndex,
    ULONGLONG sampleMs,
    std::uint16_t previousRaw,
    const mad68pr::KeySample& sample)
{
    if (g_steadyStateConfirmed.load(std::memory_order_acquire)) return;
    if (g_orderedSweepCycles.load(std::memory_order_acquire) < 3u) return;
    if (!HasCompleteSnapshot()) return;

    const ULONGLONG forcedUntil =
        g_forcedSweepGraceUntilMs.load(std::memory_order_acquire);
    if (forcedUntil == 0 || sampleMs < forcedUntil) return;

    const ULONGLONG lastOrdered =
        g_lastOrderedSweepMs.load(std::memory_order_acquire);
    if (lastOrdered != 0 && sampleMs >= lastOrdered && sampleMs - lastOrdered < 150) return;

    if (!mad68pr::IsPostSweepAnalogProof(previousRaw, sample.raw, sample.threshold)) return;

    Log(L"post-grace A0 analogue edge proof key=%hs hid=%02X raw=%u->%u threshold=%u "
        L"raw_input_edges=%llu ordered_cycles=%u",
        mad68pr::KeyName(keyIndex), sample.hid, previousRaw, sample.raw, sample.threshold,
        static_cast<unsigned long long>(g_rawInputEdges.load(std::memory_order_relaxed)),
        g_orderedSweepCycles.load(std::memory_order_relaxed));
    ConfirmSteadyState(
        L"post-grace A0 crossed its per-key actuation threshold after forced sweeps");
}

void TrackOrderedStartupSweep(std::size_t keyIndex, ULONGLONG now)
{
    if (g_activationEpochMs.load(std::memory_order_acquire) == 0) return;

    std::uint32_t pos = g_orderedSweepPosition.load(std::memory_order_relaxed);
    if (keyIndex == pos)
    {
        ++pos;
        if (pos == mad68pr::kPhysicalKeyCount)
        {
            pos = 0;
            const std::uint32_t cycle =
                g_orderedSweepCycles.fetch_add(1u, std::memory_order_acq_rel) + 1u;
            g_lastOrderedSweepMs.store(now, std::memory_order_release);
            Log(L"ordered A0 matrix cycle complete cycle=%u/4 duration_since_A8_ms=%llu kind=%s",
                cycle,
                static_cast<unsigned long long>(now - g_activationEpochMs.load(std::memory_order_relaxed)),
                cycle <= 3 ? L"forced" : (cycle == 4 ? L"steady-mirror-catchup" : L"additional"));
        }
        g_orderedSweepPosition.store(pos, std::memory_order_release);
        return;
    }

    // Restart only when the first scanner descriptor is observed. Random
    // steady-state events must not be mistaken for a complete ordered sweep.
    g_orderedSweepPosition.store(keyIndex == 0 ? 1u : 0u, std::memory_order_release);
}

bool AnalogMatchesDigital(std::uint16_t hid, bool expectedDown,
    std::uint16_t rawAtEvent, std::uint16_t currentRaw)
{
    const std::uint16_t threshold = hid < 256
        ? g_threshold[hid].load(std::memory_order_acquire) : 0;
    return mad68pr::AnalogTransitionMatchesDigital(
        expectedDown, threshold, rawAtEvent, currentRaw);
}

std::uint32_t CurrentCoverage()
{
    std::uint32_t coverage = 0;
    for (const auto& seq : g_descriptorSeq)
        if (seq.load(std::memory_order_acquire) != 0) ++coverage;
    return coverage;
}

bool HasCompleteSnapshot()
{
    return CurrentCoverage() == mad68pr::kPhysicalKeyCount;
}

std::uint32_t CurrentWasdCoverage()
{
    std::uint32_t count = 0;
    for (std::uint16_t hid : { std::uint16_t(0x1A), std::uint16_t(0x04),
        std::uint16_t(0x16), std::uint16_t(0x07) })
    {
        const int index = mad68pr::KeyIndexFromHid(hid);
        if (index >= 0 && g_descriptorSeq[static_cast<std::size_t>(index)].load(
            std::memory_order_acquire) != 0) ++count;
    }
    return count;
}

bool HasWasdSnapshot()
{
    return CurrentWasdCoverage() == 4;
}

bool ProcessPayload(
    const std::array<std::uint8_t, mad68pr::kPayloadBytes>& packet,
    SessionStats& stats,
    DigitalWatch& digital,
    bool verboseUnknown)
{
    if (packet[0] == mad68pr::kStreamHeader)
    {
        const LONGLONG a0ReceivedQpc = Mad68QpcNow();
        ++stats.a0Packets;
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG previousA0 = g_lastA0Ms.exchange(now, std::memory_order_acq_rel);
        if (previousA0 != 0 && now >= previousA0)
        {
            const ULONGLONG gap = now - previousA0;
            stats.maxA0GapMs = std::max<std::uint64_t>(stats.maxA0GapMs, gap);
            if (gap > 50)
            {
                ++stats.a0GapOver50Ms;
                if (stats.a0GapOver50Ms <= 32 || gap >= 500)
                    Log(L"A0 transport gap gap_ms=%llu count_over_50=%llu steady=%d mode=%d",
                        static_cast<unsigned long long>(gap),
                        static_cast<unsigned long long>(stats.a0GapOver50Ms),
                        g_steadyStateConfirmed.load(std::memory_order_relaxed) ? 1 : 0,
                        static_cast<int>(CurrentPublishMode()));
            }
        }
        mad68pr::KeySample sample{};
        if (!mad68pr::DecodeKeySample(packet.data(), packet.size(), sample))
        {
            ++stats.unknownA0;
            if (verboseUnknown || stats.unknownA0 <= kMaxUnknownA0LogPackets || (stats.unknownA0 % 256u) == 0u)
            {
                wchar_t hex[256]{};
                HexLine(packet.data(), 20, hex, _countof(hex));
                Log(L"RX A0 UNKNOWN desc=%02X:%02X:%02X raw=%u packet20=%s",
                    packet[1], packet[2], packet[3],
                    static_cast<unsigned>((packet[4] << 8) | packet[5]), hex);
            }
            return true;
        }

        ++stats.keyPackets;
        const std::size_t keyIndex = sample.keyIndex;
        TrackOrderedStartupSweep(keyIndex, now);
        const std::uint32_t descriptorBefore =
            g_descriptorSeq[keyIndex].fetch_add(1u, std::memory_order_acq_rel);
        const std::uint16_t previousDescriptorRaw =
            g_descriptorRaw[keyIndex].exchange(sample.raw, std::memory_order_acq_rel);

        bool publishedValueChanged = false;
        if (sample.hid != 0 && sample.hid < 256)
        {
            const std::size_t hid = sample.hid;
            const std::uint16_t previousHidRaw =
                g_raw[hid].load(std::memory_order_acquire);
            g_raw[hid].store(sample.raw, std::memory_order_release);
            g_milli[hid].store(sample.milli, std::memory_order_release);
            g_threshold[hid].store(sample.threshold, std::memory_order_release);
            g_baseline[hid].store(sample.baseline, std::memory_order_release);
            g_keyState[hid].store(sample.state, std::memory_order_release);
            g_sampleMs[hid].store(now, std::memory_order_release);
            g_sampleSeq[hid].fetch_add(1u, std::memory_order_acq_rel);
            publishedValueChanged = descriptorBefore == 0 || previousHidRaw != sample.raw;
            if (publishedValueChanged)
            {
                const LONGLONG snapshotPublishedQpc = Mad68QpcNow();
                PublishAnalogueChange(
                    static_cast<std::uint16_t>(hid), sample.raw,
                    a0ReceivedQpc, snapshotPublishedQpc);
            }
        }

        if (descriptorBefore == 0)
        {
            const std::uint32_t coverage = g_coverage.fetch_add(1u, std::memory_order_acq_rel) + 1u;
            Log(L"RX A0 first key=%hs slot=%u internal=%u hid=%02X raw=%u threshold=%u baseline=%u state=%02X coverage=%u/68",
                mad68pr::KeyName(keyIndex), sample.scannerSlot, sample.internalId, sample.hid,
                sample.raw, sample.threshold, sample.baseline, sample.state, coverage);
            if (coverage == mad68pr::kPhysicalKeyCount)
            {
                Log(L"FULL MATRIX OBSERVED: 68/68 descriptors available; publication waits for passive or post-A8 validation");
            }
        }
        else
        {
            const unsigned delta = previousDescriptorRaw > sample.raw
                ? previousDescriptorRaw - sample.raw
                : sample.raw - previousDescriptorRaw;
            if (delta >= kSignificantRawLogDelta)
            {
                Log(L"RX A0 change key=%hs hid=%02X raw=%u->%u milli=%u threshold=%u baseline=%u state=%02X",
                    mad68pr::KeyName(keyIndex), sample.hid, previousDescriptorRaw, sample.raw,
                    sample.milli, sample.threshold, sample.baseline, sample.state);
            }
        }

        if (descriptorBefore != 0)
            MaybeConfirmSteadyStateFromAnalogOnlyEdge(
                keyIndex, now, previousDescriptorRaw, sample);

        if (sample.hid != 0 && sample.hid < 256)
        {
            const std::size_t hid = sample.hid;
            if (digital.pending[hid] &&
                g_sampleSeq[hid].load(std::memory_order_acquire) > digital.sampleSeqAtEvent[hid])
            {
                const bool valueMatches = AnalogMatchesDigital(
                    static_cast<std::uint16_t>(hid), digital.expectedDown[hid],
                    digital.rawAtEvent[hid], sample.raw);
                Log(valueMatches
                        ? L"digital/analog fresh packet OK key=%hs state=%s latency_ms=%llu raw=%u"
                        : L"digital/analog fresh packet VALUE MISMATCH key=%hs state=%s latency_ms=%llu raw=%u (transport healthy; no recovery)",
                    mad68pr::KeyName(keyIndex),
                    digital.expectedDown[hid] ? L"down" : L"up",
                    static_cast<unsigned long long>(now - digital.eventMs[hid]),
                    sample.raw);
                if (!valueMatches) ++stats.semanticMismatches;
                const unsigned edgeDelta = digital.rawAtEvent[hid] > sample.raw
                    ? digital.rawAtEvent[hid] - sample.raw
                    : sample.raw - digital.rawAtEvent[hid];
                MaybeConfirmSteadyStateFromPhysicalEdge(
                    digital.eventMs[hid], now,
                    valueMatches || edgeDelta >= 8u);
                // Troubleshooting is a transport watchdog. A fresh packet for the
                // exact descriptor proves that analogue delivery is alive even if
                // our inferred threshold/direction semantics are imperfect.
                digital.pending[hid] = false;
                digital.failureStreak[hid] = 0;
            }
        }
        return true;
    }

    if (packet[0] == mad68pr::kNormalResponseHeader ||
        packet[0] == mad68pr::kChecksumErrorHeader ||
        packet[0] == mad68pr::kRawRequestHeader)
    {
        ++stats.controlResponses;
        if (packet[0] == mad68pr::kChecksumErrorHeader) ++stats.checksumErrors;
        wchar_t hex[256]{};
        HexLine(packet.data(), 20, hex, _countof(hex));
        Log(L"RX control header=%02X opcode=%02X packet20=%s", packet[0], packet[1], hex);
        return true;
    }

    ++stats.malformed;
    if (g_rawLogCount.fetch_add(1u, std::memory_order_relaxed) < kMaxRawLogPackets)
    {
        wchar_t hex[256]{};
        HexLine(packet.data(), 20, hex, _countof(hex));
        Log(L"RX other header=%02X packet20=%s", packet[0], hex);
    }
    return false;
}

void PumpFor(Session& session, DWORD durationMs, DigitalWatch& digital)
{
    const ULONGLONG deadline = GetTickCount64() + durationMs;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        const DWORD remaining = static_cast<DWORD>(std::max<ULONGLONG>(1, deadline - GetTickCount64()));
        if (session.ReadPayload(std::min<DWORD>(kReadSliceMs, remaining), packet))
            ProcessPayload(packet, session.Stats(), digital, false);
    }
}

bool WaitForAck(
    Session& session,
    const Strategy& strategy,
    std::uint8_t opcode,
    DigitalWatch& digital,
    DWORD timeoutMs)
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        const DWORD remaining = static_cast<DWORD>(std::max<ULONGLONG>(1, deadline - GetTickCount64()));
        if (!session.ReadPayload(std::min<DWORD>(kReadSliceMs, remaining), packet)) continue;

        const auto response = mad68pr::DecodeControlResponse(
            packet.data(), packet.size(), strategy.framing, opcode);
        ProcessPayload(packet, session.Stats(), digital, false);
        if (response.kind == mad68pr::ControlResponseKind::Valid)
        {
            Log(L"ACK valid strategy=%s opcode=%02X header=%02X len=%u checksum=%02X/%02X",
                strategy.name, opcode, response.header, response.length,
                response.checksum, response.expectedChecksum);
            return true;
        }
        if (response.kind == mad68pr::ControlResponseKind::ChecksumError)
        {
            Log(L"ACK checksum-error strategy=%s opcode=%02X", strategy.name, opcode);
            return false;
        }
        if (response.kind == mad68pr::ControlResponseKind::Invalid)
            Log(L"ACK invalid/mismatched strategy=%s expected=%02X got_header=%02X got_opcode=%02X",
                strategy.name, opcode, response.header, response.opcode);
    }
    Log(L"ACK timeout strategy=%s opcode=%02X timeout_ms=%lu", strategy.name, opcode, timeoutMs);
    return false;
}

bool SendCommand(
    Session& session,
    const Strategy& strategy,
    std::uint8_t opcode,
    DigitalWatch& digital)
{
    if (!session.Send(strategy, opcode)) return false;
    if (strategy.requireAck)
        return WaitForAck(session, strategy, opcode, digital, kCommandAckMs);
    PumpFor(session, strategy.interCommandDelayMs, digital);
    return true;
}

bool WaitForAllReleased(Session& session, DigitalWatch& digital)
{
    g_uiState.store(static_cast<int>(UiState::WaitingRelease), std::memory_order_release);
    ULONGLONG lastLog = 0;
    ULONGLONG allReleasedSince = 0;
    while (!g_stop.load(std::memory_order_acquire))
    {
        const ULONGLONG now = GetTickCount64();
        if (!AnyKeyboardKeyDown())
        {
            if (allReleasedSince == 0) allReleasedSince = now;
            if (now - allReleasedSince >= kAllReleasedStableMs)
            {
                Log(L"all-keys-up stable for %lu ms before short A8 service-mode transition", kAllReleasedStableMs);
                return true;
            }
        }
        else
        {
            allReleasedSince = 0;
        }

        if (lastLog == 0 || now - lastLog >= kReleaseWaitLogMs)
        {
            Log(L"waiting for all keys to be released before A8; avoiding service-mode entry while a key is held");
            lastLog = now;
        }

        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        if (session.ReadPayload(50, packet))
            ProcessPayload(packet, session.Stats(), digital, false);
    }
    return false;
}

using SnapshotBaseline = std::array<std::uint32_t, mad68pr::kPhysicalKeyCount>;

SnapshotBaseline CaptureSnapshotBaseline()
{
    SnapshotBaseline baseline{};
    for (std::size_t i = 0; i < baseline.size(); ++i)
        baseline[i] = g_descriptorSeq[i].load(std::memory_order_acquire);
    return baseline;
}

std::uint32_t FreshCoverage(const SnapshotBaseline& baseline)
{
    std::uint32_t fresh = 0;
    for (std::size_t i = 0; i < baseline.size(); ++i)
        if (g_descriptorSeq[i].load(std::memory_order_acquire) > baseline[i]) ++fresh;
    return fresh;
}

std::uint32_t FreshWasdCoverage(const SnapshotBaseline& baseline)
{
    std::uint32_t fresh = 0;
    for (std::uint16_t hid : { std::uint16_t(0x1A), std::uint16_t(0x04),
        std::uint16_t(0x16), std::uint16_t(0x07) })
    {
        const int index = mad68pr::KeyIndexFromHid(hid);
        if (index >= 0 && g_descriptorSeq[static_cast<std::size_t>(index)].load(
            std::memory_order_acquire) > baseline[static_cast<std::size_t>(index)]) ++fresh;
    }
    return fresh;
}

bool ValidateStreamAfterActivation(
    Session& session,
    DigitalWatch& digital,
    const SnapshotBaseline& beforeA8)
{
    g_uiState.store(static_cast<int>(UiState::Validating), std::memory_order_release);
    const ULONGLONG deadline = GetTickCount64() + kValidationMs;
    while (!g_stop.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        if (session.ReadPayload(kReadSliceMs, packet))
            ProcessPayload(packet, session.Stats(), digital, false);

        const std::uint32_t fresh = FreshCoverage(beforeA8);
        const std::uint32_t freshWasd = FreshWasdCoverage(beforeA8);
        if (freshWasd == 4 && CurrentPublishMode() == PublishMode::None)
        {
            SetPublishMode(PublishMode::EmergencyWasd,
                L"fresh W/A/S/D received after A8 while full sweep is still validating");
            Log(L"emergency WASD publication enabled: fresh 4/4; continuing full 68-key validation");
        }
        if (fresh == mad68pr::kPhysicalKeyCount)
        {
            ++session.Stats().completeSnapshots;
            SetPublishMode(PublishMode::EmergencyWasd,
                L"fresh 68/68 startup snapshot received; waiting for post-sweep edge proof before full ownership");
            Log(L"activation snapshot succeeded: fresh 68/68 received after A8; steady-state remains UNCONFIRMED");
            return true;
        }
    }

    const std::uint32_t fresh = FreshCoverage(beforeA8);
    const std::uint32_t freshWasd = FreshWasdCoverage(beforeA8);
    if (freshWasd == 4)
    {
        SetPublishMode(PublishMode::EmergencyWasd,
            L"full validation timed out but fresh W/A/S/D are confirmed");
        Log(L"activation degraded safely to emergency WASD: fresh=%u/68 WASD=4/4 total=%u/68",
            fresh, CurrentCoverage());
        return true;
    }

    // Use only reports from this newly opened session and only while the A0
    // stream is recent. This rescues snapshots that started before the exact
    // pre-A8 sequence capture without carrying data across reconnects.
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG lastA0 = g_lastA0Ms.load(std::memory_order_acquire);
    const bool recentA0 = lastA0 != 0 && now >= lastA0 && now - lastA0 <= kStreamSalvageAgeMs;
    if (recentA0 && HasCompleteSnapshot())
    {
        ++session.Stats().completeSnapshots;
        SetPublishMode(PublishMode::EmergencyWasd,
            L"recent 68/68 startup snapshot salvaged; waiting for post-sweep edge proof");
        Log(L"activation snapshot salvage succeeded: total current-session coverage=68/68 steady_state=UNCONFIRMED last_A0_age_ms=%llu",
            static_cast<unsigned long long>(now - lastA0));
        return true;
    }
    if (recentA0 && HasWasdSnapshot())
    {
        SetPublishMode(PublishMode::EmergencyWasd,
            L"recent current-session W/A/S/D snapshot salvaged after activation timeout");
        Log(L"activation salvage degraded to emergency WASD: fresh=%u/68 fresh_WASD=%u/4 total=%u/68 last_A0_age_ms=%llu",
            fresh, freshWasd, CurrentCoverage(),
            static_cast<unsigned long long>(now - lastA0));
        return true;
    }

    Log(L"activation validation failed: fresh coverage=%u/68 fresh_WASD=%u/4 total_session_coverage=%u/68 recent_A0=%d",
        fresh, freshWasd, CurrentCoverage(), recentA0 ? 1 : 0);
    return false;
}

void BestEffortRestore(Session& session, const Strategy& strategy, DigitalWatch& digital)
{
    Log(L"best-effort A9 input recovery strategy=%s", strategy.name);
    if (session.Send(strategy, mad68pr::kRestoreInputOpcode))
        PumpFor(session, 200, digital);

    // A fallback transport may have succeeded at A8 while failing to carry A9.
    // Always finish with the hardware-confirmed primary A9 framing as a second,
    // idempotent safety net so ordinary keyboard input is not left suppressed.
    const Strategy& primary = kStrategies[0];
    if (&strategy != &primary)
    {
        Log(L"best-effort A9 secondary safety net via audited primary transport=%s", primary.name);
        if (session.Send(primary, mad68pr::kRestoreInputOpcode))
            PumpFor(session, 200, digital);
    }
}

bool RunStrategy(Session& session, const Strategy& strategy, int index, DigitalWatch& digital, SnapshotBaseline* validatedBaseline)
{
    g_strategyIndex.store(index, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Activating), std::memory_order_release);
    Log(L"===== strategy %d/%u begin: %s =====", index + 1,
        static_cast<unsigned>(kStrategies.size()), strategy.name);
    SynchroniseDigitalWatch(digital, true);

    if (!session.Reopen())
    {
        Log(L"strategy failed: cannot reopen HID handles");
        return false;
    }

    if (!SendCommand(session, strategy, mad68pr::kRestoreInputOpcode, digital))
    {
        Log(L"strategy failed at initial A9 normalization");
        BestEffortRestore(session, strategy, digital);
        return false;
    }

    if (!WaitForAllReleased(session, digital))
    {
        BestEffortRestore(session, strategy, digital);
        return false;
    }

    // Capture before A8. A0 reports may arrive while waiting for A8/A9 ACKs,
    // so capturing after the commands would incorrectly discard the fast snapshot.
    const SnapshotBaseline beforeA8 = CaptureSnapshotBaseline();
    g_steadyStateConfirmed.store(false, std::memory_order_release);
    g_rawInputEdges.store(0, std::memory_order_release);
    g_orderedSweepCycles.store(0, std::memory_order_release);
    g_orderedSweepPosition.store(0, std::memory_order_release);
    g_lastOrderedSweepMs.store(0, std::memory_order_release);
    g_activationEpochMs.store(GetTickCount64(), std::memory_order_release);

    const bool a8Acked = SendCommand(session, strategy, mad68pr::kArmOpcode, digital);
    if (!a8Acked)
    {
        // Do not discard a successful firmware transition merely because the AA/A8
        // control response was lost. A8 itself immediately starts a forced 3-pass
        // matrix sweep. Seeing many distinct fresh descriptors after a stable
        // all-keys-up interval is strong semantic evidence that A8 executed.
        const std::uint32_t semanticFresh = FreshCoverage(beforeA8);
        if (semanticFresh < kA8SemanticEvidenceMinFresh)
        {
            Log(L"strategy failed at A8 arm: ACK missing and semantic evidence too weak fresh=%u threshold=%u",
                semanticFresh, kA8SemanticEvidenceMinFresh);
            BestEffortRestore(session, strategy, digital);
            return false;
        }
        Log(L"A8 ACK missing, but forced-sweep semantic evidence confirms execution: fresh=%u threshold=%u; continuing to mandatory A9",
            semanticFresh, kA8SemanticEvidenceMinFresh);
    }

    const ULONGLONG forcedSweepUntil = GetTickCount64() + kForcedSweepGraceMs;
    g_forcedSweepGraceUntilMs.store(forcedSweepUntil, std::memory_order_release);
    Log(L"A8 accepted%s: forced 3x72-slot sweep grace active for %lu ms (per-key deadline=%lu ms)",
        a8Acked ? L" with ACK" : L" by semantic evidence",
        kForcedSweepGraceMs, kForcedSweepDigitalDeadlineMs);

    if (!SendCommand(session, strategy, mad68pr::kRestoreInputOpcode, digital))
    {
        Log(L"strategy failed at final A9 input restore");
        BestEffortRestore(session, strategy, digital);
        return false;
    }

    const bool valid = ValidateStreamAfterActivation(session, digital, beforeA8);
    if (valid && validatedBaseline) *validatedBaseline = beforeA8;
    Log(L"===== strategy %d end result=%s =====", index + 1,
        valid ? (CurrentPublishMode() == PublishMode::Full ? L"SUCCESS FULL STEADY" : L"SUCCESS SNAPSHOT/WASD; STEADY PENDING") : L"NO USABLE STREAM");
    if (!valid) BestEffortRestore(session, strategy, digital);
    return valid;
}

void ObserveDigitalEvents(DigitalWatch& digital, SessionStats& stats)
{
    const std::uint32_t resetSeq = g_digitalResetSeq.load(std::memory_order_acquire);
    if (resetSeq != digital.seenResetSeq)
    {
        digital.seenResetSeq = resetSeq;
        SynchroniseDigitalWatch(digital, true);
        Log(L"digital watch synchronized after target Raw Input device reset generation=%u", resetSeq);
        return;
    }

    const PublishMode publishMode = CurrentPublishMode();
    if (publishMode == PublishMode::None)
    {
        SynchroniseDigitalWatch(digital, false);
        return;
    }

    const ULONGLONG now = GetTickCount64();
    for (const auto& descriptor : mad68pr::kKeyDescriptors)
    {
        const std::uint16_t hid = descriptor.hid;
        if (hid == 0 || hid >= 256 || !ModeOwnsHid(publishMode, hid)) continue;

        const std::uint32_t seq = g_digitalSeq[hid].load(std::memory_order_acquire);
        if (seq != digital.seenSeq[hid])
        {
            digital.seenSeq[hid] = seq;
            const bool down = g_digitalDown[hid].load(std::memory_order_acquire);
            const ULONGLONG eventMs = g_digitalMs[hid].load(std::memory_order_acquire);
            const std::uint16_t rawAtEvent =
                g_rawAtDigitalEvent[hid].load(std::memory_order_acquire);
            const std::uint32_t sampleSeqAtEvent =
                g_sampleSeqAtDigitalEvent[hid].load(std::memory_order_acquire);
            const ULONGLONG sampleMs = g_sampleMs[hid].load(std::memory_order_acquire);
            const std::uint32_t sampleSeq = g_sampleSeq[hid].load(std::memory_order_acquire);

            Log(L"digital event key=%hs hid=%02X state=%s seq=%u raw_now=%u sample_seq=%u",
                descriptor.name, hid, down ? L"down" : L"up", seq, rawAtEvent, sampleSeq);

            const bool alreadyPostEdgeSample =
                sampleSeq > sampleSeqAtEvent && sampleMs >= eventMs;
            const bool recentLeadingSample = sampleMs != 0 && eventMs >= sampleMs &&
                eventMs - sampleMs <= kDigitalLeadToleranceMs;
            const bool leadingValueMatches = recentLeadingSample &&
                AnalogMatchesDigital(hid, down, rawAtEvent, rawAtEvent);
            if (alreadyPostEdgeSample)
            {
                Log(L"digital/analog post-edge packet was already cached before worker correlation key=%hs latency_ms=%llu raw=%u",
                    descriptor.name,
                    static_cast<unsigned long long>(sampleMs - eventMs), rawAtEvent);
                digital.pending[hid] = false;
                digital.failureStreak[hid] = 0;
                const bool postEdgeValueMatches =
                    AnalogMatchesDigital(hid, down, rawAtEvent,
                        g_raw[hid].load(std::memory_order_acquire));
                const std::uint16_t postEdgeRaw =
                    g_raw[hid].load(std::memory_order_acquire);
                const unsigned postEdgeDelta = rawAtEvent > postEdgeRaw
                    ? rawAtEvent - postEdgeRaw : postEdgeRaw - rawAtEvent;
                MaybeConfirmSteadyStateFromPhysicalEdge(
                    eventMs, sampleMs,
                    postEdgeValueMatches || postEdgeDelta >= 8u);
            }
            else if (leadingValueMatches)
            {
                Log(L"digital/analog recent leading packet OK key=%hs analog_led_digital_by_ms=%llu raw=%u",
                    descriptor.name,
                    static_cast<unsigned long long>(eventMs - sampleMs), rawAtEvent);
                digital.pending[hid] = false;
                digital.failureStreak[hid] = 0;
            }
            else
            {
                if (recentLeadingSample)
                {
                    ++stats.semanticMismatches;
                    Log(L"digital/analog leading packet VALUE MISMATCH key=%hs analog_led_digital_by_ms=%llu raw=%u; "
                        L"relinquishing per-key ownership and waiting for a true post-edge A0",
                        descriptor.name,
                        static_cast<unsigned long long>(eventMs - sampleMs), rawAtEvent);
                }
                digital.pending[hid] = true;
                digital.expectedDown[hid] = down;
                digital.eventMs[hid] = eventMs;
                digital.sampleSeqAtEvent[hid] = sampleSeqAtEvent;
                digital.rawAtEvent[hid] = rawAtEvent;
                const ULONGLONG forcedUntil = g_forcedSweepGraceUntilMs.load(std::memory_order_acquire);
                digital.deadlineMs[hid] = eventMs < forcedUntil
                    ? kForcedSweepDigitalDeadlineMs
                    : kDigitalAnalogDeadlineMs;
                digital.liveStreamExtension[hid] = false;
            }
        }

        if (!digital.pending[hid]) continue;
        const std::uint32_t currentSampleSeq = g_sampleSeq[hid].load(std::memory_order_acquire);
        const std::uint16_t currentRaw = g_raw[hid].load(std::memory_order_acquire);
        const bool fresh = currentSampleSeq > digital.sampleSeqAtEvent[hid];
        const bool valueMatches = AnalogMatchesDigital(hid, digital.expectedDown[hid],
            digital.rawAtEvent[hid], currentRaw);
        if (fresh)
        {
            Log(valueMatches
                    ? L"digital/analog fresh packet OK key=%hs state=%s latency_ms=%llu raw=%u"
                    : L"digital/analog fresh packet VALUE MISMATCH key=%hs state=%s latency_ms=%llu raw=%u (transport healthy; no recovery)",
                descriptor.name, digital.expectedDown[hid] ? L"down" : L"up",
                static_cast<unsigned long long>(now - digital.eventMs[hid]), currentRaw);
            if (!valueMatches) ++stats.semanticMismatches;
            const unsigned edgeDelta = digital.rawAtEvent[hid] > currentRaw
                ? digital.rawAtEvent[hid] - currentRaw
                : currentRaw - digital.rawAtEvent[hid];
            MaybeConfirmSteadyStateFromPhysicalEdge(
                digital.eventMs[hid],
                g_sampleMs[hid].load(std::memory_order_acquire),
                valueMatches || edgeDelta >= 8u);
            digital.pending[hid] = false;
            digital.failureStreak[hid] = 0;
            continue;
        }

        const DWORD baseDeadline = digital.deadlineMs[hid] != 0
            ? digital.deadlineMs[hid]
            : kDigitalAnalogDeadlineMs;
        const ULONGLONG elapsed = now >= digital.eventMs[hid]
            ? now - digital.eventMs[hid]
            : 0;

        if (!digital.liveStreamExtension[hid] && elapsed >= baseDeadline)
        {
            const ULONGLONG lastA0 = g_lastA0Ms.load(std::memory_order_acquire);
            const bool globalStreamAlive = lastA0 != 0 && now >= lastA0 &&
                now - lastA0 <= kGlobalStreamAliveMs;
            if (globalStreamAlive)
            {
                digital.liveStreamExtension[hid] = true;
                Log(L"per-key A0 delayed while global stream is alive key=%hs hid=%02X elapsed_ms=%llu; "
                    L"firmware emits one changed slot per service pass, extending deadline to %lu ms",
                    descriptor.name, hid, static_cast<unsigned long long>(elapsed),
                    kSchedulerStarvationDeadlineMs);
            }
        }

        const DWORD effectiveDeadline = digital.liveStreamExtension[hid]
            ? std::max<DWORD>(baseDeadline, kSchedulerStarvationDeadlineMs)
            : baseDeadline;
        if (elapsed >= effectiveDeadline)
        {
            ++stats.digitalFailures;
            const std::uint8_t streak = static_cast<std::uint8_t>(
                std::min<unsigned>(255u, static_cast<unsigned>(digital.failureStreak[hid]) + 1u));
            digital.failureStreak[hid] = streak;
            // Onboard remaps make non-WASD digital/Hall correlation ambiguous.
            // Keep their failures in the log, but only W/A/S/D may restart A8/A9.
            const bool urgent = mad68pr::IsWasdHid(hid);
            const ULONGLONG lastA0 = g_lastA0Ms.load(std::memory_order_acquire);
            const ULONGLONG globalAge = lastA0 != 0 && now >= lastA0 ? now - lastA0 : ~0ull;
            Log(L"DIGITAL WITHOUT FRESH A0 key=%hs hid=%02X state=%s deadline_ms=%lu elapsed_ms=%llu "
                L"global_a0_age_ms=%llu scheduler_extension=%d sample_delta=%u raw_start=%u raw_now=%u "
                L"threshold=%u streak=%u action=%s",
                descriptor.name, hid, digital.expectedDown[hid] ? L"down" : L"up",
                effectiveDeadline, static_cast<unsigned long long>(elapsed),
                static_cast<unsigned long long>(globalAge),
                digital.liveStreamExtension[hid] ? 1 : 0,
                currentSampleSeq - digital.sampleSeqAtEvent[hid],
                digital.rawAtEvent[hid], currentRaw,
                g_threshold[hid].load(std::memory_order_relaxed), streak,
                urgent ? L"request-recovery-WASD" : L"degrade-to-emergency-WASD");
            digital.pending[hid] = false;
            digital.liveStreamExtension[hid] = false;
            if (urgent)
            {
                // Mad68ProR_OwnsHid() already relinquishes this exact HID usage
                // while its post-edge sample is missing. Keep the remaining MAD68
                // keys live. Re-arming the whole keyboard for a single starved key
                // would merely restart the slow forced sweeps and could make all
                // four WASD values worse.
                const bool globalStreamDead =
                    lastA0 == 0 || now < lastA0 || now - lastA0 > kSchedulerStarvationDeadlineMs;
                if (globalStreamDead)
                {
                    Log(L"W/A/S/D miss with dead global A0 transport key=%hs; requesting bounded A8/A9 recovery",
                        descriptor.name);
                    SetPublishMode(PublishMode::None,
                        L"global A0 transport is dead; relinquishing all MAD68 ownership before recovery");
                    g_streamConnected.store(false, std::memory_order_release);
                    g_uiState.store(static_cast<int>(UiState::Recovering), std::memory_order_release);
                    g_recoveryHid.store(hid, std::memory_order_release);
                    g_recoveryRequested.store(true, std::memory_order_release);
                    if (g_wakeEvent) SetEvent(g_wakeEvent);
                }
                else
                {
                    Log(L"W/A/S/D per-key scheduler starvation key=%hs while global A0 remains alive; "
                        L"keeping other native keys and using UAP/digital fallback only for this HID until its next A0",
                        descriptor.name);
                }
            }
            else if (publishMode == PublishMode::Full)
            {
                // Per-key freshness gating already removes this HID usage from
                // native ownership. Do not throw away the independently healthy
                // full matrix because one high scanner slot was delayed by the
                // firmware's first-changed-slot scheduling policy.
                Log(L"non-WASD per-key A0 starvation key=%hs; keeping full session and falling back only for this HID until a fresh packet arrives",
                    descriptor.name);
            }
        }
    }
}

void ResetSessionPublished() noexcept
{
    g_streamConnected.store(false, std::memory_order_release);
    g_publishMode.store(static_cast<int>(PublishMode::None), std::memory_order_release);
    g_lastA0Ms.store(0, std::memory_order_release);
    g_forcedSweepGraceUntilMs.store(0, std::memory_order_release);
    g_coverage.store(0, std::memory_order_release);
    g_steadyStateConfirmed.store(false, std::memory_order_release);
    g_orderedSweepCycles.store(0, std::memory_order_release);
    g_orderedSweepPosition.store(0, std::memory_order_release);
    g_activationEpochMs.store(0, std::memory_order_release);
    g_lastOrderedSweepMs.store(0, std::memory_order_release);
    for (auto& v : g_raw) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_milli) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_threshold) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_baseline) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_keyState) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_sampleMs) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_sampleSeq) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_changedHidChunks) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_changeSequenceByHid) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_a0ReceivedQpcByHid) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_snapshotPublishedQpcByHid) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_traceRawByHid) v.store(0, std::memory_order_relaxed);
    g_changeSequence.store(0, std::memory_order_relaxed);
    g_pendingChangedSamples.store(0, std::memory_order_relaxed);
    for (auto& v : g_descriptorRaw) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_descriptorSeq) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_sampleSeqAtDigitalEvent) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_rawAtDigitalEvent) v.store(0, std::memory_order_relaxed);
}

bool RunSession(const HidPath& path)
{
    ResetSessionPublished();
    g_devicePresent.store(true, std::memory_order_release);
    g_recoveryRequested.store(false, std::memory_order_release);
    g_strategyIndex.store(-1, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Passive), std::memory_order_release);
    g_firmwareVersion.store(path.attrs.VersionNumber, std::memory_order_release);

    const bool auditedFirmware = path.attrs.VersionNumber == mad68pr::kAuditedBcdDevice;
    const bool protocolValidated = IsRoutedNativePid(path.attrs.ProductID);
    g_productId.store(path.attrs.ProductID, std::memory_order_release);
    Log(L"session start build=%s path=%s vid=%04X pid=%04X version=%04X audited=%d protocol_validated=%d usage=%04X:%04X in=%u out=%u feature=%u manufacturer=%s product=%s serial=%s",
        kBuildName, path.path.c_str(), path.attrs.VendorID, path.attrs.ProductID,
        path.attrs.VersionNumber, auditedFirmware ? 1 : 0, protocolValidated ? 1 : 0, path.usagePage, path.usage,
        path.caps.InputReportByteLength, path.caps.OutputReportByteLength,
        path.caps.FeatureReportByteLength,
        path.manufacturer.c_str(), path.product.c_str(), path.serial.c_str());

    Session session(path);
    if (!session.Open())
    {
        Log(L"session cannot open readable vendor HID handle");
        return false;
    }

    DigitalWatch digital{};
    digital.seenResetSeq = g_digitalResetSeq.load(std::memory_order_acquire);
    int nextStrategy = 0;
    bool strategySucceeded = false;
    bool recoveryCycle = false;
    int recoveryCycles = 0;
    ULONGLONG recoveryWindowStartMs = 0;
    SnapshotBaseline activeActivationBaseline{};
    bool activeActivationBaselineValid = false;
    bool backgroundFullUpgradePending = false;
    ULONGLONG passiveDeadline = GetTickCount64() + kPassiveListenMs;
    ULONGLONG summaryAt = GetTickCount64() + kSummaryMs;
    std::uint64_t summaryReads = 0;
    std::uint64_t summaryA0 = 0;

    while (!g_stop.load(std::memory_order_acquire))
    {
        std::array<std::uint8_t, mad68pr::kPayloadBytes> packet{};
        if (session.ReadPayload(kReadSliceMs, packet))
            ProcessPayload(packet, session.Stats(), digital, false);
        ObserveDigitalEvents(digital, session.Stats());

        if (session.ConsecutiveReadErrors() >= 3)
        {
            Log(L"ending session after repeated HID read errors");
            break;
        }

        if (g_rescanRequested.exchange(false, std::memory_order_acq_rel))
        {
            if (!PathStillPresent(path.path))
            {
                Log(L"target vendor HID path disappeared; reconnecting");
                ClearTargetInputState(L"vendor path disappeared");
                break;
            }
            Log(L"WM_DEVICECHANGE did not remove current MAD68 vendor path; keeping healthy session");
        }

        const ULONGLONG now = GetTickCount64();
        if (!strategySucceeded && now >= passiveDeadline)
        {
            if (HasCompleteSnapshot())
            {
                Log(L"passive mode observed complete 68/68 descriptor state; steady-state remains unconfirmed until a post-sweep physical edge produces a fresh A0");
                strategySucceeded = true;
                backgroundFullUpgradePending = true;
                SetPublishMode(PublishMode::EmergencyWasd,
                    L"passive 68/68 snapshot available; emergency WASD only until post-sweep edge proof");
            }
            else if (!protocolValidated)
            {
                if (HasWasdSnapshot())
                    SetPublishMode(PublishMode::EmergencyWasd, L"unaudited firmware passive W/A/S/D only");
                else
                    g_uiState.store(static_cast<int>(UiState::UnsupportedFirmware), std::memory_order_release);
                Log(L"device %04X:%04X did not validate native A8/A9 framing; remaining passive coverage=%u/68 WASD=%u/4",
                    path.attrs.VendorID, path.attrs.ProductID, CurrentCoverage(), CurrentWasdCoverage());
                strategySucceeded = true;
            }
            else if (nextStrategy < static_cast<int>(kStrategies.size()))
            {
                g_uiState.store(static_cast<int>(UiState::Recovering), std::memory_order_release);
                SnapshotBaseline candidateBaseline{};
                strategySucceeded = RunStrategy(session, kStrategies[nextStrategy], nextStrategy, digital, &candidateBaseline);
                ++nextStrategy;
                if (strategySucceeded)
                {
                    activeActivationBaseline = candidateBaseline;
                    activeActivationBaselineValid = true;
                    backgroundFullUpgradePending =
                        CurrentPublishMode() == PublishMode::EmergencyWasd;
                }
                else
                {
                    activeActivationBaselineValid = false;
                    backgroundFullUpgradePending = false;
                    ResetSessionPublished();
                }
                passiveDeadline = GetTickCount64() + 300;
            }
            else
            {
                g_uiState.store(static_cast<int>(UiState::Exhausted), std::memory_order_release);
                if (HasWasdSnapshot())
                    SetPublishMode(PublishMode::EmergencyWasd, L"strategy matrix exhausted; retaining confirmed W/A/S/D");
                Log(L"all %u safe A8/A9 strategies exhausted; passive logging continues coverage=%u/68 WASD=%u/4 mode=%d",
                    static_cast<unsigned>(kStrategies.size()), CurrentCoverage(), CurrentWasdCoverage(),
                    static_cast<int>(CurrentPublishMode()));
                strategySucceeded = true;
            }
        }

        if (g_recoveryRequested.exchange(false, std::memory_order_acq_rel))
        {
            const ULONGLONG recoveryNow = GetTickCount64();
            if (recoveryWindowStartMs == 0 || recoveryNow - recoveryWindowStartMs >= kRecoveryWindowMs)
            {
                recoveryWindowStartMs = recoveryNow;
                recoveryCycles = 0;
            }
            if (!protocolValidated)
            {
                Log(L"recovery requested on an unvalidated native protocol pid=%04X version=%04X; mutation refused",
                    path.attrs.ProductID, path.attrs.VersionNumber);
            }
            else if (recoveryCycles >= kMaxRecoveryCyclesPerWindow)
            {
                Log(L"auto troubleshoot rate limit reached cycles=%d window_ms=%lu failed_hid=%02X; keeping current confirmed mode=%d and continuing passive diagnostics",
                    recoveryCycles, kRecoveryWindowMs, g_recoveryHid.load(std::memory_order_relaxed),
                    static_cast<int>(CurrentPublishMode()));
                g_uiState.store(static_cast<int>(UiState::Exhausted), std::memory_order_release);
            }
            else
            {
                ++recoveryCycles;
                Log(L"auto troubleshoot cycle=%d failed_hid=%02X; restarting finite strategy matrix from primary",
                    recoveryCycles, g_recoveryHid.load(std::memory_order_relaxed));
                ResetSessionPublished();
                activeActivationBaselineValid = false;
                backgroundFullUpgradePending = false;
                g_uiState.store(static_cast<int>(UiState::Recovering), std::memory_order_release);
                strategySucceeded = false;
                recoveryCycle = true;
                nextStrategy = 0;
                passiveDeadline = GetTickCount64();
            }
        }

        if (backgroundFullUpgradePending &&
            CurrentPublishMode() == PublishMode::EmergencyWasd &&
            activeActivationBaselineValid &&
            g_steadyStateConfirmed.load(std::memory_order_acquire) &&
            FreshCoverage(activeActivationBaseline) == mad68pr::kPhysicalKeyCount)
        {
            SetPublishMode(PublishMode::Full,
                L"steady-state was confirmed and background stream has fresh post-A8 68/68 coverage");
            backgroundFullUpgradePending = false;
            ++session.Stats().completeSnapshots;
        }

        if (recoveryCycle && strategySucceeded &&
            g_streamConnected.load(std::memory_order_acquire))
        {
            Log(L"auto troubleshoot recovered publish_mode=%d coverage=%u/68 WASD=%u/4 using strategy=%d",
                static_cast<int>(CurrentPublishMode()), CurrentCoverage(), CurrentWasdCoverage(),
                g_strategyIndex.load(std::memory_order_relaxed) + 1);
            recoveryCycle = false;
        }

        if (now >= summaryAt)
        {
            auto& s = session.Stats();
            const auto sampleAge = [now](std::uint16_t hid) -> unsigned long long {
                const ULONGLONG stamp = g_sampleMs[hid].load(std::memory_order_relaxed);
                return stamp != 0 && now >= stamp
                    ? static_cast<unsigned long long>(now - stamp)
                    : ~0ull;
            };
            const std::uint64_t a0Delta = s.a0Packets - summaryA0;
            const std::uint64_t a0RateX10 = (a0Delta * 10000ull) / kSummaryMs;
            Log(L"summary reads=%llu(+%llu) timeouts=%llu errors=%llu writes=%llu write_errors=%llu control=%llu checksum_errors=%llu A0=%llu(+%llu rate_x10=%llu gap50=%llu max_gap_ms=%llu) keys=%llu unknown_A0=%llu malformed=%llu digital_failures=%llu semantic_mismatches=%llu snapshots=%llu raw_edges=%llu coverage=%u/68 WASD=%u/4 steady=%d ordered_cycles=%u mode=%d state=%s strategy=%d W=%u/age%llu/own%d/down%d A=%u/age%llu/own%d/down%d S=%u/age%llu/own%d/down%d D=%u/age%llu/own%d/down%d",
                static_cast<unsigned long long>(s.reads),
                static_cast<unsigned long long>(s.reads - summaryReads),
                static_cast<unsigned long long>(s.readTimeouts),
                static_cast<unsigned long long>(s.readErrors),
                static_cast<unsigned long long>(s.writes),
                static_cast<unsigned long long>(s.writeErrors),
                static_cast<unsigned long long>(s.controlResponses),
                static_cast<unsigned long long>(s.checksumErrors),
                static_cast<unsigned long long>(s.a0Packets),
                static_cast<unsigned long long>(a0Delta),
                static_cast<unsigned long long>(a0RateX10),
                static_cast<unsigned long long>(s.a0GapOver50Ms),
                static_cast<unsigned long long>(s.maxA0GapMs),
                static_cast<unsigned long long>(s.keyPackets),
                static_cast<unsigned long long>(s.unknownA0),
                static_cast<unsigned long long>(s.malformed),
                static_cast<unsigned long long>(s.digitalFailures),
                static_cast<unsigned long long>(s.semanticMismatches),
                static_cast<unsigned long long>(s.completeSnapshots),
                static_cast<unsigned long long>(g_rawInputEdges.load(std::memory_order_relaxed)),
                CurrentCoverage(),
                CurrentWasdCoverage(),
                g_steadyStateConfirmed.load(std::memory_order_relaxed) ? 1 : 0,
                g_orderedSweepCycles.load(std::memory_order_relaxed),
                static_cast<int>(CurrentPublishMode()),
                UiStateName(static_cast<UiState>(g_uiState.load(std::memory_order_relaxed))),
                g_strategyIndex.load(std::memory_order_relaxed) + 1,
                g_raw[0x1A].load(std::memory_order_relaxed), sampleAge(0x1A),
                Mad68ProR_OwnsHid(0x1A) ? 1 : 0, g_digitalDown[0x1A].load(std::memory_order_relaxed) ? 1 : 0,
                g_raw[0x04].load(std::memory_order_relaxed), sampleAge(0x04),
                Mad68ProR_OwnsHid(0x04) ? 1 : 0, g_digitalDown[0x04].load(std::memory_order_relaxed) ? 1 : 0,
                g_raw[0x16].load(std::memory_order_relaxed), sampleAge(0x16),
                Mad68ProR_OwnsHid(0x16) ? 1 : 0, g_digitalDown[0x16].load(std::memory_order_relaxed) ? 1 : 0,
                g_raw[0x07].load(std::memory_order_relaxed), sampleAge(0x07),
                Mad68ProR_OwnsHid(0x07) ? 1 : 0, g_digitalDown[0x07].load(std::memory_order_relaxed) ? 1 : 0);
            summaryReads = s.reads;
            summaryA0 = s.a0Packets;
            summaryAt = now + kSummaryMs;
        }
    }

    if (protocolValidated)
    {
        DigitalWatch cleanupDigital{};
        const Strategy& cleanup = kStrategies[0];
        Log(L"session closing: final best-effort A9 using audited primary framing stop=%d",
            g_stop.load(std::memory_order_relaxed) ? 1 : 0);
        if (session.Send(cleanup, mad68pr::kRestoreInputOpcode))
            PumpFor(session, 150, cleanupDigital);
    }

    auto& s = session.Stats();
    Log(L"session end reads=%llu writes=%llu A0=%llu keys=%llu unknown=%llu coverage=%u/68 digital_failures=%llu",
        static_cast<unsigned long long>(s.reads),
        static_cast<unsigned long long>(s.writes),
        static_cast<unsigned long long>(s.a0Packets),
        static_cast<unsigned long long>(s.keyPackets),
        static_cast<unsigned long long>(s.unknownA0),
        CurrentCoverage(),
        static_cast<unsigned long long>(s.digitalFailures));
    return true;
}

std::uint32_t Mad68WorkerBody()
{
    Log(L"backend worker start build=%s protocol=VID373B routed by A9 ACK + IF1 65-byte fingerprint; A0[4..5]/1600 keys=68 published=67",
        kBuildName);
    while (!g_stop.load(std::memory_order_acquire))
    {
        g_uiState.store(static_cast<int>(UiState::Starting), std::memory_order_release);
        const auto candidates = EnumerateCandidates(true);
        if (candidates.empty())
        {
            ClearTargetInputState(L"no MAD68 vendor interface present");
            g_devicePresent.store(false, std::memory_order_release);
            g_streamConnected.store(false, std::memory_order_release);
            g_publishMode.store(static_cast<int>(PublishMode::None), std::memory_order_release);
            g_firmwareVersion.store(0, std::memory_order_release);
            g_productId.store(0, std::memory_order_release);
            g_uiState.store(static_cast<int>(UiState::NoDevice), std::memory_order_release);
            WaitForSingleObject(g_wakeEvent, kReconnectWaitMs);
            if (g_wakeEvent) ResetEvent(g_wakeEvent);
            continue;
        }

        g_devicePresent.store(true, std::memory_order_release);
        bool openedCandidate = false;
        for (const auto& candidate : candidates)
        {
            if (RunSession(candidate))
            {
                openedCandidate = true;
                break;
            }
            Log(L"candidate vendor interface could not be opened; trying next path=%s", candidate.path.c_str());
        }
        if (!openedCandidate)
            Log(L"all matching MAD68 vendor interfaces failed to open; retrying enumeration");
        ClearTargetInputState(L"session ended");
        ResetSessionPublished();
        if (!g_stop.load(std::memory_order_acquire))
        {
            WaitForSingleObject(g_wakeEvent, 250);
            if (g_wakeEvent) ResetEvent(g_wakeEvent);
        }
    }

    ClearTargetInputState(L"backend worker stopping");
    g_devicePresent.store(false, std::memory_order_release);
    g_streamConnected.store(false, std::memory_order_release);
    g_publishMode.store(static_cast<int>(PublishMode::None), std::memory_order_release);
    g_firmwareVersion.store(0, std::memory_order_release);
    g_productId.store(0, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Stopped), std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    Log(L"backend worker stop");
    return 0u;
}

void Mad68WorkerOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_workerFaultRecord = record;
    g_workerFaultKind.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    for (auto& down : g_physicalDown) down.store(false, std::memory_order_relaxed);
    for (auto& down : g_digitalDown) down.store(false, std::memory_order_relaxed);
    ResetSessionPublished();
    g_devicePresent.store(false, std::memory_order_release);
    g_firmwareVersion.store(0, std::memory_order_release);
    g_productId.store(0, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Stopped), std::memory_order_release);
    StabilityTrace_WriteCritical(L"ERROR", L"mad68", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
    OutputDebugStringA("[HallJoy] MAD68 native worker exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

void Mad68WorkerOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"mad68", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

void Mad68WorkerEntry() noexcept
{
    StabilityTrace_Write(L"INFO", L"mad68", L"worker.start");
    (void)halljoy::worker::RunWorkerEntryBarrier(
        [] { return Mad68WorkerBody(); },
        Mad68WorkerOnFault,
        Mad68WorkerOnCompletion,
        0xE0520001u);
}
} // namespace

bool Mad68ProR_PrepareProtocolRouting()
{
    const auto candidates = EnumerateBrandCandidates(false, false);
    std::vector<std::uint16_t> routed;
    std::vector<std::uint16_t> attempted;

    for (const auto& candidate : candidates)
    {
        const std::uint16_t pid = candidate.attrs.ProductID;
        if (std::find(attempted.begin(), attempted.end(), pid) != attempted.end())
            continue;
        attempted.push_back(pid);

        bool validated = false;
        if (pid == mad68pr::kPid &&
            candidate.attrs.VersionNumber == mad68pr::kAuditedBcdDevice)
        {
            validated = true;
        }
        else
        {
            const bool layoutCompatible = pid == mad68pr::kPid ||
                std::any_of(candidates.begin(), candidates.end(), [pid](const HidPath& path) {
                    return path.attrs.ProductID == pid && LooksLikeMad68Family(path);
                });
            if (!layoutCompatible)
                continue;

            for (const auto& path : candidates)
            {
                if (path.attrs.ProductID != pid) continue;
                if (ProbeNativeControlProtocol(path))
                {
                    validated = true;
                    break;
                }
            }
        }

        if (validated)
            routed.push_back(pid);
    }

    std::sort(routed.begin(), routed.end());
    routed.erase(std::unique(routed.begin(), routed.end()), routed.end());
    {
        std::lock_guard<std::mutex> lock(g_protocolRoutingMutex);
        g_routedNativePids = routed;
    }
    PublishRoutedPidEnvironment(routed);
    g_protocolRoutingPrepared.store(true, std::memory_order_release);
    return !routed.empty();
}

bool Mad68ProR_Start()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return true;

    // A faulted or otherwise completed std::thread remains joinable until the
    // owner reaps it. Reap that generation before assigning a new thread;
    // assigning over a joinable std::thread would call std::terminate.
    if (g_thread.joinable())
    {
        try { g_thread.join(); }
        catch (...)
        {
            g_running.store(false, std::memory_order_release);
            return false;
        }
    }
    if (g_wakeEvent)
    {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
    }

    g_workerFaultRecord = {};
    g_workerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_stop.store(false, std::memory_order_release);
    g_rescanRequested.store(false, std::memory_order_release);
    g_recoveryRequested.store(false, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Starting), std::memory_order_release);
    g_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wakeEvent)
    {
        const DWORD error = GetLastError();
        g_devicePresent.store(false, std::memory_order_release);
        g_streamConnected.store(false, std::memory_order_release);
        g_publishMode.store(static_cast<int>(PublishMode::None), std::memory_order_release);
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"mad68", L"start.failed", L"stage=create_event win32=%lu", error);
        return false;
    }

    g_devicePresent.store(ProbePresence(), std::memory_order_release);
    Log(L"start requested build=%s synchronous_presence=%d", kBuildName,
        g_devicePresent.load(std::memory_order_relaxed) ? 1 : 0);
    DebugLog_Write(L"[mad68pr] full 68-key backend start presence=%d log=HallJoyMAD68ProR.log",
        g_devicePresent.load(std::memory_order_relaxed) ? 1 : 0);
    try
    {
        g_thread = std::thread(Mad68WorkerEntry);
    }
    catch (...)
    {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = nullptr;
        g_devicePresent.store(false, std::memory_order_release);
        g_streamConnected.store(false, std::memory_order_release);
        g_publishMode.store(static_cast<int>(PublishMode::None), std::memory_order_release);
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"mad68", L"start.failed", L"stage=create_thread");
        return false;
    }
    StabilityTrace_Write(L"INFO", L"mad68", L"start.ok", L"presence=%d",
        g_devicePresent.load(std::memory_order_relaxed) ? 1 : 0);
    return true;
}

void Mad68ProR_Stop()
{
    if (!g_running.load(std::memory_order_acquire) && !g_thread.joinable()) return;
    StabilityTrace_Write(L"INFO", L"mad68", L"stop.begin", L"joinable=%d", g_thread.joinable() ? 1 : 0);
    g_stop.store(true, std::memory_order_release);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
    if (g_thread.joinable()) g_thread.join();
    if (g_wakeEvent) CloseHandle(g_wakeEvent);
    g_wakeEvent = nullptr;
    g_running.store(false, std::memory_order_release);
    g_uiState.store(static_cast<int>(UiState::Stopped), std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"mad68", L"neutralized", L"reason=stop");
    StabilityTrace_Write(L"INFO", L"mad68", L"stop.end");
}

void Mad68ProR_NotifyDeviceChange()
{
    g_rescanRequested.store(true, std::memory_order_release);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

void Mad68ProR_NotifyKeyboardDeviceReset()
{
    ClearTargetInputState(L"target Raw Input keyboard changed/removed");
    // A device reset is not a physical key edge. Signal the worker separately so
    // it drops pending correlations without manufacturing 67 release events.
    g_digitalResetSeq.fetch_add(1u, std::memory_order_acq_rel);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

void Mad68ProR_NotifyKeyboardEvent(std::uint16_t hidUsage, bool isKeyDown, bool isInjected)
{
    if (isInjected || hidUsage == 0 || hidUsage >= 256) return;

    // Capture the last fully observed analogue generation at Raw Input callback
    // entry. Any A0 completing after this point is eligible as a post-edge
    // sample. Publishing the digital sequence remains the final release step.
    std::uint32_t sampleSeqAtEdge = 0;
    std::uint16_t rawAtEdge = 0;
    CaptureAnalogSnapshot(hidUsage, sampleSeqAtEdge, rawAtEdge);
    const ULONGLONG eventMs = GetTickCount64();

    const bool previous = g_physicalDown[hidUsage].exchange(isKeyDown, std::memory_order_acq_rel);
    if (previous == isKeyDown) return; // suppress Raw Input autorepeat / duplicate break events
    if (!mad68pr::IsPublishedHid(hidUsage)) return;
    g_rawInputEdges.fetch_add(1u, std::memory_order_relaxed);
    g_digitalDown[hidUsage].store(isKeyDown, std::memory_order_relaxed);
    g_digitalMs[hidUsage].store(eventMs, std::memory_order_relaxed);
    g_sampleSeqAtDigitalEvent[hidUsage].store(sampleSeqAtEdge, std::memory_order_relaxed);
    g_rawAtDigitalEvent[hidUsage].store(rawAtEdge, std::memory_order_relaxed);
    // Release-publish the digital sequence only after all edge snapshot fields.
    g_digitalSeq[hidUsage].fetch_add(1u, std::memory_order_release);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

bool Mad68ProR_EmergencyRestoreInputOnce()
{
    const auto candidates = EnumerateCandidates(false);
    const Strategy& primary = kStrategies[0];
    for (const auto& candidate : candidates)
    {
        Session session(candidate);
        if (!session.Open())
        {
            Log(L"watchdog emergency A9: routed native candidate could not be opened path=%s",
                candidate.path.c_str());
            continue;
        }

        DigitalWatch digital{};
        const bool sent = session.Send(primary, mad68pr::kRestoreInputOpcode);
        bool acked = false;
        if (sent)
            acked = WaitForAck(session, primary, mad68pr::kRestoreInputOpcode,
                digital, kCommandAckMs);
        Log(L"watchdog emergency A9 result sent=%d acked=%d version=%04X path=%s",
            sent ? 1 : 0, acked ? 1 : 0, candidate.attrs.VersionNumber,
            candidate.path.c_str());

        // A9 is idempotent. A completed primary WriteFile is the safety action;
        // the AA/A9 response can itself be lost when recovering after a crash.
        return sent;
    }

    Log(L"watchdog emergency A9 skipped: no routed MADLIONS native interface present");
    return false;
}

bool Mad68ProR_IsRunning()
{
    return g_running.load(std::memory_order_acquire);
}

bool Mad68ProR_IsDevicePresent()
{
    return g_devicePresent.load(std::memory_order_acquire);
}

bool Mad68ProR_IsAuditedDevicePresent()
{
    const std::uint16_t activeVersion = g_firmwareVersion.load(std::memory_order_acquire);
    if (g_devicePresent.load(std::memory_order_acquire) &&
        activeVersion == mad68pr::kAuditedBcdDevice)
        return true;
    return ProbeAuditedPresence();
}

bool Mad68ProR_IsProtocolDevicePresent()
{
    if (!g_protocolRoutingPrepared.load(std::memory_order_acquire))
        Mad68ProR_PrepareProtocolRouting();
    return ProbePresence();
}

bool Mad68ProR_IsRoutedProduct(std::uint16_t productId)
{
    return IsRoutedNativePid(productId);
}

std::uint16_t Mad68ProR_GetProductId()
{
    return g_productId.load(std::memory_order_acquire);
}

bool Mad68ProR_IsConnected()
{
    return g_devicePresent.load(std::memory_order_acquire) &&
        CurrentPublishMode() != PublishMode::None;
}

bool Mad68ProR_IsFullConnected()
{
    return g_devicePresent.load(std::memory_order_acquire) &&
        CurrentPublishMode() == PublishMode::Full;
}

bool Mad68ProR_IsEmergencyWasd()
{
    return g_devicePresent.load(std::memory_order_acquire) &&
        CurrentPublishMode() == PublishMode::EmergencyWasd;
}

std::uint32_t Mad68ProR_GetCoverage()
{
    return CurrentCoverage();
}

std::uint16_t Mad68ProR_GetFirmwareVersion()
{
    return g_firmwareVersion.load(std::memory_order_acquire);
}

bool Mad68ProR_OwnsHid(std::uint16_t hidUsage)
{
    if (!g_devicePresent.load(std::memory_order_acquire) || hidUsage == 0 || hidUsage >= 256 ||
        !ModeOwnsHid(CurrentPublishMode(), hidUsage))
        return false;

    // Never keep a stale analogue value authoritative across a new physical
    // keyboard edge. Until an A0 sample is correlated with the latest Raw Input
    // press/release, relinquish this HID usage to the ordinary UAP/native/digital
    // arbitration path. This prevents a stale press from holding a ViGEm axis and
    // gives immediate fallback during firmware scheduler starvation.
    const std::uint32_t sampleSeq = g_sampleSeq[hidUsage].load(std::memory_order_acquire);
    if (sampleSeq == 0) return false;
    if (g_digitalSeq[hidUsage].load(std::memory_order_acquire) == 0) return true;

    // Sequence comparison is authoritative. Timestamps are only a tolerance for
    // a report that slightly led the Raw Input callback but already represents
    // the new physical state.
    const std::uint32_t sampleSeqAtEdge =
        g_sampleSeqAtDigitalEvent[hidUsage].load(std::memory_order_acquire);
    if (sampleSeq > sampleSeqAtEdge) return true;

    const ULONGLONG sampleMs = g_sampleMs[hidUsage].load(std::memory_order_acquire);
    const ULONGLONG digitalMs = g_digitalMs[hidUsage].load(std::memory_order_acquire);
    if (sampleMs > digitalMs) return false; // newer timestamp without a newer generation is inconsistent
    if (digitalMs - sampleMs > kDigitalLeadToleranceMs) return false;

    // A slightly leading analogue packet is valid only when its value already
    // agrees with the new digital state. Otherwise it belongs to the previous
    // state and must not suppress the normal immediate fallback path.
    const bool down = g_digitalDown[hidUsage].load(std::memory_order_acquire);
    const std::uint16_t raw = g_raw[hidUsage].load(std::memory_order_acquire);
    return AnalogMatchesDigital(hidUsage, down, raw, raw);
}

std::uint16_t Mad68ProR_GetMilli(std::uint16_t hidUsage)
{
    if (!Mad68ProR_OwnsHid(hidUsage) || hidUsage >= 256) return 0;
    return g_milli[hidUsage].load(std::memory_order_acquire);
}

std::uint16_t Mad68ProR_GetRaw(std::uint16_t hidUsage)
{
    if (!Mad68ProR_OwnsHid(hidUsage) || hidUsage >= 256) return 0;
    return g_raw[hidUsage].load(std::memory_order_acquire);
}

bool Mad68ProR_ConsumeChangeBatch(Mad68ProRChangeBatch* out)
{
    if (!out) return false;
    *out = {};

    bool any = false;
    for (std::size_t chunk = 0; chunk < out->dirtyHids.size(); ++chunk)
    {
        out->dirtyHids[chunk] =
            g_changedHidChunks[chunk].exchange(0, std::memory_order_acq_rel);
        any = any || out->dirtyHids[chunk] != 0;
    }
    out->sampleCount = g_pendingChangedSamples.exchange(0, std::memory_order_acq_rel);
    if (!any)
        return false;

    std::uint64_t latestSequence = 0;
    LONGLONG earliestReceived = 0;
    for (std::size_t chunk = 0; chunk < out->dirtyHids.size(); ++chunk)
    {
        std::uint64_t bits = out->dirtyHids[chunk];
        for (unsigned bit = 0; bit < 64; ++bit)
        {
            if ((bits & (std::uint64_t{1} << bit)) == 0)
                continue;
            const std::uint16_t hid = static_cast<std::uint16_t>(chunk * 64u + bit);
            const std::uint64_t sequence =
                g_changeSequenceByHid[hid].load(std::memory_order_acquire);
            const LONGLONG received =
                g_a0ReceivedQpcByHid[hid].load(std::memory_order_acquire);
            if (received > 0 && (earliestReceived == 0 || received < earliestReceived))
                earliestReceived = received;
            if (sequence >= latestSequence)
            {
                latestSequence = sequence;
                out->latestHid = hid;
                out->latestRaw = g_traceRawByHid[hid].load(std::memory_order_relaxed);
                out->latestA0ReceivedQpc = received;
                out->latestSnapshotPublishedQpc =
                    g_snapshotPublishedQpcByHid[hid].load(std::memory_order_acquire);
            }
        }
    }
    out->latestSequence = latestSequence;
    out->earliestA0ReceivedQpc = earliestReceived;
    return true;
}

void Mad68ProR_GetStatusText(wchar_t* buffer, std::size_t chars)
{
    if (!buffer || chars == 0) return;
    const UiState state = static_cast<UiState>(g_uiState.load(std::memory_order_acquire));
    const int strategy = g_strategyIndex.load(std::memory_order_acquire);
    const std::uint32_t coverage = g_coverage.load(std::memory_order_acquire);
    const std::uint16_t version = g_firmwareVersion.load(std::memory_order_acquire);
    const bool steady = g_steadyStateConfirmed.load(std::memory_order_acquire);
    const std::uint32_t cycles = g_orderedSweepCycles.load(std::memory_order_acquire);

    const auto valueText = [](std::uint16_t hid, wchar_t* out, std::size_t outChars) {
        if (hid >= 256 || g_sampleSeq[hid].load(std::memory_order_acquire) == 0)
            wcscpy_s(out, outChars, L"----");
        else
            _snwprintf_s(out, outChars, _TRUNCATE, L"%04u", g_raw[hid].load(std::memory_order_acquire));
    };

    wchar_t w[8]{}, a[8]{}, s[8]{}, d[8]{};
    valueText(0x1A, w, _countof(w));
    valueText(0x04, a, _countof(a));
    valueText(0x16, s, _countof(s));
    valueText(0x07, d, _countof(d));

    if (strategy >= 0)
    {
        _snwprintf_s(buffer, chars, _TRUNCATE,
            L"HallJoy — MAD68 Pro R | %s | fw:%04X | keys:%u/68 | steady:%s cycles:%u | try:%d/%u | W:%s A:%s S:%s D:%s",
            UiStateName(state), version, coverage, steady ? L"yes" : L"no", cycles, strategy + 1,
            static_cast<unsigned>(kStrategies.size()), w, a, s, d);
    }
    else
    {
        _snwprintf_s(buffer, chars, _TRUNCATE,
            L"HallJoy — MAD68 Pro R | %s | fw:%04X | keys:%u/68 | steady:%s cycles:%u | W:%s A:%s S:%s D:%s",
            UiStateName(state), version, coverage, steady ? L"yes" : L"no", cycles, w, a, s, d);
    }
}

namespace
{
void Mad68ProR_FillGenericTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = Mad68ProR_IsDevicePresent();
    out->connected = Mad68ProR_IsConnected();
    out->vendorId = 0x373B;
    out->productId = Mad68ProR_GetProductId();
    out->usagePage = 0x0001;
    out->usage = 0x0000;
    out->mappedKeys = Mad68ProR_GetCoverage();
    out->activeKeys = Mad68ProR_IsFullConnected() ? 67u : (Mad68ProR_IsEmergencyWasd() ? 4u : 0u);
    out->nominalRawLevels = 1601u;
    out->inputReportBytes = 65u;
    out->outputReportBytes = 65u;
    Mad68ProR_GetStatusText(out->status, kNativeAnalogBackendStatusChars);
}
}

const NativeAnalogBackendDescriptor& Mad68ProR_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "mad68-a0",
        L"MADLIONS MAD68 native A0",
        NativeAnalogProtocol::Mad68A0,
        NativeAnalogStartPhase::AfterRawInput,
        NativeAnalogBackendFlag_StreamTransport |
            NativeAnalogBackendFlag_ReversibleControlProbe |
            NativeAnalogBackendFlag_DynamicVidPid |
            NativeAnalogBackendFlag_RequiresRawInput,
        &Mad68ProR_PrepareProtocolRouting,
        &Mad68ProR_Start,
        [](halljoy::lifecycle::GenerationId generation) {
            Mad68ProR_Stop();
            return NativeAnalogBackendStopJoined(generation);
        },
        &Mad68ProR_NotifyDeviceChange,
        &Mad68ProR_IsProtocolDevicePresent,
        &Mad68ProR_IsConnected,
        &Mad68ProR_OwnsHid,
        &Mad68ProR_GetMilli,
        &Mad68ProR_FillGenericTelemetry,
    };
    return descriptor;
}
