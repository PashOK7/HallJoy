#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "global_profiles.h"
#include "app_paths.h"
#include "file_name_policy.h"
#include "ini_util.h"
#include "bounded_ini.h"
#include "settings_ini.h"
#include "profile_ini.h"
#include "profile_runtime_gate.h"
#include "stability_trace.h"

namespace fs = std::filesystem;

static constexpr const wchar_t* kDefaultProfileName = L"Default";
static constexpr const wchar_t* kMainSection = L"Main";
static constexpr const wchar_t* kActiveProfileKey = L"ActiveGlobalProfile";

static std::wstring g_activeProfile = kDefaultProfileName;
static bool g_dirty = false;

static bool IEquals(const std::wstring& a, const std::wstring& b)
{
    return FileNamePolicy_Equivalent(a, b);
}

std::wstring GlobalProfiles_SanitizeName(const std::wstring& in)
{
    return FileNamePolicy_NormalizeStem(in);
}

bool GlobalProfiles_IsDefault(const std::wstring& name)
{
    return name.empty() || IEquals(name, kDefaultProfileName);
}

void GlobalProfiles_InitFromSettingsIni(const wchar_t* settingsIniPath)
{
    g_activeProfile = kDefaultProfileName;
    g_dirty = false;
    if (!settingsIniPath) return;

    wchar_t buf[260]{};
    GetPrivateProfileStringW(kMainSection, kActiveProfileKey, kDefaultProfileName, buf, (DWORD)_countof(buf), settingsIniPath);
    std::wstring n = GlobalProfiles_SanitizeName(buf);
    if (n.empty()) n = kDefaultProfileName;
    g_activeProfile = n;
}

namespace
{
    struct ActiveProfileSaveContext
    {
        const wchar_t* destinationPath = nullptr;
        const wchar_t* activeName = nullptr;
    };

    bool ActiveProfileTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<ActiveProfileSaveContext*>(rawContext);
        if (!IniUtil_CopyExistingForUpdate(context->destinationPath, temporaryPath, errorOut))
            return false;

        bool ok = WritePrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"Settings", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(kMainSection, kActiveProfileKey, context->activeName, temporaryPath) != FALSE;
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool ActiveProfileTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<ActiveProfileSaveContext*>(rawContext);
        wchar_t schema[32]{};
        wchar_t kind[32]{};
        wchar_t active[260]{};
        GetPrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"{missing}", schema, (DWORD)_countof(schema), temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"{missing}", kind, (DWORD)_countof(kind), temporaryPath);
        GetPrivateProfileStringW(kMainSection, kActiveProfileKey, L"{missing}", active, (DWORD)_countof(active), temporaryPath);
        const bool ok = wcscmp(schema, L"1") == 0 &&
            wcscmp(kind, L"Settings") == 0 &&
            wcscmp(active, context->activeName) == 0;
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }
}

bool GlobalProfiles_SaveActiveToSettingsIni(const wchar_t* settingsIniPath)
{
    if (!settingsIniPath || !*settingsIniPath) return false;
    ActiveProfileSaveContext context{ settingsIniPath, g_activeProfile.c_str() };
    const auto result = IniUtil_SaveAtomic(
        settingsIniPath,
        ActiveProfileTransactionWrite,
        ActiveProfileTransactionValidate,
        &context);
    if (!result.Succeeded())
    {
        IniUtil_ReportSaveFailure(L"active profile", settingsIniPath, result);
        return false;
    }
    return true;
}

const std::wstring& GlobalProfiles_GetActiveName()
{
    return g_activeProfile;
}

void GlobalProfiles_SetActiveName(const std::wstring& name)
{
    std::wstring n = GlobalProfiles_SanitizeName(name);
    if (n.empty()) n = kDefaultProfileName;
    g_activeProfile = n;
}

bool GlobalProfiles_IsDirty()
{
    return g_dirty;
}

void GlobalProfiles_SetDirty(bool dirty)
{
    g_dirty = dirty;
}

