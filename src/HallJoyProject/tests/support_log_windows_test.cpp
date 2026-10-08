#define NOMINMAX
#include "support_log.h"
#include "input_path_diagnostics.h"
#include "backend.h"
#include "settings.h"
#include "keyboard_support_status.h"
#include "engine_runtime_owner.h"
#include <atomic>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

static std::atomic<bool> enabled{false}, missing{false};
static std::atomic<bool> experimental{false}, unstable{false}, limited{false};
const std::wstring& AppPaths_DataRoot() { static const std::wstring empty; return empty; }
const std::wstring& AppPaths_LegacyDataRoot() { static const std::wstring empty; return empty; }
bool Settings_GetDiagnosticLogging() { return enabled; }
void Backend_GetAnalogDiagnosticTelemetry(BackendAnalogTelemetry* out) {
    *out={}; out->pluginHostLastError=123;
    out->pluginDeviceCount=1;
    strcpy_s(out->pluginDevices[0].name, "PRIVATE_DEVICE_NAME_SENTINEL");
    strcpy_s(out->pluginDevices[0].manufacturer, "PRIVATE_MANUFACTURER_SENTINEL");
    out->nativeProtocolCount=kBackendMaxNativeProtocols;
    out->nativeCatalogCount=out->nativeVisitedCount=kBackendMaxNativeProtocols;
    out->nativeTelemetryComplete=true; out->nativeConnectedCount=1; out->deviceCount=1;
    for(int i=0;i<kBackendMaxNativeProtocols;++i) {
        auto& n=out->nativeProtocols[i]; n.catalogIndex=i; n.telemetryAvailable=true;
        n.lifecycleAvailable=true; n.lifecycleState=2;
        sprintf_s(n.id,"provider_%d",i);
    }
    auto& last=out->nativeProtocols[kBackendMaxNativeProtocols-1];
    last.present=last.connected=true; last.failedUpdates=7; last.successfulUpdates=42;
}
namespace halljoy::keyboard_support {
StatusSnapshot GetStatusSnapshot() noexcept { StatusSnapshot s{true,!missing.load()}; s.frozenModels=experimental ? ImplementedModels : 0; if(limited) s.frozenModels |= Mad68DualLimited; s.communicationWarning=unstable; return s; }
}
namespace halljoy::engine_runtime {
runtime_command::SnapshotV1 EngineRuntimeOwner_Snapshot() noexcept { return {}; }
}
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
bool Settings_GetBlockBoundKeys() { return true; }
namespace halljoy::engine_runtime { bool EngineRuntimeOwner_IsRunning() noexcept { return true; } }
void Backend_InputPathStatus(char* text,std::size_t capacity) noexcept {
    strcpy_s(text,capacity,"path.output state=2 ready=1 applied=42 published=42");
}
#endif
#define Check(value) do { if(!(value)) throw std::runtime_error("support log regression line "+std::to_string(__LINE__)); } while(0)
static std::string Read(const std::wstring& path) {
    std::ifstream input{std::filesystem::path(path)};
    return {std::istreambuf_iterator<char>(input),{}};
}
static void Await(const std::wstring& path, const char* text) {
    auto end=GetTickCount64()+7000;
    do { if(Read(path).find(text)!=std::string::npos) return; Sleep(50); } while(GetTickCount64()<end);
    throw std::runtime_error(std::string("await failed: ")+text);
}
int main() {
    // Create the private directory atomically. A temp file deleted and then
    // reused as a directory name can be taken by a concurrent test process.
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH,temp);
    std::wstring directory;
    for(unsigned attempt=0;attempt<1000 && directory.empty();++attempt) {
        const auto candidate=std::wstring(temp)+L"hjl-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(attempt);
        if(CreateDirectoryW(candidate.c_str(),nullptr)) directory=candidate;
    }
    Check(!directory.empty());
    const std::wstring path=directory+L"\\HallJoy.log";
    const auto mirrorDir = directory+L"\\mirror", mirrorPath=mirrorDir+L"\\HallJoy.log";
    try {
        SupportLog_SetInputConfigProvider([](char* text, std::size_t capacity) noexcept {
            snprintf(text, capacity, "global_invert=1 triggers=2 triggers_inverted=2"); });
        Check(SupportLog_Start(directory.c_str(), mirrorDir.c_str()));
        SupportLog_Event("before.failure",42,SupportLog_Win32(5));
        SupportLog_Event("metadata.profile",1,SupportLog_Data(2));
        Sleep(1300);
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        Await(path,"automatic_logging=1");
        halljoy::input_path::Add(true,halljoy::input_path::SourcePositive,7);
        halljoy::input_path::Add(false,halljoy::input_path::BoundPassed,3);
        Await(path,"source_positive=7");
        Await(path,"bound_passed=3");
        Await(path,"applied=42 published=42");
        Check(Read(path).find("raw_depth=")==std::string::npos);
#else
        Check(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);
#endif
#if !defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        // A known low-quality firmware banner must not create a log, connected or not.
        limited=true;
        Sleep(1300); Check(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);
        missing=true;
        Sleep(1300); Check(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);
        limited=false; missing=false;
        // Communication warning alone must not force a file, even without input.
        unstable=true; missing=true;
        Sleep(1300);
        Check(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);
        SupportLog_Trace("trace.pad p=0 lx=1 ly=0"); SupportLog_Trace("not.a.trace");
        // Format capture: own store of the first 8000 records, no queue, no time bound.
        Check(!SupportLog_Capture("not.a.capture") && !SupportLog_Capture(nullptr));
        for (int i = 0; i < 8000; ++i) {
            char record[64]{}; sprintf_s(record, "capture.test n=%d", i);
            Check(SupportLog_Capture(record));
        }
        Check(!SupportLog_Capture("capture.test overflow"));
        // Device evidence: kept records whole; stream records first 2000 + newest 8000.
        SupportLog_Evidence("not.evidence",true); SupportLog_Evidence(nullptr,false);
        SupportLog_Evidence("evidence.test kept probe",true);
        for (int i = 0; i < 12000; ++i) {
            char record[64]{}; sprintf_s(record, "evidence.test s=%d", i);
            SupportLog_Evidence(record,false);
        }
        SupportLog_Event("manual.snapshot",17);
        const auto request=SupportLog_RequestSnapshot();
        const auto deadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<request && GetTickCount64()<deadline) Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=request);
        const auto manualEvidence=Read(path);
        Check(manualEvidence.find("manual.snapshot value=17")!=std::string::npos);
        Check(manualEvidence.find("support.snapshot_requested source=api")!=std::string::npos);
        Check(manualEvidence.find(" input.config seq=")!=std::string::npos &&
              manualEvidence.find("global_invert=1 triggers=2 triggers_inverted=2")!=std::string::npos);
        Check(manualEvidence.find(" trace.pad p=0 lx=1 ly=0")!=std::string::npos &&
              manualEvidence.find("bound_key_trace=1")!=std::string::npos &&
              manualEvidence.find("not.a.trace")==std::string::npos);
        Check(manualEvidence.find(" capture.test n=0\n")!=std::string::npos &&
              manualEvidence.find(" capture.test n=7999\n")!=std::string::npos &&
              manualEvidence.find("capture.test overflow")==std::string::npos &&
              manualEvidence.find("not.a.capture")==std::string::npos);
        Check(manualEvidence.find(" evidence.test kept probe\n")!=std::string::npos &&
              manualEvidence.find(" evidence.test s=0\n")!=std::string::npos &&
              manualEvidence.find(" evidence.test s=1999\n")!=std::string::npos &&
              manualEvidence.find(" evidence.test s=2000\n")==std::string::npos &&
              manualEvidence.find(" evidence.test s=3999\n")==std::string::npos &&
              manualEvidence.find("evidence.omitted records=2000 between_first=2000 and_newest=8000")!=std::string::npos &&
              manualEvidence.find(" evidence.test s=4000\n")!=std::string::npos &&
              manualEvidence.find(" evidence.test s=11999\n")!=std::string::npos &&
              manualEvidence.find("not.evidence")==std::string::npos);
        Check(manualEvidence.find("support.banner_shown") == std::string::npos);
        Check(GetFileAttributesW(mirrorPath.c_str())==INVALID_FILE_ATTRIBUTES);
        missing=false; unstable=false; experimental=true;
        SupportLog_Event("experimental.snapshot",18);
        Await(path,"experimental.snapshot value=18");
        Await(path,"support.banner value=1");
        experimental=false;
        Await(path,"support.banner value=0");
