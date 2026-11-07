#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <process.h>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "corsair_k70_tkl_protocol.h"
#include "generated/layout_pipeline/identities.h"
#include "hid_io_operation.h"
#include "native_analog_backend.h"
#include "physical_analog_state.h"
#include "realtime_loop.h"
#include "support_log.h"

namespace {
namespace k70 = halljoy::corsair_k70;
constexpr auto kProtocol = NativeAnalogProtocol::CorsairK70Tkl;
constexpr char kBackendId[] = "corsair-k70-tkl";
std::atomic<bool> g_stop{false}, g_present{false}, g_connected{false}, g_busy{false};
std::atomic<std::uint64_t> g_token{0}, g_good{0}, g_bad{0}, g_last{0};
std::atomic<unsigned> g_mapped{0}, g_page{0}, g_usage{0}, g_inputBytes{0}, g_outputBytes{0};
halljoy::physical_analog::Publication g_values;
std::mutex g_service;
HANDLE g_worker = nullptr, g_wake = nullptr;
std::uint64_t g_attempt = 0;

struct Handle {
    HANDLE value;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

struct Discovery {
    std::vector<std::wstring> paths;
    unsigned products = 0;
    DWORD error = 0;
};

void Log(const char* stage, unsigned long code = 0, unsigned detail = 0) {
    char line[250]{};
    _snprintf_s(line, sizeof(line), _TRUNCATE,
        "evidence.k70.stage revision=2 attempt=%llu stage=%s code=%lu detail=%u",
        g_attempt, stage, code, detail);
    SupportLog_Evidence(line, false);
}

Discovery Discover() {
    Discovery found;
    GUID guid{};
    HidD_GetHidGuid(&guid);
    const auto set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        found.error = GetLastError();
        return found;
    }
    struct DeviceList {
        HDEVINFO value;
        ~DeviceList() { SetupDiDestroyDeviceInfoList(value); }
    } guard{set};
    for (DWORD index = 0; index < 512; ++index) {
        SP_DEVICE_INTERFACE_DATA entry{};
        entry.cbSize = sizeof(entry);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &entry)) {
            const auto error = GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) break;
            found.error = error;
            continue;
        }
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &entry, nullptr, 0, &size, nullptr);
        if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 65536) {
            found.error = ERROR_BAD_LENGTH;
            continue;
        }
        std::vector<unsigned char> storage(size);
        auto* info = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        info->cbSize = sizeof(*info);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &entry, info, size, nullptr, nullptr)) {
            found.error = GetLastError();
            continue;
        }
        std::wstring path = info->DevicePath;
        for (auto& c : path) c = static_cast<wchar_t>(towlower(c));
        if (path.find(L"vid_1b1c&pid_2b14&") == std::wstring::npos) continue;
        ++found.products;
        if (path.find(L"&mi_01") != std::wstring::npos)
            found.paths.emplace_back(info->DevicePath);
    }
    return found;
}

enum class CapsResult { Accepted, Retry, Rejected };

CapsResult CheckCaps(HANDLE handle) {
    HIDD_ATTRIBUTES attributes{};
    attributes.Size = sizeof(attributes);
    if (!HidD_GetAttributes(handle, &attributes)) {
        Log("attributes", GetLastError());
        return CapsResult::Retry;
    }
    if (attributes.VendorID != k70::kVid || attributes.ProductID != k70::kPid) {
        Log("usb_identity", attributes.VendorID, attributes.ProductID);
        return CapsResult::Rejected;
    }
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed)) {
        Log("preparsed", GetLastError());
        return CapsResult::Retry;
    }
    HIDP_CAPS caps{};
    const auto result = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if (result != HIDP_STATUS_SUCCESS) {
        Log("caps_query", static_cast<unsigned long>(result));
        return CapsResult::Retry;
    }
    g_page.store(caps.UsagePage);
    g_usage.store(caps.Usage);
    g_inputBytes.store(caps.InputReportByteLength);
    g_outputBytes.store(caps.OutputReportByteLength);
    Log("caps_usage", caps.UsagePage, caps.Usage);
    Log("caps_bytes", caps.InputReportByteLength, caps.OutputReportByteLength);
    return caps.UsagePage >= 0xff00 && caps.InputReportByteLength == k70::kReportBytes &&
        caps.OutputReportByteLength == k70::kReportBytes ? CapsResult::Accepted : CapsResult::Rejected;
}

