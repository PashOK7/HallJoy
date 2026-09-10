#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <softpub.h>
#include <wintrust.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <exception>
#include <iterator>
#include <new>
#include <string>

#include "embedded_vigem_installer.h"
#include "Resource.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wintrust.lib")

namespace
{
    constexpr DWORD kExpectedInstallerSize = 6278576u;
    constexpr std::array<unsigned char, 32> kExpectedInstallerSha256 = {
        0x89, 0x22, 0x0A, 0x78, 0x65, 0x07, 0x6B, 0x34,
        0x28, 0x92, 0xF9, 0x88, 0x65, 0xF3, 0x49, 0x9F,
        0xB7, 0xC4, 0xCF, 0xD6, 0x73, 0x15, 0x9E, 0x89,
        0xD3, 0x52, 0xC3, 0x60, 0xFD, 0x01, 0x4C, 0x6A,
    };
    constexpr DWORD kInstallerWaitMs = 20u * 60u * 1000u;
    constexpr wchar_t kInstallerFileName[] =
        L"ViGEmBus_1.22.0_x64_x86_arm64.exe";

    class UniqueHandle
    {
    public:
        UniqueHandle() noexcept = default;
        explicit UniqueHandle(HANDLE value) noexcept : value_(value) {}
        ~UniqueHandle() noexcept { Reset(); }

        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;

        HANDLE Get() const noexcept { return value_; }
        bool Valid() const noexcept
        {
            return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
        }
        HANDLE Release() noexcept
        {
            HANDLE value = value_;
            value_ = INVALID_HANDLE_VALUE;
            return value;
        }
        void Reset(HANDLE replacement = INVALID_HANDLE_VALUE) noexcept
        {
            if (Valid())
                CloseHandle(value_);
            value_ = replacement;
        }

    private:
        HANDLE value_ = INVALID_HANDLE_VALUE;
    };

    class ScopedInstallerFile
    {
    public:
        ~ScopedInstallerFile() noexcept { Cleanup(false); }

        ScopedInstallerFile(const ScopedInstallerFile&) = delete;
        ScopedInstallerFile& operator=(const ScopedInstallerFile&) = delete;
        ScopedInstallerFile() noexcept = default;

        UniqueHandle file;
        std::wstring directory;
        std::wstring path;

        void Cleanup(bool delayUntilReboot) noexcept
        {
            file.Reset();
            if (!path.empty())
            {
                if (delayUntilReboot)
                    MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                else
                    DeleteFileW(path.c_str());
            }
            if (!directory.empty())
            {
                if (delayUntilReboot)
                    MoveFileExW(directory.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                else
                    RemoveDirectoryW(directory.c_str());
            }
            path.clear();
            directory.clear();
        }
    };

