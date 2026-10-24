// DrunkDeer keyboards on the NuPhy USB VID (19F5): analog stream enable and evidence.
//
// Tester reports 2026-10-20 (A75 Ultra 19F5:FB3E, firmware v121): travel works in some
// onboard profiles only, and the working set changed between runs and after opening the
// official web driver. Cause (driver drunkdeer.keybord.net.cn): the A0 travel stream of a
// profile runs only while bit 3 of byte 7 of its settings is set; the driver sets it with
// setDebugModeKey(profile, 1) when its "Travel" tab opens and clears it on any other tab.
//
// This module:
// - enables the stream like the driver: at start for the four profiles and 1 s after each A1
//   profile report for the new profile it reads the settings (cmd 5) and, only when bit 3 is
//   clear, writes them back with that bit set (cmd 6, the driver's setFunc), then verifies.
//   Owner decision 2026-10-20: write it, the stream cannot be enabled otherwise.
// - reads every collection of the board (shared, read-only handles) and records A0 records,
//   A1 profile reports and reply heads.
//
// Privacy: an A0 record of an ordinary key (type 16, no modifier byte) is written only when
// that key is bound to the gamepad; other ordinary keys are counted, never written.
// Records are written on change (new key code, travel step >= 40 units or to/from zero).
#include "drunkdeer_nuphy_probe.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "input_trace.h"
#include "support_log.h"
#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_drunkdeer_identity.h"

namespace {

namespace dd = halljoy::drunkdeer_identity;

std::atomic<bool> g_stop{false};
std::thread g_worker;
std::mutex g_handlesLock;
std::vector<HANDLE> g_handles;

// Profile-load requests: due time (0 = none) and profile (-1 = all four at start).
std::atomic<ULONGLONG> g_loadAt{0};
std::atomic<int> g_loadProfile{-1};
std::atomic<int> g_readers{0};
std::atomic<unsigned> g_readerReports{0};  // all reports seen by the passive readers (liveness)  // the loader stops when every reader of the board has closed

void Evidence(bool keep, const char* format, ...) noexcept {
    char line[400];
    const int at = _snprintf_s(line, sizeof(line), _TRUNCATE, "evidence.dd19f5 ");
    if (at < 0) return;
    va_list args;
    va_start(args, format);
    _vsnprintf_s(line + at, sizeof(line) - static_cast<std::size_t>(at), _TRUNCATE, format, args);
    va_end(args);
    SupportLog_Evidence(line, keep);
}

std::string Hex(const std::uint8_t* data, std::size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < size; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 15];
    }
    return out;
}

// Official driver K(type, b2, b3): HID usage of an A0 record.
unsigned DriverKey(unsigned type, unsigned b2, unsigned b3) noexcept {
    if (type == 16) {
        if (!b2) return b3;
        switch (b2) {
        case 1: return 224; case 2: return 225; case 4: return 226; case 8: return 227;
        case 16: return 228; case 32: return 229; case 64: return 230; case 128: return 231;
        default: return b3;
        }
    }
    if (type == 240 && b2 == 255 && b3 == 1) return 255;
    if (type == 240 && b2 == 255 && b3 == 2) return 254;
    if (type == 224 && b2 == 0) return 254;
    return 0;
}

struct Candidate {
    std::wstring path;
    USHORT pid = 0, inputBytes = 0, usagePage = 0, usage = 0, outputBytes = 0;
    std::string where;  // "mi_02&col03" part of the interface path
};

