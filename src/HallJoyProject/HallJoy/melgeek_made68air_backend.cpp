#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "melgeek_made68air_backend.h"
#include "melgeek_made68air_protocol.h"
#include "hid_io_operation.h"
#include "input_trace.h"
#include "native_analog_routing.h"
#include "physical_analog_state.h"
#include "support_log.h"
#include "generated/layout_pipeline/identities.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

// MelGeek MADE68 Air (0416:0114). The keyboard pushes the position of its keys on the vendor HID
// collection (FF60:61) while the report mode (`set_report`) is on: mode 2 is the mode the vendor configurator
// uses on its Calibrate/Detect tabs (a full scan every 50 ms; no calibration is involved), mode 1 is the
// candidate faster mode. The backend
//   1. identifies the keyboard: exact VID:PID, the vendor collection, and a valid `get_ver` reply,
//   2. reads `get_report`; if it is 0 writes mode 1 (rev 6) and verifies it, then reports connected at once
//      (no measuring phase, no waiting),
//   3. decodes the stream (frames 08 09, entries [key id][position 0..46][ADC]) into 0..1000 depth,
//   4. maps key ids to HID usages with the verified 68-key table and checks it against Raw Input key-downs of this
//      keyboard (key_confirmed / key_conflict),
//   5. writes one `press` line per key press (samples, rise, gaps, lag behind Windows) from the first press on, and
//      falls back to mode 2 only on evidence from key presses (missed presses, stuck releases; mg::ModeHealth),
//   6. restores mode 0 when the session ends (HallJoy closes, the device list changes, device removed).
// Only the six getters and set_report 0/1/2 can be sent (melgeek_made68air_protocol.h).
// docs/current/MELGEEK_MADE68_AIR_2026-10-12.md.
namespace {
namespace mg = halljoy::melgeek_made68air;

constexpr unsigned kRevision = 6;
constexpr std::uint64_t kReplyTimeoutMs = 600;
constexpr unsigned kDistinctLimit = 200;     // distinct vendor reports written to the log
constexpr unsigned kClassLimit = 64;         // additional first-seen report classes
constexpr std::size_t kSeenLimit = 20000;    // memory bound for the distinct-report set
constexpr std::size_t kCapLinesPerCollection = 64;
constexpr std::uint64_t kRetentionMs = 1000; // publication freshness; the heartbeat refreshes held keys
constexpr std::uint64_t kHeartbeatMs = 100;
constexpr unsigned kPressLineLimit = 160;    // per-press lines; later presses only enter the summaries
constexpr unsigned kMissLineLimit = 64;

struct Handle {
    HANDLE v = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : v(h) {}
    ~Handle() { if (v && v != INVALID_HANDLE_VALUE) CloseHandle(v); }
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return v && v != INVALID_HANDLE_VALUE; }
};

std::mutex g_service;
HANDLE g_thread = nullptr, g_wake = nullptr;
std::atomic<bool> g_stop{false}, g_running{false}, g_present{false}, g_connected{false}, g_blocked{false};
std::atomic<bool> g_modeOn{false};   // report mode written (or adopted) and not yet restored
std::atomic<std::uint64_t> g_reports{0}, g_bad{0}, g_last{0};
std::atomic<unsigned> g_failedSessions{0};
std::atomic<std::uint32_t> g_vendorPage{0}, g_vendorUsage{0}, g_vendorIn{0}, g_vendorOut{0};
std::atomic<std::uint32_t> g_learnedKeys{0}, g_avgGapUs{0}, g_maxGapUs{0}, g_hz10{0};
halljoy::physical_analog::Publication g_depth;

// Raw Input key-downs and key-ups of this keyboard (UI thread -> worker), as in the Logitech backend.
// `time` is QueryPerformanceCounter milliseconds (NowMs), the same clock as the stream samples.
struct OsKey { std::uint16_t hid; bool down; std::uint64_t time; };
std::array<OsKey, 128> g_osKeys{};
std::atomic<unsigned> g_osHead{0}, g_osTail{0};

void Evidence(bool keep, const char* format, ...) noexcept {
    char line[352];
    const int at = _snprintf_s(line, sizeof(line), _TRUNCATE, "evidence.melgeek ");
    if (at < 0) return;
    va_list args;
    va_start(args, format);
    _vsnprintf_s(line + at, sizeof(line) - static_cast<std::size_t>(at), _TRUNCATE, format, args);
    va_end(args);
    SupportLog_Evidence(line, keep);
}

std::string Hex(const std::uint8_t* data, std::size_t size, std::size_t cap = 96) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    const std::size_t n = std::min(size, cap);
    text.reserve(n * 2 + 1);
    for (std::size_t i = 0; i < n; ++i) { text += digits[data[i] >> 4]; text += digits[data[i] & 15]; }
    if (size > cap) text += '+';
    return text.empty() ? std::string("-") : text;
}

unsigned PathToken(const std::wstring& lowerPath, const wchar_t* tag) {
    const auto at = lowerPath.find(tag);
    if (at == std::wstring::npos) return 0xff;
    return static_cast<unsigned>(wcstoul(lowerPath.c_str() + at + wcslen(tag), nullptr, 16) & 0xff);
}

struct CapSet {
    std::vector<HIDP_VALUE_CAPS> value[3];    // input, output, feature
    std::vector<HIDP_BUTTON_CAPS> button[3];
};

// One HID collection of the keyboard (metadata only until it is opened).
struct Found {
    std::wstring path;
    unsigned mi = 0xff, col = 0xff;
    HIDP_CAPS caps{};
    CapSet capSet;
    mg::Role role = mg::Role::Other;
};

constexpr const char* kDirection[3] = {"in", "out", "feat"};

