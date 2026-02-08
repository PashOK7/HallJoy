// keyboard_page_main.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "keyboard_ui.h"
#include "keyboard_ui_state.h"
#include "keyboard_ui_internal.h"

#include "keyboard_layout.h"
#include "keyboard_render.h"
#include "backend.h"
#include "bindings.h"
#include "profile_ini.h"
#include "remap_panel.h"
#include "keyboard_keysettings_panel.h"

#include "win_util.h"
#include "app_paths.h"
#include "ui_theme.h"
#include "tab_dark.h"

#include "binding_actions.h"
#include "remap_icons.h"
#include "settings.h"
#include "key_settings.h" // NEW

#pragma comment(lib, "Comctl32.lib")

// Scaling shortcut
static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static void SetSelectedHid(uint16_t hid);
static LRESULT CALLBACK KeyBtnSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData);

struct KeyboardViewMetrics
{
    float scale = 1.0f;
    int offsetX = 0;
    int offsetY = 0;
    int scaledW = 0;
    int scaledH = 0;
};

static void ComputeKeyboardViewMetrics(HWND hWnd, KeyboardViewMetrics& out)
{
    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();

    int maxX = 1;
    int maxBottom = KEYBOARD_KEY_H;
    for (int i = 0; i < n; ++i)
    {
        maxX = std::max(maxX, keys[i].x + std::max(KEYBOARD_KEY_MIN_DIM, keys[i].w));
        int bottom = keys[i].row * KEYBOARD_ROW_PITCH_Y + std::max(KEYBOARD_KEY_MIN_DIM, keys[i].h);
        if (bottom > maxBottom) maxBottom = bottom;
    }

    int modelW = KEYBOARD_MARGIN_X + maxX + KEYBOARD_MARGIN_X;
    int modelH = KEYBOARD_MARGIN_Y + maxBottom;

    int baseW = std::max(1, S(hWnd, modelW));
    int baseH = std::max(1, S(hWnd, modelH));

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int cw = std::max(1, (int)(rc.right - rc.left));
    int ch = std::max(1, (int)(rc.bottom - rc.top));

    int sidePad = S(hWnd, 24);
    int topPad = S(hWnd, 8);
    int bottomPad = S(hWnd, 12);
    int minSubH = S(hWnd, 170);

    int availW = std::max(1, cw - sidePad);
    int availH = std::max(1, ch - topPad - bottomPad - minSubH);

    float sx = (float)availW / (float)baseW;
    float sy = (float)availH / (float)baseH;
    out.scale = std::min(1.0f, std::min(sx, sy));
    out.scale = std::clamp(out.scale, 0.01f, 1.0f);

    out.scaledW = std::max(1, (int)std::lround((double)baseW * out.scale));
    out.scaledH = std::max(1, (int)std::lround((double)baseH * out.scale));

    if (out.scale >= 0.999f)
        out.offsetX = 0; // keep stable position at native size to avoid resize jitter/flicker
    else
        out.offsetX = std::max(0, (cw - out.scaledW) / 2);
    out.offsetY = topPad;
    if (out.offsetY + out.scaledH > ch - bottomPad)
        out.offsetY = std::max(0, (ch - bottomPad) - out.scaledH);
}

static void DestroyKeyboardButtons()
{
    for (HWND b : g_keyButtons)
    {
        if (b && IsWindow(b))
            DestroyWindow(b);
    }
    g_keyButtons.clear();
    g_btnByHid.fill(nullptr);
    g_hids.clear();
}

static int KeyboardBottomPx(HWND hWnd)
{
    KeyboardViewMetrics m{};
    ComputeKeyboardViewMetrics(hWnd, m);
    return m.offsetY + m.scaledH;
}

