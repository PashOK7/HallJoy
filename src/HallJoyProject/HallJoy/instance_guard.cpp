#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <strsafe.h>
#if defined(HALLJOY_ANALOG_SIMULATOR)
#include <shellapi.h>
#include <string>
#include <cwctype>
#endif

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
#if defined(HALLJOY_ANALOG_SIMULATOR)
    // File-only regression runs own isolated data, never devices. They must not
    // claim the interactive user's runtime or require closing the normal app.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool forbiddenBackend = false, fileOnly = false;
    std::wstring root;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--halljoy-test-forbid-backend-init") == 0) forbiddenBackend = true;
            if (wcscmp(argv[i], L"--halljoy-test-profile-transactions") == 0 ||
                wcscmp(argv[i], L"--halljoy-test-profile-startup-only") == 0) fileOnly = true;
            if (wcscmp(argv[i], L"--halljoy-test-data-root") == 0 && i + 1 < argc) root = argv[++i];
        }
        LocalFree(argv);
    }
    if (forbiddenBackend && fileOnly && !root.empty()) {
        wchar_t full[32768]{};
        const DWORD length = GetFullPathNameW(root.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
        if (!length || length >= std::size(full)) { lastError_ = ERROR_INVALID_NAME; return AcquireResult::Failed; }
        unsigned long long hash = 1469598103934665603ull;
        for (DWORD i = 0; i < length; ++i) { hash ^= static_cast<unsigned>(towlower(full[i])); hash *= 1099511628211ull; }
        wchar_t suffix[64]{};
        StringCchPrintfW(suffix, std::size(suffix), L".FileOnly.%016llx", hash);
        if (FAILED(StringCchCatW(name, std::size(name), suffix))) { lastError_ = ERROR_BUFFER_OVERFLOW; return AcquireResult::Failed; }
    }
#endif
#if defined(HALLJOY_INSTANCE_GUARD_TEST)
    // Only the isolated regression executable defines this macro.
    wchar_t run[32]{};
    const DWORD length=GetEnvironmentVariableW(L"HALLJOY_INSTANCE_GUARD_TEST_RUN",run,32);
    if (!length || length>=32) { lastError_=ERROR_INVALID_PARAMETER; return AcquireResult::Failed; }
    if (FAILED(StringCchCatW(name,256,L".Regression.")) || FAILED(StringCchCatW(name,256,run))) {
        lastError_=ERROR_BUFFER_OVERFLOW; return AcquireResult::Failed;
    }
#endif
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
