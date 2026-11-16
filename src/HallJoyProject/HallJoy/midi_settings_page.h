#pragma once
#include <windows.h>
// Main-page messages; input/runtime owners never depend on this UI.
inline constexpr UINT WM_APP_MIDI_SETTINGS_CHANGED = WM_APP + 392;
inline constexpr UINT WM_APP_OPEN_MIDI_SETTINGS = WM_APP + 393;
HWND MidiSettingsPage_Create(HWND parent, HINSTANCE instance);
void MidiSettingsPage_Tick(HWND page);
