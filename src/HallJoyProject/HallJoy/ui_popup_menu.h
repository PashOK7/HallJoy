#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

// HallJoy's own context menu: the dark PremiumCombo drop-down style (control
// background, border, accent hover row), optional icons and a muted detail
// text, drop shadow and rounded corners where Windows supports them.
// Replaces white system menus (TrackPopupMenu) in the custom pages.
namespace halljoy::ui_menu {

struct Item {
    UINT id = 0;                 // returned when chosen; 0 for separators
    std::wstring text;
    std::wstring detail;         // optional, muted, right-aligned
    HICON icon = nullptr;        // optional 16 px icon (owned by the caller)
    bool enabled = true;
    bool separator = false;
};

inline Item Separator() { Item i; i.separator = true; return i; }

// Modal, like TrackPopupMenu(TPM_RETURNCMD): opens below `anchor` (screen
// coordinates; above it when there is no room), returns the chosen id or 0.
// Mouse only plus Esc: game keys held while it is open cannot pick an item.
UINT Track(HWND owner, const std::vector<Item>& items, POINT anchor);

} // namespace halljoy::ui_menu
