// keyboard_subpages.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>

#include "keyboard_ui_internal.h"
#include "keyboard_keysettings_panel.h"
#include "keyboard_keysettings_panel_internal.h"

#include "backend.h"
#include "gamepad_render.h"
#include "ui_theme.h"
#include "settings.h"
#include "realtime_loop.h"
#include "win_util.h"
#include "keyboard_profiles.h"
#include "premium_combo.h"
#include "keyboard_layout.h"

using namespace Gdiplus;
namespace fs = std::filesystem;

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_APPLY_TIMING = WM_APP + 2;
static constexpr UINT WM_APP_PROFILE_BEGIN_CREATE = WM_APP + 120;

static constexpr UINT_PTR TOAST_TIMER_ID = 8811;
static constexpr DWORD    TOAST_SHOW_MS = 1600;
static constexpr const wchar_t* CONFIG_SCROLLY_PROP = L"DD_ConfigScrollY";
static constexpr bool kEnableSnappyDebug = false; // set true for temporary snappy toggle diagnostics

static constexpr int ID_SNAPPY = 7003;
static constexpr int ID_BLOCK_BOUND_KEYS = 7004;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static Color Gp(COLORREF c, BYTE a = 255);

static void SnappyDebugLog(const wchar_t* stage, HWND hBtn, int extraA = -1, int extraB = -1)
{
#if defined(_DEBUG)
    if (!kEnableSnappyDebug) return;

    int check = -1;
    if (hBtn && IsWindow(hBtn))
        check = (int)SendMessageW(hBtn, BM_GETCHECK, 0, 0);

    int setting = Settings_GetSnappyJoystick() ? 1 : 0;

    wchar_t buf[320]{};
    swprintf_s(buf, L"[SnappyDbg] %s hwnd=%p check=%d setting=%d a=%d b=%d\n",
        stage ? stage : L"(null)", (void*)hBtn, check, setting, extraA, extraB);
    OutputDebugStringW(buf);
#else
    (void)stage; (void)hBtn; (void)extraA; (void)extraB;
#endif
}

// ---------------- Double-buffer helpers ----------------
static void BeginDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC& outMemDC, HBITMAP& outBmp, HGDIOBJ& outOldBmp)
{
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);
    outMemDC = CreateCompatibleDC(hdc);
    outBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    outOldBmp = SelectObject(outMemDC, outBmp);
    FillRect(outMemDC, &rc, UiTheme::Brush_PanelBg());
}

static void EndDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    HDC hdc = ps.hdc;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

// ============================================================================
// Gamepad Tester page (DPI-scaled)
// ============================================================================
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        XUSB_REPORT r = Backend_GetLastReport();
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HGDIOBJ oldFont = SelectObject(memDC, font);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, UiTheme::Color_Text());

        const int x0 = S(hWnd, 12);
        int y = S(hWnd, 12);

        const int lineH = S(hWnd, 18);
        const int gapAfterText = S(hWnd, 10);

        auto textLine = [&](const std::wstring& t)
            {
                TextOutW(memDC, x0, y, t.c_str(), (int)t.size());
                y += lineH;
            };

        wchar_t buf[256]{};

        swprintf_s(buf, L"LX: %6d   LY: %6d", (int)r.sThumbLX, (int)r.sThumbLY);
        textLine(buf);

        const int barW = S(hWnd, 360);
        const int barH = S(hWnd, 20);
        const int barGapX = S(hWnd, 20);
        RECT barLX{ x0, y, x0 + barW, y + barH };
        RECT barLY{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + barH };
        GamepadRender_DrawAxisBarCentered(memDC, barLX, r.sThumbLX);
        GamepadRender_DrawAxisBarCentered(memDC, barLY, r.sThumbLY);
        y += barH + S(hWnd, 8);

        swprintf_s(buf, L"RX: %6d   RY: %6d", (int)r.sThumbRX, (int)r.sThumbRY);
        textLine(buf);

        RECT barRX{ x0, y, x0 + barW, y + barH };
        RECT barRY{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + barH };
        GamepadRender_DrawAxisBarCentered(memDC, barRX, r.sThumbRX);
        GamepadRender_DrawAxisBarCentered(memDC, barRY, r.sThumbRY);
        y += barH + S(hWnd, 8);

        swprintf_s(buf, L"LT: %3u   RT: %3u", (unsigned)r.bLeftTrigger, (unsigned)r.bRightTrigger);
        textLine(buf);

        const int trigH = S(hWnd, 18);
        RECT barLT{ x0, y, x0 + barW, y + trigH };
        RECT barRT{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + trigH };
        GamepadRender_DrawTriggerBar01(memDC, barLT, r.bLeftTrigger);
        GamepadRender_DrawTriggerBar01(memDC, barRT, r.bRightTrigger);
        y += trigH + S(hWnd, 8);

        y += gapAfterText;
        textLine(L"Buttons: " + GamepadRender_ButtonsToString(r.wButtons));

        SelectObject(memDC, oldFont);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Keyboard Layout page (preset picker + visual editor)
// ============================================================================
struct LayoutPageState
{
    HWND lblPreset = nullptr;
    HWND cmbPreset = nullptr;
    HWND btnAdd = nullptr;
    HWND btnDelete = nullptr;
    HWND btnReset = nullptr;
    HWND btnSave = nullptr;
    HWND btnUniformSpacing = nullptr;
    HWND lblUniformGap = nullptr;
    HWND edtUniformGap = nullptr;
    HWND lblLabel = nullptr;
    HWND edtLabel = nullptr;
    HWND btnApplyLabel = nullptr;
    HWND btnBindKey = nullptr;
    HWND lblPos = nullptr;
    HWND edtPos = nullptr;
    HWND lblWidth = nullptr;
    HWND edtWidth = nullptr;
    HWND lblHeight = nullptr;
    HWND edtHeight = nullptr;
    HWND lblBindState = nullptr;
    HWND lblKeys = nullptr;
    HWND lstKeys = nullptr;
    HWND lblHint = nullptr;

    int selectedIdx = -1;
    bool dragging = false;
    bool dirty = false;
    bool hasUnsaved = false;
    bool bindArmed = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    RECT canvasRc{};
    int editingPresetIdx = 0;
    std::vector<KeyDef> draftKeys;
    std::vector<std::wstring> draftLabels;
    uint32_t previewHash = 0;
    bool uniformSpacingEnabled = false;
    int uniformSpacingGap = 8;
};

static constexpr int ID_LAYOUT_PRESET = 8111;
static constexpr int ID_LAYOUT_RESET = 8112;
static constexpr int ID_LAYOUT_KEYS = 8113;
static constexpr int ID_LAYOUT_SAVE = 8114;
static constexpr int ID_LAYOUT_ADD = 8115;
static constexpr int ID_LAYOUT_DELETE = 8116;
static constexpr int ID_LAYOUT_LABEL_EDIT = 8117;
static constexpr int ID_LAYOUT_LABEL_APPLY = 8118;
static constexpr int ID_LAYOUT_BIND_KEY = 8119;
static constexpr int ID_LAYOUT_UNIFORM_SPACING = 8120;
static constexpr int ID_LAYOUT_POS_EDIT = 8122;
static constexpr int ID_LAYOUT_WIDTH_EDIT = 8123;
static constexpr int ID_LAYOUT_UNIFORM_GAP_EDIT = 8124;
static constexpr int ID_LAYOUT_HEIGHT_EDIT = 8125;
static constexpr UINT_PTR ID_LAYOUT_UI_TIMER = 8121;

static bool Layout_NudgeSelectedKey(HWND hWnd, LayoutPageState* st, int dRow, int dX, int dW);
static void Layout_SetUnsaved(LayoutPageState* st, bool on);
static void Layout_RefreshKeyList(HWND hWnd, LayoutPageState* st);
static void Layout_NotifyMainPage(HWND hWnd);
static bool Layout_LoadDraftFromPreset(HWND hWnd, LayoutPageState* st, int presetIdx, bool clearUnsaved);
static void Layout_UpdateBindStateOnly(LayoutPageState* st);
static uint32_t Layout_ComputePreviewHash(LayoutPageState* st);
static void Layout_ApplyGeometryFromEdits(HWND hWnd, LayoutPageState* st, bool applyPos, bool applyWidth, bool applyHeight);
static void Layout_BuildDisplayXMap(const LayoutPageState* st, std::vector<int>& outDisplayX);
static void Layout_UpdateUniformSpacingButton(LayoutPageState* st);
static void Layout_ApplyUniformGapFromEdit(HWND hWnd, LayoutPageState* st);
static void Layout_UpdateHintText(LayoutPageState* st);
static bool Layout_BakeUniformSpacingIntoDraft(LayoutPageState* st);

static void Layout_SetWindowTextIfChanged(HWND hWnd, const wchar_t* text)
{
    if (!hWnd || !IsWindow(hWnd)) return;
    const wchar_t* target = text ? text : L"";
    wchar_t cur[256]{};
    GetWindowTextW(hWnd, cur, (int)(sizeof(cur) / sizeof(cur[0])));
    if (wcscmp(cur, target) != 0)
        SetWindowTextW(hWnd, target);
}

static void Layout_UpdateUniformSpacingButton(LayoutPageState* st)
{
    if (!st || !st->btnUniformSpacing) return;
    Layout_SetWindowTextIfChanged(
        st->btnUniformSpacing,
        st->uniformSpacingEnabled ? L"Uniform Spacing: ON" : L"Uniform Spacing: OFF");
}

static void Layout_UpdateHintText(LayoutPageState* st)
{
    if (!st || !st->lblHint) return;
    if (st->uniformSpacingEnabled)
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Select a key and drag to move it.\nMouse wheel changes width (Shift = x4).\nUniform Spacing: drag reorders keys in row. Click Save Changes to apply.");
    }
    else
    {
        Layout_SetWindowTextIfChanged(st->lblHint,
            L"Select a key and drag to move it.\nMouse wheel changes width (Shift = x4).\nClick Save Changes to apply.");
    }
}

static bool Layout_BakeUniformSpacingIntoDraft(LayoutPageState* st)
{
    if (!st || !st->uniformSpacingEnabled) return false;
    if (st->draftKeys.empty()) return false;

    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    if (displayX.size() < st->draftKeys.size()) return false;

    bool changed = false;
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
    {
        int nx = std::clamp(displayX[i], 0, 4000);
        if (st->draftKeys[i].x != nx)
        {
            st->draftKeys[i].x = nx;
            changed = true;
        }
    }
    return changed;
}

