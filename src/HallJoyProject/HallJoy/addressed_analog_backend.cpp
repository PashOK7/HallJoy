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
#include <cstring>
#include <cstdarg>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "addressed_analog_backend.h"
#include "addressed_poll_scheduler.h"
#include "bindings.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "worker_exception_barrier.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr USAGE kProtocolUsagePage = 0xFF60;
constexpr USAGE kProtocolUsage = 0x0061;
constexpr std::size_t kProtocolReportBytes = 64;
constexpr DWORD kResponseTimeoutMs = 8;
constexpr DWORD kLateResponseWindowMs = 20;
constexpr DWORD kReadWaitMs = 100;
constexpr DWORD kReaderStopWaitMs = 750;
constexpr DWORD kReaderForceCloseWaitMs = 750;
constexpr std::uint32_t kMaxConsecutiveResponseMisses = 8;
constexpr std::uint64_t kMaxNoResponseUs = 1000000ull;
constexpr DWORD kProbeMapWindowMs = 35;
constexpr DWORD kProbeResponseWindowMs = 120;
constexpr ULONGLONG kFreshMs = 500;
constexpr ULONGLONG kBindingRefreshMs = 25;
constexpr ULONGLONG kSummaryMs = 10000;
constexpr std::size_t kTraceCount = 256;

// Canonical key-id map shared by the verified QBZ75 firmware and the captured
// compatible 84-key platform map. A successful 0x83 map response overrides this table. Keeping
// the fallback allows QBZ65/QBZ75-class firmware to work even when 0x83 is not
// exposed by a particular updater build.
constexpr std::array<addressed::PollKeyConfig, 82> kCanonicalKeys = {{
    { 0x01, 0x29 }, { 0x02, 0x3A }, { 0x03, 0x3B }, { 0x04, 0x3C },
    { 0x05, 0x3D }, { 0x06, 0x3E }, { 0x07, 0x3F }, { 0x08, 0x40 },
    { 0x09, 0x41 }, { 0x0A, 0x42 }, { 0x0B, 0x43 }, { 0x0C, 0x44 },
    { 0x0D, 0x45 }, { 0x5F, 0x46 }, { 0x64, 0x4C }, { 0x63, 0x4B },
    { 0x0E, 0x35 }, { 0x0F, 0x1E }, { 0x10, 0x1F }, { 0x11, 0x20 },
    { 0x12, 0x21 }, { 0x13, 0x22 }, { 0x14, 0x23 }, { 0x15, 0x24 },
    { 0x16, 0x25 }, { 0x17, 0x26 }, { 0x18, 0x27 }, { 0x19, 0x2D },
    { 0x1A, 0x2E }, { 0x1B, 0x2A }, { 0x1C, 0x2B }, { 0x1D, 0x14 },
    { 0x1E, 0x1A }, { 0x1F, 0x08 }, { 0x20, 0x15 }, { 0x21, 0x17 },
    { 0x22, 0x1C }, { 0x23, 0x18 }, { 0x24, 0x0C }, { 0x25, 0x12 },
    { 0x26, 0x13 }, { 0x27, 0x2F }, { 0x28, 0x30 }, { 0x65, 0x4A },
    { 0x2A, 0x39 }, { 0x2B, 0x04 }, { 0x2C, 0x16 }, { 0x2D, 0x07 },
    { 0x2E, 0x09 }, { 0x2F, 0x0A }, { 0x30, 0x0B }, { 0x31, 0x0D },
    { 0x32, 0x0E }, { 0x33, 0x0F }, { 0x34, 0x33 }, { 0x35, 0x34 },
    { 0x36, 0x28 }, { 0x37, 0xE1 }, { 0x38, 0x1D }, { 0x39, 0x1B },
    { 0x3A, 0x06 }, { 0x3B, 0x19 }, { 0x3C, 0x05 }, { 0x3D, 0x11 },
    { 0x3E, 0x10 }, { 0x3F, 0x36 }, { 0x40, 0x37 }, { 0x66, 0x4D },
    { 0x42, 0x00 }, { 0x43, 0xE0 }, { 0x44, 0xE3 }, { 0x45, 0xE6 },
    { 0x46, 0x2C }, { 0x47, 0x00 }, { 0x48, 0x00 }, { 0x4C, 0x50 },
    { 0x29, 0x31 }, { 0x41, 0x38 }, { 0x4A, 0x52 }, { 0x67, 0x4E },
    { 0x4B, 0x51 }, { 0x4D, 0x4F },
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
        if (this != &other) { reset(); value = other.value; other.value = INVALID_HANDLE_VALUE; }
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

struct DeviceProfile
{
    std::vector<addressed::PollKeyConfig> keys;
    std::wstring source;
    std::size_t mapEntries = 0;
    bool verifiedCalibrationSeed = false;
};

struct ClaimState
{
    bool valid = false;
    HidPath device{};
    DeviceProfile profile{};
};

struct TraceRecord
{
    std::uint64_t sendUs = 0;
    std::uint64_t receiveUs = 0;
    std::uint32_t rttUs = 0;
    std::uint8_t count = 0;
    std::uint8_t result = 0;
    std::array<std::uint8_t, addressed::kMaxKeysPerPacket> ids{};
    std::array<std::uint16_t, addressed::kMaxKeysPerPacket> raw{};
    std::array<std::uint16_t, addressed::kMaxKeysPerPacket> milli{};
};

struct SessionStats
{
    std::uint64_t sent = 0;
    std::uint64_t responses = 0;
    std::uint64_t timeouts = 0;
    std::uint64_t writeFailures = 0;
    std::uint64_t invalidResponses = 0;
    std::uint64_t lateResponses = 0;
    std::uint64_t recoveredMisses = 0;
    std::uint32_t consecutiveMissPeak = 0;
    std::uint64_t classBound = 0;
    std::uint64_t classLive = 0;
    std::uint64_t classBackground = 0;
    std::array<std::uint32_t, 2048> rtt{};
    std::size_t rttCount = 0;
    std::size_t rttNext = 0;
};

std::atomic<bool> g_running{false};
std::atomic<bool> g_stop{false};
std::atomic<bool> g_deviceChanged{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_routingPrepared{false};
std::atomic<std::uint16_t> g_claimedVendorId{0};
std::atomic<std::uint16_t> g_claimedProductId{0};
std::atomic<std::uint32_t> g_claimedInputBytes{0};
std::atomic<std::uint32_t> g_claimedOutputBytes{0};
std::atomic<std::uint32_t> g_mappedKeys{0};
std::atomic<std::uint64_t> g_pollAttempts{0};
std::atomic<std::uint64_t> g_pollSuccess{0};
std::atomic<std::uint64_t> g_pollFail{0};
std::atomic<ULONGLONG> g_lastResponseMs{0};
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::array<std::atomic<std::uint16_t>, 256> g_releaseRawByKey{};
std::array<std::atomic<std::uint16_t>, 256> g_bottomRawByKey{};
std::array<std::atomic<ULONGLONG>, 256> g_sampleMs{};
HANDLE g_wakeEvent = nullptr;
HANDLE g_responseEvent = nullptr;
HANDLE g_readerExitEvent = nullptr;
std::thread g_thread;
std::atomic<bool> g_workerExited{true};
halljoy::worker::WorkerExceptionRecord g_workerFaultRecord{};
std::atomic<halljoy::worker::WorkerExceptionKind> g_workerFaultKind{
    halljoy::worker::WorkerExceptionKind::None};
halljoy::worker::WorkerExceptionRecord g_readerFaultRecord{};
std::atomic<halljoy::worker::WorkerExceptionKind> g_readerFaultKind{
    halljoy::worker::WorkerExceptionKind::None};
LARGE_INTEGER g_qpcFreq{};
LARGE_INTEGER g_qpcStart{};

std::mutex g_sessionMutex;
std::mutex g_schedulerMutex;
std::mutex g_statsMutex;
std::mutex g_claimMutex;
std::mutex g_probeMutex;
std::mutex g_readerHandleMutex;
HANDLE g_readerHandle = INVALID_HANDLE_VALUE;
addressed::PollScheduler* g_scheduler = nullptr;
std::uint64_t g_pendingSendUs = 0;
addressed::PollPlan g_pendingPlan{};
SessionStats g_stats{};
std::array<TraceRecord, kTraceCount> g_trace{};
std::size_t g_traceNext = 0;
ULONGLONG g_lastTraceDumpMs = 0;
ClaimState g_claim{};

std::uint64_t NowUs()
{
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const std::uint64_t delta = static_cast<std::uint64_t>(now.QuadPart - g_qpcStart.QuadPart);
    const std::uint64_t freq = static_cast<std::uint64_t>(g_qpcFreq.QuadPart);
    if (!freq) return GetTickCount64() * 1000ull;
    return (delta / freq) * 1000000ull + ((delta % freq) * 1000000ull) / freq;
}

std::wstring PathNearExe(const wchar_t* fileName)
{
    std::array<wchar_t, 32768> path{};
    const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    std::wstring out(path.data(), n);
    const auto slash = out.find_last_of(L"\\/");
    if (slash != std::wstring::npos) out.resize(slash + 1); else out.clear();
    out += fileName ? fileName : L"HallJoyAddressedAnalog.log";
    return out;
}


void SupportLog(const wchar_t* fmt, ...)
{
#if defined(HALLJOY_DIAGNOSTIC)
    if (!fmt) return;
    wchar_t body[2048]{};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, _countof(body), _TRUNCATE, fmt, args);
    va_end(args);
    DebugLog_WriteBuffered(L"[addressed] %s", body);
#else
    (void)fmt;
#endif
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

bool IsFingerprintCandidate(const HidPath& item)
{
    return item.usagePage == kProtocolUsagePage && item.usage == kProtocolUsage &&
        item.caps.InputReportByteLength >= kProtocolReportBytes &&
        item.caps.OutputReportByteLength >= kProtocolReportBytes;
}

std::vector<HidPath> EnumerateCandidates()
{
    GUID guid{};
    HidD_GetHidGuid(&guid);
    HDEVINFO info = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return {};
    std::vector<HidPath> out;
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
        if (!metadata || !PopulateCaps(metadata.value, item) || !IsFingerprintCandidate(item)) continue;
        out.push_back(std::move(item));
    }
    SetupDiDestroyDeviceInfoList(info);
    return out;
}

bool ValidChecksum(const std::uint8_t* data, std::size_t size)
{
    if (!data || size != kProtocolReportBytes) return false;
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < size; ++i) sum += data[i];
    return (sum & 0xFFu) == 0xFFu;
}

const std::uint8_t* FindPayload64(const std::uint8_t* data, std::size_t size)
{
    if (!data || size < kProtocolReportBytes) return nullptr;
    for (std::size_t offset = 0; offset <= 1; ++offset)
    {
        if (offset + kProtocolReportBytes > size) continue;
        const auto* candidate = data + offset;
        if (ValidChecksum(candidate, kProtocolReportBytes)) return candidate;
    }
    return nullptr;
}

std::array<std::uint8_t, kProtocolReportBytes> MakePacket(
    std::uint8_t command, std::uint8_t sub,
    const std::uint8_t* payload = nullptr, std::uint16_t payloadBytes = 0)
{
    std::array<std::uint8_t, kProtocolReportBytes> packet{};
    if (payloadBytes > 54) return packet;
    packet[0] = 0x09;
    packet[1] = command;
    packet[2] = sub;
    packet[4] = 0x01;
    packet[6] = static_cast<std::uint8_t>(payloadBytes & 0xFFu);
    packet[7] = static_cast<std::uint8_t>(payloadBytes >> 8);
    if (payload && payloadBytes) std::copy(payload, payload + payloadBytes, packet.begin() + 8);
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < packet.size() - 1; ++i) sum += packet[i];
    packet.back() = static_cast<std::uint8_t>(0xFFu - (sum & 0xFFu));
    return packet;
}