static void LayoutKeyboardButtons(HWND hWnd)
{
    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();
    if (!keys || n <= 0 || g_keyButtons.empty()) return;

    KeyboardViewMetrics m{};
    ComputeKeyboardViewMetrics(hWnd, m);

    int cnt = std::min(n, (int)g_keyButtons.size());
    HDWP hdwp = BeginDeferWindowPos(std::max(1, cnt));
    bool anyChanged = false;
    for (int i = 0; i < cnt; ++i)
    {
        HWND b = g_keyButtons[(size_t)i];
        if (!b || !IsWindow(b)) continue;

        const auto& k = keys[i];
        int baseX = S(hWnd, KEYBOARD_MARGIN_X + k.x);
        int baseY = S(hWnd, KEYBOARD_MARGIN_Y + k.row * KEYBOARD_ROW_PITCH_Y);
        int baseW = S(hWnd, std::max(KEYBOARD_KEY_MIN_DIM, k.w));
        int baseH = S(hWnd, std::max(KEYBOARD_KEY_MIN_DIM, k.h));

        int px = m.offsetX + (int)std::lround((double)baseX * m.scale);
        int py = m.offsetY + (int)std::lround((double)baseY * m.scale);
        int pw = std::max(8, (int)std::lround((double)baseW * m.scale));
        int ph = std::max(8, (int)std::lround((double)baseH * m.scale));

        RECT cur{};
        GetWindowRect(b, &cur);
        MapWindowPoints(nullptr, hWnd, (LPPOINT)&cur, 2);
        int cw = cur.right - cur.left;
        int ch = cur.bottom - cur.top;
        if (cur.left == px && cur.top == py && cw == pw && ch == ph)
            continue;

        anyChanged = true;
        if (hdwp)
        {
            hdwp = DeferWindowPos(
                hdwp, b, nullptr, px, py, pw, ph,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS | SWP_NOREDRAW);
        }
        else
        {
            SetWindowPos(
                b, nullptr, px, py, pw, ph,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS | SWP_NOREDRAW);
        }
    }

    if (hdwp)
        EndDeferWindowPos(hdwp);

    if (anyChanged)
    {
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

static void RebuildKeyboardButtons(HWND hWnd)
{
    uint16_t keepSelected = g_selectedHid;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

    DestroyKeyboardButtons();

    const KeyDef* keys = KeyboardLayout_Data();
    int n = KeyboardLayout_Count();
    for (int i = 0; i < n; ++i)
    {
        const auto& k = keys[i];

        HWND b = CreateWindowW(L"BUTTON", k.label,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);

        SetWindowLongPtrW(b, GWLP_USERDATA, (LONG_PTR)k.hid);
        SetWindowSubclass(b, KeyBtnSubclassProc, 1, (DWORD_PTR)k.hid);
        g_keyButtons.push_back(b);

        if (k.hid != 0 && k.hid < 256)
        {
            g_btnByHid[k.hid] = b;
            g_hids.push_back((uint16_t)k.hid);
        }
    }

    BackendUI_SetTrackedHids(g_hids.data(), (int)g_hids.size());
    LayoutKeyboardButtons(hWnd);

    if (keepSelected >= 256 || g_btnByHid[keepSelected] == nullptr)
        keepSelected = 0;
    SetSelectedHid(keepSelected);
}

static HWND GetBtnForHid(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return nullptr;
    return g_btnByHid[hid];
}

static void InvalidateKeyByHid(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return;
    if (HWND b = g_btnByHid[hid])
        InvalidateRect(b, nullptr, FALSE);
}

static void SetSelectedHid(uint16_t hid)
{
    uint16_t old = g_selectedHid;
    g_selectedHid = hid;

    KeySettingsPanel_SetSelectedHid(hid);

    // NEW: tell renderer which HID is currently being edited/selected (for wow-spin)
    KeyboardRender_NotifySelectedHid(hid);

    if (HWND bOld = GetBtnForHid(old))
        InvalidateRect(bOld, nullptr, FALSE);
    if (HWND bNew = GetBtnForHid(g_selectedHid))
        InvalidateRect(bNew, nullptr, FALSE);
}

static void ShowSubPage(int idx)
{
    g_activeSubTab = idx;

    // NEW: clear selection when leaving Configuration tab
    if (idx != 1 && g_selectedHid != 0)
        SetSelectedHid(0);

    if (g_hPageRemap)  ShowWindow(g_hPageRemap, idx == 0 ? SW_SHOW : SW_HIDE);
    if (g_hPageConfig) ShowWindow(g_hPageConfig, idx == 1 ? SW_SHOW : SW_HIDE);
    if (g_hPageTester) ShowWindow(g_hPageTester, idx == 2 ? SW_SHOW : SW_HIDE);
    if (g_hPageGlobal) ShowWindow(g_hPageGlobal, idx == 3 ? SW_SHOW : SW_HIDE);

    if (g_hSubTab) InvalidateRect(g_hSubTab, nullptr, FALSE);
}

static void ResizeSubUi(HWND hWnd)
{
    if (!g_hSubTab) return;

    RECT rc{};
    GetClientRect(hWnd, &rc);

    int kbBottom = KeyboardBottomPx(hWnd);
    int x = S(hWnd, 12);
    int y = kbBottom + S(hWnd, 12);

    int w = (rc.right - rc.left) - S(hWnd, 24);
    int h = (rc.bottom - rc.top) - y - S(hWnd, 12);
    if (w < 10) w = 10;
    if (h < 10) h = 10;

    SetWindowPos(g_hSubTab, nullptr, x, y, w, h, SWP_NOZORDER);

    RECT tabRc{};
    GetClientRect(g_hSubTab, &tabRc);
    TabCtrl_AdjustRect(g_hSubTab, FALSE, &tabRc);

    int pw = tabRc.right - tabRc.left;
    int ph = tabRc.bottom - tabRc.top;

    if (g_hPageRemap)
        SetWindowPos(g_hPageRemap, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageConfig)
        SetWindowPos(g_hPageConfig, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageTester)
        SetWindowPos(g_hPageTester, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);

    if (g_hPageGlobal)
        SetWindowPos(g_hPageGlobal, nullptr, tabRc.left, tabRc.top, pw, ph, SWP_NOZORDER);
}

// ----- Right-click unbind on key button + drag bound icon (subclass) -----

static constexpr UINT_PTR KEYDRAG_TIMER_ID = 9101;
static constexpr UINT_PTR KEYSWAP_TIMER_ID = 9102;
static constexpr UINT_PTR KEYDELETE_TIMER_ID = 9103;

// -----------------------------------------------------------------------------
// Helpers for action type
// -----------------------------------------------------------------------------
static bool IsButtonAction(BindAction a)
{
    switch (a)
    {
    case BindAction::Btn_A: case BindAction::Btn_B: case BindAction::Btn_X: case BindAction::Btn_Y:
    case BindAction::Btn_LB: case BindAction::Btn_RB:
    case BindAction::Btn_Back: case BindAction::Btn_Start: case BindAction::Btn_Guide:
    case BindAction::Btn_LS: case BindAction::Btn_RS:
    case BindAction::Btn_DU: case BindAction::Btn_DD: case BindAction::Btn_DL: case BindAction::Btn_DR:
        return true;
    default:
        return false;
    }
}

static bool ActionToGameButton(BindAction a, GameButton& out)
{
    switch (a)
    {
    case BindAction::Btn_A: out = GameButton::A; return true;
    case BindAction::Btn_B: out = GameButton::B; return true;
    case BindAction::Btn_X: out = GameButton::X; return true;
    case BindAction::Btn_Y: out = GameButton::Y; return true;
    case BindAction::Btn_LB: out = GameButton::LB; return true;
    case BindAction::Btn_RB: out = GameButton::RB; return true;
    case BindAction::Btn_Back:  out = GameButton::Back;  return true;
    case BindAction::Btn_Start: out = GameButton::Start; return true;
    case BindAction::Btn_Guide: out = GameButton::Guide; return true;
    case BindAction::Btn_LS: out = GameButton::LS; return true;
    case BindAction::Btn_RS: out = GameButton::RS; return true;
    case BindAction::Btn_DU: out = GameButton::DpadUp; return true;
    case BindAction::Btn_DD: out = GameButton::DpadDown; return true;
    case BindAction::Btn_DL: out = GameButton::DpadLeft; return true;
    case BindAction::Btn_DR: out = GameButton::DpadRight; return true;
    default: return false;
    }
}

static int FindIconIdxForAction(BindAction a);

struct DisplayedKeyIconEntry
{
    BindAction action{};
    int padIndex = 0;
    int iconIdx = -1;
};

static int CollectDisplayedActionsByHid(uint16_t hid, DisplayedKeyIconEntry out[4])
{
    if (out)
    {
        for (int i = 0; i < 4; ++i) out[i] = DisplayedKeyIconEntry{};
    }
    if (!hid) return 0;

    int pads = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
    int count = 0;
    for (int pad = 0; pad < pads; ++pad)
    {
        if (count >= 4) break;
        BindAction act{};
        if (BindingActions_TryGetByHidForPad(pad, hid, act))
        {
            int iconIdx = FindIconIdxForAction(act);
            if (iconIdx < 0) continue;
            if (out)
            {
                out[count].action = act;
                out[count].padIndex = std::clamp(pad, 0, 3);
                out[count].iconIdx = iconIdx;
            }
            ++count;
        }
    }
    return count;
}

static bool FindDisplayedActionByHid(uint16_t hid, BindAction& outAction, int& outPadIndex)
{
    outPadIndex = 0;
    DisplayedKeyIconEntry e[4]{};
    int n = CollectDisplayedActionsByHid(hid, e);
    if (n <= 0) return false;
    outAction = e[0].action;
    outPadIndex = e[0].padIndex;
    return true;
}

static int BuildMiniIconRectsForButton(HWND hBtn, int iconCount, RECT outRects[4])
{
    if (!hBtn || !outRects) return 0;
    iconCount = std::clamp(iconCount, 0, 4);
    if (iconCount <= 0) return 0;

    RECT rc{};
    GetClientRect(hBtn, &rc);
    RECT iconArea = rc;
    InflateRect(&iconArea, -1, -1);

    int iw = iconArea.right - iconArea.left;
    int ih = iconArea.bottom - iconArea.top;
    if (iw <= 10 || ih <= 10) return 0;

    int baseSize = WinUtil_ScalePx(hBtn, (int)Settings_GetBoundKeyIconSizePx());
    baseSize = std::clamp(baseSize, 8, std::min(iw, ih));
    int gap = std::clamp(WinUtil_ScalePx(hBtn, 2), 1, 5);

    for (int i = 0; i < 4; ++i) outRects[i] = RECT{};

    if (iconCount == 1)
    {
        int size = std::clamp(baseSize, 8, std::min(iw, ih));
        int x = (iconArea.left + iconArea.right - size) / 2;
        int y = (iconArea.top + iconArea.bottom - size) / 2;
        outRects[0] = RECT{ x, y, x + size, y + size };
        return 1;
    }

    if (iconCount == 2)
    {
        int cellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(cellW, ih));
        int y = iconArea.top + (ih - size) / 2;
        int x0 = iconArea.left + (cellW - size) / 2;
        int x1 = iconArea.left + cellW + gap + (cellW - size) / 2;
        outRects[0] = RECT{ x0, y, x0 + size, y + size };
        outRects[1] = RECT{ x1, y, x1 + size, y + size };
        return 2;
    }

    if (iconCount == 3)
    {
        int rowH = std::max(8, (ih - gap) / 2);
        int topCellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(topCellW, rowH));
        int topY = iconArea.top + (rowH - size) / 2;
        int x0 = iconArea.left + (topCellW - size) / 2;
        int x1 = iconArea.left + topCellW + gap + (topCellW - size) / 2;
        int botY = iconArea.top + rowH + gap + (rowH - size) / 2;
        int x2 = iconArea.left + (iw - size) / 2;
        outRects[0] = RECT{ x0, topY, x0 + size, topY + size };
        outRects[1] = RECT{ x1, topY, x1 + size, topY + size };
        outRects[2] = RECT{ x2, botY, x2 + size, botY + size };
        return 3;
    }

    int cellW = std::max(8, (iw - gap) / 2);
    int cellH = std::max(8, (ih - gap) / 2);
    int size = std::clamp(baseSize, 8, std::min(cellW, cellH));
    for (int i = 0; i < 4; ++i)
    {
        int col = i % 2;
        int row = i / 2;
        int cellX = iconArea.left + col * (cellW + gap);
        int cellY = iconArea.top + row * (cellH + gap);
        int x = cellX + (cellW - size) / 2;
        int y = cellY + (cellH - size) / 2;
        outRects[i] = RECT{ x, y, x + size, y + size };
    }
    return 4;
}

