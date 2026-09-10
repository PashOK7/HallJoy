#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <strsafe.h>

#include "instance_guard.h"

namespace halljoy::instance_guard
{
namespace
{
constexpr wchar_t kInstancePrefix[] = L"Global\\HallJoy.InstanceGuard.v1.";

bool BuildCurrentUserName(wchar_t (&name)[256], DWORD& error) noexcept
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        error = GetLastError();
        return false;
    }

    BYTE tokenUserStorage[512]{};
    DWORD required = 0;
    const bool read = GetTokenInformation(token, TokenUser, tokenUserStorage,
        static_cast<DWORD>(sizeof(tokenUserStorage)), &required) != FALSE;
    const DWORD tokenError = read ? ERROR_SUCCESS : GetLastError();
    CloseHandle(token);
    if (!read)
    {
        error = tokenError == ERROR_INSUFFICIENT_BUFFER ? ERROR_BUFFER_OVERFLOW : tokenError;
        return false;
    }

    const auto* tokenUser = reinterpret_cast<const TOKEN_USER*>(tokenUserStorage);
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(tokenUser->User.Sid, &sidText))
    {
        error = GetLastError();
        return false;
    }
    const HRESULT written = StringCchPrintfW(name, sizeof(name) / sizeof(name[0]), L"%s%s",
        kInstancePrefix, sidText);
    LocalFree(sidText);
    if (FAILED(written))
    {
        error = ERROR_BUFFER_OVERFLOW;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}
} // namespace

Guard::~Guard() noexcept
{
    Release();
}

AcquireResult Guard::AcquireNamed(const wchar_t* name) noexcept
{
    Release();
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, name);
    if (!mutex)
    {
        lastError_ = GetLastError();
        return AcquireResult::Failed;
    }
    const DWORD createError = GetLastError();
    if (createError == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        lastError_ = ERROR_ALREADY_EXISTS;
        return AcquireResult::Conflicted;
    }
    mutex_ = mutex;
    lastError_ = ERROR_SUCCESS;
    return AcquireResult::Acquired;
}

AcquireResult Guard::AcquireForCurrentUser() noexcept
{
    wchar_t name[256]{};
    DWORD error = ERROR_SUCCESS;
    if (!BuildCurrentUserName(name, error))
    {
        lastError_ = error;
        return AcquireResult::Failed;
    }
    return AcquireNamed(name);
}

void Guard::Release() noexcept
{
    if (mutex_)
    {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

} // namespace halljoy::instance_guard