static fs::path EnsureProfilesDir()
{
    fs::path dir(AppPaths_GlobalProfilesDir());
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

static std::wstring FindExistingProfilePath(const std::wstring& name, const wchar_t* suffix)
{
    const std::wstring wanted = GlobalProfiles_SanitizeName(name);
    if (wanted.empty()) return {};
    const std::size_t suffixLength = wcslen(suffix);
    std::vector<fs::path> matches;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(EnsureProfilesDir(), ec))
    {
        if (ec) return {};
        if (!entry.is_regular_file(ec) || ec) continue;
        const std::wstring fileName = entry.path().filename().wstring();
        if (fileName.size() <= suffixLength ||
            _wcsicmp(fileName.c_str() + fileName.size() - suffixLength, suffix) != 0)
            continue;
        const std::wstring base = fileName.substr(0, fileName.size() - suffixLength);
        if (FileNamePolicy_Equivalent(base, wanted))
            matches.push_back(entry.path());
    }
    if (matches.empty()) return {};
    std::sort(matches.begin(), matches.end(), [](const fs::path& left, const fs::path& right) {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    });
    return matches.front().wstring();
}

static std::wstring BuildProfilePath(const std::wstring& name, const wchar_t* suffix)
{
    const std::wstring existing = FindExistingProfilePath(name, suffix);
    if (!existing.empty()) return existing;
    std::wstring path;
    if (!FileNamePolicy_BuildChildPath(EnsureProfilesDir().wstring(), name, suffix, path))
        return {};
    return path;
}

std::wstring GlobalProfiles_GetSettingsPath(const std::wstring& name)
{
    if (GlobalProfiles_IsDefault(name))
        return AppPaths_SettingsIni();

    return BuildProfilePath(name, L".settings.ini");
}

std::wstring GlobalProfiles_GetBindingsPath(const std::wstring& name)
{
    const auto settings = GlobalProfiles_GetSettingsPath(name);
    if (halljoy::ini::HasBundle(settings.c_str())) return settings;
    if (GlobalProfiles_IsDefault(name))
        return AppPaths_BindingsIni();

    return BuildProfilePath(name, L".bindings.ini");
}

void GlobalProfiles_List(std::vector<std::wstring>& outNames)
{
    outNames.clear();
    outNames.push_back(kDefaultProfileName);

    fs::path dir = EnsureProfilesDir();
    std::set<std::wstring> seenKeys;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        auto name = e.path().filename().wstring();
        const wchar_t* suffix = L".settings.ini";
        if (name.size() <= wcslen(suffix)) continue;
        if (_wcsicmp(name.c_str() + (name.size() - wcslen(suffix)), suffix) != 0) continue;
        std::wstring base = GlobalProfiles_SanitizeName(name.substr(0, name.size() - wcslen(suffix)));
        const std::wstring key = FileNamePolicy_CanonicalKey(base);
        if (!base.empty() && !GlobalProfiles_IsDefault(base) && seenKeys.insert(key).second)
            outNames.push_back(std::move(base));
    }

    std::sort(outNames.begin() + 1, outNames.end(), [](const std::wstring& a, const std::wstring& b) {
        return FileNamePolicy_CanonicalKey(a) < FileNamePolicy_CanonicalKey(b);
    });
}

bool GlobalProfiles_Delete(const std::wstring& name)
{
    if (GlobalProfiles_IsDefault(name) || FileNamePolicy_Equivalent(name, g_activeProfile))
        return false;

    std::wstring s = GlobalProfiles_GetSettingsPath(name);
    std::wstring b = BuildProfilePath(name, L".bindings.ini");
    std::error_code ec1, ec2;
    bool ok1 = fs::remove(fs::path(s), ec1) || !fs::exists(fs::path(s), ec1);
    bool ok2 = fs::remove(fs::path(b), ec2) || !fs::exists(fs::path(b), ec2);
    return ok1 && ok2;
}