static bool HitTestDisplayedIconOnButton(HWND hBtn, uint16_t hid, POINT ptClient, DisplayedKeyIconEntry& outEntry, RECT& outRect)
{
    outEntry = DisplayedKeyIconEntry{};
    outRect = RECT{};

    DisplayedKeyIconEntry entries[4]{};
    int count = CollectDisplayedActionsByHid(hid, entries);
    if (count <= 0) return false;

    RECT rects[4]{};
    int rectCount = BuildMiniIconRectsForButton(hBtn, count, rects);
    if (rectCount <= 0) return false;

    for (int i = 0; i < rectCount && i < count; ++i)
    {
        if (PtInRect(&rects[i], ptClient))
        {
            outEntry = entries[i];
            outRect = rects[i];
            return true;
        }
    }

    return false;
}

static int GetRemapStyleVariantForPad(int padIndex)
{
    int totalPads = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
    padIndex = std::clamp(padIndex, 0, 3);
    if (totalPads <= 1) return 0;
    return std::clamp(Bindings_GetPadStyleVariant(padIndex), 1, 4);
}

// -----------------------------------------------------------------------------
// Swap "fly back" animation (dst icon flies to src)
// -----------------------------------------------------------------------------
struct SwapFlyState
{
    bool running = false;

    HWND hPage = nullptr; // page main window for timer

    uint16_t srcHid = 0;
    uint16_t dstHid = 0;

    BindAction pendingAct{}; // the action that should end up on src
    int pendingPadIndex = 0;
    int iconIdx = -1;

    DWORD startTick = 0;
    DWORD durationMs = 170;

    float x0 = 0, y0 = 0; // start top-left (screen)
    float x1 = 0, y1 = 0; // end top-left (screen)

    // ghost
    HWND hGhost = nullptr;
    int  size = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
};

static SwapFlyState g_swapfly;

static void SwapFly_FreeSurface()
{
    if (g_swapfly.memDC)
    {
        if (g_swapfly.oldBmp) SelectObject(g_swapfly.memDC, g_swapfly.oldBmp);
        g_swapfly.oldBmp = nullptr;
    }
    if (g_swapfly.bmp)
    {
        DeleteObject(g_swapfly.bmp);
        g_swapfly.bmp = nullptr;
    }
    if (g_swapfly.memDC)
    {
        DeleteDC(g_swapfly.memDC);
        g_swapfly.memDC = nullptr;
    }
    g_swapfly.bits = nullptr;
}

static void EnsureGhostClassRegistered(HINSTANCE hInst)
{
    static bool reg = false;
    if (reg) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KeyboardBindGhostWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    reg = true;
}

static void SwapFly_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_swapfly.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_swapfly.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_swapfly.hGhost)
        ShowWindow(g_swapfly.hGhost, SW_HIDE);
}

static bool SwapFly_EnsureSurface(HWND hwndForDpi)
{
    if (!g_swapfly.hGhost) return false;

    int want = (int)Settings_GetBoundKeyIconSizePx();
    int size = WinUtil_ScalePx(hwndForDpi, want);
    size = std::clamp(size, 12, 128);

    if (g_swapfly.size == size && g_swapfly.memDC && g_swapfly.bmp && g_swapfly.bits)
        return true;

    g_swapfly.size = size;
    SwapFly_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_swapfly.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_swapfly.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_swapfly.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_swapfly.memDC || !g_swapfly.bmp || !g_swapfly.bits)
    {
        SwapFly_FreeSurface();
        return false;
    }

    g_swapfly.oldBmp = SelectObject(g_swapfly.memDC, g_swapfly.bmp);
    return true;
}

static int FindIconIdxForAction(BindAction a)
{
    int n = RemapIcons_Count();
    for (int i = 0; i < n; ++i)
        if (RemapIcons_Get(i).action == a) return i;
    return -1;
}

static void SwapFly_RenderIcon()
{
    if (!g_swapfly.memDC || !g_swapfly.bits) return;
    int sz = g_swapfly.size;
    if (sz <= 0) return;

    std::memset(g_swapfly.bits, 0, (size_t)sz * (size_t)sz * 4);
    RECT rc{ 0,0,sz,sz };
    int styleVariant = GetRemapStyleVariantForPad(g_swapfly.pendingPadIndex);
    RemapIcons_DrawGlyphAA(g_swapfly.memDC, rc, g_swapfly.iconIdx, true, 0.075f, styleVariant);
}

static void SwapFly_UpdateLayered(int x, int y, BYTE alpha)
{
    if (!g_swapfly.hGhost || !g_swapfly.memDC) return;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ x, y };
    SIZE  sz{ g_swapfly.size, g_swapfly.size };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_swapfly.hGhost, screen, &ptPos, &sz, g_swapfly.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_swapfly.hGhost, SW_SHOWNOACTIVATE);
}

static float EaseOutCubic01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static void SwapFly_Stop(bool commit)
{
    if (!g_swapfly.running) return;

    KillTimer(g_swapfly.hPage, KEYSWAP_TIMER_ID);

    if (g_swapfly.hGhost)
        ShowWindow(g_swapfly.hGhost, SW_HIDE);

    g_swapfly.running = false;

    if (commit)
    {
        if (g_swapfly.srcHid != 0)
        {
            BindingActions_ApplyForPad(g_swapfly.pendingPadIndex, g_swapfly.pendingAct, g_swapfly.srcHid);
            Profile_SaveIni(AppPaths_BindingsIni().c_str());
        }

        InvalidateKeyByHid(g_swapfly.srcHid);
        InvalidateKeyByHid(g_swapfly.dstHid);
    }

    g_swapfly.srcHid = 0;
    g_swapfly.dstHid = 0;
    g_swapfly.pendingAct = {};
    g_swapfly.pendingPadIndex = 0;
    g_swapfly.iconIdx = -1;
    g_swapfly.hPage = nullptr;
    g_swapfly.startTick = 0;
}

static void SwapFly_Tick()
{
    if (!g_swapfly.running) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_swapfly.startTick;

    float t = (g_swapfly.durationMs > 0) ? (float)dt / (float)g_swapfly.durationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    float e = EaseOutCubic01(t);

    float x = g_swapfly.x0 + (g_swapfly.x1 - g_swapfly.x0) * e;
    float y = g_swapfly.y0 + (g_swapfly.y1 - g_swapfly.y0) * e;

    SwapFly_UpdateLayered((int)std::lround(x), (int)std::lround(y), 225);

    if (t >= 1.0f - 1e-4f)
        SwapFly_Stop(true);
}

