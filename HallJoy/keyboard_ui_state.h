#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <vector>
#include <cstdint>

// Internal shared state for keyboard UI modules.
// Defined in keyboard_ui.cpp, used by keyboard_page_main.cpp and others.

extern std::array<HWND, 256> g_btnByHid;
extern std::vector<uint16_t> g_hids;

extern uint16_t g_selectedHid;
extern uint16_t g_dragHoverHid;

extern HWND g_hSubTab;
extern HWND g_hPageRemap;
extern HWND g_hPageConfig;
extern HWND g_hPageTester;
extern int  g_activeSubTab;