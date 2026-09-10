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