static bool SwapFly_Start(HWND hPage, uint16_t srcHid, uint16_t dstHid, BindAction pendingAct, int pendingPadIndex)
{
    if (!hPage) return false;
    if (!srcHid || !dstHid) return false;

    HWND hSrcBtn = GetBtnForHid(srcHid);
    HWND hDstBtn = GetBtnForHid(dstHid);
    if (!hSrcBtn || !hDstBtn) return false;

    int iconIdx = FindIconIdxForAction(pendingAct);
    if (iconIdx < 0) return false;

    if (g_swapfly.running)
        SwapFly_Stop(true);

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    SwapFly_EnsureGhostWindow(hInst, ownerTop);
    if (!SwapFly_EnsureSurface(hPage))
        return false;

    g_swapfly.hPage = hPage;
    g_swapfly.running = true;
    g_swapfly.srcHid = srcHid;
    g_swapfly.dstHid = dstHid;
    g_swapfly.pendingAct = pendingAct;
    g_swapfly.pendingPadIndex = std::clamp(pendingPadIndex, 0, 3);
    g_swapfly.iconIdx = iconIdx;
    g_swapfly.startTick = GetTickCount();
    g_swapfly.durationMs = 170;

    SwapFly_RenderIcon();

    RECT rcSrc{}, rcDst{};
    GetWindowRect(hSrcBtn, &rcSrc);
    GetWindowRect(hDstBtn, &rcDst);

    int cx0 = (rcDst.left + rcDst.right) / 2;
    int cy0 = (rcDst.top + rcDst.bottom) / 2;

    int cx1 = (rcSrc.left + rcSrc.right) / 2;
    int cy1 = (rcSrc.top + rcSrc.bottom) / 2;

    g_swapfly.x0 = (float)(cx0 - g_swapfly.size / 2);
    g_swapfly.y0 = (float)(cy0 - g_swapfly.size / 2);

    g_swapfly.x1 = (float)(cx1 - g_swapfly.size / 2);
    g_swapfly.y1 = (float)(cy1 - g_swapfly.size / 2);

    SwapFly_UpdateLayered((int)std::lround(g_swapfly.x0), (int)std::lround(g_swapfly.y0), 225);
    SetTimer(hPage, KEYSWAP_TIMER_ID, 15, nullptr);

    InvalidateKeyByHid(srcHid);
    return true;
}

// -----------------------------------------------------------------------------
// Delete shrink animation (RMB unbind): icon shrinks to zero on the key
// -----------------------------------------------------------------------------
struct KeyDeleteShrinkState
{
    bool running = false;
    HWND hPage = nullptr;

    HWND hGhost = nullptr;
    int  size = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;

    int iconIdx = -1;
    int padIndex = 0;

    float x = 0.0f; // top-left screen
    float y = 0.0f;

    DWORD startTick = 0;
    DWORD durationMs = 140;
};

static KeyDeleteShrinkState g_kdel;

static void KeyDel_FreeSurface()
{
    if (g_kdel.memDC)
    {
        if (g_kdel.oldBmp) SelectObject(g_kdel.memDC, g_kdel.oldBmp);
        g_kdel.oldBmp = nullptr;
    }
    if (g_kdel.bmp)
    {
        DeleteObject(g_kdel.bmp);
        g_kdel.bmp = nullptr;
    }
    if (g_kdel.memDC)
    {
        DeleteDC(g_kdel.memDC);
        g_kdel.memDC = nullptr;
    }
    g_kdel.bits = nullptr;
    g_kdel.size = 0;
}

static void KeyDel_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_kdel.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_kdel.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_kdel.hGhost)
        ShowWindow(g_kdel.hGhost, SW_HIDE);
}

static bool KeyDel_EnsureSurface(HWND hwndForDpi)
{
    if (!g_kdel.hGhost) return false;

    int want = (int)Settings_GetBoundKeyIconSizePx();
    int sz = WinUtil_ScalePx(hwndForDpi, want);
    sz = std::clamp(sz, 12, 128);

    if (g_kdel.size == sz && g_kdel.memDC && g_kdel.bmp && g_kdel.bits)
        return true;

    g_kdel.size = sz;
    KeyDel_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_kdel.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz;
    bi.bmiHeader.biHeight = -sz;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_kdel.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_kdel.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_kdel.memDC || !g_kdel.bmp || !g_kdel.bits)
    {
        KeyDel_FreeSurface();
        return false;
    }

    g_kdel.oldBmp = SelectObject(g_kdel.memDC, g_kdel.bmp);
    return true;
}

static void KeyDel_RenderScaled(float scale01)
{
    if (!g_kdel.memDC || !g_kdel.bits) return;
    int sz = g_kdel.size;
    if (sz <= 0) return;

    scale01 = std::clamp(scale01, 0.0f, 1.0f);

    std::memset(g_kdel.bits, 0, (size_t)sz * (size_t)sz * 4);

    int d = (int)std::lround((float)sz * scale01);

    // IMPORTANT: no 1px leftover
    if (d <= 1) return;

    d = std::clamp(d, 2, sz);

    int x = (sz - d) / 2;
    int y = (sz - d) / 2;
    RECT rc{ x, y, x + d, y + d };

    int styleVariant = GetRemapStyleVariantForPad(g_kdel.padIndex);
    RemapIcons_DrawGlyphAA(g_kdel.memDC, rc, g_kdel.iconIdx, true, 0.075f, styleVariant);
}

static void KeyDel_UpdateLayered(BYTE alpha = 225)
{
    if (!g_kdel.hGhost || !g_kdel.memDC) return;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ (int)std::lround(g_kdel.x), (int)std::lround(g_kdel.y) };
    SIZE  sz{ g_kdel.size, g_kdel.size };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_kdel.hGhost, screen, &ptPos, &sz, g_kdel.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_kdel.hGhost, SW_SHOWNOACTIVATE);
}

static void KeyDel_Stop()
{
    if (!g_kdel.running) return;

    if (g_kdel.hPage)
        KillTimer(g_kdel.hPage, KEYDELETE_TIMER_ID);

    if (g_kdel.hGhost)
        ShowWindow(g_kdel.hGhost, SW_HIDE);

    g_kdel.running = false;
    g_kdel.hPage = nullptr;
    g_kdel.iconIdx = -1;
    g_kdel.padIndex = 0;
    g_kdel.startTick = 0;
}

static bool KeyDel_Start(HWND hPage, HWND hBtn, int iconIdx, int padIndex)
{
    if (!hPage || !hBtn) return false;
    if (iconIdx < 0) return false;

    if (g_kdel.running)
        KeyDel_Stop();

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    KeyDel_EnsureGhostWindow(hInst, ownerTop);
    if (!KeyDel_EnsureSurface(hBtn))
        return false;

    RECT rcBtn{};
    GetWindowRect(hBtn, &rcBtn);

    int cx = (rcBtn.left + rcBtn.right) / 2;
    int cy = (rcBtn.top + rcBtn.bottom) / 2;

    g_kdel.hPage = hPage;
    g_kdel.running = true;
    g_kdel.iconIdx = iconIdx;
    g_kdel.padIndex = std::clamp(padIndex, 0, 3);

    g_kdel.x = (float)(cx - g_kdel.size / 2);
    g_kdel.y = (float)(cy - g_kdel.size / 2);

    g_kdel.startTick = GetTickCount();
    g_kdel.durationMs = 140;

    KeyDel_RenderScaled(1.0f);
    KeyDel_UpdateLayered(225);

    SetTimer(hPage, KEYDELETE_TIMER_ID, 15, nullptr);
    return true;
}

