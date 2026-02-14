#pragma once
#include <windows.h>

bool SettingsIni_Load(const wchar_t* path);
bool SettingsIni_Save(const wchar_t* path);
bool SettingsIni_LoadProfile(const wchar_t* path);
bool SettingsIni_SaveProfile(const wchar_t* path);
