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
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "redsquare_alumix68_backend.h"
#include "redsquare_alumix68_protocol.h"
#include "hid_io_operation.h"
#include "input_trace.h"
#include "native_analog_routing.h"
#include "physical_analog_state.h"
#include "support_log.h"
#include "generated/layout_pipeline/identities.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

// Red Square Alumix 68 (RSQ-20058, 0C45:80A2). The reviewed analog path reads the
// current per-slot key-depth table from RAM with the ordinary vendor read command
// (0x16 + offset 0x200), without enabling calibration or the selected-key stream.
// That table's RAM offset is specific to firmware v1.30, so the backend verifies
// an exact firmware fingerprint over HID (scan map + command dispatcher window,
// command 0x12) before reading it, and refuses to connect on any other firmware
// (owner decision 2026-10-06, docs/current/ALUMIX68_RSQ20058_2026-10-06.md).
namespace {
namespace proto = halljoy::redsquare_alumix68;

constexpr std::uint64_t kRetentionMs = 1000;

struct Handle {
    HANDLE v = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : v(h) {}
    ~Handle() { if (v && v != INVALID_HANDLE_VALUE) CloseHandle(v); }
    Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return v && v != INVALID_HANDLE_VALUE; }
};

std::mutex g_service;
HANDLE g_thread = nullptr, g_wake = nullptr;
std::atomic<bool> g_stop{false}, g_running{false}, g_present{false}, g_connected{false};
std::atomic<bool> g_blocked{false};  // exact device found, firmware not recognized
std::atomic<std::uint64_t> g_good{0}, g_bad{0}, g_last{0};
halljoy::physical_analog::Publication g_depth;

void Clear() {
    g_connected.store(false, std::memory_order_release);
    g_depth.Clear();
}
void Failure(const char* stage, SupportLogDetail detail = {}) {
    ++g_bad;
    SupportLog_Event(stage, 0, detail);
}
void BindSlots() {
    g_depth.Clear();
    for (std::size_t slot = 0; slot < proto::kSlots; ++slot)
        if (const auto hid = proto::kSlotHid[slot])
            g_depth.Bind(static_cast<std::uint8_t>(slot + 1), hid);
}

std::vector<std::wstring> Enumerate() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    struct FreeSet { HDEVINFO s; ~FreeSet() { SetupDiDestroyDeviceInfoList(s); } } freeSet{set};
    std::vector<std::wstring> out;
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
        if (!HidD_GetAttributes(meta.v, &a) || a.VendorID != proto::kVendorId ||
            a.ProductID != proto::kProductId) continue;
        PHIDP_PREPARSED_DATA data = nullptr;
        if (!HidD_GetPreparsedData(meta.v, &data)) continue;
        HIDP_CAPS caps{}; const auto status = HidP_GetCaps(data, &caps);
        HidD_FreePreparsedData(data);
        if (status != HIDP_STATUS_SUCCESS) continue;
        if (proto::ExactCollection(caps.UsagePage, caps.Usage,
                                   caps.InputReportByteLength, caps.OutputReportByteLength))
            out.push_back(detail->DevicePath);
    }
    return out;
}

// One overlapped request/reply transaction on the vendor collection.
class Session {
    Handle handle_;
    std::unique_ptr<HidIoOperation> read_, write_;
    std::array<std::uint8_t, proto::kReportBytes> buffer_{};

public:
    explicit Session(const std::wstring& path) {
        handle_.v = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (handle_) {
            HidD_SetNumInputBuffers(handle_.v, 64);
            read_ = std::make_unique<HidIoOperation>(handle_.v);
            write_ = std::make_unique<HidIoOperation>(handle_.v);
        }
    }
    bool Valid() const { return handle_ && read_ && write_; }

    bool Send(const std::array<std::uint8_t, proto::kReportBytes>& request) {
        DWORD error = 0, done = 0;
        auto copy = request;
        const auto start = write_->StartWrite(copy.data(),
            static_cast<DWORD>(copy.size()), &error);
        if (start == HidIoOperation::StartResult::Failed) { SetLastError(error); return false; }
        if (start == HidIoOperation::StartResult::Pending && write_->Wait(300) != WAIT_OBJECT_0) {
            write_->CancelAndDrain(&done, &error); SetLastError(WAIT_TIMEOUT); return false;
        }
        if (!write_->Finish(&done, &error, false)) { SetLastError(error); return false; }
        return true;
    }

