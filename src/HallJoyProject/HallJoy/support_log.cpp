#include "input_path_diagnostics.h"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "support_log.h"
#include "debug_log.h"
#include "settings.h"
#include "backend.h"
#include "keyboard_support_status.h"
#include "engine_runtime_owner.h"
#include "version.h"
#include "app_paths.h"
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <shlobj.h>
#pragma comment(lib, "cfgmgr32.lib")
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

namespace {
constexpr size_t kLineBytes = 1024, kQueueLines = 512, kHistoryLines = 512, kTraceLines = 4000;
constexpr DWORD kFileLimit = 4 * 1024 * 1024;
constexpr size_t kCaptureLineBytes = 192, kCaptureLines = 8000;
using Line = std::array<char, kLineBytes>;
using CaptureLine = std::array<char, kCaptureLineBytes>;
SRWLOCK captureLock = SRWLOCK_INIT;
std::vector<CaptureLine> captureStore; // append-only, first kCaptureLines records
// Device evidence (SupportLog_Evidence): kept records are stored whole; stream
// records keep the first kEvidenceHead and the newest kEvidenceTail, so the
// start of the session and what the tester did just before Open log are both
// in the report. Written with every full report.
constexpr size_t kEvidenceLineBytes = 384, kEvidenceHead = 2000, kEvidenceTail = 8000;
using EvidenceLine = std::array<char, kEvidenceLineBytes>;
SRWLOCK evidenceLock = SRWLOCK_INIT;
std::vector<EvidenceLine> evidenceKept, evidenceHead;
std::deque<EvidenceLine> evidenceTail;
std::uint64_t evidenceOmitted = 0;
SRWLOCK queueLock = SRWLOCK_INIT;
std::array<Line, kQueueLines> queue{};
size_t queued = 0;
std::atomic<unsigned> dropped{0};
std::atomic<bool> stopping{false}, inventoryDirty{true};
std::atomic<bool> incidentPending{false};
std::atomic<std::uint64_t> requestedSnapshot{0}, completedSnapshot{0};
std::atomic<bool> failurePending{false};
std::atomic<bool> researchPending{false};
std::atomic<HWND> uiWindow{nullptr};
std::atomic<DWORD> lastError{0};
std::atomic<SupportLogInputConfigProvider> inputConfigProvider{nullptr};
HANDLE worker = nullptr, stopEvent = nullptr;
std::wstring testMirrorDirectory;
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
// USB product name reported by the bus (the manufacturer's firmware string, e.g.
// "SU75 Pro"), read from Windows metadata without opening the device. Not the
// user-editable friendly name; no serial or path. Control characters, quotes
// and backslashes are replaced and the text is bounded to 48 characters.
void BusReportedName(HDEVINFO devices, SP_DEVINFO_DATA& info, char* out, int size) noexcept {
    out[0] = 0;
    wchar_t text[128]{}; DEVPROPTYPE type = 0;
    if (!SetupDiGetDevicePropertyW(devices, &info, &DEVPKEY_Device_BusReportedDeviceDesc, &type,
            reinterpret_cast<PBYTE>(text), sizeof(text) - sizeof(wchar_t), nullptr, 0) ||
        type != DEVPROP_TYPE_STRING)
        text[0] = 0;
    // Fall back to the parent USB node (composite interface or device).
    DEVINST parent = 0;
    if (!text[0] && CM_Get_Parent(&parent, info.DevInst, 0) == CR_SUCCESS) {
        ULONG bytes = sizeof(text) - sizeof(wchar_t);
        if (CM_Get_DevNode_PropertyW(parent, &DEVPKEY_Device_BusReportedDeviceDesc, &type,
                reinterpret_cast<PBYTE>(text), &bytes, 0) != CR_SUCCESS || type != DEVPROP_TYPE_STRING)
            text[0] = 0;
    }
    wchar_t clean[49]{}; size_t n = 0;
    for (const wchar_t* c = text; *c && n < 48; ++c) {
        wchar_t ch = *c;
        if (ch < 0x20 || ch == 0x7f || ch == L'"' || ch == L'\\') ch = L'_';
        clean[n++] = ch;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, clean, -1, out, size, nullptr, nullptr)) out[0] = 0;
}
void Inventory() {
    GUID guid{}; HidD_GetHidGuid(&guid);
    HDEVINFO devices = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) { SupportLog_Event("inventory.enumeration_failed", 0,SupportLog_Win32(GetLastError())); return; }
    // Read Windows metadata only. No device opens, feature reports or probes.
    for (DWORD index = 0; index < 256 && !stopping.load(); ++index) {
        SP_DEVINFO_DATA info{sizeof(info)};
        if (!SetupDiEnumDeviceInfo(devices, index, &info)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) SupportLog_Event("inventory.enum_error", index,SupportLog_Win32(GetLastError()));
            break;
        }
        wchar_t ids[2048]{}; DWORD type = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(devices, &info, SPDRP_HARDWAREID, &type,
            reinterpret_cast<BYTE*>(ids), sizeof(ids) - sizeof(wchar_t), nullptr)) {
            SupportLog_Event("inventory.metadata_error", index,SupportLog_Win32(GetLastError())); continue;
        }
        // Deliberately omit full instance paths, serials and user-editable names.
        _wcsupr_s(ids);
        const wchar_t* vid = wcsstr(ids, L"VID_");
        const wchar_t* pid = wcsstr(ids, L"PID_");
        const wchar_t* mi = wcsstr(ids, L"MI_");
        unsigned v = vid ? wcstoul(vid + 4, nullptr, 16) & 0xffff : 0;
        unsigned p = pid ? wcstoul(pid + 4, nullptr, 16) & 0xffff : 0;
        unsigned m = mi ? wcstoul(mi + 3, nullptr, 16) & 0xff : 0xff;
        char name[160]{};
        BusReportedName(devices, info, name, sizeof(name));
        char line[320]{};
        sprintf_s(line, "hid.candidate vid=%04X pid=%04X interface=%02X name=\"%s\" metadata_only=1", v, p, m, name);
        Enqueue(line);
    }
    SetupDiDestroyDeviceInfoList(devices);
    Enqueue("inventory.complete limit=256; HID candidates include mice and other devices, not proof of analogue capability");
}
const char* LifecycleName(unsigned state) noexcept {
    static constexpr const char* names[]={"stopped","starting","running","stop_requested","joined","faulted","poisoned"};
    return state<std::size(names)?names[state]:"unknown";
}
void Snapshot() {
    BackendAnalogTelemetry t{}; Backend_GetAnalogDiagnosticTelemetry(&t);
    char line[kLineBytes]{};
    static std::uint64_t sequence=0;
    const auto seq=++sequence;
    const auto engine = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot();
    const auto support = halljoy::keyboard_support::GetStatusSnapshot();
    // Fresh evidence and cached UI observation are deliberately separate.
    const char* source=t.deviceCount>0?"connected":t.nativeTelemetryComplete?"none_reported":"unknown";
    sprintf_s(line, "snapshot.begin seq=%llu schema=2 engine=%u engine_error=%u search_complete_cached=%d source=%s ui_connected_cached=%d sdk_initialised=%d sources=%d sdk_sources=%u native_sources=%u analog_error=%d full_buffer_result=%d",
        seq,unsigned(engine.state),engine.lastNativeError,support.searchCompleted,source,support.analogSourceConnected,
        t.sdkInitialised,t.deviceCount,t.sdkDeviceCount,t.nativeConnectedCount,t.lastAnalogError,t.fullBufferRet);
    Enqueue(line);
    sprintf_s(line,"diagnostics.coverage seq=%llu catalog=%u visited=%u rows=%d unavailable=%u complete=%d sampling=per_provider_not_atomic",
        seq,t.nativeCatalogCount,t.nativeVisitedCount,t.nativeProtocolCount,t.nativeTelemetryFailures,t.nativeTelemetryComplete);
    Enqueue(line);
    sprintf_s(line, "uap seq=%llu available=%d ready=%d status=%d error=%d transport_error=%d restarts=%d invalid_snapshots=%d",
        seq,t.pluginHostAvailable,t.pluginHostReady,t.pluginHostStatus,t.pluginHostLastError,
        t.pluginHostTransportError,t.pluginHostRestartCount,t.pluginHostInvalidSnapshots);
    Enqueue(line);
    if (const auto provider = inputConfigProvider.load()) {
        char config[kLineBytes - 32]{};
        provider(config, sizeof(config));
        if (config[0]) { sprintf_s(line, "input.config seq=%llu %s", seq, config); Enqueue(line); }
    }
    for (int i=0; i<t.nativeProtocolCount && i<kBackendMaxNativeProtocols; ++i) {
        const auto& n = t.nativeProtocols[i];
        const char* state=!n.telemetryAvailable?"unavailable":n.connected?"connected":n.present?"present_not_connected":"not_present";
        const char* transport=(n.flags&NativeAnalogBackendFlag_StreamTransport)?"stream":(n.flags&NativeAnalogBackendFlag_PolledTransport)?"polled":"unspecified";
        sprintf_s(line, "native seq=%llu index=%u id=%s protocol=%u observation=%s vid=%04X pid=%04X usage_page=%04X usage=%04X present=%d connected=%d flags=%u transport=%s mapped_keys=%u input_bytes=%u output_bytes=%u updates=%llu failures=%llu age_ms=%u age_valid=%d lifecycle=%s generation=%llu lifecycle_error=%u lifecycle_operation=%u native_error=%u",
            seq,n.catalogIndex,n.id[0]?n.id:"unknown",n.protocol,state,n.vendorId,n.productId,n.usagePage,n.usage,n.present,n.connected,n.flags,transport,
            n.mappedKeys,n.inputReportBytes,n.outputReportBytes,n.successfulUpdates,n.failedUpdates,n.lastUpdateAgeMs,n.telemetryAvailable&&n.successfulUpdates!=0,
            n.lifecycleAvailable?LifecycleName(n.lifecycleState):"unavailable",n.generation,n.lifecycleError,n.lifecycleOperation,n.nativeError);
        Enqueue(line);
    }
    sprintf_s(line,"snapshot.end seq=%llu rows=%d complete=%d",seq,t.nativeProtocolCount,t.nativeTelemetryComplete);
    Enqueue(line);
}
// Windows refuses to replace a file while any reader has it open, even with
// FILE_SHARE_DELETE (ERROR_ACCESS_DENIED). Readers are brief: Open log, an
// editor or antivirus. Retry the rename for up to ~1 s instead of falling into
// the 5 s write backoff that is meant for unusable destinations.
bool ReplaceLog(const std::wstring& source, const std::wstring& target) noexcept {
    for (unsigned attempt = 0;; ++attempt) {
        if (MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        const DWORD error = GetLastError();
        if ((error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION) || attempt >= 40) {
            SetLastError(error);
            return false;
        }
        Sleep(25);
    }
}
template <size_t N>
bool WriteLine(HANDLE file, const std::array<char, N>& line) {
    const DWORD length = static_cast<DWORD>(strlen(line.data())); DWORD written = 0;
    if (!WriteFile(file, line.data(), length, &written, nullptr) || written != length) return false;
    return WriteFile(file, "\r\n", 2, &written, nullptr) && written == 2;
}
DWORD WINAPI Run(void*) noexcept {
    try {
        std::deque<Line> history, researchEvidence, traceEvidence;
        std::vector<CaptureLine> capture; // writer copy of captureStore
        std::vector<Line> batch(kQueueLines);
        bool previousIncident = false, previousContinuous = false;
        struct Destination {
            std::wstring directory;
            bool opened = false, retryPending = false;
            ULONGLONG retryAfter = 0;
            DWORD fileBytes = 0, error = 0;
            size_t captureWritten = 0; // capture records already in the file
        };
        const auto primary = testDirectory.empty() ? SupportLog_Directory() : testDirectory;
        const auto mirror = testDirectory.empty() ? AppPaths_LegacyDataRoot() : testMirrorDirectory;
        std::array<Destination, 2> destinations{{{primary}, {mirror}}};
        const bool distinctMirror = !mirror.empty() && _wcsicmp(primary.c_str(), mirror.c_str()) != 0;
        unsigned ticks = 0;
        Enqueue("session.begin version=" HALLJOY_VERSION_STRING_FULL " schema=2 privacy=no_keys_no_input_values_no_serials_no_paths; support absence is not proof of an unsupported keyboard");
        for (;;) {
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
            const bool continuous = true;
            char pathLine[kLineBytes]{};
            DWORD foregroundPid=0;
            const HWND foreground=GetForegroundWindow();
            if(foreground)GetWindowThreadProcessId(foreground,&foregroundPid);
            sprintf_s(pathLine,"path.mode build=input-path-20260919-r1 diagnostic=1 automatic_logging=1 block=%u engine_running=%u foreground=%s cumulative=1 aggregate_only=1",
                unsigned(Settings_GetBlockBoundKeys()),unsigned(halljoy::engine_runtime::EngineRuntimeOwner_IsRunning()),
                !foreground ? "none" : foregroundPid==GetCurrentProcessId() ? "own" : "external");
            Enqueue(pathLine);
            for(unsigned bank=0;bank<2;++bank) {
                using namespace halljoy::input_path;
                sprintf_s(pathLine,"path.counts block=%u source_frames=%llu source_positive=%llu fn_unavailable=%llu configured_reads=%llu raw_positive=%llu filtered_positive=%llu frames=%llu active_frames=%llu published=%llu rejected=%llu bound_passed=%llu bound_blocked=%llu",
                    bank,Read(bank,SourceFrames),Read(bank,SourcePositive),Read(bank,FnUnavailable),
                    Read(bank,ConfiguredReads),Read(bank,RawPositive),Read(bank,FilteredPositive),
                    Read(bank,BuiltFrames),Read(bank,ActiveFrames),Read(bank,Published),Read(bank,PublishRejected),
                    Read(bank,BoundPassed),Read(bank,BoundBlocked));
                Enqueue(pathLine);
            }
            Backend_InputPathStatus(pathLine,sizeof(pathLine));Enqueue(pathLine);
            if(ticks%5==0)Snapshot();
#else
            const bool continuous = Settings_GetDiagnosticLogging();
#endif
            const auto s = halljoy::keyboard_support::GetStatusSnapshot();
            const bool incident = halljoy::keyboard_support::ShouldAutoSaveSupportLog(s);
            const auto snapshotRequest = requestedSnapshot.load();
            const bool bannerRequested = incidentPending.exchange(false);
            const bool snapshotRequested = snapshotRequest != completedSnapshot.load();
            const bool requested = bannerRequested || snapshotRequested;
            const bool failure = failurePending.exchange(false);
            const bool trigger = failure || requested || (incident && !previousIncident) || (continuous && !previousContinuous);
            if (bannerRequested) Enqueue("support.banner_shown incident_latched=1");
            if (snapshotRequested) Enqueue("support.snapshot_requested source=api");
            if (incident != previousIncident) SupportLog_Event("support.banner", incident);
            if (continuous != previousContinuous) SupportLog_Event("logging.continuous", continuous);
            const bool changed = inventoryDirty.exchange(false);
            if (trigger || changed) { Inventory(); Snapshot(); }
            else if (ticks % 30 == 0) Snapshot();
            const bool research = researchPending.exchange(false);
            bool shouldWrite = research || failure || requested || continuous || incident || (previousIncident && !incident) || (previousContinuous && !continuous);
            const bool mirrorEnabled = continuous || previousContinuous;
            previousIncident = incident; previousContinuous = continuous;
            ++ticks;
            size_t count;
            AcquireSRWLockExclusive(&queueLock);
            count = queued;
            for (size_t i=0;i<count;++i) batch[i]=queue[i];
            queued=0;
            ReleaseSRWLockExclusive(&queueLock);
            const unsigned lost = dropped.exchange(0);
            AcquireSRWLockShared(&captureLock);
            for (size_t i = capture.size(); i < captureStore.size(); ++i) capture.push_back(captureStore[i]);
            ReleaseSRWLockShared(&captureLock);
            if (lost) SupportLog_Event("logging.queue_dropped", lost);
            // Keep reviewed research through Open log snapshot resets; no extra file.
            // The batch itself decides: the producer sets researchPending only
            // after releasing the queue, so a record taken in this batch can
            // carry its flag into the next, empty tick and never be written.
            for(size_t i=0;i<count;++i) {
                if(strstr(batch[i].data()," redsquare.research ")) {
                    shouldWrite = true;
                    if(strstr(batch[i].data(),"HallJoy RedSquare code probe v1;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare stream probe v3;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare stream probe v4;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare stream probe v5;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare analog gamepad trial v1;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare analog trace v2;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare batch68 probe v1;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare unknown frame trace v1;") ||
                       strstr(batch[i].data(),"HallJoy RedSquare unknown packet trace v2;"))researchEvidence.clear();
                    // Keep the trial marker and newest evidence, including final
                    // cleanup, even when an unexpectedly long run exceeds the cap.
                    if(researchEvidence.size()>=2300)
                        researchEvidence.erase(researchEvidence.begin()+1);
                    researchEvidence.push_back(batch[i]);
                }
            }
            for(size_t i=0;i<count;++i) {
                // Input-chain trace keeps its own window so it neither evicts
                // nor is evicted by snapshots (support_log.h).
                if(strstr(batch[i].data()," trace.")) {
                    traceEvidence.push_back(batch[i]);
                    if(traceEvidence.size()>kTraceLines) traceEvidence.pop_front();
                    continue;
                }
                history.push_back(batch[i]); if(history.size()>kHistoryLines) history.pop_front();
            }
#if defined(HALLJOY_AULA_MINI60_DIAGNOSTIC) || defined(HALLJOY_IROK_NA87_DIAGNOSTIC) || defined(HALLJOY_DEVICE_SUPPORT_LOG)
            for (size_t i=0;i<count;++i) DebugLog_Write(L"[support] %S", batch[i].data());
            (void)shouldWrite; (void)mirrorEnabled; (void)destinations; (void)distinctMirror;
#else
            // One snapshot timestamp for both destinations. A second boundary
            // between writes must not make otherwise identical mirrors differ.
            SYSTEMTIME snapshotUtc{}; GetSystemTime(&snapshotUtc);
            for (size_t sink = 0; sink < destinations.size(); ++sink) {
                auto& state = destinations[sink];
                if (sink == 1 && (!distinctMirror || !mirrorEnabled)) {
                    state.retryPending = false; state.retryAfter = 0; state.error = 0;
                    continue;
                }
                auto& directory = state.directory;
                auto& opened = state.opened;
                auto& retryPending = state.retryPending;
                auto& retryAfter = state.retryAfter;
                auto& fileBytes = state.fileBytes;
                const auto path = directory + L"\\HallJoy.log";
                if (((shouldWrite && (count || trigger)) || retryPending) && GetTickCount64() >= retryAfter) {
                    retryPending=true;
                    retryAfter=GetTickCount64()+5000; // Retain failed automatic reports; no tight retry loop.
                    if (directory.empty()) { state.error = ERROR_PATH_NOT_FOUND; }
                    else {
                        CreateDirectoryW(directory.c_str(), nullptr);
                        const bool reset = !opened || trigger || fileBytes + count*kLineBytes > kFileLimit;
                        const auto destination = reset ? path + L".tmp" : path;
                        HANDLE file = CreateFileW(destination.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                            reset ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (file == INVALID_HANDLE_VALUE) { state.error = GetLastError(); opened=false; }
                        else {
                            bool ok = true;
                            if (reset) {
                                fileBytes=0;
                                Line header{};
                                const bool rawHidPayload=std::any_of(researchEvidence.begin(),researchEvidence.end(),[](const Line& line){
                                    return strstr(line.data(),"HallJoy RedSquare unknown packet trace v2;")!=nullptr;
                                });
                                sprintf_s(header.data(),header.size(),"HallJoy " HALLJOY_VERSION_STRING_FULL " support report schema=2 utc=%04u-%02u-%02uT%02u:%02u:%02uZ bounded_history=512 no_keyboard_text=1 raw_hid_payload=%u bound_key_trace=%u",snapshotUtc.wYear,snapshotUtc.wMonth,snapshotUtc.wDay,snapshotUtc.wHour,snapshotUtc.wMinute,snapshotUtc.wSecond,rawHidPayload?1u:0u,traceEvidence.empty()?0u:1u);
                                ok=WriteLine(file,header); fileBytes=DWORD(strlen(header.data())+2);
                                for (const auto& line : researchEvidence) { if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                                for (const auto& line : history) { if(strstr(line.data()," redsquare.research "))continue; if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                                for (const auto& line : traceEvidence) { if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                                for (const auto& line : capture) { if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                                std::vector<EvidenceLine> evidence;
                                std::uint64_t omitted = 0;
                                try {
                                    AcquireSRWLockShared(&evidenceLock);
                                    evidence.reserve(evidenceKept.size()+evidenceHead.size()+evidenceTail.size()+1);
                                    evidence.insert(evidence.end(),evidenceKept.begin(),evidenceKept.end());
                                    evidence.insert(evidence.end(),evidenceHead.begin(),evidenceHead.end());
                                    omitted = evidenceOmitted;
                                    if (omitted) {
                                        evidence.emplace_back();
                                        _snprintf_s(evidence.back().data(),kEvidenceLineBytes,_TRUNCATE,
                                            "evidence.omitted records=%llu between_first=%zu and_newest=%zu",omitted,evidenceHead.size(),evidenceTail.size());
                                    }
                                    evidence.insert(evidence.end(),evidenceTail.begin(),evidenceTail.end());
                                } catch (...) {}
                                ReleaseSRWLockShared(&evidenceLock);
                                for (const auto& line : evidence) { if (!ok || !(ok=WriteLine(file,line))) break; fileBytes += DWORD(strlen(line.data())+2); }
                            } else {
                                SetFilePointer(file, 0, nullptr, FILE_END);
                                for(size_t i=0;i<count;++i) { if(!(ok=WriteLine(file,batch[i]))) break; fileBytes += DWORD(strlen(batch[i].data())+2); }
                                for(size_t i=state.captureWritten;ok && i<capture.size();++i) { if(!(ok=WriteLine(file,capture[i]))) break; fileBytes += DWORD(strlen(capture[i].data())+2); }
                            }
                            if (ok) state.captureWritten = capture.size();
                            DWORD error = ok ? 0 : GetLastError();
                            CloseHandle(file);
                            if(ok && reset && !ReplaceLog(destination, path)) {
                                ok=false; error=GetLastError();
                            }
                            if(reset && !ok) DeleteFileW(destination.c_str()); // Only our exact transient file.
                            opened=ok;
                            if(ok) { retryPending=false; retryAfter=0;
                                if (sink == 0) completedSnapshot.store(snapshotRequest);
                            }
                            state.error = ok ? 0 : (error ? error : ERROR_WRITE_FAULT);
                        }
                    }
                }
            }
            Error(destinations[0].error ? destinations[0].error : destinations[1].error);
#endif
            if (stopping.load()) break;
            if (WaitForSingleObject(stopEvent, 1000) == WAIT_OBJECT_0) stopping=true;
        }
    } catch (...) { Error(ERROR_UNHANDLED_EXCEPTION); return 1; }
    return 0;
}
}
std::wstring SupportLog_Directory() {
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
    wchar_t executable[32768]{};
    const DWORD size=GetModuleFileNameW(nullptr,executable,_countof(executable));
    if (!size || size>=_countof(executable)) return {};
    std::wstring directory(executable,size);
    const auto separator=directory.find_last_of(L"\\/");
    if (separator==std::wstring::npos) return {};
    directory.resize(separator+1);
    return directory;
#else
    return AppPaths_DataRoot(); // Respects portable mode and isolated test roots.
#endif
}
bool SupportLog_Start(const wchar_t* directoryOverride, const wchar_t* mirrorOverride) noexcept {
    if(worker) return true;
    try { testDirectory=directoryOverride ? directoryOverride : L"";
          testMirrorDirectory=mirrorOverride ? mirrorOverride : L""; }
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
void SupportLog_Event(const char* category, std::uint64_t value, SupportLogDetail detail) noexcept {
    const bool failure=detail.kind==SupportLogDetailKind::Win32 || detail.kind==SupportLogDetailKind::Protocol;
    const char* kind=detail.kind==SupportLogDetailKind::Win32?"win32":detail.kind==SupportLogDetailKind::Protocol?"protocol":detail.kind==SupportLogDetailKind::Data?"data":"none";
    char line[256]{};
    _snprintf_s(line,sizeof(line),_TRUNCATE,"%s value=%llu error=%llu detail=%llu detail_kind=%s",category,value,failure?detail.value:0,detail.value,kind);
    Enqueue(line);
}
void SupportLog_OverlaySummary(const wchar_t* aggregate) noexcept {
    if(!Settings_GetDiagnosticLogging()) return;
    char line[kLineBytes]{};
    if(WideCharToMultiByte(CP_UTF8,0,aggregate,-1,line,sizeof(line),nullptr,nullptr)) Enqueue(line);
}
void SupportLog_InventoryChanged() noexcept { inventoryDirty=true; }
void SupportLog_ReportMissingSource() noexcept { incidentPending=true; }
void SupportLog_ReportFailure(const char* category, std::uint64_t error) noexcept {
    SupportLog_Event(category,0,SupportLog_Win32(error)); failurePending=true;
}
void SupportLog_SetWindow(HWND window) noexcept { uiWindow=window; }
DWORD SupportLog_LastError() noexcept { return lastError.load(); }
std::uint64_t SupportLog_RequestSnapshot() noexcept { return ++requestedSnapshot; }
void SupportLog_SetInputConfigProvider(SupportLogInputConfigProvider provider) noexcept { inputConfigProvider.store(provider); }
void SupportLog_Trace(const char* record) noexcept {
    if (!record || strncmp(record, "trace.", 6) != 0) return;
    Enqueue(record);
}
bool SupportLog_Capture(const char* record) noexcept {
    if (!record || strncmp(record, "capture.", 8) != 0) return false;
    bool stored = false;
    AcquireSRWLockExclusive(&captureLock);
    try {
        if (captureStore.size() < kCaptureLines) {
            if (captureStore.empty()) captureStore.reserve(kCaptureLines);
            captureStore.emplace_back();
            _snprintf_s(captureStore.back().data(), kCaptureLineBytes, _TRUNCATE, "uptime_ms=%llu %s",
                GetTickCount64(), record);
            stored = true;
        }
    } catch (...) {
    }
    ReleaseSRWLockExclusive(&captureLock);
    return stored;
}
std::uint64_t SupportLog_CompletedSnapshot() noexcept { return completedSnapshot.load(); }
void SupportLog_Evidence(const char* record, bool keep) noexcept {
    if (!record || strncmp(record, "evidence.", 9) != 0) return;
    AcquireSRWLockExclusive(&evidenceLock);
    try {
        EvidenceLine line{};
        _snprintf_s(line.data(), kEvidenceLineBytes, _TRUNCATE, "uptime_ms=%llu %s", GetTickCount64(), record);
        if (keep) evidenceKept.push_back(line);
        else if (evidenceHead.size() < kEvidenceHead) evidenceHead.push_back(line);
        else {
            evidenceTail.push_back(line);
            if (evidenceTail.size() > kEvidenceTail) { evidenceTail.pop_front(); ++evidenceOmitted; }
        }
    } catch (...) {
    }
    ReleaseSRWLockExclusive(&evidenceLock);
}

bool SupportLog_RedSquareResearch(const char* record) noexcept {
    if(!record)return false;
    const auto length=strnlen_s(record,240);
    if(!length || length>=240)return false;
    // Records are generated internally; strip CR/LF so one record is one log line.
    char clean[240]{};size_t n=0;
    for(size_t i=0;i<length;++i)if(record[i]!='\r' && record[i]!='\n') {
        if(static_cast<unsigned char>(record[i])<32 || static_cast<unsigned char>(record[i])>126)return false;
        clean[n++]=record[i];
    }
    AcquireSRWLockExclusive(&queueLock);
    const bool accepted=queued<queue.size();
    if(accepted)_snprintf_s(queue[queued++].data(),kLineBytes,_TRUNCATE,"uptime_ms=%llu redsquare.research %s",GetTickCount64(),clean);
    ReleaseSRWLockExclusive(&queueLock);
    if(accepted)researchPending.store(true);
    return accepted;
}
