#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "titan68_turbo_diagnostic_backend.h"
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
constexpr std::uint16_t kProductId = 0x31FD;
constexpr USAGE kControlUsagePage = 0xFF87;
constexpr USAGE kControlUsage = 0x0020;
constexpr USAGE kStreamUsagePage = 0xFF88;
constexpr USAGE kStreamUsage = 0x0021;
constexpr std::uint8_t kControlReportId = 0x06;
constexpr std::uint8_t kStreamReportId = 0x07;
constexpr std::size_t kControlReportBytes = 64;
constexpr std::size_t kStreamReportBytes = 3;
constexpr DWORD kReadSliceMs = 50;
constexpr DWORD kPassiveListenMs = 750;
constexpr DWORD kAckTimeoutMs = 750;
constexpr DWORD kObservationMs = 10000;
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
struct HalfSample { bool pending = false; std::uint8_t low = 0; };
struct Range { std::uint16_t minimum = 4095; std::uint16_t maximum = 0; std::uint64_t samples = 0; };
struct KeyMapping
{
    std::uint8_t type = 0, modifier = 0, usage = 0;
    bool keyboardUsage = false;
};
struct Stats
{
    std::uint64_t reports = 0, report6 = 0, report7 = 0, other = 0;
    std::uint64_t travelPairs = 0, malformed = 0, writes = 0, failures = 0;
    std::map<std::uint8_t, HalfSample> halves;
    std::map<std::uint8_t, Range> travel;
    std::map<std::uint8_t, KeyMapping> mapping;
};

std::atomic<bool> g_prepared{false}, g_present{false}, g_connected{false}, g_running{false}, g_stop{false}, g_rawInputReady{false};
std::atomic<std::uint32_t> g_inputBytes{0}, g_outputBytes{0};
std::atomic<std::uint64_t> g_reports{0}, g_pairs{0}, g_failures{0}, g_lastMs{0};
std::atomic<bool> g_mapReady{false};
std::atomic<std::uint32_t> g_mappedKeys{0}, g_maxRaw{4095};
std::array<std::atomic<std::uint8_t>, 256> g_hidAtKeyIndex{};
std::array<std::atomic<bool>, 256> g_hasHid{};
std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::mutex g_serviceMutex, g_handleMutex, g_rawMutex;
HANDLE g_thread = nullptr, g_wake = nullptr, g_activeHandle = INVALID_HANDLE_VALUE;
std::unordered_map<std::uintptr_t, bool> g_rawTargets;

std::uint64_t HashPath(const std::wstring& value)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) { hash ^= static_cast<std::uint16_t>(towlower(ch)); hash *= 1099511628211ull; }
    return hash;
}