std::array<std::uint8_t, kProtocolReportBytes> MakePollPacket(const addressed::PollPlan& plan)
{
    std::array<std::uint8_t, 18> payload{};
    for (std::size_t i = 0; i < plan.count; ++i)
    {
        payload[i * 2] = plan.keyIds[i];
        payload[i * 2 + 1] = 0;
    }
    return MakePacket(0x94, 0x02, payload.data(), static_cast<std::uint16_t>(plan.count * 2));
}

class Transport
{
public:
    explicit Transport(const HidPath& path) : path_(path)
    {
        write_ = OpenPath(path.path, GENERIC_WRITE, true);
        if (!write_) write_ = OpenPath(path.path, GENERIC_READ | GENERIC_WRITE, true);
        output_ = OpenPath(path.path, GENERIC_WRITE, false);
        if (!output_) output_ = OpenPath(path.path, 0, false);
    }

    bool Send(const std::array<std::uint8_t, kProtocolReportBytes>& packet)
    {
        if (write_ && SendWrite(packet)) return true;
        if (output_)
        {
            std::vector<std::uint8_t> wire(
                std::max<std::size_t>(kProtocolReportBytes, path_.caps.OutputReportByteLength), 0);
            const std::size_t offset = wire.size() > kProtocolReportBytes ? 1u : 0u;
            if (offset + packet.size() > wire.size()) return false;
            std::copy(packet.begin(), packet.end(), wire.begin() + offset);
            if (HidD_SetOutputReport(output_.value, wire.data(), static_cast<ULONG>(wire.size()))) return true;
        }
        return false;
    }

private:
    bool SendWrite(const std::array<std::uint8_t, kProtocolReportBytes>& packet)
    {
        std::vector<std::uint8_t> wire(
            std::max<std::size_t>(kProtocolReportBytes, path_.caps.OutputReportByteLength), 0);
        const std::size_t offset = wire.size() > kProtocolReportBytes ? 1u : 0u;
        if (offset + packet.size() > wire.size()) return false;
        std::copy(packet.begin(), packet.end(), wire.begin() + offset);
        HidIoOperation operation(write_.value);
        DWORD error = ERROR_SUCCESS;
        const auto start = operation.StartWrite(wire.data(), static_cast<DWORD>(wire.size()), &error);
        DWORD transferred = 0;
        if (start == HidIoOperation::StartResult::Completed)
            return operation.Finish(&transferred, &error, false) && transferred == wire.size();
        if (start != HidIoOperation::StartResult::Pending) return false;
        const DWORD wait = operation.Wait(1000);
        if (wait != WAIT_OBJECT_0)
        {
            operation.CancelAndDrain(&transferred, &error);
            return false;
        }
        return operation.Finish(&transferred, &error, false) && transferred == wire.size();
    }

    const HidPath& path_;
    ScopedHandle write_;
    ScopedHandle output_;
};

bool ReadNextPayload(HANDLE handle, const HidPath& path, DWORD timeoutMs,
                     std::array<std::uint8_t, kProtocolReportBytes>& out)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;
    std::vector<std::uint8_t> buffer(
        std::max<std::size_t>(kProtocolReportBytes, path.caps.InputReportByteLength), 0);
    HidIoOperation operation(handle);
    DWORD error = ERROR_SUCCESS;
    const auto start = operation.StartRead(buffer.data(), static_cast<DWORD>(buffer.size()), &error);
    DWORD transferred = 0;
    bool ok = false;
    if (start == HidIoOperation::StartResult::Completed)
        ok = operation.Finish(&transferred, &error, false);
    else if (start == HidIoOperation::StartResult::Pending)
    {
        const DWORD wait = operation.Wait(timeoutMs);
        if (wait == WAIT_OBJECT_0) ok = operation.Finish(&transferred, &error, false);
        else operation.CancelAndDrain(&transferred, &error);
    }
    if (!ok || !transferred) return false;
    const std::uint8_t* payload = FindPayload64(buffer.data(), transferred);
    if (!payload) return false;
    std::copy(payload, payload + kProtocolReportBytes, out.begin());
    return true;
}

