#pragma once
#include <string>

// Centralized paths near exe.
// Returned references are valid for the entire process lifetime.
const std::wstring& AppPaths_SettingsIni();
const std::wstring& AppPaths_BindingsIni();
const std::wstring& AppPaths_GlobalProfilesDir();
std::wstring AppPaths_ActiveSettingsIni();
std::wstring AppPaths_ActiveBindingsIni();