bool GlobalProfiles_Save(const std::wstring& name) {
    const auto path = GlobalProfiles_GetSettingsPath(name);
    return GlobalProfiles_IsDefault(name) ? SettingsIni_Save(path.c_str()) :
        SettingsIni_SaveProfile(path.c_str());
}
bool GlobalProfiles_Prepare(const std::wstring& name, std::function<void()>& apply) {
    const auto settingsPath = GlobalProfiles_GetSettingsPath(name);
    // Pin both halves for the entire preparation, including legacy pairs.
    halljoy::ini::ReadFile settingsFile(settingsPath.c_str());
    if (!settingsFile) return false;
    const auto bindingsPath = GlobalProfiles_GetBindingsPath(name);
    halljoy::ini::ReadFile bindingsFile(bindingsPath.c_str());
    if (!bindingsFile) return false;
    std::function<void()> settingsApply;
    BindingsSnapshot bindings;
    if (!SettingsIni_PrepareProfile(settingsPath.c_str(), settingsApply) ||
        !Profile_PrepareIni(bindingsPath.c_str(), bindings)) return false;
    apply = [settingsApply = std::move(settingsApply), bindings]() mutable {
        settingsApply();
        Bindings_Apply(bindings);
    };
    return true;
}
bool GlobalProfiles_Load(const std::wstring& name) {
    std::function<void()> apply;
    if (!GlobalProfiles_Prepare(name, apply)) return false;
    halljoy::profile_runtime::CommitLease commit;
    if (!commit) return false;
    apply();
    return true;
}
bool GlobalProfiles_Switch(const std::wstring& name) {
    const auto next = GlobalProfiles_SanitizeName(name);
    if (next.empty()) return false;
    if (FileNamePolicy_Equivalent(next, g_activeProfile)) return true;
    std::function<void()> apply;
    if (!GlobalProfiles_Prepare(next, apply)) return false;
    const auto previous = g_activeProfile;
    if (!GlobalProfiles_Save(previous)) return false;
    GlobalProfiles_SetActiveName(next);
    if (!GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str())) {
        GlobalProfiles_SetActiveName(previous);
        return false;
    }
    bool committed = false;
    {
        halljoy::profile_runtime::CommitLease commit;
        if (commit) { apply(); committed = true; }
    }
    if (!committed) {
        GlobalProfiles_SetActiveName(previous);
        GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str());
        return false;
    }
    g_dirty = false;
    return true;
}

namespace {
    bool MissingFile(const std::wstring& path) {
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    // Empty files must also be preserved. Reject reparse points and unreadable
    // or oversized files rather than assuming they are absent and overwriting.
    bool ReadRecoveryBytes(const std::wstring& path, std::vector<unsigned char>& bytes) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION info{};
        LARGE_INTEGER size{};
        bool ok = GetFileInformationByHandle(file, &info) &&
            !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
            GetFileSizeEx(file, &size) && size.QuadPart >= 0 &&
            size.QuadPart <= static_cast<LONGLONG>(halljoy::ini::kMaxFileBytes);
        if (ok) {
            bytes.resize(static_cast<size_t>(size.QuadPart));
            DWORD read = 0;
            ok = bytes.empty() || (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                &read, nullptr) && read == bytes.size());
        }
        CloseHandle(file);
        return ok;
    }

    bool PlainRecoveryDirectory(const fs::path& path) {
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
    }

    bool PreserveStartupFiles(std::wstring& backupPath) {
        const std::wstring paths[] = { AppPaths_SettingsIni(), AppPaths_BindingsIni() };
        std::vector<unsigned char> originals[2];
        bool exists[2]{};
        uint64_t hash = 14695981039346656037ull;
        for (int i = 0; i < 2; ++i) {
            exists[i] = !MissingFile(paths[i]);
            if (exists[i] && !ReadRecoveryBytes(paths[i], originals[i])) return false;
            hash = (hash ^ static_cast<uint64_t>(exists[i])) * 1099511628211ull;
            for (unsigned char byte : originals[i]) hash = (hash ^ byte) * 1099511628211ull;
            hash = (hash ^ static_cast<uint64_t>(originals[i].size())) * 1099511628211ull;
        }
        if (!exists[0] && !exists[1]) return true;
        const auto internal = fs::path(AppPaths_DataRoot()) / L".internal";
        const auto parent = internal / L"ProfileRecovery";
        const auto directory = parent / std::to_wstring(hash);
        if (!PlainRecoveryDirectory(internal) || !PlainRecoveryDirectory(parent) ||
            !PlainRecoveryDirectory(directory)) return false;
        backupPath = directory.wstring();
        for (int i = 0; i < 2; ++i) {
            if (!exists[i]) continue;
            const auto destination = directory / fs::path(paths[i]).filename();
            // Content-derived directory prevents duplicate backups on failed
            // retries. Hashes are only names: verify actual bytes, even on reuse.
            if (!CopyFileW(paths[i].c_str(), destination.c_str(), TRUE) &&
                GetLastError() != ERROR_FILE_EXISTS) return false;
            HANDLE saved = CreateFileW(destination.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (saved == INVALID_HANDLE_VALUE) return false;
            const bool flushed = FlushFileBuffers(saved) != FALSE;
            CloseHandle(saved);
            std::vector<unsigned char> check;
            if (!flushed || !ReadRecoveryBytes(destination.wstring(), check) || check != originals[i] ||
                !ReadRecoveryBytes(paths[i], check) || check != originals[i]) return false;
        }
        return true;
    }

    bool PrepareStartupBindings(const std::wstring& settings, BindingsSnapshot& bindings) {
        const std::wstring path = halljoy::ini::HasBundle(settings.c_str())
            ? settings : AppPaths_BindingsIni();
        return Profile_PrepareIni(path.c_str(), bindings);
    }
}

