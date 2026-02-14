#include "app_paths.h"
#include "win_util.h"
#include "global_profiles.h"

const std::wstring& AppPaths_SettingsIni()
{
    static std::wstring p = WinUtil_BuildPathNearExe(L"settings.ini");
    return p;
}

const std::wstring& AppPaths_BindingsIni()
{
    static std::wstring p = WinUtil_BuildPathNearExe(L"bindings.ini");
    return p;
}

const std::wstring& AppPaths_GlobalProfilesDir()
{
    static std::wstring p = WinUtil_BuildPathNearExe(L"GlobalProfiles");
    return p;
}

std::wstring AppPaths_ActiveSettingsIni()
{
    return GlobalProfiles_GetSettingsPath(GlobalProfiles_GetActiveName());
}

std::wstring AppPaths_ActiveBindingsIni()
{
    return GlobalProfiles_GetBindingsPath(GlobalProfiles_GetActiveName());
}
