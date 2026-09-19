#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "stability_trace.h"
int main(){
    wchar_t module[32768]{};assert(GetModuleFileNameW(nullptr,module,_countof(module)));
    auto path=std::filesystem::path(module).parent_path()/L"HallJoy.log";
    auto read=[&](){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    StabilityTrace_Init();assert(StabilityTrace_IsEnabled());assert(std::filesystem::path(StabilityTrace_Path())==path);
    StabilityTrace_Write(L"INFO",L"mini60",L"test",L"keys=8 profile=PRIVATE_PROFILE_MARKER private tail");
    StabilityTrace_AppendPlain(L"[mini60] key index=7 samples=20 travel_min=3 travel_max=270");
    StabilityTrace_AppendPlain(L"[mini60] path=C:\\PRIVATE_PATH_MARKER\\private text");
    auto active=read();assert(!active.empty() && active.find('\0')==std::string::npos);
    assert(active.find("PRIVATE_PROFILE_MARKER")==std::string::npos);
    assert(active.find("PRIVATE_PATH_MARKER")==std::string::npos);
    assert(active.find("travel_max=270")!=std::string::npos);
    StabilityTrace_Shutdown(0);auto final=read();assert(final.find("session.end")!=std::string::npos && final.find('\0')==std::string::npos);
    HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
    StabilityTrace_Init();assert(!StabilityTrace_IsEnabled());StabilityTrace_Shutdown(0);CloseHandle(lock);
    assert(read()==final); // A denied log must neither advertise success nor damage prior evidence.
    return 0;
}
