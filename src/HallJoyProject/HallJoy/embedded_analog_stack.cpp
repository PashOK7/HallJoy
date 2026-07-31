#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "embedded_analog_stack.h"
#include "Resource.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "version.h"

namespace
{
constexpr wchar_t kLegacyInstallArgument[] = L"--halljoy-install-embedded-uap";
constexpr wchar_t kForceUserRuntimeArgument[] = L"--halljoy-test-uap-exe-write-denied";
constexpr wchar_t kPrivatePluginFileName[] = L"HallJoyUniversalAnalogHost.dll";

std::wstring g_privatePluginPath;
EmbeddedAnalogRuntimeLocation g_runtimeLocation = EmbeddedAnalogRuntimeLocation::None;
DWORD g_lastError = ERROR_SUCCESS;

std::wstring BuildPathNearExe(const wchar_t* fileName)
{
    std::vector<wchar_t> path(1024);
    for (;;)
    {
        DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0 || n >= 65535)
            return fileName ? fileName : L"";
        if (n < path.size())
        {
            std::wstring result(path.data(), n);
            const size_t slash = result.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                result.erase(slash + 1);
            else
                result.clear();
            if (fileName)
                result += fileName;
            return result;
        }
        path.resize(path.size() * 2);
    }
}

bool GetResourceBytes(HINSTANCE hInst, int resourceId, const void*& bytes, DWORD& size)
{
    bytes = nullptr;
    size = 0;
    HRSRC resource = FindResourceW(hInst, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource)
        return false;
    size = SizeofResource(hInst, resource);
    if (size == 0)
        return false;
    HGLOBAL data = LoadResource(hInst, resource);
    if (!data)
        return false;
    bytes = LockResource(data);
    return bytes != nullptr;
}

bool HasExactArgument(const wchar_t* expected)
{
    if (!expected || !*expected)
        return false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return false;
    bool found = false;
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsicmp(argv[i], expected) == 0)
        {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

std::wstring BuildPerUserRuntimePath()
{
    wchar_t localAppData[MAX_PATH]{};
    const HRESULT result = SHGetFolderPathW(nullptr,
        CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, localAppData);
    if (FAILED(result) || !localAppData[0])
    {
        SetLastError(HRESULT_CODE(result));
        return {};
    }

    std::wstring directory = localAppData;
    directory += L"\\HallJoy\\Runtime\\v" HALLJOY_VERSION_WSTRING;
    const int createResult = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    if (createResult != ERROR_SUCCESS && createResult != ERROR_ALREADY_EXISTS &&
        createResult != ERROR_FILE_EXISTS)
    {
        SetLastError(static_cast<DWORD>(createResult));
        return {};
    }
    return directory + L"\\" + kPrivatePluginFileName;
}

bool WriteAll(HANDLE file, const std::uint8_t* bytes, DWORD size)
{
    DWORD total = 0;
    while (total < size)
    {
        DWORD written = 0;
        const DWORD chunk = std::min<DWORD>(size - total, 1024u * 1024u);
        if (!WriteFile(file, bytes + total, chunk, &written, nullptr) || written == 0)
            return false;
        total += written;
    }
    return true;
}

bool ExtractResourceAtomic(HINSTANCE hInst, int resourceId, const std::wstring& destination)
{
    const void* resourceBytes = nullptr;
    DWORD resourceSize = 0;
    if (!GetResourceBytes(hInst, resourceId, resourceBytes, resourceSize))
        return false;

    wchar_t suffix[80]{};
    _snwprintf_s(suffix, _countof(suffix), _TRUNCATE, L".halljoy-new-%lu-%lu",
        GetCurrentProcessId(), GetCurrentThreadId());
    const std::wstring temporary = destination + suffix;
    DeleteFileW(temporary.c_str());
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    bool complete = WriteAll(file, static_cast<const std::uint8_t*>(resourceBytes), resourceSize);
    if (complete)
        complete = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file))
        complete = false;
    if (!complete)
    {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        SetLastError(error ? error : ERROR_WRITE_FAULT);
        return false;
    }

    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        SetLastError(error);
        return false;
    }
    return true;
}

bool ResourceEqualsFile(HINSTANCE hInst, int resourceId, const std::wstring& path)
{
    const void* resourceBytes = nullptr;
    DWORD resourceSize = 0;
    if (!GetResourceBytes(hInst, resourceId, resourceBytes, resourceSize))
        return false;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart != resourceSize)
    {
        CloseHandle(file);
        return false;
    }

    std::vector<std::uint8_t> buffer(64 * 1024);
    const auto* expected = static_cast<const std::uint8_t*>(resourceBytes);
    DWORD offset = 0;
    bool equal = true;
    while (offset < resourceSize)
    {
        const DWORD wanted = std::min<DWORD>(resourceSize - offset, static_cast<DWORD>(buffer.size()));
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), wanted, &read, nullptr) || read != wanted ||
            std::memcmp(buffer.data(), expected + offset, wanted) != 0)
        {
            equal = false;
            break;
        }
        offset += read;
    }
    CloseHandle(file);
    return equal;
}