void Clear() {
    const bool connected = g_connected.exchange(false);
    g_token.store(0);
    g_mapped.store(0);
    g_last.store(0);
    const bool changed = g_values.Clear();
    if (connected || changed) RealtimeLoop_NotifyInputChanged();
}

class Transport {
public:
    explicit Transport(HANDLE handle) : read_(handle), write_(handle) {}

    bool Cancelled() const { return g_stop.load(); }

    bool Read(k70::Property property, k70::Frame& reply, std::uint8_t subcommand = 0) {
        if (Cancelled()) return false;
        const auto request = k70::Request(property, subcommand);
        const auto deadline = GetTickCount64() + (property == k70::Property::Snapshot ? 100 : 250);
        if (!Transfer(write_, write_.StartWrite(request.data(), static_cast<DWORD>(request.size()), &error_),
                      deadline, "write", property, subcommand)) return false;
        if (!Transfer(read_, read_.StartRead(reply.data(), static_cast<DWORD>(reply.size()), &error_),
                      deadline, "read", property, subcommand)) return false;
        if (k70::ReplyHeader(reply)) return true;
        // Only the protocol envelope is reported. The rest may contain key values.
        Failure("prefix", property, subcommand, 0, static_cast<unsigned>(
            reply[0] | unsigned(reply[1]) << 8 | unsigned(reply[2]) << 16 | unsigned(reply[3]) << 24));
        return false;
    }

private:
    HidIoOperation read_, write_;
    DWORD error_ = 0;

    static void Failure(const char* stage, k70::Property property, unsigned subcommand, DWORD error, unsigned value) {
        char line[280]{};
        _snprintf_s(line, sizeof(line), _TRUNCATE,
            "evidence.k70.exchange revision=2 attempt=%llu stage=%s property=%u sub=%u win32=%lu detail=%u",
            g_attempt, stage, static_cast<unsigned>(property), subcommand, error, value);
        SupportLog_Evidence(line, false);
    }

    bool Transfer(HidIoOperation& operation, HidIoOperation::StartResult start, std::uint64_t deadline,
                  const char* stage, k70::Property property, unsigned subcommand) {
        DWORD bytes = 0;
        if (start == HidIoOperation::StartResult::Completed) {
            operation.Finish(&bytes, &error_, false);
        } else if (start == HidIoOperation::StartResult::Pending) {
            for (;;) {
                const auto now = GetTickCount64();
                if (Cancelled() || now >= deadline) {
                    error_ = Cancelled() ? ERROR_CANCELLED : WAIT_TIMEOUT;
                    break;
                }
                HANDLE events[] = {operation.Event(), g_wake};
                const auto result = WaitForMultipleObjects(2, events, FALSE, static_cast<DWORD>(deadline - now));
                if (result == WAIT_OBJECT_0) {
                    operation.Finish(&bytes, &error_, false);
                    break;
                }
                if (result == WAIT_OBJECT_0 + 1) continue;
                error_ = result == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
                break;
            }
            if (error_) operation.CancelAndDrain(nullptr, nullptr);
        }
        if (Cancelled()) return false;
        if (!error_ && bytes == k70::kReportBytes) return true;
        Failure(stage, property, subcommand, error_ ? error_ : ERROR_BAD_LENGTH, bytes);
        return false;
    }
};

