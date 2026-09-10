#include <cassert>
#include <string>
#include <iostream>

#include "instance_guard.h"

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--child")
    {
        halljoy::instance_guard::Guard child;
        return child.AcquireForCurrentUser() ==
            halljoy::instance_guard::AcquireResult::Conflicted ? 0 : 17;
    }

    halljoy::instance_guard::Guard first;
    halljoy::instance_guard::Guard second;

    // This integration test deliberately uses the real per-user guard. An
    // already-running HallJoy is a precondition failure, not a broken mutex.
    if (first.AcquireForCurrentUser() != halljoy::instance_guard::AcquireResult::Acquired) {
        std::cerr << "INSTANCE_GUARD_TEST_BLOCKED: close HallJoy before running; error="
            << first.LastError() << '\n';
        return 2;
    }

    wchar_t executable[MAX_PATH]{};
    assert(GetModuleFileNameW(nullptr, executable,
        static_cast<DWORD>(sizeof(executable) / sizeof(executable[0]))) != 0);
    std::wstring command = L"\"";
    command += executable;
    command += L"\" --child";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    assert(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &startup, &child) != FALSE);
    assert(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0);
    DWORD childExit = 99;
    assert(GetExitCodeProcess(child.hProcess, &childExit) != FALSE);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    assert(childExit == 0);

    first.Release();
    assert(second.AcquireForCurrentUser() ==
        halljoy::instance_guard::AcquireResult::Acquired);
    return 0;
}
