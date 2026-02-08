// remap_panel.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Comctl32.lib")

#include "remap_panel.h"
#include "profile_ini.h"
#include "keyboard_ui.h"
#include "keyboard_ui_state.h"
#include "backend.h"
#include "settings.h"
#include "win_util.h"
#include "app_paths.h"
#include "binding_actions.h"
#include "bindings.h"
#include "ui_theme.h"
#include "remap_icons.h"

using namespace Gdiplus;

static constexpr UINT WM_APP_REMAP_APPLY_SETTINGS = WM_APP + 42;
static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;

static constexpr int ICON_GAP_X = 6;
static constexpr int ICON_GAP_Y = 6;
static constexpr int ICON_COLS = 13;

static constexpr UINT_PTR DRAG_ANIM_TIMER_ID = 9009;
static constexpr int REMAP_ID_ADD_GAMEPAD = 1901;
static constexpr int REMAP_ID_REMOVE_GAMEPAD_BASE = 3000;
static constexpr int REMAP_ICON_ID_BASE = 2100;
static constexpr int REMAP_ICON_ID_PACK_STRIDE = 128;
static constexpr int REMAP_MAX_GAMEPADS = 4;

static int Remap_GetIconStyleVariantForPack(int packIdx, int totalPacks)
{
    totalPacks = std::clamp(totalPacks, 1, REMAP_MAX_GAMEPADS);
    packIdx = std::clamp(packIdx, 0, REMAP_MAX_GAMEPADS - 1);
    if (totalPacks <= 1) return 0;
    return std::clamp(Bindings_GetPadStyleVariant(packIdx), 1, REMAP_MAX_GAMEPADS);
}

static int Remap_PickUnusedStyleVariant(int activePadCount)
{
    bool used[REMAP_MAX_GAMEPADS + 1]{};
    activePadCount = std::clamp(activePadCount, 0, REMAP_MAX_GAMEPADS);
    for (int p = 0; p < activePadCount; ++p)
    {
        int sv = std::clamp(Bindings_GetPadStyleVariant(p), 1, REMAP_MAX_GAMEPADS);
        used[sv] = true;
    }
    for (int sv = 1; sv <= REMAP_MAX_GAMEPADS; ++sv)
        if (!used[sv]) return sv;
    return REMAP_MAX_GAMEPADS;
}

static LRESULT CALLBACK IconSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData);

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static Gdiplus::Color Gp(COLORREF c, BYTE a = 255)
{
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
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

static UINT GetAnimIntervalMs()
{
    UINT ms = Settings_GetUIRefreshMs();
    return std::clamp(ms, 1u, 200u);
}

static void InvalidateHidKey(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return;
    if (g_btnByHid[hid])
        InvalidateRect(g_btnByHid[hid], nullptr, FALSE);
}

static void ClampRectToMonitorFromPoint(int& x, int& y, int w, int h)
{
    POINT pt{ x + w / 2, y + h / 2 };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi))
    {
        RECT wa = mi.rcWork;

        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;

        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }
}

// ---------------- anim helpers ----------------
static inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

static inline float EaseOutCubic(float t)
{
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static COLORREF LerpColor(COLORREF a, COLORREF b, float t)
{
    t = Clamp01(t);
    int ar = (int)GetRValue(a), ag = (int)GetGValue(a), ab = (int)GetBValue(a);
    int br = (int)GetRValue(b), bg = (int)GetGValue(b), bb = (int)GetBValue(b);
    int rr = (int)std::lround((float)ar + ((float)br - (float)ar) * t);
    int rg = (int)std::lround((float)ag + ((float)bg - (float)ag) * t);
    int rb = (int)std::lround((float)ab + ((float)bb - (float)ab) * t);
    rr = std::clamp(rr, 0, 255);
    rg = std::clamp(rg, 0, 255);
    rb = std::clamp(rb, 0, 255);
    return RGB(rr, rg, rb);
}

static COLORREF Remap_GetAccentColorForStyleVariant(int styleVariant)
{
    switch (styleVariant)
    {
    case 1: return RGB(255, 212, 92);   // yellow
    case 2: return RGB(96, 178, 255);   // sapphire
    case 3: return RGB(90, 255, 144);   // emerald
    case 4: return RGB(255, 111, 135);  // ruby
    default: return RGB(128, 136, 150); // neutral
    }
}

// ---------------- Icon glyph cache ----------------
// key = (size<<40) | (iconIdx<<8) | (styleVariant<<1) | (pressed?1:0)
struct CachedGlyph
{
    int size = 0;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
};

static std::unordered_map<uint64_t, CachedGlyph> g_iconCache;

static uint64_t MakeIconKey(int iconIdx, int size, bool pressed, int styleVariant)
{
    styleVariant = std::clamp(styleVariant, 0, 15);
    return ((uint64_t)(uint32_t)size << 40) |
        ((uint64_t)(uint32_t)iconIdx << 8) |
        ((uint64_t)(uint32_t)styleVariant << 1) |
        (pressed ? 1ULL : 0ULL);
}

static void Icon_Free(CachedGlyph& g)
{
    if (g.dc)
    {
        if (g.oldBmp) SelectObject(g.dc, g.oldBmp);
        g.oldBmp = nullptr;
        DeleteDC(g.dc);
        g.dc = nullptr;
    }
    if (g.bmp)
    {
        DeleteObject(g.bmp);
        g.bmp = nullptr;
    }
    g.bits = nullptr;
    g.size = 0;
}

static void IconCache_Clear()
{
    for (auto& kv : g_iconCache)
        Icon_Free(kv.second);
    g_iconCache.clear();
}

static CachedGlyph* Icon_GetOrCreate(int iconIdx, int size, bool pressed, float padRatio, int styleVariant)
{
    if (iconIdx < 0 || size <= 0) return nullptr;

    uint64_t key = MakeIconKey(iconIdx, size, pressed, styleVariant);
    auto it = g_iconCache.find(key);
    if (it != g_iconCache.end())
        return &it->second;

    CachedGlyph cg;
    cg.size = size;

    HDC screen = GetDC(nullptr);
    cg.dc = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    cg.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &cg.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!cg.dc || !cg.bmp || !cg.bits)
    {
        Icon_Free(cg);
        return nullptr;
    }

    cg.oldBmp = SelectObject(cg.dc, cg.bmp);

    std::memset(cg.bits, 0, (size_t)size * (size_t)size * 4);

    RECT rc{ 0,0,size,size };
    RemapIcons_DrawGlyphAA(cg.dc, rc, iconIdx, pressed, padRatio, styleVariant);

    auto [insIt, ok] = g_iconCache.emplace(key, cg);
    if (!ok)
    {
        Icon_Free(cg);
        return nullptr;
    }
    return &insIt->second;
}

static void AlphaBlitGlyph(HDC dst, int x, int y, int w, int h, CachedGlyph* g)
{
    if (!g || !g->dc) return;

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;

    AlphaBlend(dst, x, y, w, h, g->dc, 0, 0, w, h, bf);
}

// ---------------- Ghost window ----------------
enum class RemapPostAnimMode : int
{
    None = 0,
    ShrinkAway, // ghost shrinks to 0 where it is
    FlyBack,    // fly to slot, hover, reveal panel icon under, then ghost hard-hide
};

struct RemapPanelState
{
    HWND hKeyboardHost = nullptr;
    HWND txtHelp = nullptr;
    HWND btnAddGamepad = nullptr;
    int gamepadPacks = 1;
    int iconsPerPack = 0;
    int scrollY = 0;
    int scrollMax = 0;
    int contentHeight = 0;
    int wheelRemainder = 0;
    bool scrollDrag = false;
    int  scrollDragGrabOffsetY = 0;
    int  scrollDragThumbHeight = 0;
    int  scrollDragMax = 0;

    bool dragging = false;
    BindAction dragAction{};
    int dragPadIndex = 0;
    int dragIconIdx = 0;

    std::vector<HWND> keyBtns;

    uint16_t hoverHid = 0;
    RECT hoverKeyRectScreen{};

    HWND hGhost = nullptr;
    int ghostW = 0;
    int ghostH = 0;

    HDC     ghostMemDC = nullptr;
    HBITMAP ghostBmp = nullptr;
    HGDIOBJ ghostOldBmp = nullptr;
    void* ghostBits = nullptr;

    float gx = 0.0f, gy = 0.0f; // ghost top-left (screen)
    float tx = 0.0f, ty = 0.0f; // ghost target top-left (screen)
    DWORD lastTick = 0;

    UINT animIntervalMs = 0;
    std::vector<HWND> iconBtns;
    std::vector<HWND> packLabels;
    std::vector<HWND> packRemoveBtns;

    int ghostRenderedIconIdx = -1;
    int ghostRenderedSize = 0;
    int ghostRenderedStyleVariant = -1;

    // source icon detach behavior
    HWND  dragSrcIconBtn = nullptr;
    POINT dragSrcCenterScreen{};
    float srcIconScale = 1.0f;
    float srcIconScaleTarget = 1.0f;

    // post animations
    RemapPostAnimMode postMode = RemapPostAnimMode::None;

    // FlyBack phases:
    // 0 = fly, 1 = hover, 2 = reveal-beat then ghost hide
    int   postPhase = 0;
    DWORD postPhaseStartTick = 0;
    DWORD postPhaseDurationMs = 0;

    // ShrinkAway
    DWORD shrinkStartMs = 0;
    DWORD shrinkDurMs = 0;

    // Fly targets
    float postX0 = 0.0f, postY0 = 0.0f;
    float postX1 = 0.0f, postY1 = 0.0f;
};

static COLORREF Remap_GetPackTileColor(RemapPanelState* st, int packIdx, bool stronger);

static HBRUSH g_packHeaderBrushes[REMAP_MAX_GAMEPADS]{};
static COLORREF g_packHeaderBrushColors[REMAP_MAX_GAMEPADS]{};

