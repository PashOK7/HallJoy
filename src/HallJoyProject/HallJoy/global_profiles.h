#pragma once
#include <string>
#include <vector>

// Active profile name is persisted in settings.ini [Main] ActiveGlobalProfile.
// "Default" means using base settings.ini + bindings.ini files.
void GlobalProfiles_InitFromSettingsIni(const wchar_t* settingsIniPath);
bool GlobalProfiles_SaveActiveToSettingsIni(const wchar_t* settingsIniPath);

const std::wstring& GlobalProfiles_GetActiveName();
void GlobalProfiles_SetActiveName(const std::wstring& name);
bool GlobalProfiles_IsDirty();
void GlobalProfiles_SetDirty(bool dirty);

std::wstring GlobalProfiles_SanitizeName(const std::wstring& in);
bool GlobalProfiles_IsDefault(const std::wstring& name);
bool GlobalProfiles_Delete(const std::wstring& name);

void GlobalProfiles_List(std::vector<std::wstring>& outNames);
std::wstring GlobalProfiles_GetSettingsPath(const std::wstring& name);
std::wstring GlobalProfiles_GetBindingsPath(const std::wstring& name);

#include <functional>
bool GlobalProfiles_Save(const std::wstring& name);
bool GlobalProfiles_Prepare(const std::wstring& name, std::function<void()>& apply);
bool GlobalProfiles_Load(const std::wstring& name);
bool GlobalProfiles_Switch(const std::wstring& name);
