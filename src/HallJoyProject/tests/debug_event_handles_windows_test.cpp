#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include "debug_event_handles.h"

static void Check(bool ok, const char* reason) {
    if (!ok) throw std::runtime_error(reason);
}
static DWORD WINAPI Work(void*) { Sleep(2); return 0; }
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--child") {
        for (unsigned i=0;i<32;++i) {
            HANDLE thread=CreateThread(nullptr,0,Work,nullptr,0,nullptr);
            if (!thread) return 4;
            WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
        }
        return 0;
    }
    PROCESS_INFORMATION process{};
    try {
        wchar_t exe[32768]{};Check(GetModuleFileNameW(nullptr,exe,32768)!=0,"image path");
        std::wstring command=L"\""+std::wstring(exe)+L"\" --child";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);
        Check(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,
            CREATE_NO_WINDOW|DEBUG_ONLY_THIS_PROCESS,nullptr,nullptr,&startup,&process)!=FALSE,"launch");
        std::vector<HANDLE> sentinels;
        unsigned threads=0;bool exited=false;const auto deadline=GetTickCount64()+15000;
        while(!exited && GetTickCount64()<deadline) {
            DEBUG_EVENT event{};
            if (!WaitForDebugEvent(&event,100)) {
                Check(GetLastError()==ERROR_SEM_TIMEOUT,"debug wait");continue;
            }
            halljoy::debug_event_handles::ReleaseOwnedFiles(event);
            if(event.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT) {
                ++threads;
                // Regression: the old implementation closed this borrowed
                // handle here; GetThreadId then failed before thread exit.
                Check(GetThreadId(event.u.CreateThread.hThread)==event.dwThreadId,"borrowed thread handle closed early");
                HANDLE sentinel=CreateEventW(nullptr,TRUE,FALSE,nullptr);
                Check(sentinel!=nullptr,"sentinel creation");sentinels.push_back(sentinel);
            }
            if(event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT) {
                Check(event.u.ExitProcess.dwExitCode==0,"child exit");exited=true;
            }
            DWORD continuation=DBG_CONTINUE;
            if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT &&
                event.u.Exception.ExceptionRecord.ExceptionCode!=EXCEPTION_BREAKPOINT)
                continuation=DBG_EXCEPTION_NOT_HANDLED;
            Check(ContinueDebugEvent(event.dwProcessId,event.dwThreadId,continuation)!=FALSE,"continue");
            for(HANDLE sentinel:sentinels)
                Check(WaitForSingleObject(sentinel,0)==WAIT_TIMEOUT,"unrelated handle invalidated");
        }
        Check(exited && threads>=32,"thread lifecycle coverage");
        Check(WaitForSingleObject(process.hProcess,2000)==WAIT_OBJECT_0,"owned process handle survives debug exit");
        DWORD code=99;Check(GetExitCodeProcess(process.hProcess,&code)!=FALSE && code==0,"owned exit query");
        for(HANDLE sentinel:sentinels)CloseHandle(sentinel);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        std::printf("DEBUG_EVENT_HANDLES_WINDOWS=PASS threads=%u borrowed_alive=1 sentinels_intact=1 owned_process_intact=1\n",threads);
        return 0;
    }catch(const std::exception& e){
        if(process.hProcess)TerminateProcess(process.hProcess,5);
        std::fprintf(stderr,"DEBUG_EVENT_HANDLES_WINDOWS=FAIL %s error=%lu\n",e.what(),GetLastError());return 1;
    }
}