static HBRUSH Remap_GetPackHeaderBrush(RemapPanelState* st, int packIdx)
{
    packIdx = std::clamp(packIdx, 0, REMAP_MAX_GAMEPADS - 1);
    COLORREF want = Remap_GetPackTileColor(st, packIdx, false);

    if (g_packHeaderBrushes[packIdx] && g_packHeaderBrushColors[packIdx] == want)
        return g_packHeaderBrushes[packIdx];

    if (g_packHeaderBrushes[packIdx])
    {
        DeleteObject(g_packHeaderBrushes[packIdx]);
        g_packHeaderBrushes[packIdx] = nullptr;
    }

    g_packHeaderBrushes[packIdx] = CreateSolidBrush(want);
    g_packHeaderBrushColors[packIdx] = want;
    return g_packHeaderBrushes[packIdx] ? g_packHeaderBrushes[packIdx] : (HBRUSH)UiTheme::Brush_PanelBg();
}

static void Remap_FreePackHeaderBrushes()
{
    for (int i = 0; i < REMAP_MAX_GAMEPADS; ++i)
    {
        if (g_packHeaderBrushes[i])
        {
            DeleteObject(g_packHeaderBrushes[i]);
            g_packHeaderBrushes[i] = nullptr;
        }
        g_packHeaderBrushColors[i] = RGB(0, 0, 0);
    }
}

static void Ghost_FreeSurface(RemapPanelState* st)
{
    if (!st) return;

    if (st->ghostMemDC)
    {
        if (st->ghostOldBmp) SelectObject(st->ghostMemDC, st->ghostOldBmp);
        st->ghostOldBmp = nullptr;
        DeleteDC(st->ghostMemDC);
        st->ghostMemDC = nullptr;
    }

    if (st->ghostBmp)
    {
        DeleteObject(st->ghostBmp);
        st->ghostBmp = nullptr;
    }

    st->ghostBits = nullptr;
    st->ghostRenderedIconIdx = -1;
    st->ghostRenderedSize = 0;
    st->ghostRenderedStyleVariant = -1;
}

static bool Ghost_EnsureSurface(RemapPanelState* st)
{
    if (!st || !st->hGhost) return false;
    if (st->ghostW <= 0 || st->ghostH <= 0) return false;

    Ghost_FreeSurface(st);

    HDC screen = GetDC(nullptr);
    st->ghostMemDC = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = st->ghostW;
    bi.bmiHeader.biHeight = -st->ghostH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    st->ghostBmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &st->ghostBits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!st->ghostMemDC || !st->ghostBmp || !st->ghostBits)
    {
        Ghost_FreeSurface(st);
        return false;
    }

    st->ghostOldBmp = SelectObject(st->ghostMemDC, st->ghostBmp);
    return true;
}

static void Ghost_RenderFullPressedCachedIfNeeded(RemapPanelState* st)
{
    if (!st || !st->ghostMemDC || !st->ghostBits) return;

    int sz = st->ghostW;
    if (sz <= 0 || st->ghostH != sz) return;

    int styleVariant = Remap_GetIconStyleVariantForPack(st->dragPadIndex, st->gamepadPacks);

    if (st->ghostRenderedIconIdx == st->dragIconIdx &&
        st->ghostRenderedSize == sz &&
        st->ghostRenderedStyleVariant == styleVariant)
        return;

    st->ghostRenderedIconIdx = st->dragIconIdx;
    st->ghostRenderedSize = sz;
    st->ghostRenderedStyleVariant = styleVariant;

    std::memset(st->ghostBits, 0, (size_t)sz * (size_t)sz * 4);

    CachedGlyph* cg = Icon_GetOrCreate(st->dragIconIdx, sz, true, 0.135f, styleVariant);
    if (cg && cg->dc)
        BitBlt(st->ghostMemDC, 0, 0, sz, sz, cg->dc, 0, 0, SRCCOPY);
    else
    {
        RECT rc{ 0,0,sz,sz };
        RemapIcons_DrawGlyphAA(st->ghostMemDC, rc, st->dragIconIdx, true, 0.135f, styleVariant);
    }
}

static void Ghost_RenderScaledPressed(RemapPanelState* st, float scale01)
{
    if (!st || !st->ghostMemDC || !st->ghostBits) return;

    int w = st->ghostW;
    int h = st->ghostH;
    if (w <= 0 || h <= 0) return;

    scale01 = Clamp01(scale01);

    std::memset(st->ghostBits, 0, (size_t)w * (size_t)h * 4);

    int base = std::min(w, h);
    int s = (int)std::lround((float)base * scale01);

    // no 1px artifacts
    if (s <= 1)
    {
        st->ghostRenderedIconIdx = -1;
        st->ghostRenderedSize = 0;
        st->ghostRenderedStyleVariant = -1;
        return;
    }

    s = std::clamp(s, 2, base);

    int x = (w - s) / 2;
    int y = (h - s) / 2;

    int styleVariant = Remap_GetIconStyleVariantForPack(st->dragPadIndex, st->gamepadPacks);
    RECT rc{ x, y, x + s, y + s };
    RemapIcons_DrawGlyphAA(st->ghostMemDC, rc, st->dragIconIdx, true, 0.135f, styleVariant);

    st->ghostRenderedIconIdx = -1;
    st->ghostRenderedSize = 0;
    st->ghostRenderedStyleVariant = -1;
}

static void Ghost_UpdateLayered(RemapPanelState* st, int x, int y)
{
    if (!st || !st->hGhost || !st->ghostMemDC) return;

    BYTE alpha = 255;

    HDC screen = GetDC(nullptr);
    POINT ptPos{ x, y };
    SIZE  sz{ st->ghostW, st->ghostH };
    POINT ptSrc{ 0, 0 };

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(st->hGhost, screen, &ptPos, &sz, st->ghostMemDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);

    ShowWindow(st->hGhost, SW_SHOWNOACTIVATE);
}

static LRESULT CALLBACK GhostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void Ghost_EnsureCreated(RemapPanelState* st, HINSTANCE hInst, HWND hOwner)
{
    if (!st || st->hGhost) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = GhostWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"RemapGhostWindow";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        reg = true;
    }

    st->hGhost = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"RemapGhostWindow",
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        hOwner, nullptr, hInst, nullptr);

    if (st->hGhost)
        ShowWindow(st->hGhost, SW_HIDE);
}

static bool Ghost_EnsureSurfaceAndResetCache(RemapPanelState* st)
{
    if (!st) return false;
    if (!st->hGhost) return false;

    if (!Ghost_EnsureSurface(st)) return false;

    st->ghostRenderedIconIdx = -1;
    st->ghostRenderedSize = 0;
    st->ghostRenderedStyleVariant = -1;
    return true;
}

static void Ghost_ShowFullAt(RemapPanelState* st, float x, float y)
{
    if (!st || !st->hGhost) return;

    st->gx = x;
    st->gy = y;

    int xi = (int)lroundf(x);
    int yi = (int)lroundf(y);

    ClampRectToMonitorFromPoint(xi, yi, st->ghostW, st->ghostH);

    if (!st->ghostMemDC)
    {
        if (!Ghost_EnsureSurfaceAndResetCache(st)) return;
    }

    Ghost_RenderFullPressedCachedIfNeeded(st);
    Ghost_UpdateLayered(st, xi, yi);
}

static void Ghost_ShowScaledAt(RemapPanelState* st, float x, float y, float scale01)
{
    if (!st || !st->hGhost) return;

    st->gx = x;
    st->gy = y;

    int xi = (int)lroundf(x);
    int yi = (int)lroundf(y);

    ClampRectToMonitorFromPoint(xi, yi, st->ghostW, st->ghostH);

    if (!st->ghostMemDC)
    {
        if (!Ghost_EnsureSurfaceAndResetCache(st)) return;
    }

    Ghost_RenderScaledPressed(st, scale01);
    Ghost_UpdateLayered(st, xi, yi);
}

static void Ghost_Hide(RemapPanelState* st)
{
    if (!st || !st->hGhost) return;
    ShowWindow(st->hGhost, SW_HIDE);
}

// ---------------- Detach thresholds ----------------
static void GetDetachThresholds(HWND hPanel, RemapPanelState* st, int& outShowPx, int& outHidePx)
{
    static constexpr float kDetachDistanceMul = 1.5f;

    int oldShowBase = std::max(S(hPanel, 28), st ? (st->ghostW / 2) : S(hPanel, 20));
    int oldHideBase = std::max(S(hPanel, 24), st ? (st->ghostW / 2) : S(hPanel, 16));

    int oldShowPx = (int)std::lround((float)oldShowBase * kDetachDistanceMul);
    int oldHidePx = (int)std::lround((float)oldHideBase * kDetachDistanceMul);

    int geomShowPx = 0;
    int geomHidePx = 0;

    if (st && st->dragSrcIconBtn)
    {
        RECT rc{};
        GetWindowRect(st->dragSrcIconBtn, &rc);
        int bw = (rc.right - rc.left);
        int bh = (rc.bottom - rc.top);

        int srcHalf = std::max(bw, bh) / 2;
        int ghostHalf = std::max(1, st->ghostW / 2);

        int overlap = srcHalf + ghostHalf;

        int padHide = S(hPanel, 10);
        int padShow = S(hPanel, 20);

        geomHidePx = overlap + padHide;
        geomShowPx = overlap + padShow;

        geomHidePx = (int)std::lround((float)geomHidePx * kDetachDistanceMul);
        geomShowPx = (int)std::lround((float)geomShowPx * kDetachDistanceMul);
    }

    int showPx = std::max(oldShowPx, geomShowPx);
    int hidePx = std::max(oldHidePx, geomHidePx);

    if (showPx < 1) showPx = 1;
    if (hidePx < 1) hidePx = 1;

    int minGap = S(hPanel, 6);
    if (minGap < 2) minGap = 2;

    if (showPx < hidePx + minGap)
        showPx = hidePx + minGap;

    outShowPx = showPx;
    outHidePx = hidePx;
}

