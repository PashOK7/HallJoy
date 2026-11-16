#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "usb_path_identity.h"
#include "native_analog_backend.h"
#include "mad68_dual_trial_protocol.h"
#include "support_log.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"

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
constexpr std::uint16_t kVendorId = 0x28E9;
constexpr std::uint16_t kProductId = 0x3265;
constexpr USAGE kControlUsagePage = 0xFF87;
constexpr USAGE kControlUsage = 0x0020;
constexpr USAGE kStreamUsagePage = 0xFF88;
constexpr USAGE kStreamUsage = 0x0021;
constexpr std::uint8_t kControlReportId = 0x06;
constexpr std::uint8_t kStreamReportId = 0x07;
constexpr std::size_t kControlReportBytes = 64;
constexpr std::size_t kStreamReportBytes = 3;
constexpr DWORD kReadSliceMs = 50;

constexpr DWORD kAckTimeoutMs = 750;

constexpr DWORD kStopTimeoutMs = 3000;

using ControlReport = std::array<std::uint8_t, kControlReportBytes>;
using StreamReport = std::array<std::uint8_t, kStreamReportBytes>;

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

struct Candidate { std::wstring path, parentKey; HIDD_ATTRIBUTES attributes{}; HIDP_CAPS caps{}; };
struct DevicePair { Candidate control, stream; };


struct KeyMapping
{
    std::uint8_t type = 0, modifier = 0, usage = 0;
    bool keyboardUsage = false;
};
struct Stats
{
    std::uint64_t reports = 0, report6 = 0, report7 = 0, other = 0;
    std::uint64_t travelPairs = 0, malformed = 0, writes = 0, failures = 0;
    halljoy::mad68_dual_trial::Decoder decoder;

    std::map<std::uint8_t, KeyMapping> mapping;
};

std::atomic<bool> g_prepared{false}, g_present{false}, g_connected{false}, g_running{false}, g_stop{false};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0};
std::atomic<std::uint64_t> g_reports{0}, g_pairs{0}, g_failures{0}, g_lastMs{0};
std::atomic<bool> g_mapReady{false};
std::atomic<std::uint32_t> g_mappedKeys{0}, g_maxRaw{3250};
std::array<std::atomic<std::uint8_t>, 256> g_hidAtKeyIndex{};
std::array<std::atomic<bool>, 256> g_hasHid{};
halljoy::observation::Source g_observation;
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::mutex g_serviceMutex, g_handleMutex;
HANDLE g_thread = nullptr, g_wake = nullptr, g_activeHandle = INVALID_HANDLE_VALUE;


std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) { hash ^= static_cast<std::uint16_t>(towlower(ch)); hash *= 1099511628211ull; }
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
        DWORD drained = 0; operation.CancelAndDrain(&drained, &error);
        // Keep a fragment that completed between the timeout and CancelIoEx.
        if (error == ERROR_SUCCESS && drained != 0) { if (transferred) *transferred = drained; return true; }
        SetLastError(waitError ? waitError : ERROR_GEN_FAILURE); return false;
    }
    const bool ok = operation.Finish(transferred, &error, false); if (!ok) SetLastError(error); return ok;
}