std::vector<Found> Enumerate() {
    if (!mg::kEnabled) return {};
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    struct FreeSet { HDEVINFO s; ~FreeSet() { SetupDiDestroyDeviceInfoList(s); } } freeSet{set};
    std::vector<Found> out;
    for (DWORD i = 0; !g_stop.load(); ++i) {
        SP_DEVICE_INTERFACE_DATA d{}; d.cbSize = sizeof(d);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &d)) break;
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &d, nullptr, 0, &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || needed > 65536) continue;
        std::vector<unsigned char> bytes(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &d, detail, needed, nullptr, nullptr)) continue;
        Handle meta(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr));
        if (!meta) continue;
        HIDD_ATTRIBUTES a{}; a.Size = sizeof(a);
        if (!HidD_GetAttributes(meta.v, &a) || a.VendorID != mg::kVendorId ||
            a.ProductID != mg::kProductId) continue;
        PHIDP_PREPARSED_DATA data = nullptr;
        if (!HidD_GetPreparsedData(meta.v, &data)) continue;
        Found f;
        f.path = detail->DevicePath;
        bool ok = HidP_GetCaps(data, &f.caps) == HIDP_STATUS_SUCCESS;
        if (ok) {
            const USHORT valueCount[3] = {f.caps.NumberInputValueCaps, f.caps.NumberOutputValueCaps,
                                          f.caps.NumberFeatureValueCaps};
            const USHORT buttonCount[3] = {f.caps.NumberInputButtonCaps, f.caps.NumberOutputButtonCaps,
                                           f.caps.NumberFeatureButtonCaps};
            const HIDP_REPORT_TYPE type[3] = {HidP_Input, HidP_Output, HidP_Feature};
            for (int t = 0; t < 3; ++t) {
                if (valueCount[t]) {
                    USHORT n = valueCount[t];
                    f.capSet.value[t].resize(n);
                    if (HidP_GetValueCaps(type[t], f.capSet.value[t].data(), &n, data) != HIDP_STATUS_SUCCESS) n = 0;
                    f.capSet.value[t].resize(n);
                }
                if (buttonCount[t]) {
                    USHORT n = buttonCount[t];
                    f.capSet.button[t].resize(n);
                    if (HidP_GetButtonCaps(type[t], f.capSet.button[t].data(), &n, data) != HIDP_STATUS_SUCCESS) n = 0;
                    f.capSet.button[t].resize(n);
                }
            }
        }
        HidD_FreePreparsedData(data);
        if (!ok) continue;
        std::wstring lower = f.path;
        for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
        f.mi = PathToken(lower, L"mi_");
        f.col = PathToken(lower, L"&col");
        f.role = mg::Classify(f.caps.UsagePage, f.caps.Usage);
        out.push_back(std::move(f));
    }
    std::sort(out.begin(), out.end(), [](const Found& a, const Found& b) { return a.path < b.path; });
    return out;
}

// Interface and collection numbers only; the device path itself is never logged.
void LogCollection(unsigned index, const Found& f) {
    const auto& c = f.caps;
    Evidence(true, "caps idx=%u mi=%02x col=%02x up=%04x u=%04x role=%s in=%u out=%u feat=%u "
                   "inbtn=%u inval=%u outbtn=%u outval=%u featbtn=%u featval=%u links=%u",
             index, f.mi, f.col, c.UsagePage, c.Usage, mg::RoleName(f.role),
             c.InputReportByteLength, c.OutputReportByteLength, c.FeatureReportByteLength,
             c.NumberInputButtonCaps, c.NumberInputValueCaps, c.NumberOutputButtonCaps,
             c.NumberOutputValueCaps, c.NumberFeatureButtonCaps, c.NumberFeatureValueCaps,
             c.NumberLinkCollectionNodes);
    std::size_t lines = 0;
    for (int t = 0; t < 3; ++t) {
        for (const auto& v : f.capSet.value[t]) {
            if (lines++ >= kCapLinesPerCollection) return;
            Evidence(true, "cap idx=%u dir=%s kind=val rid=%u up=%04x usage=%04x-%04x bits=%u count=%u "
                           "lmin=%ld lmax=%ld abs=%u",
                     index, kDirection[t], v.ReportID, v.UsagePage,
                     v.IsRange ? v.Range.UsageMin : v.NotRange.Usage,
                     v.IsRange ? v.Range.UsageMax : v.NotRange.Usage,
                     v.BitSize, v.ReportCount, v.LogicalMin, v.LogicalMax, v.IsAbsolute ? 1u : 0u);
        }
        for (const auto& b : f.capSet.button[t]) {
            if (lines++ >= kCapLinesPerCollection) return;
            Evidence(true, "cap idx=%u dir=%s kind=btn rid=%u up=%04x usage=%04x-%04x",
                     index, kDirection[t], b.ReportID, b.UsagePage,
                     b.IsRange ? b.Range.UsageMin : b.NotRange.Usage,
                     b.IsRange ? b.Range.UsageMax : b.NotRange.Usage);
        }
    }
}

// One request of the vendor exchange (a getter or the report-mode setter).
struct Request {
    std::uint8_t module = 0, op = 0, mode = 0;
    bool set = false;
    const char* name = "";
};
enum class Phase : std::uint8_t {
    Getters, EnableSet, EnableVerify, Streaming, Watchdog, RestoreSet, RestoreVerify, Done
};

// A finished press waiting for a late Windows key-down (at most KeyLearner::kLearnAfterMs) before its line is written.
struct FinishedPress {
    std::uint8_t id = 0;
    std::uint8_t mode = 0;
    bool os = false;
    std::int64_t lagMs = 0;
    std::uint64_t endMs = 0;
    mg::PressRecord rec;
};

// Per report mode: what the press lines add up to (written as press_summary).
struct ModeSummary {
    unsigned presses = 0, withOs = 0, misses = 0, stuck = 0;
    std::uint64_t samples = 0, riseSamples = 0, gaps = 0;
    std::int64_t gapSumUs = 0, riseSumUs = 0, lagSumMs = 0;
};

// The opened vendor collection: overlapped reader, report statistics and exchange state.
struct Col {
    unsigned index = 0;
    Found info;
    std::uint8_t outReportId = 0;
    Handle handle;
    std::unique_ptr<HidIoOperation> read, write;
    std::vector<std::uint8_t> buffer;
    bool reading = false, live = false;

    mg::ReportStats stats;
    std::uint64_t nextFlush = 1;
    std::unordered_set<std::uint64_t> seen;
    std::unordered_set<std::uint32_t> classes;
    unsigned logged = 0;
    unsigned classesLogged = 0;

    Phase phase = Phase::Done;
    Request current;
    bool awaiting = false, crcVariant = true, retried = false;
    std::size_t step = 0;
    unsigned txSeq = 0, replies = 0;
    ULONGLONG sentAt = 0;
    int reportMode = -1;               // last value read with get_report
    bool modeOn = false;               // set_report 2 sent (or found on) and not yet restored
    int setCrc = -1;                   // CRC variant whose set_report was acknowledged (-1 unknown)
    ULONGLONG lastFrameAt = 0, lastWatchdogAt = 0;
    const char* fallbackReason = nullptr;   // set when mode 1 failed; acted on when no request is pending

    // Press records (QueryPerformanceCounter clock, NowMs/NowUs).
    mg::PressTracker presses;
    std::array<bool, mg::kKeyCount> pressOs{};          // the current press was matched to a Windows key-down
    std::array<std::int64_t, mg::kKeyCount> pressLag{}; // its lag (stream press start minus key-down), ms
    std::array<std::uint64_t, mg::kKeyCount> sampleAtMs{}, upAtMs{};
    std::array<bool, mg::kKeyCount> upPending{};        // Windows key-up seen, no stream sample of the key since
    std::vector<FinishedPress> finished;
    mg::ModeHealth health;                              // of the preferred mode only
    ModeSummary summary[3];                             // by report mode 0..2
    unsigned pressLines = 0, missLines = 0, nextSummary = 1;

