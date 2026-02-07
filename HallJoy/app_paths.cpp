#include "app_paths.h"
#include "win_util.h"

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