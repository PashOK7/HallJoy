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
#include <bitset>
#include <cstdio>
#include <cwctype>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "logitech_rapid_backend.h"
#include "logitech_rapid_protocol.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "support_log.h"
#include "input_trace.h"
#include "keyboard_support_status.h"
#include "generated/layout_pipeline/identities.h"
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

// Logitech G RAPID keyboards: HID++ 2.0 feature 0x1B08 live key-depth stream
// (docs/current/LOGITECH_RAPID_2026-10-05.md). The stream is switched on after
// admission and off at pause/exit; it carries 0.1 mm steps. Windows exposes the
// HID++ short (report 0x10, 7 bytes) and long (0x11, 20 bytes) reports as two
// collections of one interface: requests go to the long one, events may come
// on either, so both are read. G HUB may share the interface: the stream is
// re-armed every 2 s and replies are told apart by our software id.
namespace {
namespace lr = halljoy::logitech_rapid;

struct Handle {
    HANDLE v = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : v(h) {}
    ~Handle() { if (v && v != INVALID_HANDLE_VALUE) CloseHandle(v); }
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return v && v != INVALID_HANDLE_VALUE; }
};
// Generic HID++ 0x1B08 support (owner decision 2026-10-07): any Logitech
// device whose vendor HID++ collection reports the analog feature is admitted
// as a keyboard outside the catalog; keys are learned at run time.
const lr::Model kGenericModel{0, L"Logitech keyboard (HID++ analog)", L"Logitech keyboard", nullptr};
struct Candidate { std::wstring longPath, shortPath; const lr::Model* model = nullptr; std::uint16_t pid = 0; bool generic = false; };

std::atomic<bool> g_stop{false}, g_running{false}, g_present{false}, g_connected{false};
std::atomic<std::uint64_t> g_good{0}, g_bad{0}, g_last{0};
std::atomic<unsigned> g_travel{0};
std::array<std::atomic<std::uint16_t>, 0x500> g_values{};
std::atomic<const lr::Model*> g_model{&lr::kModels[1]};
std::atomic<std::uint16_t> g_pid{0};
std::atomic<bool> g_generic{false}, g_genericFound{false}, g_genericReset{false};
std::vector<std::wstring> g_genericRejected; // interface keys without 0x1B08 (worker only)
std::mutex g_service;
HANDLE g_thread = nullptr, g_wake = nullptr;
std::bitset<256> g_unmappedLogged;
bool g_featuresLogged = false, g_extraLogged = false;
std::atomic<bool> g_formatBlocked{false}, g_capturing{false}, g_captureFull{false};
std::atomic<bool> g_v2{false}; // the session uses version-2 frames and kKeyMapV2
std::array<std::atomic<bool>, 0x500> g_learnedOwned{}; // HID usages learned this session

// Key-downs of the Logitech keyboard from the UI thread (single producer) to
// the worker (single consumer), for V2KeyLearner.
struct OsKey { std::uint16_t hid; ULONGLONG time; };
std::array<OsKey, 64> g_osKeys{};
std::atomic<unsigned> g_osHead{0}, g_osTail{0};

// Raw format capture (owner rule 2026-10-06: the log must carry the data; no
// time limits, no instructions for the user). While the event layout of a
// feature version is unknown nothing is published. The stream stays on for
// the whole session and every report of the HID++ collections except our own
// replies (the first 4 are kept) goes to the log's capture store
// (SupportLog_Capture) as hex; an exact repeat of the previous report only
// counts. Meanwhile every key's first presses are recorded (capture.key) to
// match the data to physical keys. Bounded by content: when the store is full
// the stream is switched off. Pressing any keys for a few seconds and opening
// the log is enough.
constexpr unsigned kOwnRepliesLogged = 4;
struct Capture {
    std::array<std::uint8_t, lr::kLongBytes> last{};
    std::size_t lastSize = 0;
    unsigned own = 0;
    unsigned long long repeats = 0, records = 0;
    bool full = false;
    void Store(const char* line) {
        if (SupportLog_Capture(line)) ++records;
        else full = true;
    }
    void FlushRepeats() {
        if (!repeats || full) return;
        char line[64]{};
        snprintf(line, sizeof(line), "capture.logitech repeat=%llu", repeats);
        repeats = 0;
        Store(line);
    }
    void Report(const std::uint8_t* r, std::size_t n) {
        if (full || !r || !n) return;
        n = std::min(n, lr::kLongBytes);
        if (n >= 4 && (r[3] & 0x0f) == lr::kSoftwareId && own++ >= kOwnRepliesLogged) return;
        if (n == lastSize && std::equal(r, r + n, last.begin())) { ++repeats; return; }
        FlushRepeats();
        std::copy(r, r + n, last.begin()); lastSize = n;
        char line[128]{};
        int at = snprintf(line, sizeof(line), "capture.logitech n=%zu", n);
        for (std::size_t i = 0; i < n && at > 0 && at < static_cast<int>(sizeof(line)) - 4; ++i)
            at += snprintf(line + at, sizeof(line) - at, " %02x", r[i]);
        Store(line);
    }
};

