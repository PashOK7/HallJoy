#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// PremiumCombo: custom combo box without WC_COMBOBOX / LISTBOX.
// Draws itself and hosts its own popup list.
//
// Notifications:
//
// 1) Selection changed:
//    Sends WM_COMMAND to parent:
//      HIWORD(wParam) = CBN_SELCHANGE
//      LOWORD(wParam) = controlId
//      lParam         = (LPARAM)hWndCombo
//
// 2) Item buttons inside dropdown rows:
//    Sends MsgItemButton():
//      LOWORD(wParam) = item index
//      HIWORD(wParam) = (int)ItemButtonKind
//      lParam         = (LPARAM)hWndCombo
//
//    PREMIUM behavior:
//      - ItemButtonKind::Rename does NOT send MsgItemButton().
//        It starts inline rename editor.
//      - When rename/inline edit is committed (Enter / focus loss), sends MsgItemTextCommit().
//        The text is retrieved via ConsumeCommittedText().
//
// 3) Extra icon click in closed state (left of arrow):
//    Sends MsgExtraIcon():
//      LOWORD(wParam) = (int)ExtraIconKind
//      HIWORD(wParam) = controlId
//      lParam         = (LPARAM)hWndCombo
//
// 4) Placeholder (NEW):
//    If current selection == -1, closed state shows placeholder text
//    (e.g. "Custom"). This is NOT a list item.

namespace PremiumCombo
{
    enum class ItemButtonKind : int
    {
        None = 0,
        Delete = 1,
        Rename = 2,
    };

    enum class ExtraIconKind : int
    {
        None = 0,
        Save = 1,
    };

    inline UINT MsgItemButton()
    {
        static UINT msg = RegisterWindowMessageW(L"PremiumCombo_ItemButton");
        return msg;
    }

    inline UINT MsgExtraIcon()
    {
        static UINT msg = RegisterWindowMessageW(L"PremiumCombo_ExtraIcon");
        return msg;
    }

    inline UINT MsgItemTextCommit()
    {
        static UINT msg = RegisterWindowMessageW(L"PremiumCombo_ItemTextCommit");
        return msg;
    }

    // Create control.
    // style: typically WS_CHILD|WS_VISIBLE|WS_TABSTOP etc. (0 => default)
    HWND Create(HWND parent, HINSTANCE hInst,
        int x, int y, int w, int h,
        int controlId,
        DWORD style = 0);

    void SetFont(HWND hCombo, HFONT hFont, bool redraw = true);

    void Clear(HWND hCombo);

    int  AddString(HWND hCombo, const wchar_t* text);

    int  GetCount(HWND hCombo);

    // Current selection:
    //  -1 => no selection (placeholder may be shown)
    int  GetCurSel(HWND hCombo);

    // Set selection index:
    //  -1 => no selection (NEW)
    //  0..count-1 => selected item
    void SetCurSel(HWND hCombo, int idx, bool notify = false);

    // Placeholder shown when GetCurSel()==-1 (NEW).
    void SetPlaceholderText(HWND hCombo, const wchar_t* text);

    int  GetLBText(HWND hCombo, int idx, wchar_t* out, int outCch);
    int  GetLBTextLen(HWND hCombo, int idx);

    void SetDropMaxVisible(HWND hCombo, int count);
    void SetItemHeightPx(HWND hCombo, int itemHeightPx);

    void ShowDropDown(HWND hCombo, bool show);
    bool GetDroppedState(HWND hCombo);

    void SetEnabled(HWND hCombo, bool enabled);
    void Invalidate(HWND hCombo);

    void SetExtraIcon(HWND hCombo, ExtraIconKind kind);

    void SetItemButtonKind(HWND hCombo, int idx, ItemButtonKind kind);

    void ResetVisualState(HWND hCombo);

    // Inline edit API
    bool IsEditingItem(HWND hCombo);
    bool BeginInlineEdit(HWND hCombo, int idx, bool openDropDownIfNeeded = true);
    bool BeginInlineEditSelected(HWND hCombo, bool openDropDownIfNeeded = true);

    int ConsumeCommittedText(HWND hCombo, wchar_t* out, int outCch);
}