    // Stream state.
    std::array<std::uint8_t, mg::kKeyCount> position{};
    std::array<LONGLONG, mg::kKeyCount> keyLastUs{};
    LONGLONG lastFrameUs = 0;
    std::uint64_t gapBucket[4]{};      // inter-frame gaps: <2 ms, <10 ms, <30 ms, >=30 ms
    std::uint64_t keyGapBucket[4]{};   // gaps between updates of one pressed key: <20, <40, <80, >=80 ms
    double avgGapUs = 0;
    std::uint64_t streamFrames = 0;
};

mg::KeyLearner g_learner;   // worker thread only; survives reconnects (the physical layout is fixed)

// Report-mode choice for this run (worker thread only): mode 1 until key presses show it does not serve them;
// then mode 2 for the rest of the run (the next HallJoy start tries mode 1 again).
std::uint8_t g_chosenMode = mg::kPreferredMode;
std::atomic<unsigned> g_statusMode{mg::kPreferredMode};   // for the status text (UI thread)

LONGLONG NowUs() {
    static const LONGLONG freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return static_cast<LONGLONG>(static_cast<long double>(t.QuadPart) * 1000000.0L / static_cast<long double>(freq));
}
std::uint64_t NowMs() { return static_cast<std::uint64_t>(NowUs() / 1000); }

void FlushStats(Col& c, bool final) {
    if (!c.stats.frames) return;
    Evidence(true, "stats col=%u frames=%llu len=%zu varying=%zu stream=%llu known=%zu learned=%zu final=%u", c.index,
             static_cast<unsigned long long>(c.stats.frames), c.stats.length, c.stats.VaryingCount(),
             static_cast<unsigned long long>(c.streamFrames), g_learner.KnownCount(), g_learner.LearnedCount(), final ? 1u : 0u);
    if (c.streamFrames) {
        Evidence(true, "rate col=%u avg_gap_us=%.0f gap_lt2ms=%llu gap_lt10ms=%llu gap_lt30ms=%llu gap_ge30ms=%llu "
                       "key_gap_lt20ms=%llu key_gap_lt40ms=%llu key_gap_lt80ms=%llu key_gap_ge80ms=%llu",
                 c.index, c.avgGapUs, static_cast<unsigned long long>(c.gapBucket[0]),
                 static_cast<unsigned long long>(c.gapBucket[1]), static_cast<unsigned long long>(c.gapBucket[2]),
                 static_cast<unsigned long long>(c.gapBucket[3]), static_cast<unsigned long long>(c.keyGapBucket[0]),
                 static_cast<unsigned long long>(c.keyGapBucket[1]), static_cast<unsigned long long>(c.keyGapBucket[2]),
                 static_cast<unsigned long long>(c.keyGapBucket[3]));
    }
    // Byte 0 is the report id as delivered by Windows; entries are index:min-max:changes.
    char text[256]{};
    std::size_t at = 0;
    unsigned inLine = 0;
    const auto flushLine = [&] {
        if (!at) return;
        Evidence(true, "statbytes col=%u %s", c.index, text);
        at = 0; inLine = 0; text[0] = 0;
    };
    for (std::size_t i = 0; i < c.stats.length; ++i) {
        if (!c.stats.Varies(i)) continue;
        const int w = _snprintf_s(text + at, sizeof(text) - at, _TRUNCATE, "%s%zu:%02x-%02x:%u",
                                  at ? " " : "", i, c.stats.Min(i), c.stats.Max(i), c.stats.Changes(i));
        if (w < 0) { flushLine(); continue; }
        at += static_cast<std::size_t>(w);
        if (++inLine >= 10) flushLine();
    }
    flushLine();
}

void StartRead(Col& c) {
    if (!c.live || c.reading || c.buffer.empty()) return;
    DWORD error = 0;
    const auto start = c.read->StartRead(c.buffer.data(), static_cast<DWORD>(c.buffer.size()), &error);
    if (start == HidIoOperation::StartResult::Failed) {
        ++g_bad;
        Evidence(true, "read_failed col=%u err=%lu", c.index, error);
        c.live = false;
        return;
    }
    c.reading = true;
}

bool SendReport(Col& c, const std::array<std::uint8_t, mg::kFrameBytes>& frame) {
    const DWORD length = c.info.caps.OutputReportByteLength;
    if (length < 2 || !c.write) { SetLastError(ERROR_INVALID_PARAMETER); return false; }
    std::vector<std::uint8_t> out(length, 0);
    out[0] = c.outReportId;
    std::memcpy(out.data() + 1, frame.data(), std::min<std::size_t>(frame.size(), length - 1u));
    DWORD error = 0, done = 0;
    const auto start = c.write->StartWrite(out.data(), length, &error);
    if (start == HidIoOperation::StartResult::Failed) { SetLastError(error); return false; }
    if (start == HidIoOperation::StartResult::Pending && c.write->Wait(300) != WAIT_OBJECT_0) {
        c.write->CancelAndDrain(&done, &error);
        SetLastError(WAIT_TIMEOUT);
        return false;
    }
    if (!c.write->Finish(&done, &error, false)) { SetLastError(error); return false; }
    return true;
}

Request GetterRequest(std::size_t index) {
    const auto& g = mg::kGetters[index];
    return {g.module, g.op, 0, false, g.name};
}
Request GetReportRequest() {
    for (const auto& g : mg::kGetters)
        if (g.module == mg::kSetReport.module && g.op == mg::kGetReportOp)
            return {g.module, g.op, 0, false, g.name};
    return {};
}
Request SetReportRequest(std::uint8_t mode) {
    return {mg::kSetReport.module, mg::kSetReport.op, mode, true,
            mode == mg::kReportLive ? "set_report_live" : mode == mg::kReportMode1 ? "set_report_mode1" : "set_report_off"};
}
bool BuildFrame(const Request& r, bool withCrc, std::array<std::uint8_t, mg::kFrameBytes>& frame) {
    return r.set ? mg::MakeSetReportFrame(r.mode, withCrc, frame)
                 : mg::MakeGetterFrame(r.module, r.op, withCrc, frame);
}

void SetModeOn(Col& c, bool on) {
    c.modeOn = on;
    g_modeOn.store(on, std::memory_order_release);
}
void SetConnected(bool on) {
    if (g_connected.exchange(on, std::memory_order_acq_rel) != on) Evidence(true, "connected=%u", on ? 1u : 0u);
}