void Clear() {
    g_connected.store(false);
    for (auto& v : g_values) v.store(0);
    for (auto& v : g_learnedOwned) v.store(false);
}
void Failure(const char* stage, SupportLogDetail detail) {
    ++g_bad;
    SupportLog_Event(stage, 0, detail);
}

std::wstring Lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; }
// Collections of one interface share the path up to "&col".
std::wstring InterfaceKey(const std::wstring& path) {
    auto lower = Lower(path);
    const auto col = lower.find(L"&col");
    return col == std::wstring::npos ? lower : lower.substr(0, col);
}

std::vector<Candidate> Enumerate() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    struct FreeSet { HDEVINFO s; ~FreeSet() { SetupDiDestroyDeviceInfoList(s); } } freeSet{set};
    struct Found { std::wstring path, key; const lr::Model* model; std::uint16_t pid; bool isLong; };
    std::vector<Found> found;
    for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA d{}; d.cbSize = sizeof(d);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &d)) break;
        DWORD needed = 0; SetupDiGetDeviceInterfaceDetailW(set, &d, nullptr, 0, &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<unsigned char> bytes(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &d, detail, needed, nullptr, nullptr)) continue;
        Handle h(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
        if (!h) continue;
        HIDD_ATTRIBUTES a{}; a.Size = sizeof(a);
        if (!HidD_GetAttributes(h.v, &a)) continue;
        if (a.VendorID != lr::kVendorId) continue;
        const auto* model = lr::FindModel(a.VendorID, a.ProductID);
        const std::uint16_t pid = a.ProductID;
        PHIDP_PREPARSED_DATA data = nullptr;
        if (!HidD_GetPreparsedData(h.v, &data)) continue;
        HIDP_CAPS caps{}; const auto status = HidP_GetCaps(data, &caps); HidD_FreePreparsedData(data);
        if (status != HIDP_STATUS_SUCCESS || caps.UsagePage < 0xff00) continue;
        const bool isLong = caps.InputReportByteLength == lr::kLongBytes && caps.OutputReportByteLength == lr::kLongBytes;
        const bool isShort = caps.InputReportByteLength == lr::kShortBytes;
        if (isLong || isShort) found.push_back({detail->DevicePath, InterfaceKey(detail->DevicePath), model, pid, isLong});
    }
    std::vector<Candidate> out;
    for (const auto& f : found) {
        if (!f.isLong) continue;
        Candidate c{f.path, {}, f.model ? f.model : &kGenericModel, f.pid, f.model == nullptr};
        for (const auto& s : found) if (!s.isLong && s.key == f.key) c.shortPath = s.path;
        out.push_back(std::move(c));
    }
    return out;
}

// One overlapped read in flight per collection; whichever completes first is handled.
class Session {
    struct Channel {
        Handle handle;
        std::unique_ptr<HidIoOperation> op;
        std::array<std::uint8_t, lr::kLongBytes> buffer{};
        DWORD size = 0;
        bool armed = false;
    };
    Channel channels_[2];
    unsigned count_ = 0;
    std::unique_ptr<HidIoOperation> write_;

