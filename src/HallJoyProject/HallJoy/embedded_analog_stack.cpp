#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "embedded_analog_stack.h"
#include "Resource.h"
#include "debug_log.h"

namespace
{
constexpr wchar_t kLegacyInstallArgument[] = L"--halljoy-install-embedded-uap";
constexpr wchar_t kPrivatePluginFileName[] = L"HallJoyUniversalAnalogHost.dll";

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

    const std::wstring temporary = destination + L".halljoy-new";
    DeleteFileW(temporary.c_str());
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    const bool written = WriteAll(file, static_cast<const std::uint8_t*>(resourceBytes), resourceSize);
    if (written)
        FlushFileBuffers(file);
    CloseHandle(file);
    if (!written)
    {
        DeleteFileW(temporary.c_str());
        return false;
    }

    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
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

bool EnsurePrivatePlugin(HINSTANCE hInst)
{
    const std::wstring pluginPath = BuildPathNearExe(kPrivatePluginFileName);
    if (!ResourceEqualsFile(hInst, IDR_UAP_ABIV1, pluginPath))
    {
        DebugLog_Write(L"[embedded.uap] extracting private ABI1 host plugin path=%s", pluginPath.c_str());
        if (!ExtractResourceAtomic(hInst, IDR_UAP_ABIV1, pluginPath))
        {
            DebugLog_Write(L"[embedded.uap] private extraction failed err=%lu", GetLastError());
            return false;
        }
    }

    const void* bytes = nullptr;
    DWORD size = 0;
    const bool resourceOk = GetResourceBytes(hInst, IDR_UAP_ABIV1, bytes, size);
    const bool match = resourceOk && ResourceEqualsFile(hInst, IDR_UAP_ABIV1, pluginPath);
    DebugLog_Write(L"[embedded.uap] private_abi=1 resource_bytes=%lu local_match=%d sdk_layer=bypassed",
        resourceOk ? size : 0, match ? 1 : 0);
    return match;
}
} // namespace

const wchar_t* EmbeddedAnalogStack_PrivatePluginFileName()
{
    return kPrivatePluginFileName;
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
        MessageBoxW(nullptr,
            L"HallJoy could not extract its private crash-isolated Universal Analog Plugin next to the executable.",
            L"HallJoy diagnostic", MB_OK | MB_ICONERROR);
        return false;
    }

    DebugLog_Write(L"[embedded.stack] mode=direct_private_abi1 wooting_sdk_loaded=0 system_plugin_modified=0 uac_required=0");
    return true;
}
