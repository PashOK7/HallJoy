#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "support_log.h"
#include "settings.h"
#include "backend.h"
#include "keyboard_support_status.h"
#include "engine_runtime_owner.h"
#include "version.h"
#include "app_paths.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <shlobj.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

namespace {
constexpr size_t kLineBytes = 1024, kQueueLines = 512, kHistoryLines = 512;
constexpr DWORD kFileLimit = 4 * 1024 * 1024;
using Line = std::array<char, kLineBytes>;
SRWLOCK queueLock = SRWLOCK_INIT;
std::array<Line, kQueueLines> queue{};
size_t queued = 0;
std::atomic<unsigned> dropped{0};
std::atomic<bool> stopping{false}, inventoryDirty{true};
std::atomic<bool> incidentPending{false};
std::atomic<bool> failurePending{false};
std::atomic<HWND> uiWindow{nullptr};
std::atomic<DWORD> lastError{0};
HANDLE worker = nullptr, stopEvent = nullptr;
std::wstring testDirectory; // Optional isolated destination for real writer tests.

void Error(DWORD error) noexcept {
    if (lastError.exchange(error) != error)
        if (auto hwnd = uiWindow.load()) PostMessageW(hwnd, WM_APP + 362, 0, 0);
}
void Enqueue(const char* text) noexcept {
    if (!TryAcquireSRWLockExclusive(&queueLock)) { ++dropped; return; }
    if (queued < queue.size()) {
        _snprintf_s(queue[queued++].data(), kLineBytes, _TRUNCATE, "uptime_ms=%llu %s",
            GetTickCount64(), text);
    } else ++dropped;
    ReleaseSRWLockExclusive(&queueLock);
}
void Inventory() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO devices = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) { SupportLog_Event("inventory.enumeration_failed", 0, GetLastError()); return; }
    // Read Windows metadata only. No device opens, feature reports or probes.
    for (DWORD index = 0; index < 256 && !stopping.load(); ++index) {
        SP_DEVINFO_DATA info{sizeof(info)};
        if (!SetupDiEnumDeviceInfo(devices, index, &info)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) SupportLog_Event("inventory.enum_error", index, GetLastError());
            break;
        }
        wchar_t ids[2048]{}; DWORD type = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(devices, &info, SPDRP_HARDWAREID, &type,
            reinterpret_cast<BYTE*>(ids), sizeof(ids) - sizeof(wchar_t), nullptr)) {
            SupportLog_Event("inventory.metadata_error", index, GetLastError()); continue;
        }
        // Deliberately omit full instance paths, serials and user-editable names.
        _wcsupr_s(ids);
        const wchar_t* vid = wcsstr(ids, L"VID_");
        const wchar_t* pid = wcsstr(ids, L"PID_");
        const wchar_t* mi = wcsstr(ids, L"MI_");
        unsigned v = vid ? wcstoul(vid + 4, nullptr, 16) & 0xffff : 0;
        unsigned p = pid ? wcstoul(pid + 4, nullptr, 16) & 0xffff : 0;
        unsigned m = mi ? wcstoul(mi + 3, nullptr, 16) & 0xff : 0xff;
        char line[160]{};
        sprintf_s(line, "hid.candidate vid=%04X pid=%04X interface=%02X metadata_only=1", v, p, m);
        Enqueue(line);
    }
    SetupDiDestroyDeviceInfoList(devices);
    Enqueue("inventory.complete limit=256; HID candidates include mice and other devices, not proof of analogue capability");
}
void Snapshot(bool detail) {
    BackendAnalogTelemetry t{}; Backend_GetAnalogTelemetry(&t);
    char line[kLineBytes]{};
    const auto engine = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot();
    const auto support = halljoy::keyboard_support::GetStatusSnapshot();
    sprintf_s(line, "snapshot engine=%u engine_error=%u search_complete=%d analogue_connected=%d sdk_initialised=%d devices=%d analog_error=%d full_buffer_result=%d",
        unsigned(engine.state), engine.lastNativeError, support.searchCompleted, support.analogSourceConnected,
        t.sdkInitialised, t.deviceCount, t.lastAnalogError, t.fullBufferRet);
    Enqueue(line);
    sprintf_s(line, "uap available=%d ready=%d status=%d error=%d transport_error=%d restarts=%d invalid_snapshots=%d",
        t.pluginHostAvailable,t.pluginHostReady,t.pluginHostStatus,t.pluginHostLastError,
        t.pluginHostTransportError,t.pluginHostRestartCount,t.pluginHostInvalidSnapshots);
    Enqueue(line);
    if (!detail) return;
    for (int i=0; i<t.nativeProtocolCount && i<kBackendMaxNativeProtocols; ++i) {
        const auto& n = t.nativeProtocols[i];
        sprintf_s(line, "native protocol=%u vid=%04X pid=%04X usage_page=%04X usage=%04X present=%d connected=%d flags=%u mapped_keys=%u input_bytes=%u output_bytes=%u updates=%llu failures=%llu age_ms=%u",
            n.protocol, n.vendorId, n.productId, n.usagePage, n.usage, n.present, n.connected, n.flags,
            n.mappedKeys, n.inputReportBytes, n.outputReportBytes, n.successfulUpdates, n.failedUpdates, n.lastUpdateAgeMs);
        Enqueue(line);
    }
}
bool WriteLine(HANDLE file, const Line& line) {
    const DWORD length = static_cast<DWORD>(strlen(line.data())); DWORD written = 0;
    if (!WriteFile(file, line.data(), length, &written, nullptr) || written != length) return false;
    return WriteFile(file, "\r\n", 2, &written, nullptr) && written == 2;
}
DWORD WINAPI Run(void*) noexcept {
    try {
        std::deque<Line> history;
        std::vector<Line> batch(kQueueLines);
        bool previousIncident = false, previousContinuous = false, opened = false;
        bool retryPending = false;
        ULONGLONG retryAfter = 0;
        DWORD fileBytes = 0;
        auto directory = testDirectory.empty() ? SupportLog_Directory() : testDirectory;
        const auto path = directory + L"\\HallJoy.log";
        unsigned ticks = 0;
        Enqueue("session.begin version=" HALLJOY_VERSION_STRING_FULL " schema=1 privacy=no_keys_no_input_values_no_serials_no_paths; support absence is not proof of an unsupported keyboard");
        for (;;) {
            const bool continuous = Settings_GetDiagnosticLogging();
            const auto s = halljoy::keyboard_support::GetStatusSnapshot();
            const bool incident = s.searchCompleted && !s.analogSourceConnected;
            const bool requested = incidentPending.exchange(false);
            const bool failure = failurePending.exchange(false);
            const bool trigger = failure || requested || (incident && !previousIncident) || (continuous && !previousContinuous);
            if (requested) Enqueue("support.banner_shown incident_latched=1");
            if (incident != previousIncident) SupportLog_Event("support.banner", incident);
            if (continuous != previousContinuous) SupportLog_Event("logging.continuous", continuous);
            const bool changed = inventoryDirty.exchange(false);
            if (trigger || changed) { Inventory(); Snapshot(true); }
            else if (ticks % 30 == 0) Snapshot(continuous || incident);
            const bool shouldWrite = failure || requested || continuous || incident || (previousIncident && !incident) || (previousContinuous && !continuous);
            previousIncident = incident; previousContinuous = continuous;
            ++ticks;
            size_t count;
            AcquireSRWLockExclusive(&queueLock);
            count = queued;
            for (size_t i=0;i<count;++i) batch[i]=queue[i];
            queued=0;
            ReleaseSRWLockExclusive(&queueLock);
            const unsigned lost = dropped.exchange(0);
            if (lost) SupportLog_Event("logging.queue_dropped", lost);
            for(size_t i=0;i<count;++i) { history.push_back(batch[i]); if(history.size()>kHistoryLines) history.pop_front(); }
            if (((shouldWrite && (count || trigger)) || retryPending) && GetTickCount64() >= retryAfter) {
                retryPending=true;
                retryAfter=GetTickCount64()+5000; // Retain failed automatic reports; no tight retry loop.
                if (directory.empty()) { Error(ERROR_PATH_NOT_FOUND); }
                else {
                    CreateDirectoryW(directory.c_str(), nullptr);
                    const bool reset = !opened || trigger || fileBytes + count*kLineBytes > kFileLimit;
                    const auto destination = reset ? path + L".tmp" : path;
                    HANDLE file = CreateFileW(destination.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                        reset ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (file == INVALID_HANDLE_VALUE) { Error(GetLastError()); opened=false; }
                    else {
                        bool ok = true;
                        if (reset) {
                            fileBytes=0;
                            Line header{};
                            SYSTEMTIME utc{}; GetSystemTime(&utc);
                            sprintf_s(header.data(),header.size(),"HallJoy " HALLJOY_VERSION_STRING_FULL " support report schema=1 utc=%04u-%02u-%02uT%02u:%02u:%02uZ bounded_history=512 no_keyboard_text=1",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond);
                            ok=WriteLine(file,header); fileBytes=DWORD(strlen(header.data())+2);
                            for (const auto& line : history) { if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                        } else {
                            SetFilePointer(file, 0, nullptr, FILE_END);
                            for(size_t i=0;i<count;++i) { if(!(ok=WriteLine(file,batch[i]))) break; fileBytes += DWORD(strlen(batch[i].data())+2); }
                        }
                        DWORD error = ok ? 0 : GetLastError();
                        CloseHandle(file);
                        if(ok && reset && !MoveFileExW(destination.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                            ok=false; error=GetLastError();
                        }
                        if(reset && !ok) DeleteFileW(destination.c_str()); // Only our exact transient file.
                        opened=ok;
                        if(ok) { retryPending=false; retryAfter=0; }
                        Error(ok ? 0 : (error ? error : ERROR_WRITE_FAULT));
                    }
                }
            }
            if (stopping.load()) break;
            if (WaitForSingleObject(stopEvent, 1000) == WAIT_OBJECT_0) stopping=true;
        }
    } catch (...) { Error(ERROR_UNHANDLED_EXCEPTION); return 1; }
    return 0;
}
}
std::wstring SupportLog_Directory() {
    return AppPaths_DataRoot(); // Respects portable mode and isolated test roots.
}
bool SupportLog_Start(const wchar_t* directoryOverride) noexcept {
    if(worker) return true;
    try { testDirectory=directoryOverride ? directoryOverride : L""; }
    catch(...) { Error(ERROR_NOT_ENOUGH_MEMORY); return false; }
    inventoryDirty=true;
    stopping=false;
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stopEvent) { Error(GetLastError()); return false; }
    worker=CreateThread(nullptr,0,Run,nullptr,0,nullptr);
    if(!worker) { Error(GetLastError()); CloseHandle(stopEvent); stopEvent=nullptr; return false; }
    return true;
}
bool SupportLog_Stop() noexcept {
    uiWindow=nullptr;
    if(!worker) return true;
    SupportLog_Event("session.end",0);
    stopping=true; SetEvent(stopEvent);
    if(WaitForSingleObject(worker,5000)!=WAIT_OBJECT_0) return false;
    CloseHandle(worker); CloseHandle(stopEvent); worker=nullptr; stopEvent=nullptr; return true;
}
void SupportLog_Event(const char* category, std::uint64_t value, std::uint64_t error) noexcept {
    char line[256]{}; _snprintf_s(line,sizeof(line),_TRUNCATE,"%s value=%llu error=%llu",category,value,error); Enqueue(line);
}
void SupportLog_OverlaySummary(const wchar_t* aggregate) noexcept {
    if(!Settings_GetDiagnosticLogging()) return;
    char line[kLineBytes]{};
    if(WideCharToMultiByte(CP_UTF8,0,aggregate,-1,line,sizeof(line),nullptr,nullptr)) Enqueue(line);
}
void SupportLog_InventoryChanged() noexcept { inventoryDirty=true; }
void SupportLog_ReportMissingSource() noexcept { incidentPending=true; }
void SupportLog_ReportFailure(const char* category, std::uint64_t error) noexcept {
    SupportLog_Event(category,0,error); failurePending=true;
}
void SupportLog_SetWindow(HWND window) noexcept { uiWindow=window; }
DWORD SupportLog_LastError() noexcept { return lastError.load(); }
