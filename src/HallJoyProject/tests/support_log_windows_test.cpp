#define NOMINMAX
#include "support_log.h"
#include "input_path_diagnostics.h"
#include "backend.h"
#include "settings.h"
#include "keyboard_support_status.h"
#include "engine_runtime_owner.h"
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static std::atomic<bool> enabled{false}, missing{false};
const std::wstring& AppPaths_DataRoot() { static const std::wstring empty; return empty; }
const std::wstring& AppPaths_LegacyDataRoot() { static const std::wstring empty; return empty; }
bool Settings_GetDiagnosticLogging() { return enabled; }
void Backend_GetAnalogTelemetry(BackendAnalogTelemetry* out) {
    *out={}; out->pluginHostLastError=123;
    out->pluginDeviceCount=1;
    strcpy_s(out->pluginDevices[0].name, "PRIVATE_DEVICE_NAME_SENTINEL");
    strcpy_s(out->pluginDevices[0].manufacturer, "PRIVATE_MANUFACTURER_SENTINEL");
    out->nativeProtocolCount=1; out->nativeProtocols[0].failedUpdates=7;
}
namespace halljoy::keyboard_support {
StatusSnapshot GetStatusSnapshot() noexcept { return {true,!missing.load()}; }
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
static void Check(bool value) { if(!value) throw std::runtime_error("support log regression"); }
static std::string Read(const std::wstring& path) {
    std::ifstream input{std::filesystem::path(path)};
    return {std::istreambuf_iterator<char>(input),{}};
}
static void Await(const std::wstring& path, const char* text) {
    auto end=GetTickCount64()+7000;
    do { if(Read(path).find(text)!=std::string::npos) return; Sleep(50); } while(GetTickCount64()<end);
    Check(false);
}
int main() {
    wchar_t temp[MAX_PATH]{}, unique[MAX_PATH]{};
    GetTempPathW(MAX_PATH,temp); Check(GetTempFileNameW(temp,L"hjl",0,unique)!=0);
    Check(DeleteFileW(unique)); Check(CreateDirectoryW(unique,nullptr));
    const std::wstring directory=unique, path=directory+L"\\HallJoy.log";
    const auto mirrorDir = directory+L"\\mirror", mirrorPath=mirrorDir+L"\\HallJoy.log";
    try {
        Check(SupportLog_Start(directory.c_str(), mirrorDir.c_str()));
        SupportLog_Event("before.failure",42,5);
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
        // Even a banner that disappears before the writer tick must be captured.
        SupportLog_ReportMissingSource();
        Await(path,"support.banner_shown incident_latched=1");
        missing=true;
        Await(path,"support.banner value=1");
        Await(path,"before.failure value=42 error=5");
        Await(path,"failures=7");
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
        enabled=true;
        for (unsigned i=0;i<100000;++i) SupportLog_Event("test.burst",i);
        Await(path,"logging.queue_dropped");
        Check(std::filesystem::file_size(path)<=4*1024*1024);
        Check(SupportLog_Stop());
        Check(GetFileAttributesW((path+L".tmp").c_str())==INVALID_FILE_ATTRIBUTES);
        Check(std::filesystem::file_size(path)<=4*1024*1024);
        Check(Read(path)==Read(mirrorPath));
        DeleteFileW(mirrorPath.c_str()); RemoveDirectoryW(mirrorDir.c_str());
        // Real failed destination must report failure, not claim logging worked.
        enabled=true;
        Check(SupportLog_Start((directory+L"\\absent\\child").c_str()));
        Sleep(1300); Check(SupportLog_LastError()!=0);
        Check(CreateDirectoryW((directory+L"\\absent").c_str(),nullptr));
        const auto recovered = directory+L"\\absent\\child\\HallJoy.log";
        Await(recovered,"support report schema=1");
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
        Sleep(200); Check(SupportLog_LastError()!=0);
        CloseHandle(lock);
        Await(blockedLog,"mirror.locked value=1");
        Check(SupportLog_LastError()==0);
        Check(SupportLog_Stop());
        DeleteFileW(blockedLog.c_str()); RemoveDirectoryW(blocked.c_str());
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
        std::cout<<"SUPPORT_LOG_WINDOWS=PASS off_no_file auto_incident prehistory recovery opt_in io_failure\n";
#endif
        return 0;
    } catch(...) {
        SupportLog_Stop(); DeleteFileW(path.c_str()); DeleteFileW((path+L".tmp").c_str()); RemoveDirectoryW(directory.c_str());
        throw;
    }
}