static void KeyDel_Tick()
{
    if (!g_kdel.running) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_kdel.startTick;

    float t = (g_kdel.durationMs > 0) ? (float)dt / (float)g_kdel.durationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float e = EaseOutCubic01(t);
    float scale = 1.0f - e;

    if (t >= 1.0f - 1e-4f)
        scale = 0.0f;

    KeyDel_RenderScaled(scale);
    KeyDel_UpdateLayered(225);

    if (t >= 1.0f - 1e-4f)
        KeyDel_Stop();
}

// -----------------------------------------------------------------------------
// Dragging state (existing)
// -----------------------------------------------------------------------------
struct KeyIconDragState
{
    bool dragging = false;
    bool growing = false;

    uint16_t srcHid = 0;
    uint16_t hoverHid = 0;
    int srcPadIndex = 0;

    BindAction action{};
    int iconIdx = -1;

    HWND hPage = nullptr;

    HWND hGhost = nullptr;
    int ghostW = 0;
    int ghostH = 0;

    HDC     memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;

    float gx = 0.0f, gy = 0.0f;
    float tx = 0.0f, ty = 0.0f;
    DWORD lastTick = 0;
    DWORD growStartTick = 0;
    DWORD growDurationMs = 0;
    float growFromScale = 1.0f;

    // NEW: shrink-out animation when dropping to empty (unbind)
    bool shrinking = false;
    DWORD shrinkStartTick = 0;
    DWORD shrinkDurationMs = 140;
};

static KeyIconDragState g_kdrag;

static void KeyDrag_FreeSurface()
{
    if (g_kdrag.memDC)
    {
        if (g_kdrag.oldBmp) SelectObject(g_kdrag.memDC, g_kdrag.oldBmp);
        g_kdrag.oldBmp = nullptr;
    }
    if (g_kdrag.bmp)
    {
        DeleteObject(g_kdrag.bmp);
        g_kdrag.bmp = nullptr;
    }
    if (g_kdrag.memDC)
    {
        DeleteDC(g_kdrag.memDC);
        g_kdrag.memDC = nullptr;
    }
    g_kdrag.bits = nullptr;
}

static bool KeyDrag_EnsureSurface()
{
    if (!g_kdrag.hGhost) return false;
    if (g_kdrag.ghostW <= 0 || g_kdrag.ghostH <= 0) return false;

    KeyDrag_FreeSurface();

    HDC screen = GetDC(nullptr);
    g_kdrag.memDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_kdrag.ghostW;
    bi.bmiHeader.biHeight = -g_kdrag.ghostH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_kdrag.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &g_kdrag.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!g_kdrag.memDC || !g_kdrag.bmp || !g_kdrag.bits)
    {
        KeyDrag_FreeSurface();
        return false;
    }

    g_kdrag.oldBmp = SelectObject(g_kdrag.memDC, g_kdrag.bmp);
    return true;
}

static void KeyDrag_EnsureGhostWindow(HINSTANCE hInst, HWND hOwnerTop)
{
    if (g_kdrag.hGhost) return;

    EnsureGhostClassRegistered(hInst);

    g_kdrag.hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"KeyboardBindGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwnerTop, nullptr, hInst, nullptr);

    if (g_kdrag.hGhost)
        ShowWindow(g_kdrag.hGhost, SW_HIDE);
}

static void KeyDrag_UpdateLayered(int x, int y)
{
    if (!g_kdrag.hGhost || !g_kdrag.memDC) return;

    BYTE alpha = 255;

    HDC screen = GetDC(nullptr);

    POINT ptPos{ x, y };
    SIZE  sz{ g_kdrag.ghostW, g_kdrag.ghostH };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_kdrag.hGhost, screen, &ptPos, &sz, g_kdrag.memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(g_kdrag.hGhost, SW_SHOWNOACTIVATE);
}

static void KeyDrag_Hide()
{
    if (g_kdrag.hGhost)
        ShowWindow(g_kdrag.hGhost, SW_HIDE);
}

static void KeyDrag_Stop()
{
    if (!g_kdrag.dragging && !g_kdrag.shrinking) return;

    uint16_t oldSrc = g_kdrag.srcHid;

    g_kdrag.dragging = false;
    g_kdrag.growing = false;
    g_kdrag.shrinking = false;

    if (g_kdrag.hPage)
    {
        KillTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID);
        if (GetCapture() == g_kdrag.hPage)
            ReleaseCapture();
    }

    KeyboardUI_SetDragHoverHid(0);
    KeyDrag_Hide();

    g_kdrag.srcHid = 0;
    g_kdrag.hoverHid = 0;
    g_kdrag.srcPadIndex = 0;
    g_kdrag.action = {};
    g_kdrag.iconIdx = -1;
    g_kdrag.lastTick = 0;
    g_kdrag.growStartTick = 0;
    g_kdrag.growDurationMs = 0;
    g_kdrag.growFromScale = 1.0f;
    g_kdrag.hPage = nullptr;

    InvalidateKeyByHid(oldSrc);
}

static bool KeyDrag_TryPickTargetKey(HWND hPage, POINT ptScreen, uint16_t& outHid, RECT& outRc)
{
    outHid = 0;
    outRc = RECT{};

    HWND w = WindowFromPoint(ptScreen);
    if (w)
    {
        for (HWND cur = w; cur; cur = GetParent(cur))
        {
            if (GetParent(cur) == hPage)
            {
                uint16_t hid = (uint16_t)GetWindowLongPtrW(cur, GWLP_USERDATA);
                if (hid != 0 && KeyboardUI_HasHid(hid))
                {
                    GetWindowRect(cur, &outRc);
                    outHid = hid;
                    return true;
                }
                break;
            }
            if (cur == hPage) break;
        }
    }

    int thr = S(hPage, 42);
    int thr2 = thr * thr;

    int best = INT_MAX;
    uint16_t bestHid = 0;
    RECT bestRc{};

    for (uint16_t hid : g_hids)
    {
        HWND btn = g_btnByHid[hid];
        if (!btn || !IsWindowVisible(btn)) continue;

        RECT rc{};
        GetWindowRect(btn, &rc);

        int dx = 0;
        if (ptScreen.x < rc.left) dx = rc.left - ptScreen.x;
        else if (ptScreen.x > rc.right) dx = ptScreen.x - rc.right;

        int dy = 0;
        if (ptScreen.y < rc.top) dy = rc.top - ptScreen.y;
        else if (ptScreen.y > rc.bottom) dy = ptScreen.y - rc.bottom;

        int d2 = dx * dx + dy * dy;
        if (d2 < best)
        {
            best = d2;
            bestHid = hid;
            bestRc = rc;
        }
    }

    if (bestHid != 0 && best <= thr2)
    {
        outHid = bestHid;
        outRc = bestRc;
        return true;
    }

    return false;
}

static void KeyDrag_RenderGlyphToSurfaceScaled(float scale01)
{
    if (!g_kdrag.memDC || !g_kdrag.bits) return;

    int sz = g_kdrag.ghostW;
    if (sz <= 0 || g_kdrag.ghostH != sz) return;

    scale01 = std::clamp(scale01, 0.0f, 1.0f);

    std::memset(g_kdrag.bits, 0, (size_t)sz * (size_t)sz * 4);

    int d = (int)std::lround((float)sz * scale01);

    // IMPORTANT: avoid 1px artifact
    if (d <= 1) return;

    d = std::clamp(d, 2, sz);

    int x = (sz - d) / 2;
    int y = (sz - d) / 2;
    RECT rc{ x, y, x + d, y + d };

    int styleVariant = GetRemapStyleVariantForPad(g_kdrag.srcPadIndex);
    RemapIcons_DrawGlyphAA(g_kdrag.memDC, rc, g_kdrag.iconIdx, true, 0.135f, styleVariant);
}

