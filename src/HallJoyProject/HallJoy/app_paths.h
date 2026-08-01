#pragma once

#include <string>

enum class AppDataMode
{
    LocalAppData,
    Portable,
    SimulatorOverride,
};

// Resolves the writable root and completes any legacy EXE-directory migration.
// Production defaults to %LOCALAPPDATA%\HallJoy. Portable mode is enabled only
// by HallJoy.portable beside the executable and a successful write probe.
bool AppPaths_Initialize();
AppDataMode AppPaths_Mode();
const wchar_t* AppPaths_ModeName();

// Returned references are valid for the entire process lifetime.
const std::wstring& AppPaths_DataRoot();
const std::wstring& AppPaths_LegacyDataRoot();
const std::wstring& AppPaths_SettingsIni();
const std::wstring& AppPaths_BindingsIni();
const std::wstring& AppPaths_GlobalProfilesDir();
const std::wstring& AppPaths_LayoutsDir();
const std::wstring& AppPaths_CurvePresetsDir();
std::wstring AppPaths_ActiveSettingsIni();
std::wstring AppPaths_ActiveBindingsIni();

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool AppPaths_RunStoragePolicySelfTest();
#endif