void SendCurrent(Col& c) {
    std::array<std::uint8_t, mg::kFrameBytes> frame{};
    if (!BuildFrame(c.current, c.crcVariant, frame)) {   // not an allowed frame: nothing is sent
        c.phase = Phase::Done; c.awaiting = false; return;
    }
    ++c.txSeq;
    if (!SendReport(c, frame)) {
        ++g_bad;
        Evidence(true, "tx_failed col=%u seq=%u name=%s err=%lu", c.index, c.txSeq, c.current.name, GetLastError());
        c.phase = Phase::Done; c.awaiting = false;
        SetConnected(false);
        return;
    }
    // Replies to the getters are logged in full below; the transmit lines only for the exchange phases.
    if (c.phase != Phase::Watchdog)
        Evidence(true, "tx col=%u seq=%u name=%s m=%02x op=%02x mode=%d crc=%s", c.index, c.txSeq, c.current.name,
                 c.current.module, c.current.op, c.current.set ? static_cast<int>(c.current.mode) : -1,
                 c.crcVariant ? "calc" : "0000");
    c.awaiting = true;
    c.sentAt = GetTickCount64();
}

// Starts one request. Switching the live mode on is recorded BEFORE the write, so a write that fails
// half way is still followed by a restore attempt.
void Begin(Col& c, Phase phase, const Request& request, bool withCrc = true) {
    c.phase = phase;
    c.current = request;
    c.crcVariant = withCrc;
    c.retried = false;
    c.awaiting = false;
    if (request.set && request.mode != mg::kReportOff) SetModeOn(c, true);
    SendCurrent(c);
}

void BeginRestore(Col& c, const char* reason) {
    SetConnected(false);
    if (!c.modeOn) { c.phase = Phase::Done; c.awaiting = false; return; }
    Evidence(true, "mode_restore col=%u reason=%s stream_frames=%llu", c.index, reason,
             static_cast<unsigned long long>(c.streamFrames));
    c.reportMode = -1;
    Begin(c, Phase::RestoreSet, SetReportRequest(mg::kReportOff), c.setCrc != 0);
}

// The verified mode is used at once: no measuring phase, the keyboard is connected right away.
void EnterStreaming(Col& c) {
    c.phase = Phase::Streaming;
    c.lastFrameAt = GetTickCount64();
    g_statusMode.store(g_chosenMode);
    SetConnected(true);
}
void AfterEnabled(Col& c) { EnterStreaming(c); }

double Ms(std::int64_t us) { return static_cast<double>(us) / 1000.0; }

void LogSummary(const Col& c, unsigned mode, const char* when) {
    const ModeSummary& s = c.summary[mode];
    if (!s.presses && !s.misses && !s.stuck) return;
    Evidence(true, "press_summary col=%u m=%u at=%s presses=%u os=%u misses=%u stuck=%u samples_avg=%.1f rise_samples_avg=%.2f "
                   "rise_ms_avg=%.1f gap_ms_avg=%.1f lag_ms_avg=%.1f",
             c.index, mode, when, s.presses, s.withOs, s.misses, s.stuck,
             s.presses ? static_cast<double>(s.samples) / s.presses : 0.0,
             s.presses ? static_cast<double>(s.riseSamples) / s.presses : 0.0,
             s.presses ? Ms(s.riseSumUs) / s.presses : 0.0,
             s.gaps ? Ms(s.gapSumUs) / static_cast<double>(s.gaps) : 0.0,
             s.withOs ? static_cast<double>(s.lagSumMs) / s.withOs : 0.0);
}

// Mode 1 did not serve the key presses: log why (with the mode-1 numbers so far) and switch to mode 2.
void FallBack(Col& c) {
    if (!c.fallbackReason || c.awaiting || g_chosenMode != mg::kReportMode1) return;
    if (c.phase != Phase::Streaming && c.phase != Phase::Done) return;
    const mg::ModeHealth& h = c.health;
    Evidence(true, "mode1_fallback col=%u reason=%s confirmed=%u misses=%u miss_run=%u stuck=%u", c.index,
             c.fallbackReason, h.confirmed, h.misses, h.missRun, h.stuck);
    LogSummary(c, mg::kReportMode1, "fallback");
    c.fallbackReason = nullptr;
    g_chosenMode = mg::kReportLive;
    c.reportMode = -1;
    Begin(c, Phase::EnableSet, SetReportRequest(g_chosenMode));
}
void CheckHealth(Col& c) {
    if (g_chosenMode != mg::kReportMode1 || c.fallbackReason) return;
    c.fallbackReason = c.health.FallbackReason();
    FallBack(c);
}

void StartMode(Col& c) {
    c.phase = Phase::Done;
    if (!mg::kLiveReportMode) return;
    if (c.reportMode == mg::kReportLive || c.reportMode == mg::kReportMode1) {   // already on: adopt it
        SetModeOn(c, true);                                                      // (and restore it at the end)
        if (c.reportMode == g_chosenMode) {
            Evidence(true, "mode_adopted col=%u mode=%d", c.index, c.reportMode);
            AfterEnabled(c);
            return;
        }
        Evidence(true, "mode_recover col=%u found=%d want=%u", c.index, c.reportMode, g_chosenMode);
        Begin(c, Phase::EnableSet, SetReportRequest(g_chosenMode));   // e.g. left on in another mode by a crash
        return;
    }
    if (c.reportMode != mg::kReportOff) {
        Evidence(true, "mode_skipped col=%u reason=%s mode=%d", c.index,
                 c.reportMode < 0 ? "report_unknown" : "report_unexpected", c.reportMode);
        return;
    }
    Evidence(true, "mode_enable col=%u rev=%u mode=%u", c.index, kRevision, g_chosenMode);
    Begin(c, Phase::EnableSet, SetReportRequest(g_chosenMode));
}

// The current request was answered or abandoned: move to the next step of the exchange.
void Advance(Col& c) {
    c.awaiting = false;
    switch (c.phase) {
    case Phase::Getters:
        if (++c.step < mg::kGetters.size()) { Begin(c, Phase::Getters, GetterRequest(c.step)); return; }
        Evidence(true, "getters_done col=%u replies=%u of=%zu", c.index, c.replies, mg::kGetters.size());
        StartMode(c);
        return;
    case Phase::EnableSet:
        c.reportMode = -1;
        Begin(c, Phase::EnableVerify, GetReportRequest());
        return;
    case Phase::EnableVerify:
        if (c.reportMode == g_chosenMode) {
            Evidence(true, "mode_on col=%u mode=%d", c.index, c.reportMode);
            AfterEnabled(c);
        } else {
            Evidence(true, "mode_enable_failed col=%u mode=%d want=%u", c.index, c.reportMode, g_chosenMode);
            c.phase = Phase::Done;
            SetConnected(false);
            if (g_chosenMode == mg::kReportMode1) {   // mode 1 not accepted: mode 2 is the known working mode
                c.fallbackReason = "readback";
                FallBack(c);
            }
        }
        return;
    case Phase::Watchdog:
        c.lastWatchdogAt = GetTickCount64();
        if (c.reportMode == mg::kReportOff) {   // the mode fell back to 0: switch it on again
            Evidence(true, "mode_lost col=%u mode=0 reenable=%u", c.index, g_chosenMode);
            SetConnected(false);
            Begin(c, Phase::EnableSet, SetReportRequest(g_chosenMode));
        } else {
            c.phase = Phase::Streaming;
        }
        return;
    case Phase::RestoreSet:
        c.reportMode = -1;
        Begin(c, Phase::RestoreVerify, GetReportRequest());
        return;
    case Phase::RestoreVerify:
        if (c.reportMode == mg::kReportOff) {
            SetModeOn(c, false);
            Evidence(true, "mode_off col=%u mode=0 stream_frames=%llu", c.index,
                     static_cast<unsigned long long>(c.streamFrames));
        } else {
            Evidence(true, "restore_unconfirmed col=%u mode=%d", c.index, c.reportMode);
        }
        c.phase = Phase::Done;
        return;
    default:
        return;
    }
}

