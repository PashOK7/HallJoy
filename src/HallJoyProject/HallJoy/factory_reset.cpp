#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "factory_reset.h"

#include "app_paths.h"
#include "ini_util.h"
#include "stability_trace.h"

#include <array>
#include <cwchar>
#include <string>
#include <vector>

namespace
{
    constexpr const wchar_t* kRequestLeaf = L".factory-reset-request.ini";
    constexpr const wchar_t* kBackupDirectoryLeaf = L"FactoryResetBackups";

    struct ResetTarget
    {
        const wchar_t* leaf;
        bool directory;
    };

    constexpr std::array<ResetTarget, 5> kResetTargets{ {
        { L"settings.ini", false },
        { L"bindings.ini", false },
        { L"GlobalProfiles", true },
        { L"Layouts", true },
        { L"CurvePresets", true },
    } };

    std::wstring JoinPath(const std::wstring& root, const wchar_t* leaf)
    {
        if (root.empty()) return {};
        std::wstring path = root;
        if (path.back() != L'\\' && path.back() != L'/') path += L'\\';
        path += leaf;
        return path;
    }

    bool IsPlainDirectory(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    bool EnsurePlainDirectory(const std::wstring& path, DWORD* errorOut)
    {
        DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES)
        {
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                return true;
            if (errorOut) *errorOut = ERROR_DIRECTORY;
            return false;
        }

        if (!CreateDirectoryW(path.c_str(), nullptr))
        {
            if (errorOut) *errorOut = GetLastError();
            return false;
        }
        if (!IsPlainDirectory(path))
        {
            if (errorOut) *errorOut = ERROR_INVALID_DATA;
            return false;
        }
        return true;
    }

    bool IsSafeBackupLeaf(const wchar_t* leaf)
    {
        if (!leaf || wcsncmp(leaf, L"reset-", 6) != 0) return false;
        const size_t length = wcslen(leaf);
        if (length < 12 || length > 96) return false;
        for (size_t i = 0; i < length; ++i)
        {
            const wchar_t ch = leaf[i];
            if (!((ch >= L'a' && ch <= L'z') ||
                  (ch >= L'A' && ch <= L'Z') ||
                  (ch >= L'0' && ch <= L'9') || ch == L'-'))
                return false;
        }
        return true;
    }

