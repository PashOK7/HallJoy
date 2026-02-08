// keyboard_ui.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <array>
#include <vector>
#include <algorithm>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "keyboard_ui.h"
#include "backend.h"
#include "keyboard_ui_internal.h"
#include "keyboard_ui_state.h"

// NEW: for premium partial invalidation of graph (live marker updates)
#include "keyboard_keysettings_panel.h"

// NEW: gear animation invalidation
#include "keyboard_render.h"

// created in keyboard_page_main.cpp
extern "C" HWND KeyboardPageMain_CreatePage(HWND hParent, HINSTANCE hInst);

// ---------- shared state (defined here) ----------
std::array<HWND, 256> g_btnByHid{};
std::vector<uint16_t> g_hids;
std::vector<HWND> g_keyButtons;

uint16_t g_selectedHid = 0;
uint16_t g_dragHoverHid = 0;

HWND g_hSubTab = nullptr;
HWND g_hPageRemap = nullptr;
HWND g_hPageConfig = nullptr;
HWND g_hPageLayout = nullptr;
HWND g_hPageTester = nullptr;
HWND g_hPageGlobal = nullptr;
int  g_activeSubTab = 0;

// expose selected HID to subpages module
uint16_t KeyboardUI_Internal_GetSelectedHid()
{
    return g_selectedHid;
}

void KeyboardUI_SetDragHoverHid(uint16_t hid)
{
    if (hid >= 256) hid = 0;

    if (hid == g_dragHoverHid) return;

    uint16_t old = g_dragHoverHid;
    g_dragHoverHid = hid;

    if (old < 256 && g_btnByHid[old])
        InvalidateRect(g_btnByHid[old], nullptr, FALSE);
    if (hid < 256 && g_btnByHid[hid])
        InvalidateRect(g_btnByHid[hid], nullptr, FALSE);
}

bool KeyboardUI_HasHid(uint16_t hid)
{
    if (hid == 0) return false;
    if (hid < 256) return g_btnByHid[hid] != nullptr;
    return false;
}

HWND KeyboardUI_CreatePage(HWND hParent, HINSTANCE hInst)
{
    return KeyboardPageMain_CreatePage(hParent, hInst);
}

// ---- helper: iterate set bits in uint64_t efficiently (MSVC-friendly) ----
static void InvalidateDirtyBits(uint64_t bits, int chunk)
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    while (bits)
    {
        unsigned long idx = 0;
        _BitScanForward64(&idx, bits);
        bits &= (bits - 1);

        uint16_t hid = (uint16_t)(chunk * 64 + (int)idx);
        if (hid < 256 && g_btnByHid[hid])
            InvalidateRect(g_btnByHid[hid], nullptr, FALSE);
    }
#else
    for (int b = 0; b < 64; ++b)
    {
        if (bits & (1ULL << b))
        {
            uint16_t hid = (uint16_t)(chunk * 64 + b);
            if (hid < 256 && g_btnByHid[hid])
                InvalidateRect(g_btnByHid[hid], nullptr, FALSE);
        }
    }
#endif
}

// NEW: live marker updates (invalidate ONLY the graph region on Config page)
static void TickConfigLiveMarker()
{
    if (g_activeSubTab != 1) return;      // Configuration tab
    if (!g_hPageConfig) return;

    static uint16_t s_lastHid = 0;
    static uint16_t s_lastRawM = 0xFFFF;
    static uint16_t s_lastOutM = 0xFFFF;

    uint16_t hid = g_selectedHid;
    if (hid >= 256) hid = 0;

    if (hid != s_lastHid)
    {
        s_lastHid = hid;
        s_lastRawM = 0xFFFF;
        s_lastOutM = 0xFFFF;

        RECT gr{};
        if (KeySettingsPanel_GetGraphRect(g_hPageConfig, &gr))
            InvalidateRect(g_hPageConfig, &gr, FALSE);
        else
            InvalidateRect(g_hPageConfig, nullptr, FALSE);

        return;
    }

    if (hid == 0) return;

    uint16_t rawM = BackendUI_GetRawMilli(hid);
    uint16_t outM = BackendUI_GetAnalogMilli(hid);

    if (rawM == s_lastRawM && outM == s_lastOutM)
        return;

    s_lastRawM = rawM;
    s_lastOutM = outM;

    RECT gr{};
    if (KeySettingsPanel_GetGraphRect(g_hPageConfig, &gr))
        InvalidateRect(g_hPageConfig, &gr, FALSE);
    else
        InvalidateRect(g_hPageConfig, nullptr, FALSE);
}

// NEW: gear animation invalidation
static void TickOverrideGearAnim()
{
    uint16_t hids[256]{};
    int n = KeyboardRender_GetAnimatingHids(hids, 256);
    for (int i = 0; i < n; ++i)
    {
        uint16_t hid = hids[i];
        if (hid < 256 && g_btnByHid[hid])
            InvalidateRect(g_btnByHid[hid], nullptr, FALSE);
    }
}

void KeyboardUI_OnTimerTick(HWND)
{
    HWND root = nullptr;
    if (g_hPageRemap) root = GetAncestor(g_hPageRemap, GA_ROOT);
    if (!root || !IsWindowVisible(root) || IsIconic(root))
        return;

    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = BackendUI_ConsumeDirtyChunk(chunk);
        if (!bits) continue;

        InvalidateDirtyBits(bits, chunk);
    }

    // NEW: gear anim redraw for keys with override toggle animation
    TickOverrideGearAnim();

    // NEW: live marker (graph only)
    TickConfigLiveMarker();

    if (g_activeSubTab == 2 && g_hPageTester)
    {
        static uint32_t s_lastTesterHash = 0;

        int pads = std::clamp(Backend_GetVirtualGamepadCount(), 1, 4);
        uint32_t h = 2166136261u ^ (uint32_t)pads;
        for (int i = 0; i < pads; ++i)
        {
            XUSB_REPORT r = Backend_GetLastReportForPad(i);
            auto mix = [&](uint32_t v) {
                h ^= v;
                h *= 16777619u;
                };
            mix((uint32_t)(uint16_t)r.wButtons);
            mix((uint32_t)(uint16_t)r.sThumbLX);
            mix((uint32_t)(uint16_t)r.sThumbLY);
            mix((uint32_t)(uint16_t)r.sThumbRX);
            mix((uint32_t)(uint16_t)r.sThumbRY);
            mix((uint32_t)r.bLeftTrigger | ((uint32_t)r.bRightTrigger << 8));
        }

        if (h != s_lastTesterHash)
        {
            s_lastTesterHash = h;
            InvalidateRect(g_hPageTester, nullptr, FALSE);
        }
    }
}