std::vector<Candidate> Find() {
    std::vector<Candidate> found;
    GUID hid{};
    HidD_GetHidGuid(&hid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return found;
    SP_DEVICE_INTERFACE_DATA item{};
    item.cbSize = sizeof(item);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &hid, i, &item); ++i) {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &item, nullptr, 0, &size, nullptr);
        if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<std::uint8_t> storage(size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &item, detail, size, nullptr, nullptr)) continue;
        // Cheap path filter first: only NuPhy-VID interfaces are opened at all.
        std::wstring lower(detail->DevicePath);
        for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
        if (lower.find(L"vid_19f5") == std::wstring::npos) continue;
        HANDLE probe = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, 0, nullptr);
        if (probe == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(probe, &attributes) &&
            dd::IsNuphyPlatformBoard(attributes.VendorID, attributes.ProductID) &&
            HidD_GetPreparsedData(probe, &preparsed) && HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS;
        if (preparsed) HidD_FreePreparsedData(preparsed);
        CloseHandle(probe);
        if (!ok) continue;
        Candidate c{detail->DevicePath, attributes.ProductID, caps.InputReportByteLength, caps.UsagePage, caps.Usage,
                    caps.OutputReportByteLength, {}};
        const auto mi = lower.find(L"mi_");
        for (std::size_t k = mi; k != std::wstring::npos && k < lower.size() && c.where.size() < 16; ++k) {
            const wchar_t ch = lower[k];
            if (ch == L'#' || ch == L'{' || ch == 0x5C) break;  // 0x5C = backslash
            c.where += static_cast<char>(ch < 128 ? ch : '?');
        }
        found.push_back(std::move(c));
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

// Overlapped read of one report with a timeout. Returns the byte count, 0 on timeout,
// -1 on failure. Strips the report-ID 0 Windows prepends for collections without IDs.
int ReadReport(HANDLE handle, HANDLE event, std::vector<std::uint8_t>& buffer, DWORD timeoutMs,
               const std::uint8_t*& data) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = event;
    ResetEvent(event);
    DWORD got = 0;
    if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, &overlapped) &&
        GetLastError() != ERROR_IO_PENDING)
        return -1;
    if (WaitForSingleObject(event, timeoutMs) == WAIT_TIMEOUT) {
        CancelIoEx(handle, &overlapped);
        GetOverlappedResult(handle, &overlapped, &got, TRUE);
        return 0;
    }
    if (!GetOverlappedResult(handle, &overlapped, &got, FALSE)) return -1;
    data = buffer.data();
    if (got && data[0] == 0) { ++data; --got; }
    return static_cast<int>(got);
}

// ---- Profile load, as the official driver's e5 -------------------------------------------

struct Loader {
    HANDLE handle = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    std::vector<std::uint8_t> buffer;
    unsigned requests = 0, replies = 0, missing = 0, travelSkipped = 0;

    bool Write(std::uint8_t cmd, unsigned offset, unsigned length) {
        std::array<std::uint8_t, 65> report{};
        const unsigned lo = offset & 255, hi = (offset >> 8) & 255;
        report[1] = 0x55; report[2] = cmd; report[3] = 0;
        report[4] = static_cast<std::uint8_t>((lo + hi + length) & 255);
        report[5] = static_cast<std::uint8_t>(length);
        report[6] = static_cast<std::uint8_t>(lo); report[7] = static_cast<std::uint8_t>(hi);
        return WriteReport(report);
    }

    bool WriteReport(const std::array<std::uint8_t, 65>& report) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = event;
        ResetEvent(event);
        DWORD written = 0;
        if (!WriteFile(handle, report.data(), static_cast<DWORD>(report.size()), nullptr, &overlapped)) {
            if (GetLastError() != ERROR_IO_PENDING) return false;
            if (WaitForSingleObject(event, 300) == WAIT_TIMEOUT) CancelIoEx(handle, &overlapped);
        }
        return GetOverlappedResult(handle, &overlapped, &written, TRUE) && written == report.size();
    }

    // The driver takes the next report as the reply; travel records may interleave here.
    bool Reply(std::uint8_t cmd, std::vector<std::uint8_t>& out, unsigned length) {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const std::uint8_t* r = nullptr;
            const int got = ReadReport(handle, event, buffer, 150, r);
            if (got <= 0) return false;
            const bool travel = (r[0] == 0xA0 && got > 1 && dd::IsNuphyPlatformKeyType(r[1])) || r[0] == 0xA1;
            if (travel) { ++travelSkipped; continue; }
            if (got < 8) continue;
            out.insert(out.end(), r + 8, r + 8 + (std::min<unsigned>)(length, static_cast<unsigned>(got) - 8));
            (void)cmd;
            return true;
        }
        return false;
    }

    std::vector<std::uint8_t> Range(std::uint8_t cmd, unsigned start, unsigned total) {
        std::vector<std::uint8_t> data;
        for (unsigned offset = start; offset < start + total && !g_stop.load(); offset += 56) {
            const unsigned length = (std::min)(56u, start + total - offset);
            ++requests;
            if (!Write(cmd, offset, length) || !Reply(cmd, data, length)) {
                ++missing;
                data.resize(data.size() + length, 0xEE);  // marks a missing chunk
            } else {
                ++replies;
            }
        }
        return data;
    }
};