std::size_t ParseMapPacket(
    const std::array<std::uint8_t, kProtocolReportBytes>& packet,
    std::array<std::uint16_t, 256>& keyToHid)
{
    if (packet[0] != 0x09 || packet[1] != 0x83 || packet[2] != 0x00 ||
        packet[3] != 0x00 || packet[4] != 0x01 || packet[5] != 0x00)
        return 0;
    const std::uint16_t length = static_cast<std::uint16_t>(packet[6]) |
        (static_cast<std::uint16_t>(packet[7]) << 8);
    if (!length || length > 54 || (length % 6) != 0) return 0;
    std::size_t updates = 0;
    for (std::uint16_t off = 0; off + 5 < length; off += 6)
    {
        const std::uint16_t keyId = static_cast<std::uint16_t>(packet[8 + off]) |
            (static_cast<std::uint16_t>(packet[9 + off]) << 8);
        const std::uint16_t hid = static_cast<std::uint16_t>(packet[12 + off]) |
            (static_cast<std::uint16_t>(packet[13 + off]) << 8);
        if (!keyId || keyId >= 256 || !hid || hid >= 256) continue;
        if (keyToHid[keyId] != hid)
        {
            keyToHid[keyId] = hid;
            ++updates;
        }
    }
    return updates;
}

std::uint8_t FindKeyIdForHid(const std::array<std::uint16_t, 256>& map, std::uint16_t hid)
{
    for (std::size_t keyId = 1; keyId < map.size(); ++keyId)
        if (map[keyId] == hid) return static_cast<std::uint8_t>(keyId);
    for (const auto& key : kCanonicalKeys)
        if (key.hidUsage == hid) return key.keyId;
    return 0;
}

bool ProbeAddressedResponse(
    const HidPath& path,
    Transport& transport,
    HANDLE readHandle,
    const std::array<std::uint8_t, addressed::kMaxKeysPerPacket>& ids,
    std::size_t count,
    std::uint32_t* outRttUs)
{
    addressed::PollPlan plan{};
    plan.count = count;
    for (std::size_t i = 0; i < count; ++i) plan.keyIds[i] = ids[i];
    const auto request = MakePollPacket(plan);
    const std::uint64_t sendUs = NowUs();
    if (!transport.Send(request)) return false;

    const ULONGLONG deadline = GetTickCount64() + kProbeResponseWindowMs;
    while (GetTickCount64() < deadline)
    {
        std::array<std::uint8_t, kProtocolReportBytes> packet{};
        const DWORD remaining = static_cast<DWORD>(std::max<ULONGLONG>(1, deadline - GetTickCount64()));
        if (!ReadNextPayload(readHandle, path, remaining, packet)) continue;
        if (packet[0] != 0x09 || packet[1] != 0x94 || packet[2] != 0x02) continue;
        const std::uint16_t length = static_cast<std::uint16_t>(packet[6]) |
            (static_cast<std::uint16_t>(packet[7]) << 8);
        if (length != count * 6) continue;
        std::array<bool, 256> expected{};
        std::array<bool, 256> seen{};
        for (std::size_t i = 0; i < count; ++i) expected[ids[i]] = true;
        std::size_t plausible = 0;
        bool valid = true;
        for (std::uint16_t off = 0; off + 5 < length; off += 6)
        {
            const std::uint8_t keyId = packet[8 + off];
            if (!expected[keyId] || seen[keyId]) { valid = false; break; }
            seen[keyId] = true;
            const std::uint16_t raw = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(packet[9 + off] & 0x7Fu) << 8) | packet[10 + off]);
            if (raw >= 500 && raw <= 20000) ++plausible;
        }
        if (!valid || plausible == 0) continue;
        if (outRttUs)
        {
            const std::uint64_t nowUs = NowUs();
            *outRttUs = nowUs >= sendUs
                ? static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffffu, nowUs - sendUs))
                : 0;
        }
        return true;
    }
    return false;
}

DeviceProfile BuildProfile(const HidPath& path, const std::array<std::uint16_t, 256>& dynamicMap,
                           std::size_t dynamicEntries)
{
    DeviceProfile profile{};
    profile.mapEntries = dynamicEntries;
    profile.verifiedCalibrationSeed = path.attrs.VendorID == 0x372E && path.attrs.ProductID == 0x105C;
    std::array<std::uint16_t, 256> merged{};
    for (const auto& key : kCanonicalKeys) merged[key.keyId] = key.hidUsage;
    for (std::size_t keyId = 1; keyId < dynamicMap.size(); ++keyId)
        if (dynamicMap[keyId]) merged[keyId] = dynamicMap[keyId];

    // A sufficiently complete device-provided map defines the physical profile.
    // With a partial/no map, retain the canonical fallback so verified firmware
    // variants remain usable.
    if (dynamicEntries >= 20)
    {
        for (std::size_t keyId = 1; keyId < dynamicMap.size(); ++keyId)
            if (dynamicMap[keyId]) profile.keys.push_back({ static_cast<std::uint8_t>(keyId), dynamicMap[keyId] });
        profile.source = L"device-map";
    }
    else
    {
        for (const auto& key : kCanonicalKeys) profile.keys.push_back({ key.keyId, merged[key.keyId] });
        profile.source = dynamicEntries ? L"canonical+partial-map" : L"canonical-fallback";
    }
    return profile;
}

bool ProbeCandidate(const HidPath& path, DeviceProfile& outProfile, std::uint32_t& outRttUs)
{
    auto read = OpenPath(path.path, GENERIC_READ | GENERIC_WRITE, true);
    if (!read) read = OpenPath(path.path, GENERIC_READ, true);
    if (!read) return false;
    HidD_SetNumInputBuffers(read.value, 64);
    Transport transport(path);

    std::array<std::uint16_t, 256> dynamicMap{};
    std::size_t dynamicEntries = 0;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!transport.Send(MakePacket(0x83, 0x00))) break;
        const ULONGLONG deadline = GetTickCount64() + kProbeMapWindowMs;
        std::size_t before = dynamicEntries;
        while (GetTickCount64() < deadline)
        {
            std::array<std::uint8_t, kProtocolReportBytes> packet{};
            const DWORD remaining = static_cast<DWORD>(std::max<ULONGLONG>(1, deadline - GetTickCount64()));
            if (!ReadNextPayload(read.value, path, remaining, packet)) continue;
            dynamicEntries += ParseMapPacket(packet, dynamicMap);
        }
        if (dynamicEntries == before && attempt > 0) break;
    }

    const std::array<std::uint16_t, 4> probeHids = { 0x1A, 0x04, 0x16, 0x07 }; // W A S D
    std::array<std::uint8_t, addressed::kMaxKeysPerPacket> probeIds{};
    std::size_t probeCount = 0;
    for (const std::uint16_t hid : probeHids)
    {
        const std::uint8_t keyId = FindKeyIdForHid(dynamicMap, hid);
        if (keyId) probeIds[probeCount++] = keyId;
    }
    if (probeCount < 2) return false;
    if (!ProbeAddressedResponse(path, transport, read.value, probeIds, probeCount, &outRttUs)) return false;
    outProfile = BuildProfile(path, dynamicMap, dynamicEntries);
    return !outProfile.keys.empty();
}