bool EnsurePrivatePluginAt(HINSTANCE hInst, const std::wstring& pluginPath,
    EmbeddedAnalogRuntimeLocation location)
{
    if (!ResourceEqualsFile(hInst, IDR_UAP_ABIV1, pluginPath))
    {
        DebugLog_Write(L"[embedded.uap] extracting private ABI1 host plugin path=%s", pluginPath.c_str());
        if (!ExtractResourceAtomic(hInst, IDR_UAP_ABIV1, pluginPath))
        {
            DebugLog_Write(L"[embedded.uap] private extraction failed err=%lu", GetLastError());
            g_lastError = GetLastError();
            return false;
        }
    }

    const void* bytes = nullptr;
    DWORD size = 0;
    const bool resourceOk = GetResourceBytes(hInst, IDR_UAP_ABIV1, bytes, size);
    const bool match = resourceOk && ResourceEqualsFile(hInst, IDR_UAP_ABIV1, pluginPath);
    DebugLog_Write(L"[embedded.uap] private_abi=1 resource_bytes=%lu local_match=%d sdk_layer=bypassed",
        resourceOk ? size : 0, match ? 1 : 0);
    if (!match)
    {
        g_lastError = resourceOk ? ERROR_CRC : GetLastError();
        return false;
    }
    g_privatePluginPath = pluginPath;
    g_runtimeLocation = location;
    g_lastError = ERROR_SUCCESS;
    return match;
}

bool EnsurePrivatePlugin(HINSTANCE hInst)
{
    g_privatePluginPath.clear();
    g_runtimeLocation = EmbeddedAnalogRuntimeLocation::None;
    g_lastError = ERROR_SUCCESS;

    const bool forceUserRuntime =
#if defined(HALLJOY_STABILITY_TRACE)
        HasExactArgument(kForceUserRuntimeArgument);
#else
        false;
#endif
    if (!forceUserRuntime && EnsurePrivatePluginAt(hInst,
        BuildPathNearExe(kPrivatePluginFileName), EmbeddedAnalogRuntimeLocation::BesideExecutable))
        return true;

    const DWORD executableError = forceUserRuntime ? ERROR_ACCESS_DENIED : g_lastError;
    DebugLog_Write(L"[embedded.uap] executable-directory runtime unavailable err=%lu forced=%d; trying per-user runtime",
        executableError, forceUserRuntime ? 1 : 0);

    const std::wstring perUserPath = BuildPerUserRuntimePath();
    if (perUserPath.empty())
    {
        g_lastError = GetLastError();
        return false;
    }
    return EnsurePrivatePluginAt(hInst, perUserPath, EmbeddedAnalogRuntimeLocation::PerUser);
}
} // namespace

const std::wstring& EmbeddedAnalogStack_PrivatePluginPath()
{
    return g_privatePluginPath;
}

EmbeddedAnalogRuntimeLocation EmbeddedAnalogStack_RuntimeLocation()
{
    return g_runtimeLocation;
}

const wchar_t* EmbeddedAnalogStack_RuntimeLocationName()
{
    switch (g_runtimeLocation)
    {
    case EmbeddedAnalogRuntimeLocation::BesideExecutable: return L"executable";
    case EmbeddedAnalogRuntimeLocation::PerUser: return L"user";
    default: return L"none";
    }
}

DWORD EmbeddedAnalogStack_LastError()
{
    return g_lastError;
}

bool EmbeddedAnalogStack_TryRunInstallerCommand(HINSTANCE hInst, int& exitCode)
{
    // Kept only so an old shortcut invoking the former elevated installer does
    // not start the UI. V6 never writes to Program Files and never requests UAC.
    if (!wcsstr(GetCommandLineW(), kLegacyInstallArgument))
        return false;
    exitCode = EnsurePrivatePlugin(hInst) ? ERROR_SUCCESS : ERROR_WRITE_FAULT;
    return true;
}

bool EmbeddedAnalogStack_Prepare(HINSTANCE hInst)
{
    if (!EnsurePrivatePlugin(hInst))
    {
        StabilityTrace_WriteCritical(L"ERROR", L"embedded-uap", L"prepare.failed",
            L"error=%lu system_sdk_required=0", g_lastError);
        return false;
    }

    DebugLog_Write(L"[embedded.stack] mode=direct_private_abi1 location=%s wooting_sdk_loaded=0 system_plugin_modified=0 uac_required=0",
        EmbeddedAnalogStack_RuntimeLocationName());
    StabilityTrace_Write(L"INFO", L"embedded-uap", L"prepare.ok",
        L"location=%s exact_resource_match=1 system_sdk_required=0",
        EmbeddedAnalogStack_RuntimeLocationName());
    return true;
}