// ---------------- Post animation constants ----------------
static constexpr DWORD FLY_FLY_MS = 190;
static constexpr DWORD FLY_HOVER_MS = 85;
static constexpr DWORD FLY_REVEAL_BEAT_MS = 18;

static void StopAllPanelAnim_Immediate(HWND hPanel, RemapPanelState* st)
{
    if (!st) return;

    st->dragging = false;
    st->hoverHid = 0;
    st->hoverKeyRectScreen = RECT{};
    st->keyBtns.clear();

    if (GetCapture() == hPanel)
        ReleaseCapture();

    KeyboardUI_SetDragHoverHid(0);

    st->postMode = RemapPostAnimMode::None;
    st->postPhase = 0;
    st->postPhaseStartTick = 0;
    st->postPhaseDurationMs = 0;

    st->shrinkStartMs = 0;
    st->shrinkDurMs = 0;

    st->srcIconScale = 1.0f;
    st->srcIconScaleTarget = 1.0f;

    HWND src = st->dragSrcIconBtn;
    st->dragSrcIconBtn = nullptr;
    st->dragSrcCenterScreen = POINT{};

    if (src) InvalidateRect(src, nullptr, FALSE);

    KillTimer(hPanel, DRAG_ANIM_TIMER_ID);
    st->animIntervalMs = 0;

    Ghost_Hide(st);

    if (st->hKeyboardHost)
        InvalidateRect(st->hKeyboardHost, nullptr, FALSE);
}

static void PostAnim_StartShrinkAway(HWND hPanel, RemapPanelState* st)
{
    if (!st) return;

    st->postMode = RemapPostAnimMode::ShrinkAway;
    st->postPhase = 0;

    st->shrinkStartMs = GetTickCount();
    st->shrinkDurMs = 140;

    // Ensure source icon is visible in this mode
    if (st->dragSrcIconBtn)
    {
        st->srcIconScale = 1.0f;
        st->srcIconScaleTarget = 1.0f;
        InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);
    }

    UINT wantMs = GetAnimIntervalMs();
    st->animIntervalMs = wantMs;
    SetTimer(hPanel, DRAG_ANIM_TIMER_ID, st->animIntervalMs, nullptr);
}

static void PostAnim_StartFlyBack(HWND hPanel, RemapPanelState* st)
{
    if (!st) return;

    if (!st->dragSrcIconBtn)
    {
        PostAnim_StartShrinkAway(hPanel, st);
        return;
    }

    RECT rcSrc{};
    GetWindowRect(st->dragSrcIconBtn, &rcSrc);

    int cx = (rcSrc.left + rcSrc.right) / 2;
    int cy = (rcSrc.top + rcSrc.bottom) / 2;

    st->postMode = RemapPostAnimMode::FlyBack;
    st->postPhase = 0;
    st->postPhaseStartTick = GetTickCount();
    st->postPhaseDurationMs = FLY_FLY_MS;

    st->postX0 = st->gx;
    st->postY0 = st->gy;
    st->postX1 = (float)(cx - st->ghostW / 2);
    st->postY1 = (float)(cy - st->ghostH / 2);

    // Hide panel icon until REVEAL moment (under ghost)
    st->srcIconScale = 0.0f;
    st->srcIconScaleTarget = 0.0f;
    InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);

    UINT wantMs = GetAnimIntervalMs();
    st->animIntervalMs = wantMs;
    SetTimer(hPanel, DRAG_ANIM_TIMER_ID, st->animIntervalMs, nullptr);
}

static bool PostAnim_Tick(HWND hPanel, RemapPanelState* st)
{
    if (!st) return true;
    if (st->postMode == RemapPostAnimMode::None) return true;

    DWORD now = GetTickCount();

    if (st->postMode == RemapPostAnimMode::ShrinkAway)
    {
        DWORD dt = now - st->shrinkStartMs;
        DWORD dur = (st->shrinkDurMs > 0) ? st->shrinkDurMs : 1;

        float t = Clamp01((float)dt / (float)dur);
        float e = EaseOutCubic(t);

        float scale = 1.0f - e;
        if (t >= 1.0f - 1e-4f) scale = 0.0f;

        Ghost_ShowScaledAt(st, st->gx, st->gy, scale);

        if (t >= 1.0f - 1e-4f)
            return true;

        return false;
    }

    if (st->postMode == RemapPostAnimMode::FlyBack)
    {
        // Phase 0: fly (ghost full, panel hidden)
        if (st->postPhase == 0)
        {
            DWORD dt = now - st->postPhaseStartTick;
            DWORD dur = (st->postPhaseDurationMs > 0) ? st->postPhaseDurationMs : 1;

            float t = Clamp01((float)dt / (float)dur);
            float e = EaseOutCubic(t);

            float x = Lerp(st->postX0, st->postX1, e);
            float y = Lerp(st->postY0, st->postY1, e);

            Ghost_ShowFullAt(st, x, y);

            if (t >= 1.0f - 1e-4f)
            {
                Ghost_ShowFullAt(st, st->postX1, st->postY1);

                st->postPhase = 1;
                st->postPhaseStartTick = now;
                st->postPhaseDurationMs = FLY_HOVER_MS;
            }

            return false;
        }

        // Phase 1: hover (panel still hidden)
        if (st->postPhase == 1)
        {
            Ghost_ShowFullAt(st, st->postX1, st->postY1);

            DWORD dt = now - st->postPhaseStartTick;
            if (dt >= st->postPhaseDurationMs)
            {
                // REVEAL under ghost
                if (st->dragSrcIconBtn)
                {
                    st->srcIconScale = 1.0f;
                    st->srcIconScaleTarget = 1.0f;
                    InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);

                    // best-effort immediate paint for the under-ghost reveal
                    UpdateWindow(st->dragSrcIconBtn);
                }

                st->postPhase = 2;
                st->postPhaseStartTick = now;
                st->postPhaseDurationMs = FLY_REVEAL_BEAT_MS;
            }

            return false;
        }

        // Phase 2: hold a beat, then hide ghost sharply
        if (st->postPhase == 2)
        {
            Ghost_ShowFullAt(st, st->postX1, st->postY1);

            DWORD dt = now - st->postPhaseStartTick;
            if (dt >= st->postPhaseDurationMs)
            {
                Ghost_Hide(st);
                return true;
            }

            return false;
        }

        return true;
    }

    (void)hPanel;
    return true;
}

static void PostAnim_Finish(HWND hPanel, RemapPanelState* st)
{
    if (!st) return;

    Ghost_Hide(st);

    st->postMode = RemapPostAnimMode::None;
    st->postPhase = 0;
    st->postPhaseStartTick = 0;
    st->postPhaseDurationMs = 0;

    st->shrinkStartMs = 0;
    st->shrinkDurMs = 0;

    if (st->dragSrcIconBtn)
        InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);

    st->srcIconScale = 1.0f;
    st->srcIconScaleTarget = 1.0f;
    st->dragSrcIconBtn = nullptr;
    st->dragSrcCenterScreen = POINT{};

    if (!st->dragging)
    {
        KillTimer(hPanel, DRAG_ANIM_TIMER_ID);
        st->animIntervalMs = 0;
    }
}

// ---------------- Icon buttons (owner-draw) ----------------
static bool BeginBuffered(HDC outDC, int w, int h, HDC& memDC, HBITMAP& bmp, HGDIOBJ& oldBmp)
{
    memDC = CreateCompatibleDC(outDC);
    if (!memDC) return false;

    bmp = CreateCompatibleBitmap(outDC, w, h);
    if (!bmp) { DeleteDC(memDC); memDC = nullptr; return false; }

    oldBmp = SelectObject(memDC, bmp);
    return true;
}

