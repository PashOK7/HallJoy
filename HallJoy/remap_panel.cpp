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
#include "settings.h"
#include "win_util.h"
#include "app_paths.h"
#include "binding_actions.h"
#include "bindings.h"
#include "ui_theme.h"
#include "remap_icons.h"

using namespace Gdiplus;

static constexpr UINT WM_APP_REMAP_APPLY_SETTINGS = WM_APP + 42;

static constexpr int ICON_GAP_X = 6;
static constexpr int ICON_GAP_Y = 6;
static constexpr int ICON_COLS = 13;

static constexpr UINT_PTR DRAG_ANIM_TIMER_ID = 9009;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

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

static constexpr BYTE GHOST_ALPHA_FREE = 190;
static constexpr BYTE GHOST_ALPHA_SNAP = 215;

// ---------------- anim helpers ----------------
static inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

static inline float EaseOutCubic(float t)
{
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// ---------------- Icon glyph cache ----------------
// key = (size<<32) | (iconIdx<<1) | (pressed?1:0)
struct CachedGlyph
{
    int size = 0;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
};

static std::unordered_map<uint64_t, CachedGlyph> g_iconCache;

static uint64_t MakeIconKey(int iconIdx, int size, bool pressed)
{
    return ((uint64_t)(uint32_t)size << 32) | ((uint64_t)(uint32_t)iconIdx << 1) | (pressed ? 1ULL : 0ULL);
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

static CachedGlyph* Icon_GetOrCreate(int iconIdx, int size, bool pressed, float padRatio)
{
    if (iconIdx < 0 || size <= 0) return nullptr;

    uint64_t key = MakeIconKey(iconIdx, size, pressed);
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
    RemapIcons_DrawGlyphAA(cg.dc, rc, iconIdx, pressed, padRatio);

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

    bool dragging = false;
    BindAction dragAction{};
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

    int ghostRenderedIconIdx = -1;
    int ghostRenderedSize = 0;

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

    if (st->ghostRenderedIconIdx == st->dragIconIdx && st->ghostRenderedSize == sz)
        return;

    st->ghostRenderedIconIdx = st->dragIconIdx;
    st->ghostRenderedSize = sz;

    std::memset(st->ghostBits, 0, (size_t)sz * (size_t)sz * 4);

    CachedGlyph* cg = Icon_GetOrCreate(st->dragIconIdx, sz, true, 0.135f);
    if (cg && cg->dc)
        BitBlt(st->ghostMemDC, 0, 0, sz, sz, cg->dc, 0, 0, SRCCOPY);
    else
    {
        RECT rc{ 0,0,sz,sz };
        RemapIcons_DrawGlyphAA(st->ghostMemDC, rc, st->dragIconIdx, true, 0.135f);
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
        return;
    }

    s = std::clamp(s, 2, base);

    int x = (w - s) / 2;
    int y = (h - s) / 2;

    RECT rc{ x, y, x + s, y + s };
    RemapIcons_DrawGlyphAA(st->ghostMemDC, rc, st->dragIconIdx, true, 0.135f);

    st->ghostRenderedIconIdx = -1;
    st->ghostRenderedSize = 0;
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
        return;

    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    RECT local{ 0,0,w,h };
    FillRect(mem, &local, pressed ? UiTheme::Brush_ControlBg() : UiTheme::Brush_PanelBg());

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
    CachedGlyph* cg = Icon_GetOrCreate(iconIdx, size, pressed, 0.135f);
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
        RemapIcons_DrawGlyphAA(mem, rcIcon, iconIdx, pressed, 0.135f);
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
static void ApplyRemapSizing(HWND hWnd, RemapPanelState* st)
{
    if (!st) return;

    st->ghostW = S(hWnd, (int)Settings_GetDragIconSizePx());
    st->ghostH = S(hWnd, (int)Settings_GetDragIconSizePx());

    if (st->hGhost)
        Ghost_EnsureSurfaceAndResetCache(st);

    const int startX = S(hWnd, 12);
    const int startY = S(hWnd, 70);
    const int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
    const int btnH = btnW;
    const int gapX = S(hWnd, ICON_GAP_X);
    const int gapY = S(hWnd, ICON_GAP_Y);
    const int cols = ICON_COLS;

    int n = (int)st->iconBtns.size();
    for (int i = 0; i < n; ++i)
    {
        if (!st->iconBtns[i]) continue;
        int cx = i % cols;
        int cy = i / cols;

        SetWindowPos(st->iconBtns[i], nullptr,
            startX + cx * (btnW + gapX),
            startY + cy * (btnH + gapY),
            btnW, btnH,
            SWP_NOZORDER);

        InvalidateRect(st->iconBtns[i], nullptr, FALSE);
    }
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
static void PaintPanelBg(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);
    FillRect(hdc, &rc, UiTheme::Brush_PanelBg());
    EndPaint(hWnd, &ps);
}

// ---------------- Apply binding helpers ----------------
static uint16_t GetOldHidForAction(BindAction act)
{
    switch (act)
    {
    case BindAction::Axis_LX_Minus: return Bindings_GetAxis(Axis::LX).minusHid;
    case BindAction::Axis_LX_Plus:  return Bindings_GetAxis(Axis::LX).plusHid;
    case BindAction::Axis_LY_Minus: return Bindings_GetAxis(Axis::LY).minusHid;
    case BindAction::Axis_LY_Plus:  return Bindings_GetAxis(Axis::LY).plusHid;
    case BindAction::Axis_RX_Minus: return Bindings_GetAxis(Axis::RX).minusHid;
    case BindAction::Axis_RX_Plus:  return Bindings_GetAxis(Axis::RX).plusHid;
    case BindAction::Axis_RY_Minus: return Bindings_GetAxis(Axis::RY).minusHid;
    case BindAction::Axis_RY_Plus:  return Bindings_GetAxis(Axis::RY).plusHid;
    case BindAction::Trigger_LT:    return Bindings_GetTrigger(Trigger::LT);
    case BindAction::Trigger_RT:    return Bindings_GetTrigger(Trigger::RT);
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

            ApplyRemapSizing(hPanel, st);

            st->dragging = true;
            st->dragAction = (BindAction)dwRefData;
            st->dragIconIdx = (int)GetWindowLongPtrW(hBtn, GWLP_USERDATA);

            st->hoverHid = 0;
            st->hoverKeyRectScreen = RECT{};
            st->keyBtns.clear();

            st->ghostRenderedIconIdx = -1;
            st->ghostRenderedSize = 0;

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
            InvalidateRect(hBtn, nullptr, FALSE);

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
        PaintPanelBg(hWnd);
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UiTheme::Color_Text());
        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_APP_REMAP_APPLY_SETTINGS:
        if (st) ApplyRemapSizing(hWnd, st);
        return 0;

    case WM_SIZE:
        if (st) ApplyRemapSizing(hWnd, st);
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

        HWND txt = CreateWindowW(L"STATIC",
            L"Drag and drop a gamepad control onto a keyboard key to bind.\n"
            L"Right click a key on the keyboard to unbind.\n"
            L"Press ESC to cancel dragging.",
            WS_CHILD | WS_VISIBLE,
            S(hWnd, 12), S(hWnd, 10), S(hWnd, 820), S(hWnd, 52),
            hWnd, nullptr, cs->hInstance, nullptr);
        SendMessageW(txt, WM_SETFONT, (WPARAM)hFont, TRUE);

        const int startX = S(hWnd, 12);
        const int startY = S(hWnd, 70);
        const int btnW = S(hWnd, (int)Settings_GetRemapButtonSizePx());
        const int btnH = btnW;
        const int gapX = S(hWnd, ICON_GAP_X);
        const int gapY = S(hWnd, ICON_GAP_Y);
        const int cols = ICON_COLS;

        int n = RemapIcons_Count();
        st->iconBtns.assign(n, nullptr);

        for (int i = 0; i < n; ++i)
        {
            int cx = i % cols;
            int cy = i / cols;
            const RemapIconDef& idef = RemapIcons_Get(i);

            HWND b = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                startX + cx * (btnW + gapX),
                startY + cy * (btnH + gapY),
                btnW, btnH,
                hWnd, (HMENU)(1000 + i), cs->hInstance, nullptr);

            SendMessageW(b, WM_SETFONT, (WPARAM)hFont, TRUE);
            SetWindowLongPtrW(b, GWLP_USERDATA, (LONG_PTR)i);
            SetWindowSubclass(b, IconSubclassProc, 1, (DWORD_PTR)idef.action);
            st->iconBtns[i] = b;
        }

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

    case WM_TIMER:
        if (wParam == DRAG_ANIM_TIMER_ID && st)
        {
            PanelAnimTick(hWnd, st);
            return 0;
        }
        return 0;

    case WM_LBUTTONUP:
        if (st && st->dragging)
        {
            // (патч) --- обновлЄнна€ логика "после отпускани€" ---
            uint16_t newHid = st->hoverHid;
            BindAction act = st->dragAction;
            uint16_t oldHid = GetOldHidForAction(act);

            bool bound = (newHid != 0);

            if (bound)
            {
                // Apply binding immediately
                BindingActions_Apply(act, newHid);
                Profile_SaveIni(AppPaths_BindingsIni().c_str());
                InvalidateHidKey(oldHid);
                InvalidateHidKey(newHid);
                if (st->hKeyboardHost) InvalidateRect(st->hKeyboardHost, nullptr, FALSE);

                // IMPORTANT: instant cleanup Ч NO shrink animation
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
        if (st && st->dragging)
            StopAllPanelAnim_Immediate(hWnd, st);
        return 0;

    case WM_DESTROY:
        KeyboardUI_SetDragHoverHid(0);
        if (st)
        {
            StopAllPanelAnim_Immediate(hWnd, st);

            if (st->hGhost)
            {
                DestroyWindow(st->hGhost);
                st->hGhost = nullptr;
            }
            Ghost_FreeSurface(st);

            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }

        IconCache_Clear();
        return 0;

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
        if (!dis || dis->CtlType != ODT_BUTTON) return FALSE;

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