ProfileStartupResult GlobalProfiles_InitializeStartup() {
    ProfileStartupResult result;
    const auto settings = AppPaths_SettingsIni();
    const bool missingSettings = MissingFile(settings);
    const bool missingBindings = MissingFile(AppPaths_BindingsIni());
    result.firstRun = missingSettings && missingBindings;

    // Validate both halves before applying anything. A rejected profile must
    // not leave partially applied settings behind for the recovery/default path.
    const bool baseValid = SettingsIni_CanLoad(settings.c_str());
    if (baseValid) {
        GlobalProfiles_InitFromSettingsIni(settings.c_str());
        std::function<void()> apply;
        if (GlobalProfiles_Prepare(GlobalProfiles_GetActiveName(), apply) &&
            SettingsIni_Load(settings.c_str())) {
            halljoy::profile_runtime::CommitLease commit;
            if (commit) { apply(); return result; }
        }
    }

    result.recovered = !result.firstRun;
    if (result.recovered) StabilityTrace_WriteCritical(L"WARN", L"profile-recovery", L"startup.fallback",
        L"settings_valid=%d settings_missing=%d bindings_missing=%d active=%ls",
        baseValid ? 1 : 0, missingSettings ? 1 : 0, missingBindings ? 1 : 0,
        GlobalProfiles_GetActiveName().c_str());

    // Nothing is removed. Only root settings is atomically replaced, after a
    // verified backup; named profiles/layouts and original bindings stay intact.
    result.writable = PreserveStartupFiles(result.backupPath);
    std::wstring source;
    BindingsSnapshot bindings{};
    const std::wstring candidates[] = { settings, settings + L".pre-bundle.bak", settings + L".bak" };
    bool complete = false;
    for (const auto& candidate : candidates) {
        if (SettingsIni_CanLoad(candidate.c_str()) && PrepareStartupBindings(candidate, bindings)) {
            source = candidate;
            complete = true;
            break;
        }
    }
    if (!complete) {
        // Preserve independently valid settings or bindings, but do not turn a
        // broken modern bundle into unrelated stale legacy bindings.
        for (const auto& candidate : candidates) {
            if (SettingsIni_CanLoad(candidate.c_str())) { source = candidate; break; }
        }
        const auto& bindingSource = source.empty() ? settings : source;
        if (!PrepareStartupBindings(bindingSource, bindings)) bindings = {};
    }
    if (!source.empty()) SettingsIni_Load(source.c_str());
    Bindings_Apply(bindings);
    GlobalProfiles_SetActiveName(L"Default");
    GlobalProfiles_SetDirty(false);
    if (result.writable) result.writable = SettingsIni_SaveRecovered(settings.c_str());
    if (!result.writable) IniUtil_SetSessionReadOnly();
    if (result.recovered || !result.writable) StabilityTrace_WriteCritical(L"WARN", L"profile-recovery", L"startup.ready",
        L"recovered=%d writable=%d complete_profile=%d source=%ls backup=%ls",
        result.recovered ? 1 : 0, result.writable ? 1 : 0, complete ? 1 : 0,
        source.empty() ? L"defaults" : source.c_str(), result.backupPath.c_str());
    return result;
}