bool HasReportId(PHIDP_PREPARSED_DATA preparsed, const HIDP_CAPS& caps, HIDP_REPORT_TYPE type, std::uint8_t wanted)
{
    const auto buttons = [&](USHORT count) {
        if (!count) return false; std::vector<HIDP_BUTTON_CAPS> values(count);
        if (HidP_GetButtonCaps(type, values.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) return false;
        return std::any_of(values.begin(), values.begin() + count, [wanted](const HIDP_BUTTON_CAPS& c) { return c.ReportID == wanted; });
    };
    const auto values = [&](USHORT count) {
        if (!count) return false; std::vector<HIDP_VALUE_CAPS> entries(count);
        if (HidP_GetValueCaps(type, entries.data(), &count, preparsed) != HIDP_STATUS_SUCCESS) return false;
        return std::any_of(entries.begin(), entries.begin() + count, [wanted](const HIDP_VALUE_CAPS& c) { return c.ReportID == wanted; });
    };
    return buttons(type == HidP_Input ? caps.NumberInputButtonCaps : caps.NumberOutputButtonCaps) ||
        values(type == HidP_Input ? caps.NumberInputValueCaps : caps.NumberOutputValueCaps);
}

std::vector<DevicePair> Enumerate(bool verbose)
{
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return {};
    std::vector<Candidate> controls, streams;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &guid, index, &iface))
        { if (GetLastError() == ERROR_NO_MORE_ITEMS) break; continue; }
        DWORD needed = 0; SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &needed, nullptr);
        if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data()); detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, nullptr, nullptr)) continue;
        // Opening every HID interface on the machine cost ~60 ms per discovery.
        // A path that proves another USB identity cannot be this device; any
        // other path is still opened and checked by its HID attributes.
        if (!halljoy::usb_path::MayBe(detail->DevicePath, kVendorId, kProductId)) {
            if (verbose) {
                std::uint16_t pathVid = 0, pathPid = 0;
                halljoy::usb_path::TryRead(detail->DevicePath, pathVid, pathPid);
                DebugLog_Write(L"[mad68dual.enumerate] path_hash=%016llX vid=%04X pid=%04X skipped_by_path=1", static_cast<unsigned long long>(HashPath(detail->DevicePath)), pathVid, pathPid);
            }
            continue;
        }
        Handle metadata(CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!metadata) continue;
        HIDD_ATTRIBUTES attributes{}; attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(metadata.value, &attributes) || attributes.VendorID != kVendorId || attributes.ProductID != kProductId) continue;
        PHIDP_PREPARSED_DATA preparsed = nullptr; if (!HidD_GetPreparsedData(metadata.value, &preparsed)) continue;
        HIDP_CAPS caps{}; const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        const bool in6 = status == HIDP_STATUS_SUCCESS && HasReportId(preparsed, caps, HidP_Input, kControlReportId);
        const bool in7 = status == HIDP_STATUS_SUCCESS && HasReportId(preparsed, caps, HidP_Input, kStreamReportId);
        const bool out6 = status == HIDP_STATUS_SUCCESS && HasReportId(preparsed, caps, HidP_Output, kControlReportId);
        HidD_FreePreparsedData(preparsed); if (status != HIDP_STATUS_SUCCESS) continue;
        const bool controlUsageMatch = caps.UsagePage == kControlUsagePage && caps.Usage == kControlUsage;
        const bool streamUsageMatch = caps.UsagePage == kStreamUsagePage && caps.Usage == kStreamUsage;
        // Exact model, collection usages, sizes and report IDs are mandatory.
        const bool control = controlUsageMatch && caps.InputReportByteLength == kControlReportBytes && caps.OutputReportByteLength == kControlReportBytes && in6 && out6;
        const bool stream = streamUsageMatch && caps.InputReportByteLength == kStreamReportBytes && caps.OutputReportByteLength == 0 && in7;
        if (verbose) DebugLog_Write(L"[mad68dual.enumerate] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u report06_in=%d report07_in=%d report06_out=%d control=%d stream=%d control_usage_match=%d stream_usage_match=%d", static_cast<unsigned long long>(HashPath(detail->DevicePath)), attributes.VendorID, attributes.ProductID, attributes.VersionNumber, caps.UsagePage, caps.Usage, caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, in6 ? 1 : 0, in7 ? 1 : 0, out6 ? 1 : 0, control ? 1 : 0, stream ? 1 : 0, controlUsageMatch ? 1 : 0, streamUsageMatch ? 1 : 0);
        if (control) controls.push_back({detail->DevicePath, L"", attributes, caps});
        if (stream) streams.push_back({detail->DevicePath, L"", attributes, caps});
    }
    SetupDiDestroyDeviceInfoList(set);
    if (verbose) SupportLog_Event("mad68dual.collections", (controls.size()<<16) | streams.size());
    std::vector<DevicePair> result;
    // Multiple identical devices are deliberately refused in this trial.
    if (controls.size() == 1 && streams.size() == 1) {
        DebugLog_Write(L"[mad68dual.pair] control_hash=%016llX stream_hash=%016llX singleton_exact=1", static_cast<unsigned long long>(HashPath(controls[0].path)), static_cast<unsigned long long>(HashPath(streams[0].path)));
        result.push_back({controls[0], streams[0]});
        return result;
    }
    return result;
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
        if (!handle_) return false; HidD_SetNumInputBuffers(handle_.value, 256);
        if (writable_) { std::lock_guard<std::mutex> lock(g_handleMutex); g_activeHandle = handle_.value; active_ = true; }
        return true;
    }
    void Flush() { HidD_FlushQueue(handle_.value); }
    bool Send(const ControlReport& report)
    {
        DebugLog_WriteBuffered(L"[mad68dual.tx] command=%02X enabled=%u bytes=64 data=%ls", report[1], report[8], Hex(report.data(), report.size()).c_str());
        ControlReport copy = report; DWORD sent = 0;
        return TimedIo(handle_.value, true, copy.data(), static_cast<DWORD>(copy.size()), kReadSliceMs, &sent) && sent == copy.size();
    }
    template <std::size_t N>
    bool Read(std::array<std::uint8_t, N>* out, DWORD timeout, DWORD* received)
    {
        if (!out) return false; out->fill(0); DWORD bytes = 0;
        if (!TimedIo(handle_.value, false, out->data(), static_cast<DWORD>(N), timeout, &bytes)) return false;
        if (!bytes || bytes > out->size()) { SetLastError(ERROR_BAD_LENGTH); return false; }
        if (received) *received = bytes; return true;
    }
