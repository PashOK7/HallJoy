#pragma once
#include <windows.h>

bool SettingsIni_Load(const wchar_t* path);
bool SettingsIni_Save(const wchar_t* path);
bool SettingsIni_LoadProfile(const wchar_t* path);
bool SettingsIni_SaveProfile(const wchar_t* path);
bool SettingsIni_SaveOverlay(const wchar_t* path);
// Atomic update of window geometry only, preserving Default and named profiles.
bool SettingsIni_SaveWindow(const wchar_t* path);

#include <functional>
// The prepared closure owns all profile data. Invoke once under CommitLease.
bool SettingsIni_PrepareProfile(const wchar_t* path, std::function<void()>& apply);