    // Reads the matching reply to one read command, skipping unrelated frames.
    const std::uint8_t* Transact(std::uint8_t command, std::uint8_t length,
                                 std::uint16_t offset) {
        std::array<std::uint8_t, proto::kReportBytes> request{};
        if (!proto::MakeReadRequest(command, length, offset, request)) return nullptr;
        if (!Send(request)) { Failure("alumix68.write_failed", SupportLog_Win32(GetLastError())); return nullptr; }
        const auto deadline = GetTickCount64() + 300;
        while (!g_stop.load() && GetTickCount64() < deadline) {
            DWORD error = 0, done = 0;
            const auto start = read_->StartRead(buffer_.data(),
                static_cast<DWORD>(buffer_.size()), &error);
            if (start == HidIoOperation::StartResult::Failed) {
                Failure("alumix68.read_failed", SupportLog_Win32(error)); return nullptr;
            }
            if (start == HidIoOperation::StartResult::Pending) {
                const auto wait = read_->Wait(static_cast<DWORD>(
                    std::max<std::int64_t>(1, deadline - GetTickCount64())));
                if (wait != WAIT_OBJECT_0) { read_->CancelAndDrain(&done, &error); continue; }
            }
            if (!read_->Finish(&done, &error, false)) {
                Failure("alumix68.read_failed", SupportLog_Win32(error)); return nullptr;
            }
            if (const auto* payload = proto::ParseReadReply(command, length, offset,
                                                            buffer_.data(), done))
                return payload;
            // Unrelated frame on this collection: keep waiting for the reply.
        }
        return nullptr;
    }

    // Reads `length` bytes from command/base at `offset` into out.
    bool Read(std::uint8_t command, std::uint16_t offset, std::uint8_t* out,
              std::size_t length) {
        std::size_t done = 0;
        while (done < length) {
            const auto chunk = static_cast<std::uint8_t>(
                std::min<std::size_t>(proto::kMaxChunk, length - done));
            const auto* payload = Transact(command,
                chunk, static_cast<std::uint16_t>(offset + done));
            if (!payload) return false;
            std::memcpy(out + done, payload, chunk);
            done += chunk;
        }
        return true;
    }
};

// Exact firmware fingerprint over HID: the immutable scan map plus the command
// dispatcher window. Both come from the pinned v1.30 image and are compared
// byte for byte. A firmware that could move the depth table changes the
// dispatcher window and fails this gate.
bool VerifyFirmware(Session& session) {
    std::array<std::uint8_t, proto::kSlots> scan{};
    if (!session.Read(proto::kCmdReadFlash, proto::kScanMapOffset, scan.data(), scan.size()))
        return false;
    std::array<std::uint8_t, proto::kCodeFp.size()> code{};
    if (!session.Read(proto::kCmdReadFlash, proto::kCodeFpOffset, code.data(), code.size()))
        return false;
    const bool scanOk = std::equal(scan.begin(), scan.end(), proto::kScanMap.begin());
    const bool codeOk = std::equal(code.begin(), code.end(), proto::kCodeFp.begin());
    if (scanOk && codeOk) return true;
    // Record a compact fingerprint so an unrecognized version can be added later.
    std::uint64_t scanSig = 0, codeSig = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        scanSig = (scanSig << 8) | scan[i];
        codeSig = (codeSig << 8) | code[i];
    }
    SupportLog_Event("alumix68.firmware_unknown", (scanOk ? 2u : 0u) | (codeOk ? 1u : 0u),
                     SupportLog_Protocol(scanSig));
    SupportLog_Event("alumix68.firmware_code_head", codeSig);
    return false;
}

