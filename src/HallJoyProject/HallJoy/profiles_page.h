#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Profiles tab and the always-visible profile selector at the right end of the
// tab strip. State lives in game_profile_service.h; both views only render it.
// Design: docs/current/GAME_PROFILES_V2_2026-10-03.md.

// Posted to the keyboard page when the user asks to manage profiles.
constexpr UINT WM_APP_PROFILES_OPEN_TAB = WM_APP + 484;

HWND ProfilesPage_Create(HWND parent, HINSTANCE instance);
HWND ProfilesPage_Window();
HWND ProfileSelector_Create(HWND parent, HINSTANCE instance);
// Places the selector right of the last tab; hides it when there is no room.
void ProfileSelector_Place(HWND selector, HWND tab);
// Service state changed: refresh the selector and the page (deferred while hidden).
void ProfilesUi_Refresh();

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool ProfilesPage_Test();
#endif