bool SamePath(const std::wstring& a, const std::wstring& b)
{
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool TryClaimCandidate(const HidPath& candidate)
{
    if (!IsFingerprintCandidate(candidate)) return false;
    const bool alreadyOurs = NativeAnalogRouting_IsClaimedBy(
        candidate.attrs.VendorID, candidate.attrs.ProductID,
        NativeAnalogProtocol::Addressed09402);
    if (NativeAnalogRouting_IsClaimed(candidate.attrs.VendorID, candidate.attrs.ProductID) && !alreadyOurs)
        return false;
    // After UAP starts, only a previously proven VID/PID may be re-opened.
    // A newly attached protocol candidate requires restart so ownership can be
    // recalculated before the UAP child enumerates HID paths.
    if (g_routingPrepared.load(std::memory_order_acquire) && !alreadyOurs)
        return false;
    {
        std::lock_guard<std::mutex> lock(g_claimMutex);
        if (g_claim.valid) return SamePath(g_claim.device.path, candidate.path);
    }

    std::lock_guard<std::mutex> probeLock(g_probeMutex);
    {
        std::lock_guard<std::mutex> lock(g_claimMutex);
        if (g_claim.valid) return SamePath(g_claim.device.path, candidate.path);
    }

    SupportLog(L"candidate vid=%04X pid=%04X usage=%04X:%04X in=%u out=%u product=\"%s\"",
        candidate.attrs.VendorID, candidate.attrs.ProductID,
        candidate.usagePage, candidate.usage,
        candidate.caps.InputReportByteLength, candidate.caps.OutputReportByteLength,
        candidate.product.empty() ? L"" : candidate.product.c_str());

    DeviceProfile profile{};
    std::uint32_t rttUs = 0;
    if (!ProbeCandidate(candidate, profile, rttUs))
    {
        SupportLog(L"probe rejected vid=%04X pid=%04X reason=no-valid-94/02-response",
            candidate.attrs.VendorID, candidate.attrs.ProductID);
        return false;
    }

    if (!NativeAnalogRouting_Claim(candidate.attrs.VendorID, candidate.attrs.ProductID,
            NativeAnalogProtocol::Addressed09402))
        return false;
    {
        std::lock_guard<std::mutex> lock(g_claimMutex);
        if (g_claim.valid) return SamePath(g_claim.device.path, candidate.path);
        g_claim.valid = true;
        g_claim.device = candidate;
        g_claim.profile = profile;
    }
    g_claimedVendorId.store(candidate.attrs.VendorID, std::memory_order_release);
    g_claimedProductId.store(candidate.attrs.ProductID, std::memory_order_release);
    g_claimedInputBytes.store(candidate.caps.InputReportByteLength, std::memory_order_release);
    g_claimedOutputBytes.store(candidate.caps.OutputReportByteLength, std::memory_order_release);
    g_mappedKeys.store(static_cast<std::uint32_t>(profile.keys.size()), std::memory_order_release);
    SupportLog(L"probe accepted vid=%04X pid=%04X profile=%s keys=%u map_entries=%u rtt_us=%u",
        candidate.attrs.VendorID, candidate.attrs.ProductID, profile.source.c_str(),
        static_cast<unsigned>(profile.keys.size()), static_cast<unsigned>(profile.mapEntries), rttUs);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
    return true;
}

bool FindAndClaimCandidate()
{
    const auto candidates = EnumerateCandidates();
    for (const auto& candidate : candidates)
        if (TryClaimCandidate(candidate)) return true;
    return false;
}

bool GetClaimSnapshot(HidPath& path, DeviceProfile& profile)
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    if (!g_claim.valid) return false;
    path = g_claim.device;
    profile = g_claim.profile;
    return true;
}

void ClearClaimForPath(const std::wstring& path)
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    if (g_claim.valid && SamePath(g_claim.device.path, path)) g_claim = ClaimState{};
}

void ResetPublished(const DeviceProfile* profile = nullptr) noexcept
{
    bool analogueChanged = false;
    for (auto& v : g_milli)
        analogueChanged = v.exchange(0, std::memory_order_acq_rel) != 0 || analogueChanged;
    for (auto& v : g_releaseRawByKey) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_bottomRawByKey) v.store(0, std::memory_order_relaxed);
    for (auto& v : g_sampleMs) v.store(0, std::memory_order_relaxed);
    if (profile && profile->verifiedCalibrationSeed)
    {
        g_releaseRawByKey[0x1E].store(10112); g_bottomRawByKey[0x1E].store(1796);
        g_releaseRawByKey[0x2B].store(10351); g_bottomRawByKey[0x2B].store(1881);
        g_releaseRawByKey[0x2C].store(10129); g_bottomRawByKey[0x2C].store(1935);
        g_releaseRawByKey[0x2D].store(10564); g_bottomRawByKey[0x2D].store(2033);
    }
    g_lastResponseMs.store(0, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    if (analogueChanged)
    {
        try { RealtimeLoop_NotifyInputChanged(); }
        catch (...) { OutputDebugStringA("[HallJoy] Addressed neutral notification failed\n"); }
    }
}

std::uint16_t Normalise(std::uint8_t keyId, std::uint16_t raw)
{
    if (!keyId || raw < 500 || raw > 20000) return 0;
    auto& releaseAtomic = g_releaseRawByKey[keyId];
    auto& bottomAtomic = g_bottomRawByKey[keyId];
    std::uint16_t released = releaseAtomic.load(std::memory_order_relaxed);
    std::uint16_t bottom = bottomAtomic.load(std::memory_order_relaxed);
    if (!released) { released = raw; releaseAtomic.store(released, std::memory_order_relaxed); }
    if (!bottom)
    {
        bottom = released > 8400 ? static_cast<std::uint16_t>(released - 8400) : 500;
        bottomAtomic.store(bottom, std::memory_order_relaxed);
    }
    if (raw > released && raw < 20000) { released = raw; releaseAtomic.store(raw, std::memory_order_relaxed); }
    if (raw < bottom && raw > 500) { bottom = raw; bottomAtomic.store(raw, std::memory_order_relaxed); }
    if (released <= bottom + 256 || raw >= released) return 0;
    const std::uint32_t den = released - bottom;
    std::uint16_t milli = static_cast<std::uint16_t>(std::min<std::uint32_t>(1000,
        ((released - raw) * 1000u + den / 2u) / den));
    return milli < 8 ? 0 : milli;
}

void PushTrace(const addressed::PollPlan& plan, std::uint64_t sendUs, std::uint64_t receiveUs,
               std::uint32_t rttUs, std::uint8_t result,
               const std::array<std::uint16_t, addressed::kMaxKeysPerPacket>* raw = nullptr,
               const std::array<std::uint16_t, addressed::kMaxKeysPerPacket>* milli = nullptr)
{
    std::lock_guard<std::mutex> lock(g_sessionMutex);
    auto& t = g_trace[g_traceNext++ % g_trace.size()];
    t = TraceRecord{};
    t.sendUs = sendUs;
    t.receiveUs = receiveUs;
    t.rttUs = rttUs;
    t.count = static_cast<std::uint8_t>(plan.count);
    t.result = result;
    t.ids = plan.keyIds;
    if (raw) t.raw = *raw;
    if (milli) t.milli = *milli;
}

void DumpTrace(const wchar_t* reason)
{
#if defined(HALLJOY_DIAGNOSTIC)
    const ULONGLONG now = GetTickCount64();
    if (now - g_lastTraceDumpMs < 10000) return;
    g_lastTraceDumpMs = now;
    const std::wstring path = PathNearExe(L"HallJoyAddressedAnalogTrace.log");
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    char line[512]{};
    int n = _snprintf_s(line, _countof(line), _TRUNCATE, "reason=%ls\r\n", reason ? reason : L"unknown");
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(std::max(0, n)), &written, nullptr);
    std::lock_guard<std::mutex> lock(g_sessionMutex);
    for (std::size_t k = 0; k < g_trace.size(); ++k)
    {
        const auto& t = g_trace[(g_traceNext + k) % g_trace.size()];
        if (!t.sendUs) continue;
        n = _snprintf_s(line, _countof(line), _TRUNCATE,
            "send_us=%llu recv_us=%llu rtt_us=%u result=%u count=%u values=",
            static_cast<unsigned long long>(t.sendUs), static_cast<unsigned long long>(t.receiveUs),
            t.rttUs, t.result, t.count);
        std::string s(line, static_cast<std::size_t>(std::max(0, n)));
        for (std::size_t i = 0; i < t.count; ++i)
        {
            char item[48]{};
            _snprintf_s(item, _countof(item), _TRUNCATE, "%s%02X:%u/%u",
                i ? "," : "", t.ids[i], t.raw[i], t.milli[i]);
            s += item;
        }
        s += "\r\n";
        WriteFile(file, s.data(), static_cast<DWORD>(s.size()), &written, nullptr);
    }
    CloseHandle(file);
    SupportLog(L"anomaly trace saved reason=%s path=%s", reason, path.c_str());
