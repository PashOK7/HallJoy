#define NOMINMAX
#include "support_log.h"
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
bool Settings_GetDiagnosticLogging() { return enabled; }
void Backend_GetAnalogTelemetry(BackendAnalogTelemetry* out) {
    *out={}; out->pluginHostLastError=123;
    out->nativeProtocolCount=1; out->nativeProtocols[0].failedUpdates=7;
}
namespace halljoy::keyboard_support {
StatusSnapshot GetStatusSnapshot() noexcept { return {true,!missing.load()}; }
}
namespace halljoy::engine_runtime {
runtime_command::SnapshotV1 EngineRuntimeOwner_Snapshot() noexcept { return {}; }
}
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
    try {
        Check(SupportLog_Start(directory.c_str()));
        SupportLog_Event("before.failure",42,5);
        Sleep(1300);
        Check(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);
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
        enabled=true;
        Await(path,"logging.continuous value=1");
        enabled=false;
        Await(path,"logging.continuous value=0");
        SupportLog_ReportFailure("runtime.failure",87);
        Await(path,"runtime.failure value=0 error=87");
        Check(SupportLog_Stop());
        Check(GetFileAttributesW((path+L".tmp").c_str())==INVALID_FILE_ATTRIBUTES);
        Check(std::filesystem::file_size(path)<=4*1024*1024);
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
        DeleteFileW(path.c_str()); RemoveDirectoryW(directory.c_str());
        std::cout<<"SUPPORT_LOG_WINDOWS=PASS off_no_file auto_incident prehistory recovery opt_in io_failure\n";
        return 0;
    } catch(...) {
        SupportLog_Stop(); DeleteFileW(path.c_str()); DeleteFileW((path+L".tmp").c_str()); RemoveDirectoryW(directory.c_str());
        throw;
    }
}
