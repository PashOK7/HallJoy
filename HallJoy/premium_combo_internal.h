// premium_combo_internal.h
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include <cwctype>

#include "win_util.h"

// public enums/messages for item buttons + extra icon
#include "premium_combo.h"

// Internal implementation details for PremiumCombo.
// This header is NOT part of the public API.

namespace PremiumComboInternal
{
    // ---------------------------------------------------------------------
    // Classes
    // ---------------------------------------------------------------------
    inline constexpr const wchar_t* PC_COMBO_CLASS = L"PremiumCombo_Control";
    inline constexpr const wchar_t* PC_POPUP_CLASS = L"PremiumCombo_Popup";

    // ---------------------------------------------------------------------
    // DPI helper
    // ---------------------------------------------------------------------
    inline int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

    // ---------------------------------------------------------------------
    // Small color helpers
    // ---------------------------------------------------------------------
    inline int Clamp255(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : v); }

    inline COLORREF Brighten(COLORREF c, int add)
    {
        return RGB(
            Clamp255((int)GetRValue(c) + add),
            Clamp255((int)GetGValue(c) + add),
            Clamp255((int)GetBValue(c) + add));
    }

    // ---------------------------------------------------------------------
    // Animation helpers
    // ---------------------------------------------------------------------
    inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

    inline float EaseOutCubic(float t)
    {
        t = Clamp01(t);
        float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    inline constexpr UINT_PTR PC_TIMER_ANIM = 1;
    inline constexpr UINT     PC_ANIM_TICK_MS = 15;

    inline constexpr DWORD PC_POPUP_OPEN_MS = 140;
    inline constexpr DWORD PC_POPUP_CLOSE_MS = 110;

    inline constexpr BYTE PC_POPUP_ALPHA_MIN = 0;
    inline constexpr BYTE PC_POPUP_ALPHA_MAX = 255;

    // NEW: hover animations
    inline constexpr DWORD PC_HOVER_FADE_IN_MS = 110;
    inline constexpr DWORD PC_HOVER_FADE_OUT_MS = 140;

    // NEW: extra icon show/hide animation
    inline constexpr DWORD PC_EXTRAICON_SHOW_MS = 110;
    inline constexpr DWORD PC_EXTRAICON_HIDE_MS = 140;

    enum class PopupAnimMode : int
    {
        None = 0,
        Opening,
        Closing
    };

    // ---------------------------------------------------------------------
    // Layout constants (DPI-scaled via S())
    // ---------------------------------------------------------------------
    // Extra icon (closed combo, left of arrow)
    inline constexpr int PC_EXTRAICON_PAD_X = 6;     // padding from arrow separator
    inline constexpr int PC_EXTRAICON_SIZE = 14;     // icon square size (will be scaled)
    inline constexpr int PC_EXTRAICON_GAP = 6;       // gap between text and icon

    // Popup item buttons (right side of row, e.g. Rename + Delete)
    inline constexpr int PC_ITEMBTN_PAD_R = 6;       // padding from right edge
    inline constexpr int PC_ITEMBTN_SIZE = 16;       // button hit box (square)
    inline constexpr int PC_ITEMBTN_GAP = 6;         // gap between text and FIRST(leftmost) button
    inline constexpr int PC_ITEMBTN_GAP_BETWEEN = 6; // gap between buttons (Rename <-> Delete)

    // Inline edit padding inside row text rect (popup)
    inline constexpr int PC_EDIT_PAD_X = 4;
    inline constexpr int PC_EDIT_PAD_Y = 2;

    // ---------------------------------------------------------------------
    // Item button mask helpers (per row)
    // ---------------------------------------------------------------------
    // We allow multiple buttons per row (e.g. Rename + Delete).
    using ItemBtnMask = uint8_t;

    inline constexpr ItemBtnMask BTN_NONE = 0;
    inline constexpr ItemBtnMask BTN_DELETE = 1 << 0;
    inline constexpr ItemBtnMask BTN_RENAME = 1 << 1;

    inline ItemBtnMask KindToMask(PremiumCombo::ItemButtonKind k)
    {
        switch (k)
        {
        case PremiumCombo::ItemButtonKind::Delete: return BTN_DELETE;
        case PremiumCombo::ItemButtonKind::Rename: return BTN_RENAME;
        default: return BTN_NONE;
        }
    }

    inline bool MaskHas(ItemBtnMask m, PremiumCombo::ItemButtonKind k)
    {
        return (m & KindToMask(k)) != 0;
    }

    // ---------------------------------------------------------------------
    // State
    // ---------------------------------------------------------------------
    struct State
    {
        HWND hwnd = nullptr;
        HWND parent = nullptr;
        int  controlId = 0;

        HFONT font = nullptr;

        std::vector<std::wstring> items;
        int curSel = -1;

        // NEW: placeholder drawn when curSel == -1 (closed state only)
        std::wstring placeholderText;

        // Dropdown behavior
        int dropMaxVisible = 8;
        int itemHeightPx = 0; // 0 => auto from combo height

        // Runtime
        bool enabled = true;
        bool dropped = false;

        // Popup
        HWND hwndPopup = nullptr;      // current popup (if open OR closing animation)
        RECT popupTargetRect{};        // final rect (screen coords)
        RECT popupStartRect{};         // animated-from rect (screen coords)
        bool popupOpensUp = false;

        // Dropdown hover / selection while dropped
        int hotIndex = -1;

        // Scroll
        int scrollTop = 0;             // index of first visible item in popup

        // Capture-based dropdown (robust close on outside click)
        bool captureActive = false;

        // Hover state (logical)
        bool hovered = false;
        bool arrowHot = false;

        // Extra icon hover (closed state)
        bool extraIconHot = false;

        // Hover animations (0..1)
        bool  hoverAnimRunning = false;
        DWORD hoverAnimStartTick = 0;
        DWORD hoverAnimDurationMs = 0;
        float hoverFrom = 0.0f;
        float hoverTo = 0.0f;
        float hoverT = 0.0f;

        bool  arrowHotAnimRunning = false;
        DWORD arrowHotAnimStartTick = 0;
        DWORD arrowHotAnimDurationMs = 0;
        float arrowHotFrom = 0.0f;
        float arrowHotTo = 0.0f;
        float arrowHotT = 0.0f;

        // Type-to-search
        std::wstring typeBuf;
        DWORD lastTypeTick = 0;

        // Popup animation
        PopupAnimMode popupAnim = PopupAnimMode::None;
        DWORD popupAnimStartTick = 0;
        DWORD popupAnimDurationMs = 0;
        BYTE  popupAlphaFrom = 0;
        BYTE  popupAlphaTo = 255;

        // Arrow rotation animation (0 = down, 1 = up)
        bool  arrowAnimRunning = false;
        DWORD arrowAnimStartTick = 0;
        DWORD arrowAnimDurationMs = 0;
        float arrowFrom = 0.0f;
        float arrowTo = 0.0f;
        float arrowT = 0.0f;

        // -----------------------------------------------------------------
        // Profiles/actions UI
        // -----------------------------------------------------------------

        // Extra icon target (closed state)
        PremiumCombo::ExtraIconKind extraIcon = PremiumCombo::ExtraIconKind::None;

        // Extra icon currently drawn (kept during hide animation)
        PremiumCombo::ExtraIconKind extraIconDraw = PremiumCombo::ExtraIconKind::None;

        // Extra icon show/hide animation (0..1)
        bool  extraIconAnimRunning = false;
        DWORD extraIconAnimStartTick = 0;
        DWORD extraIconAnimDurationMs = 0;
        float extraIconFrom = 0.0f;
        float extraIconTo = 0.0f;
        float extraIconT = 0.0f;

        // Per-item buttons shown in popup rows (bitmask; same size as items).
        std::vector<ItemBtnMask> itemBtnMask;

        // Popup button hover state (which row's button is hot)
        int hotBtnIndex = -1;
        PremiumCombo::ItemButtonKind hotBtnKind = PremiumCombo::ItemButtonKind::None;

        // -----------------------------------------------------------------
        // Inline edit (premium rename)
        // -----------------------------------------------------------------
        HWND hwndEdit = nullptr;     // EDIT child hosted inside popup-host
        int  editIndex = -1;         // which item is being edited
        RECT editRect{};             // popup client coords used for positioning

        // Committed text buffering (consumed by PremiumCombo::ConsumeCommittedText)
        std::wstring committedText;
        int committedIndex = -1;
        PremiumCombo::ItemButtonKind committedKind = PremiumCombo::ItemButtonKind::None;

        // -----------------------------------------------------------------
        // Selection change animation (closed state "scroll" to new item)
        // -----------------------------------------------------------------
        bool  selAnimInitialized = false;   // first SetCurSel snaps, no animation
        bool  selAnimRunning = false;
        DWORD selAnimStartTick = 0;
        DWORD selAnimDurationMs = 140;

        int   selAnimFromSel = -1;          // previous selection index (can be -1 for placeholder)
        int   selAnimToSel = -1;            // new selection index (can be -1 for placeholder)
        int   selAnimDir = 1;               // +1 or -1 (down/up)
        float selAnimDistPx = 0.0f;         // how far text slides (pixels)
    };

    // ---------------------------------------------------------------------
    // State access (stored in GWLP_USERDATA of combo window)
    // ---------------------------------------------------------------------
    State* Get(HWND hWnd);
    void   Set(HWND hWnd, State* st);

    HFONT  GetFont(State* st);
    void   NotifySelChange(State* st);

    // ---------------------------------------------------------------------
    // Geometry / list metrics
    // ---------------------------------------------------------------------
    int  GetItemHeightPx(HWND hwndCombo, State* st);
    int  GetVisibleRows(State* st);
    int  GetMaxScrollTop(State* st);
    void ClampScroll(State* st);
    void EnsureIndexVisible(State* st, int idx);
    void ClampPopupToMonitor(RECT& r);

    // ---------------------------------------------------------------------
    // Extra icon / item button geometry + hit-testing
    // ---------------------------------------------------------------------
    RECT GetExtraIconRect(State* st); // combo client coords; empty if not shown
    bool HitTestExtraIcon(State* st, POINT ptClient);

    RECT GetPopupItemButtonRect(State* st, int idx, PremiumCombo::ItemButtonKind kind);

    bool HitTestPopupItemButtonFromScreen(State* st, POINT ptScreen,
        int& outIdx, PremiumCombo::ItemButtonKind& outKind, bool& outInsidePopup);

    RECT GetPopupItemTextRect(State* st, int idx);

    // ---------------------------------------------------------------------
    // Inline edit control helpers
    // ---------------------------------------------------------------------
    bool IsInlineEditing(State* st);

    void BeginInlineEdit(State* st, int idx);
    void EndInlineEdit(State* st, bool commit);
    void UpdateInlineEditRect(State* st);

    void StoreCommittedText(State* st, int idx, PremiumCombo::ItemButtonKind kind, const std::wstring& txt);

    // ---------------------------------------------------------------------
    // Type-to-search
    // ---------------------------------------------------------------------
    void ResetTypeSearch(State* st);
    void TypeSearchApply(State* st, wchar_t ch, bool droppedMode);

    // ---------------------------------------------------------------------
    // Popup hit-test
    // ---------------------------------------------------------------------
    int HitTestPopupIndexFromScreen(State* st, POINT ptScreen, bool& outInsidePopup);

    // ---------------------------------------------------------------------
    // Painting
    // ---------------------------------------------------------------------
    void PaintCombo(HWND hwndCombo, State* st, HDC hdc);
    void PaintPopup(HWND hwndPopup, State* st, HDC hdc);

    // ---------------------------------------------------------------------
    // Popup open/close + input helpers
    // ---------------------------------------------------------------------
    void OpenDropDown(State* st);
    void CloseDropDown(State* st, bool keepFocusOnCombo);

    void DropdownMouseMove(State* st);
    void DropdownClick(State* st);

    void MoveHot(State* st, int delta);
    void PageHot(State* st, int pages);
    void HotToEdge(State* st, bool toEnd);

    // ---------------------------------------------------------------------
    // Hover hit test
    // ---------------------------------------------------------------------
    void UpdateHoverState(HWND hWnd, State* st, LPARAM lParam);

    // ---------------------------------------------------------------------
    // Animation control (timer lives on combo window)
    // ---------------------------------------------------------------------
    void EnsureAnimTimer(State* st);
    void StopAnimTimerIfIdle(State* st);

    void PopupStartAnim(State* st, PopupAnimMode mode);
    void TickPopupAndArrowAnim(State* st);

    // hover animation starters
    void StartHoverAnim(State* st, float to, DWORD durMs);
    void StartArrowHotAnim(State* st, float to, DWORD durMs);

    // ---------------------------------------------------------------------
    // Window procs (implemented in core module)
    // ---------------------------------------------------------------------
    LRESULT CALLBACK ComboProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK PopupProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Register both window classes once.
    void RegisterOnce(HINSTANCE hInst);
}