static void EndBuffered(HDC outDC, int dstX, int dstY, int w, int h, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    BitBlt(outDC, dstX, dstY, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

static void DrawIconButton(const DRAWITEMSTRUCT* dis, int iconIdx, RemapPanelState* st)
{
    HDC out = dis->hDC;
    RECT rc = dis->rcItem;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    HDC mem = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    if (!BeginBuffered(out, w, h, mem, bmp, oldBmp))
    {
        // Fallback for transient GDI allocation failures during heavy scrolling:
        // render directly so we never show white empty tiles.
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool srcDragging = (st && st->dragSrcIconBtn && dis->hwndItem == st->dragSrcIconBtn &&
            (st->dragging || st->postMode == RemapPostAnimMode::FlyBack));

        COLORREF tileBg = UiTheme::Color_PanelBg();
        if (st && dis->CtlID >= (UINT)REMAP_ICON_ID_BASE)
        {
            int packIdx = ((int)dis->CtlID - REMAP_ICON_ID_BASE) / REMAP_ICON_ID_PACK_STRIDE;
            bool stronger = (pressed && !srcDragging);
            tileBg = Remap_GetPackTileColor(st, packIdx, stronger);
        }
        HBRUSH bTile = CreateSolidBrush(tileBg);
        FillRect(out, &rc, bTile);
        DeleteObject(bTile);

        float scale = 1.0f;
        if (st && st->dragSrcIconBtn && dis->hwndItem == st->dragSrcIconBtn &&
            (st->dragging || st->postMode == RemapPostAnimMode::FlyBack))
        {
            scale = std::clamp(st->srcIconScale, 0.0f, 1.0f);
        }

        int size = std::min(w, h);
        int styleVariant = 0;
        if (dis->CtlID >= (UINT)REMAP_ICON_ID_BASE)
        {
            int packIdx = ((int)dis->CtlID - REMAP_ICON_ID_BASE) / REMAP_ICON_ID_PACK_STRIDE;
            int totalPacks = st ? st->gamepadPacks : 1;
            styleVariant = Remap_GetIconStyleVariantForPack(packIdx, totalPacks);
        }

        int dstSize = (int)std::lround((float)size * std::clamp(scale, 0.0f, 1.0f));
        if (dstSize <= 1) return;
        dstSize = std::clamp(dstSize, 2, size);

        int x = rc.left + (w - dstSize) / 2;
        int y = rc.top + (h - dstSize) / 2;

        CachedGlyph* cg = Icon_GetOrCreate(iconIdx, size, pressed, 0.135f, styleVariant);
        if (cg && cg->dc)
        {
            BLENDFUNCTION bf{};
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.SourceConstantAlpha = 255;
            bf.AlphaFormat = AC_SRC_ALPHA;
            AlphaBlend(out, x, y, dstSize, dstSize, cg->dc, 0, 0, size, size, bf);
        }
        else
        {
            RECT rcIcon{ x, y, x + dstSize, y + dstSize };
            RemapIcons_DrawGlyphAA(out, rcIcon, iconIdx, pressed, 0.135f, styleVariant);
        }
        return;
    }

    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool srcDragging = (st && st->dragSrcIconBtn && dis->hwndItem == st->dragSrcIconBtn &&
        (st->dragging || st->postMode == RemapPostAnimMode::FlyBack));

    RECT local{ 0,0,w,h };
    COLORREF tileBg = UiTheme::Color_PanelBg();
    if (st && dis->CtlID >= (UINT)REMAP_ICON_ID_BASE)
    {
        int packIdx = ((int)dis->CtlID - REMAP_ICON_ID_BASE) / REMAP_ICON_ID_PACK_STRIDE;
        bool stronger = (pressed && !srcDragging);
        tileBg = Remap_GetPackTileColor(st, packIdx, stronger);
    }
    HBRUSH bTile = CreateSolidBrush(tileBg);
    FillRect(mem, &local, bTile);
    DeleteObject(bTile);

    // scale logic for the source button only
    float scale = 1.0f;
    if (st && st->dragSrcIconBtn && dis->hwndItem == st->dragSrcIconBtn)
    {
        // During dragging: animated detach scale.
        // During FlyBack: scale is controlled by reveal/hide (0 while hidden, 1 on reveal).
        if (st->dragging || st->postMode == RemapPostAnimMode::FlyBack)
            scale = std::clamp(st->srcIconScale, 0.0f, 1.0f);
        else
            scale = 1.0f;
    }

    int size = std::min(w, h);
    int styleVariant = 0;
    if (dis->CtlID >= (UINT)REMAP_ICON_ID_BASE)
    {
        int packIdx = ((int)dis->CtlID - REMAP_ICON_ID_BASE) / REMAP_ICON_ID_PACK_STRIDE;
        int totalPacks = st ? st->gamepadPacks : 1;
        styleVariant = Remap_GetIconStyleVariantForPack(packIdx, totalPacks);
    }

    // scale in [0..1]
    scale = std::clamp(scale, 0.0f, 1.0f);

    // IMPORTANT: keep center fixed. If size becomes too small => draw nothing.
    int dstSize = (int)std::lround((float)size * scale);
    if (dstSize <= 1)
    {
        // nothing: we already filled background above
        EndBuffered(out, rc.left, rc.top, w, h, mem, bmp, oldBmp);
        return;
    }

    // clamp (avoid artifacts, but keep center)
    dstSize = std::clamp(dstSize, 2, size);

    // center in the button tile
    int x = (w - dstSize) / 2;
    int y = (h - dstSize) / 2;

    // draw scaled glyph centered
    CachedGlyph* cg = Icon_GetOrCreate(iconIdx, size, pressed, 0.135f, styleVariant);
    if (cg && cg->dc)
    {
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;

        // NOTE: src is full size (size x size), dst is (dstSize x dstSize) at centered x/y
        AlphaBlend(mem, x, y, dstSize, dstSize, cg->dc, 0, 0, size, size, bf);
    }
    else
    {
        RECT rcIcon{ x, y, x + dstSize, y + dstSize };
        RemapIcons_DrawGlyphAA(mem, rcIcon, iconIdx, pressed, 0.135f, styleVariant);
    }

    EndBuffered(out, rc.left, rc.top, w, h, mem, bmp, oldBmp);
}

// ---------------- Key cache / hit tests ----------------
static void BuildKeyCache(RemapPanelState* st)
{
    st->keyBtns.clear();
    if (!st->hKeyboardHost) return;

    for (HWND c = GetWindow(st->hKeyboardHost, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
    {
        if (!IsWindowVisible(c)) continue;
        uint16_t hid = (uint16_t)GetWindowLongPtrW(c, GWLP_USERDATA);
        if (hid != 0 && KeyboardUI_HasHid(hid))
            st->keyBtns.push_back(c);
    }
}

static int DistSqPointToRect(POINT p, const RECT& r)
{
    int dx = 0;
    if (p.x < r.left) dx = r.left - p.x;
    else if (p.x > r.right) dx = p.x - r.right;

    int dy = 0;
    if (p.y < r.top) dy = r.top - p.y;
    else if (p.y > r.bottom) dy = p.y - r.bottom;

    return dx * dx + dy * dy;
}

static bool FindNearestKey(RemapPanelState* st, POINT ptScreen, int thresholdPx, uint16_t& outHid, RECT& outRc)
{
    outHid = 0;
    outRc = RECT{};

    if (!st || !st->hKeyboardHost) return false;
    if (st->keyBtns.empty()) BuildKeyCache(st);

    int best = INT_MAX;
    HWND bestWnd = nullptr;
    RECT bestRc{};

    for (HWND w : st->keyBtns)
    {
        RECT rc{};
        GetWindowRect(w, &rc);
        int d2 = DistSqPointToRect(ptScreen, rc);
        if (d2 < best)
        {
            best = d2;
            bestWnd = w;
            bestRc = rc;
        }
    }

    const int thr2 = thresholdPx * thresholdPx;
    if (!bestWnd || best > thr2) return false;

    outHid = (uint16_t)GetWindowLongPtrW(bestWnd, GWLP_USERDATA);
    outRc = bestRc;
    return (outHid != 0);
}

static bool TryGetKeyUnderCursor(RemapPanelState* st, POINT ptScreen, uint16_t& outHid, RECT& outRc)
{
    outHid = 0;
    outRc = RECT{};

    if (!st || !st->hKeyboardHost) return false;

    HWND w = WindowFromPoint(ptScreen);
    if (!w) return false;

    for (HWND cur = w; cur; cur = GetParent(cur))
    {
        if (GetParent(cur) == st->hKeyboardHost)
        {
            uint16_t hid = (uint16_t)GetWindowLongPtrW(cur, GWLP_USERDATA);
            if (hid != 0 && KeyboardUI_HasHid(hid))
            {
                GetWindowRect(cur, &outRc);
                outHid = hid;
                return true;
            }
            return false;
        }
        if (cur == st->hKeyboardHost) break;
    }
    return false;
}

// ---------------- Layout / sizing ----------------
static void Remap_RequestSettingsSave(HWND hWnd)
{
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root)
        PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void Remap_UpdateAddGamepadButtonText(RemapPanelState* st)
{
    if (!st || !st->btnAddGamepad) return;

    wchar_t text[64]{};
    int packs = std::clamp(st->gamepadPacks, 1, REMAP_MAX_GAMEPADS);
    if (packs < REMAP_MAX_GAMEPADS)
        swprintf_s(text, L"Add Gamepad (%d/%d)", packs, REMAP_MAX_GAMEPADS);
    else
        swprintf_s(text, L"Max Gamepads (%d/%d)", packs, REMAP_MAX_GAMEPADS);

    bool canAdd = (packs < REMAP_MAX_GAMEPADS);
    SetWindowTextW(st->btnAddGamepad, text);
    EnableWindow(st->btnAddGamepad, canAdd);
}

static void DrawAddGamepadButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    HBRUSH bg = CreateSolidBrush(UiTheme::Color_PanelBg());
    FillRect(dis->hDC, &rc, bg);
    DeleteObject(bg);

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pushed = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF fill = pushed ? RGB(44, 44, 48) : RGB(30, 30, 33);
    COLORREF border = disabled ? RGB(70, 70, 76) : UiTheme::Color_Border();
    COLORREF text = disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text();

    HBRUSH bFill = CreateSolidBrush(fill);
    HPEN pBorder = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pBorder);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, bFill);

    Rectangle(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);

    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(bFill);
    DeleteObject(pBorder);

    wchar_t txt[96]{};
    GetWindowTextW(dis->hwndItem, txt, (int)(sizeof(txt) / sizeof(txt[0])));

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);

    RECT tr = rc;
    DrawTextW(dis->hDC, txt, -1, &tr, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
}

static void DrawDisableGamepadIconButton(const DRAWITEMSTRUCT* dis, RemapPanelState* st, int packIdx)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    COLORREF bgC = st ? Remap_GetPackTileColor(st, packIdx, false) : UiTheme::Color_PanelBg();
    HBRUSH bg = CreateSolidBrush(bgC);
    FillRect(dis->hDC, &rc, bg);
    DeleteObject(bg);

    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool pushed = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF glyph = disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text();
    if (pushed && !disabled)
        glyph = LerpColor(glyph, UiTheme::Color_Accent(), 0.45f);

    Graphics g(dis->hDC);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const float cx = (float)(rc.left + rc.right) * 0.5f;
    const float cy = (float)(rc.top + rc.bottom) * 0.5f;
    const float radius = std::max(4.0f, std::min((float)w, (float)h) * 0.38f);

    Pen penPower(Gp(glyph, 248), std::max(2.0f, (float)S(dis->hwndItem, 2)));
    penPower.SetStartCap(LineCapRound);
    penPower.SetEndCap(LineCapRound);

    // Power icon: open ring + top vertical stem.
    // Rotate the open ring by -90 deg (CCW) so the gap is at the top.
    g.DrawArc(&penPower, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, -48.0f, 276.0f);
    g.DrawLine(&penPower, cx, cy - radius * 1.03f, cx, cy - radius * 0.18f);
}