    bool Arm(Channel& c) {
        if (c.armed) return true;
        DWORD error = 0;
        if (c.op->StartRead(c.buffer.data(), c.size, &error) == HidIoOperation::StartResult::Failed) {
            SetLastError(error); return false;
        }
        c.armed = true; return true;
    }

public:
    explicit Session(const Candidate& c) {
        const auto open = [](const std::wstring& path, DWORD access) {
            return CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        };
        channels_[0].handle.v = open(c.longPath, GENERIC_READ | GENERIC_WRITE);
        channels_[0].size = static_cast<DWORD>(lr::kLongBytes);
        count_ = 1;
        if (!c.shortPath.empty()) {
            channels_[1].handle.v = open(c.shortPath, GENERIC_READ);
            channels_[1].size = static_cast<DWORD>(lr::kShortBytes);
            if (channels_[1].handle) count_ = 2;
        }
        for (unsigned i = 0; i < count_; ++i) {
            if (!channels_[i].handle) continue;
            HidD_SetNumInputBuffers(channels_[i].handle.v, 512);
            channels_[i].op = std::make_unique<HidIoOperation>(channels_[i].handle.v);
        }
        if (channels_[0].handle) write_ = std::make_unique<HidIoOperation>(channels_[0].handle.v);
    }
    ~Session() { for (auto& c : channels_) c.op.reset(); write_.reset(); }
    bool Valid() const { return channels_[0].handle && channels_[0].op && write_; }
    bool HasShort() const { return count_ == 2; }

    bool Send(const lr::Request& request) {
        DWORD error = 0, done = 0;
        auto copy = request;
        const auto start = write_->StartWrite(copy.data(), static_cast<DWORD>(copy.size()), &error);
        if (start == HidIoOperation::StartResult::Failed) { SetLastError(error); return false; }
        if (start == HidIoOperation::StartResult::Pending && write_->Wait(200) != WAIT_OBJECT_0) {
            write_->CancelAndDrain(&done, &error); SetLastError(WAIT_TIMEOUT); return false;
        }
        if (!write_->Finish(&done, &error, false)) { SetLastError(error); return false; }
        return true;
    }

    // Waits up to `timeout` for one report; false only on an I/O failure.
    template <class F> bool Pump(DWORD timeout, F&& handle) {
        HANDLE events[2]{};
        for (unsigned i = 0; i < count_; ++i) {
            if (!Arm(channels_[i])) return false;
            events[i] = channels_[i].op->Event();
        }
        const DWORD wait = WaitForMultipleObjects(count_, events, FALSE, timeout);
        if (wait == WAIT_TIMEOUT) return true;
        if (wait < WAIT_OBJECT_0 || wait >= WAIT_OBJECT_0 + count_) return false;
        auto& c = channels_[wait - WAIT_OBJECT_0];
        DWORD done = 0, error = 0;
        if (!c.op->Finish(&done, &error, false)) {
            if (error == ERROR_IO_INCOMPLETE) return true;
            c.armed = false; SetLastError(error); return false;
        }
        c.armed = false;
        handle(c.buffer.data(), static_cast<std::size_t>(done));
        return true;
    }
};

template <class Events>
bool Transact(Session& s, const lr::Request& request, std::array<std::uint8_t, 16>& params,
              std::uint8_t& error, Events&& events) {
    if (!s.Send(request)) { Failure("logitech.send_failed", SupportLog_Win32(GetLastError())); return false; }
    const auto end = GetTickCount64() + 500;
    lr::ReplyKind kind = lr::ReplyKind::None;
    while (kind == lr::ReplyKind::None && GetTickCount64() < end && !g_stop.load()) {
        if (!s.Pump(20, [&](const std::uint8_t* r, std::size_t n) {
                kind = lr::MatchReply(request, r, n, &params, &error);
                if (kind == lr::ReplyKind::None) events(r, n);
            })) { Failure("logitech.read_failed", SupportLog_Win32(GetLastError())); return false; }
    }
    if (kind == lr::ReplyKind::Reply) return true;
    if (kind == lr::ReplyKind::Error)
        Failure("logitech.hidpp_error", SupportLog_Protocol((std::uint64_t{request[2]} << 16) | (std::uint64_t{request[3]} << 8) | error));
    else if (!g_stop.load())
        Failure("logitech.reply_timeout", SupportLog_Protocol((std::uint64_t{request[2]} << 8) | request[3]));
    return false;
}

const auto kIgnore = [](const std::uint8_t*, std::size_t) {};