static void KeyDrag_BeginShrinkOut()
{
    if (!g_kdrag.hPage) return;
    if (!g_kdrag.hGhost) return;

    g_kdrag.dragging = false;
    g_kdrag.growing = false;
    g_kdrag.shrinking = true;
    g_kdrag.shrinkStartTick = GetTickCount();
    g_kdrag.shrinkDurationMs = 140;

    g_kdrag.hoverHid = 0;
    KeyboardUI_SetDragHoverHid(0);

    // release capture so UI returns to normal
    if (GetCapture() == g_kdrag.hPage)
        ReleaseCapture();

    // keep timer running (reuse same timer id)
    UINT tickMs = std::clamp(Settings_GetUIRefreshMs(), 1u, 50u);
    SetTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID, tickMs, nullptr);
}

static void KeyDrag_ShrinkTick()
{
    if (!g_kdrag.shrinking || !g_kdrag.hPage) return;

    DWORD now = GetTickCount();
    DWORD dt = now - g_kdrag.shrinkStartTick;

    float t = (g_kdrag.shrinkDurationMs > 0) ? (float)dt / (float)g_kdrag.shrinkDurationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float e = EaseOutCubic01(t);
    float scale = 1.0f - e;

    if (t >= 1.0f - 1e-4f)
        scale = 0.0f;

    KeyDrag_RenderGlyphToSurfaceScaled(scale);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));

    if (t >= 1.0f - 1e-4f)
    {
        g_kdrag.shrinking = false;
        KeyDrag_Hide();

        KillTimer(g_kdrag.hPage, KEYDRAG_TIMER_ID);

        // fully reset state
        g_kdrag.srcHid = 0;
        g_kdrag.hoverHid = 0;
        g_kdrag.srcPadIndex = 0;
        g_kdrag.action = {};
        g_kdrag.iconIdx = -1;
        g_kdrag.lastTick = 0;
        g_kdrag.growing = false;
        g_kdrag.growStartTick = 0;
        g_kdrag.growDurationMs = 0;
        g_kdrag.growFromScale = 1.0f;
        g_kdrag.hPage = nullptr;
    }
}

static void KeyDrag_Tick()
{
    if (!g_kdrag.hPage) return;

    if (g_kdrag.shrinking)
    {
        KeyDrag_ShrinkTick();
        return;
    }

    if (!g_kdrag.dragging) return;

    POINT pt{};
    GetCursorPos(&pt);

    uint16_t hid = 0;
    RECT rcKey{};
    KeyDrag_TryPickTargetKey(g_kdrag.hPage, pt, hid, rcKey);

    g_kdrag.hoverHid = hid;
    KeyboardUI_SetDragHoverHid(hid);

    if (hid != 0)
    {
        int cx = (rcKey.left + rcKey.right) / 2;
        int cy = (rcKey.top + rcKey.bottom) / 2;
        g_kdrag.tx = (float)(cx - g_kdrag.ghostW / 2);
        g_kdrag.ty = (float)(cy - g_kdrag.ghostH / 2);
    }
    else
    {
        g_kdrag.tx = (float)(pt.x - g_kdrag.ghostW / 2);
        g_kdrag.ty = (float)(pt.y - g_kdrag.ghostH / 2);
    }

    DWORD now = GetTickCount();
    float dt = 0.016f;
    if (g_kdrag.lastTick != 0)
    {
        dt = (float)(now - g_kdrag.lastTick) / 1000.0f;
        dt = std::clamp(dt, 0.001f, 0.050f);
    }
    g_kdrag.lastTick = now;

    const float lambda = (hid != 0) ? 24.0f : 18.0f;
    float a = 1.0f - expf(-lambda * dt);

    g_kdrag.gx += (g_kdrag.tx - g_kdrag.gx) * a;
    g_kdrag.gy += (g_kdrag.ty - g_kdrag.gy) * a;

    float renderScale = 1.0f;
    if (g_kdrag.growing)
    {
        DWORD growDt = now - g_kdrag.growStartTick;
        float gt = (g_kdrag.growDurationMs > 0) ? (float)growDt / (float)g_kdrag.growDurationMs : 1.0f;
        gt = std::clamp(gt, 0.0f, 1.0f);
        float ge = EaseOutCubic01(gt);
        renderScale = g_kdrag.growFromScale + (1.0f - g_kdrag.growFromScale) * ge;
        if (gt >= 1.0f - 1e-4f)
        {
            g_kdrag.growing = false;
            renderScale = 1.0f;
        }
    }

    KeyDrag_RenderGlyphToSurfaceScaled(renderScale);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));
}

static bool HitTestGearMarker(HWND hBtn, POINT ptClient)
{
    if (!hBtn) return false;

    RECT rc{};
    GetClientRect(hBtn, &rc);

    RECT inner = rc;
    InflateRect(&inner, -3, -3);

    // MUST match DrawOverrideGearMarkerAA() geometry in keyboard_render.cpp
    int d = WinUtil_ScalePx(hBtn, 11);
    d = std::clamp(d, 7, 24);

    int pad = WinUtil_ScalePx(hBtn, 0);
    pad = std::clamp(pad, 0, 24);

    int x = inner.right - pad - d;
    int y = inner.top + pad;

    if (x < inner.left + 1) x = inner.left + 1;
    if (y < inner.top + 1) y = inner.top + 1;

    RECT r{ x, y, x + d, y + d };

    return (ptClient.x >= r.left && ptClient.x < r.right &&
        ptClient.y >= r.top && ptClient.y < r.bottom);
}

static bool StartKeyDragFromButton(HWND hBtn, uint16_t hid, POINT ptClient)
{
    if (g_activeSubTab != 0) return false;

    if (g_swapfly.running)
        SwapFly_Stop(true);

    if (!hid) return false;
    DisplayedKeyIconEntry picked{};
    RECT pickedRc{};
    if (!HitTestDisplayedIconOnButton(hBtn, hid, ptClient, picked, pickedRc))
        return false;

    BindAction act = picked.action;
    int srcPadIndex = picked.padIndex;
    int iconIdx = picked.iconIdx;
    if (iconIdx < 0)
        return false;

    HWND hPage = GetParent(hBtn);
    if (!hPage) return false;

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    KeyDrag_Stop();

    g_kdrag.dragging = true;
    g_kdrag.growing = false;
    g_kdrag.shrinking = false;
    g_kdrag.srcHid = hid;
    g_kdrag.hoverHid = 0;
    g_kdrag.srcPadIndex = srcPadIndex;
    g_kdrag.action = act;
    g_kdrag.iconIdx = iconIdx;
    g_kdrag.hPage = hPage;

    g_kdrag.ghostW = WinUtil_ScalePx(hBtn, (int)Settings_GetDragIconSizePx());
    g_kdrag.ghostH = g_kdrag.ghostW;

    KeyDrag_EnsureGhostWindow(hInst, ownerTop);
    if (!KeyDrag_EnsureSurface())
        return false;

    POINT iconCenter{
        (pickedRc.left + pickedRc.right) / 2,
        (pickedRc.top + pickedRc.bottom) / 2 };
    ClientToScreen(hBtn, &iconCenter);

    g_kdrag.gx = (float)(iconCenter.x - g_kdrag.ghostW / 2);
    g_kdrag.gy = (float)(iconCenter.y - g_kdrag.ghostH / 2);
    g_kdrag.tx = g_kdrag.gx;
    g_kdrag.ty = g_kdrag.gy;
    g_kdrag.lastTick = 0;
    int miniW = std::max<int>(1, (int)(pickedRc.right - pickedRc.left));
    g_kdrag.growFromScale = std::clamp((float)miniW / (float)std::max<int>(1, g_kdrag.ghostW), 0.20f, 1.0f);
    g_kdrag.growStartTick = GetTickCount();
    g_kdrag.growDurationMs = 120;
    g_kdrag.growing = (g_kdrag.growFromScale < 0.995f);

    SetFocus(hPage);
    SetCapture(hPage);

    UINT tickMs = std::clamp(Settings_GetUIRefreshMs(), 1u, 50u);
    SetTimer(hPage, KEYDRAG_TIMER_ID, tickMs, nullptr);

    KeyboardUI_SetDragHoverHid(0);
    InvalidateRect(hBtn, nullptr, FALSE);
    UpdateWindow(hBtn);

    KeyDrag_RenderGlyphToSurfaceScaled(g_kdrag.growing ? g_kdrag.growFromScale : 1.0f);
    KeyDrag_UpdateLayered((int)lroundf(g_kdrag.gx), (int)lroundf(g_kdrag.gy));

    return true;
}