static void Remap_CreateIconPackButtons(HWND hWnd, RemapPanelState* st, HINSTANCE hInst, HFONT hFont, int packIdx)
{
    if (!st || packIdx < 0 || packIdx >= REMAP_MAX_GAMEPADS) return;

    int iconsPerPack = std::max(0, RemapIcons_Count());
    if (iconsPerPack <= 0) return;

    if (st->iconsPerPack <= 0)
        st->iconsPerPack = iconsPerPack;

    int startX = S(hWnd, 12);
    int startY = S(hWnd, 104);
    int packGapY = S(hWnd, 16);
    int headerH = S(hWnd, 22);
    int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
    int btnH = btnW;
    int gapX = S(hWnd, ICON_GAP_X);
    int gapY = S(hWnd, ICON_GAP_Y);
    int rowsPerPack = 2;
    int packStrideY = headerH + rowsPerPack * btnH + (rowsPerPack - 1) * gapY + packGapY;

    HWND lbl = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        startX, startY + packIdx * packStrideY, S(hWnd, 220), headerH,
        hWnd, nullptr, hInst, nullptr);
    SendMessageW(lbl, WM_SETFONT, (WPARAM)hFont, TRUE);

    wchar_t packName[64]{};
    swprintf_s(packName, L"Gamepad %d", packIdx + 1);
    SetWindowTextW(lbl, packName);
    st->packLabels.push_back(lbl);

    HWND removeBtn = nullptr;
    if (packIdx > 0)
    {
        int removeX = startX + S(hWnd, 98);
        removeBtn = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            removeX, startY + packIdx * packStrideY, S(hWnd, 22), S(hWnd, 22),
            hWnd, (HMENU)(INT_PTR)(REMAP_ID_REMOVE_GAMEPAD_BASE + packIdx), hInst, nullptr);
        SendMessageW(removeBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    st->packRemoveBtns.push_back(removeBtn);

    for (int i = 0; i < iconsPerPack; ++i)
    {
        int row = i / ICON_COLS;
        int col = i % ICON_COLS;

        const RemapIconDef& idef = RemapIcons_Get(i);
        int ctrlId = REMAP_ICON_ID_BASE + packIdx * REMAP_ICON_ID_PACK_STRIDE + i;

        HWND b = CreateWindowW(L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            startX + col * (btnW + gapX),
            startY + packIdx * packStrideY + headerH + row * (btnH + gapY),
            btnW, btnH,
            hWnd, (HMENU)(INT_PTR)ctrlId, hInst, nullptr);

        SendMessageW(b, WM_SETFONT, (WPARAM)hFont, TRUE);
        SetWindowLongPtrW(b, GWLP_USERDATA, (LONG_PTR)i);
        SetWindowSubclass(b, IconSubclassProc, 1, (DWORD_PTR)idef.action);
        st->iconBtns.push_back(b);
    }
}

static void Remap_DestroyPackControls(RemapPanelState* st)
{
    if (!st) return;

    for (HWND h : st->iconBtns)
        if (h && IsWindow(h)) DestroyWindow(h);
    st->iconBtns.clear();

    for (HWND h : st->packLabels)
        if (h && IsWindow(h)) DestroyWindow(h);
    st->packLabels.clear();

    for (HWND h : st->packRemoveBtns)
        if (h && IsWindow(h)) DestroyWindow(h);
    st->packRemoveBtns.clear();
}

static void Remap_RebuildGamepadPacks(HWND hWnd, RemapPanelState* st, HINSTANCE hInst, HFONT hFont, int newCount)
{
    if (!st) return;
    newCount = std::clamp(newCount, 1, REMAP_MAX_GAMEPADS);

    Remap_DestroyPackControls(st);
    st->iconsPerPack = RemapIcons_Count();
    st->gamepadPacks = newCount;

    for (int p = 0; p < st->gamepadPacks; ++p)
        Remap_CreateIconPackButtons(hWnd, st, hInst, hFont, p);
}

static void ApplyRemapSizing(HWND hWnd, RemapPanelState* st);