#endif
        // Even a banner that disappears before the writer tick must be captured.
        SupportLog_ReportMissingSource();
        Await(path,"support.banner_shown incident_latched=1");
        missing=true;
        Await(path,"support.banner value=1");
        Await(path,"before.failure value=42 error=5");
        Await(path,"metadata.profile value=1 error=0 detail=2 detail_kind=data");
        Await(path,"failures=7");
        Await(path,"source=connected");
        Await(path,"age_valid=1 lifecycle=running");
        Await(path,"snapshot.end seq=");
        Check(Read(path).find("observation=not_present")!=std::string::npos);
        Check(Read(path).find("observation=connected")!=std::string::npos);
        Await(path,"error=123");
        missing=false;
        Await(path,"support.banner value=0");
#if !defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        Check(GetFileAttributesW(mirrorPath.c_str())==INVALID_FILE_ATTRIBUTES);
#endif
        enabled=true;
        Await(path,"logging.continuous value=1");
        Await(mirrorPath,"logging.continuous value=1");
        enabled=false;
#if !defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        Await(path,"logging.continuous value=0");
        Await(mirrorPath,"logging.continuous value=0");
        const auto mirrorBefore=Read(mirrorPath);
#endif
        SupportLog_ReportFailure("runtime.failure",87);
        Await(path,"runtime.failure value=0 error=87");