std::wstring ParentKey(const std::wstring& path)
{
    std::wstring key = path; std::transform(key.begin(), key.end(), key.begin(), towlower);
    const auto mi = key.find(L"&mi_");
    if (mi != std::wstring::npos) key.erase(mi, 6);
    return key;
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
        // Descriptor usage is diagnostic evidence, not an automatic dead-end:
        // the command is still constrained by the exact VID/PID, one unique
        // 64-byte report-06 output endpoint and one unique 3-byte report-07
        // input endpoint. This lets a harmless collection-label variation be
        // recorded instead of wasting a volunteer test.
        const bool control = caps.InputReportByteLength == kControlReportBytes && caps.OutputReportByteLength == kControlReportBytes && in6 && out6;
        const bool stream = caps.InputReportByteLength == kStreamReportBytes && caps.OutputReportByteLength == 0 && in7;
        if (verbose) DebugLog_Write(L"[titan68.enumerate] path_hash=%016llX vid=%04X pid=%04X version=%04X usage=%04X:%04X in=%u out=%u feature=%u report06_in=%d report07_in=%d report06_out=%d control=%d stream=%d control_usage_match=%d stream_usage_match=%d", static_cast<unsigned long long>(HashPath(detail->DevicePath)), attributes.VendorID, attributes.ProductID, attributes.VersionNumber, caps.UsagePage, caps.Usage, caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, in6 ? 1 : 0, in7 ? 1 : 0, out6 ? 1 : 0, control ? 1 : 0, stream ? 1 : 0, controlUsageMatch ? 1 : 0, streamUsageMatch ? 1 : 0);
        if (control) controls.push_back({detail->DevicePath, ParentKey(detail->DevicePath), attributes, caps});
        if (stream) streams.push_back({detail->DevicePath, ParentKey(detail->DevicePath), attributes, caps});
    }
    SetupDiDestroyDeviceInfoList(set);
    std::vector<DevicePair> result;
    // A single exact control endpoint and a single exact stream endpoint are
    // already an unambiguous physical Titan68 topology.  Windows may encode
    // different collection suffixes after MI_xx, so do not reject this safe
    // singleton pair on a path-string heuristic alone.
    if (controls.size() == 1 && streams.size() == 1) {
        DebugLog_Write(L"[titan68.pair] control_hash=%016llX stream_hash=%016llX singleton_exact=1", static_cast<unsigned long long>(HashPath(controls[0].path)), static_cast<unsigned long long>(HashPath(streams[0].path)));
        result.push_back({controls[0], streams[0]});
        return result;
    }
    for (const auto& control : controls) for (const auto& stream : streams)
        if (control.parentKey == stream.parentKey) {
            DebugLog_Write(L"[titan68.pair] control_hash=%016llX stream_hash=%016llX same_parent=1", static_cast<unsigned long long>(HashPath(control.path)), static_cast<unsigned long long>(HashPath(stream.path)));
            result.push_back({control, stream});
        }
    if (verbose && result.empty()) DebugLog_Write(L"[titan68.pair] no_exact_split_pair controls=%u streams=%u", static_cast<unsigned>(controls.size()), static_cast<unsigned>(streams.size()));
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
    bool Send(const ControlReport& report)
    {
        DebugLog_WriteBuffered(L"[titan68.tx] command=%02X enabled=%u bytes=64 data=%ls", report[1], report[8], Hex(report.data(), report.size()).c_str());
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
    ControlReport report{};
    report[0] = kControlReportId; report[1] = command; report[4] = 1; report[8] = enabled ? 1 : 0;
    const std::uint16_t sum = static_cast<std::uint16_t>(report[1] + report[2] + report[3] + report[4] + report[8]);
    report[5] = static_cast<std::uint8_t>(sum); report[6] = static_cast<std::uint8_t>(sum >> 8);
    return report;
}

ControlReport BuildReadControl(std::uint8_t command, std::uint16_t offset, std::uint8_t length)
{
    ControlReport report{};
    report[0] = kControlReportId; report[1] = command;
    report[2] = static_cast<std::uint8_t>(offset); report[3] = static_cast<std::uint8_t>(offset >> 8);
    report[4] = length;
    const std::uint16_t sum = static_cast<std::uint16_t>(report[1] + report[2] + report[3] + report[4]);
    report[5] = static_cast<std::uint8_t>(sum); report[6] = static_cast<std::uint8_t>(sum >> 8);
    return report;
}

bool IsControlAck(const ControlReport& report, DWORD bytes, std::uint8_t command)
{
    return bytes >= 8 && report[0] == kControlReportId && report[1] == command && report[2] == 0 && report[3] == 0 && report[4] == 1 && report[7] == 0x55;
}

bool ReadControlChunk(Session& session, std::uint8_t command, std::uint16_t offset, std::uint8_t length, std::vector<std::uint8_t>* out)
{
    // The official driver uses commands 0x12 and 0x16 with this exact frame
    // shape.  They are reads: command, byte offset and requested byte count.
    // Keep this deliberately independent of the active analog mode; a failed
    // optional map read must never prevent report-07 observation.
    if (!out || !length || length > kControlReportBytes - 8) return false;
    const auto request = BuildReadControl(command, offset, length);
    if (!session.Send(request))
    {
        DebugLog_Write(L"[titan68.mapping] read_send_failed command=%02X offset=%u length=%u win32=%lu", command, offset, length, GetLastError());
        return false;
    }
    const auto end = GetTickCount64() + kAckTimeoutMs;
    while (GetTickCount64() < end)
    {
        ControlReport response{}; DWORD bytes = 0;
        const auto remaining = static_cast<DWORD>(end - GetTickCount64());
        if (!session.Read(&response, std::min<DWORD>(kReadSliceMs, remaining), &bytes))
        {
            if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_OPERATION_ABORTED) continue;
            DebugLog_Write(L"[titan68.mapping] read_receive_failed command=%02X offset=%u win32=%lu", command, offset, GetLastError());
            return false;
        }
        if (bytes < 8 || response[0] != kControlReportId || response[1] != command || response[2] != static_cast<std::uint8_t>(offset) || response[3] != static_cast<std::uint8_t>(offset >> 8)) continue;
        if (response[7] != 0x55)
        {
            DebugLog_Write(L"[titan68.mapping] read_rejected command=%02X offset=%u result=%02X", command, offset, response[7]);
            return false;
        }
        if (response[4] != length || bytes < 8u + length)
        {
            DebugLog_Write(L"[titan68.mapping] read_short_response command=%02X offset=%u expected=%u reported=%u bytes=%lu", command, offset, length, response[4], bytes);
            return false;
        }
        out->insert(out->end(), response.begin() + 8, response.begin() + 8 + length);
        return true;
    }
    DebugLog_Write(L"[titan68.mapping] read_timeout command=%02X offset=%u length=%u", command, offset, length);
    return false;
}