// Feature list once per process (read-only): shows what a new model offers.
void LogFeatures(Session& s) {
    if (g_featuresLogged) return;
    g_featuresLogged = true;
    std::array<std::uint8_t, 16> p{}; std::uint8_t e = 0;
    if (!Transact(s, lr::GetFeature(lr::kFeatureSet), p, e, kIgnore) || !p[0]) return;
    const auto setIndex = p[0];
    if (!Transact(s, lr::Build(setIndex, 0), p, e, kIgnore)) return;
    const unsigned count = std::min<unsigned>(p[0], 64);
    SupportLog_Event("logitech.feature_count", count);
    for (unsigned i = 1; i <= count && !g_stop.load(); ++i) {
        if (!Transact(s, lr::Build(setIndex, 1, {static_cast<std::uint8_t>(i)}), p, e, kIgnore)) return;
        SupportLog_Event("logitech.feature",
            (std::uint64_t{i} << 32) | (std::uint64_t{p[0]} << 24) | (std::uint64_t{p[1]} << 16) |
            (std::uint64_t{p[2]} << 8) | p[3]);
    }
}

void Run(const Candidate& c) {
    // After an unknown or mismatching format, wait for a device change.
    if (g_formatBlocked.load()) return;
    Clear();
    struct End { ~End() { Clear(); } } end;
    g_model.store(c.model);
    g_pid.store(c.pid);
    g_generic.store(c.generic);
    Session s(c);
    if (!s.Valid()) { Failure("logitech.open_failed", SupportLog_Win32(GetLastError())); return; }
    SupportLog_Event(c.generic ? "logitech.generic_model" : "logitech.model", c.pid, SupportLog_Data(s.HasShort() ? 2 : 1));
    LogFeatures(s);
    std::array<std::uint8_t, 16> p{}; std::uint8_t e = 0;
    if (!Transact(s, lr::GetFeature(lr::kAnalogFeature), p, e, kIgnore)) return;
    const std::uint8_t index = p[0], version = p[2];
    SupportLog_Event("logitech.analog_feature", (std::uint64_t{p[0]} << 16) | (std::uint64_t{p[1]} << 8) | p[2]);
    if (!index) { Failure("logitech.no_analog_feature", SupportLog_Protocol(lr::kAnalogFeature)); return; }
    if (!Transact(s, lr::Build(index, 0), p, e, kIgnore)) return;
    // Full info reply (device metadata), for new feature versions.
    std::uint64_t infoA = 0, infoB = 0;
    for (int i = 0; i < 8; ++i) { infoA = (infoA << 8) | p[i]; infoB = (infoB << 8) | p[8 + i]; }
    SupportLog_Event("logitech.analog_info", infoA, SupportLog_Data(infoB));
    const auto format = lr::FormatFromInfo(version, p);

    // Event shape (aggregate, no key ids or per-key values): which payload
    // bytes ever carry data, and the largest values of bytes 5 and 6.
    struct Shape { std::uint32_t mask = 0; std::uint8_t max5 = 0, max6 = 0; std::uint32_t events = 0; bool logged = false; } shape;
    const auto logShape = [&]() {
        if (!shape.events) return;
        SupportLog_Event("logitech.event_shape", (std::uint64_t{shape.events} << 32) |
            (std::uint64_t{shape.mask & 0xffff} << 16) | (std::uint64_t{shape.max5} << 8) | shape.max6);
    };
    const auto observe = [&](const std::uint8_t* r, std::size_t n) {
        lr::DepthEvent probe{};
        if (!lr::DecodeDepth(index, false, r, n, &probe)) return false;
        const std::size_t size = r[0] == lr::kLongReport ? lr::kLongBytes : lr::kShortBytes;
        for (std::size_t i = 4; i < size; ++i) if (r[i]) shape.mask |= 1u << (i - 4);
        shape.max5 = std::max(shape.max5, r[5]);
        shape.max6 = std::max(shape.max6, r[6]);
        if (++shape.events == 64 && !shape.logged) { shape.logged = true; logShape(); }
        return true;
    };

    // Switch the stream off on every exit path (pause, exit, disconnect),
    // including a failed switch-on whose reply was lost.
    struct StreamOff {
        Session& s; std::uint8_t index;
        ~StreamOff() {
            Clear();
            if (!s.Send(lr::Build(index, 3, {0})))
                SupportLog_Event("logitech.stream_off_failed", 0, SupportLog_Win32(GetLastError()));
        }
    };

    if (!format.travel || !format.eventsVerified) {
        // Unknown or unverified event layout: publish nothing and capture the
        // raw stream for the log (Capture) until the capture store is full.
        Failure("logitech.format_unknown", SupportLog_Protocol((std::uint64_t{version} << 16) | format.travel));
        if (g_captureFull.load()) { g_formatBlocked.store(true); return; }
        StreamOff off{s, index};
        Capture capture;
        const auto record = [&](const std::uint8_t* r, std::size_t n) { capture.Report(r, n); observe(r, n); };
        g_capturing.store(true);
        InputTrace_SetKeyCapture(true);
        struct EndCapture { ~EndCapture() { g_capturing.store(false); InputTrace_SetKeyCapture(false); } } endCapture;
        if (!Transact(s, lr::Build(index, 3, {1}), p, e, record)) return;
        SupportLog_Event("logitech.capture_start", (std::uint64_t{version} << 16) | format.travel);
        auto rearm = GetTickCount64() + 2000;
        while (!g_stop.load() && !capture.full) {
            if (!s.Pump(50, record)) { Failure("logitech.stream_failed", SupportLog_Win32(GetLastError())); break; }
            if (GetTickCount64() >= rearm) {
                // Protocol keep-alive only: another program (G HUB) may switch the stream off.
                if (!s.Send(lr::Build(index, 3, {1}))) { Failure("logitech.rearm_failed", SupportLog_Win32(GetLastError())); break; }
                rearm = GetTickCount64() + 2000;
            }
        }
        capture.FlushRepeats();
        logShape();
        if (capture.full) { g_captureFull.store(true); g_formatBlocked.store(true); }
        SupportLog_Event("logitech.raw_capture", capture.records, SupportLog_Data(capture.full ? 1 : 0));
        return;
    }
    if (!NativeAnalogRouting_Claim(lr::kVendorId, c.pid, c.longPath.c_str(), NativeAnalogProtocol::LogitechRapid) &&
        !NativeAnalogRouting_IsClaimedBy(c.longPath.c_str(), NativeAnalogProtocol::LogitechRapid)) return;

    std::uint64_t unmapped = 0;
    bool mismatch = false;
    const auto unmappedId = [&](std::uint8_t id) {
        ++unmapped;
        if (!g_unmappedLogged.test(id)) { g_unmappedLogged.set(id); SupportLog_Event("logitech.unmapped_key", id); }
    };
    // Version 2 (frames): the raw capture keeps running until the capture
    // store is full, so later logs complete kKeyMapV2 without user steps.
    Capture capture;
    capture.full = g_captureFull.load();
    lr::V2FrameAssembler frames;
    lr::V2KeyState keys;
    lr::V2KeyLearner learner;
    // A keyboard outside the catalog has no key table: every key is learned.
    const auto& v2Table = c.generic ? lr::kNoKeyMap : lr::kKeyMapV2;
    const auto& v0Table = c.generic ? lr::kNoKeyMap : lr::kKeyMap;
    learner.table = format.wide ? &v2Table : &v0Table;
    std::array<std::uint16_t, 256> lastDepth{}, v0Depth{};
    std::uint64_t invalid = 0, validFrames = 0;
    g_osTail.store(g_osHead.load()); // key-downs from before this session do not count
    const auto learn = [&](const std::array<std::uint16_t, 256>& depth, ULONGLONG now) {
        for (auto tail = g_osTail.load(); tail != g_osHead.load(std::memory_order_acquire); ++tail) {
            const auto key = g_osKeys[tail % g_osKeys.size()];
            learner.OnKeyDown(key.hid, key.time);
            g_osTail.store(tail + 1);
        }
        learner.Resolve(depth, now, [&](std::uint8_t id, std::uint16_t hid) {
            // Logged with the pair so the static table can take it over.
            SupportLog_Event("logitech.learned_key", (std::uint64_t{id} << 16) | hid);
            if (hid < g_values.size()) {
                g_values[hid].store(lr::ToMilli(depth[id], format.travel));
                g_learnedOwned[hid].store(true);
            }
        });
    };
    const auto onFrameReport = [&](const std::uint8_t* r, std::size_t n) {
        if (!capture.full) {
            capture.Report(r, n);
            if (capture.full) { g_captureFull.store(true); InputTrace_SetKeyCapture(false); }
        }
        observe(r, n);
        const auto result = frames.Feed(index, format.travel, r, n);
        if (result == lr::V2FrameAssembler::Result::Invalid) {
            // Skip a report outside the decoded layout (first one logged with
            // its bytes); stop only when such reports outnumber good frames.
            if (!invalid++) {
                std::uint64_t a = 0, b = 0;
                for (int i = 0; i < 8; ++i) { a = (a << 8) | r[4 + i]; b = (b << 8) | r[12 + i]; }
                SupportLog_Event("logitech.v2_invalid_report", a, SupportLog_Data(b));
            }
            if (invalid > 16 && invalid > validFrames) mismatch = true;
            return;
        }
        if (result != lr::V2FrameAssembler::Result::Frame) return;
        ++validFrames;
        const auto now = GetTickCount64();
        keys.Apply(frames.frame(), [&](std::uint8_t id, std::uint16_t depth) {
            learner.OnDepth(id, lastDepth[id], depth, now);
            lastDepth[id] = depth;
            const auto hid = v2Table[id] ? v2Table[id] : learner.learned[id];
            if (!hid) { if (depth) unmappedId(id); return; }
            g_values[hid].store(lr::ToMilli(depth, format.travel));
        });
        learn(keys.depth, now);
        g_last.store(now); ++g_good;
    };
    const auto onReport = [&](const std::uint8_t* r, std::size_t n) {
        if (format.wide) { onFrameReport(r, n); return; }
        if (c.generic && !capture.full) {
            capture.Report(r, n);
            if (capture.full) { g_captureFull.store(true); InputTrace_SetKeyCapture(false); }
        }
        if (!observe(r, n)) return;
        lr::DepthEvent ev{};
        if (!lr::DecodeDepth(index, format.wide, r, n, &ev)) return;
        if (ev.extraPayload && !g_extraLogged) {
            g_extraLogged = true;
            SupportLog_Event("logitech.extra_payload", r[0]);
        }
        if (!lr::DepthPlausible(ev.depth, format.travel)) { mismatch = true; return; }
        const auto now = GetTickCount64();
        learner.OnDepth(ev.keyId, v0Depth[ev.keyId], ev.depth, now);
        v0Depth[ev.keyId] = ev.depth;
        learn(v0Depth, now);
        const auto hid = v0Table[ev.keyId] ? v0Table[ev.keyId] : learner.learned[ev.keyId];
        if (!hid) { unmappedId(ev.keyId); return; }
        g_values[hid].store(lr::ToMilli(ev.depth, format.travel));
        g_last.store(now); ++g_good;
    };

    g_travel.store(format.travel);
    g_v2.store(format.wide);
    StreamOff off{s, index};
    if ((format.wide || c.generic) && !capture.full) InputTrace_SetKeyCapture(true);
    struct EndKeyCapture { ~EndKeyCapture() { InputTrace_SetKeyCapture(false); } } endKeyCapture;
    if (!Transact(s, lr::Build(index, 3, {1}), p, e, onReport)) return;
    SupportLog_Event("logitech.stream_on", format.travel, SupportLog_Data(format.wide ? 2 : 1));
    g_connected.store(true);
    auto rearm = GetTickCount64() + 2000;
    while (!g_stop.load() && !mismatch) {
        if (!s.Pump(50, onReport)) { Failure("logitech.stream_failed", SupportLog_Win32(GetLastError())); break; }
        const auto now = GetTickCount64();
        if (now >= rearm) {
            // Another program (G HUB) may switch the stream off; keep it on.
            if (!s.Send(lr::Build(index, 3, {1}))) { Failure("logitech.rearm_failed", SupportLog_Win32(GetLastError())); break; }
            rearm = now + 2000;
        }
    }
    if (mismatch) {
        // A depth beyond the full travel: the format guess is wrong. Stop
        // rather than publish wrong depths; wait for a device change.
        Failure("logitech.depth_format_mismatch", SupportLog_Protocol((std::uint64_t{version} << 16) | format.travel));
        g_formatBlocked.store(true);
    }
    if (format.wide || c.generic) {
        capture.FlushRepeats();
        SupportLog_Event("logitech.raw_capture", capture.records, SupportLog_Data(capture.full ? 1 : 0));
        SupportLog_Event("logitech.v2_frames", validFrames, SupportLog_Data(invalid));
    }
    logShape();
    if (unmapped) SupportLog_Event("logitech.unmapped_events", unmapped);
}