#else
    (void)reason;
#endif
}

void RegisterReaderHandle(HANDLE handle)
{
    std::lock_guard<std::mutex> lock(g_readerHandleMutex);
    g_readerHandle = handle;
}

void ReleaseReaderHandle(HANDLE handle)
{
    bool closeHandle = false;
    {
        std::lock_guard<std::mutex> lock(g_readerHandleMutex);
        if (g_readerHandle == handle)
        {
            g_readerHandle = INVALID_HANDLE_VALUE;
            closeHandle = true;
        }
    }
    if (closeHandle && handle && handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
    if (g_readerExitEvent)
        SetEvent(g_readerExitEvent);
}

class ScopedRegisteredReaderHandle
{
public:
    explicit ScopedRegisteredReaderHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~ScopedRegisteredReaderHandle() noexcept { ReleaseReaderHandle(handle_); }
    ScopedRegisteredReaderHandle(const ScopedRegisteredReaderHandle&) = delete;
    ScopedRegisteredReaderHandle& operator=(const ScopedRegisteredReaderHandle&) = delete;

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

void CancelReaderIo()
{
    std::lock_guard<std::mutex> lock(g_readerHandleMutex);
    if (g_readerHandle && g_readerHandle != INVALID_HANDLE_VALUE)
        CancelIoEx(g_readerHandle, nullptr);
}

void ForceCloseReaderHandle()
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    {
        std::lock_guard<std::mutex> lock(g_readerHandleMutex);
        handle = g_readerHandle;
        g_readerHandle = INVALID_HANDLE_VALUE;
    }
    if (handle && handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
}

void RecordRtt(std::uint32_t rtt)
{
    g_stats.rtt[g_stats.rttNext++ % g_stats.rtt.size()] = rtt;
    g_stats.rttCount = std::min(g_stats.rttCount + 1, g_stats.rtt.size());
}

void PublishResponse(const std::uint8_t* data, std::size_t size, std::uint64_t receiveUs)
{
    const std::uint8_t* packet = FindPayload64(data, size);
    if (!packet || packet[0] != 0x09 || packet[1] != 0x94 || packet[2] != 0x02) return;
    const std::uint16_t payloadBytes = static_cast<std::uint16_t>(packet[6]) |
        (static_cast<std::uint16_t>(packet[7]) << 8);
    if (!payloadBytes || payloadBytes > 54 || payloadBytes % 6)
    {
        std::lock_guard<std::mutex> statsLock(g_statsMutex);
        ++g_stats.invalidResponses;
        return;
    }

    std::uint64_t sendUs = 0;
    addressed::PollPlan plan{};
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        sendUs = g_pendingSendUs;
        plan = g_pendingPlan;
    }
    if (!sendUs || !plan.count)
    {
        std::lock_guard<std::mutex> statsLock(g_statsMutex);
        ++g_stats.lateResponses;
        return;
    }
    if (payloadBytes / 6 != plan.count)
    {
        std::lock_guard<std::mutex> statsLock(g_statsMutex);
        ++g_stats.invalidResponses;
        return;
    }

    std::array<bool, 256> expected{};
    std::array<bool, 256> seen{};
    for (std::size_t i = 0; i < plan.count; ++i) expected[plan.keyIds[i]] = true;
    for (std::uint16_t off = 0; off + 5 < payloadBytes; off += 6)
    {
        const std::uint8_t keyId = packet[8 + off];
        if (!expected[keyId] || seen[keyId])
        {
            std::lock_guard<std::mutex> statsLock(g_statsMutex);
            ++g_stats.invalidResponses;
            return;
        }
        seen[keyId] = true;
    }

    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        if (g_pendingSendUs != sendUs)
        {
            std::lock_guard<std::mutex> statsLock(g_statsMutex);
            ++g_stats.lateResponses;
            return;
        }
        g_pendingSendUs = 0;
    }

    const std::uint32_t rtt = receiveUs >= sendUs
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(0xffffffffu, receiveUs - sendUs)) : 0;
    const ULONGLONG nowMs = GetTickCount64();
    std::array<std::uint16_t, addressed::kMaxKeysPerPacket> rawValues{};
    std::array<std::uint16_t, addressed::kMaxKeysPerPacket> milliValues{};
    bool analogueChanged = false;

    for (std::uint16_t off = 0; off + 5 < payloadBytes; off += 6)
    {
        const std::uint8_t keyId = packet[8 + off];
        const std::uint16_t raw = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(packet[9 + off] & 0x7Fu) << 8) | packet[10 + off]);
        std::uint16_t hid = 0;
        {
            std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
            if (g_scheduler) hid = g_scheduler->HidForKeyId(keyId);
        }
        const std::uint16_t milli = Normalise(keyId, raw);
        if (hid && hid < 256)
        {
            const std::uint16_t previous = g_milli[hid].exchange(milli, std::memory_order_acq_rel);
            analogueChanged = analogueChanged || previous != milli;
            g_sampleMs[hid].store(nowMs, std::memory_order_release);
        }
        {
            std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
            if (g_scheduler) g_scheduler->OnSample(keyId, raw, milli, receiveUs);
        }
        for (std::size_t i = 0; i < plan.count; ++i)
            if (plan.keyIds[i] == keyId) { rawValues[i] = raw; milliValues[i] = milli; break; }
    }

    {
        std::lock_guard<std::mutex> statsLock(g_statsMutex);
        ++g_stats.responses;
        if (rtt) RecordRtt(rtt);
    }
    g_lastResponseMs.store(nowMs, std::memory_order_release);
    g_connected.store(true, std::memory_order_release);
    g_pollSuccess.fetch_add(1, std::memory_order_relaxed);
    if (analogueChanged) RealtimeLoop_NotifyInputChanged();
    PushTrace(plan, sendUs, receiveUs, rtt, 1, &rawValues, &milliValues);
    if (g_responseEvent) SetEvent(g_responseEvent);
}

std::uint32_t ReaderLoopBody(const HidPath* path)
{
    if (!path)
    {
        if (g_readerExitEvent) SetEvent(g_readerExitEvent);
        return 0u;
    }
    auto opened = OpenPath(path->path, GENERIC_READ | GENERIC_WRITE, true);
    if (!opened) opened = OpenPath(path->path, GENERIC_READ, true);
    if (!opened)
    {
        if (g_readerExitEvent) SetEvent(g_readerExitEvent);
        return 0u;
    }

    // Ownership is registered so the shutdown thread can cancel or, as a
    // last resort, close the exact handle that owns the pending overlapped read.
    HANDLE handle = opened.value;
    opened.value = INVALID_HANDLE_VALUE;
    RegisterReaderHandle(handle);
    ScopedRegisteredReaderHandle registeredHandle(handle);
    HidD_SetNumInputBuffers(handle, 128);
    std::vector<std::uint8_t> buffer(
        std::max<std::size_t>(kProtocolReportBytes, path->caps.InputReportByteLength), 0);

    while (!g_stop.load(std::memory_order_acquire) && !g_deviceChanged.load(std::memory_order_acquire))
    {
        HidIoOperation operation(handle);
        DWORD error = ERROR_SUCCESS;
        const auto start = operation.StartRead(buffer.data(), static_cast<DWORD>(buffer.size()), &error);
        if (start == HidIoOperation::StartResult::Failed)
        {
            if (error == ERROR_INVALID_HANDLE || error == ERROR_DEVICE_NOT_CONNECTED ||
                error == ERROR_OPERATION_ABORTED)
                break;
            Sleep(10);
            continue;
        }
        DWORD transferred = 0;
        bool ok = false;
        if (start == HidIoOperation::StartResult::Completed)
            ok = operation.Finish(&transferred, &error, false);
        else
        {
            const DWORD wait = operation.Wait(kReadWaitMs);
            if (wait == WAIT_OBJECT_0)
                ok = operation.Finish(&transferred, &error, false);
            else
            {
                operation.CancelAndDrain(&transferred, &error);
                if (g_stop.load(std::memory_order_acquire) ||
                    g_deviceChanged.load(std::memory_order_acquire) ||
                    error == ERROR_INVALID_HANDLE || error == ERROR_DEVICE_NOT_CONNECTED)
                    break;
                continue;
            }
        }
        if (ok && transferred)
            PublishResponse(buffer.data(), transferred, NowUs());
        else if (error == ERROR_INVALID_HANDLE || error == ERROR_DEVICE_NOT_CONNECTED ||
            error == ERROR_OPERATION_ABORTED)
            break;
    }

    return 0u;
}

void AddressedReaderOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_readerFaultRecord = record;
    g_readerFaultKind.store(record.kind, std::memory_order_release);
    g_deviceChanged.store(true, std::memory_order_release);
    ResetPublished();
    StabilityTrace_WriteCritical(L"ERROR", L"addressed.reader", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
    OutputDebugStringA("[HallJoy] Addressed reader exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

void AddressedReaderOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    if (g_readerExitEvent) SetEvent(g_readerExitEvent);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"addressed.reader", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

void AddressedReaderEntry(const HidPath* path) noexcept
{
    StabilityTrace_Write(L"INFO", L"addressed.reader", L"worker.start");
    (void)halljoy::worker::RunWorkerEntryBarrier(
        [path] { return ReaderLoopBody(path); },
        AddressedReaderOnFault,
        AddressedReaderOnCompletion,
        0xE0520004u);
}

void RefreshBindings(const DeviceProfile& profile)
{
    std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
    if (!g_scheduler) return;
    for (const auto& key : profile.keys)
        if (key.hidUsage) g_scheduler->SetBound(key.hidUsage, Bindings_IsHidBound(key.hidUsage));
}

void LogSummary(std::uint64_t windowStartUs, std::uint64_t nowUs, const SessionStats& before)
{
    SessionStats current{};
    {
        std::lock_guard<std::mutex> statsLock(g_statsMutex);
        current = g_stats;
    }
    const double seconds = std::max(0.001, static_cast<double>(nowUs - windowStartUs) / 1000000.0);
    std::vector<std::uint32_t> rtt;
    rtt.reserve(current.rttCount);
    for (std::size_t i = 0; i < current.rttCount; ++i) rtt.push_back(current.rtt[i]);
    std::sort(rtt.begin(), rtt.end());
    auto pct = [&](double p) -> std::uint32_t {
        if (rtt.empty()) return 0;
        const std::size_t last = rtt.size() - 1;
        const double scaled = p * static_cast<double>(last);
        const std::size_t index = std::min(last, static_cast<std::size_t>(scaled));
        return rtt[index];
    };
    addressed::PollSchedulerStats sched{};
    {
        std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
        if (g_scheduler) sched = g_scheduler->GetStats(nowUs);
    }
    const std::uint64_t sent = current.sent - before.sent;
    const std::uint64_t recv = current.responses - before.responses;
    const std::uint64_t boundSlots = current.classBound - before.classBound;
    const std::uint64_t liveSlots = current.classLive - before.classLive;
    const std::uint64_t backgroundSlots = current.classBackground - before.classBackground;
    const double pps = static_cast<double>(recv) / seconds;
    const double boundHz = sched.boundCount ? static_cast<double>(boundSlots) / seconds / sched.boundCount : 0.0;
    const std::uint32_t liveCount = sched.movingCount + sched.activeCount + sched.recentCount;
    const double liveHz = liveCount ? static_cast<double>(liveSlots) / seconds / liveCount : 0.0;
    const double backgroundHz = sched.backgroundCount
        ? static_cast<double>(backgroundSlots) / seconds / sched.backgroundCount : 0.0;

    SupportLog(L"pps=%.1f sent=%llu recv=%llu timeout=%llu recovered=%llu late=%llu fail=%llu invalid=%llu miss_peak=%u rtt_us=%u/%u/%u keys=%u/%u/%u/%u/%u hz=%.1f/%.1f/%.1f age_us=%llu/%llu/%llu missed=%llu",
        pps, static_cast<unsigned long long>(sent), static_cast<unsigned long long>(recv),
        static_cast<unsigned long long>(current.timeouts - before.timeouts),
        static_cast<unsigned long long>(current.recoveredMisses - before.recoveredMisses),
        static_cast<unsigned long long>(current.lateResponses - before.lateResponses),
        static_cast<unsigned long long>(current.writeFailures - before.writeFailures),
        static_cast<unsigned long long>(current.invalidResponses - before.invalidResponses),
        current.consecutiveMissPeak,
        pct(0.50), pct(0.95), pct(0.99),
        sched.boundCount, sched.movingCount, sched.activeCount, sched.recentCount, sched.backgroundCount,
        boundHz, liveHz, backgroundHz,
        static_cast<unsigned long long>(sched.maxBoundAgeUs),
        static_cast<unsigned long long>(sched.maxActiveAgeUs),
        static_cast<unsigned long long>(sched.maxBackgroundAgeUs),
        static_cast<unsigned long long>(sched.deadlineMisses));

    DebugLog_WriteBuffered(L"[addressed] pps=%.1f sent=%llu recv=%llu timeout=%llu recovered=%llu late=%llu invalid=%llu",
        pps, static_cast<unsigned long long>(sent), static_cast<unsigned long long>(recv),
        static_cast<unsigned long long>(current.timeouts - before.timeouts),
        static_cast<unsigned long long>(current.recoveredMisses - before.recoveredMisses),
        static_cast<unsigned long long>(current.lateResponses - before.lateResponses),
        static_cast<unsigned long long>(current.invalidResponses - before.invalidResponses));

    if (current.invalidResponses > before.invalidResponses) DumpTrace(L"invalid addressed response");
    else if (sched.maxBoundAgeUs > 20000 || sched.maxActiveAgeUs > 40000 || sched.maxBackgroundAgeUs > 150000)
        DumpTrace(L"scheduler age anomaly");
}

bool RunSession(const HidPath& path, const DeviceProfile& profile)
{
    ResetPublished(&profile);
    addressed::PollScheduler scheduler(profile.keys.data(), profile.keys.size());
    std::thread reader;
    try
    {
        scheduler.Reset(NowUs());
        {
            std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
            g_scheduler = &scheduler;
        }
        {
            std::lock_guard<std::mutex> lock(g_sessionMutex);
            g_pendingSendUs = 0;
            g_trace = {};
            g_traceNext = 0;
        }
        {
            std::lock_guard<std::mutex> statsLock(g_statsMutex);
            g_stats = SessionStats{};
        }

        Transport transport(path);
        g_readerFaultRecord = {};
        g_readerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
            std::memory_order_release);
        if (g_readerExitEvent) ResetEvent(g_readerExitEvent);
        reader = std::thread(AddressedReaderEntry, &path);
        transport.Send(MakePacket(0x98, 0x02));
        SupportLog(L"session start vid=%04X pid=%04X profile=%s keys=%u",
            path.attrs.VendorID, path.attrs.ProductID, profile.source.c_str(), static_cast<unsigned>(profile.keys.size()));

        ULONGLONG lastBindMs = 0;
        ULONGLONG lastSummaryMs = GetTickCount64();
        std::uint64_t summaryStartUs = NowUs();
        SessionStats summaryBefore{};
        bool ok = true;
        std::uint32_t consecutiveMisses = 0;
        std::uint64_t lastGoodResponseUs = NowUs();

        while (!g_stop.load(std::memory_order_acquire) && !g_deviceChanged.load(std::memory_order_acquire))
        {
            const ULONGLONG nowMs = GetTickCount64();
            if (nowMs - lastBindMs >= kBindingRefreshMs) { RefreshBindings(profile); lastBindMs = nowMs; }
            const std::uint64_t sendUs = NowUs();
            addressed::PollPlan plan{};
            {
                std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
                plan = scheduler.BuildPlan(sendUs);
            }
            if (!plan.count) { Sleep(1); continue; }
            const auto packet = MakePollPacket(plan);
            if (g_responseEvent) ResetEvent(g_responseEvent);
            {
                std::lock_guard<std::mutex> lock(g_sessionMutex);
                g_pendingSendUs = sendUs;
                g_pendingPlan = plan;
            }
            g_pollAttempts.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> statsLock(g_statsMutex);
                ++g_stats.sent;
                for (std::size_t i = 0; i < plan.count; ++i)
                {
                    switch (plan.classes[i])
                    {
                    case addressed::PollClass::Bound: ++g_stats.classBound; break;
                    case addressed::PollClass::Background: ++g_stats.classBackground; break;
                    default: ++g_stats.classLive; break;
                    }
                }
            }
            if (!transport.Send(packet))
            {
                g_pollFail.fetch_add(1, std::memory_order_relaxed);
                { std::lock_guard<std::mutex> statsLock(g_statsMutex); ++g_stats.writeFailures; }
                PushTrace(plan, sendUs, 0, 0, 3);
                DumpTrace(L"write failure");
                ok = false;
                break;
            }
            const DWORD wait = WaitForSingleObject(g_responseEvent, kResponseTimeoutMs);
            bool received = wait == WAIT_OBJECT_0;
            if (!received)
            {
                const DWORD late = WaitForSingleObject(g_responseEvent, kLateResponseWindowMs);
                received = late == WAIT_OBJECT_0;
            }

            if (received)
            {
                if (consecutiveMisses)
                {
                    std::lock_guard<std::mutex> statsLock(g_statsMutex);
                    g_stats.recoveredMisses += consecutiveMisses;
                }
                consecutiveMisses = 0;
                lastGoodResponseUs = NowUs();
            }
            else
            {
                g_pollFail.fetch_add(1, std::memory_order_relaxed);
                ++consecutiveMisses;
                {
                    std::lock_guard<std::mutex> statsLock(g_statsMutex);
                    ++g_stats.timeouts;
                    g_stats.consecutiveMissPeak = std::max(g_stats.consecutiveMissPeak, consecutiveMisses);
                }
                {
                    // Make a late reply harmless. The next request will install a
                    // new token; PublishResponse counts an old reply as late rather
                    // than corrupting or terminating the session.
                    std::lock_guard<std::mutex> lock(g_sessionMutex);
                    if (g_pendingSendUs == sendUs)
                        g_pendingSendUs = 0;
                }
                PushTrace(plan, sendUs, 0, 0, 2);

                const std::uint64_t nowUs = NowUs();
                if (consecutiveMisses >= kMaxConsecutiveResponseMisses ||
                    nowUs - lastGoodResponseUs >= kMaxNoResponseUs)
                {
                    SupportLog(L"response recovery exhausted consecutive=%u no_response_ms=%llu",
                        consecutiveMisses,
                        static_cast<unsigned long long>((nowUs - lastGoodResponseUs) / 1000ull));
                    DumpTrace(L"addressed response recovery exhausted");
                    ok = false;
                    break;
                }
                continue;
            }
            if (nowMs - lastSummaryMs >= kSummaryMs)
            {
                LogSummary(summaryStartUs, NowUs(), summaryBefore);
                { std::lock_guard<std::mutex> statsLock(g_statsMutex); summaryBefore = g_stats; }
                summaryStartUs = NowUs();
                lastSummaryMs = nowMs;
            }
        }

        g_deviceChanged.store(true, std::memory_order_release);
        CancelReaderIo();
        if (g_readerExitEvent && WaitForSingleObject(g_readerExitEvent, kReaderStopWaitMs) != WAIT_OBJECT_0)
        {
            SupportLog(L"reader cancellation delayed; force-closing HID handle");
            StabilityTrace_WriteCritical(L"ERROR", L"addressed.reader", L"force_close", L"phase=normal_cleanup");
            ForceCloseReaderHandle();
            WaitForSingleObject(g_readerExitEvent, kReaderForceCloseWaitMs);
        }
        if (reader.joinable()) reader.join();
        {
            std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
            g_scheduler = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(g_sessionMutex);
            g_pendingSendUs = 0;
        }
        ResetPublished();
        SupportLog(L"session end vid=%04X pid=%04X ok=%d", path.attrs.VendorID, path.attrs.ProductID, ok ? 1 : 0);
        return ok;
    }
    catch (...)
    {
        // A joinable std::thread must be reaped before stack unwinding reaches
        // its destructor. Otherwise an allocation/logging exception in the
        // session owner would bypass the outer worker barrier via std::terminate.
        g_deviceChanged.store(true, std::memory_order_release);
        CancelReaderIo();
        if (g_readerExitEvent && WaitForSingleObject(g_readerExitEvent, kReaderStopWaitMs) != WAIT_OBJECT_0)
        {
            StabilityTrace_WriteCritical(L"ERROR", L"addressed.reader", L"force_close", L"phase=exception_cleanup");
            ForceCloseReaderHandle();
            WaitForSingleObject(g_readerExitEvent, kReaderForceCloseWaitMs);
        }
        if (reader.joinable()) reader.join();
        {
            std::lock_guard<std::mutex> schedulerLock(g_schedulerMutex);
            g_scheduler = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(g_sessionMutex);
            g_pendingSendUs = 0;
        }
        ResetPublished();
        throw;
    }
}