bool ReadControlData(Session& session, std::uint8_t command, std::size_t length, std::vector<std::uint8_t>* out)
{
    if (!out || !length || length > 0xffffu) return false;
    out->clear(); out->reserve(length);
    for (std::size_t offset = 0; offset < length;)
    {
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
    g_mapReady.store(false, std::memory_order_release); g_mappedKeys.store(0, std::memory_order_release); g_maxRaw.store(4095, std::memory_order_release);
    for (std::size_t i = 0; i < g_hidAtKeyIndex.size(); ++i)
    {
        g_hidAtKeyIndex[i].store(0, std::memory_order_relaxed);
        g_hasHid[i].store(false, std::memory_order_relaxed);
        g_milli[i].store(0, std::memory_order_relaxed);
    }
}

bool ReadDefaultKeyMapping(Session& session, Stats* stats)
{
    if (!stats) return false;
    ClearVisualAnalog();
    std::vector<std::uint8_t> info;
    if (!ReadControlData(session, 0x12, 64, &info))
    {
        DebugLog_Write(L"[titan68.mapping] device_info_unavailable; continuing_without_mapping=1");
        return false;
    }
    const std::size_t slots = info.size() > 4 ? info[4] : 0; // official driver: key_rect_size = byte[4] * 3
    const std::uint16_t reportedMaxRaw = info.size() > 18 ? static_cast<std::uint16_t>(info[17] | info[18] << 8) : 0;
    const std::uint32_t maxRaw = reportedMaxRaw && reportedMaxRaw <= 4095 ? reportedMaxRaw : 4095;
    DebugLog_Write(L"[titan68.mapping] device_info key_slots=%u key_rect_bytes=%u reported_pid=%02X%02X max_raw=%u max_raw_fallback=%d", static_cast<unsigned>(slots), static_cast<unsigned>(slots * 3), info[3], info[2], maxRaw, reportedMaxRaw == 0 || reportedMaxRaw > 4095 ? 1 : 0);
    if (!slots)
    {
        DebugLog_Write(L"[titan68.mapping] empty_key_rect; continuing_without_mapping=1");
        return false;
    }
    std::vector<std::uint8_t> rect;
    if (!ReadControlData(session, 0x16, slots * 3, &rect))
    {
        DebugLog_Write(L"[titan68.mapping] default_key_rect_unavailable; continuing_without_mapping=1");
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
        DebugLog_Write(L"[titan68.mapping.slot] key_index=%u triplet=%02X:%02X:%02X keyboard_usage=%s", static_cast<unsigned>(slot), type, middle, last, entry.keyboardUsage ? Hex(&entry.usage, 1).c_str() : L"none");
    }
    g_maxRaw.store(maxRaw, std::memory_order_release);
    g_mappedKeys.store(static_cast<std::uint32_t>(keyboard), std::memory_order_release);
    g_mapReady.store(keyboard != 0, std::memory_order_release);
    DebugLog_Write(L"[titan68.mapping] complete slots=%u keyboard=%u non_keyboard=%u visual_ready=%d", static_cast<unsigned>(slots), static_cast<unsigned>(keyboard), static_cast<unsigned>(unsupported), keyboard ? 1 : 0);
    return keyboard != 0;
}

void PublishVisualAnalog(std::uint8_t keyIndex, std::uint16_t raw12)
{
    if (!g_mapReady.load(std::memory_order_acquire)) return;
    const auto hid = g_hidAtKeyIndex[keyIndex].load(std::memory_order_relaxed);
    const auto maxRaw = g_maxRaw.load(std::memory_order_acquire);
    if (!hid || !maxRaw) return;
    const auto milli = static_cast<std::uint16_t>(std::min<std::uint32_t>(1000, (static_cast<std::uint32_t>(raw12) * 1000u + maxRaw / 2u) / maxRaw));
    g_milli[hid].store(milli, std::memory_order_release);
}

void ProcessStreamReport(const StreamReport& report, DWORD bytes, Stats* stats)
{
    if (!stats) return; ++stats->reports; g_reports.fetch_add(1, std::memory_order_relaxed); g_lastMs.store(GetTickCount64(), std::memory_order_relaxed);
    DebugLog_WriteBuffered(L"[titan68.rx] bytes=%lu data=%ls", bytes, Hex(report.data(), bytes).c_str());
    if (report[0] == kControlReportId) { ++stats->report6; return; }
    if (report[0] != kStreamReportId) { ++stats->other; return; }
    ++stats->report7;
    if (bytes < 3) { ++stats->malformed; return; }
    const std::uint8_t key = report[1], part = report[2];
    if ((part & 0x40u) == 0) { ++stats->malformed; DebugLog_Write(L"[titan68.stream] malformed key=%u part=%02X", key, part); return; }
    auto& half = stats->halves[key];
    if (!half.pending) { half.pending = true; half.low = static_cast<std::uint8_t>(part & 0x3fu); return; }
    const std::uint16_t travel = static_cast<std::uint16_t>((part & 0x3fu) << 6 | half.low);
    half = {}; ++stats->travelPairs; g_pairs.fetch_add(1, std::memory_order_relaxed);
    auto& range = stats->travel[key]; range.minimum = std::min(range.minimum, travel); range.maximum = std::max(range.maximum, travel); ++range.samples;
    PublishVisualAnalog(key, travel);
    const auto mapping = stats->mapping.find(key);
    if (mapping != stats->mapping.end() && mapping->second.keyboardUsage)
        DebugLog_WriteBuffered(L"[titan68.travel] key_index=%u hid_usage=%02X raw12=%u pair=%llu", key, mapping->second.usage, travel, static_cast<unsigned long long>(stats->travelPairs));
    else
        DebugLog_WriteBuffered(L"[titan68.travel] key_index=%u hid_usage=none raw12=%u pair=%llu", key, travel, static_cast<unsigned long long>(stats->travelPairs));
}

bool Listen(Session& session, Stats* stats, DWORD durationMs, bool obeyStop)
{
    const auto end = GetTickCount64() + durationMs;
    while (GetTickCount64() < end && (!obeyStop || !g_stop.load(std::memory_order_acquire)))
    {
        StreamReport report{}; DWORD bytes = 0; const auto remaining = static_cast<DWORD>(end - GetTickCount64());
        if (session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining), &bytes)) ProcessStreamReport(report, bytes, stats);
        else if (GetLastError() != WAIT_TIMEOUT && GetLastError() != ERROR_OPERATION_ABORTED)
        { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[titan68.rx_error] win32=%lu", GetLastError()); return false; }
    }
    return true;
}