// Read-only, quiet probe of a Logitech device outside the catalog: root
// getFeature(0x1B08). Not counted as a failure (mice and receivers answer
// with an error or not at all); nothing else is sent.
bool GenericHasAnalog(const Candidate& c) {
    Session s(c);
    if (!s.Valid()) return false;
    const auto request = lr::GetFeature(lr::kAnalogFeature);
    if (!s.Send(request)) return false;
    const auto end = GetTickCount64() + 300;
    std::array<std::uint8_t, 16> params{}; std::uint8_t error = 0;
    lr::ReplyKind kind = lr::ReplyKind::None;
    while (kind == lr::ReplyKind::None && GetTickCount64() < end && !g_stop.load())
        if (!s.Pump(20, [&](const std::uint8_t* r, std::size_t n) { kind = lr::MatchReply(request, r, n, &params, &error); }))
            return false;
    const bool analog = kind == lr::ReplyKind::Reply && params[0] != 0;
    if (analog) SupportLog_Event("logitech.generic_analog", c.pid, SupportLog_Data((std::uint64_t{params[0]} << 8) | params[2]));
    return analog;
}

unsigned __stdcall Worker(void*) noexcept {
    try {
        while (!g_stop.load()) {
            if (g_genericReset.exchange(false)) { g_genericRejected.clear(); g_genericFound.store(false); }
            const auto candidates = Enumerate();
            std::vector<Candidate> exact, generic;
            for (const auto& c : candidates) {
                if (!c.generic) { exact.push_back(c); continue; }
                const auto key = InterfaceKey(c.longPath);
                if (std::find(g_genericRejected.begin(), g_genericRejected.end(), key) == g_genericRejected.end())
                    generic.push_back(c);
            }
            // Catalog models first; other Logitech devices (mice, receivers,
            // headsets) count as present only after they report 0x1B08.
            g_present.store(!exact.empty() || g_genericFound.load());
            if (exact.size() == 1) Run(exact[0]);
            else if (exact.size() > 1) SupportLog_Event("logitech.multiple_devices", exact.size());
            else for (const auto& c : generic) {
                if (g_stop.load()) break;
                if (!GenericHasAnalog(c)) { g_genericRejected.push_back(InterfaceKey(c.longPath)); continue; }
                g_genericFound.store(true); g_present.store(true);
                Run(c);
                break;
            }
            const bool waitForChange = exact.empty() && !g_genericFound.load();
            if (!g_stop.load()) WaitForSingleObject(g_wake, waitForChange ? INFINITE : 5000);
        }
    } catch (...) { Failure("logitech.worker_exception", SupportLog_Protocol(1)); }
    Clear(); g_running.store(false); return 0;
}
bool Start() {
    std::lock_guard<std::mutex> l(g_service);
    if (g_thread) return g_running.load();
    g_stop.store(false);
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
    const auto wait = WaitForSingleObject(g_thread, 5000);
    if (wait != WAIT_OBJECT_0)
        return NativeAnalogBackendStopFailed(generation, halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, WAIT_TIMEOUT);
    CloseHandle(g_thread); g_thread = nullptr; CloseHandle(g_wake); g_wake = nullptr; Clear();
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
    std::lock_guard<std::mutex> l(g_service);
    g_formatBlocked.store(false); // a device change may bring other firmware
    // Re-probe other Logitech devices after a change (worker reads the list
    // only between enumerations; clearing it here is the documented reset).
    g_genericReset.store(true);
    if (g_wake) SetEvent(g_wake);
}
bool Present() { return g_present.load(); }
bool Connected() { return g_connected.load(); }
std::array<bool, 0x500> OwnedBy(const std::array<std::uint16_t, 256>& map) {
    std::array<bool, 0x500> owned{};
    for (const auto hid : map) if (hid < owned.size()) owned[hid] = true;
    owned[0] = false;
    return owned;
}
const std::array<bool, 0x500> kOwned = OwnedBy(lr::kKeyMap), kOwnedV2 = OwnedBy(lr::kKeyMapV2);
unsigned MappedCount(const std::array<std::uint16_t, 256>& map) {
    unsigned count = 0;
    for (const auto hid : map) count += hid != 0;
    return count;
}
const std::array<bool, 0x500> kOwnedNone{};
bool Owns(std::uint16_t hid) {
    const auto& owned = g_generic.load() ? kOwnedNone : g_v2.load() ? kOwnedV2 : kOwned;
    return hid < owned.size() && (owned[hid] || g_learnedOwned[hid].load()) && Connected();
}
std::uint16_t Get(std::uint16_t hid) { return hid < g_values.size() && Owns(hid) ? g_values[hid].load() : 0; }
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if (!out) return;
    *out = {};
    const auto& model = *g_model.load();
    out->present = Present(); out->connected = Connected();
    const bool generic = g_generic.load();
    out->genericProtocol = out->connected && generic;
    out->vendorId = lr::kVendorId; out->productId = generic ? g_pid.load() : model.pid; out->usagePage = 0xff00;
    out->inputReportBytes = lr::kLongBytes; out->outputReportBytes = lr::kLongBytes;
    out->successfulUpdates = g_good.load(); out->failedUpdates = g_bad.load();
    out->nominalRawLevels = g_travel.load() + 1;
    const auto& map = generic ? lr::kNoKeyMap : g_v2.load() ? lr::kKeyMapV2 : lr::kKeyMap;
    unsigned learned = 0;
    for (const auto& v : g_learnedOwned) learned += v.load() ? 1u : 0u;
    out->mappedKeys = Connected() ? MappedCount(map) + learned : 0;
    for (std::uint16_t hid = 1; hid < g_values.size(); ++hid) if (Get(hid)) ++out->activeKeys;
    const auto last = g_last.load();
    out->lastUpdateAgeMs = last ? static_cast<unsigned>(std::min<ULONGLONG>(0xffffffff, GetTickCount64() - last)) : 0;
    wcscpy_s(out->deviceName, model.name);
    if (out->connected && model.layoutProduct)
        out->verifiedLayoutToken = halljoy::layout_identity::Token("logitech-rapid", model.layoutProduct);
    swprintf_s(out->status, L"%ls: %ls", model.shortName,
        Connected() ? (g_v2.load() ? L"analog depth stream active (0.01 mm steps)" : L"analog depth stream active (0.1 mm steps)") :
        g_capturing.load() ? L"recording analog data for the log: press any keys, then Open log" :
        !Present() ? L"not connected" :
        g_captureFull.load() ? L"analog data recorded: Open log and send it" :
        g_formatBlocked.load() ? L"analog event format not supported yet; see log" :
        L"checking HID++ analog feature; see log if this persists");
}
} // namespace