static LRESULT CALLBACK KeyBtnSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData)
{
    uint16_t hid = (uint16_t)dwRefData;

    // Premium UX: hand cursor over the gear marker (override indicator)
    if (msg == WM_SETCURSOR)
    {
        if (g_activeSubTab == 1 && hid != 0 && hid < 256)
        {
            if (KeySettings_GetUseUnique(hid))
            {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(hBtn, &pt);

                // Show hand only when really over the gear marker
                if (HitTestGearMarker(hBtn, pt))
                {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
        }
        // else fall through
    }

    if (msg == WM_RBUTTONUP)
    {
        if (hid != 0 && g_activeSubTab == 0)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            DisplayedKeyIconEntry picked{};
            RECT pickedRc{};
            bool pickedSpecific = HitTestDisplayedIconOnButton(hBtn, hid, pt, picked, pickedRc);

            // If key has a bound icon, play shrink animation on top of the key.
            BindAction act{};
            int padIndex = 0;
            int iconIdx = -1;
            if (pickedSpecific)
            {
                act = picked.action;
                padIndex = picked.padIndex;
                iconIdx = picked.iconIdx;
            }
            else if (FindDisplayedActionByHid(hid, act, padIndex))
            {
                iconIdx = FindIconIdxForAction(act);
            }

            if (iconIdx >= 0)
            {
                HWND hPage = GetParent(hBtn);
                KeyDel_Start(hPage, hBtn, iconIdx, padIndex);

                // Logical unbind happens immediately (visual is handled by overlay ghost)
                Bindings_ClearHidForPad(padIndex, hid);
                Profile_SaveIni(AppPaths_BindingsIni().c_str());
                InvalidateRect(hBtn, nullptr, FALSE);
            }
        }
        return 0;
    }

    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };

        // Remap tab: allow dragging bound icon as before
        if (StartKeyDragFromButton(hBtn, hid, pt))
            return 0;

        // Configuration tab (premium): if Override is enabled for this key,
        // ANY click on the key reliably triggers wow-spin.
        // This removes the "sometimes works" feeling caused by tiny gear hitbox.
        if (g_activeSubTab == 1 && hid != 0 && hid < 256)
        {
            if (KeySettings_GetUseUnique(hid))
            {
                SetSelectedHid(hid);
                KeyboardRender_OnGearClicked(hid);

                // Ensure keyboard shortcuts go to config page (Ctrl+Z/Ctrl+S etc.)
                if (g_hPageConfig) SetFocus(g_hPageConfig);

                InvalidateRect(hBtn, nullptr, FALSE);
                return 0; // swallow click (we already selected + triggered spin)
            }
        }

        return DefSubclassProc(hBtn, msg, wParam, lParam);
    }

    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

static HPEN PenDropHover()
{
    static HPEN p = CreatePen(PS_SOLID, 3, RGB(60, 200, 120));
    return p;
}