bool SendAndAwait(Session& session, Stats* stats, std::uint8_t command, bool enabled)
{
    const auto request = BuildModeControl(command, enabled);
    if (!session.Send(request)) { ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[titan68.control] send_failed command=%02X enabled=%d win32=%lu", command, enabled ? 1 : 0, GetLastError()); return false; }
    ++stats->writes; const auto end = GetTickCount64() + kAckTimeoutMs;
    while (GetTickCount64() < end)
    {
        ControlReport report{}; DWORD bytes = 0; const auto remaining = static_cast<DWORD>(end - GetTickCount64());
        if (!session.Read(&report, std::min<DWORD>(kReadSliceMs, remaining), &bytes))
        {
            if (GetLastError() == WAIT_TIMEOUT || GetLastError() == ERROR_OPERATION_ABORTED) continue;
            ++stats->failures; g_failures.fetch_add(1, std::memory_order_relaxed); DebugLog_Write(L"[titan68.control] ack_read_failed command=%02X enabled=%d win32=%lu", command, enabled ? 1 : 0, GetLastError()); return false;
        }
        DebugLog_WriteBuffered(L"[titan68.rx_control] bytes=%lu data=%ls", bytes, Hex(report.data(), bytes).c_str());
        if (IsControlAck(report, bytes, command)) { DebugLog_Write(L"[titan68.control] ack command=%02X enabled=%d", command, enabled ? 1 : 0); return true; }
        if (bytes >= 8 && report[0] == kControlReportId && report[1] == command && report[7] == 0x0f) { DebugLog_Write(L"[titan68.control] rejected command=%02X enabled=%d", command, enabled ? 1 : 0); return false; }
    }
    DebugLog_Write(L"[titan68.control] ack_timeout command=%02X enabled=%d", command, enabled ? 1 : 0); return false;
}

void LogSummary(const Stats& stats)
{
    DebugLog_Write(L"[titan68.summary] reports=%llu report06=%llu report07=%llu other=%llu travel_pairs=%llu malformed=%llu writes=%llu failures=%llu", static_cast<unsigned long long>(stats.reports), static_cast<unsigned long long>(stats.report6), static_cast<unsigned long long>(stats.report7), static_cast<unsigned long long>(stats.other), static_cast<unsigned long long>(stats.travelPairs), static_cast<unsigned long long>(stats.malformed), static_cast<unsigned long long>(stats.writes), static_cast<unsigned long long>(stats.failures));
    for (const auto& [key, range] : stats.travel) DebugLog_Write(L"[titan68.summary.travel] key_index=%u samples=%llu min_raw12=%u max_raw12=%u", key, static_cast<unsigned long long>(range.samples), range.minimum, range.maximum);
}

bool IsTargetRawKeyboard(std::uintptr_t rawDevice)
{
    const auto hit = g_rawTargets.find(rawDevice); if (hit != g_rawTargets.end()) return hit->second;
    UINT chars = 0; bool match = false, queryOk = false; const HANDLE device = reinterpret_cast<HANDLE>(rawDevice);
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars) != static_cast<UINT>(-1) && chars)
    {
        std::wstring path(chars + 1, L'\0');
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, path.data(), &chars) != static_cast<UINT>(-1))
        { std::transform(path.begin(), path.end(), path.begin(), towupper); queryOk = true; match = path.find(L"VID_28E9&PID_31FD") != std::wstring::npos; }
    }
    DebugLog_Write(L"[titan68.raw_device] device=%p name_chars=%u query_ok=%d target_match=%d", device, chars, queryOk ? 1 : 0, match ? 1 : 0);
    g_rawTargets.emplace(rawDevice, match); return match;
}