void CheckTimeout(Col& c, ULONGLONG now) {
    if (!c.awaiting || now - c.sentAt < kReplyTimeoutMs) return;
    Evidence(true, "no_reply col=%u seq=%u name=%s retry=%s", c.index, c.txSeq, c.current.name,
             c.retried ? "none" : "other_crc");
    if (!c.retried) {                          // once more with the other CRC variant
        c.retried = true;
        c.crcVariant = !c.crcVariant;
        SendCurrent(c);
        return;
    }
    Advance(c);
}

// Silence while streaming: the mode may have been switched off (configurator tab left, keyboard reset).
void CheckSilence(Col& c, ULONGLONG now) {
    if (c.phase != Phase::Streaming || c.awaiting) return;
    if (now - c.lastFrameAt < mg::kSilenceCheckMs || now - c.lastWatchdogAt < mg::kSilenceCheckMs) return;
    c.reportMode = -1;
    Begin(c, Phase::Watchdog, GetReportRequest());
}

void DrainOsKeys(Col& c) {
    unsigned tail = g_osTail.load(std::memory_order_acquire);
    const unsigned head = g_osHead.load(std::memory_order_acquire);
    while (tail != head) {
        const OsKey key = g_osKeys[tail % g_osKeys.size()];
        if (key.down) {
            g_learner.OnKeyDown(key.hid, key.time);
        } else {
            const int id = g_learner.IdOfHid(key.hid);
            if (id >= 0) { c.upAtMs[id] = key.time; c.upPending[id] = true; }
        }
        ++tail;
    }
    g_osTail.store(tail, std::memory_order_release);
}

// The current mode's summary; a mode that is not 1 or 2 (watchdog found something else) counts as 0.
ModeSummary& Summary(Col& c) { return c.summary[g_chosenMode <= 2 ? g_chosenMode : 0]; }

// `nowMs` is the QueryPerformanceCounter clock of the key matching, `tick` the GetTickCount64 clock of the publication.
void ResolveKeys(Col& c, std::uint64_t nowMs, std::uint64_t tick) {
    DrainOsKeys(c);
    static unsigned conflicts = 0;   // log bound: a wrong table entry is reported, not repeated forever
    g_learner.Resolve(c.position, nowMs,
        [&](std::uint8_t id, std::uint16_t hid) {                       // learned for an id the table does not know
            const auto slot = static_cast<std::uint8_t>(id + 1);
            g_depth.Bind(slot, hid);
            g_depth.Publish(slot, mg::PositionToMilli(c.position[id]), tick);
            g_learnedKeys.store(static_cast<std::uint32_t>(g_learner.KnownCount()));
            Evidence(true, "learned_key id=%u hid=%02x", id, hid);
        },
        [&](std::uint8_t id, std::uint16_t hid, std::int64_t lagMs, bool first) {   // the table entry matched a key-down
            // Key lines name only keys bound to the gamepad: ordinary typing never enters the log.
            if (first && InputTrace_IsBound(hid))
                Evidence(true, "key_confirmed id=%u hid=%02x lag_ms=%lld", id, hid, static_cast<long long>(lagMs));
            if (g_chosenMode == mg::kReportMode1) c.health.OnConfirm();
            if (c.presses.Active(id)) { c.pressOs[id] = true; c.pressLag[id] = lagMs; return; }
            // A quick tap whose release came before the key-down: its record is still waiting to be written.
            for (auto it = c.finished.rbegin(); it != c.finished.rend(); ++it)
                if (it->id == id) { if (!it->os) { it->os = true; it->lagMs = lagMs; } break; }
        },
        [&](std::uint8_t id, std::uint16_t hid) {                       // the table disagrees with the keyboard
            if (conflicts++ < 64)
                Evidence(true, "key_conflict id=%u table=%02x os=%02x", id, g_learner.Hid(id), hid);
        },
        [&](std::uint8_t id, std::uint16_t hid) {                       // Windows saw the key, the stream did not
            ++Summary(c).misses;
            if (g_chosenMode == mg::kReportMode1) c.health.OnMiss();
            if (InputTrace_IsBound(hid) && c.missLines++ < kMissLineLimit)
                Evidence(true, "key_miss id=%u hid=%02x m=%u pos=%u miss_run=%u", id, hid, g_chosenMode, c.position[id],
                         c.health.missRun);
        });
    CheckHealth(c);
}

// Writes the press lines whose Windows key-down can no longer arrive, and the summaries at 1, 2, 4, 8 ... presses.
void FlushPresses(Col& c, std::uint64_t nowMs, bool all) {
    std::size_t kept = 0;
    for (std::size_t i = 0; i < c.finished.size(); ++i) {
        const FinishedPress& f = c.finished[i];
        if (!all && nowMs <= f.endMs + mg::KeyLearner::kLearnAfterMs) { c.finished[kept++] = f; continue; }
        ModeSummary& s = c.summary[f.mode <= 2 ? f.mode : 0];
        const mg::PressRecord& r = f.rec;
        ++s.presses;
        s.samples += r.samples; s.riseSamples += r.riseSamples; s.riseSumUs += r.RiseUs();
        s.gaps += r.gaps; s.gapSumUs += r.gapSumUs;
        if (f.os) { ++s.withOs; s.lagSumMs += f.lagMs; }
        if (InputTrace_IsBound(g_learner.Hid(f.id)) && c.pressLines++ < kPressLineLimit) {
            char lag[24] = "-";
            if (f.os) _snprintf_s(lag, sizeof(lag), _TRUNCATE, "%lld", static_cast<long long>(f.lagMs));
            Evidence(true, "press id=%u hid=%02x m=%u os=%u lag_ms=%s samples=%u rise=%u rise_ms=%.1f dur_ms=%.1f peak=%u "
                           "gap_min_ms=%.1f gap_avg_ms=%.1f gap_max_ms=%.1f",
                     f.id, g_learner.Hid(f.id), f.mode, f.os ? 1u : 0u, lag, r.samples, r.riseSamples, Ms(r.RiseUs()),
                     Ms(r.DurationUs()), r.peak, Ms(r.gapMinUs), Ms(r.GapAvgUs()), Ms(r.gapMaxUs));
        }
        const unsigned total = c.summary[0].presses + c.summary[1].presses + c.summary[2].presses;
        if (total >= c.nextSummary) {
            c.nextSummary *= 2;
            LogSummary(c, f.mode <= 2 ? f.mode : 0, "count");
        }
    }
    c.finished.resize(kept);
}

