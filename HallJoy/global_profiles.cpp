#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "global_profiles.h"
#include "app_paths.h"

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

void GlobalProfiles_SaveActiveToSettingsIni(const wchar_t* settingsIniPath)
{
    if (!settingsIniPath) return;
    WritePrivateProfileStringW(kMainSection, kActiveProfileKey, g_activeProfile.c_str(), settingsIniPath);
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
