#include <cassert>
#include <cstdint>

using DWORD = std::uint32_t;
using BOOL = int;
using HANDLE = void*;

struct OVERLAPPED
{
    std::uintptr_t Internal = 0;
    std::uintptr_t InternalHigh = 0;
    DWORD Offset = 0;
    DWORD OffsetHigh = 0;
    HANDLE hEvent = nullptr;
};

constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;
constexpr DWORD ERROR_SUCCESS = 0;
constexpr DWORD ERROR_INVALID_FUNCTION = 1;
constexpr DWORD ERROR_INVALID_PARAMETER = 87;
constexpr DWORD ERROR_IO_INCOMPLETE = 996;
constexpr DWORD ERROR_IO_PENDING = 997;
constexpr DWORD ERROR_OPERATION_ABORTED = 995;
constexpr DWORD ERROR_NOT_FOUND = 1168;
constexpr DWORD WAIT_OBJECT_0 = 0;
constexpr DWORD WAIT_TIMEOUT = 258;
constexpr DWORD WAIT_FAILED = 0xFFFFFFFFu;
#define INVALID_HANDLE_VALUE ((HANDLE)(std::intptr_t)-1)

namespace fake_winapi
{
    int eventStorage = 0;
    int deviceStorage = 0;
    DWORD error = ERROR_SUCCESS;
    bool pending = false;
    bool cancelled = false;
    bool completed = false;
    int cancelCalls = 0;
    int blockingDrainCalls = 0;

    void Reset()
    {
        error = ERROR_SUCCESS;
        pending = false;
        cancelled = false;
        completed = false;
        cancelCalls = 0;
        blockingDrainCalls = 0;
    }
}

DWORD GetLastError() { return fake_winapi::error; }
HANDLE CreateEventW(void*, BOOL, BOOL, const wchar_t*) { return &fake_winapi::eventStorage; }
BOOL CloseHandle(HANDLE) { return TRUE; }
BOOL ResetEvent(HANDLE) { return TRUE; }
DWORD WaitForSingleObject(HANDLE, DWORD) { return fake_winapi::pending ? WAIT_TIMEOUT : WAIT_OBJECT_0; }
BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, OVERLAPPED*)
{
    fake_winapi::pending = true;
    fake_winapi::error = ERROR_IO_PENDING;
    return FALSE;
}
BOOL WriteFile(HANDLE, void*, DWORD, DWORD*, OVERLAPPED*)
{
    fake_winapi::pending = true;
    fake_winapi::error = ERROR_IO_PENDING;
    return FALSE;
}
BOOL CancelIoEx(HANDLE, OVERLAPPED*)
{
    ++fake_winapi::cancelCalls;
    fake_winapi::cancelled = true;
    return TRUE;
}
BOOL GetOverlappedResult(HANDLE, OVERLAPPED*, DWORD* bytes, BOOL wait)
{
    if (wait)
        ++fake_winapi::blockingDrainCalls;
    if (fake_winapi::cancelled)
    {
        fake_winapi::pending = false;
        fake_winapi::error = ERROR_OPERATION_ABORTED;
        if (bytes) *bytes = 0;
        return FALSE;
    }
    if (fake_winapi::completed)
    {
        fake_winapi::pending = false;
        if (bytes) *bytes = 64;
        return TRUE;
    }
    fake_winapi::error = ERROR_IO_INCOMPLETE;
    return FALSE;
}

#include "../HallJoy/hid_io_operation.h"

int main()
{
    std::uint8_t data[64]{};

    fake_winapi::Reset();
    {
        HidIoOperation io(&fake_winapi::deviceStorage);
        DWORD error = ERROR_SUCCESS;
        assert(io.StartRead(data, 64, &error) == HidIoOperation::StartResult::Pending);
        assert(io.Wait(1) == WAIT_TIMEOUT);
        assert(io.CancelAndDrain(nullptr, &error));
        assert(error == ERROR_OPERATION_ABORTED);
        assert(fake_winapi::cancelCalls == 1);
        assert(fake_winapi::blockingDrainCalls == 1);
    }
    assert(fake_winapi::cancelCalls == 1); // no second cancellation in destructor

    fake_winapi::Reset();
    {
        HidIoOperation io(&fake_winapi::deviceStorage);
        DWORD error = ERROR_SUCCESS;
        DWORD bytes = 0;
        assert(io.StartWrite(data, 64, &error) == HidIoOperation::StartResult::Pending);
        fake_winapi::pending = false;
        fake_winapi::completed = true;
        assert(io.Finish(&bytes, &error, false));
        assert(bytes == 64);
        assert(error == ERROR_SUCCESS);
    }
    assert(fake_winapi::cancelCalls == 0);

    fake_winapi::Reset();
    {
        HidIoOperation io(&fake_winapi::deviceStorage);
        DWORD error = ERROR_SUCCESS;
        DWORD bytes = 0;
        assert(io.StartRead(data, 64, &error) == HidIoOperation::StartResult::Pending);
        assert(!io.Finish(&bytes, &error, false));
        assert(error == ERROR_IO_INCOMPLETE);
        // The object must still own the request; its destructor cancels and drains it.
    }
    assert(fake_winapi::cancelCalls == 1);
    assert(fake_winapi::blockingDrainCalls == 1);

    for (int iteration = 0; iteration < 100000; ++iteration)
    {
        fake_winapi::Reset();
        HidIoOperation io(&fake_winapi::deviceStorage);
        DWORD error = ERROR_SUCCESS;
        assert(io.StartRead(data, 64, &error) == HidIoOperation::StartResult::Pending);
        assert(io.CancelAndDrain(nullptr, &error));
        assert(error == ERROR_OPERATION_ABORTED);
        assert(fake_winapi::cancelCalls == 1);
        assert(fake_winapi::blockingDrainCalls == 1);
    }

    return 0;
}