// A Windows key-up whose key never sent its release in the stream (kStuckAfterMs): the key would stay pressed.
void CheckStuck(Col& c, std::uint64_t nowMs) {
    for (std::size_t id = 0; id < mg::kKeyCount; ++id) {
        if (!c.upPending[id] || nowMs < c.upAtMs[id] + mg::kStuckAfterMs) continue;
        c.upPending[id] = false;
        if (!c.position[id]) continue;
        ++Summary(c).stuck;
        if (g_chosenMode == mg::kReportMode1) c.health.OnStuck();
        if (InputTrace_IsBound(g_learner.Hid(id)))
            Evidence(true, "release_stuck id=%u hid=%02x m=%u pos=%u last_sample_before_up_ms=%lld", static_cast<unsigned>(id),
                 g_learner.Hid(id), g_chosenMode, c.position[id],
                 static_cast<long long>(c.upAtMs[id]) - static_cast<long long>(c.sampleAtMs[id]));
    }
    CheckHealth(c);
}

void PublishHeld(const Col& c, std::uint64_t now) {
    for (std::size_t id = 0; id < mg::kKeyCount; ++id) {
        if (!g_learner.Hid(id)) continue;
        g_depth.Publish(static_cast<std::uint8_t>(id + 1), mg::PositionToMilli(c.position[id]), now);
    }
}

// Applies one decoded stream frame. Key-downs that arrived since the last frame are matched against the positions
// before this frame first, so a press whose release is in this frame is still matched.
void OnStream(Col& c, const mg::KeyReading* readings, int count, std::uint64_t now) {
    const LONGLONG nowUs = NowUs();
    const auto nowMs = static_cast<std::uint64_t>(nowUs / 1000);
    ResolveKeys(c, nowMs, now);
    ++c.streamFrames;
    if (c.lastFrameUs) {
        const double gap = static_cast<double>(nowUs - c.lastFrameUs);
        c.avgGapUs += (gap - c.avgGapUs) / 16.0;
        const unsigned bucket = gap < 2000 ? 0u : gap < 10000 ? 1u : gap < 30000 ? 2u : 3u;
        ++c.gapBucket[bucket];
        g_avgGapUs.store(static_cast<std::uint32_t>(c.avgGapUs));
        g_maxGapUs.store(std::max<std::uint32_t>(g_maxGapUs.load(), static_cast<std::uint32_t>(std::min(gap, 4.0e9))));
        if (c.avgGapUs > 1.0) g_hz10.store(static_cast<std::uint32_t>(1.0e7 / c.avgGapUs));
    } else {
        c.avgGapUs = 20000;
    }
    c.lastFrameUs = nowUs;
    c.lastFrameAt = GetTickCount64();
    for (int i = 0; i < count; ++i) {
        const mg::KeyReading& r = readings[i];
        const std::uint8_t before = c.position[r.id];
        c.sampleAtMs[r.id] = nowMs;
        c.upPending[r.id] = false;                    // the key sent a sample after its Windows key-up
        const bool wasActive = c.presses.Active(r.id);
        mg::PressRecord done;
        if (c.presses.Sample(r.id, r.position, nowUs, done)) {
            FinishedPress f;
            f.id = r.id; f.mode = g_chosenMode; f.os = c.pressOs[r.id]; f.lagMs = c.pressLag[r.id];
            f.endMs = nowMs; f.rec = done;
            c.finished.push_back(f);
        } else if (!wasActive && c.presses.Active(r.id)) {
            c.pressOs[r.id] = false; c.pressLag[r.id] = 0;
        }
        if (r.position) {
            // Update interval of a pressed key (the figure that limits gaming use).
            if (c.keyLastUs[r.id] && before) {
                const double gap = static_cast<double>(nowUs - c.keyLastUs[r.id]);
                if (gap > 0) ++c.keyGapBucket[gap < 20000 ? 0 : gap < 40000 ? 1 : gap < 80000 ? 2 : 3];
            }
            c.keyLastUs[r.id] = nowUs;
        } else {
            c.keyLastUs[r.id] = 0;
        }
        c.position[r.id] = r.position;
        g_learner.OnPosition(r.id, before, r.position, nowMs);
        const std::uint16_t hid = g_learner.Hid(r.id);
        if (!hid) continue;
        const unsigned milli = mg::PositionToMilli(r.position);
        g_depth.Publish(static_cast<std::uint8_t>(r.id + 1), static_cast<std::uint16_t>(milli), now);
        if (InputTrace_IsBound(hid)) InputTrace_Source("melgeek", r.id, 0, r.position, milli, hid);
    }
    ResolveKeys(c, nowMs, now);
    FlushPresses(c, nowMs, false);
    g_last.store(now);
}

void OnReport(Col& c, const std::uint8_t* d, std::size_t n) {
    ++g_reports;
    c.stats.Add(d, n);
    const std::uint8_t* frame = n > 1 ? d + 1 : nullptr;
    const std::size_t frameSize = n > 0 ? n - 1 : 0;
    const mg::Header header = mg::ParseHeader(frame, frameSize);

    const char* context = c.phase == Phase::Streaming ? "live" : "stream";
    unsigned seq = 0;
    bool matched = false;
    if (c.awaiting) {
        seq = c.txSeq;
        matched = frameSize >= 2 && frame[0] == c.current.module && frame[1] == mg::ReplyOp(c.current.op);
        context = matched ? "reply" : "during";
    }

    // Live key positions: any valid stream frame is applied whatever the exchange phase is.
    mg::KeyReading readings[mg::kMaxEntries];
    const int count = matched ? -1 : mg::DecodeStreamFrame(frame, frameSize, readings);
    if (count > 0) OnStream(c, readings, count, GetTickCount64());

    const std::uint64_t hash = mg::Fnv1a(d, n);
    const bool isNew = c.seen.size() < kSeenLimit && c.seen.insert(hash).second;
    const std::uint32_t cls = frameSize >= 2 ? (static_cast<std::uint32_t>(frame[0]) << 8) | frame[1] : 0u;
    const bool newClass = c.classes.size() < 4096 && c.classes.insert(cls).second;
    // The periodic watchdog reply (every few seconds on a quiet keyboard) is not logged.
    // Live stream frames carry key ids, i.e. typing: only their statistics are logged.
    const bool stream = count > 0;
    bool write = matched && c.phase != Phase::Watchdog;
    if (!write && !matched && !stream && isNew && c.logged < kDistinctLimit) { write = true; ++c.logged; }
    else if (!write && !matched && !stream && newClass && c.classesLogged < kClassLimit) { write = true; ++c.classesLogged; }
    if (write) {
        char head[96];
        _snprintf_s(head, sizeof(head), _TRUNCATE, "m%02x.op%02x.len%u.b%02x%02x%02x.crc%04x:%s",
                    header.module, header.op, header.length, header.b3, header.b4, header.b5,
                    header.crc, mg::CrcName(header.state));
        const std::string data = n > 1 ? Hex(d + 1, n - 1) : std::string("-");
        Evidence(matched, "rx col=%u ctx=%s seq=%u n=%zu rid=%02x hdr=%s data=%s", c.index, context, seq, n,
                 n ? d[0] : 0u, head, data.c_str());
    }
    if (matched) {
        if (c.current.set) c.setCrc = c.crcVariant ? 1 : 0;
        else if (c.current.op == mg::kGetReportOp) c.reportMode = mg::ReportModeFromReply(frame, frameSize);
        if (c.phase == Phase::Getters) ++c.replies;
        Advance(c);
    }
    if (c.stats.frames >= c.nextFlush) {
        FlushStats(c, false);
        c.nextFlush = mg::NextFlushAt(c.nextFlush);
    }
}