void LogBytes(const char* what, unsigned profile, const std::vector<std::uint8_t>& data) {
    for (std::size_t at = 0; at < data.size(); at += 96) {
        const std::size_t n = (std::min<std::size_t>)(96, data.size() - at);
        Evidence(true, "%s p=%u off=%zu hex=%s", what, profile, at, Hex(data.data() + at, n).c_str());
    }
}

// Official driver sendData(cmd, address, bytes): 56-byte chunks, each
// 55 cmd 00 sum len lo hi last data..., sum = len + lo + hi + last + data (mod 256).
bool WriteData(Loader& loader, std::uint8_t cmd, unsigned address, const std::vector<std::uint8_t>& bytes) {
    for (std::size_t at = 0; at < bytes.size(); at += 56) {
        const unsigned length = static_cast<unsigned>((std::min<std::size_t>)(56, bytes.size() - at));
        const unsigned offset = address + static_cast<unsigned>(at);
        const unsigned last = at + length == bytes.size() ? 1u : 0u;
        std::array<std::uint8_t, 65> report{};
        report[1] = 0x55; report[2] = cmd;
        report[5] = static_cast<std::uint8_t>(length);
        report[6] = static_cast<std::uint8_t>(offset & 255);
        report[7] = static_cast<std::uint8_t>((offset >> 8) & 255);
        report[8] = static_cast<std::uint8_t>(last);
        unsigned sum = length + (offset & 255) + ((offset >> 8) & 255) + last;
        for (unsigned i = 0; i < length; ++i) {
            report[9 + i] = bytes[at + i];
            sum += bytes[at + i];
        }
        report[4] = static_cast<std::uint8_t>(sum & 255);
        if (!loader.WriteReport(report)) return false;
        std::vector<std::uint8_t> ack;
        if (!loader.Reply(cmd, ack, 0)) return false;  // the driver waits for the reply of every chunk
    }
    Sleep(50);  // driver setFunc: 50 ms after the write
    return true;
}

// The analog travel stream (A0) of a profile runs only while bit 3 of byte 7 of the profile
// settings is set. The official driver sets it with setDebugModeKey(profile, 1) when its
// "Travel" tab opens and clears it when another tab opens, so profiles work or not depending
// on where the driver was last closed. HallJoy sets it the same way (owner decision
// 2026-10-20: write it, the stream cannot be enabled otherwise). Only that bit changes; the
// other 63 bytes are written back exactly as read. Written only when the bit is clear.
void EnsureStream(Loader& loader, unsigned profile, const char* reason) {
    loader.requests = loader.replies = loader.missing = loader.travelSkipped = 0;
    const auto func = loader.Range(5, 64 * profile, 64);
    const bool readable = func.size() == 64 && loader.missing == 0 && func[2] == 0xAA && func[3] == 0xBB;
    if (!readable) {
        Evidence(true, "stream p=%u reason=%s action=skip_unreadable missing=%u head=%s", profile, reason, loader.missing,
                 Hex(func.data(), (std::min<std::size_t>)(func.size(), 8)).c_str());
        return;
    }
    if (func[7] & 0x08) {
        Evidence(true, "stream p=%u reason=%s action=already_on byte7=%02x", profile, reason, func[7]);
        return;
    }
    auto updated = func;
    updated[7] = static_cast<std::uint8_t>(func[7] | 0x08);
    const bool written = WriteData(loader, 6, 64 * profile, updated);
    loader.requests = loader.replies = loader.missing = 0;
    const auto check = loader.Range(5, 64 * profile, 64);
    const bool verified = check.size() == 64 && loader.missing == 0 && check == updated;
    Evidence(true, "stream p=%u reason=%s action=set byte7=%02x->%02x written=%d verified=%d after7=%02x", profile, reason,
             func[7], updated[7], written ? 1 : 0, verified ? 1 : 0, check.size() == 64 ? check[7] : 0xEE);
    if (!verified) {
        LogBytes("stream_mismatch_before", profile, func);
        LogBytes("stream_mismatch_after", profile, check);
    }
}