static void Layout_ApplyUniformGapFromEdit(HWND hWnd, LayoutPageState* st)
{
    if (!st || !st->edtUniformGap) return;

    wchar_t b[32]{};
    GetWindowTextW(st->edtUniformGap, b, (int)(sizeof(b) / sizeof(b[0])));
    int v = 0;
    if (swscanf_s(b, L"%d", &v) != 1)
        v = st->uniformSpacingGap;
    v = std::clamp(v, 0, 120);

    if (v != st->uniformSpacingGap)
    {
        st->uniformSpacingGap = v;
        Layout_SetUnsaved(st, true);
        if (st->uniformSpacingEnabled)
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
}

static HWND ResolveAppMainWindow(HWND hWnd)
{
    if (!hWnd) return nullptr;

    auto isMainAppWindow = [](HWND w) -> bool
    {
        if (!w || !IsWindow(w)) return false;

        wchar_t cls[128]{};
        GetClassNameW(w, cls, (int)(sizeof(cls) / sizeof(cls[0])));
        if (_wcsicmp(cls, L"WootingVigemGui") == 0)
            return true;

        HWND page = FindWindowExW(w, nullptr, L"PageMainClass", nullptr);
        return (page != nullptr);
    };

    HWND rootOwner = GetAncestor(hWnd, GA_ROOTOWNER);
    if (isMainAppWindow(rootOwner))
        return rootOwner;

    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (isMainAppWindow(root))
        return root;

    // Detached editor fallback: locate main app window by class name.
    HWND byClass = FindWindowW(L"WootingVigemGui", nullptr);
    if (isMainAppWindow(byClass))
        return byClass;

    return nullptr;
}

static void Layout_RequestSave(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Layout_RebindDraftLabels(LayoutPageState* st)
{
    if (!st) return;
    if (st->draftLabels.size() < st->draftKeys.size())
        st->draftLabels.resize(st->draftKeys.size());
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
        st->draftKeys[i].label = st->draftLabels[i].c_str();
}

static int Layout_DraftCount(const LayoutPageState* st)
{
    return st ? (int)st->draftKeys.size() : 0;
}

static bool Layout_DraftGet(const LayoutPageState* st, int idx, KeyDef& out)
{
    if (!st) return false;
    if (idx < 0 || idx >= (int)st->draftKeys.size()) return false;
    out = st->draftKeys[idx];
    return true;
}

static bool Layout_LoadDraftFromPreset(HWND hWnd, LayoutPageState* st, int presetIdx, bool clearUnsaved)
{
    if (!st) return false;

    std::vector<KeyDef> keys;
    std::vector<std::wstring> labels;
    bool uniformSpacing = false;
    int uniformGap = 8;
    if (!KeyboardLayout_GetPresetSnapshot(presetIdx, keys, labels, &uniformSpacing, &uniformGap))
        return false;

    st->editingPresetIdx = presetIdx;
    st->draftKeys = std::move(keys);
    st->draftLabels = std::move(labels);
    st->uniformSpacingEnabled = uniformSpacing;
    st->uniformSpacingGap = std::clamp(uniformGap, 0, 120);
    Layout_RebindDraftLabels(st);
    Layout_UpdateUniformSpacingButton(st);
    Layout_UpdateHintText(st);

    st->selectedIdx = -1;
    Layout_RefreshKeyList(hWnd, st);
    st->previewHash = Layout_ComputePreviewHash(st);
    if (clearUnsaved)
        Layout_SetUnsaved(st, false);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Layout_SetUnsaved(LayoutPageState* st, bool on)
{
    if (!st) return;
    st->hasUnsaved = on;
    if (st->btnSave && IsWindow(st->btnSave))
        EnableWindow(st->btnSave, on ? TRUE : FALSE);
    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, st->selectedIdx >= 0 ? TRUE : FALSE);
}

static void Layout_UpdateMetaControls(LayoutPageState* st)
{
    if (!st) return;
    bool hasSel = (st->selectedIdx >= 0);
    bool labelHasFocus = (st->edtLabel && GetFocus() == st->edtLabel);
    bool posHasFocus = (st->edtPos && GetFocus() == st->edtPos);
    bool widthHasFocus = (st->edtWidth && GetFocus() == st->edtWidth);
    bool heightHasFocus = (st->edtHeight && GetFocus() == st->edtHeight);
    bool uniformGapHasFocus = (st->edtUniformGap && GetFocus() == st->edtUniformGap);

    if (st->btnDelete && IsWindow(st->btnDelete))
        EnableWindow(st->btnDelete, hasSel ? TRUE : FALSE);
    if (st->btnApplyLabel && IsWindow(st->btnApplyLabel))
        EnableWindow(st->btnApplyLabel, hasSel ? TRUE : FALSE);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        EnableWindow(st->btnBindKey, hasSel ? TRUE : FALSE);
    if (st->edtLabel && IsWindow(st->edtLabel))
        EnableWindow(st->edtLabel, hasSel ? TRUE : FALSE);
    if (st->edtPos && IsWindow(st->edtPos))
        EnableWindow(st->edtPos, hasSel ? TRUE : FALSE);
    if (st->edtWidth && IsWindow(st->edtWidth))
        EnableWindow(st->edtWidth, hasSel ? TRUE : FALSE);
    if (st->edtHeight && IsWindow(st->edtHeight))
        EnableWindow(st->edtHeight, hasSel ? TRUE : FALSE);
    if (st->edtUniformGap && IsWindow(st->edtUniformGap))
        EnableWindow(st->edtUniformGap, st->uniformSpacingEnabled ? TRUE : FALSE);
    if (st->lblUniformGap && IsWindow(st->lblUniformGap))
        EnableWindow(st->lblUniformGap, st->uniformSpacingEnabled ? TRUE : FALSE);
    if (st->edtUniformGap && !uniformGapHasFocus)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%d", st->uniformSpacingGap);
        Layout_SetWindowTextIfChanged(st->edtUniformGap, b);
    }

    if (!hasSel)
    {
        if (st->edtLabel && !labelHasFocus) Layout_SetWindowTextIfChanged(st->edtLabel, L"");
        if (st->edtPos && !posHasFocus) Layout_SetWindowTextIfChanged(st->edtPos, L"");
        if (st->edtWidth && !widthHasFocus) Layout_SetWindowTextIfChanged(st->edtWidth, L"");
        if (st->edtHeight && !heightHasFocus) Layout_SetWindowTextIfChanged(st->edtHeight, L"");
        if (st->lblBindState) Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    KeyDef k{};
    if (Layout_DraftGet(st, st->selectedIdx, k))
    {
        if (st->edtLabel && !labelHasFocus)
            Layout_SetWindowTextIfChanged(st->edtLabel, (k.label && k.label[0]) ? k.label : L"");
        if (st->edtPos && !posHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", k.x);
            Layout_SetWindowTextIfChanged(st->edtPos, b);
        }
        if (st->edtWidth && !widthHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", k.w);
            Layout_SetWindowTextIfChanged(st->edtWidth, b);
        }
        if (st->edtHeight && !heightHasFocus)
        {
            wchar_t b[32]{};
            swprintf_s(b, L"%d", std::max(KEYBOARD_KEY_MIN_DIM, k.h));
            Layout_SetWindowTextIfChanged(st->edtHeight, b);
        }
        if (st->lblBindState)
        {
            wchar_t s[96]{};
            swprintf_s(s, L"HID: %u  Raw: %u", (unsigned)k.hid, (unsigned)BackendUI_GetRawMilli(k.hid));
            Layout_SetWindowTextIfChanged(st->lblBindState, s);
        }
    }
}

static void Layout_UpdateBindStateOnly(LayoutPageState* st)
{
    if (!st || !st->lblBindState) return;
    if (st->selectedIdx < 0)
    {
        Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    KeyDef k{};
    if (!Layout_DraftGet(st, st->selectedIdx, k))
    {
        Layout_SetWindowTextIfChanged(st->lblBindState, L"");
        return;
    }

    wchar_t s[96]{};
    swprintf_s(s, L"HID: %u  Raw: %u", (unsigned)k.hid, (unsigned)BackendUI_GetRawMilli(k.hid));
    Layout_SetWindowTextIfChanged(st->lblBindState, s);
}

static uint32_t Layout_ComputePreviewHash(LayoutPageState* st)
{
    if (!st) return 0;
    uint32_t h = 2166136261u;
    for (const KeyDef& k : st->draftKeys)
    {
        if (k.hid == 0 || k.hid >= 256)
            continue;
        uint32_t raw = (uint32_t)BackendUI_GetRawMilli(k.hid);
        uint32_t v = ((uint32_t)k.hid << 16) ^ raw;
        h ^= v;
        h *= 16777619u;
    }
    return h;
}

static void Layout_StopBindCapture(HWND hWnd, LayoutPageState* st, const wchar_t* statusText = nullptr)
{
    if (!st) return;
    st->bindArmed = false;
    BackendUI_SetBindCapture(false);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        Layout_SetWindowTextIfChanged(st->btnBindKey, L"Bind Physical Key");
    if (st->lblBindState && statusText)
        Layout_SetWindowTextIfChanged(st->lblBindState, statusText);
}

static void Layout_StartBindCapture(HWND hWnd, LayoutPageState* st)
{
    if (!st || st->selectedIdx < 0) return;
    st->bindArmed = true;
    BackendUI_SetBindCapture(true);
    if (st->btnBindKey && IsWindow(st->btnBindKey))
        Layout_SetWindowTextIfChanged(st->btnBindKey, L"Press Physical Key...");
    if (st->lblBindState)
        Layout_SetWindowTextIfChanged(st->lblBindState, L"Waiting for key press...");
}

static void Layout_ApplyLabelFromEdit(HWND hWnd, LayoutPageState* st)
{
    if (!st || st->selectedIdx < 0 || !st->edtLabel) return;
    wchar_t txt[64]{};
    GetWindowTextW(st->edtLabel, txt, (int)(sizeof(txt) / sizeof(txt[0])));
    if (st->selectedIdx >= (int)st->draftLabels.size()) return;

    st->draftLabels[st->selectedIdx] = (txt[0] ? txt : L"Key");
    Layout_RebindDraftLabels(st);
    Layout_RefreshKeyList(hWnd, st);
    Layout_SetUnsaved(st, true);
    InvalidateRect(hWnd, nullptr, FALSE);
}

static void Layout_ApplyGeometryFromEdits(HWND hWnd, LayoutPageState* st, bool applyPos, bool applyWidth, bool applyHeight)
{
    if (!st || st->selectedIdx < 0) return;
    if (st->selectedIdx >= (int)st->draftKeys.size()) return;

    KeyDef& k = st->draftKeys[st->selectedIdx];
    bool changed = false;

    if (applyPos && st->edtPos)
    {
        wchar_t b[32]{};
        GetWindowTextW(st->edtPos, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, 0, 4000);
            if (k.x != v)
            {
                k.x = v;
                changed = true;
            }
        }
    }

    if (applyWidth && st->edtWidth)
    {
        wchar_t b[32]{};
        GetWindowTextW(st->edtWidth, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
            if (k.w != v)
            {
                k.w = v;
                changed = true;
            }
        }
    }

    if (applyHeight && st->edtHeight)
    {
        wchar_t b[32]{};
        GetWindowTextW(st->edtHeight, b, (int)(sizeof(b) / sizeof(b[0])));
        int v = 0;
        if (swscanf_s(b, L"%d", &v) == 1)
        {
            v = std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
            if (k.h != v)
            {
                k.h = v;
                changed = true;
            }
        }
    }

    if (changed)
    {
        Layout_RefreshKeyList(hWnd, st);
        Layout_SetUnsaved(st, true);
        InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
    Layout_UpdateMetaControls(st);
}

static void Layout_NotifyMainPage(HWND hWnd)
{
    HWND page = nullptr;

    HWND tab = GetParent(hWnd);
    if (tab) page = GetParent(tab);

    if (!page)
    {
        HWND root = ResolveAppMainWindow(hWnd);
        if (root)
            page = FindWindowExW(root, nullptr, L"PageMainClass", nullptr);
    }
    if (page) PostMessageW(page, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);

    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
}

static void Layout_ComputeCanvasRect(HWND hWnd, LayoutPageState* st)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 12);
    int leftW = S(hWnd, 300);
    int topY = S(hWnd, 56);

    st->canvasRc.left = margin + leftW + S(hWnd, 12);
    st->canvasRc.top = topY;
    st->canvasRc.right = rc.right - margin;
    st->canvasRc.bottom = rc.bottom - margin;
}

static void Layout_BuildDisplayXMap(const LayoutPageState* st, std::vector<int>& outDisplayX)
{
    outDisplayX.clear();
    if (!st) return;

    int n = (int)st->draftKeys.size();
    outDisplayX.resize((size_t)n, 0);
    for (int i = 0; i < n; ++i)
        outDisplayX[(size_t)i] = st->draftKeys[(size_t)i].x;

    if (!st->uniformSpacingEnabled || n <= 0) return;

    int kUniformGap = std::clamp(st->uniformSpacingGap, 0, 120);
    for (int row = 0; row <= 20; ++row)
    {
        std::vector<int> ids;
        ids.reserve((size_t)n);
        for (int i = 0; i < n; ++i)
        {
            if (st->draftKeys[(size_t)i].row == row)
                ids.push_back(i);
        }
        if (ids.empty()) continue;

        std::sort(ids.begin(), ids.end(), [&](int a, int b)
        {
            const KeyDef& ka = st->draftKeys[(size_t)a];
            const KeyDef& kb = st->draftKeys[(size_t)b];
            if (ka.x != kb.x) return ka.x < kb.x;
            return a < b;
        });

        int x = st->draftKeys[(size_t)ids[0]].x;
        for (int id : ids)
        {
            outDisplayX[(size_t)id] = x;
            x += st->draftKeys[(size_t)id].w + kUniformGap;
        }
    }
}

static void Layout_ComputeTransform(LayoutPageState* st, const RECT& canvas, const std::vector<int>* pDisplayX, float& scale, float& ox, float& oy)
{
    if (!st)
    {
        scale = 1.0f;
        ox = (float)canvas.left;
        oy = (float)canvas.top;
        return;
    }

    int maxX = 1;
    int maxBottom = KEYBOARD_KEY_H;
    for (size_t i = 0; i < st->draftKeys.size(); ++i)
    {
        const KeyDef& k = st->draftKeys[i];
        int x = k.x;
        if (pDisplayX && i < pDisplayX->size())
            x = (*pDisplayX)[i];
        maxX = std::max(maxX, x + k.w);
        maxBottom = std::max(maxBottom, k.row * KEYBOARD_ROW_PITCH_Y + std::max(KEYBOARD_KEY_MIN_DIM, k.h));
    }

    int modelW = KEYBOARD_MARGIN_X + maxX + KEYBOARD_MARGIN_X;
    int modelH = KEYBOARD_MARGIN_Y + maxBottom + KEYBOARD_MARGIN_Y;

    float cw = (float)(canvas.right - canvas.left);
    float ch = (float)(canvas.bottom - canvas.top);
    float sx = cw / (float)std::max(1, modelW);
    float sy = ch / (float)std::max(1, modelH);
    scale = std::max(0.1f, std::min(sx, sy));

    float drawW = (float)modelW * scale;
    float drawH = (float)modelH * scale;

    ox = (float)canvas.left + (cw - drawW) * 0.5f;
    oy = (float)canvas.top + (ch - drawH) * 0.5f;
}

static RECT Layout_KeyRectOnCanvasFast(LayoutPageState* st, int idx, const std::vector<int>* pDisplayX, float scale, float ox, float oy)
{
    RECT r{};
    KeyDef k{};
    if (!Layout_DraftGet(st, idx, k)) return r;

    int modelX = k.x;
    if (pDisplayX && idx >= 0 && (size_t)idx < pDisplayX->size())
        modelX = (*pDisplayX)[(size_t)idx];

    int x = (int)std::lround(ox + (KEYBOARD_MARGIN_X + modelX) * scale);
    int y = (int)std::lround(oy + (KEYBOARD_MARGIN_Y + k.row * KEYBOARD_ROW_PITCH_Y) * scale);
    int w = std::max(10, (int)std::lround(k.w * scale));
    int h = std::max(10, (int)std::lround(std::max(KEYBOARD_KEY_MIN_DIM, k.h) * scale));
    r = RECT{ x, y, x + w, y + h };
    return r;
}

static RECT Layout_KeyRectOnCanvas(LayoutPageState* st, int idx, const RECT& canvas)
{
    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, canvas, pDisplayX, scale, ox, oy);
    return Layout_KeyRectOnCanvasFast(st, idx, pDisplayX, scale, ox, oy);
}

static int Layout_HitTestKey(LayoutPageState* st, POINT pt)
{
    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);

    int n = Layout_DraftCount(st);
    for (int i = n - 1; i >= 0; --i)
    {
        RECT r = Layout_KeyRectOnCanvasFast(st, i, pDisplayX, scale, ox, oy);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

static void Layout_RefreshKeyList(HWND hWnd, LayoutPageState* st)
{
    if (!st || !st->lstKeys) return;
    SendMessageW(st->lstKeys, LB_RESETCONTENT, 0, 0);

    int n = Layout_DraftCount(st);
    for (int i = 0; i < n; ++i)
    {
        KeyDef k{};
        if (!Layout_DraftGet(st, i, k)) continue;

        wchar_t line[256]{};
        swprintf_s(line, L"%2d. %-7ls HID:%3u  row:%d x:%d w:%d h:%d", i + 1,
            (k.label ? k.label : L""), (unsigned)k.hid, k.row, k.x, k.w, std::max(KEYBOARD_KEY_MIN_DIM, k.h));
        SendMessageW(st->lstKeys, LB_ADDSTRING, 0, (LPARAM)line);
    }

    if (st->selectedIdx >= n) st->selectedIdx = -1;
    if (st->selectedIdx >= 0)
        SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
    Layout_UpdateMetaControls(st);
}

static void Layout_DrawFlatButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

    COLORREF bg = UiTheme::Color_ControlBg();
    if (pressed)
        bg = RGB(42, 42, 44);
    else if (hot)
        bg = RGB(40, 40, 42);

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
}

static void Layout_DrawCanvas(HWND hWnd, HDC hdc, LayoutPageState* st)
{
    if (!st) return;
    Layout_ComputeCanvasRect(hWnd, st);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF canvas((REAL)st->canvasRc.left, (REAL)st->canvasRc.top,
        (REAL)(st->canvasRc.right - st->canvasRc.left), (REAL)(st->canvasRc.bottom - st->canvasRc.top));

    SolidBrush bg(Gp(RGB(28, 28, 30)));
    g.FillRectangle(&bg, canvas);

    // subtle grid helps alignment while dragging keys
    {
        Pen rowPen(Gp(RGB(70, 70, 76), 110), 1.0f);
        Pen colPen(Gp(RGB(56, 56, 60), 80), 1.0f);
        const int step = std::max(8, S(hWnd, 12));
        int x0 = (int)canvas.X;
        int y0 = (int)canvas.Y;
        int x1 = (int)canvas.GetRight();
        int y1 = (int)canvas.GetBottom();

        for (int y = y0; y <= y1; y += step)
            g.DrawLine(&rowPen, (REAL)x0, (REAL)y, (REAL)x1, (REAL)y);
        for (int x = x0; x <= x1; x += step)
            g.DrawLine(&colPen, (REAL)x, (REAL)y0, (REAL)x, (REAL)y1);
    }

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawRectangle(&border, canvas);

    std::vector<int> displayX;
    Layout_BuildDisplayXMap(st, displayX);
    const std::vector<int>* pDisplayX = displayX.empty() ? nullptr : &displayX;
    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);

    int n = Layout_DraftCount(st);
    for (int i = 0; i < n; ++i)
    {
        KeyDef k{};
        if (!Layout_DraftGet(st, i, k)) continue;

        RECT rr = Layout_KeyRectOnCanvasFast(st, i, pDisplayX, scale, ox, oy);
        RectF r((REAL)rr.left, (REAL)rr.top, (REAL)(rr.right - rr.left), (REAL)(rr.bottom - rr.top));
        r.Inflate(-1.0f, -1.0f);

        bool sel = (i == st->selectedIdx);
        SolidBrush fill(sel ? Gp(UiTheme::Color_Accent(), 210) : Gp(RGB(48, 48, 52), 230));
        g.FillRectangle(&fill, r);

        // Live analog preview for bound HID keys (helps verify bind immediately).
        if (k.hid > 0 && k.hid < 256)
        {
            float v = (float)BackendUI_GetRawMilli(k.hid) / 1000.0f;
            v = std::clamp(v, 0.0f, 1.0f);
            if (v > 0.001f)
            {
                RectF rf = r;
                rf.Height = r.Height * v;
                SolidBrush fb(Gp(UiTheme::Color_Accent(), 140));
                g.FillRectangle(&fb, rf);
            }
        }

        Pen keyBorder(sel ? Gp(RGB(245, 245, 245)) : Gp(UiTheme::Color_Border()), sel ? 2.0f : 1.0f);
        g.DrawRectangle(&keyBorder, r);

        if (k.label && k.label[0])
        {
            FontFamily ff(L"Segoe UI");
            float em = std::clamp(r.Height * 0.36f, 9.0f, 13.0f);
            Font font(&ff, em, FontStyleRegular, UnitPixel);
            StringFormat fmt;
            fmt.SetAlignment(StringAlignmentCenter);
            fmt.SetLineAlignment(StringAlignmentCenter);
            fmt.SetFormatFlags(StringFormatFlagsNoWrap);
            SolidBrush txt(sel ? Gp(RGB(12, 12, 12)) : Gp(UiTheme::Color_Text()));
            g.DrawString(k.label, -1, &font, r, &fmt, &txt);
        }
    }
}