#if !defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        Check(Read(mirrorPath)==mirrorBefore);
#endif
        Check(Read(path).find("PRIVATE_DEVICE_NAME_SENTINEL")==std::string::npos);
        Check(Read(path).find("PRIVATE_MANUFACTURER_SENTINEL")==std::string::npos);
        Check(Read(path).find("hid.candidate vid=")!=std::string::npos ||
            Read(path).find("inventory.complete")!=std::string::npos);
        {   // Every inventory row carries the bus-reported product name field.
            const std::string text=Read(path);
            for (size_t at=text.find("hid.candidate vid=");at!=std::string::npos;at=text.find("hid.candidate vid=",at+1)) {
                const size_t end=text.find('\n',at);
                const std::string row=text.substr(at,end==std::string::npos?std::string::npos:end-at);
                Check(row.find(" name=\"")!=std::string::npos && row.find("\" metadata_only=1")!=std::string::npos);
            }
        }
        enabled=true;
        for (unsigned i=0;i<100000;++i) SupportLog_Event("test.burst",i);
        Await(path,"logging.queue_dropped");
        Check(std::filesystem::file_size(path)<=4*1024*1024);
        Check(SupportLog_Stop());
        Check(GetFileAttributesW((path+L".tmp").c_str())==INVALID_FILE_ATTRIBUTES);
        Check(std::filesystem::file_size(path)<=4*1024*1024);
        Check(Read(path)==Read(mirrorPath));
        DeleteFileW(mirrorPath.c_str()); RemoveDirectoryW(mirrorDir.c_str());
        // Reviewed research remains in the one normal log after history eviction
        // and Open log snapshot reset; ordinary logging stays disabled.
        enabled=false;
        Check(SupportLog_Start(directory.c_str()));
        Check(SupportLog_RedSquareResearch("HallJoy RedSquare code probe v1; test"));
        for(unsigned group=0;group<3;++group) {
            for(unsigned i=0;i<250;++i) {
                const auto row="block "+std::to_string(group*250+i)+" aa";
                Check(SupportLog_RedSquareResearch(row.c_str()));
            }
            Sleep(1200);
        }
        Check(SupportLog_RedSquareResearch("complete blocks=750"));
        Await(path,"complete blocks=750");
        auto researchRequest=SupportLog_RequestSnapshot();
        auto researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto evidence=Read(path);
        Check(evidence.find("block 0 aa")!=std::string::npos);
        Check(evidence.find("block 749 aa")!=std::string::npos);
        Check(evidence.find("complete blocks=750")!=std::string::npos);
        const auto firstBlock=evidence.find("block 0 aa");
        Check(evidence.find("block 0 aa",firstBlock+1)==std::string::npos);
        Check(SupportLog_RedSquareResearch("HallJoy RedSquare analog gamepad trial v1; exact Alumix104 only"));
        Check(SupportLog_RedSquareResearch("analog_mode initial_off=1 on_write=1"));
        Check(SupportLog_RedSquareResearch("analog_idle reads=16 samples=0 on_ack=0"));
        Check(SupportLog_RedSquareResearch("analog_cleanup begin=1 reason=0"));
        Check(SupportLog_RedSquareResearch("analog_pipeline published=0 consumer_reads=0"));
        Await(path,"analog_pipeline published=0 consumer_reads=0");
        researchRequest=SupportLog_RequestSnapshot();
        researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto streamEvidence=Read(path);
        Check(streamEvidence.find("HallJoy RedSquare analog gamepad trial v1;")!=std::string::npos);
        Check(streamEvidence.find("analog_mode initial_off=1")!=std::string::npos);
        Check(streamEvidence.find("analog_idle reads=16 samples=0")!=std::string::npos);
        Check(streamEvidence.find("analog_cleanup begin=1 reason=0")!=std::string::npos);
        Check(streamEvidence.find("analog_pipeline published=0")!=std::string::npos);
        Check(streamEvidence.find("block 0 aa")==std::string::npos);
        for(unsigned group=0;group<10;++group) {
            for(unsigned i=0;i<250;++i) {
                const auto row="long_block "+std::to_string(group*250+i)+" end";
                Check(SupportLog_RedSquareResearch(row.c_str()));
            }
            Sleep(1200);
        }
        Check(SupportLog_RedSquareResearch("analog_end reason=0 final_off=1 final_off_ack=1"));
        Check(SupportLog_RedSquareResearch("analog_pipeline published=100 consumer_reads=20"));
        Check(SupportLog_RedSquareResearch("analog_gamepad pub_while_connected=10"));
        Await(path,"analog_gamepad pub_while_connected=10");
        researchRequest=SupportLog_RequestSnapshot();
        researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto longEvidence=Read(path);
        Check(longEvidence.find("HallJoy RedSquare analog gamepad trial v1;")!=std::string::npos);
        Check(longEvidence.find("long_block 0 end")==std::string::npos);
        Check(longEvidence.find("long_block 2499 end")!=std::string::npos);
        Check(longEvidence.find("analog_end reason=0 final_off=1 final_off_ack=1")!=std::string::npos);
        Check(longEvidence.find("analog_gamepad pub_while_connected=10")!=std::string::npos);
        Check(SupportLog_RedSquareResearch("HallJoy RedSquare analog trace v2; exact Alumix104 only"));
        Check(SupportLog_RedSquareResearch("trace_source ms=250 r=250 p=200 z=0 same=180"));
        Check(SupportLog_RedSquareResearch("trace_final windows=1 slots=2 aa=0 other55=0 other_prefix=0"));
        Await(path,"trace_final windows=1 slots=2");
        researchRequest=SupportLog_RequestSnapshot();
        researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto traceEvidence=Read(path);
        Check(traceEvidence.find("HallJoy RedSquare analog trace v2;")!=std::string::npos);
        Check(traceEvidence.find("trace_source ms=250")!=std::string::npos);
        Check(traceEvidence.find("trace_final windows=1")!=std::string::npos);
        Check(traceEvidence.find("HallJoy RedSquare analog gamepad trial v1;")==std::string::npos);
        Check(SupportLog_RedSquareResearch("HallJoy RedSquare unknown frame trace v1; exact Alumix104 only"));
        Check(SupportLog_RedSquareResearch("trace_unknown_classes count=1 short_or_report_id=0"));
        Check(SupportLog_RedSquareResearch("trace_unknown slot=1 reports=20 changed=3"));
        Await(path,"trace_unknown slot=1 reports=20");
        researchRequest=SupportLog_RequestSnapshot();
        researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto unknownEvidence=Read(path);
        Check(unknownEvidence.find("HallJoy RedSquare unknown frame trace v1;")!=std::string::npos);
        Check(unknownEvidence.find("raw_hid_payload=0")!=std::string::npos);
        Check(unknownEvidence.find("trace_unknown slot=1 reports=20")!=std::string::npos);
        Check(unknownEvidence.find("HallJoy RedSquare analog trace v2;")==std::string::npos);
        Check(SupportLog_RedSquareResearch("HallJoy RedSquare unknown packet trace v2; exact Alumix104 only"));
        Check(SupportLog_RedSquareResearch("unknown_packet id=1 slot=1 report_n=1 bytes=65 held_letters=2"));
        const std::string rawLine="unknown_bytes id=1 hex="+std::string(130,'A');
        Check(SupportLog_RedSquareResearch(rawLine.c_str()));
        Await(path,"unknown_bytes id=1 hex=");
        researchRequest=SupportLog_RequestSnapshot();
        researchDeadline=GetTickCount64()+7000;
        while(SupportLog_CompletedSnapshot()<researchRequest && GetTickCount64()<researchDeadline)Sleep(50);
        Check(SupportLog_CompletedSnapshot()>=researchRequest);
        const auto rawEvidence=Read(path);
        Check(rawEvidence.find("HallJoy RedSquare unknown packet trace v2;")!=std::string::npos);
        Check(rawEvidence.find("raw_hid_payload=1")!=std::string::npos);
        Check(rawEvidence.find("unknown_packet id=1 slot=1")!=std::string::npos);
        Check(rawEvidence.find(rawLine)!=std::string::npos);
        Check(rawEvidence.find("HallJoy RedSquare unknown frame trace v1;")==std::string::npos);
        Check(!SupportLog_RedSquareResearch("bad\tvalue"));
        Check(SupportLog_Stop());
        // Real failed destination must report failure, not claim logging worked.
        enabled=true;
        Check(SupportLog_Start((directory+L"\\absent\\child").c_str()));
        Sleep(1300); Check(SupportLog_LastError()!=0);
        Check(CreateDirectoryW((directory+L"\\absent").c_str(),nullptr));
        const auto recovered = directory+L"\\absent\\child\\HallJoy.log";
        Await(recovered,"support report schema=2");
        Check(SupportLog_LastError()==0);
        Check(SupportLog_Stop());
        DeleteFileW(recovered.c_str());
        RemoveDirectoryW((directory+L"\\absent\\child").c_str());
        RemoveDirectoryW((directory+L"\\absent").c_str());
        // A blocked mirror must not stop the primary; recovery remains independent.
        const auto blocked=directory+L"\\blocked";
        Check(CreateDirectoryW(blocked.c_str(),nullptr));
        const auto blockedLog=blocked+L"\\HallJoy.log";
        HANDLE lock=CreateFileW(blockedLog.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
        Check(lock!=INVALID_HANDLE_VALUE);
        Check(SupportLog_Start(directory.c_str(),blocked.c_str()));
        SupportLog_Event("mirror.locked",1);
        Await(path,"mirror.locked value=1");
        // A held destination is retried for ~1 s (a brief reader looks the
        // same to Windows) before the failure is reported.
        {
            const auto deadline=GetTickCount64()+3000;
            while(SupportLog_LastError()==0 && GetTickCount64()<deadline) Sleep(25);
        }
        Check(SupportLog_LastError()!=0);
        CloseHandle(lock);
        Await(blockedLog,"mirror.locked value=1");
        Check(SupportLog_LastError()==0);
        Check(SupportLog_Stop());
        DeleteFileW(blockedLog.c_str()); RemoveDirectoryW(blocked.c_str());
        // A reader holding the log (Open log, an editor, antivirus) blocks the
        // atomic replace. That brief lock must not push the rewrite into the
        // 5 s failure backoff: the snapshot completes soon after the reader closes.
        Check(SupportLog_Start(directory.c_str()));
        SupportLog_Event("reader.race",1);
        Await(path,"reader.race value=1");
        {
            // Busy reader: holds the log 20 ms of every 30 ms for 2.5 s, which
            // covers the rewrite whenever the worker reaches it.
            std::atomic<bool> polling{true};
            std::thread reader([&] {
                while(polling.load()) {
                    { std::ifstream holder{std::filesystem::path(path)}; Sleep(20); }
                    Sleep(10);
                }
            });
            const auto request=SupportLog_RequestSnapshot();
            Sleep(2500);
            polling=false; reader.join();
            const auto deadline=GetTickCount64()+2000; // completes well before a 5 s backoff
            while(SupportLog_CompletedSnapshot()<request && GetTickCount64()<deadline) Sleep(25);
            Check(SupportLog_CompletedSnapshot()>=request);
            Check(SupportLog_LastError()==0);
        }
        Check(SupportLog_Stop());
        // Portable paths coincide: one writer, no duplicated session records.
        Check(SupportLog_Start(directory.c_str(),directory.c_str()));
        Check(SupportLog_Stop());
        const auto portable=Read(path);
        const auto begin=portable.find("session.begin");
        Check(begin!=std::string::npos && portable.find("session.begin",begin+1)==std::string::npos);
        DeleteFileW(path.c_str()); RemoveDirectoryW(directory.c_str());
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        std::cout<<"INPUT_PATH_LOG_WINDOWS=PASS forced_logging two_banks aggregate_counts output_ack final_flush io_failure recovery\n";
#else
        std::cout<<"SUPPORT_LOG_WINDOWS=PASS off_no_file auto_incident prehistory recovery opt_in io_failure capture_store evidence_store\n";
#endif
        return 0;
    } catch(...) {
        SupportLog_Stop(); DeleteFileW(path.c_str()); DeleteFileW((path+L".tmp").c_str()); RemoveDirectoryW(directory.c_str());
        throw;
    }
}