private:
    void ReleaseActive()
    {
        if (!active_) return; std::lock_guard<std::mutex> lock(g_handleMutex);
        if (g_activeHandle == handle_.value) g_activeHandle = INVALID_HANDLE_VALUE; active_ = false;
    }
    Candidate candidate_; bool writable_ = false, active_ = false; Handle handle_{};
};

ControlReport BuildModeControl(std::uint8_t command, bool enabled)
{
    return halljoy::mad68_dual_trial::Command(command, 0, 1, enabled ? 1 : 0);
}
ControlReport BuildReadControl(std::uint8_t command, std::uint16_t offset, std::uint8_t length)
{
    return halljoy::mad68_dual_trial::Command(command, offset, length);
}

bool IsControlAck(const ControlReport& report, DWORD bytes, std::uint8_t command, bool enabled)
{
    return bytes >= 9 && report[0] == kControlReportId && report[1] == command && report[2] == 0 && report[3] == 0 && report[4] == 1 && report[7] == 0x55 && report[8] == (enabled ? 1 : 0);
}

bool ReadControlChunk(Session& session, std::uint8_t command, std::uint16_t offset, std::uint8_t length, std::vector<std::uint8_t>* out)
{
    // The official driver uses commands 0x12 and 0x16 with this exact frame
    // shape.  They are reads: command, byte offset and requested byte count.
    // Keep this deliberately independent of the active analog mode; a failed
    // optional map read must never prevent report-07 observation.
    if (!out || !length || length > kControlReportBytes - 8) return false;
    const auto request = BuildReadControl(command, offset, length);
    SupportLog_Event("mad68dual.read_request", (static_cast<std::uint64_t>(command)<<24) | (static_cast<std::uint64_t>(offset)<<8) | length);
    if (!session.Send(request))
    {
        DebugLog_Write(L"[mad68dual.mapping] read_send_failed command=%02X offset=%u length=%u win32=%lu", command, offset, length, GetLastError());
        return false;
    }
    const auto end = GetTickCount64() + kAckTimeoutMs;
    while (GetTickCount64() < end && !g_stop.load())
    {
        ControlReport response{}; DWORD bytes = 0;
        const auto remaining = static_cast<DWORD>(end > GetTickCount64() ? end - GetTickCount64() : 0);
        if (!session.Read(&response, std::min<DWORD>(kReadSliceMs, remaining), &bytes))
        {
            if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_OPERATION_ABORTED) continue;
            DebugLog_Write(L"[mad68dual.mapping] read_receive_failed command=%02X offset=%u win32=%lu", command, offset, GetLastError());
            return false;
        }
        if (bytes < 8 || response[0] != kControlReportId || response[1] != command || response[2] != static_cast<std::uint8_t>(offset) || response[3] != static_cast<std::uint8_t>(offset >> 8)) continue;
        if (response[7] != 0x55)
        {
            DebugLog_Write(L"[mad68dual.mapping] read_rejected command=%02X offset=%u result=%02X", command, offset, response[7]);
            return false;
        }
        if (response[4] != length || bytes < 8u + length)
        {
            DebugLog_Write(L"[mad68dual.mapping] read_short_response command=%02X offset=%u expected=%u reported=%u bytes=%lu", command, offset, length, response[4], bytes);
            return false;
        }
        out->insert(out->end(), response.begin() + 8, response.begin() + 8 + length);
        return true;
    }
    SupportLog_Event("mad68dual.read_timeout", command,SupportLog_Win32(WAIT_TIMEOUT));
    SetLastError(WAIT_TIMEOUT);
    return false;
}