// Reaps every completed read of the collection (bounded per call) and restarts it.
void Pump(Col& c) {
    for (int guard = 0; guard < 256 && c.live; ++guard) {
        if (!c.reading) StartRead(c);
        if (!c.live || !c.reading) return;
        if (c.read->Wait(0) != WAIT_OBJECT_0) return;
        DWORD done = 0, error = 0;
        const bool ok = c.read->Finish(&done, &error, false);
        c.reading = false;
        if (!ok) {
            if (g_stop.load()) return;
            ++g_bad;
            Evidence(true, "read_failed col=%u err=%lu", c.index, error);
            c.live = false;
            return;
        }
        OnReport(c, c.buffer.data(), done);
    }
}

bool OpenCollection(Col& c) {
    c.handle.v = CreateFileW(c.info.path.c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED, nullptr);
    if (!c.handle) {
        Evidence(true, "open_failed col=%u err=%lu", c.index, GetLastError());
        return false;
    }
    HidD_SetNumInputBuffers(c.handle.v, 64);
    c.read = std::make_unique<HidIoOperation>(c.handle.v);
    c.write = std::make_unique<HidIoOperation>(c.handle.v);
    c.buffer.assign(c.info.caps.InputReportByteLength, 0);
    for (const auto& v : c.info.capSet.value[1]) { c.outReportId = v.ReportID; break; }
    if (!c.outReportId)
        for (const auto& b : c.info.capSet.button[1]) { c.outReportId = b.ReportID; break; }
    return c.read->IsValid() && c.write->IsValid();
}

// Leaves the keyboard the way it was found: report mode back to 0. Runs on session end, stop and
// device removal (the write simply fails when the device is gone).
void FinalRestore(Col& c) {
    SetConnected(false);
    if (!c.modeOn) return;
    if (c.phase != Phase::RestoreSet && c.phase != Phase::RestoreVerify)
        BeginRestore(c, g_stop.load() ? "stop" : "session_end");
    const auto deadline = GetTickCount64() + 3000;
    while (c.phase != Phase::Done && GetTickCount64() < deadline) {
        Pump(c);
        CheckTimeout(c, GetTickCount64());
        if (c.phase == Phase::Done) break;
        if (c.live && c.reading) WaitForSingleObject(c.read->Event(), 50);
        else Sleep(10);
    }
    if (c.modeOn) Evidence(true, "restore_incomplete col=%u phase=%u", c.index, static_cast<unsigned>(c.phase));
}

void Run(const std::vector<Found>& found) {
    struct End {
        ~End() {
            g_present.store(false, std::memory_order_release);
            g_modeOn.store(false);
            g_connected.store(false);
            g_depth.Clear();
        }
    } end;
    g_depth.Clear();
    Evidence(true, "session rev=%u mode=%u vid=%04x pid=%04x collections=%zu", kRevision, g_chosenMode, mg::kVendorId,
             mg::kProductId, found.size());
    std::unique_ptr<Col> vendor;
    for (std::size_t i = 0; i < found.size(); ++i) {
        LogCollection(static_cast<unsigned>(i), found[i]);
        if (found[i].role != mg::Role::Vendor || vendor) continue;
        // The vendor collection FF60:61 with 64-byte reports both ways (verified on hardware).
        if (found[i].caps.UsagePage != 0xff60 || found[i].caps.Usage != 0x61 ||
            found[i].caps.InputReportByteLength != 65 || found[i].caps.OutputReportByteLength != 65) {
            Evidence(true, "skipped col=%zu reason=unexpected_vendor_collection", i);
            continue;
        }
        auto c = std::make_unique<Col>();
        c->index = static_cast<unsigned>(i);
        c->info = found[i];
        if (!NativeAnalogRouting_Claim(mg::kVendorId, mg::kProductId, c->info.path.c_str(),
                NativeAnalogProtocol::MelGeekMade68Air) &&
            !NativeAnalogRouting_IsClaimedBy(c->info.path.c_str(), NativeAnalogProtocol::MelGeekMade68Air)) {
            Evidence(true, "skipped col=%zu reason=claimed_elsewhere", i);
            continue;
        }
        if (!OpenCollection(*c)) { ++g_bad; continue; }
        c->live = !c->buffer.empty();
        g_vendorPage.store(c->info.caps.UsagePage); g_vendorUsage.store(c->info.caps.Usage);
        g_vendorIn.store(c->info.caps.InputReportByteLength);
        g_vendorOut.store(c->info.caps.OutputReportByteLength);
        g_present.store(true, std::memory_order_release);
        Evidence(true, "opened col=%u outrid=%02x", c->index, c->outReportId);
        vendor = std::move(c);
    }
    if (!vendor) { g_failedSessions.fetch_add(1); return; }
    Col& c = *vendor;

    // The table and the keys learned earlier in this process are bound from the start.
    for (std::size_t id = 0; id < mg::kKeyCount; ++id)
        if (g_learner.Hid(id)) g_depth.Bind(static_cast<std::uint8_t>(id + 1), g_learner.Hid(id));
    g_learnedKeys.store(static_cast<std::uint32_t>(g_learner.KnownCount()));

    // Reader first, so no early reply is lost, then the getters, then the report mode.
    StartRead(c);
    if (c.live) Begin(c, Phase::Getters, GetterRequest(0));

    ULONGLONG lastHeartbeat = 0;
    while (!g_stop.load()) {
        Pump(c);
        if (!c.live || !c.reading) break;        // the reader is gone: the device left
        const auto now = GetTickCount64();
        CheckTimeout(c, now);
        FallBack(c);                             // a fallback decided while a request was pending
        CheckSilence(c, now);
        if (now - lastHeartbeat >= kHeartbeatMs) {
            lastHeartbeat = now;
            const auto nowMs = NowMs();
            ResolveKeys(c, nowMs, now);
            CheckStuck(c, nowMs);
            FlushPresses(c, nowMs, false);
            if (g_connected.load()) PublishHeld(c, now);
        }
        HANDLE event = c.read->Event();
        WaitForMultipleObjects(1, &event, FALSE, 50);
    }
    FinalRestore(c);
    if (c.reading) { DWORD done = 0, error = 0; c.read->CancelAndDrain(&done, &error); c.reading = false; }
    FlushPresses(c, NowMs(), true);
    for (unsigned mode = 1; mode <= 2; ++mode) LogSummary(c, mode, "session_end");
    FlushStats(c, true);
    Evidence(true, "session_end reports=%llu stream=%llu known=%zu learned=%zu", static_cast<unsigned long long>(g_reports.load()),
             static_cast<unsigned long long>(c.streamFrames), g_learner.KnownCount(), g_learner.LearnedCount());
    if (!g_stop.load()) g_failedSessions.fetch_add(1);
}

