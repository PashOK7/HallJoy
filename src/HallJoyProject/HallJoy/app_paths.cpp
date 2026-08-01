#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "app_paths.h"
#include "file_name_policy.h"
#include "global_profiles.h"
#include "ini_util.h"
#include "stability_trace.h"
#include "win_util.h"

namespace fs = std::filesystem;

namespace
{
    struct PathState
    {
        bool success = false;
        AppDataMode mode = AppDataMode::LocalAppData;
        std::wstring dataRoot;
        std::wstring legacyRoot;
        std::wstring settingsIni;
        std::wstring bindingsIni;
        std::wstring globalProfilesDir;
        std::wstring layoutsDir;
        std::wstring curvePresetsDir;
    };

    std::once_flag g_initOnce;
    PathState g_paths;
    std::atomic<unsigned long long> g_writeProbeSequence{ 0 };

    std::wstring FullPath(const std::wstring& path)
    {
        if (path.empty()) return {};
        DWORD required = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (required == 0) return {};
        std::wstring full(static_cast<std::size_t>(required), L'\0');
        DWORD written = GetFullPathNameW(path.c_str(), required, full.data(), nullptr);
        if (written == 0 || written >= required) return {};
        full.resize(static_cast<std::size_t>(written));
        while (full.size() > 3 && (full.back() == L'\\' || full.back() == L'/'))
            full.pop_back();
        return full;
    }

    bool PathsEqual(const std::wstring& left, const std::wstring& right)
    {
        const std::wstring a = FullPath(left);
        const std::wstring b = FullPath(right);
        return !a.empty() && a.size() == b.size() &&
            CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    bool EnsureOrdinaryDirectory(const std::wstring& path)
    {
        if (path.empty()) return false;
        std::error_code ec;
        fs::create_directories(fs::path(path), ec);
        if (ec) return false;
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    bool DirectoryIsWritable(const std::wstring& directory)
    {
        if (!EnsureOrdinaryDirectory(directory)) return false;
        for (unsigned attempt = 0; attempt < 32; ++attempt)
        {
            const unsigned long long sequence =
                g_writeProbeSequence.fetch_add(1, std::memory_order_relaxed) + 1;
            wchar_t name[96]{};
            swprintf_s(name, L".halljoy-write-probe-%08lX-%016llX.tmp",
                GetCurrentProcessId(), sequence);
            const std::wstring path = (fs::path(directory) / name).wstring();
            HANDLE file = CreateFileW(
                path.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                const char probe[] = "HallJoy";
                DWORD written = 0;
                const bool ok = WriteFile(file, probe, sizeof(probe), &written, nullptr) != FALSE &&
                    written == sizeof(probe) && FlushFileBuffers(file) != FALSE;
                CloseHandle(file);
                return ok;
            }
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
                return false;
        }
        return false;
    }

    std::wstring LocalAppDataRoot()
    {
        PWSTR raw = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)) || !raw)
            return {};
        std::wstring root = (fs::path(raw) / L"HallJoy").wstring();
        CoTaskMemFree(raw);
        return root;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    std::wstring SimulatorArgument(const wchar_t* name)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return {};
        std::wstring value;
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] && wcscmp(argv[i], name) == 0 && i + 1 < argc && argv[i + 1])
            {
                value = argv[i + 1];
                break;
            }
        }
        LocalFree(argv);
        return value;
    }