void LogitechRapid_ObserveRawKey(void* device, std::uint16_t hid, bool down) noexcept {
    if (!down || !hid || !g_v2.load() || !Connected()) return;
    // Only key-downs of the Logitech keyboard itself (device name has its VID/PID).
    static void* lastDevice = nullptr;
    static bool lastMatch = false;
    if (device != lastDevice) {
        lastDevice = device;
        lastMatch = false;
        wchar_t name[512]{};
        UINT size = static_cast<UINT>(std::size(name));
        if (device && GetRawInputDeviceInfoW(static_cast<HANDLE>(device), RIDI_DEVICENAME, name, &size) != static_cast<UINT>(-1)) {
            wchar_t want[32]{};
            swprintf_s(want, L"vid_%04x&pid_%04x", lr::kVendorId, g_pid.load());
            lastMatch = Lower(name).find(want) != std::wstring::npos;
        }
    }
    if (!lastMatch) return;
    const auto head = g_osHead.load();
    if (head - g_osTail.load() >= g_osKeys.size()) return; // worker behind: drop
    g_osKeys[head % g_osKeys.size()] = OsKey{hid, GetTickCount64()};
    g_osHead.store(head + 1, std::memory_order_release);
}

const NativeAnalogBackendDescriptor& LogitechRapid_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor),
        "logitech_rapid", L"Logitech G RAPID (HID++ analog)", NativeAnalogProtocol::LogitechRapid,
        NativeAnalogStartPhase::AfterRealtime, NativeAnalogBackendFlag_StreamTransport,
        nullptr, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return d;
}