std::uint32_t AddressedWorkerBody()
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        g_deviceChanged.store(false, std::memory_order_release);
        HidPath path{};
        DeviceProfile profile{};
        if (!GetClaimSnapshot(path, profile))
        {
            FindAndClaimCandidate();
            if (!GetClaimSnapshot(path, profile))
            {
                ResetPublished();
                WaitForSingleObject(g_wakeEvent, 5000);
                if (g_wakeEvent) ResetEvent(g_wakeEvent);
                continue;
            }
        }
        RunSession(path, profile);
        ClearClaimForPath(path.path);
        if (!g_stop.load(std::memory_order_acquire))
        {
            WaitForSingleObject(g_wakeEvent, 500);
            if (g_wakeEvent) ResetEvent(g_wakeEvent);
        }
    }
    ResetPublished();
    g_running.store(false, std::memory_order_release);
    return 0u;
}

void AddressedWorkerOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_workerFaultRecord = record;
    g_workerFaultKind.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_deviceChanged.store(true, std::memory_order_release);
    ResetPublished();
    if (g_wakeEvent) SetEvent(g_wakeEvent);
    StabilityTrace_WriteCritical(L"ERROR", L"addressed", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
    OutputDebugStringA("[HallJoy] Addressed worker exception: ");
    OutputDebugStringA(record.message[0] ? record.message : "unknown");
    OutputDebugStringA("\n");
}

void AddressedWorkerOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    g_workerExited.store(true, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"addressed", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

void AddressedWorkerEntry() noexcept
{
    StabilityTrace_Write(L"INFO", L"addressed", L"worker.start");
    (void)halljoy::worker::RunWorkerEntryBarrier(
        [] { return AddressedWorkerBody(); },
        AddressedWorkerOnFault,
        AddressedWorkerOnCompletion,
        0xE0520005u);
}
} // namespace

bool AddressedAnalog_PrepareProtocolRouting()
{
    if (g_routingPrepared.load(std::memory_order_acquire))
        return g_claimedProductId.load(std::memory_order_acquire) != 0;
    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_qpcStart);
    const bool found = FindAndClaimCandidate();
    g_routingPrepared.store(true, std::memory_order_release);
    return found;
}