static void Remap_RebuildGamepadPacksBatched(HWND hWnd, RemapPanelState* st, HINSTANCE hInst, HFONT hFont, int newCount)
{
    if (!st) return;

    HWND hKeyboardHost = st->hKeyboardHost;

    SendMessageW(hWnd, WM_SETREDRAW, FALSE, 0);
    if (hKeyboardHost && IsWindow(hKeyboardHost))
        SendMessageW(hKeyboardHost, WM_SETREDRAW, FALSE, 0);

    Remap_RebuildGamepadPacks(hWnd, st, hInst, hFont, newCount);
    ApplyRemapSizing(hWnd, st);

    SendMessageW(hWnd, WM_SETREDRAW, TRUE, 0);
    if (hKeyboardHost && IsWindow(hKeyboardHost))
        SendMessageW(hKeyboardHost, WM_SETREDRAW, TRUE, 0);

    RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (hKeyboardHost && IsWindow(hKeyboardHost))
        RedrawWindow(hKeyboardHost, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

static int Remap_ScrollbarWidthPx(HWND hWnd) { return S(hWnd, 12); }
static int Remap_ScrollbarMarginPx(HWND hWnd) { return S(hWnd, 8); }

static int Remap_GetViewportHeight(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    return std::max(0, (int)(rc.bottom - rc.top));
}

static int Remap_RecalcContentHeight(HWND hWnd, RemapPanelState* st)
{
    if (!st) return 0;
    int clientH = Remap_GetViewportHeight(hWnd);

    const int startY = S(hWnd, 104);
    const int packGapY = S(hWnd, 16);
    const int headerH = S(hWnd, 22);
    const int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
    const int btnH = btnW;
    const int gapY = S(hWnd, ICON_GAP_Y);
    const int rowsPerPack = 2;
    const int packs = std::max(1, st->gamepadPacks);

    const int iconsHeight = headerH + rowsPerPack * btnH + (rowsPerPack - 1) * gapY;
    const int packStrideY = iconsHeight + packGapY;
    const int contentBottom = startY + (packs - 1) * packStrideY + iconsHeight + S(hWnd, 16);

    st->contentHeight = std::max(contentBottom, clientH);
    return st->contentHeight;
}

static int Remap_GetMaxScroll(HWND hWnd, RemapPanelState* st)
{
    if (!st) return 0;
    int viewH = Remap_GetViewportHeight(hWnd);
    int contentH = Remap_RecalcContentHeight(hWnd, st);
    return std::max(0, contentH - viewH);
}

static RECT Remap_GetPackRectClient(HWND hWnd, RemapPanelState* st, int packIdx)
{
    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);

    const int yOff = st ? -st->scrollY : 0;
    const int startY = S(hWnd, 104);
    const int packGapY = S(hWnd, 16);
    const int headerH = S(hWnd, 22);
    const int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
    const int btnH = btnW;
    const int gapY = S(hWnd, ICON_GAP_Y);
    const int rowsPerPack = 2;
    const int packStrideY = headerH + rowsPerPack * btnH + (rowsPerPack - 1) * gapY + packGapY;

    RECT r{};
    const int topPad = S(hWnd, 2);
    const int gridBottomPad = S(hWnd, 6);
    r.left = S(hWnd, 8);
    // Include the "Gamepad N" header line inside the colored card.
    r.top = yOff + startY + packIdx * packStrideY - topPad;
    r.right = rcClient.right - Remap_ScrollbarWidthPx(hWnd) - Remap_ScrollbarMarginPx(hWnd) * 2;
    r.bottom = yOff + startY + packIdx * packStrideY + headerH + rowsPerPack * btnH + (rowsPerPack - 1) * gapY + gridBottomPad;

    if (r.right <= r.left)
        r.right = r.left + S(hWnd, 320);

    return r;
}

static COLORREF Remap_GetPackTileColor(RemapPanelState* st, int packIdx, bool stronger)
{
    int total = st ? std::max(1, st->gamepadPacks) : 1;
    int styleVariant = Remap_GetIconStyleVariantForPack(packIdx, total);
    COLORREF accent = Remap_GetAccentColorForStyleVariant(styleVariant);
    COLORREF base = UiTheme::Color_PanelBg();
    float t = stronger ? 0.16f : 0.09f;
    return LerpColor(base, accent, t);
}

static void DrawPackBackgrounds(HWND hWnd, HDC hdc, RemapPanelState* st)
{
    if (!st) return;

    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    int packs = std::max(1, st->gamepadPacks);
    for (int p = 0; p < packs; ++p)
    {
        RECT rr = Remap_GetPackRectClient(hWnd, st, p);
        if (rr.bottom <= 0 || rr.top >= rcClient.bottom)
            continue;

        int total = std::max(1, st->gamepadPacks);
        int styleVariant = Remap_GetIconStyleVariantForPack(p, total);
        COLORREF accent = Remap_GetAccentColorForStyleVariant(styleVariant);

        COLORREF fill = LerpColor(UiTheme::Color_PanelBg(), accent, 0.10f);
        COLORREF border = LerpColor(UiTheme::Color_Border(), accent, 0.36f);

        RectF rf((REAL)rr.left, (REAL)rr.top, (REAL)(rr.right - rr.left), (REAL)(rr.bottom - rr.top));
        float rad = (float)std::max(4, S(hWnd, 10));

        SolidBrush brFill(Gp(fill, 232));
        Pen penBorder(Gp(border, 165), 1.0f);

        GraphicsPath pth;
        AddRoundRectPath(pth, rf, rad);
        g.FillPath(&brFill, &pth);
        g.DrawPath(&penBorder, &pth);
    }
}

static RECT Remap_GetScrollTrackRect(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = Remap_ScrollbarWidthPx(hWnd);
    int m = Remap_ScrollbarMarginPx(hWnd);
    int right = (int)rc.right;
    int bottom = (int)rc.bottom;

    RECT tr{};
    int left = std::max(0, right - m - w);
    tr.left = (LONG)left;
    tr.right = (LONG)std::max(left + 1, right - m);
    tr.top = m;
    tr.bottom = (LONG)std::max((int)tr.top + 1, bottom - m);
    return tr;
}

static RECT Remap_GetScrollThumbRect(HWND hWnd, RemapPanelState* st)
{
    RECT tr = Remap_GetScrollTrackRect(hWnd);
    if (!st) return tr;

    int trackH = std::max(1, (int)tr.bottom - (int)tr.top);
    int viewH = std::max(1, Remap_GetViewportHeight(hWnd));
    int maxScroll = Remap_GetMaxScroll(hWnd, st);
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

static void Remap_RequestFullRepaint(HWND hWnd)
{
    RedrawWindow(hWnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ALLCHILDREN);
}

static void Remap_SetScrollY(HWND hWnd, RemapPanelState* st, int newScrollY)
{
    if (!st) return;

    int maxScroll = Remap_GetMaxScroll(hWnd, st);
    st->scrollMax = maxScroll;

    int target = std::clamp(newScrollY, 0, maxScroll);
    if (target != st->scrollY)
    {
        st->scrollY = target;

        // Keep scrolling deterministic: recompute child positions instead of pixel-scroll.
        // This avoids owner-draw icon loss and scrollbar smear artifacts.
        ApplyRemapSizing(hWnd, st);
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return;
    }

    RECT tr = Remap_GetScrollTrackRect(hWnd);
    InvalidateRect(hWnd, &tr, FALSE);
}

static bool Remap_AddGamepadPack(HWND hWnd, RemapPanelState* st, HINSTANCE hInst, HFONT hFont)
{
    if (!st) return false;
    if (st->gamepadPacks >= REMAP_MAX_GAMEPADS) return false;

    int newCount = std::clamp(st->gamepadPacks + 1, 1, REMAP_MAX_GAMEPADS);
    int newPadIndex = newCount - 1;
    int styleForNewPad = Remap_PickUnusedStyleVariant(st->gamepadPacks);
    Bindings_SetPadStyleVariant(newPadIndex, styleForNewPad);
    Settings_SetVirtualGamepadCount(newCount);
    Backend_SetVirtualGamepadCount(newCount);

    Remap_RebuildGamepadPacksBatched(hWnd, st, hInst, hFont, newCount);

    Settings_SetVirtualGamepadCount(st->gamepadPacks);
    Backend_SetVirtualGamepadCount(st->gamepadPacks);
    Remap_UpdateAddGamepadButtonText(st);
    Remap_RequestSettingsSave(hWnd);
    return true;
}

static void ApplyRemapSizing(HWND hWnd, RemapPanelState* st)
{
    if (!st) return;

    st->scrollMax = Remap_GetMaxScroll(hWnd, st);
    st->scrollY = std::clamp(st->scrollY, 0, st->scrollMax);
    const int yOff = -st->scrollY;

    st->ghostW = S(hWnd, (int)Settings_GetDragIconSizePx());
    st->ghostH = S(hWnd, (int)Settings_GetDragIconSizePx());

    if (st->hGhost)
        Ghost_EnsureSurfaceAndResetCache(st);

    int wndCount = 0;
    if (st->txtHelp && IsWindow(st->txtHelp)) ++wndCount;
    if (st->btnAddGamepad && IsWindow(st->btnAddGamepad)) ++wndCount;
    for (HWND h : st->packLabels) if (h && IsWindow(h)) ++wndCount;
    for (HWND h : st->packRemoveBtns) if (h && IsWindow(h)) ++wndCount;
    for (HWND h : st->iconBtns) if (h && IsWindow(h)) ++wndCount;
    HDWP hdwp = BeginDeferWindowPos(std::max(8, wndCount));

    auto Move = [&](HWND w, int x, int y, int ww, int hh)
    {
        if (!w || !IsWindow(w)) return;
        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
        if (hdwp) hdwp = DeferWindowPos(hdwp, w, nullptr, x, y, ww, hh, flags);
        else SetWindowPos(w, nullptr, x, y, ww, hh, flags);
    };

    int helpX = S(hWnd, 12);
    RECT trackForHelp = Remap_GetScrollTrackRect(hWnd);
    int helpRight = std::max(helpX + S(hWnd, 220), (int)trackForHelp.left - S(hWnd, 8));
    int helpW = std::max(S(hWnd, 220), helpRight - helpX);
    Move(st->txtHelp, helpX, yOff + S(hWnd, 10), helpW, S(hWnd, 52));
    Move(st->btnAddGamepad, S(hWnd, 12), yOff + S(hWnd, 66), S(hWnd, 210), S(hWnd, 28));

    const int startX = S(hWnd, 12);
    const int startY = S(hWnd, 104);
    const int packGapY = S(hWnd, 16);
    const int headerH = S(hWnd, 22);
    const int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
    const int btnH = btnW;
    const int gapX = S(hWnd, ICON_GAP_X);
    const int gapY = S(hWnd, ICON_GAP_Y);
    const int cols = ICON_COLS;
    const int rowsPerPack = 2;
    const int packStrideY = headerH + rowsPerPack * btnH + (rowsPerPack - 1) * gapY + packGapY;

    for (int p = 0; p < (int)st->packLabels.size(); ++p)
    {
        HWND lbl = st->packLabels[(size_t)p];
        if (lbl && IsWindow(lbl))
        {
            Move(lbl, startX, yOff + startY + p * packStrideY, S(hWnd, 220), headerH);
        }

        if (p < (int)st->packRemoveBtns.size())
        {
            HWND rb = st->packRemoveBtns[(size_t)p];
            if (rb && IsWindow(rb))
            {
                Move(rb, startX + S(hWnd, 68), yOff + startY + p * packStrideY, S(hWnd, 22), S(hWnd, 22));
            }
        }
    }

    int n = (int)st->iconBtns.size();
    for (int i = 0; i < n; ++i)
    {
        if (!st->iconBtns[i]) continue;
        int iconsPerPack = std::max(1, st->iconsPerPack);
        int packIdx = i / iconsPerPack;
        int localIdx = i % iconsPerPack;
        int cx = localIdx % cols;
        int row = localIdx / cols;

        Move(st->iconBtns[i],
            startX + cx * (btnW + gapX),
            yOff + startY + packIdx * packStrideY + headerH + row * (btnH + gapY),
            btnW, btnH);
    }

    if (hdwp) EndDeferWindowPos(hdwp);
    InvalidateRect(hWnd, nullptr, FALSE);
}

// ---------------- Drag tick (while dragging only) ----------------
static void DragTick(HWND hPanel, RemapPanelState* st, float dt)
{
    if (!st || !st->dragging) return;

    POINT pt{};
    GetCursorPos(&pt);

    // source icon detach/attach
    if (st->dragSrcIconBtn)
    {
        int showPx = 0, hidePx = 0;
        GetDetachThresholds(hPanel, st, showPx, hidePx);

        float dx = (float)(pt.x - st->dragSrcCenterScreen.x);
        float dy = (float)(pt.y - st->dragSrcCenterScreen.y);
        float dist = std::sqrt(dx * dx + dy * dy);

        float target = st->srcIconScaleTarget;

        if (target < 0.5f)
            target = (dist >= (float)showPx) ? 1.0f : 0.0f;
        else
            target = (dist <= (float)hidePx) ? 0.0f : 1.0f;

        st->srcIconScaleTarget = target;

        const float lambda = 22.0f;
        float a = 1.0f - std::exp(-lambda * dt);

        float oldScale = st->srcIconScale;
        st->srcIconScale = std::clamp(oldScale + (target - oldScale) * a, 0.0f, 1.0f);

        if (std::fabs(st->srcIconScale - oldScale) >= 0.004f)
            InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);
    }

    uint16_t hid = 0;
    RECT rcKey{};

    if (!TryGetKeyUnderCursor(st, pt, hid, rcKey))
    {
        int thr = S(hPanel, 42);
        FindNearestKey(st, pt, thr, hid, rcKey);
    }

    if (hid != 0)
    {
        st->hoverHid = hid;
        st->hoverKeyRectScreen = rcKey;
    }
    else
    {
        st->hoverHid = 0;
        st->hoverKeyRectScreen = RECT{};
    }

    KeyboardUI_SetDragHoverHid(st->hoverHid);

    if (st->hoverHid != 0)
    {
        int cx = (st->hoverKeyRectScreen.left + st->hoverKeyRectScreen.right) / 2;
        int cy = (st->hoverKeyRectScreen.top + st->hoverKeyRectScreen.bottom) / 2;
        st->tx = (float)(cx - st->ghostW / 2);
        st->ty = (float)(cy - st->ghostH / 2);
    }
    else
    {
        st->tx = (float)(pt.x - st->ghostW / 2);
        st->ty = (float)(pt.y - st->ghostH / 2);
    }

    const float lambda = (st->hoverHid != 0) ? 24.0f : 18.0f;
    float a = 1.0f - expf(-lambda * dt);

    st->gx += (st->tx - st->gx) * a;
    st->gy += (st->ty - st->gy) * a;

    Ghost_ShowFullAt(st, st->gx, st->gy);
}

// ---------------- Timer tick ----------------
static void PanelAnimTick(HWND hPanel, RemapPanelState* st)
{
    if (!st) return;

    UINT wantMs = GetAnimIntervalMs();
    if (wantMs != st->animIntervalMs)
    {
        st->animIntervalMs = wantMs;
        SetTimer(hPanel, DRAG_ANIM_TIMER_ID, st->animIntervalMs, nullptr);
    }

    DWORD now = GetTickCount();
    float dt = 0.016f;
    if (st->lastTick != 0)
    {
        dt = (float)(now - st->lastTick) / 1000.0f;
        dt = std::clamp(dt, 0.001f, 0.050f);
    }
    st->lastTick = now;

    if (st->dragging)
    {
        DragTick(hPanel, st, dt);
        return;
    }

    if (st->postMode != RemapPostAnimMode::None)
    {
        bool done = PostAnim_Tick(hPanel, st);
        if (done)
            PostAnim_Finish(hPanel, st);
        return;
    }

    KillTimer(hPanel, DRAG_ANIM_TIMER_ID);
    st->animIntervalMs = 0;
}

// ---------------- Panel background ----------------
static void DrawRemapScrollbar(HWND hWnd, HDC hdc, RemapPanelState* st)
{
    if (!st) return;
    int maxScroll = Remap_GetMaxScroll(hWnd, st);
    if (maxScroll <= 0) return;

    RECT trR = Remap_GetScrollTrackRect(hWnd);
    RECT thR = Remap_GetScrollThumbRect(hWnd, st);

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

static void PaintPanelBg(HWND hWnd, RemapPanelState* st)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int w = (int)(rc.right - rc.left);
    int h = (int)(rc.bottom - rc.top);
    if (w <= 0 || h <= 0)
    {
        EndPaint(hWnd, &ps);
        return;
    }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    if (!mem || !bmp)
    {
        if (bmp) DeleteObject(bmp);
        if (mem) DeleteDC(mem);
        FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
        DrawPackBackgrounds(hWnd, hdc, st);
        DrawRemapScrollbar(hWnd, hdc, st);
        EndPaint(hWnd, &ps);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    RECT local{ 0,0,w,h };
    FillRect(mem, &local, UiTheme::Brush_PanelBg());
    DrawPackBackgrounds(hWnd, mem, st);
    DrawRemapScrollbar(hWnd, mem, st);

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hWnd, &ps);
}

// ---------------- Apply binding helpers ----------------
static uint16_t GetOldHidForAction(int padIndex, BindAction act)
{
    switch (act)
    {
    case BindAction::Axis_LX_Minus: return Bindings_GetAxisForPad(padIndex, Axis::LX).minusHid;
    case BindAction::Axis_LX_Plus:  return Bindings_GetAxisForPad(padIndex, Axis::LX).plusHid;
    case BindAction::Axis_LY_Minus: return Bindings_GetAxisForPad(padIndex, Axis::LY).minusHid;
    case BindAction::Axis_LY_Plus:  return Bindings_GetAxisForPad(padIndex, Axis::LY).plusHid;
    case BindAction::Axis_RX_Minus: return Bindings_GetAxisForPad(padIndex, Axis::RX).minusHid;
    case BindAction::Axis_RX_Plus:  return Bindings_GetAxisForPad(padIndex, Axis::RX).plusHid;
    case BindAction::Axis_RY_Minus: return Bindings_GetAxisForPad(padIndex, Axis::RY).minusHid;
    case BindAction::Axis_RY_Plus:  return Bindings_GetAxisForPad(padIndex, Axis::RY).plusHid;
    case BindAction::Trigger_LT:    return Bindings_GetTriggerForPad(padIndex, Trigger::LT);
    case BindAction::Trigger_RT:    return Bindings_GetTriggerForPad(padIndex, Trigger::RT);
    default: return 0;
    }
}

// ---------------- Icon subclass (start drag) ----------------
static LRESULT CALLBACK IconSubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dwRefData)
{
    if (msg == WM_LBUTTONDOWN)
    {
        HWND hPanel = GetParent(hBtn);
        auto* st = (RemapPanelState*)GetWindowLongPtrW(hPanel, GWLP_USERDATA);
        if (st)
        {
            if (st->postMode != RemapPostAnimMode::None)
                StopAllPanelAnim_Immediate(hPanel, st);

            st->dragging = true;
            st->dragAction = (BindAction)dwRefData;
            st->dragIconIdx = (int)GetWindowLongPtrW(hBtn, GWLP_USERDATA);
            {
                int ctrlId = GetDlgCtrlID(hBtn);
                int packIdx = (ctrlId - REMAP_ICON_ID_BASE) / REMAP_ICON_ID_PACK_STRIDE;
                st->dragPadIndex = std::clamp(packIdx, 0, REMAP_MAX_GAMEPADS - 1);
            }

            st->hoverHid = 0;
            st->hoverKeyRectScreen = RECT{};
            st->keyBtns.clear();

            st->ghostRenderedIconIdx = -1;
            st->ghostRenderedSize = 0;
            st->ghostRenderedStyleVariant = -1;

            st->dragSrcIconBtn = hBtn;
            {
                RECT src{};
                GetWindowRect(hBtn, &src);
                st->dragSrcCenterScreen.x = (src.left + src.right) / 2;
                st->dragSrcCenterScreen.y = (src.top + src.bottom) / 2;
            }

            // Hide slot icon immediately
            st->srcIconScale = 0.0f;
            st->srcIconScaleTarget = 0.0f;
            RedrawWindow(hBtn, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);

            // Start ghost at icon position
            RECT src{};
            GetWindowRect(hBtn, &src);
            st->gx = (float)src.left;
            st->gy = (float)src.top;

            POINT pt{};
            GetCursorPos(&pt);
            st->tx = (float)(pt.x - st->ghostW / 2);
            st->ty = (float)(pt.y - st->ghostH / 2);

            st->lastTick = 0;
            BuildKeyCache(st);

            SetFocus(hPanel);
            SetCapture(hPanel);

            st->animIntervalMs = GetAnimIntervalMs();
            SetTimer(hPanel, DRAG_ANIM_TIMER_ID, st->animIntervalMs, nullptr);

            KeyboardUI_SetDragHoverHid(0);

            Ghost_ShowFullAt(st, st->gx, st->gy);
        }
        return 0;
    }

    // IMPORTANT FIX: pass through the real wParam/lParam, NOT 0/0
    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

// ---------------- Panel wndproc ----------------
static LRESULT CALLBACK RemapPanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (RemapPanelState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
        PaintPanelBg(hWnd, st);
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, UiTheme::Color_Text());
        HWND hCtl = (HWND)lParam;
        if (st)
        {
            for (int i = 0; i < (int)st->packLabels.size(); ++i)
            {
                HWND h = st->packLabels[(size_t)i];
                if (h && h == hCtl)
                {
                    SetBkMode(hdc, OPAQUE);
                    HBRUSH hb = Remap_GetPackHeaderBrush(st, i);
                    SetBkColor(hdc, g_packHeaderBrushColors[std::clamp(i, 0, REMAP_MAX_GAMEPADS - 1)]);
                    return (LRESULT)hb;
                }
            }
        }
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_APP_REMAP_APPLY_SETTINGS:
        if (st) ApplyRemapSizing(hWnd, st);
        return 0;

    case WM_SIZE:
        if (st) ApplyRemapSizing(hWnd, st);
        return 0;

    case WM_LBUTTONDOWN:
        if (st && !st->dragging)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT thumb = Remap_GetScrollThumbRect(hWnd, st);
            RECT track = Remap_GetScrollTrackRect(hWnd);
            int maxScroll = Remap_GetMaxScroll(hWnd, st);

            if (maxScroll > 0 && PtInRect(&thumb, pt))
            {
                st->scrollDrag = true;
                st->scrollDragGrabOffsetY = pt.y - thumb.top;
                st->scrollDragThumbHeight = std::max(1, (int)thumb.bottom - (int)thumb.top);
                st->scrollDragMax = maxScroll;
                SetCapture(hWnd);
                Remap_RequestFullRepaint(hWnd);
                return 0;
            }

            if (maxScroll > 0 && PtInRect(&track, pt))
            {
                int page = std::max(1, Remap_GetViewportHeight(hWnd) - S(hWnd, 48));
                if (pt.y < thumb.top) Remap_SetScrollY(hWnd, st, st->scrollY - page);
                else if (pt.y >= thumb.bottom) Remap_SetScrollY(hWnd, st, st->scrollY + page);
                return 0;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (st && st->scrollDrag)
        {
            POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT track = Remap_GetScrollTrackRect(hWnd);
            int trackH = std::max(1, (int)track.bottom - (int)track.top);
            int thumbH = std::max(1, st->scrollDragThumbHeight);
            int travel = std::max(1, trackH - thumbH);
            int maxScroll = std::max(1, st->scrollDragMax);

            int topWanted = pt.y - st->scrollDragGrabOffsetY;
            int topMin = (int)track.top;
            int topMax = (int)track.bottom - thumbH;
            int top = std::clamp(topWanted, topMin, topMax);

            double t = (double)(top - topMin) / (double)travel;
            int target = (int)std::lround(t * (double)maxScroll);
            Remap_SetScrollY(hWnd, st, target);
            return 0;
        }
        break;

    case WM_VSCROLL:
        if (st)
        {
            const int clientH = Remap_GetViewportHeight(hWnd);
            const int lineStep = std::max(S(hWnd, 28), 12);
            const int pageStep = std::max(clientH - S(hWnd, 32), lineStep);

            int next = st->scrollY;
            switch (LOWORD(wParam))
            {
            case SB_TOP:           next = 0; break;
            case SB_BOTTOM:        next = Remap_GetMaxScroll(hWnd, st); break;
            case SB_LINEUP:        next -= lineStep; break;
            case SB_LINEDOWN:      next += lineStep; break;
            case SB_PAGEUP:        next -= pageStep; break;
            case SB_PAGEDOWN:      next += pageStep; break;
            default:               return 0;
            }

            Remap_SetScrollY(hWnd, st, next);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st)
        {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            st->wheelRemainder += delta;

            UINT lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);

            const int clientH = Remap_GetViewportHeight(hWnd);
            const int lineStep = std::max(S(hWnd, 28), 12);
            const int pageStep = std::max(clientH - S(hWnd, 32), lineStep);

            int next = st->scrollY;
            while (std::abs(st->wheelRemainder) >= WHEEL_DELTA)
            {
                const int dir = (st->wheelRemainder > 0) ? 1 : -1;
                st->wheelRemainder -= dir * WHEEL_DELTA;

                int step = 0;
                if (lines == WHEEL_PAGESCROLL) step = pageStep;
                else step = (int)std::max<UINT>(1, lines) * lineStep;

                next -= dir * step;
            }
            Remap_SetScrollY(hWnd, st, next);
            return 0;
        }
        return 0;

    case WM_CREATE:
    {
        auto* cs = (CREATESTRUCTW*)lParam;
        auto* init = (RemapPanelState*)cs->lpCreateParams;
        st = new RemapPanelState(*init);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        HWND owner = st->hKeyboardHost ? GetAncestor(st->hKeyboardHost, GA_ROOT) : nullptr;
        Ghost_EnsureCreated(st, cs->hInstance, owner);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st->txtHelp = CreateWindowW(L"STATIC",
            L"Drag and drop a gamepad control onto a keyboard key to bind.\n"
            L"Right click a key on the keyboard to unbind.\n"
            L"Press ESC to cancel dragging.",
            WS_CHILD | WS_VISIBLE,
            S(hWnd, 12), S(hWnd, 10), S(hWnd, 860), S(hWnd, 52),
            hWnd, nullptr, cs->hInstance, nullptr);
        SendMessageW(st->txtHelp, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->btnAddGamepad = CreateWindowW(L"BUTTON", L"Add Gamepad",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            S(hWnd, 12), S(hWnd, 66), S(hWnd, 210), S(hWnd, 28),
            hWnd, (HMENU)(INT_PTR)REMAP_ID_ADD_GAMEPAD, cs->hInstance, nullptr);
        SendMessageW(st->btnAddGamepad, WM_SETFONT, (WPARAM)hFont, TRUE);

        int savedPacks = std::clamp(Settings_GetVirtualGamepadCount(), 1, REMAP_MAX_GAMEPADS);
        Remap_RebuildGamepadPacks(hWnd, st, cs->hInstance, hFont, savedPacks);
        Settings_SetVirtualGamepadCount(st->gamepadPacks);
        Settings_SetVirtualGamepadsEnabled(true);
        Backend_SetVirtualGamepadCount(st->gamepadPacks);
        Backend_SetVirtualGamepadsEnabled(true);
        Remap_UpdateAddGamepadButtonText(st);

        ApplyRemapSizing(hWnd, st);
        return 0;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (st) StopAllPanelAnim_Immediate(hWnd, st);
            return 0;
        }
        return 0;

    case WM_COMMAND:
        if (st && LOWORD(wParam) == (UINT)REMAP_ID_ADD_GAMEPAD && HIWORD(wParam) == BN_CLICKED)
        {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            if (Remap_AddGamepadPack(hWnd, st, hInst, hFont))
                InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (st && HIWORD(wParam) == BN_CLICKED)
        {
            int id = (int)LOWORD(wParam);
            if (id >= REMAP_ID_REMOVE_GAMEPAD_BASE && id < REMAP_ID_REMOVE_GAMEPAD_BASE + REMAP_MAX_GAMEPADS)
            {
                int packIdx = id - REMAP_ID_REMOVE_GAMEPAD_BASE;
                if (packIdx > 0 && packIdx < st->gamepadPacks)
                {
                    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
                    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                    int newCount = std::clamp(st->gamepadPacks - 1, 1, REMAP_MAX_GAMEPADS);

                    // Remove selected pad and keep following pads (shift left).
                    Bindings_RemovePadAndCompact(packIdx, st->gamepadPacks);
                    Profile_SaveIni(AppPaths_BindingsIni().c_str());
                    Settings_SetVirtualGamepadCount(newCount);
                    Backend_SetVirtualGamepadCount(newCount);

                    Remap_RebuildGamepadPacksBatched(hWnd, st, hInst, hFont, newCount);

                    Settings_SetVirtualGamepadCount(st->gamepadPacks);
                    Backend_SetVirtualGamepadCount(st->gamepadPacks);
                    Remap_UpdateAddGamepadButtonText(st);
                    Remap_RequestSettingsSave(hWnd);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
        }
        return 0;

    case WM_TIMER:
        if (wParam == DRAG_ANIM_TIMER_ID && st)
        {
            PanelAnimTick(hWnd, st);
            return 0;
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            if (!st->dragging && GetCapture() == hWnd)
                ReleaseCapture();
            Remap_RequestFullRepaint(hWnd);
            return 0;
        }

        if (st && st->dragging)
        {
            // Updated post-drop logic.
            uint16_t newHid = st->hoverHid;
            BindAction act = st->dragAction;
            uint16_t oldHid = GetOldHidForAction(st->dragPadIndex, act);

            bool bound = (newHid != 0);

            if (bound)
            {
                // Apply binding immediately
                BindingActions_ApplyForPad(st->dragPadIndex, act, newHid);
                Profile_SaveIni(AppPaths_BindingsIni().c_str());
                InvalidateHidKey(oldHid);
                InvalidateHidKey(newHid);
                if (st->hKeyboardHost) InvalidateRect(st->hKeyboardHost, nullptr, FALSE);

                // IMPORTANT: instant cleanup - NO shrink animation
                st->dragging = false;

                if (GetCapture() == hWnd)
                    ReleaseCapture();

                KeyboardUI_SetDragHoverHid(0);

                st->hoverHid = 0;
                st->hoverKeyRectScreen = RECT{};
                st->keyBtns.clear();

                // restore source icon in remap panel immediately
                if (st->dragSrcIconBtn)
                {
                    st->srcIconScale = 1.0f;
                    st->srcIconScaleTarget = 1.0f;
                    InvalidateRect(st->dragSrcIconBtn, nullptr, FALSE);
                }
                st->dragSrcIconBtn = nullptr;
                st->dragSrcCenterScreen = POINT{};

                // stop any post animation + hide ghost instantly
                st->postMode = RemapPostAnimMode::None;
                st->postPhase = 0;
                st->postPhaseStartTick = 0;
                st->postPhaseDurationMs = 0;
                st->shrinkStartMs = 0;
                st->shrinkDurMs = 0;

                KillTimer(hWnd, DRAG_ANIM_TIMER_ID);
                st->animIntervalMs = 0;

                Ghost_Hide(st);

                return 0;
            }

            // --- else: NOT bound => keep existing fly back / shrink away logic ---
            st->dragging = false;

            if (GetCapture() == hWnd)
                ReleaseCapture();

            KeyboardUI_SetDragHoverHid(0);

            st->hoverHid = 0;
            st->hoverKeyRectScreen = RECT{};
            st->keyBtns.clear();

            bool shouldFlyBack = false;

            if (!bound && st->dragSrcIconBtn)
            {
                int showPx = 0, hidePx = 0;
                GetDetachThresholds(hWnd, st, showPx, hidePx);
                (void)hidePx;

                POINT pt{};
                GetCursorPos(&pt);

                float dx = (float)(pt.x - st->dragSrcCenterScreen.x);
                float dy = (float)(pt.y - st->dragSrcCenterScreen.y);
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < (float)showPx && st->srcIconScale < 0.35f)
                    shouldFlyBack = true;
            }

            if (shouldFlyBack)
                PostAnim_StartFlyBack(hWnd, st);
            else
                PostAnim_StartShrinkAway(hWnd, st);

            return 0;
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (st && st->scrollDrag)
        {
            st->scrollDrag = false;
            Remap_RequestFullRepaint(hWnd);
        }
        if (st && st->dragging)
            StopAllPanelAnim_Immediate(hWnd, st);
        return 0;

    case WM_DESTROY:
        KeyboardUI_SetDragHoverHid(0);
        if (st)
        {
            StopAllPanelAnim_Immediate(hWnd, st);
            Remap_DestroyPackControls(st);

            if (st->hGhost)
            {
                DestroyWindow(st->hGhost);
                st->hGhost = nullptr;
            }
            Ghost_FreeSurface(st);

            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }

        Remap_FreePackHeaderBrushes();
        IconCache_Clear();
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (!dis || dis->CtlType != ODT_BUTTON) return FALSE;

        if (dis->CtlID == (UINT)REMAP_ID_ADD_GAMEPAD)
        {
            DrawAddGamepadButton(dis);
            return TRUE;
        }

        if (dis->CtlID >= (UINT)REMAP_ID_REMOVE_GAMEPAD_BASE &&
            dis->CtlID < (UINT)(REMAP_ID_REMOVE_GAMEPAD_BASE + REMAP_MAX_GAMEPADS))
        {
            int packIdx = (int)dis->CtlID - REMAP_ID_REMOVE_GAMEPAD_BASE;
            DrawDisableGamepadIconButton(dis, st, packIdx);
            return TRUE;
        }

        int idx = (int)GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA);
        if (idx < 0 || idx >= RemapIcons_Count()) return FALSE;

        DrawIconButton(dis, idx, st);
        return TRUE;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HWND RemapPanel_Create(HWND hParent, HINSTANCE hInst, HWND hKeyboardHost)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = RemapPanelProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"RemapPanelClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    RemapPanelState init{};
    init.hKeyboardHost = hKeyboardHost;

    return CreateWindowW(L"RemapPanelClass", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 100, 100,
        hParent, nullptr, hInst, &init);
}

void RemapPanel_SetSelectedHid(uint16_t) {}