    bool HashBytes(const unsigned char* bytes, DWORD size,
        std::array<unsigned char, 32>& digest) noexcept
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD resultSize = 0;
        unsigned char* object = nullptr;
        bool ok = false;

        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                nullptr, 0) < 0)
            goto cleanup;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                &resultSize, 0) < 0 || objectSize == 0)
            goto cleanup;
        object = new (std::nothrow) unsigned char[objectSize];
        if (!object)
            goto cleanup;
        if (BCryptCreateHash(algorithm, &hash, object, objectSize,
                nullptr, 0, 0) < 0)
            goto cleanup;
        if (BCryptHashData(hash, const_cast<PUCHAR>(bytes), size, 0) < 0)
            goto cleanup;
        if (BCryptFinishHash(hash, digest.data(),
                static_cast<ULONG>(digest.size()), 0) < 0)
            goto cleanup;
        ok = true;

    cleanup:
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        delete[] object;
        return ok;
    }

    bool ReadResource(HINSTANCE hInst, const unsigned char*& bytes,
        DWORD& size, DWORD& nativeError) noexcept
    {
        bytes = nullptr;
        size = 0;
        HRSRC resource = FindResourceW(
            hInst, MAKEINTRESOURCEW(IDR_VIGEMBUS_INSTALLER), RT_RCDATA);
        if (!resource)
        {
            nativeError = GetLastError();
            return false;
        }
        size = SizeofResource(hInst, resource);
        if (size != kExpectedInstallerSize)
        {
            nativeError = ERROR_BAD_LENGTH;
            return false;
        }
        HGLOBAL loaded = LoadResource(hInst, resource);
        if (!loaded)
        {
            nativeError = GetLastError();
            return false;
        }
        bytes = static_cast<const unsigned char*>(LockResource(loaded));
        if (!bytes)
        {
            nativeError = ERROR_RESOURCE_DATA_NOT_FOUND;
            return false;
        }

        std::array<unsigned char, 32> digest{};
        if (!HashBytes(bytes, size, digest) || digest != kExpectedInstallerSha256)
        {
            nativeError = ERROR_CRC;
            return false;
        }
        nativeError = ERROR_SUCCESS;
        return true;
    }

    bool BuildUniqueTempDirectory(std::wstring& directory,
        DWORD& nativeError)
    {
        wchar_t tempPath[MAX_PATH + 1]{};
        const DWORD length = GetTempPathW(
            static_cast<DWORD>(std::size(tempPath)), tempPath);
        if (length == 0 || length >= std::size(tempPath))
        {
            nativeError = length == 0 ? GetLastError() : ERROR_BUFFER_OVERFLOW;
            return false;
        }

        for (unsigned attempt = 0; attempt < 32; ++attempt)
        {
            std::array<unsigned char, 16> random{};
            if (BCryptGenRandom(nullptr, random.data(),
                    static_cast<ULONG>(random.size()),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            {
                nativeError = ERROR_GEN_FAILURE;
                return false;
            }

            constexpr wchar_t kHexDigits[] = L"0123456789ABCDEF";
            directory.assign(tempPath);
            directory.append(L"HallJoy-ViGEm-");
            directory.reserve(directory.size() + random.size() * 2u);
            for (const unsigned char byte : random)
            {
                directory.push_back(kHexDigits[(byte >> 4u) & 0x0Fu]);
                directory.push_back(kHexDigits[byte & 0x0Fu]);
            }
            if (CreateDirectoryW(directory.c_str(), nullptr))
            {
                nativeError = ERROR_SUCCESS;
                return true;
            }
            nativeError = GetLastError();
            if (nativeError != ERROR_ALREADY_EXISTS)
                return false;
        }
        nativeError = ERROR_ALREADY_EXISTS;
        return false;
    }

    bool HashFileHandle(HANDLE file,
        std::array<unsigned char, 32>& digest) noexcept
    {
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
            return false;

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD resultSize = 0;
        unsigned char* object = nullptr;
        bool ok = false;
        std::array<unsigned char, 64 * 1024> buffer{};

        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                nullptr, 0) < 0)
            goto cleanup;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                &resultSize, 0) < 0 || objectSize == 0)
            goto cleanup;
        object = new (std::nothrow) unsigned char[objectSize];
        if (!object)
            goto cleanup;
        if (BCryptCreateHash(algorithm, &hash, object, objectSize,
                nullptr, 0, 0) < 0)
            goto cleanup;

        for (;;)
        {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(),
                    static_cast<DWORD>(buffer.size()), &read, nullptr))
                goto cleanup;
            if (read == 0)
                break;
            if (BCryptHashData(hash, buffer.data(), read, 0) < 0)
                goto cleanup;
        }
        if (BCryptFinishHash(hash, digest.data(),
                static_cast<ULONG>(digest.size()), 0) < 0)
            goto cleanup;
        ok = true;

    cleanup:
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        delete[] object;
        SetFilePointerEx(file, zero, nullptr, FILE_BEGIN);
        return ok;
    }

    bool VerifyAuthenticode(const std::wstring& path, HANDLE lockedFile,
        DWORD& nativeError) noexcept
    {
        WINTRUST_FILE_INFO fileInfo{};
        fileInfo.cbStruct = sizeof(fileInfo);
        fileInfo.pcwszFilePath = path.c_str();
        fileInfo.hFile = lockedFile;

        WINTRUST_DATA trust{};
        trust.cbStruct = sizeof(trust);
        trust.dwUIChoice = WTD_UI_NONE;
        trust.fdwRevocationChecks = WTD_REVOKE_NONE;
        trust.dwUnionChoice = WTD_CHOICE_FILE;
        trust.pFile = &fileInfo;
        trust.dwStateAction = WTD_STATEACTION_VERIFY;
        trust.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;

        GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        const LONG status = WinVerifyTrust(nullptr, &policy, &trust);
        trust.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &policy, &trust);
        if (status != ERROR_SUCCESS)
        {
            nativeError = static_cast<DWORD>(status);
            return false;
        }
        nativeError = ERROR_SUCCESS;
        return true;
    }

    bool TransitionToReadOnlyLock(ScopedInstallerFile& installer,
        DWORD& nativeError) noexcept
    {
        // A bridge handle keeps the random path alive while the writer is
        // closed. The final read-only handle denies all later write/delete
        // opens. Any writer that races this transition either blocks the final
        // lock or changes the hash checked immediately afterwards.
        UniqueHandle bridge(CreateFileW(installer.path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!bridge.Valid())
        {
            nativeError = GetLastError();
            return false;
        }

        installer.file.Reset();
        UniqueHandle locked(CreateFileW(installer.path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!locked.Valid())
        {
            nativeError = GetLastError();
            return false;
        }
        bridge.Reset();
        installer.file.Reset(locked.Release());
        nativeError = ERROR_SUCCESS;
        return true;
    }

    bool PrepareInstallerFile(HINSTANCE hInst, ScopedInstallerFile& output,
        EmbeddedVigemInstallStatus& failureStatus, DWORD& nativeError)
    {
        const unsigned char* bytes = nullptr;
        DWORD size = 0;
        if (!ReadResource(hInst, bytes, size, nativeError))
        {
            failureStatus = EmbeddedVigemInstallStatus::ResourceInvalid;
            return false;
        }
        if (!BuildUniqueTempDirectory(output.directory, nativeError))
        {
            failureStatus = EmbeddedVigemInstallStatus::ExtractionFailed;
            return false;
        }
        output.path = output.directory + L"\\" + kInstallerFileName;
        output.file.Reset(CreateFileW(output.path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
            nullptr));
        if (!output.file.Valid())
        {
            nativeError = GetLastError();
            failureStatus = EmbeddedVigemInstallStatus::ExtractionFailed;
            return false;
        }

        DWORD offset = 0;
        while (offset < size)
        {
            DWORD written = 0;
            if (!WriteFile(output.file.Get(), bytes + offset, size - offset,
                    &written, nullptr) || written == 0)
            {
                nativeError = GetLastError();
                failureStatus = EmbeddedVigemInstallStatus::ExtractionFailed;
                return false;
            }
            offset += written;
        }
        if (!FlushFileBuffers(output.file.Get()))
        {
            nativeError = GetLastError();
            failureStatus = EmbeddedVigemInstallStatus::ExtractionFailed;
            return false;
        }
        if (!TransitionToReadOnlyLock(output, nativeError))
        {
            failureStatus = EmbeddedVigemInstallStatus::ExtractionFailed;
            return false;
        }

        std::array<unsigned char, 32> digest{};
        if (!HashFileHandle(output.file.Get(), digest) ||
            digest != kExpectedInstallerSha256)
        {
            nativeError = ERROR_CRC;
            failureStatus = EmbeddedVigemInstallStatus::ResourceInvalid;
            return false;
        }
        if (!VerifyAuthenticode(
                output.path, output.file.Get(), nativeError))
        {
            failureStatus = EmbeddedVigemInstallStatus::SignatureInvalid;
            return false;
        }

        // Non-executing compatibility probe: the final lock must still allow
        // Windows to obtain the read/execute access used to map the image.
        UniqueHandle launchProbe(CreateFileW(output.path.c_str(),
            GENERIC_READ | GENERIC_EXECUTE,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!launchProbe.Valid())
        {
            nativeError = GetLastError();
            failureStatus = EmbeddedVigemInstallStatus::LaunchFailed;
            return false;
        }
        failureStatus = EmbeddedVigemInstallStatus::Installed;
        nativeError = ERROR_SUCCESS;
        return true;
    }

    DWORD WaitForInstaller(HANDLE process, HWND owner,
        bool& timedOut) noexcept
    {
        timedOut = false;
        const ULONGLONG deadline = GetTickCount64() + kInstallerWaitMs;
        bool sawQuit = false;
        int quitCode = 0;
        if (owner)
            EnableWindow(owner, FALSE);

        for (;;)
        {
            const ULONGLONG now = GetTickCount64();
            if (now >= deadline)
            {
                timedOut = true;
                break;
            }
            const DWORD remaining = static_cast<DWORD>(deadline - now);
            const DWORD slice = remaining < 250u ? remaining : 250u;
            const DWORD wait = MsgWaitForMultipleObjectsEx(
                1, &process, slice, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wait == WAIT_OBJECT_0)
                break;
            if (wait == WAIT_FAILED)
            {
                timedOut = true;
                break;
            }
            if (wait == WAIT_OBJECT_0 + 1)
            {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        sawQuit = true;
                        quitCode = static_cast<int>(message.wParam);
                        continue;
                    }
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }

        if (owner)
        {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        if (sawQuit)
            PostQuitMessage(quitCode);

        DWORD exitCode = STILL_ACTIVE;
        if (!timedOut && !GetExitCodeProcess(process, &exitCode))
            exitCode = static_cast<DWORD>(-1);
        return exitCode;
    }
}

bool EmbeddedVigemInstaller_VerifyOnly(HINSTANCE hInst,
    DWORD* nativeError) noexcept
{
    try
    {
        ScopedInstallerFile installer;
        EmbeddedVigemInstallStatus failure =
            EmbeddedVigemInstallStatus::ResourceInvalid;
        DWORD error = ERROR_SUCCESS;
        const bool ok = PrepareInstallerFile(
            hInst, installer, failure, error);
        installer.Cleanup(false);
        if (nativeError)
            *nativeError = error;
        return ok;
    }
    catch (const std::bad_alloc&)
    {
        if (nativeError)
            *nativeError = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    catch (...)
    {
        if (nativeError)
            *nativeError = ERROR_UNHANDLED_EXCEPTION;
        return false;
    }
}

EmbeddedVigemInstallResult EmbeddedVigemInstaller_Run(
    HINSTANCE hInst, HWND owner) noexcept
{
    EmbeddedVigemInstallResult result{};
    try
    {
        ScopedInstallerFile installer;
        if (!PrepareInstallerFile(hInst, installer, result.status,
                result.nativeError))
            return result;

        // Re-hash the same open file handle immediately before elevation. The
        // handle deliberately denies write/delete sharing until setup exits,
        // so a path replacement cannot occur between verification and execution.
        std::array<unsigned char, 32> digest{};
        if (!HashFileHandle(installer.file.Get(), digest) ||
            digest != kExpectedInstallerSha256)
        {
            result.status = EmbeddedVigemInstallStatus::ResourceInvalid;
            result.nativeError = ERROR_CRC;
            return result;
        }

        SHELLEXECUTEINFOW launch{};
        launch.cbSize = sizeof(launch);
        launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC |
            SEE_MASK_FLAG_NO_UI;
        launch.hwnd = owner;
        launch.lpVerb = L"runas";
        launch.lpFile = installer.path.c_str();
        launch.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&launch))
        {
            result.nativeError = GetLastError();
            result.status = result.nativeError == ERROR_CANCELLED
                ? EmbeddedVigemInstallStatus::UserCancelled
                : EmbeddedVigemInstallStatus::LaunchFailed;
            return result;
        }

        UniqueHandle process(launch.hProcess);
        bool timedOut = false;
        result.installerExitCode = WaitForInstaller(
            process.Get(), owner, timedOut);
        if (timedOut)
        {
            result.status = EmbeddedVigemInstallStatus::TimedOut;
            result.nativeError = WAIT_TIMEOUT;
            installer.Cleanup(true);
            return result;
        }

        installer.Cleanup(false);
        if (result.installerExitCode == ERROR_SUCCESS)
        {
            result.status = EmbeddedVigemInstallStatus::Installed;
            return result;
        }
        if (result.installerExitCode == ERROR_SUCCESS_REBOOT_REQUIRED ||
            result.installerExitCode == ERROR_SUCCESS_REBOOT_INITIATED)
        {
            result.status = EmbeddedVigemInstallStatus::RestartRequired;
            return result;
        }
        result.status = EmbeddedVigemInstallStatus::InstallerFailed;
        return result;
    }
    catch (const std::bad_alloc&)
    {
        result.status = EmbeddedVigemInstallStatus::ExtractionFailed;
        result.nativeError = ERROR_NOT_ENOUGH_MEMORY;
        return result;
    }
    catch (...)
    {
        result.status = EmbeddedVigemInstallStatus::ExtractionFailed;
        result.nativeError = ERROR_UNHANDLED_EXCEPTION;
        return result;
    }
}