static void DrawDropHoverOutline(const DRAWITEMSTRUCT* dis)
{
    RECT rc = dis->rcItem;
    InflateRect(&rc, -2, -2);

    HDC hdc = dis->hDC;
    HGDIOBJ oldPen = SelectObject(hdc, PenDropHover());
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

static LRESULT CALLBACK PageMainProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == TabDark::MsgSelChanged())
    {
        HWND hTab = (HWND)wParam;
        if (hTab && hTab == g_hSubTab)
        {
            int idx = TabCtrl_GetCurSel(g_hSubTab);
            if (idx < 0) idx = 0;
            ShowSubPage(idx);

            if (g_activeSubTab == 2 && g_hPageTester)
                InvalidateRect(g_hPageTester, nullptr, FALSE);
        }
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

        Profile_LoadIni(AppPaths_BindingsIni().c_str());

        RebuildKeyboardButtons(hWnd);

        g_hSubTab = CreateWindowW(WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 100, 100,
            hWnd, (HMENU)9001, hInst, nullptr);

        UiTheme::ApplyToControl(g_hSubTab);
        TabDark::Apply(g_hSubTab);

        TCITEMW tie{};
        tie.mask = TCIF_TEXT;

        tie.pszText = (LPWSTR)L"Remap";
        TabCtrl_InsertItem(g_hSubTab, 0, &tie);

        tie.pszText = (LPWSTR)L"Configuration";
        TabCtrl_InsertItem(g_hSubTab, 1, &tie);

        tie.pszText = (LPWSTR)L"Gamepad Tester";
        TabCtrl_InsertItem(g_hSubTab, 2, &tie);

        tie.pszText = (LPWSTR)L"Global settings";
        TabCtrl_InsertItem(g_hSubTab, 3, &tie);

        g_hPageRemap = RemapPanel_Create(g_hSubTab, hInst, hWnd);

        static bool cfgReg = false;
        if (!cfgReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_ConfigPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubConfigPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            cfgReg = true;
        }
        g_hPageConfig = CreateWindowW(L"KeyboardSubConfigPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        static bool tstReg = false;
        if (!tstReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_TesterPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubTesterPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            tstReg = true;
        }
        g_hPageTester = CreateWindowW(L"KeyboardSubTesterPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        static bool glbReg = false;
        if (!glbReg)
        {
            WNDCLASSW wc{};
            wc.lpfnWndProc = KeyboardSubpages_GlobalSettingsPageProc;
            wc.hInstance = hInst;
            wc.lpszClassName = L"KeyboardSubGlobalSettingsPage";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassW(&wc);
            glbReg = true;
        }
        g_hPageGlobal = CreateWindowW(L"KeyboardSubGlobalSettingsPage", L"",
            WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, g_hSubTab, nullptr, hInst, nullptr);

        ResizeSubUi(hWnd);
        TabCtrl_SetCurSel(g_hSubTab, 0);
        ShowSubPage(0);

        for (uint16_t hid2 : g_hids)
            InvalidateRect(g_btnByHid[hid2], nullptr, FALSE);
        return 0;
    }

    case WM_SIZE:
        LayoutKeyboardButtons(hWnd);
        ResizeSubUi(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_NOTIFY:
        if (g_hSubTab && ((LPNMHDR)lParam)->hwndFrom == g_hSubTab && ((LPNMHDR)lParam)->code == TCN_SELCHANGE)
        {
            int idx = TabCtrl_GetCurSel(g_hSubTab);
            if (idx < 0) idx = 0;
            ShowSubPage(idx);
            if (g_activeSubTab == 2 && g_hPageTester)
                InvalidateRect(g_hPageTester, nullptr, FALSE);
            return 0;
        }
        return 0;

    case WM_TIMER:
        if (wParam == KEYDRAG_TIMER_ID)
        {
            KeyDrag_Tick();
            return 0;
        }
        if (wParam == KEYDELETE_TIMER_ID)
        {
            KeyDel_Tick();
            return 0;
        }
        if (wParam == KEYSWAP_TIMER_ID)
        {
            SwapFly_Tick();
            return 0;
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_kdrag.dragging)
        {
            uint16_t src = g_kdrag.srcHid;
            uint16_t dst = g_kdrag.hoverHid;
            int srcPadIndex = std::clamp(g_kdrag.srcPadIndex, 0, 3);
            BindAction srcAct = g_kdrag.action;

            bool copy = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            BindAction dstAct{};
            bool dstHasAction = (dst != 0) && BindingActions_TryGetByHidForPad(srcPadIndex, dst, dstAct);

            // Drop to empty:
            if (dst == 0)
            {
                if (!copy)
                {
                    // Unbind immediately
                    Bindings_ClearHidForPad(srcPadIndex, src);
                    Profile_SaveIni(AppPaths_BindingsIni().c_str());
                    InvalidateKeyByHid(src);

                    // Visual: shrink dragged ghost to zero (instead of instant hide)
                    KeyDrag_BeginShrinkOut();
                    return 0;
                }

                // copy-mode: behave as before (instant stop)
                KeyDrag_Stop();
                InvalidateKeyByHid(src);
                return 0;
            }

            // Normal drop to a key: stop drag right away.
            KeyDrag_Stop();

            if (dst == src)
            {
                InvalidateKeyByHid(src);
                return 0;
            }

            if (dst != 0)
            {
                if (!copy && dstHasAction && dstAct != srcAct)
                {
                    Bindings_ClearHidForPad(srcPadIndex, src);
                    Bindings_ClearHidForPad(srcPadIndex, dst);

                    BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                    InvalidateKeyByHid(dst);

                    if (!SwapFly_Start(hWnd, src, dst, dstAct, srcPadIndex))
                    {
                        BindingActions_ApplyForPad(srcPadIndex, dstAct, src);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                        InvalidateKeyByHid(src);
                        InvalidateKeyByHid(dst);
                    }
                    else
                    {
                        InvalidateKeyByHid(src);
                    }
                    return 0;
                }

                if (!copy && dstHasAction && dstAct == srcAct && IsButtonAction(srcAct))
                {
                    GameButton gb{};
                    if (ActionToGameButton(srcAct, gb))
                    {
                        Bindings_RemoveButtonHidForPad(srcPadIndex, gb, src);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                        InvalidateKeyByHid(src);
                        InvalidateKeyByHid(dst);
                        return 0;
                    }
                }

                if (IsButtonAction(srcAct))
                {
                    GameButton gb{};
                    if (ActionToGameButton(srcAct, gb))
                    {
                        if (!copy)
                            Bindings_RemoveButtonHidForPad(srcPadIndex, gb, src);

                        BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                        Profile_SaveIni(AppPaths_BindingsIni().c_str());
                    }
                }
                else
                {
                    BindingActions_ApplyForPad(srcPadIndex, srcAct, dst);
                    Profile_SaveIni(AppPaths_BindingsIni().c_str());
                }

                InvalidateKeyByHid(src);
                InvalidateKeyByHid(dst);
                return 0;
            }

            // unreachable because dst==0 handled above
            return 0;
        }
        return 0;

    case WM_CAPTURECHANGED:
        // IMPORTANT: do not stop if we are shrinking (we release capture intentionally)
        if (g_kdrag.dragging)
            KeyDrag_Stop();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            // cancel any drag or shrink
            KeyDrag_Stop();
            if (g_swapfly.running)
                SwapFly_Stop(true);
            if (g_kdel.running)
                KeyDel_Stop();
            return 0;
        }
        return 0;

    case WM_LBUTTONDOWN:
    {
        // Selection clear works only in Configuration tab
        if (g_activeSubTab == 1 && !g_kdrag.dragging && !g_kdrag.shrinking)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            HWND child = ChildWindowFromPointEx(hWnd, pt, CWP_SKIPINVISIBLE);
            if (!child || child == hWnd)
                SetSelectedHid(0);
        }
        return 0;
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            HWND btn = (HWND)lParam;
            if (btn && GetParent(btn) == hWnd)
            {
                // Selection works only in Configuration tab
                if (g_activeSubTab == 1)
                {
                    uint16_t hid = (uint16_t)GetWindowLongPtrW(btn, GWLP_USERDATA);
                    SetSelectedHid(hid);
                }
                return 0;
            }
        }
        return 0;

    case WM_APP_KEYBOARD_LAYOUT_CHANGED:
        // Rebuild visible keyboard immediately when layout preset changes from subpages.
        KeyDrag_Stop();
        if (g_kdel.running) KeyDel_Stop();
        if (g_swapfly.running) SwapFly_Stop(true);
        KeyboardUI_SetDragHoverHid(0);

        RebuildKeyboardButtons(hWnd);
        ResizeSubUi(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (dis && dis->CtlType == ODT_BUTTON)
        {
            if (GetParent(dis->hwndItem) == hWnd)
            {
                uint16_t hid = (uint16_t)GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA);
                uint16_t renderHid = hid;

                bool suppressDraggedMiniIcon = false;
                if (g_kdrag.dragging && hid != 0 && hid == g_kdrag.srcHid)
                {
                    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool copyMode = ctrl && IsButtonAction(g_kdrag.action);
                    suppressDraggedMiniIcon = !copyMode;
                }

                KeyboardRender_ClearSuppressedBinding();
                if (suppressDraggedMiniIcon)
                {
                    KeyboardRender_SetSuppressedBinding(
                        g_kdrag.srcHid,
                        g_kdrag.srcPadIndex,
                        g_kdrag.action);
                }

                // Selection highlight only in Configuration tab
                bool sel = (g_activeSubTab == 1 && hid != 0 && hid == g_selectedHid);
                KeyboardRender_DrawKey(dis, renderHid, sel, -1.0f);
                KeyboardRender_ClearSuppressedBinding();

                if (hid != 0 && hid == g_dragHoverHid)
                    DrawDropHoverOutline(dis);

                return TRUE;
            }
        }
        return FALSE;
    }

    case WM_DESTROY:
        KeyDrag_Stop();
        if (g_kdrag.hGhost)
        {
            DestroyWindow(g_kdrag.hGhost);
            g_kdrag.hGhost = nullptr;
        }
        KeyDrag_FreeSurface();

        if (g_kdel.running)
            KeyDel_Stop();
        if (g_kdel.hGhost)
        {
            DestroyWindow(g_kdel.hGhost);
            g_kdel.hGhost = nullptr;
        }
        KeyDel_FreeSurface();

        if (g_swapfly.running)
            SwapFly_Stop(true);

        if (g_swapfly.hGhost)
        {
            DestroyWindow(g_swapfly.hGhost);
            g_swapfly.hGhost = nullptr;
        }
        SwapFly_FreeSurface();

        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

extern "C" HWND KeyboardPageMain_CreatePage(HWND hParent, HINSTANCE hInst)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PageMainProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PageMainClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        registered = true;
    }

    return CreateWindowExW(0, L"PageMainClass", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 100, 100, hParent, nullptr, hInst, nullptr);
}