static void Layout_ApplyDrag(HWND hWnd, LayoutPageState* st, POINT ptClient)
{
    if (!st || st->selectedIdx < 0) return;

    KeyDef k{};
    if (!Layout_DraftGet(st, st->selectedIdx, k)) return;

    std::vector<int> displayX;
    const std::vector<int>* pDisplayX = nullptr;
    if (st->uniformSpacingEnabled)
    {
        Layout_BuildDisplayXMap(st, displayX);
        pDisplayX = displayX.empty() ? nullptr : &displayX;
    }

    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    Layout_ComputeTransform(st, st->canvasRc, pDisplayX, scale, ox, oy);
    if (scale <= 0.0001f) return;

    int left = ptClient.x - st->dragOffsetX;
    int top = ptClient.y - st->dragOffsetY;

    int targetDisplayX = (int)std::lround(((float)left - ox) / scale) - KEYBOARD_MARGIN_X;
    float rowPitch = (float)KEYBOARD_ROW_PITCH_Y * scale;
    int modelRow = (int)std::lround((((float)top - oy) - (float)KEYBOARD_MARGIN_Y * scale) / std::max(1.0f, rowPitch));

    KeyDef& edit = st->draftKeys[st->selectedIdx];
    int nextRow = std::clamp(modelRow, 0, 20);
    bool changed = false;

    if (st->uniformSpacingEnabled)
    {
        auto keyDisplayX = [&](int idx) -> int
        {
            if (pDisplayX && idx >= 0 && (size_t)idx < pDisplayX->size())
                return (*pDisplayX)[(size_t)idx];
            if (idx >= 0 && (size_t)idx < st->draftKeys.size())
                return st->draftKeys[(size_t)idx].x;
            return 0;
        };

        auto buildRowOrder = [&](int row) -> std::vector<int>
        {
            std::vector<int> ids;
            ids.reserve(st->draftKeys.size());
            for (int i = 0; i < (int)st->draftKeys.size(); ++i)
            {
                if (st->draftKeys[(size_t)i].row == row)
                    ids.push_back(i);
            }
            std::sort(ids.begin(), ids.end(), [&](int a, int b)
            {
                int ax = keyDisplayX(a);
                int bx = keyDisplayX(b);
                if (ax != bx) return ax < bx;
                return a < b;
            });
            return ids;
        };

        auto rowBaseX = [&](int row) -> int
        {
            int base = INT_MAX;
            for (int i = 0; i < (int)st->draftKeys.size(); ++i)
            {
                if (st->draftKeys[(size_t)i].row == row)
                    base = std::min(base, keyDisplayX(i));
            }
            if (base == INT_MAX)
                base = std::clamp(targetDisplayX, 0, 4000);
            return base;
        };

        auto insertionPos = [&](const std::vector<int>& ids) -> int
        {
            int ins = (int)ids.size();
            for (int i = 0; i < (int)ids.size(); ++i)
            {
                int id = ids[(size_t)i];
                int center = keyDisplayX(id) + st->draftKeys[(size_t)id].w / 2;
                if (targetDisplayX < center)
                {
                    ins = i;
                    break;
                }
            }
            return ins;
        };

        auto applyRowOrder = [&](int row, const std::vector<int>& order, int baseX)
        {
            int gap = std::max(0, st->uniformSpacingGap);
            int x = std::clamp(baseX, 0, 4000);
            for (int id : order)
            {
                KeyDef& kk = st->draftKeys[(size_t)id];
                kk.row = row;
                kk.x = std::clamp(x, 0, 4000);
                x += kk.w + gap;
            }
        };

        int rowFrom = edit.row;
        int rowTo = nextRow;
        std::vector<int> fromOrder = buildRowOrder(rowFrom);

        if (rowTo == rowFrom)
        {
            int oldPos = -1;
            for (int i = 0; i < (int)fromOrder.size(); ++i)
            {
                if (fromOrder[(size_t)i] == st->selectedIdx)
                {
                    oldPos = i;
                    break;
                }
            }

            if (oldPos >= 0)
            {
                std::vector<int> movable = fromOrder;
                movable.erase(movable.begin() + oldPos);

                int ins = insertionPos(movable);
                ins = std::clamp(ins, 0, (int)movable.size());
                movable.insert(movable.begin() + ins, st->selectedIdx);

                if (movable != fromOrder)
                {
                    applyRowOrder(rowFrom, movable, rowBaseX(rowFrom));
                    changed = true;
                }
            }
        }
        else
        {
            std::vector<int> toOrder = buildRowOrder(rowTo);

            std::vector<int> fromWithout = fromOrder;
            fromWithout.erase(std::remove(fromWithout.begin(), fromWithout.end(), st->selectedIdx), fromWithout.end());

            int ins = insertionPos(toOrder);
            ins = std::clamp(ins, 0, (int)toOrder.size());
            toOrder.insert(toOrder.begin() + ins, st->selectedIdx);

            if (!fromWithout.empty())
                applyRowOrder(rowFrom, fromWithout, rowBaseX(rowFrom));
            applyRowOrder(rowTo, toOrder, rowBaseX(rowTo));
            changed = true;
        }
    }
    else
    {
        int nextX = std::clamp(targetDisplayX, 0, 4000);
        if (edit.x != nextX)
        {
            edit.x = nextX;
            changed = true;
        }
    }

    if (!st->uniformSpacingEnabled && edit.row != nextRow)
    {
        edit.row = nextRow;
        changed = true;
    }

    if (changed)
    {
        st->dirty = true;
        Layout_UpdateMetaControls(st);
        InvalidateRect(hWnd, &st->canvasRc, FALSE);
    }
}