bool Calibration(Transport& io, const k70::Layout& layout, k70::Profile& profile) {
    k70::Frame reply{};
    k70::Samples rest{}, bottom{}, verify{};
    const auto get = [&](std::uint8_t sub, k70::Samples& out) {
        if (!io.Read(k70::Property::Calibration, reply, sub)) return false;
        if (k70::ReadWords(reply, layout, out)) return true;
        Log("calibration_tail", sub, layout.count);
        return false;
    };
    if (!get(3, rest) || !get(2, bottom) || !get(3, verify)) return false;
    if (rest != verify) {
        Log("calibration_changed");
        return false;
    }
    const auto invalid = k70::BuildProfile(layout, rest, bottom, profile);
    Log("calibration", invalid, layout.count - 2);
    return invalid == 0;
}

struct TravelStats {
    k70::Samples previous{}, previousRaw{};
    std::uint64_t start = GetTickCount64(), next = start + 30000;
    std::uint64_t frames = 0, rawChanges = 0, changes = 0, releases = 0;
    unsigned maximumActive = 0, milestones = 0;

    void LogSummary() const {
        if (!frames) return;
        const auto elapsed = std::max<std::uint64_t>(1, GetTickCount64() - start);
        char line[280]{};
        _snprintf_s(line, sizeof(line), _TRUNCATE,
            "evidence.k70.travel revision=2 attempt=%llu frames=%llu elapsed_ms=%llu hz_x10=%llu raw_changes=%llu value_changes=%llu releases=%llu max_active=%u",
            g_attempt, frames, elapsed, frames * 10000 / elapsed, rawChanges, changes, releases, maximumActive);
        SupportLog_Evidence(line, false);
    }
};

enum class Outcome { Retry, ProofFailed, Rejected };