void LoadThread(Candidate device) {
    Loader loader;
    loader.handle = CreateFileW(device.path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (loader.handle == INVALID_HANDLE_VALUE) {
        Evidence(true, "load_open_failed at=%s error=%lu", device.where.c_str(), GetLastError());
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_handlesLock);
        g_handles.push_back(loader.handle);
    }
    loader.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    loader.buffer.resize(device.inputBytes ? device.inputBytes : 65);
    while (!g_stop.load() && g_readers.load() > 0) {
        ULONGLONG at = g_loadAt.load();
        if (!at || GetTickCount64() < at || !g_loadAt.compare_exchange_strong(at, 0)) {
            Sleep(50);
            continue;
        }
        const int profile = g_loadProfile.load();
        if (profile < 0) {
            for (unsigned p = 0; p < 4 && !g_stop.load(); ++p) EnsureStream(loader, p, "start");
        } else {
            EnsureStream(loader, static_cast<unsigned>(profile), "profile");
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_handlesLock);
        for (auto it = g_handles.begin(); it != g_handles.end(); ++it)
            if (*it == loader.handle) { g_handles.erase(it); break; }
    }
    CloseHandle(loader.event);
    CloseHandle(loader.handle);
}

// ---- Passive readers ---------------------------------------------------------------------

struct Stats {
    unsigned reports = 0, a0 = 0, written = 0, ordinaryUnbound = 0, replies = 0, other = 0, shortA0 = 0;
};