    std::wstring BuildBackupLeaf()
    {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t leaf[96]{};
        swprintf_s(
            leaf,
            L"reset-%04u%02u%02u-%02u%02u%02u-%08lX-%llu",
            static_cast<unsigned>(now.wYear),
            static_cast<unsigned>(now.wMonth),
            static_cast<unsigned>(now.wDay),
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond),
            static_cast<unsigned long>(GetCurrentProcessId()),
            static_cast<unsigned long long>(GetTickCount64()));
        return leaf;
    }

    struct RequestContext
    {
        const wchar_t* backupLeaf = nullptr;
    };

    bool RequestWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<RequestContext*>(rawContext);
        bool ok = WritePrivateProfileStringW(
            L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(
            L"HallJoyPersistence", L"Kind", L"FactoryResetRequest", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(
            L"FactoryReset", L"Requested", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(
            L"FactoryReset", L"BackupLeaf", context->backupLeaf, temporaryPath) != FALSE;
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool ReadMarker(const wchar_t* path, wchar_t* backupLeaf, DWORD backupLeafCount)
    {
        wchar_t schema[16]{};
        wchar_t kind[64]{};
        const int requested = GetPrivateProfileIntW(L"FactoryReset", L"Requested", 0, path);
        GetPrivateProfileStringW(
            L"HallJoyPersistence", L"SchemaVersion", L"", schema, _countof(schema), path);
        GetPrivateProfileStringW(
            L"HallJoyPersistence", L"Kind", L"", kind, _countof(kind), path);
        GetPrivateProfileStringW(
            L"FactoryReset", L"BackupLeaf", L"", backupLeaf, backupLeafCount, path);
        return wcscmp(schema, L"1") == 0 &&
            wcscmp(kind, L"FactoryResetRequest") == 0 &&
            requested == 1 && IsSafeBackupLeaf(backupLeaf);
    }

    bool RequestValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<RequestContext*>(rawContext);
        wchar_t backupLeaf[128]{};
        const bool ok = ReadMarker(temporaryPath, backupLeaf, _countof(backupLeaf)) &&
            wcscmp(backupLeaf, context->backupLeaf) == 0;
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }

    void SetFirstError(DWORD* firstError, DWORD error)
    {
        if (firstError && *firstError == ERROR_SUCCESS)
            *firstError = error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE;
    }

    bool RollBackMoves(
        const std::vector<size_t>& moved,
        const std::wstring& root,
        const std::wstring& backupRoot,
        DWORD* rollbackError)
    {
        bool complete = true;
        for (auto iterator = moved.rbegin(); iterator != moved.rend(); ++iterator)
        {
            const auto& target = kResetTargets[*iterator];
            const std::wstring source = JoinPath(backupRoot, target.leaf);
            const std::wstring destination = JoinPath(root, target.leaf);
            if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
            {
                complete = false;
                SetFirstError(rollbackError, GetLastError());
            }
        }
        return complete;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    bool ShouldInjectRollbackTestFailure(size_t movedCount)
    {
        return movedCount == 3 &&
            wcsstr(GetCommandLineW(), L"--halljoy-test-factory-reset-fail-after-three-moves") != nullptr;
    }
#endif
}

bool FactoryReset_Request(DWORD* errorOut)
{
    if (errorOut) *errorOut = ERROR_SUCCESS;
    const std::wstring& root = AppPaths_DataRoot();
    if (root.empty())
    {
        if (errorOut) *errorOut = ERROR_PATH_NOT_FOUND;
        return false;
    }

    const std::wstring requestPath = JoinPath(root, kRequestLeaf);
    const std::wstring backupLeaf = BuildBackupLeaf();
    RequestContext context{ backupLeaf.c_str() };
    const auto result = IniUtil_SaveAtomic(
        requestPath.c_str(), RequestWrite, RequestValidate, &context);
    if (!result.Succeeded())
    {
        if (errorOut) *errorOut = result.nativeError;
        StabilityTrace_WriteCritical(L"ERROR", L"factory-reset", L"request.failed",
            L"error=%lu path=%ls", static_cast<unsigned long>(result.nativeError), requestPath.c_str());
        return false;
    }

    StabilityTrace_Write(L"INFO", L"factory-reset", L"request.saved",
        L"backup_leaf=%ls", backupLeaf.c_str());
    return true;
}

FactoryResetApplyResult FactoryReset_ApplyPending()
{
    FactoryResetApplyResult result{};
    const std::wstring& root = AppPaths_DataRoot();
    const std::wstring requestPath = JoinPath(root, kRequestLeaf);
    const DWORD markerAttributes = GetFileAttributesW(requestPath.c_str());
    if (markerAttributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return result;
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = error;
        return result;
    }
    if ((markerAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
    {
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = ERROR_INVALID_DATA;
        return result;
    }

    wchar_t backupLeaf[128]{};
    if (!ReadMarker(requestPath.c_str(), backupLeaf, _countof(backupLeaf)))
    {
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = ERROR_INVALID_DATA;
        StabilityTrace_WriteCritical(L"ERROR", L"factory-reset", L"marker.invalid",
            L"path=%ls", requestPath.c_str());
        return result;
    }

    DWORD firstError = ERROR_SUCCESS;
    const std::wstring backupParent = JoinPath(root, kBackupDirectoryLeaf);
    if (!EnsurePlainDirectory(backupParent, &firstError))
    {
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = firstError;
        return result;
    }

    const std::wstring backupRoot = JoinPath(backupParent, backupLeaf);
    if (GetFileAttributesW(backupRoot.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = ERROR_ALREADY_EXISTS;
        return result;
    }
    if (!CreateDirectoryW(backupRoot.c_str(), nullptr))
    {
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = GetLastError();
        return result;
    }
    if (!IsPlainDirectory(backupRoot))
    {
        RemoveDirectoryW(backupRoot.c_str());
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = ERROR_INVALID_DATA;
        return result;
    }

    std::vector<size_t> moved;
    for (size_t index = 0; index < kResetTargets.size(); ++index)
    {
        const auto& target = kResetTargets[index];
        const std::wstring source = JoinPath(root, target.leaf);
        const DWORD attributes = GetFileAttributesW(source.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                continue;
            SetFirstError(&firstError, error);
            break;
        }
        const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory != target.directory || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            SetFirstError(&firstError, ERROR_INVALID_DATA);
            break;
        }

        const std::wstring destination = JoinPath(backupRoot, target.leaf);
        if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        {
            SetFirstError(&firstError, GetLastError());
            break;
        }
        moved.push_back(index);
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (ShouldInjectRollbackTestFailure(moved.size()))
        {
            SetFirstError(&firstError, ERROR_WRITE_FAULT);
            break;
        }
#endif
    }

    std::vector<std::wstring> createdDirectories;
    if (firstError == ERROR_SUCCESS)
    {
        for (const auto& target : kResetTargets)
        {
            if (!target.directory) continue;
            const std::wstring directory = JoinPath(root, target.leaf);
            if (!EnsurePlainDirectory(directory, &firstError)) break;
            createdDirectories.push_back(directory);
        }
    }

    if (firstError == ERROR_SUCCESS && !DeleteFileW(requestPath.c_str()))
        SetFirstError(&firstError, GetLastError());

    if (firstError != ERROR_SUCCESS)
    {
        DWORD rollbackError = ERROR_SUCCESS;
        bool rollbackComplete = true;
        for (auto iterator = createdDirectories.rbegin(); iterator != createdDirectories.rend(); ++iterator)
        {
            if (!RemoveDirectoryW(iterator->c_str()))
            {
                const DWORD error = GetLastError();
                if (error != ERROR_PATH_NOT_FOUND)
                {
                    rollbackComplete = false;
                    SetFirstError(&rollbackError, error);
                }
            }
        }
        rollbackComplete = RollBackMoves(moved, root, backupRoot, &rollbackError) && rollbackComplete;
        if (!RemoveDirectoryW(backupRoot.c_str()))
        {
            const DWORD error = GetLastError();
            if (error != ERROR_PATH_NOT_FOUND && error != ERROR_DIR_NOT_EMPTY)
            {
                rollbackComplete = false;
                SetFirstError(&rollbackError, error);
            }
            if (error == ERROR_DIR_NOT_EMPTY)
            {
                rollbackComplete = false;
                SetFirstError(&rollbackError, error);
            }
        }
        result.status = FactoryResetApplyStatus::Failed;
        result.nativeError = rollbackComplete ? firstError : rollbackError;
        result.rollbackComplete = rollbackComplete;
        result.backupRoot = backupRoot;
        StabilityTrace_WriteCritical(L"ERROR", L"factory-reset", L"apply.rollback",
            L"apply_error=%lu rollback_complete=%d rollback_error=%lu moved=%zu backup=%ls",
            static_cast<unsigned long>(firstError), rollbackComplete ? 1 : 0,
            static_cast<unsigned long>(rollbackError), moved.size(), backupRoot.c_str());
        return result;
    }

    result.status = FactoryResetApplyStatus::Applied;
    result.backupRoot = backupRoot;
    StabilityTrace_Write(L"INFO", L"factory-reset", L"apply.commit",
        L"moved=%zu backup=%ls", moved.size(), backupRoot.c_str());
    return result;
}