bool RunCandidate(const DevicePair& pair)
{
    Stats stats{};
    DebugLog_Write(L"[titan68.session] begin vid=28E9 pid=31FD version=%04X control=FF87:0020/64 stream=FF88:0021/3 plan=passive_then_readonly_mapping_then_36_01_10_seconds_36_00", pair.control.attributes.VersionNumber);
    { Session passive(pair.stream, false); if (passive.Open()) { DebugLog_Write(L"[titan68.phase] passive duration_ms=%lu", kPassiveListenMs); (void)Listen(passive, &stats, kPassiveListenMs, true); } else DebugLog_Write(L"[titan68.phase] passive_open_failed win32=%lu", GetLastError()); }
    if (g_stop.load(std::memory_order_acquire)) { DebugLog_Write(L"[titan68.session] cancelled_before_control"); LogSummary(stats); return false; }
    Session control(pair.control, true), stream(pair.stream, false); if (!control.Open() || !stream.Open()) { DebugLog_Write(L"[titan68.session] split_open_failed win32=%lu", GetLastError()); LogSummary(stats); return false; }
    g_connected.store(true, std::memory_order_release); g_inputBytes.store(pair.stream.caps.InputReportByteLength); g_outputBytes.store(pair.control.caps.OutputReportByteLength);
    (void)ReadDefaultKeyMapping(control, &stats);
    const bool simulationEnterAck = SendAndAwait(control, &stats, 0x36, true);
    DebugLog_Write(L"[titan68.phase] active begin simulation_enter_ack=%d visual_mapping_ready=%d duration_ms=%lu press=WASD_and_type_before_during_after", simulationEnterAck ? 1 : 0, g_mapReady.load(std::memory_order_acquire) ? 1 : 0, kObservationMs);
    if (simulationEnterAck) (void)Listen(stream, &stats, kObservationMs, true);
    DebugLog_Write(L"[titan68.phase] active complete; sending mandatory 36_00");
    const bool simulationExitAck = SendAndAwait(control, &stats, 0x36, false);
    DebugLog_Write(L"[titan68.phase] simulation_exit_ack=%d; post_exit_passive", simulationExitAck ? 1 : 0);
    (void)Listen(stream, &stats, kPassiveListenMs, false); LogSummary(stats); g_connected.store(false, std::memory_order_release); ClearVisualAnalog();
    return simulationEnterAck && simulationExitAck;
}