bool ReadControlData(Session& session, std::uint8_t command, std::size_t length, std::vector<std::uint8_t>* out)
{
    if (!out || !length || length > 0xffffu) return false;
    out->clear(); out->reserve(length);
    for (std::size_t offset = 0; offset < length;)
    {
        if (g_stop.load()) return false;
        const auto chunk = static_cast<std::uint8_t>(std::min<std::size_t>(kControlReportBytes - 8, length - offset));
        if (!ReadControlChunk(session, command, static_cast<std::uint16_t>(offset), chunk, out)) return false;
        offset += chunk;
    }
    return out->size() == length;
}

bool DecodeKeyboardTriplet(std::uint8_t type, std::uint8_t middle, std::uint8_t last, KeyMapping* out)
{
    if (!out) return false;
    *out = {type, middle, last, false};
    if (type != 0x10) return false; // non-keyboard consumer/system/Fn action
    if (last) { out->usage = last; out->keyboardUsage = true; return true; }
    // The driver's codeValues encodes modifiers as [0x10, modifier-bit, 0].
    switch (middle)
    {
    case 0x01: out->usage = 0xe0; break; case 0x02: out->usage = 0xe1; break;
    case 0x04: out->usage = 0xe2; break; case 0x08: out->usage = 0xe3; break;
    case 0x10: out->usage = 0xe4; break; case 0x20: out->usage = 0xe5; break;
    case 0x40: out->usage = 0xe6; break; case 0x80: out->usage = 0xe7; break;
    default: return false;
    }
    out->keyboardUsage = true; return true;
}

void ClearVisualAnalog()
{
    g_observation.Clear();
    g_mapReady.store(false, std::memory_order_release); g_mappedKeys.store(0, std::memory_order_release); g_maxRaw.store(3250, std::memory_order_release);
    for (std::size_t i = 0; i < g_hidAtKeyIndex.size(); ++i)
    {
        g_hidAtKeyIndex[i].store(0, std::memory_order_relaxed);
        g_hasHid[i].store(false, std::memory_order_relaxed);
        g_observation.Store(g_milli[i],static_cast<std::uint16_t>(i),0, std::memory_order_relaxed);
    }
}

