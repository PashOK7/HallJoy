#pragma once
#include <windows.h>

namespace halljoy::debug_event_handles {
// Debug-event process/thread handles are borrowed from Windows. Continuing
// their exit events closes them automatically. Only the file handles belong
// to the debugger. PROCESS_INFORMATION handles have separate ownership.
// https://learn.microsoft.com/windows/win32/api/debugapi/nf-debugapi-continuedebugevent
inline void ReleaseOwnedFiles(const DEBUG_EVENT& event) noexcept {
    HANDLE file = nullptr;
    if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        file = event.u.CreateProcessInfo.hFile;
    else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
        file = event.u.LoadDll.hFile;
    if (file && file != INVALID_HANDLE_VALUE) CloseHandle(file);
}
}