unsigned __stdcall Worker(void*)
{
    while (!g_stop.load(std::memory_order_acquire))
    {
        bool attempted = false;
    for (const auto& candidate : Enumerate(false))
        {
            if (NativeAnalogRouting_IsClaimed(candidate.control.path.c_str()) && !NativeAnalogRouting_IsClaimedBy(candidate.control.path.c_str(), NativeAnalogProtocol::Titan68TurboDiagnostic)) continue;
            NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.control.path.c_str(), NativeAnalogProtocol::Titan68TurboDiagnostic);
            g_present.store(true, std::memory_order_release); attempted = true; (void)RunCandidate(candidate);
            if (g_stop.load(std::memory_order_acquire)) break;
            // A failed acknowledgement is evidence to log, not a reason to repeat
            // a control-mode probe automatically against the user's keyboard.
            break;
        }
        if (attempted) break;
        if (!g_stop.load(std::memory_order_acquire)) { g_present.store(false, std::memory_order_release); if (g_wake) { WaitForSingleObject(g_wake, 1000); ResetEvent(g_wake); } }
    }
    g_connected.store(false, std::memory_order_release); g_running.store(false, std::memory_order_release); return 0;
}

bool Prepare()
{
    bool any = false;
    for (const auto& candidate : Enumerate(true))
    {
        const bool claimed = NativeAnalogRouting_Claim(kVendorId, kProductId, candidate.control.path.c_str(), NativeAnalogProtocol::Titan68TurboDiagnostic);
        DebugLog_Write(L"[titan68.prepare] control_hash=%016llX stream_hash=%016llX exact_split_identity=1 claimed=%d", static_cast<unsigned long long>(HashPath(candidate.control.path)), static_cast<unsigned long long>(HashPath(candidate.stream.path)), claimed ? 1 : 0);
        any = claimed || NativeAnalogRouting_IsClaimedBy(candidate.control.path.c_str(), NativeAnalogProtocol::Titan68TurboDiagnostic) || any;
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
    DebugLog_Write(L"[titan68.start] worker=%u visual_analog_only=1 gamepad_output=0", id); return true;
}

halljoy::lifecycle::StopResult Stop(halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> lock(g_serviceMutex); if (!g_thread) return NativeAnalogBackendStopJoined(generation);
    g_stop.store(true, std::memory_order_release); if (g_wake) SetEvent(g_wake);
    { std::lock_guard<std::mutex> active(g_handleMutex); if (g_activeHandle != INVALID_HANDLE_VALUE) CancelIoEx(g_activeHandle, nullptr); }
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
    _snwprintf_s(out->status, _countof(out->status), _TRUNCATE, L"Titan68 Turbo visual diagnostic: %u mapped; report07 pairs=%llu; gamepad output disabled", g_mappedKeys.load(), static_cast<unsigned long long>(g_pairs.load()));
}
}

#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
void Titan68TurboDiagnostic_RecordRawKeyboardEvent(std::uintptr_t rawDevice, std::uint16_t hidUsage, std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey)
{
    std::lock_guard<std::mutex> lock(g_rawMutex); if (!IsTargetRawKeyboard(rawDevice)) return;
    DebugLog_WriteBuffered(L"[titan68.raw_key] device=%p hid=%02X down=%d make=%02X flags=%04X vkey=%02X", reinterpret_cast<void*>(rawDevice), hidUsage, (flags & RI_KEY_BREAK) == 0 ? 1 : 0, makeCode, flags, virtualKey);
}
void Titan68TurboDiagnostic_NotifyRawInputReady(bool registered)
{
    g_rawInputReady.store(registered, std::memory_order_release); DebugLog_Write(L"[titan68.raw_input] registered=%d", registered ? 1 : 0); if (g_wake) SetEvent(g_wake);
}
#endif

const NativeAnalogBackendDescriptor& Titan68TurboDiagnostic_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{kNativeAnalogBackendAbiVersion, sizeof(NativeAnalogBackendDescriptor), "titan68-turbo-diagnostic", L"Madlions Titan68 Turbo visual analogue diagnostic", NativeAnalogProtocol::Titan68TurboDiagnostic, NativeAnalogStartPhase::BeforeUap, NativeAnalogBackendFlag_StreamTransport | NativeAnalogBackendFlag_ReversibleControlProbe, &Prepare, &Start, &Stop, &Notify, &Present, &Connected, &Owns, &Get, &Telemetry};
    return descriptor;
}