bool ReadDefaultKeyMapping(Session& session, Stats* stats)
{
    if (!stats) return false;
    ClearVisualAnalog();
    std::vector<std::uint8_t> info;
    if (!ReadControlData(session, 0x12, 64, &info))
    {
        DebugLog_Write(L"[mad68dual.mapping] device_info_unavailable; continuing_without_mapping=1");
        return false;
    }
    const std::size_t slots = info.size() > 4 ? info[4] : 0; // official driver: key_rect_size = byte[4] * 3
    const std::uint16_t reportedMaxRaw = info.size() > 18 ? static_cast<std::uint16_t>(info[17] | info[18] << 8) : 0;
    const std::uint32_t maxRaw = halljoy::mad68_dual_trial::Maximum;
    DebugLog_Write(L"[mad68dual.mapping] device_info key_slots=%u key_rect_bytes=%u reported_pid=%02X%02X max_raw=%u reported_max_invalid=%d", static_cast<unsigned>(slots), static_cast<unsigned>(slots * 3), info[3], info[2], maxRaw, reportedMaxRaw == 0 || reportedMaxRaw > 4095 ? 1 : 0);
    SupportLog_Event("mad68dual.map_slots", slots);
    if (!slots || slots > 128)
    {
        DebugLog_Write(L"[mad68dual.mapping] empty_key_rect; continuing_without_mapping=1");
        return false;
    }
    std::vector<std::uint8_t> rect;
    if (!ReadControlData(session, 0x16, slots * 3, &rect))
    {
        DebugLog_Write(L"[mad68dual.mapping] default_key_rect_unavailable; continuing_without_mapping=1");
        return false;
    }
    std::size_t keyboard = 0, unsupported = 0;
    for (std::size_t slot = 0; slot < slots; ++slot)
    {
        KeyMapping entry{};
        const auto type = rect[slot * 3], middle = rect[slot * 3 + 1], last = rect[slot * 3 + 2];
        if (DecodeKeyboardTriplet(type, middle, last, &entry))
        {
            ++keyboard;
            g_hidAtKeyIndex[slot].store(entry.usage, std::memory_order_relaxed);
            g_hasHid[entry.usage].store(true, std::memory_order_relaxed);
        }
        else ++unsupported;
        stats->mapping.emplace(static_cast<std::uint8_t>(slot), entry);
        DebugLog_Write(L"[mad68dual.mapping.slot] key_index=%u triplet=%02X:%02X:%02X keyboard_usage=%s", static_cast<unsigned>(slot), type, middle, last, entry.keyboardUsage ? Hex(&entry.usage, 1).c_str() : L"none");
    }
    g_maxRaw.store(maxRaw, std::memory_order_release);
    g_mappedKeys.store(static_cast<std::uint32_t>(keyboard), std::memory_order_release);
    g_mapReady.store(keyboard != 0, std::memory_order_release);
    DebugLog_Write(L"[mad68dual.mapping] complete slots=%u keyboard=%u non_keyboard=%u visual_ready=%d", static_cast<unsigned>(slots), static_cast<unsigned>(keyboard), static_cast<unsigned>(unsupported), keyboard ? 1 : 0);
    SupportLog_Event("mad68dual.mapping", keyboard);
    return keyboard >= 60 && g_hasHid[0x1a] && g_hasHid[0x04] && g_hasHid[0x16] && g_hasHid[0x07];
}

void PublishVisualAnalog(std::uint8_t keyIndex, std::uint16_t raw12)
{
    if (!g_mapReady.load(std::memory_order_acquire)) return;
    const auto hid = g_hidAtKeyIndex[keyIndex].load(std::memory_order_relaxed);
    const auto maxRaw = g_maxRaw.load(std::memory_order_acquire);
    if (!hid || !maxRaw) return;
    const auto milli = static_cast<std::uint16_t>(std::min<std::uint32_t>(1000, (static_cast<std::uint32_t>(raw12) * 1000u + maxRaw / 2u) / maxRaw));
    g_observation.Store(g_milli[hid],static_cast<std::uint16_t>(hid),milli, std::memory_order_release);
}

void ProcessStreamReport(const StreamReport& report, DWORD bytes, Stats* stats)
{
    if (!stats) return;
    ++stats->reports; g_reports.fetch_add(1); g_lastMs.store(GetTickCount64());
    std::uint8_t key=0; std::uint16_t raw=0;
    if (!stats->decoder.Push(report.data(), bytes, GetTickCount64(), key, raw)) return;
    ++stats->travelPairs; g_pairs.fetch_add(1);
    if (stats->travelPairs == 1) SupportLog_Event("mad68dual.first_pair", 1);
    PublishVisualAnalog(key, raw);
}

bool Listen(Session& session, Stats* stats, DWORD durationMs, bool obeyStop)
{
    const auto end = GetTickCount64() + durationMs;
    while (GetTickCount64() < end && (!obeyStop || !g_stop.load(std::memory_order_acquire)))
    {
        StreamReport report{}; DWORD bytes = 0; const auto remaining = static_cast<DWORD>(end > GetTickCount64() ? end - GetTickCount64() : 0);
        if (session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining), &bytes)) ProcessStreamReport(report, bytes, stats);
        else if (GetLastError() != WAIT_TIMEOUT && GetLastError() != ERROR_OPERATION_ABORTED)
        { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mad68dual.rx_error] win32=%lu", GetLastError()); return false; }
    }
    return true;
}