unsigned __stdcall Worker(void*) noexcept {
    try {
        while (!g_stop.load()) {
            const auto found = Enumerate();
            if (found.empty()) {
                g_present.store(false, std::memory_order_release);
                g_failedSessions.store(0);
                g_blocked.store(false);
            } else if (!g_blocked.load()) {
                Run(found);
                if (g_failedSessions.load() >= 3) {   // stop retrying until the device list changes
                    g_blocked.store(true);
                    Evidence(true, "blocked after=%u", g_failedSessions.load());
                }
            }
            if (!g_stop.load())
                WaitForSingleObject(g_wake, (found.empty() || g_blocked.load()) ? INFINITE : 2000);
        }
    } catch (...) {
        ++g_bad;
        SupportLog_Event("melgeek.worker_exception", 1);
    }
    g_present.store(false);
    g_connected.store(false);
    g_running.store(false);
    return 0;
}

bool Start() {
    std::lock_guard<std::mutex> l(g_service);
    if (g_thread) return g_running.load();
    g_stop.store(false);
    g_present.store(false);
    g_connected.store(false);
    g_blocked.store(false);
    g_failedSessions.store(0);
    g_reports.store(0); g_bad.store(0); g_last.store(0);
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return false;
    g_running.store(true);
    unsigned id = 0;
    g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &id));
    if (!g_thread) { g_running.store(false); CloseHandle(g_wake); g_wake = nullptr; return false; }
    return true;
}
halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard<std::mutex> l(g_service);
    g_stop.store(true);
    if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    SetEvent(g_wake);
    const auto wait = WaitForSingleObject(g_thread, 8000);   // includes restoring report mode 0
    if (wait != WAIT_OBJECT_0)
        return NativeAnalogBackendStopFailed(generation,
            halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, WAIT_TIMEOUT);
    CloseHandle(g_thread); g_thread = nullptr; CloseHandle(g_wake); g_wake = nullptr;
    g_connected.store(false);
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
    std::lock_guard<std::mutex> l(g_service);
    g_blocked.store(false);
    g_failedSessions.store(0);
    if (g_wake) SetEvent(g_wake);
}
bool Present() { return g_present.load(); }
bool Connected() { return g_connected.load(); }
bool Owns(std::uint16_t hid) { return Connected() && g_depth.Owns(hid); }
std::uint16_t Get(std::uint16_t hid) {
    if (!Owns(hid)) return 0;
    return g_depth.Read(hid, GetTickCount64(), kRetentionMs).milli;
}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if (!out) return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
    out->vendorId = mg::kVendorId;
    out->productId = mg::kProductId;
    out->usagePage = static_cast<std::uint16_t>(g_vendorPage.load());
    out->usage = static_cast<std::uint16_t>(g_vendorUsage.load());
    out->inputReportBytes = g_vendorIn.load();
    out->outputReportBytes = g_vendorOut.load();
    out->nominalRawLevels = mg::kMaxPosition + 1u;
    out->mappedKeys = Connected() ? g_learnedKeys.load() : 0;
    out->activeKeys = Connected() ? g_depth.Active(GetTickCount64()) : 0;
    out->updateHz10 = g_hz10.load();
    out->averageIntervalUs = g_avgGapUs.load();
    out->maximumIntervalUs = g_maxGapUs.load();
    out->successfulUpdates = g_reports.load();
    out->failedUpdates = g_bad.load();
    const auto last = g_last.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff, now - last)) : 0;
    // The keyboard identified itself through the protocol (exact VID:PID, vendor collection, valid get_ver reply):
    // the built-in ANSI layout is selected automatically while the live report mode is active.
    if (Connected()) out->verifiedLayoutToken = halljoy::layout_identity::Token("melgeek-made68air", "MADE68AIR-0416-0114");
    wcscpy_s(out->deviceName, L"MelGeek MADE68 Air");
    wcscpy_s(out->status, Connected()
        ? (g_statusMode.load() == mg::kReportMode1 ? L"MADE68 Air: live key travel active (report mode 1, under test)"
                                                    : L"MADE68 Air: live key travel active (low update rate, 20 per second)")
        : g_modeOn.load() ? L"MADE68 Air: enabling live key travel"
                          : L"MADE68 Air: checking the vendor protocol (details in the log)");
}
}  // namespace

// Raw Input key-downs of this keyboard feed the key-id to HID-usage matching; key-ups the stuck-release check.
void MelGeekMade68Air_ObserveRawKey(void* device, std::uint16_t hid, bool down) noexcept {
    if (!hid || !g_connected.load()) return;
    static void* lastDevice = nullptr;
    static bool lastMatch = false;
    if (device != lastDevice) {
        lastDevice = device;
        lastMatch = false;
        wchar_t name[512]{};
        UINT size = static_cast<UINT>(std::size(name));
        if (device && GetRawInputDeviceInfoW(static_cast<HANDLE>(device), RIDI_DEVICENAME, name, &size) != static_cast<UINT>(-1)) {
            for (auto& ch : name) ch = static_cast<wchar_t>(towlower(ch));
            wchar_t want[32]{};
            swprintf_s(want, L"vid_%04x&pid_%04x", mg::kVendorId, mg::kProductId);
            lastMatch = std::wstring(name).find(want) != std::wstring::npos;
        }
    }
    if (!lastMatch) return;
    const auto head = g_osHead.load();
    if (head - g_osTail.load() >= g_osKeys.size()) return;   // worker behind: drop
    g_osKeys[head % g_osKeys.size()] = OsKey{hid, down, NowMs()};
    g_osHead.store(head + 1, std::memory_order_release);
}

const NativeAnalogBackendDescriptor& MelGeekMade68Air_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor),
        "melgeek-made68air", L"MelGeek MADE68 Air live key travel",
        NativeAnalogProtocol::MelGeekMade68Air,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_StreamTransport | NativeAnalogBackendFlag_ReversibleControlProbe,
        nullptr, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return d;
}
