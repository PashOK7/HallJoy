#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cwchar>
#include <string>

#include "ini_util.h"
#include "stability_trace.h"

namespace
{
    std::atomic<unsigned long long> g_tempSequence{ 0 };
    std::atomic<bool> g_failureDialogShown{ false };

    DWORD NormalizeError(DWORD error, DWORD fallback) noexcept
    {
        return error != ERROR_SUCCESS ? error : fallback;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    bool ShouldInjectFailure(HallJoyPersistence::SaveStage stage) noexcept
    {
        const wchar_t* commandLine = GetCommandLineW();
        if (!commandLine) return false;

        const wchar_t* marker = nullptr;
        switch (stage)
        {
        case HallJoyPersistence::SaveStage::Prepare: marker = L"--halljoy-test-persistence-failure-prepare"; break;
        case HallJoyPersistence::SaveStage::Write: marker = L"--halljoy-test-persistence-failure-write"; break;
        case HallJoyPersistence::SaveStage::Flush: marker = L"--halljoy-test-persistence-failure-flush"; break;
        case HallJoyPersistence::SaveStage::Validate: marker = L"--halljoy-test-persistence-failure-validate"; break;
        case HallJoyPersistence::SaveStage::Replace: marker = L"--halljoy-test-persistence-failure-replace"; break;
        default: return false;
        }
        return wcsstr(commandLine, marker) != nullptr;
    }
#else
    bool ShouldInjectFailure(HallJoyPersistence::SaveStage) noexcept { return false; }
#endif

    class Win32TransactionAdapter
    {
    public:
        Win32TransactionAdapter(
            const wchar_t* destinationPath,
            IniUtilWriteCallback writer,
            IniUtilValidateCallback validator,
            void* context) :
            destination_(destinationPath ? destinationPath : L""),
            writer_(writer),
            validator_(validator),
            context_(context)
        {
        }

        bool Prepare()
        {
            if (destination_.empty() || !writer_ || !validator_)
            {
                error_ = ERROR_INVALID_PARAMETER;
                return false;
            }
            if (ShouldInjectFailure(HallJoyPersistence::SaveStage::Prepare))
            {
                error_ = ERROR_WRITE_FAULT;
                return false;
            }

            for (unsigned attempt = 0; attempt < 128; ++attempt)
            {
                const unsigned long long sequence = g_tempSequence.fetch_add(1, std::memory_order_relaxed) + 1;
                wchar_t suffix[96]{};
                swprintf_s(
                    suffix,
                    L".halljoy-new-%08lX-%08lX-%016llX",
                    GetCurrentProcessId(),
                    GetCurrentThreadId(),
                    sequence);
                temporary_ = destination_ + suffix;

                HANDLE file = CreateFileW(
                    temporary_.c_str(),
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ,
                    nullptr,
                    CREATE_NEW,
                    FILE_ATTRIBUTE_TEMPORARY,
                    nullptr);
                if (file != INVALID_HANDLE_VALUE)
                {
                    if (!CloseHandle(file))
                    {
                        error_ = NormalizeError(GetLastError(), ERROR_WRITE_FAULT);
                        DeleteFileW(temporary_.c_str());
                        temporary_.clear();
                        return false;
                    }
                    return true;
                }

                error_ = NormalizeError(GetLastError(), ERROR_CANNOT_MAKE);
                if (error_ != ERROR_FILE_EXISTS && error_ != ERROR_ALREADY_EXISTS)
                    return false;
            }

            error_ = ERROR_FILE_EXISTS;
            return false;
        }

        bool Write()
        {
            if (ShouldInjectFailure(HallJoyPersistence::SaveStage::Write))
            {
                error_ = ERROR_WRITE_FAULT;
                return false;
            }
            DWORD callbackError = ERROR_SUCCESS;
            if (!writer_(temporary_.c_str(), context_, &callbackError))
            {
                error_ = NormalizeError(callbackError, ERROR_WRITE_FAULT);
                return false;
            }
            return true;
        }

        bool Flush()
        {
            if (ShouldInjectFailure(HallJoyPersistence::SaveStage::Flush))
            {
                error_ = ERROR_WRITE_FAULT;
                return false;
            }
            if (!IniUtil_Flush(temporary_.c_str()))
            {
                error_ = NormalizeError(GetLastError(), ERROR_WRITE_FAULT);
                return false;
            }
            return true;
        }

        bool Validate()
        {
            if (ShouldInjectFailure(HallJoyPersistence::SaveStage::Validate))
            {
                error_ = ERROR_INVALID_DATA;
                return false;
            }
            DWORD callbackError = ERROR_SUCCESS;
            if (!validator_(temporary_.c_str(), context_, &callbackError))
            {
                error_ = NormalizeError(callbackError, ERROR_INVALID_DATA);
                return false;
            }
            return true;
        }

        bool Replace()
        {
            if (ShouldInjectFailure(HallJoyPersistence::SaveStage::Replace))
            {
                error_ = ERROR_ACCESS_DENIED;
                return false;
            }
            if (!MoveFileExW(
                temporary_.c_str(),
                destination_.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                error_ = NormalizeError(GetLastError(), ERROR_WRITE_FAULT);
                return false;
            }
            temporary_.clear();
            return true;
        }

        void Cleanup() noexcept
        {
            if (!temporary_.empty())
            {
                DeleteFileW(temporary_.c_str());
                temporary_.clear();
            }
        }

        DWORD LastError() const noexcept { return error_; }

    private:
        std::wstring destination_;
        std::wstring temporary_;
        IniUtilWriteCallback writer_ = nullptr;
        IniUtilValidateCallback validator_ = nullptr;
        void* context_ = nullptr;
        DWORD error_ = ERROR_SUCCESS;
    };
}

bool IniUtil_Flush(const wchar_t* path)
{
    if (!path || !*path)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    // The legacy profile-cache flush call can return FALSE with ERROR_FILE_NOT_FOUND
    // even after successful absolute-path writes. Treat it as a cache hint; the
    // checked FlushFileBuffers below is the durability boundary.
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path);

    HANDLE file = CreateFileW(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        const DWORD error = NormalizeError(GetLastError(), ERROR_OPEN_FAILED);
        StabilityTrace_WriteCritical(
            L"ERROR", L"persistence", L"flush.open_failed",
            L"error=%lu attributes=0x%08lX path=%ls",
            static_cast<unsigned long>(error),
            static_cast<unsigned long>(GetFileAttributesW(path)),
            path);
        SetLastError(error);
        return false;
    }

    if (!FlushFileBuffers(file))
    {
        const DWORD error = GetLastError();
        CloseHandle(file);
        SetLastError(error);
        return false;
    }
    if (!CloseHandle(file))
        return false;
    return true;
}

HallJoyPersistence::SaveResult IniUtil_SaveAtomic(
    const wchar_t* destinationPath,
    IniUtilWriteCallback writer,
    IniUtilValidateCallback validator,
    void* context)
{
    Win32TransactionAdapter adapter(destinationPath, writer, validator, context);
    return HallJoyPersistence::SaveTransaction(adapter);
}

const wchar_t* IniUtil_SaveStageName(HallJoyPersistence::SaveStage stage) noexcept
{
    switch (stage)
    {
    case HallJoyPersistence::SaveStage::None: return L"none";
    case HallJoyPersistence::SaveStage::Prepare: return L"prepare";
    case HallJoyPersistence::SaveStage::Write: return L"write";
    case HallJoyPersistence::SaveStage::Flush: return L"flush";
    case HallJoyPersistence::SaveStage::Validate: return L"validate";
    case HallJoyPersistence::SaveStage::Replace: return L"replace";
    case HallJoyPersistence::SaveStage::UnexpectedException: return L"exception";
    default: return L"unknown";
    }
}

void IniUtil_ReportSaveFailure(
    const wchar_t* dataKind,
    const wchar_t* destinationPath,
    const HallJoyPersistence::SaveResult& result)
{
    const wchar_t* kind = dataKind ? dataKind : L"data";
    const wchar_t* path = destinationPath ? destinationPath : L"(null)";
    const wchar_t* stage = IniUtil_SaveStageName(result.stage);
    StabilityTrace_WriteCritical(
        L"ERROR",
        L"persistence",
        L"save.failure",
        L"kind=%ls stage=%ls error=%lu path=%ls",
        kind,
        stage,
        static_cast<unsigned long>(result.nativeError),
        path);

#if !defined(HALLJOY_ANALOG_SIMULATOR)
    bool expected = false;
    if (g_failureDialogShown.compare_exchange_strong(expected, true, std::memory_order_relaxed))
    {
        wchar_t message[1024]{};
        swprintf_s(
            message,
            L"HallJoy could not save %ls.\n\nStage: %ls\nWindows error: %lu\nFile: %ls\n\nThe previous file was kept unchanged.",
            kind,
            stage,
            static_cast<unsigned long>(result.nativeError),
            path);
        MessageBoxW(GetActiveWindow(), message, L"HallJoy - save failed", MB_OK | MB_ICONERROR);
    }
#endif
}

bool IniUtil_CopyExistingForUpdate(
    const wchar_t* destinationPath,
    const wchar_t* temporaryPath,
    DWORD* errorOut)
{
    if (!destinationPath || !temporaryPath)
    {
        if (errorOut) *errorOut = ERROR_INVALID_PARAMETER;
        return false;
    }

    const DWORD attributes = GetFileAttributesW(destinationPath);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return true;
        if (errorOut) *errorOut = NormalizeError(error, ERROR_READ_FAULT);
        return false;
    }

    if (!CopyFileW(destinationPath, temporaryPath, FALSE))
    {
        if (errorOut) *errorOut = NormalizeError(GetLastError(), ERROR_WRITE_FAULT);
        return false;
    }
    return true;
}

bool IniUtil_AtomicReplace(const wchar_t* tmpPath, const wchar_t* dstPath)
{
    if (!tmpPath || !dstPath) return false;

    if (!IniUtil_Flush(tmpPath))
    {
        DeleteFileW(tmpPath);
        return false;
    }

    BOOL ok = MoveFileExW(tmpPath, dstPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok)
    {
        DeleteFileW(tmpPath);
        return false;
    }
    return true;
}