bool SendAndAwait(Session& session, Stats* stats, std::uint8_t command, bool enabled)
{
    const auto request = BuildModeControl(command, enabled);
    if (!session.Send(request)) { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mad68dual.control] send_failed command=%02X enabled=%d win32=%lu", command, enabled ? 1 : 0, GetLastError()); return false; }
    ++stats->writes; const auto end = GetTickCount64() + kAckTimeoutMs;
    while (GetTickCount64() < end)
    {
        ControlReport report{}; DWORD bytes = 0; const auto remaining = static_cast<DWORD>(end > GetTickCount64() ? end - GetTickCount64() : 0);
        if (!session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining), &bytes))
        {
            if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_OPERATION_ABORTED) continue;
            ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[mad68dual.control] ack_read_failed command=%02X enabled=%d win32=%lu", command, enabled ? 1 : 0, GetLastError()); return false;
        }
        DebugLog_WriteBuffered(L"[mad68dual.rx_control] bytes=%lu data=%ls", bytes, Hex(report.data(), bytes).c_str());
        if (IsControlAck(report, bytes, command, enabled)) { DebugLog_Write(L"[mad68dual.control] ack command=%02X enabled=%d", command, enabled ? 1 : 0); return true; }
        if (bytes >= 8 && report[0] == kControlReportId && report[1] == command && report[7] == 0x0f) { DebugLog_Write(L"[mad68dual.control] rejected command=%02X enabled=%d", command, enabled ? 1 : 0); return false; }
    }
    SupportLog_Event("mad68dual.ack_timeout", enabled ? 1 : 0,SupportLog_Win32(WAIT_TIMEOUT)); SetLastError(WAIT_TIMEOUT); return false;
}

struct ModeGuard {
    Session& control; Stats& stats;
    ~ModeGuard() noexcept {
        g_connected.store(false, std::memory_order_release);
        ClearVisualAnalog();
        try {
            const bool exited=SendAndAwait(control, &stats, 0x36, false);
            SupportLog_Event("mad68dual.exit_ack", exited ? 1 : 0);
            if (!exited) SupportLog_ReportFailure("mad68dual.exit_failed", GetLastError());
        } catch (...) { SupportLog_ReportFailure("mad68dual.exit_exception", 1); }
    }
};
bool RunCandidate(const DevicePair& pair)
{
    Stats stats{};
    SupportLog_Event("mad68dual.session", pair.control.attributes.VersionNumber);
    Session control(pair.control, true), stream(pair.stream, false);
    if (!control.Open() || !stream.Open()) {
        SupportLog_ReportFailure("mad68dual.open", GetLastError()); return false;
    }
    g_inputBytes.store(pair.stream.caps.InputReportByteLength);
    g_outputBytes.store(pair.control.caps.OutputReportByteLength);
    if (!ReadDefaultKeyMapping(control, &stats)) {
        SupportLog_ReportFailure("mad68dual.mapping_failed", GetLastError());
        ClearVisualAnalog(); return false;
    }
    if (g_stop.load()) { ClearVisualAnalog(); return false; }
    stream.Flush();
    ModeGuard cleanup{control, stats};
    if (!SendAndAwait(control, &stats, 0x36, true)) {
        SupportLog_ReportFailure("mad68dual.enter_failed", GetLastError()); return false;
    }
    SupportLog_Event("mad68dual.enter_ack", 1);
    g_connected.store(true, std::memory_order_release);
    while (!g_stop.load()) {
        if (!Listen(stream, &stats, 1000, true)) {
            SupportLog_ReportFailure("mad68dual.stream_failed", GetLastError()); return false;
        }
    }
    SupportLog_Event("mad68dual.pairs", stats.travelPairs);
    return true;
}

unsigned __stdcall Worker(void*) noexcept
{
    try {
    while (!g_stop.load(std::memory_order_acquire))
    {
        bool attempted = false;
    for (const auto& candidate : Enumerate(false))
        {
            if (NativeAnalogRouting_IsClaimed(candidate.control.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(candidate.control.path.c_str(), NativeAnalogProtocol::Mad68DualTrial)) continue;
            NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.control.path.c_str(), NativeAnalogProtocol::Mad68DualTrial);
            g_present.store(true, std::memory_order_release); attempted = true; (void)RunCandidate(candidate);
            if (g_stop.load(std::memory_order_acquire)) break;
            // A failed acknowledgement is evidence to log, not a reason to repeat
            // a control-mode probe automatically against the user's keyboard.
            break;
        }
        if (attempted) break;
        if (!g_stop.load(std::memory_order_acquire)) { g_present.store(false, std::memory_order_release); if (g_wake) { WaitForSingleObject(g_wake, 1000); ResetEvent(g_wake); } }
    }
    } catch (...) { SupportLog_ReportFailure("mad68dual.worker_exception", 1); }
    g_connected.store(false, std::memory_order_release); ClearVisualAnalog(); g_running.store(false, std::memory_order_release); return 0;
}