Outcome Run(const std::wstring& path) {
    if (NativeAnalogRouting_IsClaimed(path.c_str()) && !NativeAnalogRouting_IsClaimedBy(path.c_str(), kProtocol)) {
        Log("foreign_claim");
        return Outcome::Rejected;
    }
    ++g_attempt;
    Handle handle{CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_FLAG_OVERLAPPED, nullptr)};
    const auto openError = handle.value == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    g_busy.store(NativeAnalog_IsAccessBusyError(openError));
    if (openError) {
        static DWORD lastError = ERROR_SUCCESS;
        if (openError != lastError || g_attempt <= 2) Log("open", openError);
        lastError = openError;
        return Outcome::Retry;
    }
    const auto caps = CheckCaps(handle.value);
    if (caps != CapsResult::Accepted) {
        ++g_bad;
        return caps == CapsResult::Rejected ? Outcome::Rejected : Outcome::ProofFailed;
    }
    if (!HidD_FlushQueue(handle.value)) {
        Log("flush", GetLastError());
        return Outcome::ProofFailed;
    }
    Transport io(handle.value);
    k70::Frame reply{};
    const auto property = [&](k70::Property selector, std::uint32_t& value) {
        if (!io.Read(selector, reply)) return false;
        value = k70::U32(reply, k70::kDataOffset);
        return true;
    };
    std::uint32_t vid = 0, pid = 0, firmware = 0, region = 0;
    if (!property(k70::Property::Vendor, vid) || !property(k70::Property::Product, pid))
        return io.Cancelled() ? Outcome::Retry : Outcome::ProofFailed;
    Log("identity", vid, pid);
    if (vid != k70::kVid || pid != k70::kPid) return Outcome::Rejected;
    if (!property(k70::Property::Firmware, firmware) || !property(k70::Property::Region, region))
        return io.Cancelled() ? Outcome::Retry : Outcome::ProofFailed;
    Log("firmware_region", firmware, region);
    const auto* layout = k70::FindLayout(region);
    if (firmware != k70::kFirmware || !layout) {
        Log(firmware != k70::kFirmware ? "firmware_rejected" : "region_rejected", firmware, region);
        return Outcome::Rejected;
    }
    const auto token = halljoy::layout_identity::Token(kBackendId, layout->product);
    if (!token) {
        Log("missing_preset", region);
        return Outcome::Rejected;
    }
    k70::Profile profile{};
    if (!Calibration(io, *layout, profile)) return io.Cancelled() ? Outcome::Retry : Outcome::ProofFailed;
    for (std::size_t i = 0; i < layout->count; ++i) {
        if (!layout->hid[i]) continue;
        if (!g_values.Bind(static_cast<std::uint8_t>(i + 1), layout->hid[i])) {
            Log("map_rejected", region);
            return Outcome::Rejected;
        }
    }
    TravelStats stats{};
    while (!io.Cancelled()) {
        if (!io.Read(k70::Property::Snapshot, reply)) break;
        k70::Samples raw{};
        if (!k70::ReadWords(reply, *layout, raw)) {
            Log("snapshot_tail", region, layout->count);
            break;
        }
        unsigned zero = 0, outOfRange = 0;
        for (std::size_t i = 0; i < layout->count; ++i) {
            if (!layout->hid[i]) continue;
            zero += raw[i] == 0;
            outOfRange += raw[i] >= 0x4000;
        }
        if (zero || outOfRange) {
            Log("snapshot_invalid", zero, outOfRange);
            break;
        }
        if (!stats.frames && !NativeAnalogRouting_Claim(k70::kVid, k70::kPid, path.c_str(), kProtocol) &&
            !NativeAnalogRouting_IsClaimedBy(path.c_str(), kProtocol)) {
            Log("claim");
            return Outcome::Rejected;
        }
        const auto now = GetTickCount64();
        unsigned active = 0;
        bool changed = false;
        for (std::size_t i = 0; i < layout->count; ++i) {
            if (!layout->hid[i]) continue;
            const auto milli = k70::Normalize(raw[i], profile, i);
            active += milli != 0;
            stats.rawChanges += stats.frames && stats.previousRaw[i] != raw[i];
            stats.changes += stats.previous[i] != milli;
            stats.releases += stats.previous[i] != 0 && milli == 0;
            stats.previous[i] = milli;
            stats.previousRaw[i] = raw[i];
            changed = g_values.Publish(static_cast<std::uint8_t>(i + 1), milli, now) || changed;
        }
        if (!stats.frames++) {
            g_mapped.store(layout->count - 2);
            g_token.store(token);
            g_connected.store(true);
            Log("live", region, layout->count - 2);
        }
        g_last.store(now);
        ++g_good;
        if (changed) RealtimeLoop_NotifyInputChanged();
        stats.maximumActive = std::max(stats.maximumActive, active);
        const unsigned milestones = 1u | (stats.rawChanges ? 2u : 0u) | (active ? 4u : 0u) |
            (stats.releases ? 8u : 0u) | (stats.maximumActive >= 2 ? 16u : 0u);
        if ((milestones & ~stats.milestones) || now >= stats.next) {
            stats.LogSummary();
            stats.milestones |= milestones;
            stats.next = now + 30000;
        }
    }
    const bool cancelled = io.Cancelled();
    if (!cancelled) ++g_bad;
    stats.LogSummary();
    return cancelled || stats.frames ? Outcome::Retry : Outcome::ProofFailed;
}

unsigned __stdcall Worker(void*) noexcept {
    try {
        std::wstring rejected, previousPath;
        unsigned proofFailures = 0, previousProducts = ~0u;
        std::size_t previousControls = ~std::size_t{0};
        DWORD previousError = ERROR_SUCCESS;
        while (!g_stop.load()) {
            const auto found = Discover();
            g_present.store(found.products != 0);
            if (!found.error && std::find(found.paths.begin(), found.paths.end(), rejected) == found.paths.end())
                rejected.clear();
            if (found.products != previousProducts || found.paths.size() != previousControls || found.error != previousError) {
                char line[240]{};
                _snprintf_s(line, sizeof(line), _TRUNCATE,
                    "evidence.k70.discovery revision=2 usb=1B1C:2B14 collections=%u if1=%zu win32=%lu",
                    found.products, found.paths.size(), found.error);
                SupportLog_Evidence(line, false);
                previousProducts = found.products;
                previousControls = found.paths.size();
                previousError = found.error;
            }
            bool retry = found.error != 0;
            if (found.paths.size() == 1 && found.paths.front() != rejected) {
                const auto& path = found.paths.front();
                if (path != previousPath) proofFailures = 0;
                previousPath = path;
                const auto result = Run(path);
                const bool wasLive = g_connected.load();
                Clear();
                if (wasLive) proofFailures = 0;
                if (result == Outcome::Rejected || (result == Outcome::ProofFailed && ++proofFailures >= 2)) {
                    rejected = path;
                    Log("wait_for_reconnect", proofFailures);
                } else {
                    retry = true;
                }
            } else {
                if (found.paths.empty() && !found.error) previousPath.clear();
                g_busy.store(false);
            }
            if (!g_stop.load()) WaitForSingleObject(g_wake, retry ? 1000 : INFINITE);
        }
    } catch (...) {
        ++g_bad;
        Log("worker_exception");
    }
    Clear();
    g_busy.store(false);
    g_present.store(false);
    return 0;
}

