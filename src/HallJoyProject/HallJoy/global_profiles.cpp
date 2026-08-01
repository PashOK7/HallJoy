#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "global_profiles.h"
#include "app_paths.h"
#include "ini_util.h"

namespace fs = std::filesystem;

static constexpr const wchar_t* kDefaultProfileName = L"Default";
static constexpr const wchar_t* kMainSection = L"Main";
static constexpr const wchar_t* kActiveProfileKey = L"ActiveGlobalProfile";

static std::wstring g_activeProfile = kDefaultProfileName;
static bool g_dirty = false;

static bool IEquals(const std::wstring& a, const std::wstring& b)
{
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

std::wstring GlobalProfiles_SanitizeName(const std::wstring& in)
{
    std::wstring s = in;
    while (!s.empty() && (s.front() == L' ' || s.front() == L'\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t' || s.back() == L'.')) s.pop_back();

    const wchar_t* bad = L"<>:\"/\\|?*";
    for (wchar_t& ch : s)
    {
        if (ch < 32 || wcschr(bad, ch))
            ch = L'_';
    }
    return s;
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

std::wstring GlobalProfiles_GetSettingsPath(const std::wstring& name)
{
    if (GlobalProfiles_IsDefault(name))
        return AppPaths_SettingsIni();

    fs::path p = EnsureProfilesDir() / (GlobalProfiles_SanitizeName(name) + L".settings.ini");
    return p.wstring();
}

std::wstring GlobalProfiles_GetBindingsPath(const std::wstring& name)
{
    if (GlobalProfiles_IsDefault(name))
        return AppPaths_BindingsIni();

    fs::path p = EnsureProfilesDir() / (GlobalProfiles_SanitizeName(name) + L".bindings.ini");
    return p.wstring();
}

void GlobalProfiles_List(std::vector<std::wstring>& outNames)
{
    outNames.clear();
    outNames.push_back(kDefaultProfileName);

    fs::path dir = EnsureProfilesDir();
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
    {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        auto name = e.path().filename().wstring();
        const wchar_t* suffix = L".settings.ini";
        if (name.size() <= wcslen(suffix)) continue;
        if (_wcsicmp(name.c_str() + (name.size() - wcslen(suffix)), suffix) != 0) continue;
        std::wstring base = name.substr(0, name.size() - wcslen(suffix));
        if (!base.empty() && !GlobalProfiles_IsDefault(base))
            outNames.push_back(base);
    }

    std::sort(outNames.begin() + 1, outNames.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
}

bool GlobalProfiles_Delete(const std::wstring& name)
{
    if (GlobalProfiles_IsDefault(name))
        return false;

    std::wstring s = GlobalProfiles_GetSettingsPath(name);
    std::wstring b = GlobalProfiles_GetBindingsPath(name);
    std::error_code ec1, ec2;
    bool ok1 = fs::remove(fs::path(s), ec1) || !fs::exists(fs::path(s), ec1);
    bool ok2 = fs::remove(fs::path(b), ec2) || !fs::exists(fs::path(b), ec2);
    return ok1 && ok2;
}