bool Prepare()
{
    bool any = false;
    for (const auto& candidate : Enumerate(true))
    {
        const bool claimed = NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.control.path.c_str(), NativeAnalogProtocol::Mad68DualTrial);
        if (claimed) NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.stream.path.c_str(), NativeAnalogProtocol::Mad68DualTrial);
        DebugLog_Write(L"[mad68dual.prepare] control_hash=%016llX stream_hash=%016llX exact_split_identity=1 claimed=%d", static_cast<unsigned long long>(HashPath(candidate.control.path)), static_cast<unsigned long long>(HashPath(candidate.stream.path)), claimed ? 1 : 0);
        any = claimed || NativeAnalogRouting_IsClaimedBy(candidate.control.path.c_str(), NativeAnalogProtocol::Mad68DualTrial) || any;
    }
    g_prepared.store(true, std::memory_order_release); g_present.store(any, std::memory_order_release); return any;
}

bool Start()
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_prepared.load(std::memory_order_acquire)) (void)Prepare(); if (g_thread) return g_running.load(std::memory_order_acquire);
    g_stop.store(false, std::memory_order_release); g_running.store(true, std::memory_order_release); g_wake = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_wake) { g_running.store(false, std::memory_order_release); return false; }
    unsigned id = 0; g_thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Worker, nullptr, 0, &id));
    if (!g_thread) { CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); return false; }
    SupportLog_Event("mad68dual.start", id); return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release); if (g_wake) SetEvent(g_wake);
    // All operations have bounded slices. Do not cancel a concurrently issued exit command.
    const DWORD wait = WaitForSingleObject(g_thread, kStopTimeoutMs);
    if (wait != WAIT_OBJECT_0) return NativeAnalogBackendStopFailed(generation, halljoy::lifecycle::LifecycleErrorCode::StopTimedOut, wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError());
    CloseHandle(g_thread); g_thread = nullptr; if (g_wake) CloseHandle(g_wake); g_wake = nullptr; g_running.store(false, std::memory_order_release); g_connected.store(false, std::memory_order_release); ClearVisualAnalog(); return NativeAnalogBackendStopJoined(generation);
}

void Notify() { if (g_wake) SetEvent(g_wake); }
bool Present() { return g_present.load(std::memory_order_acquire); }
bool Connected() { return g_connected.load(std::memory_order_acquire) && g_mapReady.load(std::memory_order_acquire); }
bool Owns(std::uint16_t hid) { return hid && hid < 256 && Connected() && g_hasHid[hid].load(std::memory_order_acquire); }
std::uint16_t Get(std::uint16_t hid) { return Owns(hid) ? g_milli[hid].load(std::memory_order_acquire) : 0; }
void Telemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return; *out = {}; out->present = Present(); out->connected = Connected(); out->vendorId = kVendorId; out->productId = kProductId; out->usagePage = kControlUsagePage; out->usage = kControlUsage; out->mappedKeys = g_mappedKeys.load(); out->inputReportBytes = g_inputBytes.load(); out->outputReportBytes = g_outputBytes.load(); out->successfulUpdates = g_pairs.load(); out->failedUpdates = g_failures.load();
    const auto last = g_lastMs.load(), now = GetTickCount64(); out->lastUpdateAgeMs = last && now >= last ? static_cast<std::uint32_t>(std::min<std::uint64_t>(now - last, 0xffffffffull)) : 0;
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE, L"MAD 68 V2 Dual trial: %u mapped; %llu pairs; firmware limits apply", g_mappedKeys.load(), static_cast<unsigned long long>(g_pairs.load()));
}
}

const NativeAnalogBackendDescriptor& Mad68DualTrial_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), "mad68-dual-trial", L"MADLIONS MAD 68 V2 Dual limited analog trial", NativeAnalogProtocol::Mad68DualTrial, NativeAnalogStartPhase::BeforeUap, NativeAnalogBackendFlag_StreamTransport | NativeAnalogBackendFlag_ReversibleControlProbe, &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry, nullptr, +[](halljoy::observation::Sink sink) { g_observation.Visit(sink,&Owns,&Get); }};
    return descriptor;
}