#endif

    unsigned long long HashPath(const std::wstring& path)
    {
        const std::wstring full = FullPath(path);
        unsigned long long hash = 1469598103934665603ull;
        for (wchar_t ch : full)
        {
            wchar_t folded = ch;
            CharLowerBuffW(&folded, 1);
            hash ^= static_cast<unsigned short>(folded);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    bool FilesEqual(const wchar_t* leftPath, const wchar_t* rightPath, DWORD* errorOut)
    {
        HANDLE left = CreateFileW(leftPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (left == INVALID_HANDLE_VALUE)
        {
            if (errorOut) *errorOut = GetLastError();
            return false;
        }
        HANDLE right = CreateFileW(rightPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (right == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();
            CloseHandle(left);
            if (errorOut) *errorOut = error;
            return false;
        }

        LARGE_INTEGER leftSize{};
        LARGE_INTEGER rightSize{};
        bool equal = GetFileSizeEx(left, &leftSize) != FALSE &&
            GetFileSizeEx(right, &rightSize) != FALSE &&
            leftSize.QuadPart == rightSize.QuadPart;
        std::array<unsigned char, 64 * 1024> leftBuffer{};
        std::array<unsigned char, 64 * 1024> rightBuffer{};
        while (equal)
        {
            DWORD leftRead = 0;
            DWORD rightRead = 0;
            if (!ReadFile(left, leftBuffer.data(), static_cast<DWORD>(leftBuffer.size()), &leftRead, nullptr) ||
                !ReadFile(right, rightBuffer.data(), static_cast<DWORD>(rightBuffer.size()), &rightRead, nullptr))
            {
                equal = false;
                break;
            }
            if (leftRead != rightRead ||
                !std::equal(leftBuffer.begin(), leftBuffer.begin() + leftRead, rightBuffer.begin()))
            {
                equal = false;
                break;
            }
            if (leftRead == 0) break;
        }
        const DWORD error = equal ? ERROR_SUCCESS : GetLastError();
        CloseHandle(right);
        CloseHandle(left);
        if (!equal && errorOut) *errorOut = error != ERROR_SUCCESS ? error : ERROR_CRC;
        return equal;
    }

    struct CopyContext
    {
        const wchar_t* source = nullptr;
    };

    bool CopyTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CopyContext*>(rawContext);
        if (!CopyFileW(context->source, temporaryPath, FALSE))
        {
            if (errorOut) *errorOut = GetLastError();
            return false;
        }
        SetFileAttributesW(temporaryPath, FILE_ATTRIBUTE_NORMAL);
        return true;
    }

    bool CopyTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<CopyContext*>(rawContext);
        return FilesEqual(context->source, temporaryPath, errorOut);
    }

    bool CopyFileTransactional(const std::wstring& source, const std::wstring& destination, const wchar_t* kind)
    {
        if (!EnsureOrdinaryDirectory(fs::path(destination).parent_path().wstring()))
            return false;
        CopyContext context{ source.c_str() };
        const auto result = IniUtil_SaveAtomic(
            destination.c_str(),
            CopyTransactionWrite,
            CopyTransactionValidate,
            &context);
        if (!result.Succeeded())
        {
            IniUtil_ReportSaveFailure(kind, destination.c_str(), result);
            return false;
        }
        return true;
    }

    struct MigrationMarkerContext
    {
        const wchar_t* sourceRoot = nullptr;
    };

    bool MarkerWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<MigrationMarkerContext*>(rawContext);
        bool ok = WritePrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"DataMigration", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"Migration", L"SourceRoot", context->sourceRoot, temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"Migration", L"Complete", L"1", temporaryPath) != FALSE;
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool MarkerValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<MigrationMarkerContext*>(rawContext);
        wchar_t schema[16]{};
        wchar_t kind[32]{};
        std::vector<wchar_t> source(32768, L'\0');
        const int complete = GetPrivateProfileIntW(L"Migration", L"Complete", 0, temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"", schema, _countof(schema), temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"", kind, _countof(kind), temporaryPath);
        GetPrivateProfileStringW(L"Migration", L"SourceRoot", L"", source.data(), static_cast<DWORD>(source.size()), temporaryPath);
        const bool ok = wcscmp(schema, L"1") == 0 && wcscmp(kind, L"DataMigration") == 0 &&
            complete == 1 && PathsEqual(source.data(), context->sourceRoot);
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }

    bool ExistingMarkerIsComplete(const std::wstring& marker, const std::wstring& sourceRoot)
    {
        if (GetFileAttributesW(marker.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        MigrationMarkerContext context{ sourceRoot.c_str() };
        DWORD error = ERROR_SUCCESS;
        return MarkerValidate(marker.c_str(), &context, &error);
    }

    bool CollectLegacyFiles(const std::wstring& sourceRoot, std::vector<fs::path>& out)
    {
        out.clear();
        for (const wchar_t* leaf : { L"settings.ini", L"bindings.ini" })
        {
            fs::path path = fs::path(sourceRoot) / leaf;
            std::error_code ec;
            if (fs::is_regular_file(path, ec) && !ec)
                out.push_back(path);
        }

        for (const wchar_t* directory : { L"GlobalProfiles", L"Layouts", L"CurvePresets" })
        {
            fs::path root = fs::path(sourceRoot) / directory;
            std::error_code ec;
            if (!fs::is_directory(root, ec) || ec) continue;
            const DWORD rootAttributes = GetFileAttributesW(root.c_str());
            if (rootAttributes == INVALID_FILE_ATTRIBUTES || (rootAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                return false;

            fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;
            while (!ec && iterator != end)
            {
                const DWORD attributes = GetFileAttributesW(iterator->path().c_str());
                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                {
                    if (iterator->is_directory(ec)) iterator.disable_recursion_pending();
                }
                else if (iterator->is_regular_file(ec) && !ec &&
                    iterator->path().filename().wstring().find(L".halljoy-new-") == std::wstring::npos)
                {
                    out.push_back(iterator->path());
                }
                iterator.increment(ec);
            }
            if (ec) return false;
        }
        return true;
    }

    bool MigrateLegacyState(const std::wstring& sourceRoot, const std::wstring& destinationRoot)
    {
        if (PathsEqual(sourceRoot, destinationRoot)) return true;
        if (!EnsureOrdinaryDirectory(destinationRoot)) return false;

        const unsigned long long sourceHash = HashPath(sourceRoot);
        wchar_t hashText[32]{};
        swprintf_s(hashText, L"%016llX", sourceHash);
        const std::wstring marker = (fs::path(destinationRoot) /
            (std::wstring(L".migration-from-exe-") + hashText + L".ini")).wstring();
        if (ExistingMarkerIsComplete(marker, sourceRoot))
        {
            StabilityTrace_Write(L"INFO", L"storage", L"migration.skip",
                L"reason=complete source=%ls destination=%ls", sourceRoot.c_str(), destinationRoot.c_str());
            return true;
        }

        std::vector<fs::path> legacyFiles;
        if (!CollectLegacyFiles(sourceRoot, legacyFiles)) return false;
        const std::wstring backupRoot = (fs::path(destinationRoot) / L"MigrationBackups" /
            (std::wstring(L"legacy-") + hashText)).wstring();
        if (!EnsureOrdinaryDirectory(backupRoot)) return false;

        StabilityTrace_Write(L"INFO", L"storage", L"migration.begin",
            L"files=%zu source=%ls destination=%ls backup=%ls",
            legacyFiles.size(), sourceRoot.c_str(), destinationRoot.c_str(), backupRoot.c_str());

        for (const fs::path& source : legacyFiles)
        {
            std::error_code ec;
            const fs::path relative = fs::relative(source, fs::path(sourceRoot), ec);
            if (ec || relative.empty() || relative.is_absolute()) return false;
            const std::wstring relativeText = relative.wstring();
            for (const fs::path& part : relative)
            {
                if (part == L".." || part == L".") return false;
            }

            const std::wstring backup = (fs::path(backupRoot) / relative).wstring();
            if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                if (!CopyFileTransactional(source.wstring(), backup, L"migration backup")) return false;
            }
            else
            {
                DWORD error = ERROR_SUCCESS;
                if (!FilesEqual(source.c_str(), backup.c_str(), &error)) return false;
            }

            const std::wstring destination = (fs::path(destinationRoot) / relative).wstring();
            if (GetFileAttributesW(destination.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                if (!CopyFileTransactional(source.wstring(), destination, L"migration data")) return false;
                StabilityTrace_Write(L"INFO", L"storage", L"migration.copy", L"relative=%ls", relativeText.c_str());
            }
            else
            {
                StabilityTrace_Write(L"INFO", L"storage", L"migration.keep_existing", L"relative=%ls", relativeText.c_str());
            }
        }

        MigrationMarkerContext markerContext{ sourceRoot.c_str() };
        const auto markerResult = IniUtil_SaveAtomic(marker.c_str(), MarkerWrite, MarkerValidate, &markerContext);
        if (!markerResult.Succeeded())
        {
            IniUtil_ReportSaveFailure(L"migration marker", marker.c_str(), markerResult);
            return false;
        }
        StabilityTrace_Write(L"INFO", L"storage", L"migration.complete",
            L"files=%zu source_preserved=1 backup=%ls", legacyFiles.size(), backupRoot.c_str());
        return true;
    }

    void InitializePaths()
    {
        g_paths.legacyRoot = FullPath(WinUtil_GetExeDir());
        if (g_paths.legacyRoot.empty()) return;

#if defined(HALLJOY_ANALOG_SIMULATOR)
        const std::wstring simulatorRoot = SimulatorArgument(L"--halljoy-test-data-root");
        const std::wstring simulatorLegacy = SimulatorArgument(L"--halljoy-test-legacy-root");
        if (!simulatorRoot.empty())
        {
            g_paths.mode = AppDataMode::SimulatorOverride;
            g_paths.dataRoot = FullPath(simulatorRoot);
            if (!simulatorLegacy.empty()) g_paths.legacyRoot = FullPath(simulatorLegacy);
        }
        else
#endif
        {
            const std::wstring portableMarker = (fs::path(g_paths.legacyRoot) / L"HallJoy.portable").wstring();
            const DWORD markerAttributes = GetFileAttributesW(portableMarker.c_str());
            if (markerAttributes != INVALID_FILE_ATTRIBUTES &&
                (markerAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0 &&
                DirectoryIsWritable(g_paths.legacyRoot))
            {
                g_paths.mode = AppDataMode::Portable;
                g_paths.dataRoot = g_paths.legacyRoot;
            }
            else
            {
                g_paths.mode = AppDataMode::LocalAppData;
                g_paths.dataRoot = FullPath(LocalAppDataRoot());
            }
        }

        if (g_paths.dataRoot.empty() || !EnsureOrdinaryDirectory(g_paths.dataRoot)) return;
        if (g_paths.mode != AppDataMode::Portable &&
            !MigrateLegacyState(g_paths.legacyRoot, g_paths.dataRoot))
            return;

        g_paths.settingsIni = (fs::path(g_paths.dataRoot) / L"settings.ini").wstring();
        g_paths.bindingsIni = (fs::path(g_paths.dataRoot) / L"bindings.ini").wstring();
        g_paths.globalProfilesDir = (fs::path(g_paths.dataRoot) / L"GlobalProfiles").wstring();
        g_paths.layoutsDir = (fs::path(g_paths.dataRoot) / L"Layouts").wstring();
        g_paths.curvePresetsDir = (fs::path(g_paths.dataRoot) / L"CurvePresets").wstring();
        if (!EnsureOrdinaryDirectory(g_paths.globalProfilesDir) ||
            !EnsureOrdinaryDirectory(g_paths.layoutsDir) ||
            !EnsureOrdinaryDirectory(g_paths.curvePresetsDir))
            return;

        g_paths.success = true;
        StabilityTrace_Write(L"INFO", L"storage", L"root.ready",
            L"mode=%ls root=%ls legacy=%ls", AppPaths_ModeName(), g_paths.dataRoot.c_str(), g_paths.legacyRoot.c_str());
    }

    const PathState& Paths()
    {
        std::call_once(g_initOnce, InitializePaths);
        return g_paths;
    }
}

bool AppPaths_Initialize()
{
    return Paths().success;
}

AppDataMode AppPaths_Mode()
{
    return Paths().mode;
}

const wchar_t* AppPaths_ModeName()
{
    switch (g_paths.mode)
    {
    case AppDataMode::Portable: return L"portable";
    case AppDataMode::SimulatorOverride: return L"simulator";
    default: return L"localappdata";
    }
}

const std::wstring& AppPaths_DataRoot() { return Paths().dataRoot; }
const std::wstring& AppPaths_LegacyDataRoot() { return Paths().legacyRoot; }
const std::wstring& AppPaths_SettingsIni() { return Paths().settingsIni; }
const std::wstring& AppPaths_BindingsIni() { return Paths().bindingsIni; }
const std::wstring& AppPaths_GlobalProfilesDir() { return Paths().globalProfilesDir; }
const std::wstring& AppPaths_LayoutsDir() { return Paths().layoutsDir; }
const std::wstring& AppPaths_CurvePresetsDir() { return Paths().curvePresetsDir; }

std::wstring AppPaths_ActiveSettingsIni()
{
    return GlobalProfiles_GetSettingsPath(GlobalProfiles_GetActiveName());
}

std::wstring AppPaths_ActiveBindingsIni()
{
    return GlobalProfiles_GetBindingsPath(GlobalProfiles_GetActiveName());
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool AppPaths_RunStoragePolicySelfTest()
{
    const std::wstring composed = L"Caf\u00E9";
    const std::wstring decomposed = L"Cafe\u0301";
    std::wstring path;
    return FileNamePolicy_Equivalent(composed, decomposed) &&
        FileNamePolicy_Equivalent(L"Profile", L"profile") &&
        FileNamePolicy_NormalizeStem(L" CON ") == L"_CON" &&
        FileNamePolicy_NormalizeStem(std::wstring(200, L'A')).size() == FileNamePolicy_MaxStemLength() &&
        !FileNamePolicy_BuildChildPath(AppPaths_DataRoot(), L"..", L".ini", path) &&
        FileNamePolicy_BuildChildPath(AppPaths_DataRoot(), L"Safe", L".ini", path);
}
#endif