bool AddressedAnalog_Start()
{
    if (!AddressedAnalog_PrepareProtocolRouting()) return false;
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return true;

    // A completed/faulted std::thread remains joinable until its owner reaps it.
    // Reap before assigning the next generation to avoid std::terminate.
    if (g_thread.joinable())
    {
        try { g_thread.join(); }
        catch (...)
        {
            g_running.store(false, std::memory_order_release);
            return false;
        }
    }
    g_workerFaultRecord = {};
    g_workerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_readerFaultRecord = {};
    g_readerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);
    g_workerExited.store(false, std::memory_order_release);
    g_stop.store(false, std::memory_order_release);
    g_deviceChanged.store(false, std::memory_order_release);
    QueryPerformanceFrequency(&g_qpcFreq);
    QueryPerformanceCounter(&g_qpcStart);
    g_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_responseEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_readerExitEvent = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    if (!g_wakeEvent || !g_responseEvent || !g_readerExitEvent)
    {
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        if (g_responseEvent) CloseHandle(g_responseEvent);
        if (g_readerExitEvent) CloseHandle(g_readerExitEvent);
        g_wakeEvent = g_responseEvent = g_readerExitEvent = nullptr;
        g_workerExited.store(true, std::memory_order_release);
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"addressed", L"start.failed", L"stage=create_events");
        return false;
    }

    // The startup routing pass already proved and reserved the device before UAP.
    try { g_thread = std::thread(AddressedWorkerEntry); }
    catch (...)
    {
        CloseHandle(g_wakeEvent);
        CloseHandle(g_responseEvent);
        CloseHandle(g_readerExitEvent);
        g_wakeEvent = g_responseEvent = g_readerExitEvent = nullptr;
        g_workerExited.store(true, std::memory_order_release);
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"addressed", L"start.failed", L"stage=create_thread");
        return false;
    }
    if (g_workerExited.load(std::memory_order_acquire))
    {
        if (g_thread.joinable()) g_thread.join();
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        if (g_responseEvent) CloseHandle(g_responseEvent);
        if (g_readerExitEvent) CloseHandle(g_readerExitEvent);
        g_wakeEvent = g_responseEvent = g_readerExitEvent = nullptr;
        g_running.store(false, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"addressed", L"start.failed", L"stage=worker_early_exit");
        return false;
    }
    StabilityTrace_Write(L"INFO", L"addressed", L"start.ok");
    return true;
}

void AddressedAnalog_Stop()
{
    if (!g_running.load(std::memory_order_acquire) && !g_thread.joinable()) return;
    StabilityTrace_Write(L"INFO", L"addressed", L"stop.begin", L"joinable=%d", g_thread.joinable() ? 1 : 0);
    g_stop.store(true, std::memory_order_release);
    g_deviceChanged.store(true, std::memory_order_release);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
    if (g_responseEvent) SetEvent(g_responseEvent);
    CancelReaderIo();
    if (g_thread.joinable()) g_thread.join();
    if (g_wakeEvent) CloseHandle(g_wakeEvent);
    if (g_responseEvent) CloseHandle(g_responseEvent);
    if (g_readerExitEvent) CloseHandle(g_readerExitEvent);
    g_wakeEvent = g_responseEvent = g_readerExitEvent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_claimMutex);
        g_claim = ClaimState{};
    }
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"addressed", L"neutralized", L"reason=stop");
    StabilityTrace_Write(L"INFO", L"addressed", L"stop.end");
}

void AddressedAnalog_NotifyDeviceChange()
{
    g_deviceChanged.store(true, std::memory_order_release);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
    if (g_responseEvent) SetEvent(g_responseEvent);
    CancelReaderIo();
}

bool AddressedAnalog_IsProtocolDevicePresent()
{
    return g_claimedVendorId.load(std::memory_order_acquire) != 0 &&
        g_claimedProductId.load(std::memory_order_acquire) != 0;
}

bool AddressedAnalog_IsConnected()
{
    if (!g_connected.load(std::memory_order_acquire)) return false;
    const ULONGLONG last = g_lastResponseMs.load(std::memory_order_acquire);
    const ULONGLONG now = GetTickCount64();
    return last && now >= last && now - last <= kFreshMs;
}

bool AddressedAnalog_OwnsHid(std::uint16_t hidUsage)
{
    if (!hidUsage || hidUsage >= 256 || !AddressedAnalog_IsConnected()) return false;
    const ULONGLONG sample = g_sampleMs[hidUsage].load(std::memory_order_acquire);
    const ULONGLONG now = GetTickCount64();
    return sample && now >= sample && now - sample <= kFreshMs;
}

std::uint16_t AddressedAnalog_GetMilli(std::uint16_t hidUsage)
{
    if (!AddressedAnalog_OwnsHid(hidUsage)) return 0;
    return g_milli[hidUsage].load(std::memory_order_acquire);
}

void AddressedAnalog_GetTelemetry(AddressedAnalogTelemetry* out)
{
    if (!out) return;
    AddressedAnalogTelemetry t{};
    t.present = g_claimedProductId.load(std::memory_order_acquire) != 0;
    t.connected = AddressedAnalog_IsConnected();
    t.vendorId = g_claimedVendorId.load(std::memory_order_relaxed);
    t.productId = g_claimedProductId.load(std::memory_order_relaxed);
    t.mappedKeys = g_mappedKeys.load(std::memory_order_relaxed);
    t.inputReportBytes = g_claimedInputBytes.load(std::memory_order_relaxed);
    t.outputReportBytes = g_claimedOutputBytes.load(std::memory_order_relaxed);
    t.pollAttempts = g_pollAttempts.load(std::memory_order_relaxed);
    t.pollSuccess = g_pollSuccess.load(std::memory_order_relaxed);
    t.pollFail = g_pollFail.load(std::memory_order_relaxed);
    const ULONGLONG last = g_lastResponseMs.load(std::memory_order_relaxed);
    const ULONGLONG now = GetTickCount64();
    if (last != 0 && now >= last)
        t.lastResponseAgeMs = static_cast<std::uint32_t>(
            std::min<ULONGLONG>(now - last, 0xffffffffull));
    for (const auto& value : g_milli)
        if (value.load(std::memory_order_relaxed) != 0) ++t.activeKeys;
    *out = t;
}


namespace
{
void AddressedAnalog_FillGenericTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{};
    AddressedAnalogTelemetry t{};
    AddressedAnalog_GetTelemetry(&t);
    out->present = t.present;
    out->connected = t.connected;
    out->vendorId = t.vendorId;
    out->productId = t.productId;
    out->usagePage = 0xFF60;
    out->usage = 0x0061;
    out->mappedKeys = t.mappedKeys;
    out->activeKeys = t.activeKeys;
    out->nominalRawLevels = 1001u;
    out->inputReportBytes = t.inputReportBytes;
    out->outputReportBytes = t.outputReportBytes;
    out->lastUpdateAgeMs = t.lastResponseAgeMs;
    out->successfulUpdates = t.pollSuccess;
    out->failedUpdates = t.pollFail;
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"09/94/02 addressed polling, up to 9 keys/request, mapped=%u",
        static_cast<unsigned>(t.mappedKeys));
}
}

const NativeAnalogBackendDescriptor& AddressedAnalog_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "addressed-099402",
        L"Addressed Analog 09/94/02",
        NativeAnalogProtocol::Addressed09402,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe |
            NativeAnalogBackendFlag_DynamicVidPid,
        &AddressedAnalog_PrepareProtocolRouting,
        &AddressedAnalog_Start,
        [](halljoy::lifecycle::GenerationId generation) {
            AddressedAnalog_Stop();
            return NativeAnalogBackendStopJoined(generation);
        },
        &AddressedAnalog_NotifyDeviceChange,
        &AddressedAnalog_IsProtocolDevicePresent,
        &AddressedAnalog_IsConnected,
        &AddressedAnalog_OwnsHid,
        &AddressedAnalog_GetMilli,
        &AddressedAnalog_FillGenericTelemetry,
    };
    return descriptor;
}