void Run(const std::wstring& path) {
    if (g_blocked.load()) return;  // unrecognized firmware; wait for a device change
    Clear();
    struct End { ~End() { Clear(); } } end;
    BindSlots();
    Session session(path);
    if (!session.Valid()) {
        Failure("alumix68.open_failed", SupportLog_Win32(GetLastError()));
        return;
    }
    if (!VerifyFirmware(session)) {
        // Known-device, unknown firmware: show the yellow notice (present, not
        // connected) and stop until the hardware changes. Never read the RAM
        // depth table on an unverified version.
        g_present.store(true, std::memory_order_release);
        g_blocked.store(true, std::memory_order_release);
        return;
    }
    if (!NativeAnalogRouting_Claim(proto::kVendorId, proto::kProductId, path.c_str(),
            NativeAnalogProtocol::RedSquareAlumix68) &&
        !NativeAnalogRouting_IsClaimedBy(path.c_str(),
            NativeAnalogProtocol::RedSquareAlumix68)) return;
    SupportLog_Event("alumix68.connected", proto::kMappedSlots,
                     SupportLog_Data(proto::kFullScaleStroke));
    g_present.store(true, std::memory_order_release);

    std::array<std::uint8_t, proto::kDepthBytes> depth{};
    bool firstFrame = true;
    while (!g_stop.load()) {
        bool ok = true;
        for (const auto& chunk : proto::kDepthChunks) {
            const auto* payload = session.Transact(proto::kCmdReadRam, chunk.length, chunk.offset);
            if (!payload) { ok = false; break; }
            std::memcpy(depth.data() + chunk.firstSlot * 2u, payload, chunk.length);
        }
        if (!ok) { Failure("alumix68.stream_failed", SupportLog_Win32(GetLastError())); break; }
        const auto now = GetTickCount64();
        for (std::size_t slot = 0; slot < proto::kSlots; ++slot) {
            const auto hid = proto::kSlotHid[slot];
            if (!hid) continue;
            const auto stroke = proto::SlotStroke(depth.data(), slot);
            const auto milli = proto::ToMilli(stroke);
            g_depth.Publish(static_cast<std::uint8_t>(slot + 1), milli, now);
            if (InputTrace_IsBound(hid))
                InputTrace_Source("alumix68", static_cast<unsigned>(slot), 0, stroke, milli, hid);
        }
        g_last.store(now);
        ++g_good;
        if (firstFrame) { g_connected.store(true, std::memory_order_release); firstFrame = false; }
    }
}

unsigned __stdcall Worker(void*) noexcept {
    try {
        while (!g_stop.load()) {
            const auto devices = Enumerate();
            if (devices.empty()) {
                // Device gone: drop any unknown-firmware block so a replug re-checks.
                g_present.store(false, std::memory_order_release);
                g_blocked.store(false, std::memory_order_release);
            } else if (devices.size() == 1) {
                Run(devices.front());  // Run owns present/connected while a device is here.
            } else {
                g_present.store(false, std::memory_order_release);
                SupportLog_Event("alumix68.multiple_devices", devices.size());
            }
            if (!g_stop.load()) WaitForSingleObject(g_wake, devices.empty() ? INFINITE : 2000);
        }
    } catch (...) { Failure("alumix68.worker_exception", SupportLog_Protocol(1)); }
    Clear();
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
    g_good.store(0); g_bad.store(0); g_last.store(0);
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
        return NativeAnalogBackendStopFailed(generation,
            halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, WAIT_TIMEOUT);
    CloseHandle(g_thread); g_thread = nullptr; CloseHandle(g_wake); g_wake = nullptr;
    Clear();
    return NativeAnalogBackendStopJoined(generation);
}
void Notify() {
    std::lock_guard<std::mutex> l(g_service);
    g_blocked.store(false);  // a device change may bring recognized firmware
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
    out->vendorId = proto::kVendorId;
    out->productId = proto::kProductId;
    out->usagePage = 0xff68;
    out->usage = 0x61;
    out->inputReportBytes = static_cast<std::uint32_t>(proto::kReportBytes);
    out->outputReportBytes = static_cast<std::uint32_t>(proto::kReportBytes);
    out->nominalRawLevels = proto::kFullScaleStroke + 1u;
    out->mappedKeys = Connected() ? static_cast<std::uint32_t>(proto::kMappedSlots) : 0;
    out->activeKeys = Connected() ? g_depth.Active(GetTickCount64()) : 0;
    out->successfulUpdates = g_good.load();
    out->failedUpdates = g_bad.load();
    const auto last = g_last.load(), now = GetTickCount64();
    out->lastUpdateAgeMs = last && now >= last
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffff, now - last)) : 0;
    if (Connected())
        out->verifiedLayoutToken =
            halljoy::layout_identity::Token("redsquare-alumix68", "RSQ20058-0C45-80A2");
    wcscpy_s(out->deviceName, L"Red Square Alumix 68");
    wcscpy_s(out->status, Connected()
        ? L"Alumix 68: analog depth table active (firmware v1.30)"
        : g_blocked.load()
            ? L"Alumix 68: unrecognized firmware version; not connected (see log)"
            : L"Alumix 68: checking firmware");
}
}  // namespace

const NativeAnalogBackendDescriptor& RedSquareAlumix68_GetNativeBackendDescriptor() {
    static const NativeAnalogBackendDescriptor d{
        kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor),
        "redsquare-alumix68", L"Red Square Alumix 68 (RSQ-20058) analog",
        NativeAnalogProtocol::RedSquareAlumix68,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_PolledTransport,
        nullptr, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return d;
}