void Read(const Candidate& device) {
    struct Done { ~Done() { --g_readers; } } done;
    HANDLE handle = CreateFileW(device.path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        Evidence(true, "open_failed pid=%04x at=%s usage=%04x:%04x error=%lu", device.pid, device.where.c_str(),
                 device.usagePage, device.usage, GetLastError());
        return;
    }
    Evidence(true, "open pid=%04x at=%s usage=%04x:%04x input_bytes=%u output_bytes=%u", device.pid,
             device.where.c_str(), device.usagePage, device.usage, device.inputBytes, device.outputBytes);
    {
        std::lock_guard<std::mutex> lock(g_handlesLock);
        g_handles.push_back(handle);
    }
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::vector<std::uint8_t> buffer(device.inputBytes ? device.inputBytes : 65);
    Stats stats;
    std::array<int, 65536> lastTravel;
    lastTravel.fill(-1);
    unsigned lastCode = 0x10000;
    std::array<bool, 256> otherSeen{};
    while (!g_stop.load()) {
        const std::uint8_t* r = nullptr;
        const int got = ReadReport(handle, event, buffer, 250, r);
        if (got < 0) {
            Evidence(true, "read_failed at=%s error=%lu reports=%u", device.where.c_str(), GetLastError(), stats.reports);
            break;
        }
        if (got == 0) continue;
        const std::size_t size = static_cast<std::size_t>(got);
        ++stats.reports;
        ++g_readerReports;
        if (stats.reports == 1)
            Evidence(true, "first_report at=%s len=%zu head=%s", device.where.c_str(), size, Hex(r, size < 4 ? size : 4).c_str());
        if (stats.reports % 2000 == 0)
            Evidence(false, "summary at=%s reports=%u a0=%u written=%u ordinary_unbound=%u replies=%u other=%u short=%u",
                     device.where.c_str(), stats.reports, stats.a0, stats.written, stats.ordinaryUnbound,
                     stats.replies, stats.other, stats.shortA0);
        if (r[0] == 0xA1) {
            // Profile report (driver listener "profile", index in byte 2). Whole report: settings only.
            Evidence(true, "profile at=%s index=%u reports=%u a0=%u hex=%s", device.where.c_str(),
                     size > 2 ? static_cast<unsigned>(r[2]) : 255u, stats.reports, stats.a0, Hex(r, size).c_str());
            if (size > 2 && r[2] < 4) {
                g_loadProfile.store(r[2]);
                g_loadAt.store(GetTickCount64() + 1000);  // the driver reloads the profile after 1 s
            }
            continue;
        }
        if (r[0] == 0xAA || (r[0] == 0xA0 && size > 1 && !dd::IsNuphyPlatformKeyType(r[1])) || r[0] == 0xA2) {
            ++stats.replies;  // replies to the profile load (logged there)
            continue;
        }
        if (r[0] != 0xA0) {
            ++stats.other;
            if (!otherSeen[r[0]]) {
                otherSeen[r[0]] = true;
                Evidence(true, "other at=%s first=%02x len=%zu head=%s", device.where.c_str(), r[0], size,
                         Hex(r, size < 4 ? size : 4).c_str());
            }
            continue;
        }
        ++stats.a0;
        if (size < 11) {
            ++stats.shortA0;
            Evidence(false, "a0_short len=%zu hex=%s", size, Hex(r, size).c_str());
            continue;
        }
        const unsigned type = r[1], b2 = r[2], b3 = r[3];
        const unsigned soup = (b2 << 8) | b3;
        const unsigned f45 = (unsigned(r[4]) << 8) | r[5];
        const unsigned f67 = (unsigned(r[6]) << 8) | r[7];
        if (type == 16 && b2 == 0 && !InputTrace_IsBound(static_cast<std::uint16_t>(b3))) {
            ++stats.ordinaryUnbound;
            continue;
        }
        const unsigned key = (type << 16) | soup;
        const int previous = lastTravel[soup];
        const bool changed = key != lastCode || previous < 0 || (previous == 0) != (f67 == 0) ||
            (f67 > unsigned(previous) ? f67 - unsigned(previous) : unsigned(previous) - f67) >= 40;
        if (!changed) continue;
        lastTravel[soup] = static_cast<int>(f67);
        lastCode = key;
        ++stats.written;
        Evidence(false, "a0 at=%s n=%u type=%u soup=%04x driver_key=%u f45=%u f67=%u st=%u len=%zu hex=%s",
                 device.where.c_str(), stats.a0, type, soup, DriverKey(type, b2, b3), f45, f67, r[10], size,
                 Hex(r, size < 16 ? size : 16).c_str());
    }
    Evidence(true, "closed at=%s reports=%u a0=%u written=%u ordinary_unbound=%u replies=%u other=%u short=%u",
             device.where.c_str(), stats.reports, stats.a0, stats.written, stats.ordinaryUnbound, stats.replies,
             stats.other, stats.shortA0);
    {
        std::lock_guard<std::mutex> lock(g_handlesLock);
        for (auto it = g_handles.begin(); it != g_handles.end(); ++it)
            if (*it == handle) { g_handles.erase(it); break; }
    }
    CloseHandle(event);
    CloseHandle(handle);
}

void Run() {
    while (!g_stop.load()) {
        const auto devices = Find();
        if (devices.empty()) {
            for (int i = 0; i < 8 && !g_stop.load(); ++i) Sleep(250);
            continue;
        }
        std::vector<std::thread> threads;
        g_readers.store(static_cast<int>(devices.size()));
        for (const auto& device : devices) {
            try { threads.emplace_back(Read, device); } catch (...) { --g_readers; }
        }
        for (const auto& device : devices) {
            if (device.usagePage == 1 && device.usage == 0 && device.outputBytes >= 64) {
                g_loadProfile.store(-1);
                g_loadAt.store(GetTickCount64() + 1000);  // all four profiles once at start
                try { threads.emplace_back(LoadThread, device); } catch (...) {}
                break;
            }
        }
        for (auto& thread : threads) thread.join();
        for (int i = 0; i < 4 && !g_stop.load(); ++i) Sleep(250);
    }
}

}  // namespace

void DrunkDeerNuphyProbe_Start() noexcept {
    if (g_worker.joinable()) return;
    g_stop.store(false);
    try {
        g_worker = std::thread(Run);
    } catch (...) {
    }
}

void DrunkDeerNuphyProbe_Stop() noexcept {
    g_stop.store(true);
    {
        std::lock_guard<std::mutex> lock(g_handlesLock);
        for (HANDLE handle : g_handles) CancelIoEx(handle, nullptr);
    }
    if (g_worker.joinable()) g_worker.join();
}