bool Start() {
    std::lock_guard lock(g_service);
    if (g_worker) return WaitForSingleObject(g_worker, 0) == WAIT_TIMEOUT;
    g_stop.store(false);
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_wake) return false;
    g_worker = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, nullptr));
    if (g_worker) return true;
    CloseHandle(g_wake);
    g_wake = nullptr;
    return false;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation) {
    std::lock_guard lock(g_service);
    g_stop.store(true);
    if (!g_worker) return NativeAnalogBackendStopJoined(generation);
    SetEvent(g_wake);
    const auto wait = WaitForSingleObject(g_worker, 3000);
    if (wait != WAIT_OBJECT_0) return NativeAnalogBackendStopFailed(generation,
        halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_worker);
    CloseHandle(g_wake);
    g_worker = nullptr;
    g_wake = nullptr;
    return NativeAnalogBackendStopJoined(generation);
}

void Notify() {
    // WM_DEVICECHANGE can describe an unrelated device, including the virtual pad.
    // A live session follows its own HID handle and reports actual I/O failure.
    std::unique_lock lock(g_service, std::try_to_lock);
    if (lock.owns_lock() && g_wake) SetEvent(g_wake);
}

bool Present() { return g_present.load(); }
bool Connected() { return g_connected.load(); }
bool Owns(std::uint16_t hid) { return Connected() && g_values.Owns(hid); }
std::uint16_t Get(std::uint16_t hid) {
    return Owns(hid) ? g_values.Read(hid, GetTickCount64(), 100).milli : 0;
}
void Telemetry(NativeAnalogBackendTelemetry* out) {
    if (!out) return;
    *out = {};
    out->present = Present();
    out->connected = Connected();
    out->accessBusy = out->present && !out->connected && g_busy.load();
    out->vendorId = k70::kVid;
    out->productId = k70::kPid;
    out->usagePage = static_cast<std::uint16_t>(g_page.load());
    out->usage = static_cast<std::uint16_t>(g_usage.load());
    out->inputReportBytes = g_inputBytes.load();
    out->outputReportBytes = g_outputBytes.load();
    out->nominalRawLevels = 41;
    out->successfulUpdates = g_good.load();
    out->failedUpdates = g_bad.load();
    const auto now = GetTickCount64(), last = g_last.load();
    if (last && now >= last)
        out->lastUpdateAgeMs = static_cast<std::uint32_t>(std::min<std::uint64_t>(UINT32_MAX, now - last));
    if (out->connected) {
        out->verifiedLayoutToken = g_token.load();
        out->mappedKeys = g_mapped.load();
        out->activeKeys = g_values.Active(now);
    }
    wcscpy_s(out->deviceName, L"CORSAIR K70 PRO TKL");
    swprintf_s(out->status, L"K70 PRO TKL: %ls", out->connected ? L"stock analog" :
        out->accessBusy ? L"control interface in use" : out->present ? L"see HallJoy.log" : L"not present");
}
} // namespace

const NativeAnalogBackendDescriptor& CorsairK70Tkl_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), kBackendId,
        L"CORSAIR K70 PRO TKL", kProtocol, NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_PolledTransport | NativeAnalogBackendFlag_ReadOnlyProbe,
        nullptr, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return descriptor;
}