LRESULT CALLBACK KeyboardSubpages_LayoutPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (LayoutPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        Layout_DrawCanvas(hWnd, memDC, st);

        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_TextMuted());
        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, UiTheme::Color_ControlBg());
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_ControlBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new LayoutPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblPreset = CreateWindowW(L"STATIC", L"Keyboard model", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPreset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbPreset = PremiumCombo::Create(hWnd, hInst, 0, 0, 10, 10, ID_LAYOUT_PRESET,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbPreset, hFont, true);
        PremiumCombo::SetDropMaxVisible(st->cmbPreset, 8);

        st->btnReset = CreateWindowW(L"BUTTON", L"Reset To Preset", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_RESET, hInst, nullptr);
        SendMessageW(st->btnReset, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnAdd = CreateWindowW(L"BUTTON", L"Add Key", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_ADD, hInst, nullptr);
        SendMessageW(st->btnAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnDelete = CreateWindowW(L"BUTTON", L"Delete Selected", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_DELETE, hInst, nullptr);
        SendMessageW(st->btnDelete, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnSave = CreateWindowW(L"BUTTON", L"Save Changes", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_SAVE, hInst, nullptr);
        SendMessageW(st->btnSave, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnUniformSpacing = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_UNIFORM_SPACING, hInst, nullptr);
        SendMessageW(st->btnUniformSpacing, WM_SETFONT, (WPARAM)hFont, TRUE);
        Layout_UpdateUniformSpacingButton(st);

        st->lblUniformGap = CreateWindowW(L"STATIC", L"Uniform gap",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblUniformGap, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtUniformGap = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_UNIFORM_GAP_EDIT, hInst, nullptr);
        SendMessageW(st->edtUniformGap, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblLabel = CreateWindowW(L"STATIC", L"Label", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtLabel = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_LABEL_EDIT, hInst, nullptr);
        SendMessageW(st->edtLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnApplyLabel = CreateWindowW(L"BUTTON", L"Apply Label", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_LABEL_APPLY, hInst, nullptr);
        SendMessageW(st->btnApplyLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnBindKey = CreateWindowW(L"BUTTON", L"Bind Physical Key", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_BIND_KEY, hInst, nullptr);
        SendMessageW(st->btnBindKey, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblPos = CreateWindowW(L"STATIC", L"Position", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtPos = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_POS_EDIT, hInst, nullptr);
        SendMessageW(st->edtPos, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblWidth = CreateWindowW(L"STATIC", L"Width", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtWidth = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_WIDTH_EDIT, hInst, nullptr);
        SendMessageW(st->edtWidth, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblHeight = CreateWindowW(L"STATIC", L"Height", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHeight, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->edtHeight = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_HEIGHT_EDIT, hInst, nullptr);
        SendMessageW(st->edtHeight, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblBindState = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblBindState, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblKeys = CreateWindowW(L"STATIC", L"Keys", WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lstKeys = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)ID_LAYOUT_KEYS, hInst, nullptr);
        SendMessageW(st->lstKeys, WM_SETFONT, (WPARAM)hFont, TRUE);
        UiTheme::ApplyToControl(st->lstKeys);

        st->lblHint = CreateWindowW(L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);
        Layout_UpdateHintText(st);

        int presetCount = KeyboardLayout_GetPresetCount();
        for (int i = 0; i < presetCount; ++i)
            PremiumCombo::AddString(st->cmbPreset, KeyboardLayout_GetPresetName(i));
        st->editingPresetIdx = KeyboardLayout_GetCurrentPresetIndex();
        PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
        Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, false);
        EnableWindow(st->btnDelete, FALSE);
        Layout_SetUnsaved(st, false);
        Layout_UpdateMetaControls(st);
        SetTimer(hWnd, ID_LAYOUT_UI_TIMER, 16, nullptr);

        return 0;
    }

    case WM_SIZE:
        if (st)
        {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            int margin = S(hWnd, 12);
            int leftW = S(hWnd, 300);
            int topY = S(hWnd, 330);

            SetWindowPos(st->lblPreset, nullptr, margin, margin, leftW, S(hWnd, 18), SWP_NOZORDER);
            SetWindowPos(st->cmbPreset, nullptr, margin, margin + S(hWnd, 20), leftW - S(hWnd, 110), S(hWnd, 26), SWP_NOZORDER);
            SetWindowPos(st->btnReset, nullptr, margin + leftW - S(hWnd, 104), margin + S(hWnd, 20), S(hWnd, 104), S(hWnd, 26), SWP_NOZORDER);
            int actionY = margin + S(hWnd, 52);
            int actionH = S(hWnd, 26);
            int actionGap = S(hWnd, 8);
            int addW = S(hWnd, 90);
            int saveW = S(hWnd, 100);
            int addX = margin;
            int saveX = margin + leftW - saveW;
            int delX = addX + addW + actionGap;
            int delW = saveX - actionGap - delX;
            delW = std::max(S(hWnd, 72), delW);
            if (delX + delW > saveX - actionGap)
                delW = std::max(S(hWnd, 60), (saveX - actionGap) - delX);
            if (st->btnAdd)
                SetWindowPos(st->btnAdd, nullptr, addX, actionY, addW, actionH, SWP_NOZORDER);
            if (st->btnDelete)
                SetWindowPos(st->btnDelete, nullptr, delX, actionY, delW, actionH, SWP_NOZORDER);
            if (st->btnSave)
                SetWindowPos(st->btnSave, nullptr, saveX, actionY, saveW, actionH, SWP_NOZORDER);
            if (st->btnUniformSpacing)
                SetWindowPos(st->btnUniformSpacing, nullptr, margin, margin + S(hWnd, 84), leftW, S(hWnd, 24), SWP_NOZORDER);
            if (st->lblUniformGap)
                SetWindowPos(st->lblUniformGap, nullptr, margin, margin + S(hWnd, 112), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtUniformGap)
                SetWindowPos(st->edtUniformGap, nullptr, margin, margin + S(hWnd, 132), S(hWnd, 96), S(hWnd, 24), SWP_NOZORDER);
            if (st->lblLabel)
                SetWindowPos(st->lblLabel, nullptr, margin, margin + S(hWnd, 162), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtLabel)
                SetWindowPos(st->edtLabel, nullptr, margin, margin + S(hWnd, 182), leftW - S(hWnd, 114), S(hWnd, 24), SWP_NOZORDER);
            if (st->btnApplyLabel)
                SetWindowPos(st->btnApplyLabel, nullptr, margin + leftW - S(hWnd, 104), margin + S(hWnd, 182), S(hWnd, 104), S(hWnd, 24), SWP_NOZORDER);
            if (st->btnBindKey)
                SetWindowPos(st->btnBindKey, nullptr, margin, margin + S(hWnd, 212), leftW, S(hWnd, 24), SWP_NOZORDER);
            int fieldLabelY = margin + S(hWnd, 239);
            int fieldEditY = margin + S(hWnd, 257);
            int fieldGap = S(hWnd, 8);
            int fieldW = (leftW - fieldGap * 2) / 3;
            if (st->lblPos)
                SetWindowPos(st->lblPos, nullptr, margin, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblWidth)
                SetWindowPos(st->lblWidth, nullptr, margin + fieldW + fieldGap, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblHeight)
                SetWindowPos(st->lblHeight, nullptr, margin + (fieldW + fieldGap) * 2, fieldLabelY, fieldW, S(hWnd, 18), SWP_NOZORDER);
            if (st->edtPos)
                SetWindowPos(st->edtPos, nullptr, margin, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->edtWidth)
                SetWindowPos(st->edtWidth, nullptr, margin + fieldW + fieldGap, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->edtHeight)
                SetWindowPos(st->edtHeight, nullptr, margin + (fieldW + fieldGap) * 2, fieldEditY, fieldW, S(hWnd, 24), SWP_NOZORDER);
            if (st->lblBindState)
                SetWindowPos(st->lblBindState, nullptr, margin, margin + S(hWnd, 284), leftW, S(hWnd, 18), SWP_NOZORDER);
            if (st->lblKeys)
                SetWindowPos(st->lblKeys, nullptr, margin, margin + S(hWnd, 306), leftW, S(hWnd, 18), SWP_NOZORDER);
            int hintH = S(hWnd, 68);
            int listH = std::max(S(hWnd, 80), (int)rc.bottom - topY - margin - hintH - S(hWnd, 8));
            SetWindowPos(st->lstKeys, nullptr, margin, topY, leftW, listH, SWP_NOZORDER);
            SetWindowPos(st->lblHint, nullptr, margin, rc.bottom - margin - hintH, leftW, hintH, SWP_NOZORDER);

            Layout_ComputeCanvasRect(hWnd, st);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        if (!st) return 0;
        if (LOWORD(wParam) == ID_LAYOUT_PRESET && HIWORD(wParam) == CBN_SELCHANGE)
        {
            int sel = PremiumCombo::GetCurSel(st->cmbPreset);
            Layout_LoadDraftFromPreset(hWnd, st, sel, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_RESET && HIWORD(wParam) == BN_CLICKED)
        {
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_ADD && HIWORD(wParam) == BN_CLICKED)
        {
            int n = Layout_DraftCount(st);
            int maxRow = 0;
            for (int i = 0; i < n; ++i)
            {
                KeyDef kk{};
                if (Layout_DraftGet(st, i, kk))
                    maxRow = std::max(maxRow, kk.row);
            }
            st->draftLabels.emplace_back(L"Key");
            KeyDef kd{};
            kd.label = nullptr;
            kd.hid = 0;
            kd.row = std::clamp(maxRow + 1, 0, 20);
            kd.x = 0;
            kd.w = 42;
            kd.h = KEYBOARD_KEY_H;
            st->draftKeys.push_back(kd);
            Layout_RebindDraftLabels(st);
            st->selectedIdx = (int)st->draftKeys.size() - 1;
            Layout_RefreshKeyList(hWnd, st);
            SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
            Layout_SetUnsaved(st, true);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_LABEL_APPLY && HIWORD(wParam) == BN_CLICKED)
        {
            Layout_ApplyLabelFromEdit(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_BIND_KEY && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->bindArmed)
                Layout_StopBindCapture(hWnd, st, L"Bind cancelled.");
            else
                Layout_StartBindCapture(hWnd, st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_SPACING && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->uniformSpacingEnabled)
            {
                if (Layout_BakeUniformSpacingIntoDraft(st))
                    Layout_RefreshKeyList(hWnd, st);
            }
            st->uniformSpacingEnabled = !st->uniformSpacingEnabled;
            Layout_UpdateUniformSpacingButton(st);
            Layout_UpdateHintText(st);
            Layout_UpdateMetaControls(st);
            Layout_SetUnsaved(st, true);
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_DELETE && HIWORD(wParam) == BN_CLICKED)
        {
            if (st->selectedIdx < 0)
                st->selectedIdx = (int)SendMessageW(st->lstKeys, LB_GETCURSEL, 0, 0);
            if (st->selectedIdx >= 0 && st->selectedIdx < (int)st->draftKeys.size())
            {
                st->draftKeys.erase(st->draftKeys.begin() + st->selectedIdx);
                if (st->selectedIdx < (int)st->draftLabels.size())
                    st->draftLabels.erase(st->draftLabels.begin() + st->selectedIdx);
                Layout_RebindDraftLabels(st);

                int n = (int)st->draftKeys.size();
                if (st->selectedIdx >= n) st->selectedIdx = n - 1;
                Layout_RefreshKeyList(hWnd, st);
                if (st->selectedIdx >= 0)
                    SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)st->selectedIdx, 0);
                Layout_SetUnsaved(st, true);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_SAVE && HIWORD(wParam) == BN_CLICKED)
        {
            bool wasActive = (st->editingPresetIdx == KeyboardLayout_GetCurrentPresetIndex());
            if (KeyboardLayout_StorePresetSnapshot(st->editingPresetIdx, st->draftKeys, st->draftLabels, true,
                st->uniformSpacingEnabled, st->uniformSpacingGap))
            {
                if (wasActive)
                    Layout_NotifyMainPage(hWnd);
                Layout_RequestSave(hWnd);
                Layout_SetUnsaved(st, false);
            }
            else
            {
                MessageBoxW(hWnd, L"Failed to save layout preset file.", L"Layout Editor", MB_ICONERROR);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_KEYS && HIWORD(wParam) == LBN_SELCHANGE)
        {
            st->selectedIdx = (int)SendMessageW(st->lstKeys, LB_GETCURSEL, 0, 0);
            Layout_UpdateMetaControls(st);
            SetFocus(hWnd);
            InvalidateRect(hWnd, &st->canvasRc, FALSE);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_POS_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, true, false, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_POS_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtPos && GetFocus() == st->edtPos)
                Layout_ApplyGeometryFromEdits(hWnd, st, true, false, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_WIDTH_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, false, true, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_WIDTH_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtWidth && GetFocus() == st->edtWidth)
                Layout_ApplyGeometryFromEdits(hWnd, st, false, true, false);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_HEIGHT_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyGeometryFromEdits(hWnd, st, false, false, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_HEIGHT_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtHeight && GetFocus() == st->edtHeight)
                Layout_ApplyGeometryFromEdits(hWnd, st, false, false, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_GAP_EDIT && HIWORD(wParam) == EN_KILLFOCUS)
        {
            Layout_ApplyUniformGapFromEdit(hWnd, st);
            Layout_UpdateMetaControls(st);
            return 0;
        }
        if (LOWORD(wParam) == ID_LAYOUT_UNIFORM_GAP_EDIT && HIWORD(wParam) == EN_CHANGE)
        {
            if ((HWND)lParam == st->edtUniformGap && GetFocus() == st->edtUniformGap)
            {
                Layout_ApplyUniformGapFromEdit(hWnd, st);
                Layout_UpdateMetaControls(st);
            }
            return 0;
        }
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == ID_LAYOUT_RESET && st->btnReset == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_ADD && st->btnAdd == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_DELETE && st->btnDelete == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_LABEL_APPLY && st->btnApplyLabel == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_BIND_KEY && st->btnBindKey == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_SAVE && st->btnSave == dis->hwndItem) ||
             (dis->CtlID == ID_LAYOUT_UNIFORM_SPACING && st->btnUniformSpacing == dis->hwndItem)))
        {
            Layout_DrawFlatButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (PtInRect(&st->canvasRc, pt))
            {
                int hit = Layout_HitTestKey(st, pt);
                if (hit >= 0)
                {
                    st->selectedIdx = hit;
                    SendMessageW(st->lstKeys, LB_SETCURSEL, (WPARAM)hit, 0);
                    Layout_UpdateMetaControls(st);
                    SetFocus(hWnd);
                    RECT rr = Layout_KeyRectOnCanvas(st, hit, st->canvasRc);
                    st->dragOffsetX = pt.x - rr.left;
                    st->dragOffsetY = pt.y - rr.top;
                    st->dragging = true;
                    st->dirty = false;
                    SetCapture(hWnd);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st && st->dragging)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            Layout_ApplyDrag(hWnd, st, pt);
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->dragging)
        {
            st->dragging = false;
            ReleaseCapture();
            if (st->dirty)
            {
                Layout_RefreshKeyList(hWnd, st);
                Layout_SetUnsaved(st, true);
                st->dirty = false;
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st && st->selectedIdx >= 0)
        {
            KeyDef k{};
            if (Layout_DraftGet(st, st->selectedIdx, k))
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int step = shift ? 4 : 1;
                int newW = k.w + ((delta > 0) ? step : -step);
                int clampedW = std::clamp(newW, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
                if (st->draftKeys[st->selectedIdx].w != clampedW)
                {
                    st->draftKeys[st->selectedIdx].w = clampedW;
                    Layout_RefreshKeyList(hWnd, st);
                    Layout_SetUnsaved(st, true);
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        if (st) st->dragging = false;
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_TIMER:
        if (!st) return 0;
        if (wParam == ID_LAYOUT_UI_TIMER)
        {
            if (st->bindArmed)
            {
                uint16_t hid = 0, rawM = 0;
                if (BackendUI_ConsumeBindCapture(&hid, &rawM))
                {
                    if (st->selectedIdx >= 0 && st->selectedIdx < (int)st->draftKeys.size())
                    {
                        st->draftKeys[st->selectedIdx].hid = hid;
                        Layout_RefreshKeyList(hWnd, st);
                        Layout_SetUnsaved(st, true);
                    }
                    wchar_t s[128]{};
                    swprintf_s(s, L"Bound HID %u (raw %u).", (unsigned)hid, (unsigned)rawM);
                    Layout_StopBindCapture(hWnd, st, s);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                else
                {
                    Layout_UpdateBindStateOnly(st);
                }
            }
            else
            {
                // Update only lightweight status text; repaint preview only when values changed.
                Layout_UpdateBindStateOnly(st);
                uint32_t h = Layout_ComputePreviewHash(st);
                if (h != st->previewHash)
                {
                    st->previewHash = h;
                    InvalidateRect(hWnd, &st->canvasRc, FALSE);
                }
            }
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (!st) return 0;
        if (wParam == VK_ESCAPE && st->bindArmed)
        {
            Layout_StopBindCapture(hWnd, st, L"Bind cancelled.");
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            HWND focus = GetFocus();
            if (focus && (focus == st->edtPos || focus == st->edtWidth || focus == st->edtHeight))
            {
                Layout_ApplyGeometryFromEdits(hWnd, st, true, true, true);
                SetFocus(hWnd);
                Layout_UpdateMetaControls(st);
                return 0;
            }
            if (focus && focus == st->edtUniformGap)
            {
                Layout_ApplyUniformGapFromEdit(hWnd, st);
                SetFocus(hWnd);
                Layout_UpdateMetaControls(st);
                return 0;
            }
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && (wParam == 'R' || wParam == 'r'))
        {
            Layout_LoadDraftFromPreset(hWnd, st, st->editingPresetIdx, true);
            PremiumCombo::SetCurSel(st->cmbPreset, st->editingPresetIdx, false);
            return 0;
        }
        if (st->selectedIdx < 0) return 0;
        {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int step = shift ? 4 : 1;
            switch (wParam)
            {
            case VK_LEFT:  Layout_NudgeSelectedKey(hWnd, st, 0, -step, 0); return 0;
            case VK_RIGHT: Layout_NudgeSelectedKey(hWnd, st, 0, +step, 0); return 0;
            case VK_UP:    Layout_NudgeSelectedKey(hWnd, st, -1, 0, 0); return 0;
            case VK_DOWN:  Layout_NudgeSelectedKey(hWnd, st, +1, 0, 0); return 0;
            case VK_OEM_4: Layout_NudgeSelectedKey(hWnd, st, 0, 0, -step * 2); return 0; // [
            case VK_OEM_6: Layout_NudgeSelectedKey(hWnd, st, 0, 0, +step * 2); return 0; // ]
            }
        }
        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            KillTimer(hWnd, ID_LAYOUT_UI_TIMER);
            Layout_StopBindCapture(hWnd, st);
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Detached layout editor window
// ============================================================================
static HWND g_hLayoutEditorWindow = nullptr;

struct LayoutEditorHostState
{
    HWND hPage = nullptr;
};

static void LayoutEditor_ApplyDarkFrame(HWND hWnd)
{
    if (!hWnd) return;
    UiTheme::ApplyToTopLevelWindow(hWnd);
    SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
}

static LRESULT CALLBACK LayoutEditorHostProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (LayoutEditorHostState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        HINSTANCE hInst = cs ? (HINSTANCE)cs->hInstance : (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        st = new LayoutEditorHostState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->hPage = CreateWindowW(L"KeyboardSubLayoutPage", L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 100, 100, hWnd, nullptr, hInst, nullptr);
        LayoutEditor_ApplyDarkFrame(hWnd);
        return 0;
    }

    case WM_SIZE:
        if (st && st->hPage)
        {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            SetWindowPos(st->hPage, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_SHOWWINDOW:
    case WM_ACTIVATE:
        LayoutEditor_ApplyDarkFrame(hWnd);
        return 0;

    case WM_NCDESTROY:
        g_hLayoutEditorWindow = nullptr;
        if (st)
        {
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void LayoutEditor_OpenWindow(HWND hOwnerPage)
{
    if (g_hLayoutEditorWindow && IsWindow(g_hLayoutEditorWindow))
    {
        ShowWindow(g_hLayoutEditorWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_hLayoutEditorWindow);
        return;
    }

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hOwnerPage, GWLP_HINSTANCE);
    HWND hOwnerTop = GetAncestor(hOwnerPage, GA_ROOT);

    static bool childReg = false;
    if (!childReg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = KeyboardSubpages_LayoutPageProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"KeyboardSubLayoutPage";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        childReg = true;
    }

    static bool hostReg = false;
    if (!hostReg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = LayoutEditorHostProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"KeyboardLayoutEditorHost";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = UiTheme::Brush_PanelBg();
        RegisterClassW(&wc);
        hostReg = true;
    }

    int w = S(hOwnerPage, 1180);
    int h = S(hOwnerPage, 760);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    g_hLayoutEditorWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"KeyboardLayoutEditorHost",
        L"HallJoy - Layout Editor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, w, h,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_hLayoutEditorWindow)
        LayoutEditor_ApplyDarkFrame(g_hLayoutEditorWindow);
}

// ============================================================================
// Premium slider + value chip
// ============================================================================
static Color Gp(COLORREF c, BYTE a) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

struct PremiumSliderState
{
    int minV = 1;
    int maxV = 20;
    int posV = 5;
    bool dragging = false;
};

static int PremiumSlider_Clamp(const PremiumSliderState* st, int v)
{
    if (!st) return v;
    return std::clamp(v, st->minV, st->maxV);
}

static float PremiumSlider_ValueToT(const PremiumSliderState* st)
{
    if (!st) return 0.0f;
    int den = (st->maxV - st->minV);
    if (den <= 0) return 0.0f;
    return (float)(st->posV - st->minV) / (float)den;
}

static int PremiumSlider_XToValue(const PremiumSliderState* st, int x, int w, int pad)
{
    if (!st) return 0;
    int usable = w - pad * 2;
    if (usable <= 1) return st->minV;

    float t = (float)(x - pad) / (float)usable;
    t = std::clamp(t, 0.0f, 1.0f);

    float v = (float)st->minV + t * (float)(st->maxV - st->minV);
    int iv = (int)lroundf(v);
    return PremiumSlider_Clamp(st, iv);
}

static void PremiumSlider_Notify(HWND hWnd, int code)
{
    HWND parent = GetParent(hWnd);
    if (!parent) return;
    PostMessageW(parent, WM_HSCROLL, (WPARAM)code, (LPARAM)hWnd);
}

static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float rad)
{
    float rr = std::clamp(rad, 0.0f, std::min(r.Width, r.Height) * 0.5f);
    float d = rr * 2.0f;
    RectF arc(r.X, r.Y, d, d);

    path.StartFigure();
    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d; path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d; path.AddArc(arc, 0, 90);
    arc.X = r.X; path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

static void PremiumSlider_Paint(HWND hWnd, HDC hdc)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, (REAL)0, (REAL)0, (REAL)w, (REAL)h);

    int pad = std::clamp(h / 3, 8, 14);
    int trackH = std::clamp(h / 5, 6, 10);
    int cy = h / 2;

    RectF track((REAL)pad, (REAL)(cy - trackH / 2), (REAL)(w - pad * 2), (REAL)trackH);
    float rr = track.Height * 0.5f;

    {
        SolidBrush br(Gp(RGB(55, 55, 55)));
        GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);

        Pen border(Gp(UiTheme::Color_Border()), 1.0f);
        g.DrawPath(&border, &p);
    }

    float t = PremiumSlider_ValueToT(st);

    RectF fill = track;
    fill.Width = std::max(0.0f, track.Width * t);

    if (fill.Width > 0.5f)
    {
        Color accent = Gp(UiTheme::Color_Accent());
        Color accent2(
            255,
            (BYTE)std::min(255, (int)accent.GetR() + 18),
            (BYTE)std::min(255, (int)accent.GetG() + 18),
            (BYTE)std::min(255, (int)accent.GetB() + 18));

        LinearGradientBrush grad(fill, accent2, accent, LinearGradientModeVertical);

        GraphicsPath p;
        AddRoundRectPath(p, fill, rr);
        g.FillPath(&grad, &p);
    }

    float knobX = track.X + track.Width * t;
    float knobR = std::clamp((float)h * 0.22f, 7.0f, 12.0f);

    SolidBrush knobFill(Gp(RGB(235, 235, 235)));
    Pen knobBorder(Gp(RGB(15, 15, 15), 220), 1.5f);

    RectF knob(knobX - knobR, (REAL)cy - knobR, knobR * 2.0f, knobR * 2.0f);
    g.FillEllipse(&knobFill, knob);
    g.DrawEllipse(&knobBorder, knob);

    if (st && st->dragging)
    {
        Pen ring(Gp(UiTheme::Color_Accent(), 230), 2.5f);
        g.DrawEllipse(&ring, RectF(knob.X - 2.0f, knob.Y - 2.0f, knob.Width + 4.0f, knob.Height + 4.0f));
    }
}

static LRESULT CALLBACK PremiumSliderProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE:
    {
        st = new PremiumSliderState();
        st->posV = (int)Settings_GetPollingMs();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;
    }

    case WM_NCDESTROY:
        if (st)
        {
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumSlider_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        SetFocus(hWnd);
        SetCapture(hWnd);
        st->dragging = true;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        else
        {
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!st || !st->dragging) break;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!st) break;
        if (st->dragging)
        {
            st->dragging = false;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_ENDSCROLL);
            PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (!st) break;

        if (wParam == VK_LEFT || wParam == VK_DOWN) st->posV -= 1;
        else if (wParam == VK_RIGHT || wParam == VK_UP) st->posV += 1;
        else break;

        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        return 0;
    }

    case TBM_SETRANGE:
    {
        if (!st) break;
        int minV = (int)LOWORD(lParam);
        int maxV = (int)HIWORD(lParam);
        if (minV > maxV) std::swap(minV, maxV);
        st->minV = minV;
        st->maxV = maxV;
        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_SETPOS:
    {
        if (!st) break;
        st->posV = PremiumSlider_Clamp(st, (int)lParam);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_GETPOS:
    {
        if (!st) break;
        return (LRESULT)st->posV;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumSlider_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumSliderProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumSlider";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumSlider", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ---------------- Premium value chip ----------------
struct PremiumChipState
{
    wchar_t text[64]{};
};

static void PremiumChip_Paint(HWND hWnd, HDC hdc)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, 0, 0, w, h);

    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
    r.Inflate(-1.0f, -1.0f);
    float rad = std::clamp(r.Height * 0.40f, 6.0f, 14.0f);

    GraphicsPath p;
    AddRoundRectPath(p, r, rad);

    SolidBrush fill(Gp(UiTheme::Color_ControlBg()));
    g.FillPath(&fill, &p);

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawPath(&border, &p);

    const wchar_t* txt = (st && st->text[0]) ? st->text : L"";
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    float em = std::clamp(r.Height * 0.52f, 11.0f, 16.0f);
    Font font(&ff, em, FontStyleBold, UnitPixel);

    SolidBrush tbr(Gp(UiTheme::Color_Text()));
    g.DrawString(txt, -1, &font, r, &fmt, &tbr);
}

static LRESULT CALLBACK PremiumChipProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE: return TRUE;

    case WM_CREATE:
        st = new PremiumChipState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;

    case WM_NCDESTROY:
        if (st) { delete st; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETTEXT:
        if (st)
        {
            const wchar_t* s = (const wchar_t*)lParam;
            if (!s) s = L"";
            wcsncpy_s(st->text, s, _TRUNCATE);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumChip_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumChip_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumChipProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumValueChip";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumValueChip", L"",
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ============================================================================
// Global settings page
// ============================================================================
struct GlobalSettingsPageState
{
    HWND lblLayout = nullptr;
    HWND cmbLayout = nullptr;
    HWND btnLayoutEditor = nullptr;
    HWND btnOpenLayoutsFolder = nullptr;

    HWND lblPoll = nullptr;
    HWND sldPoll = nullptr;
    HWND chipPoll = nullptr;

    HWND lblUiRefresh = nullptr;
    HWND sldUiRefresh = nullptr;
    HWND chipUiRefresh = nullptr;

    HWND lblHint = nullptr;

    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;
    HWND hToast = nullptr;
    std::wstring toastText;
    DWORD toastHideAt = 0;
};

static constexpr int GLOB_ID_POLL_SLIDER = 7601;
static constexpr int GLOB_ID_UIREFRESH_SLIDER = 7602;
static constexpr int GLOB_ID_LAYOUT_COMBO = 7603;
static constexpr int GLOB_ID_LAYOUT_EDITOR = 7604;
static constexpr int GLOB_ID_LAYOUTS_FOLDER = 7605;

static void Global_DrawLayoutEditorButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;

    COLORREF bg = UiTheme::Color_ControlBg();
    if (pressed)
        bg = RGB(42, 42, 44);
    else if (hot)
        bg = RGB(40, 40, 42);

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text());
    DrawTextW(hdc, text, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS)
    {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(hdc, &focus);
    }
}

static void Global_RequestSave(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Global_RequestApplyTiming(HWND hWnd)
{
    HWND root = ResolveAppMainWindow(hWnd);
    if (root) PostMessageW(root, WM_APP_APPLY_TIMING, 0, 0);
}

static void Global_NotifyMainPage(HWND hWnd)
{
    HWND tab = GetParent(hWnd);
    HWND page = tab ? GetParent(tab) : nullptr;
    if (page) PostMessageW(page, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
}

static void Global_OpenLayoutsFolder(HWND hWnd)
{
    std::wstring dir = WinUtil_BuildPathNearExe(L"Layouts");
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    ShellExecuteW(hWnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void GlobalToast_EnsureWindow(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st || st->hToast) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
            {
                auto* stLocal = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
                switch (msg)
                {
                case WM_NCCREATE: return TRUE;
                case WM_CREATE:
                {
                    auto* cs = (CREATESTRUCTW*)lParam;
                    stLocal = (GlobalSettingsPageState*)cs->lpCreateParams;
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)stLocal);
                    SetLayeredWindowAttributes(hWnd, 0, 235, LWA_ALPHA);
                    return 0;
                }
                case WM_ERASEBKGND: return 1;
                case WM_PAINT:
                {
                    PAINTSTRUCT ps{};
                    HDC hdc = BeginPaint(hWnd, &ps);
                    RECT rc{};
                    GetClientRect(hWnd, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;

                    Graphics g(hdc);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    g.SetCompositingQuality(CompositingQualityHighQuality);
                    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

                    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
                    r.Inflate(-1.0f, -1.0f);

                    float rad = std::clamp(r.Height * 0.35f, 8.0f, 14.0f);
                    GraphicsPath p;
                    AddRoundRectPath(p, r, rad);

                    SolidBrush brFill(Color(245, 34, 34, 34));
                    g.FillPath(&brFill, &p);

                    Pen pen(Color(255, 255, 90, 90), 2.0f);
                    pen.SetLineJoin(LineJoinRound);
                    g.DrawPath(&pen, &p);

                    std::wstring text = (stLocal ? stLocal->toastText : L"");
                    if (!text.empty())
                    {
                        FontFamily ff(L"Segoe UI");
                        float em = std::clamp(r.Height * 0.36f, 11.0f, 14.0f);
                        Font font(&ff, em, FontStyleRegular, UnitPixel);
                        StringFormat fmt;
                        fmt.SetAlignment(StringAlignmentNear);
                        fmt.SetLineAlignment(StringAlignmentCenter);
                        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                        fmt.SetFormatFlags(StringFormatFlagsNoWrap);
                        RectF tr = r;
                        tr.Inflate(-10.0f, 0.0f);
                        SolidBrush txtBr(Gp(UiTheme::Color_Text(), 255));
                        g.DrawString(text.c_str(), -1, &font, tr, &fmt, &txtBr);
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }
                case WM_NCDESTROY:
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                    return 0;
                }
                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };

        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
        wc.lpszClassName = L"DD_LayoutDeleteToast";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    HWND ownerTop = GetAncestor(hPage, GA_ROOT);
    st->hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"DD_LayoutDeleteToast",
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        ownerTop, nullptr, (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE),
        st);
    if (st->hToast)
        ShowWindow(st->hToast, SW_HIDE);
}

static void GlobalToast_Hide(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st) return;
    st->toastHideAt = 0;
    if (hPage) KillTimer(hPage, TOAST_TIMER_ID);
    if (st->hToast) ShowWindow(st->hToast, SW_HIDE);
}

static void GlobalToast_ShowNearCursor(HWND hPage, GlobalSettingsPageState* st, const wchar_t* text)
{
    if (!st || !hPage) return;
    GlobalToast_EnsureWindow(hPage, st);
    if (!st->hToast) return;

    st->toastText = (text ? text : L"");

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(hPage);
    HGDIOBJ oldF = SelectObject(hdc, font);
    RECT calc{ 0,0,0,0 };
    DrawTextW(hdc, st->toastText.c_str(), (int)st->toastText.size(), &calc,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldF);
    ReleaseDC(hPage, hdc);

    int padX = S(hPage, 16);
    int padY = S(hPage, 10);
    int textW = (int)(calc.right - calc.left);
    int textH = (int)(calc.bottom - calc.top);
    int w = std::clamp(textW + padX * 2, S(hPage, 220), S(hPage, 520));
    int h = std::max(S(hPage, 34), textH + padY * 2);

    POINT pt{};
    GetCursorPos(&pt);
    int x = pt.x + S(hPage, 14);
    int y = pt.y + S(hPage, 18);

    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }

    SetWindowPos(st->hToast, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(st->hToast, nullptr, TRUE);
    st->toastHideAt = GetTickCount() + TOAST_SHOW_MS;
    SetTimer(hPage, TOAST_TIMER_ID, 30, nullptr);
}

static void GlobalDeleteConfirm_Clear(HWND hPage, GlobalSettingsPageState* st)
{
    if (!st) return;
    st->pendingDeleteIdx = -1;
    st->pendingDeleteTick = 0;
    GlobalToast_Hide(hPage, st);
}

static void Global_RefreshLayoutCombo(GlobalSettingsPageState* st)
{
    if (!st || !st->cmbLayout) return;

    PremiumCombo::Clear(st->cmbLayout);

    int presetCount = KeyboardLayout_GetPresetCount();
    for (int i = 0; i < presetCount; ++i)
    {
        int idx = PremiumCombo::AddString(st->cmbLayout, KeyboardLayout_GetPresetName(i));
        if (presetCount > 1)
            PremiumCombo::SetItemButtonKind(st->cmbLayout, idx, PremiumCombo::ItemButtonKind::Delete);
    }

    PremiumCombo::AddString(st->cmbLayout, L"+ Create New Layout...");
    PremiumCombo::SetDropMaxVisible(st->cmbLayout, 10);

    int cur = KeyboardLayout_GetCurrentPresetIndex();
    if (presetCount > 0)
        PremiumCombo::SetCurSel(st->cmbLayout, std::clamp(cur, 0, presetCount - 1), false);
    else
        PremiumCombo::SetCurSel(st->cmbLayout, -1, false);
}

static void Global_UpdateUi(GlobalSettingsPageState* st)
{
    if (!st) return;

    if (st->cmbLayout)
    {
        int cur = KeyboardLayout_GetCurrentPresetIndex();
        int sel = PremiumCombo::GetCurSel(st->cmbLayout);
        int count = PremiumCombo::GetCount(st->cmbLayout);
        bool selIsCreateRow = (count > 0 && sel == count - 1);
        if (!selIsCreateRow && sel != cur)
            PremiumCombo::SetCurSel(st->cmbLayout, cur, false);
    }

    if (st->chipPoll)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%u ms", (unsigned)Settings_GetPollingMs());
        SetWindowTextW(st->chipPoll, b);
    }

    if (st->chipUiRefresh)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%u ms", (unsigned)Settings_GetUIRefreshMs());
        SetWindowTextW(st->chipUiRefresh, b);
    }
}

static bool Layout_NudgeSelectedKey(HWND hWnd, LayoutPageState* st, int dRow, int dX, int dW)
{
    if (!st || st->selectedIdx < 0) return false;
    if (st->selectedIdx >= (int)st->draftKeys.size()) return false;

    KeyDef& k = st->draftKeys[st->selectedIdx];
    int nextRow = std::clamp(k.row + dRow, 0, 20);
    int nextX = std::clamp(k.x + dX, 0, 4000);
    int nextW = std::clamp(k.w + dW, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
    if (nextRow == k.row && nextX == k.x && nextW == k.w)
        return false;
    k.row = nextRow;
    k.x = nextX;
    k.w = nextW;

    Layout_RefreshKeyList(hWnd, st);
    Layout_SetUnsaved(st, true);
    InvalidateRect(hWnd, nullptr, FALSE);
    return true;
}

static void Global_Layout(HWND hWnd, GlobalSettingsPageState* st)
{
    if (!st) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int margin = S(hWnd, 16);
    int x = margin;
    int y = margin;

    int chipW = S(hWnd, 86);
    int gap = S(hWnd, 10);
    int comboVisibleH = S(hWnd, 26);
    int sliderH = S(hWnd, 34);
    int chipH = sliderH;
    int labelH = S(hWnd, 18);
    int rowGap = S(hWnd, 18);

    int sliderW = (rc.right - rc.left) - margin * 2 - chipW - gap;
    sliderW = std::max(S(hWnd, 180), sliderW);

    if (st->lblLayout)
        SetWindowPos(st->lblLayout, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->cmbLayout)
        SetWindowPos(st->cmbLayout, nullptr, x, y, sliderW + gap + chipW, comboVisibleH, SWP_NOZORDER);
    y += comboVisibleH + rowGap;

    if (st->btnLayoutEditor)
    {
        int bw = S(hWnd, 210);
        int bh = S(hWnd, 28);
        SetWindowPos(st->btnLayoutEditor, nullptr, x, y, bw, bh, SWP_NOZORDER);
        y += bh + S(hWnd, 14);
    }

    if (st->btnOpenLayoutsFolder)
    {
        int bw = S(hWnd, 210);
        int bh = S(hWnd, 28);
        SetWindowPos(st->btnOpenLayoutsFolder, nullptr, x, y, bw, bh, SWP_NOZORDER);
        y += bh + S(hWnd, 14);
    }

    if (st->lblPoll)
        SetWindowPos(st->lblPoll, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldPoll)
        SetWindowPos(st->sldPoll, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipPoll)
        SetWindowPos(st->chipPoll, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + rowGap;

    if (st->lblUiRefresh)
        SetWindowPos(st->lblUiRefresh, nullptr, x, y, sliderW + gap + chipW, labelH, SWP_NOZORDER);
    y += labelH + S(hWnd, 6);

    if (st->sldUiRefresh)
        SetWindowPos(st->sldUiRefresh, nullptr, x, y, sliderW, sliderH, SWP_NOZORDER);
    if (st->chipUiRefresh)
        SetWindowPos(st->chipUiRefresh, nullptr, x + sliderW + gap, y, chipW, chipH, SWP_NOZORDER);
    y += sliderH + S(hWnd, 14);

    if (st->lblHint)
        SetWindowPos(st->lblHint, nullptr, x, y, std::max(S(hWnd, 120), sliderW + gap + chipW), S(hWnd, 20), SWP_NOZORDER);
}

LRESULT CALLBACK KeyboardSubpages_GlobalSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (GlobalSettingsPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (msg == PremiumCombo::MsgItemTextCommit())
    {
        if (!st || !st->cmbLayout || (HWND)lParam != st->cmbLayout)
            return 0;
        GlobalDeleteConfirm_Clear(hWnd, st);

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        if (kind != PremiumCombo::ItemButtonKind::Rename)
            return 0;

        wchar_t nameBuf[260]{};
        PremiumCombo::ConsumeCommittedText(st->cmbLayout, nameBuf, 260);

        int presetCount = KeyboardLayout_GetPresetCount();
        if (idx != presetCount)
            return 0; // create row is always the last item

        int newIdx = -1;
        if (KeyboardLayout_CreatePreset(nameBuf, &newIdx))
        {
            Global_RefreshLayoutCombo(st);
            if (newIdx >= 0)
                PremiumCombo::SetCurSel(st->cmbLayout, newIdx, false);
            PremiumCombo::ShowDropDown(st->cmbLayout, false);
            Global_NotifyMainPage(hWnd);
            Global_RequestSave(hWnd);
        }
        else
        {
            MessageBoxW(hWnd, L"Failed to create layout. Name may be empty or already exists.", L"Layouts", MB_ICONWARNING);
            Global_UpdateUi(st);
        }
        return 0;
    }

    if (msg == PremiumCombo::MsgItemButton())
    {
        if (!st || !st->cmbLayout || (HWND)lParam != st->cmbLayout)
            return 0;

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        if (kind != PremiumCombo::ItemButtonKind::Delete)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            return 0;
        }

        int presetCount = KeyboardLayout_GetPresetCount();
        if (idx < 0 || idx >= presetCount)
            return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shift)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            if (KeyboardLayout_DeletePreset(idx))
            {
                Global_RefreshLayoutCombo(st);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            return 0;
        }

        DWORD now = GetTickCount();
        if (st->pendingDeleteIdx == idx && (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            if (KeyboardLayout_DeletePreset(idx))
            {
                Global_RefreshLayoutCombo(st);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            return 0;
        }

        st->pendingDeleteIdx = idx;
        st->pendingDeleteTick = now;
        GlobalToast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl && st->lblHint && hCtl == st->lblHint)
            SetTextColor(hdc, UiTheme::Color_TextMuted());
        else
            SetTextColor(hdc, UiTheme::Color_Text());

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new GlobalSettingsPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        st->lblLayout = CreateWindowW(L"STATIC", L"Keyboard layout",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblLayout, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->cmbLayout = PremiumCombo::Create(hWnd, hInst,
            0, 0, 10, 10, GLOB_ID_LAYOUT_COMBO,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP);
        PremiumCombo::SetFont(st->cmbLayout, hFont, true);
        Global_RefreshLayoutCombo(st);

        st->btnLayoutEditor = CreateWindowW(L"BUTTON", L"Open Layout Editor Window",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_LAYOUT_EDITOR, hInst, nullptr);
        SendMessageW(st->btnLayoutEditor, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnOpenLayoutsFolder = CreateWindowW(L"BUTTON", L"Open Layouts Folder",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)GLOB_ID_LAYOUTS_FOLDER, hInst, nullptr);
        SendMessageW(st->btnOpenLayoutsFolder, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblPoll = CreateWindowW(L"STATIC", L"Polling rate",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldPoll = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_POLL_SLIDER);
        SendMessageW(st->sldPoll, TBM_SETRANGE, TRUE, MAKELONG(1, 20));
        SendMessageW(st->sldPoll, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetPollingMs(), 1u, 20u));

        st->chipPoll = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_POLL_SLIDER + 100);
        SendMessageW(st->chipPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblUiRefresh = CreateWindowW(L"STATIC", L"UI refresh interval",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblUiRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldUiRefresh = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_UIREFRESH_SLIDER);
        SendMessageW(st->sldUiRefresh, TBM_SETRANGE, TRUE, MAKELONG(1, 200));
        SendMessageW(st->sldUiRefresh, TBM_SETPOS, TRUE, (LPARAM)std::clamp(Settings_GetUIRefreshMs(), 1u, 200u));

        st->chipUiRefresh = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, GLOB_ID_UIREFRESH_SLIDER + 100);
        SendMessageW(st->chipUiRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblHint = CreateWindowW(L"STATIC",
            L"Changes are applied immediately and saved automatically.",
            WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblHint, WM_SETFONT, (WPARAM)hFont, TRUE);

        Global_UpdateUi(st);
        Global_Layout(hWnd, st);
        return 0;
    }

    case WM_SIZE:
        Global_Layout(hWnd, st);
        return 0;

    case WM_TIMER:
        if (st && wParam == TOAST_TIMER_ID)
        {
            DWORD now = GetTickCount();
            if (st->toastHideAt != 0 && now >= st->toastHideAt)
                GlobalToast_Hide(hWnd, st);
            return 0;
        }
        break;

    case WM_HSCROLL:
    {
        if (!st) return 0;

        if ((HWND)lParam == st->sldPoll)
        {
            int v = (int)SendMessageW(st->sldPoll, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 1, 20);
            Settings_SetPollingMs((UINT)v);
            RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());
            Global_UpdateUi(st);
            Global_RequestApplyTiming(hWnd);
            Global_RequestSave(hWnd);
            return 0;
        }

        if ((HWND)lParam == st->sldUiRefresh)
        {
            int v = (int)SendMessageW(st->sldUiRefresh, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 1, 200);
            Settings_SetUIRefreshMs((UINT)v);
            Global_UpdateUi(st);
            Global_RequestApplyTiming(hWnd);
            Global_RequestSave(hWnd);
            return 0;
        }

        return 0;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == GLOB_ID_LAYOUT_EDITOR && st->btnLayoutEditor == dis->hwndItem) ||
             (dis->CtlID == GLOB_ID_LAYOUTS_FOLDER && st->btnOpenLayoutsFolder == dis->hwndItem)))
        {
            Global_DrawLayoutEditorButton(dis);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (!st) return 0;

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUT_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            int sel = PremiumCombo::GetCurSel(st->cmbLayout);
            int count = PremiumCombo::GetCount(st->cmbLayout);
            bool selIsCreateRow = (count > 0 && sel == count - 1);

            if (selIsCreateRow)
            {
                PremiumCombo::ShowDropDown(st->cmbLayout, true);
                PremiumCombo::BeginInlineEditSelected(st->cmbLayout, false);
                return 0;
            }

            if (sel >= 0 && sel != KeyboardLayout_GetCurrentPresetIndex())
            {
                KeyboardLayout_SetPresetIndex(sel);
                Global_NotifyMainPage(hWnd);
                Global_RequestSave(hWnd);
            }
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUT_EDITOR && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            LayoutEditor_OpenWindow(hWnd);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)GLOB_ID_LAYOUTS_FOLDER && HIWORD(wParam) == BN_CLICKED)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            Global_OpenLayoutsFolder(hWnd);
            return 0;
        }
        return 0;

    case WM_NCDESTROY:
        if (st)
        {
            GlobalDeleteConfirm_Clear(hWnd, st);
            if (st->hToast && IsWindow(st->hToast))
                DestroyWindow(st->hToast);
            st->hToast = nullptr;
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Snappy Joystick Toggle (premium owner-draw)
// ============================================================================
static constexpr const wchar_t* SNAPPY_TOGGLE_ANIM_PROP = L"DD_SnappyToggleAnimPtr";

static KspToggleAnimState* SnappyToggle_Get(HWND hBtn)
{
    return (KspToggleAnimState*)GetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
}

static void SnappyToggle_Free(HWND hBtn)
{
    if (auto* st = SnappyToggle_Get(hBtn))
    {
        RemovePropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
        delete st;
    }
}

static float SnappyClamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static bool SnappyToggle_HitTestSwitchOnly(HWND hBtn, POINT ptClient)
{
    RECT rc{};
    GetClientRect(hBtn, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    float sw = std::clamp((float)h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp((float)h * 0.78f, 18.0f, 28.0f);
    float sy = ((float)h - sh) * 0.5f;

    RECT r{};
    r.left = 0;
    r.right = (int)std::lround(sw);
    r.top = (int)std::lround(sy);
    r.bottom = (int)std::lround(sy + sh);

    return (ptClient.x >= r.left && ptClient.x < r.right && ptClient.y >= r.top && ptClient.y < r.bottom);
}

static void SnappyToggle_StartAnim(HWND hBtn, bool checked, bool animate)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st)
    {
        st = new KspToggleAnimState();
        SetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP, (HANDLE)st);
    }

    float target = checked ? 1.0f : 0.0f;

    if (!st->initialized || !animate)
    {
        st->initialized = true;
        st->checked = checked;
        st->t = target;
        st->from = target;
        st->to = target;
        st->running = false;
        st->startTick = GetTickCount();
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    st->checked = checked;
    st->from = st->t;
    st->to = target;
    st->startTick = GetTickCount();
    st->durationMs = 140;
    st->running = true;

    SetTimer(hBtn, 1, 15, nullptr);
    InvalidateRect(hBtn, nullptr, FALSE);
}

static void SnappyToggle_Tick(HWND hBtn)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st || !st->running) { KillTimer(hBtn, 1); return; }

    DWORD now = GetTickCount();
    DWORD dt = now - st->startTick;
    float x = (st->durationMs > 0) ? (float)dt / (float)st->durationMs : 1.0f;
    x = SnappyClamp01(x);

    // smoothstep
    float s = x * x * (3.0f - 2.0f * x);
    st->t = st->from + (st->to - st->from) * s;

    if (x >= 1.0f - 1e-4f)
    {
        st->t = st->to;
        st->running = false;
        KillTimer(hBtn, 1);
    }

    InvalidateRect(hBtn, nullptr, FALSE);
}

static LRESULT CALLBACK SnappyToggle_SubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        bool onSwitch = SnappyToggle_HitTestSwitchOnly(hBtn, pt);
        SnappyDebugLog(L"WM_LBUTTONDOWN", hBtn, onSwitch ? 1 : 0, (int)wParam);
        if (!onSwitch) { SetFocus(hBtn); return 0; }
        break;
    }

    case WM_SETCURSOR:
    {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hBtn, &pt);
        if (SnappyToggle_HitTestSwitchOnly(hBtn, pt)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
        break;
    }

    case WM_TIMER:
        if (wParam == 1) { SnappyToggle_Tick(hBtn); return 0; }
        break;

    case WM_NCDESTROY:
        SnappyDebugLog(L"WM_NCDESTROY", hBtn);
        KillTimer(hBtn, 1);
        SnappyToggle_Free(hBtn);
        RemoveWindowSubclass(hBtn, SnappyToggle_SubclassProc, 1);
        break;
    }
    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

static void DrawSnappyToggleOwnerDraw_Impl(const DRAWITEMSTRUCT* dis)
{
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    bool checked = (SendMessageW(dis->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);

    float t = checked ? 1.0f : 0.0f;
    if (auto* st = SnappyToggle_Get(dis->hwndItem))
        if (st->initialized) t = std::clamp(st->t, 0.0f, 1.0f);

    Gdiplus::Graphics g(dis->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    Gdiplus::RectF bounds(
        (float)dis->rcItem.left, (float)dis->rcItem.top,
        (float)(dis->rcItem.right - dis->rcItem.left),
        (float)(dis->rcItem.bottom - dis->rcItem.top));

    // background
    {
        Gdiplus::SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
        g.FillRectangle(&bg, bounds);
    }

    float h = bounds.Height;
    float sw = std::clamp(h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp(h * 0.78f, 18.0f, 28.0f);
    float sx = bounds.X;
    float sy = bounds.Y + (bounds.Height - sh) * 0.5f;

    Gdiplus::RectF track(sx, sy, sw, sh);
    float rr = sh * 0.5f;

    Gdiplus::Color onC = disabled ? Gp(UiTheme::Color_Border()) : Gp(UiTheme::Color_Accent());
    Gdiplus::Color offC = Gp(RGB(70, 70, 70));
    auto lerpC = [&](const Gdiplus::Color& a, const Gdiplus::Color& b, float tt)
        {
            tt = std::clamp(tt, 0.0f, 1.0f);
            auto L = [&](BYTE aa, BYTE bb) -> BYTE { return (BYTE)std::clamp((int)lroundf(aa + (bb - aa) * tt), 0, 255); };
            return Gdiplus::Color(L(a.GetA(), b.GetA()), L(a.GetR(), b.GetR()), L(a.GetG(), b.GetG()), L(a.GetB(), b.GetB()));
        };

    {
        Gdiplus::SolidBrush br(lerpC(offC, onC, t));
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);
    }

    float thumbD = sh - 4.0f;
    float thumbX0 = track.X + 2.0f;
    float thumbX1 = track.GetRight() - 2.0f - thumbD;
    float thumbX = thumbX0 + (thumbX1 - thumbX0) * t;

    {
        Gdiplus::RectF thumb(thumbX, track.Y + 2.0f, thumbD, thumbD);
        Gdiplus::SolidBrush brThumb(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(RGB(240, 240, 240)));
        g.FillEllipse(&brThumb, thumb);
    }

    // label
    {
        const wchar_t* label = L"Snap Stick";
        if (GetDlgCtrlID(dis->hwndItem) == ID_BLOCK_BOUND_KEYS)
            label = L"Block Bound Keys";
        Gdiplus::RectF textR(track.GetRight() + 10.0f, bounds.Y,
            bounds.GetRight() - (track.GetRight() + 10.0f), bounds.Height);

        FontFamily ff(L"Segoe UI");
        Gdiplus::StringFormat fmt;
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetAlignment(Gdiplus::StringAlignmentNear);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        float em = std::clamp(bounds.Height * 0.46f, 11.0f, 16.0f);
        Gdiplus::Font font(&ff, em, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush br(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text()));
        g.DrawString(label, -1, &font, textR, &fmt, &br);
    }
}

static void DrawSnappyToggleOwnerDraw(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2)
    {
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HDC memDC = CreateCompatibleDC(dis->hDC);
    if (!memDC)
    {
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HBITMAP bmp = CreateCompatibleBitmap(dis->hDC, w, h);
    if (!bmp)
    {
        DeleteDC(memDC);
        DrawSnappyToggleOwnerDraw_Impl(dis);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    DRAWITEMSTRUCT di = *dis;
    di.hDC = memDC;
    di.rcItem = RECT{ 0, 0, w, h };

    DrawSnappyToggleOwnerDraw_Impl(&di);
    BitBlt(dis->hDC, rc.left, rc.top, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

// ============================================================================
// Config page
// ============================================================================
struct ConfigPageState
{
    HWND chkSnappy = nullptr;
    HWND chkBlockBoundKeys = nullptr;

    // status label for presets
    HWND lblProfileStatus = nullptr;

    // --- Delete confirmation (two-click) ---
    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;

    // --- Premium toast (popup hint) ---
    HWND hToast = nullptr;
    std::wstring toastText;
    DWORD toastHideAt = 0;

    // vertical scroll state for Configuration page
    int scrollY = 0;
    int contentHeight = 0;
    bool scrollDrag = false;
    int  scrollDragStartY = 0;
    int  scrollDragStartScrollY = 0;
    int  scrollDragGrabOffsetY = 0;
    int  scrollDragThumbHeight = 0;
    int  scrollDragMax = 0;
};

static int Config_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int Config_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static void RequestSave(HWND hWnd)
{
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void SetProfileStatus(ConfigPageState* st, const wchar_t* text)
{
    if (!st || !st->lblProfileStatus) return;
    SetWindowTextW(st->lblProfileStatus, text ? text : L"");
}

static void LayoutConfigControls(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;

    int margin = S(hWnd, 12);
    int totalW = S(hWnd, 416);

    int x = margin;
    int y = S(hWnd, 310);
    int yAfter = y;

    // Snappy toggle
    if (st->chkSnappy)
    {
        int toggleH = S(hWnd, 26);
        SetWindowPos(st->chkSnappy, nullptr, x, yAfter,
            totalW, toggleH, SWP_NOZORDER);

        yAfter += toggleH + S(hWnd, 10);
    }

    if (st->chkBlockBoundKeys)
    {
        int toggleH = S(hWnd, 26);
        SetWindowPos(st->chkBlockBoundKeys, nullptr, x, yAfter,
            totalW, toggleH, SWP_NOZORDER);

        yAfter += toggleH + S(hWnd, 10);
    }

    if (st->lblProfileStatus)
    {
        SetWindowPos(st->lblProfileStatus, nullptr, x, yAfter,
            (int)std::max(10, (int)totalW), S(hWnd, 18), SWP_NOZORDER);
    }
}

static void Config_OffsetAllChildren(HWND hWnd, int dy)
{
    if (dy == 0) return;

    int count = 0;
    for (HWND child = GetWindow(hWnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        ++count;
    if (count <= 0) return;

    HDWP hdwp = BeginDeferWindowPos(count);
    for (HWND child = GetWindow(hWnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        RECT rc{};
        if (!GetWindowRect(child, &rc)) continue;
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&rc, 2);
        if (hdwp)
        {
            hdwp = DeferWindowPos(hdwp, child, nullptr,
                rc.left, rc.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        else
        {
            SetWindowPos(child, nullptr,
                rc.left, rc.top + dy, 0, 0,
                SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    if (hdwp) EndDeferWindowPos(hdwp);
}

static void Config_RequestFullRepaint(HWND hWnd)
{
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static int Config_GetViewportHeight(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int h = (int)(rc.bottom - rc.top);
    return std::max(0, h);
}

static int Config_ComputeBaseContentBottom(HWND hWnd)
{
    int margin = S(hWnd, 12);

    // Graph + CP hint region (painted on parent, not child controls)
    int graphBottom = S(hWnd, 86) + S(hWnd, 160) + margin;
    int cpHintBottom = S(hWnd, 286) + S(hWnd, 20) + margin;

    // Explicit controls below graph
    int y = S(hWnd, 310);
    int bottom = y + S(hWnd, 26) + S(hWnd, 10) + S(hWnd, 26) + S(hWnd, 10) + S(hWnd, 18) + margin;

    return std::max(bottom, std::max(graphBottom, cpHintBottom));
}

static int Config_RecalcContentHeight(HWND hWnd, ConfigPageState* st)
{
    if (!st) return 0;

    int bottom = Config_ComputeBaseContentBottom(hWnd);
    int margin = S(hWnd, 12);

    struct EnumCtx
    {
        HWND parent = nullptr;
        int bottom = 0;
        int margin = 0;
        int scrollY = 0;
    } ctx;
    ctx.parent = hWnd;
    ctx.bottom = bottom;
    ctx.margin = margin;
    ctx.scrollY = st->scrollY;

    EnumChildWindows(hWnd,
        [](HWND child, LPARAM lp) -> BOOL
        {
            auto* c = (EnumCtx*)lp;
            if (!c || !c->parent) return TRUE;
            if (!IsWindowVisible(child)) return TRUE;

            RECT rc{};
            if (!GetWindowRect(child, &rc)) return TRUE;
            MapWindowPoints(nullptr, c->parent, (LPPOINT)&rc, 2);
            int childBottom = (int)rc.bottom + c->margin + c->scrollY;
            c->bottom = std::max(c->bottom, childBottom);
            return TRUE;
        },
        (LPARAM)&ctx);

    st->contentHeight = std::max(0, ctx.bottom);
    return st->contentHeight;
}

static int Config_GetMaxScroll(HWND hWnd, ConfigPageState* st)
{
    if (!st) return 0;
    int viewH = Config_GetViewportHeight(hWnd);
    int contentH = Config_RecalcContentHeight(hWnd, st);
    return std::max(0, contentH - viewH);
}

static RECT Config_GetScrollTrackRect(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = Config_ScrollbarWidthPx(hWnd);
    int m = Config_ScrollbarMarginPx(hWnd);
    int rcRight = (int)rc.right;
    int rcBottom = (int)rc.bottom;
    RECT tr{};
    int left = std::max(0, rcRight - m - w);
    tr.left = (LONG)left;
    tr.right = (LONG)std::max(left + 1, rcRight - m);
    tr.top = m;
    int top = (int)tr.top;
    tr.bottom = (LONG)std::max(top + 1, rcBottom - m);
    return tr;
}

static RECT Config_GetScrollThumbRect(HWND hWnd, ConfigPageState* st)
{
    RECT tr = Config_GetScrollTrackRect(hWnd);
    if (!st) return tr;

    int trackHRaw = (int)tr.bottom - (int)tr.top;
    int trackH = std::max(1, trackHRaw);
    int viewH = std::max(1, Config_GetViewportHeight(hWnd));
    int maxScroll = Config_GetMaxScroll(hWnd, st);
    int contentH = std::max(1, st->contentHeight);

    int thumbH = (int)std::lround((double)trackH * (double)viewH / (double)contentH);
    thumbH = std::clamp(thumbH, S(hWnd, 36), trackH);

    int travel = std::max(0, trackH - thumbH);
    int top = tr.top;
    if (travel > 0 && maxScroll > 0)
    {
        double t = (double)std::clamp(st->scrollY, 0, maxScroll) / (double)maxScroll;
        top = tr.top + (int)std::lround(t * (double)travel);
    }

    RECT th{ tr.left, top, tr.right, top + thumbH };
    return th;
}

static void Config_SetScrollY(HWND hWnd, ConfigPageState* st, int newScrollY)
{
    if (!st) return;

    int maxScroll = Config_GetMaxScroll(hWnd, st);

    int target = std::clamp(newScrollY, 0, maxScroll);
    if (target != st->scrollY)
    {
        int dy = st->scrollY - target;
        Config_OffsetAllChildren(hWnd, dy);
        st->scrollY = target;
        SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)st->scrollY);
    }
    Config_RequestFullRepaint(hWnd);
}

static LPARAM Config_AdjustClientMouseLParamForScroll(ConfigPageState* st, LPARAM lParam)
{
    if (!st || st->scrollY == 0) return lParam;
    int x = (short)LOWORD(lParam);
    int y = (short)HIWORD(lParam);
    y += st->scrollY;
    return MAKELPARAM((short)x, (short)y);
}

static LPARAM Config_AdjustWheelLParamForScroll(HWND hWnd, ConfigPageState* st, LPARAM lParam)
{
    if (!st || st->scrollY == 0) return lParam;

    POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
    ScreenToClient(hWnd, &pt);
    pt.y += st->scrollY;
    ClientToScreen(hWnd, &pt);
    return MAKELPARAM((short)pt.x, (short)pt.y);
}

static void DrawConfigScrollbar(HWND hWnd, HDC hdc, ConfigPageState* st)
{
    if (!st) return;
    int maxScroll = Config_GetMaxScroll(hWnd, st);
    if (maxScroll <= 0) return;

    RECT trR = Config_GetScrollTrackRect(hWnd);
    RECT thR = Config_GetScrollThumbRect(hWnd, st);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    RectF tr((REAL)trR.left, (REAL)trR.top, (REAL)(trR.right - trR.left), (REAL)(trR.bottom - trR.top));
    RectF th((REAL)thR.left, (REAL)thR.top, (REAL)(thR.right - thR.left), (REAL)(thR.bottom - thR.top));

    float rTrack = std::max(2.0f, tr.Width * 0.5f);
    float rThumb = std::max(2.0f, th.Width * 0.5f);

    {
        SolidBrush bg(Gp(RGB(44, 44, 48), 180));
        GraphicsPath p;
        AddRoundRectPath(p, tr, rTrack);
        g.FillPath(&bg, &p);
    }

    {
        Color thumbC = st->scrollDrag ? Gp(UiTheme::Color_Accent(), 240) : Gp(UiTheme::Color_Accent(), 205);
        SolidBrush br(thumbC);
        GraphicsPath p;
        AddRoundRectPath(p, th, rThumb);
        g.FillPath(&br, &p);
    }
}

static std::wstring SanitizePresetNameForFile(const std::wstring& in)
{
    std::wstring s = in;

    while (!s.empty() && s.back() == L' ') s.pop_back();
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());

    if (s.size() >= 4)
    {
        const wchar_t* tail = s.c_str() + (s.size() - 4);
        if (_wcsicmp(tail, L".ini") == 0)
            s.resize(s.size() - 4);
    }

    const wchar_t* bad = L"<>:\"/\\|?*";
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (wcschr(bad, s[i]) || s[i] < 32)
            s[i] = L'_';
    }

    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) s.pop_back();

    return s;
}

// ============================================================================
// Premium toast (small popup hint) for delete confirmation
// ============================================================================
static void Toast_EnsureWindow(HWND hPage, ConfigPageState* st)
{
    if (!st || st->hToast) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
            {
                auto* stLocal = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

                switch (msg)
                {
                case WM_NCCREATE:
                    return TRUE;

                case WM_CREATE:
                {
                    auto* cs = (CREATESTRUCTW*)lParam;
                    stLocal = (ConfigPageState*)cs->lpCreateParams;
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)stLocal);

                    // layered alpha
                    SetLayeredWindowAttributes(hWnd, 0, 235, LWA_ALPHA);
                    return 0;
                }

                case WM_ERASEBKGND:
                    return 1;

                case WM_PAINT:
                {
                    PAINTSTRUCT ps{};
                    HDC hdc = BeginPaint(hWnd, &ps);

                    RECT rc{};
                    GetClientRect(hWnd, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;

                    Graphics g(hdc);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    g.SetCompositingQuality(CompositingQualityHighQuality);
                    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

                    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
                    r.Inflate(-1.0f, -1.0f);

                    float rad = std::clamp(r.Height * 0.35f, 8.0f, 14.0f);

                    GraphicsPath p;
                    AddRoundRectPath(p, r, rad);

                    // Fill
                    Color fill(245, 34, 34, 34); // slightly translucent
                    SolidBrush brFill(fill);
                    g.FillPath(&brFill, &p);

                    // Border (soft red)
                    Color border(255, 255, 90, 90);
                    Pen pen(border, 2.0f);
                    pen.SetLineJoin(LineJoinRound);
                    g.DrawPath(&pen, &p);

                    // Text
                    std::wstring text = (stLocal ? stLocal->toastText : L"");
                    if (!text.empty())
                    {
                        FontFamily ff(L"Segoe UI");
                        float em = std::clamp(r.Height * 0.36f, 11.0f, 14.0f);
                        Font font(&ff, em, FontStyleRegular, UnitPixel);

                        StringFormat fmt;
                        fmt.SetAlignment(StringAlignmentNear);
                        fmt.SetLineAlignment(StringAlignmentCenter);
                        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

                        RectF tr = r;
                        tr.Inflate(-10.0f, 0.0f);

                        SolidBrush brTxt(Gp(UiTheme::Color_Text(), 255));
                        g.DrawString(text.c_str(), -1, &font, tr, &fmt, &brTxt);
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }

                case WM_NCDESTROY:
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                    return 0;
                }

                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };

        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
        wc.lpszClassName = L"DD_PresetToast";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    st->hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"DD_PresetToast",
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        ownerTop, nullptr, (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE),
        st);

    if (st->hToast)
        ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_Hide(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->toastHideAt = 0;
    if (hPage) KillTimer(hPage, TOAST_TIMER_ID);
    if (st->hToast) ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_ShowNearCursor(HWND hPage, ConfigPageState* st, const wchar_t* text)
{
    if (!st || !hPage) return;

    Toast_EnsureWindow(hPage, st);
    if (!st->hToast) return;

    st->toastText = (text ? text : L"");

    // Measure text using Win32 DrawText for good sizing
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(hPage);
    HGDIOBJ oldF = SelectObject(hdc, font);

    RECT calc{ 0,0,0,0 };
    DrawTextW(hdc, st->toastText.c_str(), (int)st->toastText.size(), &calc,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldF);
    ReleaseDC(hPage, hdc);

    int padX = S(hPage, 16);
    int padY = S(hPage, 10);

    // FIX: RECT uses LONG; std::max needs same types
    int textW = (int)(calc.right - calc.left);
    int textH = (int)(calc.bottom - calc.top);

    int w = textW + padX * 2;
    int h = std::max(S(hPage, 34), textH + padY * 2);

    w = std::clamp(w, S(hPage, 220), S(hPage, 520));

    POINT pt{};
    GetCursorPos(&pt);

    int x = pt.x + S(hPage, 14);
    int y = pt.y + S(hPage, 18);

    // Clamp to monitor work area
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }

    SetWindowPos(st->hToast, HWND_TOPMOST, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    InvalidateRect(st->hToast, nullptr, TRUE);

    st->toastHideAt = GetTickCount() + TOAST_SHOW_MS;
    SetTimer(hPage, TOAST_TIMER_ID, 30, nullptr);
}

static void DeleteConfirm_Clear(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->pendingDeleteIdx = -1;
    st->pendingDeleteTick = 0;
    Toast_Hide(hPage, st);
}

// Draw hint for CP weights
static void DrawCpWeightHintIfNeeded(HWND hWnd, HDC hdc)
{
    float w01 = 0.0f;
    KeySettingsPanel_DragHint hint = KeySettingsPanel_GetDragHint(&w01);
    if (hint == KeySettingsPanel_DragHint::None)
        return;

    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);

    int x = S(hWnd, 12);
    int y = S(hWnd, 286);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    const Color orange(255, 255, 170, 0);
    const Color orangeBorder(255, 210, 135, 0);

    float iconD = (float)S(hWnd, 16);
    RectF icon((float)x, (float)y, iconD, iconD);

    {
        SolidBrush br(orange);
        g.FillEllipse(&br, icon);
        Pen pen(orangeBorder, 1.0f);
        g.DrawEllipse(&pen, icon);
    }

    {
        FontFamily ff(L"Segoe UI");
        float em = std::clamp(iconD * 0.78f, 10.0f, 14.0f);
        Font font(&ff, em, FontStyleBold, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        SolidBrush txt(Color(255, 20, 20, 20));
        g.DrawString(L"!", -1, &font, icon, &fmt, &txt);
    }

    int pct = (int)std::lround(std::clamp(w01, 0.0f, 1.0f) * 100.0f);

    wchar_t msg2[256]{};
    swprintf_s(msg2, L"Use mouse wheel to change weight (%d%%).", pct);

    {
        FontFamily ff(L"Segoe UI");
        float em = (float)S(hWnd, 13);
        em = std::clamp(em, 11.0f, 14.0f);
        Font font(&ff, em, FontStyleRegular, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentNear);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        RectF tr(icon.GetRight() + 8.0f, (REAL)y - 1.0f,
            (REAL)(rcClient.right - rcClient.left) - (icon.GetRight() + 8.0f) - (REAL)S(hWnd, 12),
            iconD + 2.0f);

        SolidBrush txt(Gp(UiTheme::Color_TextMuted(), 255));
        g.DrawString(msg2, -1, &font, tr, &fmt, &txt);
    }

    (void)hint;
}

static HWND GetPresetCombo(HWND hWnd)
{
    return GetDlgItem(hWnd, KSP_ID_PROFILE);
}

static void SelectActivePresetInCombo(HWND hWnd)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    std::vector<KeyboardProfiles::ProfileInfo> list;
    int activeIdx = KeyboardProfiles::RefreshList(list); // active preset index among presets

    if (activeIdx >= 0)
    {
        // Indices match: KeySettingsPanel adds "+ Create..." after preset items.
        PremiumCombo::SetCurSel(hCombo, activeIdx, false);
        PremiumCombo::SetExtraIcon(hCombo, PremiumCombo::ExtraIconKind::None);
    }
}

static void DoBeginInlineCreate(HWND hWnd, ConfigPageState* st)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    int count = PremiumCombo::GetCount(hCombo);
    if (count <= 0) return;

    int idx = count - 1;

    PremiumCombo::ShowDropDown(hCombo, true);
    PremiumCombo::SetCurSel(hCombo, idx, false);
    PremiumCombo::BeginInlineEdit(hCombo, idx, false);

    SetProfileStatus(st, L"Type a name and press Enter to create a new preset.");
}

static bool DeletePreset_NoPopup_ConfigPage(HWND hWnd, ConfigPageState* st, int idx, bool requireShift)
{
    std::vector<KeyboardProfiles::ProfileInfo> list;
    KeyboardProfiles::RefreshList(list);

    if (idx < 0 || idx >= (int)list.size())
        return false;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (requireShift && !shift)
    {
        SetProfileStatus(st, L"Hold Shift to delete.");
        return false;
    }

    if (KeyboardProfiles::DeletePreset(list[idx].path))
    {
        SetProfileStatus(st, L"Preset deleted.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        RequestSave(hWnd);
        return true;
    }

    SetProfileStatus(st, L"ERROR: Failed to delete preset.");
    KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
    return false;
}

LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (msg == WM_TIMER)
    {
        // 1) toast auto-hide
        if (wParam == TOAST_TIMER_ID && st)
        {
            DWORD now = GetTickCount();
            if (st->toastHideAt != 0 && now >= st->toastHideAt)
                Toast_Hide(hWnd, st);
            return 0;
        }

        // 2) forward other timers to KeySettings panel (morph etc.)
        KeySettingsPanel_HandleTimer(hWnd, wParam);
        // When page is scrolled, graph internals invalidate fixed (content) rects.
        // Force full repaint to avoid stale fragments during morph/toggle animations.
        if (st && st->scrollY != 0)
            Config_RequestFullRepaint(hWnd);
        return 0;
    }

    if (msg == WM_APP_PROFILE_BEGIN_CREATE)
    {
        DoBeginInlineCreate(hWnd, st);
        return 0;
    }

    // Inline text commit from PremiumCombo (Rename + Create New)
    if (msg == PremiumCombo::MsgItemTextCommit())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        HWND hCombo = (HWND)lParam;

        if (kind != PremiumCombo::ItemButtonKind::Rename || !hCombo)
            return 0;

        wchar_t newNameBuf[260]{};
        PremiumCombo::ConsumeCommittedText(hCombo, newNameBuf, 260);

        std::wstring raw = newNameBuf;
        std::wstring safe = SanitizePresetNameForFile(raw);

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        // Current curve shown on screen (the whole point of presets)
        KeyDeadzone curCurve = Ksp_GetVisualCurve();

        // Case A: commit came from the last row => create new preset
        if (idx == (int)list.size())
        {
            if (safe.empty())
            {
                SetProfileStatus(st, L"");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            if (KeyboardProfiles::CreatePreset(safe, curCurve))
            {
                SetProfileStatus(st, L"Preset created.");

                // refresh list in KeySettingsPanel
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);

                // force UI selection to the now-active preset (newly created)
                SelectActivePresetInCombo(hWnd);

                // optional: close dropdown after creation (feels premium)
                HWND hCombo2 = GetPresetCombo(hWnd);
                if (hCombo2) PremiumCombo::ShowDropDown(hCombo2, false);

                RequestSave(hWnd);
            }
            else
            {
                SetProfileStatus(st, L"ERROR: Failed to create preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            }
            return 0;
        }

        // Case B: rename existing preset
        if (idx < 0 || idx >= (int)list.size())
        {
            SetProfileStatus(st, L"Rename failed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        const auto& p = list[idx];

        if (safe.empty())
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        if (_wcsicmp(safe.c_str(), p.name.c_str()) == 0)
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        fs::path oldPath = fs::path(p.path);
        fs::path dir = oldPath.parent_path();
        fs::path newPath = dir / (safe + L".ini");

        if (GetFileAttributesW(newPath.wstring().c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            SetProfileStatus(st, L"Rename failed: name already exists.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        std::wstring active = KeyboardProfiles::GetActiveProfileName();

        // If renaming active preset:
        // Save current curve into new file (this also updates "active" state inside module),
        // then delete the old file.
        if (!active.empty() && _wcsicmp(active.c_str(), p.name.c_str()) == 0)
        {
            if (!KeyboardProfiles::SavePreset(newPath.wstring(), curCurve))
            {
                SetProfileStatus(st, L"Rename failed: could not save new preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            DeleteFileW(oldPath.wstring().c_str());

            SetProfileStatus(st, L"Preset renamed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            RequestSave(hWnd);
            return 0;
        }

        // Non-active preset: simple file rename
        BOOL ok = MoveFileExW(oldPath.wstring().c_str(), newPath.wstring().c_str(),
            MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);

        if (!ok)
        {
            SetProfileStatus(st, L"Rename failed: file rename error.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        SetProfileStatus(st, L"Preset renamed.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        return 0;
    }

    // Item button clicks (Delete gets delivered here; Rename starts inline edit internally)
    if (msg == PremiumCombo::MsgItemButton())
    {
        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);

        if (kind == PremiumCombo::ItemButtonKind::Delete)
        {
            if (!st) return 0;

            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // Shift => instant delete (no confirmation)
            if (shift)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            DWORD now = GetTickCount();

            // Second click within window => delete
            if (st->pendingDeleteIdx == idx && (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            // First click => arm confirmation + show premium toast
            st->pendingDeleteIdx = idx;
            st->pendingDeleteTick = now;

            Toast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
            SetProfileStatus(st, L""); // don't spam status bar
            return 0;
        }

        // any other button cancels pending delete
        if (st) DeleteConfirm_Clear(hWnd, st);
        return 0;
    }

    // Extra icon click (save dirty preset)
    if (msg == PremiumCombo::MsgExtraIcon())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        HWND hCombo = (HWND)lParam;

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        int sel = -1;
        int count = 0;
        if (hCombo)
        {
            sel = PremiumCombo::GetCurSel(hCombo);
            count = PremiumCombo::GetCount(hCombo);
        }

        auto refreshUi = [&]()
            {
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                RequestSave(hWnd);
            };

        bool selIsCreateNew = (count > 0 && sel == (count - 1));

        if (list.empty() || selIsCreateNew || sel < 0)
        {
            PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
            return 0;
        }

        if (sel >= 0 && sel < (int)list.size())
        {
            const auto& p = list[sel];

            // Save CURRENT curve (visual state) into selected preset
            KeyDeadzone curCurve = Ksp_GetVisualCurve();

            if (KeyboardProfiles::SavePreset(p.path, curCurve))
            {
                std::wstring ok = L"Preset saved: " + p.name;
                SetProfileStatus(st, ok.c_str());
                refreshUi();
            }
            else
            {
                std::wstring err = L"ERROR: Failed to save preset: " + p.name;
                SetProfileStatus(st, err.c_str());
                refreshUi();
            }
            return 0;
        }

        PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        RECT rc{};
        GetClientRect(hWnd, &rc);

        SaveDC(memDC);
        if (st && st->scrollY != 0)
            SetViewportOrgEx(memDC, 0, -st->scrollY, nullptr);

        KeySettingsPanel_DrawGraph(memDC, rc);
        DrawCpWeightHintIfNeeded(hWnd, memDC);

        RestoreDC(memDC, -1);
        DrawConfigScrollbar(hWnd, memDC, st);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl)
        {
            if (st->lblProfileStatus && hCtl == st->lblProfileStatus)
            {
                SetTextColor(hdc, UiTheme::Color_TextMuted());
            }
            else
            {
                SetTextColor(hdc, UiTheme::Color_Text());
            }
        }
        else
        {
            SetTextColor(hdc, UiTheme::Color_Text());
        }

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new ConfigPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        SetPropW(hWnd, CONFIG_SCROLLY_PROP, (HANDLE)(INT_PTR)0);

        KeySettingsPanel_Create(hWnd, hInst);
        KeySettingsPanel_SetSelectedHid(KeyboardUI_Internal_GetSelectedHid());

        st->lblProfileStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblProfileStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->chkSnappy = CreateWindowW(L"BUTTON", L"Snap Stick",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_SNAPPY, hInst, nullptr);
        SendMessageW(st->chkSnappy, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->chkBlockBoundKeys = CreateWindowW(L"BUTTON", L"Block Bound Keys",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_BLOCK_BOUND_KEYS, hInst, nullptr);
        SendMessageW(st->chkBlockBoundKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        // initial state
        SendMessageW(st->chkSnappy, BM_SETCHECK, Settings_GetSnappyJoystick() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(st->chkBlockBoundKeys, BM_SETCHECK, Settings_GetBlockBoundKeys() ? BST_CHECKED : BST_UNCHECKED, 0);
        SnappyDebugLog(L"WM_CREATE_INIT", st->chkSnappy);

        // anim init (snap, no boot-animation)
        SetWindowSubclass(st->chkSnappy, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkSnappy, Settings_GetSnappyJoystick(), false);
        SetWindowSubclass(st->chkBlockBoundKeys, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkBlockBoundKeys, Settings_GetBlockBoundKeys(), false);

        LayoutConfigControls(hWnd, st);
        Config_SetScrollY(hWnd, st, 0);

        SetProfileStatus(st, L"");
        return 0;
    }

    case WM_SIZE:
        if (st)
        {
            int keepScroll = st->scrollY;
            if (keepScroll != 0)
            {
                // normalize current child coordinates back to "content space"
                Config_OffsetAllChildren(hWnd, keepScroll);
                st->scrollY = 0;
            }

            LayoutConfigControls(hWnd, st);
            Config_SetScrollY(hWnd, st, keepScroll);
        }
        else
        {
            LayoutConfigControls(hWnd, st);
        }
        break;

    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (KeySettingsPanel_HandleMeasureItem(mis))
            return TRUE;
        break;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;

        // 1) KeySettings panel controls
        if (KeySettingsPanel_HandleDrawItem(dis))
            return TRUE;

        // 2) Config toggles
        if (st && dis && dis->CtlType == ODT_BUTTON &&
            ((dis->CtlID == ID_SNAPPY && st->chkSnappy == dis->hwndItem) ||
             (dis->CtlID == ID_BLOCK_BOUND_KEYS && st->chkBlockBoundKeys == dis->hwndItem)))
        {
            DrawSnappyToggleOwnerDraw(dis);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        if (st)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT thumb = Config_GetScrollThumbRect(hWnd, st);
            RECT track = Config_GetScrollTrackRect(hWnd);
            int maxScroll = Config_GetMaxScroll(hWnd, st);

            if (maxScroll > 0 && PtInRect(&thumb, pt))
            {
                st->scrollDrag = true;
                st->scrollDragStartY = pt.y;
                st->scrollDragStartScrollY = st->scrollY;
                st->scrollDragGrabOffsetY = pt.y - thumb.top;
                st->scrollDragThumbHeight = std::max(1, (int)thumb.bottom - (int)thumb.top);
                st->scrollDragMax = maxScroll;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }

            if (maxScroll > 0 && PtInRect(&track, pt))
            {
                if (pt.y < thumb.top)
                    Config_SetScrollY(hWnd, st, st->scrollY - std::max(1, Config_GetViewportHeight(hWnd) - S(hWnd, 48)));
                else if (pt.y >= thumb.bottom)
                    Config_SetScrollY(hWnd, st, st->scrollY + std::max(1, Config_GetViewportHeight(hWnd) - S(hWnd, 48)));
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (KeySettingsPanel_HandleMouse(hWnd, WM_LBUTTONDOWN, wParam, lpAdj))
        {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (st && st->scrollDrag)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT track = Config_GetScrollTrackRect(hWnd);
            int trackH = std::max(1, (int)track.bottom - (int)track.top);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, trackH - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);

            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topMin = (int)track.top;
            int topMax = (int)track.bottom - thumbH;
            if (topMax < topMin) topMax = topMin;
            int top = std::clamp(topWanted, topMin, topMax);
            double t = (double)(top - topMin) / (double)travel;
            int target = (int)std::lround(t * (double)maxScroll);

            Config_SetScrollY(hWnd, st, target);
            return 0;
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (KeySettingsPanel_HandleMouse(hWnd, WM_MOUSEMOVE, wParam, lpAdj))
        {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            if (GetCapture() == hWnd) ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        LPARAM lpAdj = Config_AdjustClientMouseLParamForScroll(st, lParam);
        if (KeySettingsPanel_HandleMouse(hWnd, WM_LBUTTONUP, wParam, lpAdj))
        {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_MOUSEWHEEL:
    {
        LPARAM lpAdj = Config_AdjustWheelLParamForScroll(hWnd, st, lParam);
        if (KeySettingsPanel_HandleMouse(hWnd, msg, wParam, lpAdj))
        {
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        if (st)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            if (lines <= 0) lines = 3;

            int linePx = S(hWnd, 18);
            int step = std::max(S(hWnd, 24), lines * linePx);
            int next = st->scrollY - ((delta / WHEEL_DELTA) * step);
            Config_SetScrollY(hWnd, st, next);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SETCURSOR:
    {
        if (!st) break;
        if ((HWND)wParam != hWnd) break;

        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        RECT thumb = Config_GetScrollThumbRect(hWnd, st);
        RECT track = Config_GetScrollTrackRect(hWnd);
        int maxScroll = Config_GetMaxScroll(hWnd, st);

        if (maxScroll > 0 && (PtInRect(&thumb, pt) || PtInRect(&track, pt)))
        {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        HWND hCombo = GetPresetCombo(hWnd);

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        bool comboOpen = (hCombo && PremiumCombo::GetDroppedState(hCombo));
        bool comboEditing = (hCombo && PremiumCombo::IsEditingItem(hCombo));
        bool allowNonCtrl = comboOpen || comboEditing;

        // Ctrl+S = save preset
        if (ctrl && (wParam == 'S' || wParam == 's'))
        {
            if (hCombo)
            {
                WPARAM wp = MAKEWPARAM((UINT)PremiumCombo::ExtraIconKind::Save, (UINT)KSP_ID_PROFILE);
                PostMessageW(hWnd, PremiumCombo::MsgExtraIcon(), wp, (LPARAM)hCombo);
            }
            return 0;
        }

        // Undo/redo shortcuts
        if (KeySettingsPanel_HandleKey(hWnd, msg, wParam, lParam))
            return 0;

        // Non-ctrl keys: do nothing unless dropdown is open
        if (!ctrl && !allowNonCtrl)
            return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // F2 = rename selected (inline) [only when dropdown open]
        if (wParam == VK_F2)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);

                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                }
                else
                {
                    PremiumCombo::BeginInlineEditSelected(hCombo, true);
                    SetProfileStatus(st, L"Type a new name and press Enter.");
                }
            }
            return 0;
        }

        // Delete = delete selected (require Shift) [only when dropdown open]
        if (wParam == VK_DELETE)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                    return 0;

                if (!shift)
                {
                    SetProfileStatus(st, L"Hold Shift and press Delete to delete.");
                    return 0;
                }

                DeletePreset_NoPopup_ConfigPage(hWnd, st, sel, true);
            }
            return 0;
        }

        return 0;
    }

    case WM_COMMAND:
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        // Snappy toggle
        if (LOWORD(wParam) == (UINT)ID_SNAPPY && HIWORD(wParam) == BN_CLICKED && st && st->chkSnappy)
        {
            SnappyDebugLog(L"WM_COMMAND_BN_CLICKED_BEFORE", st->chkSnappy, (int)LOWORD(wParam), (int)HIWORD(wParam));
            bool on = !Settings_GetSnappyJoystick();
            SendMessageW(st->chkSnappy, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);

            Settings_SetSnappyJoystick(on);
            SnappyToggle_StartAnim(st->chkSnappy, on, true);
            SnappyDebugLog(L"WM_COMMAND_BN_CLICKED_AFTER", st->chkSnappy, on ? 1 : 0, 0);

            RequestSave(hWnd);
            return 0;
        }

        if (LOWORD(wParam) == (UINT)ID_BLOCK_BOUND_KEYS && HIWORD(wParam) == BN_CLICKED && st && st->chkBlockBoundKeys)
        {
            bool on = !Settings_GetBlockBoundKeys();
            SendMessageW(st->chkBlockBoundKeys, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
            Settings_SetBlockBoundKeys(on);
            SnappyToggle_StartAnim(st->chkBlockBoundKeys, on, true);
            RequestSave(hWnd);
            return 0;
        }

        // Preset selection:
        // If user selected "+ Create New..." row, start inline create (deferred).
        if (LOWORD(wParam) == (UINT)KSP_ID_PROFILE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            HWND hCombo = (HWND)lParam;
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                    return 0;
                }
            }
            // else: fallthrough to KeySettingsPanel_HandleCommand to apply preset
        }

        if (KeySettingsPanel_HandleCommand(hWnd, wParam, lParam))
        {
            if (st && st->scrollY != 0)
                Config_RequestFullRepaint(hWnd);
            return 0;
        }

        return 0;
    }

    case WM_NCDESTROY:
        // FIX: free cached GDI resources used by graph renderer
        KeySettingsPanel_Shutdown();

        RemovePropW(hWnd, CONFIG_SCROLLY_PROP);

        if (st)
        {
            Toast_Hide(hWnd, st);
            if (st->hToast)
            {
                DestroyWindow(st->hToast);
                st->hToast = nullptr;
            }

            if (st->chkSnappy && IsWindow(st->chkSnappy))
            {
                // subclass will free state on WM_NCDESTROY of the control, but best-effort safety:
                SnappyToggle_Free(st->chkSnappy);
            }
            if (st->chkBlockBoundKeys && IsWindow(st->chkBlockBoundKeys))
            {
                SnappyToggle_Free(st->chkBlockBoundKeys);
            }

            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
