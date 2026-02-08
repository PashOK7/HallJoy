// keyboard_render.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <array>
#include <vector>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#pragma comment(lib, "Msimg32.lib")

#include "keyboard_render.h"
#include "backend.h"
#include "settings.h"
#include "ui_theme.h"

#include "binding_actions.h"
#include "bindings.h"
#include "remap_icons.h"
#include "win_util.h"
#include "key_settings.h"

using namespace Gdiplus;

static constexpr COLORREF KEY_INNER_BG = RGB(28, 28, 28);

// ---------------- anim helpers ----------------
static inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }
static inline float EaseOutQuad(float t) { t = Clamp01(t); return t * (2.0f - t); }
static inline float EaseOutCubic(float t)
{
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

static inline COLORREF LerpColor(COLORREF a, COLORREF b, float t)
{
    t = Clamp01(t);
    int ar = (int)GetRValue(a), ag = (int)GetGValue(a), ab = (int)GetBValue(a);
    int br = (int)GetRValue(b), bg = (int)GetGValue(b), bb = (int)GetBValue(b);
    int rr = (int)lroundf((float)ar + ((float)br - (float)ar) * t);
    int rg = (int)lroundf((float)ag + ((float)bg - (float)ag) * t);
    int rb = (int)lroundf((float)ab + ((float)bb - (float)ab) * t);
    rr = std::clamp(rr, 0, 255);
    rg = std::clamp(rg, 0, 255);
    rb = std::clamp(rb, 0, 255);
    return RGB(rr, rg, rb);
}

// ---------------- selection glow anim (UI thread only) ----------------
enum SelAnimMode : uint8_t { SEL_NONE = 0, SEL_IN = 1, SEL_OUT = 2 };

static std::array<uint8_t, 256> g_selLast{};
static std::array<uint8_t, 256> g_selMode{};
static std::array<DWORD, 256>   g_selStartTick{};

static void SelAnim_Notify(uint16_t hid, bool selected, DWORD now)
{
    if (hid == 0 || hid >= 256) return;

    uint8_t prev = g_selLast[hid];
    uint8_t cur = selected ? 1u : 0u;

    if (cur && !prev)
    {
        g_selMode[hid] = SEL_IN;
        g_selStartTick[hid] = now;
    }
    else if (!cur && prev)
    {
        g_selMode[hid] = SEL_OUT;
        g_selStartTick[hid] = now;
    }

    g_selLast[hid] = cur;
}

static float SelAnim_GetT(uint16_t hid, bool selected, DWORD now)
{
    if (hid == 0 || hid >= 256) return selected ? 1.0f : 0.0f;

    constexpr DWORD IN_MS = 80;
    constexpr DWORD OUT_MS = 170;

    uint8_t mode = g_selMode[hid];

    if (selected && mode == SEL_NONE) return 1.0f;
    if (!selected && mode == SEL_NONE) return 0.0f;

    DWORD dt = now - g_selStartTick[hid];

    if (mode == SEL_IN)
    {
        float t = EaseOutCubic((float)dt / (float)IN_MS);
        if (dt >= IN_MS) g_selMode[hid] = SEL_NONE;
        return Clamp01(t);
    }
    if (mode == SEL_OUT)
    {
        float t = 1.0f - EaseOutCubic((float)dt / (float)OUT_MS);
        if (dt >= OUT_MS) g_selMode[hid] = SEL_NONE;
        return Clamp01(t);
    }

    return selected ? 1.0f : 0.0f;
}

// ---------------- gear anim (UI thread only) ----------------
enum GearAnimMode : uint8_t { GEAR_NONE = 0, GEAR_APPEAR = 1, GEAR_DISAPPEAR = 2 };

static std::array<uint8_t, 256>  g_lastOverride{};
static std::array<uint8_t, 256>  g_gearMode{};
static std::array<DWORD, 256>    g_gearStartTick{};

// NEW: current selected HID (from UI) so renderer can run "gear wow spin" only while editing that key
static uint16_t g_renderSelectedHid = 0;

static void GearAnim_NotifyOverrideState(uint16_t hid, bool overrideOn, DWORD now)
{
    if (hid == 0 || hid >= 256) return;

    uint8_t prev = g_lastOverride[hid];
    uint8_t cur = overrideOn ? 1u : 0u;

    if (cur && !prev)
    {
        g_gearMode[hid] = GEAR_APPEAR;
        g_gearStartTick[hid] = now;
    }
    else if (!cur && prev)
    {
        g_gearMode[hid] = GEAR_DISAPPEAR;
        g_gearStartTick[hid] = now;
    }

    g_lastOverride[hid] = cur;
}

// ---------------- IMPACT FLASH ANIM (NEW) ----------------
static std::array<bool, 256>  g_impactWasFull{}; // was value >= 0.999f last frame?
static std::array<DWORD, 256> g_impactStartTick{};
static std::array<bool, 256>  g_impactActive{};

static void ImpactAnim_NotifyValue(uint16_t hid, float v01, DWORD now)
{
    if (hid == 0 || hid >= 256) return;

    bool isFull = (v01 >= 0.999f);
    bool wasFull = g_impactWasFull[hid];

    if (isFull && !wasFull)
    {
        // Trigger flash
        g_impactActive[hid] = true;
        g_impactStartTick[hid] = now;
    }

    g_impactWasFull[hid] = isFull;
}

static float ImpactAnim_GetAlpha(uint16_t hid, DWORD now)
{
    if (hid == 0 || hid >= 256) return 0.0f;
    if (!g_impactActive[hid]) return 0.0f;

    constexpr DWORD FLASH_MS = 250;
    DWORD dt = now - g_impactStartTick[hid];

    if (dt >= FLASH_MS)
    {
        g_impactActive[hid] = false;
        return 0.0f;
    }

    float t = (float)dt / (float)FLASH_MS;
    // Fast attack, slow decay
    // t=0 -> alpha=1.0, t=1 -> alpha=0.0
    return 1.0f - EaseOutQuad(t);
}

// ---------------- GEAR WOW SPIN (NEW) ----------------
enum GearSpinPhase : uint8_t
{
    GSPIN_NONE = 0,
    GSPIN_ACCEL,
    GSPIN_DECEL,
    GSPIN_IDLE,
    GSPIN_STOP
};

struct GearSpinState
{
    uint16_t hid = 0;
    GearSpinPhase phase = GSPIN_NONE;

    DWORD lastTick = 0;
    DWORD phaseStartTick = 0;

    float angle = 0.0f;       // radians
    float vel = 0.0f;         // rad/s

    float stopFromVel = 0.0f; // rad/s at stop start
};

static GearSpinState g_gspin;

static constexpr float GEARSPIN_BURST_VEL = 16.0f; // rad/s (softer burst)
static constexpr float GEARSPIN_IDLE_VEL = 1.15f; // rad/s (slow idle while editing)
static constexpr DWORD GEARSPIN_ACCEL_MS = 80;    // quicker ramp-up
static constexpr DWORD GEARSPIN_DECEL_MS = 420;   // shorter decel to idle
static constexpr DWORD GEARSPIN_STOP_MS = 320;   // a bit quicker stop

static float WrapAngleRad(float a)
{
    constexpr float TWO_PI = 6.28318530718f;
    if (!std::isfinite(a)) return 0.0f;
    a = fmodf(a, TWO_PI);
    if (a < 0.0f) a += TWO_PI;
    return a;
}

static float PhaseT01(DWORD now, DWORD start, DWORD durMs)
{
    if (durMs == 0) return 1.0f;
    DWORD dt = now - start;
    float t = (float)dt / (float)durMs;
    return Clamp01(t);
}

static void GearSpin_Clear()
{
    g_gspin = GearSpinState{};
}

static void GearSpin_Start(uint16_t hid, DWORD now)
{
    if (!hid || hid >= 256) return;

    if (g_gspin.hid != hid)
    {
        // new key: reset angle for a clean premium burst
        g_gspin.angle = 0.0f;
    }

    g_gspin.hid = hid;
    g_gspin.phase = GSPIN_ACCEL;
    g_gspin.phaseStartTick = now;
    g_gspin.lastTick = now;
    g_gspin.vel = 0.0f;
    g_gspin.stopFromVel = 0.0f;
}

static void GearSpin_BeginStop(DWORD now)
{
    if (g_gspin.phase == GSPIN_NONE) return;
    if (g_gspin.phase == GSPIN_STOP) return;

    g_gspin.phase = GSPIN_STOP;
    g_gspin.phaseStartTick = now;
    g_gspin.lastTick = now;
    g_gspin.stopFromVel = g_gspin.vel;
}

static void GearSpin_Tick(DWORD now)
{
    if (g_gspin.phase == GSPIN_NONE || g_gspin.hid == 0) return;

    DWORD dtMs = (g_gspin.lastTick == 0) ? 0 : (now - g_gspin.lastTick);
    g_gspin.lastTick = now;

    float dt = (float)dtMs / 1000.0f;
    dt = std::clamp(dt, 0.0f, 0.050f);

    switch (g_gspin.phase)
    {
    case GSPIN_ACCEL:
    {
        float t = PhaseT01(now, g_gspin.phaseStartTick, GEARSPIN_ACCEL_MS);
        float e = EaseOutCubic(t);
        g_gspin.vel = GEARSPIN_BURST_VEL * e;

        if (t >= 1.0f - 1e-4f)
        {
            g_gspin.phase = GSPIN_DECEL;
            g_gspin.phaseStartTick = now;
        }
        break;
    }
    case GSPIN_DECEL:
    {
        float t = PhaseT01(now, g_gspin.phaseStartTick, GEARSPIN_DECEL_MS);
        float e = EaseOutCubic(t);
        g_gspin.vel = GEARSPIN_BURST_VEL + (GEARSPIN_IDLE_VEL - GEARSPIN_BURST_VEL) * e;

        if (t >= 1.0f - 1e-4f)
        {
            g_gspin.phase = GSPIN_IDLE;
            g_gspin.phaseStartTick = now;
            g_gspin.vel = GEARSPIN_IDLE_VEL;
        }
        break;
    }
    case GSPIN_IDLE:
        g_gspin.vel = GEARSPIN_IDLE_VEL;
        break;

    case GSPIN_STOP:
    {
        float t = PhaseT01(now, g_gspin.phaseStartTick, GEARSPIN_STOP_MS);
        float e = EaseOutCubic(t);
        g_gspin.vel = g_gspin.stopFromVel * (1.0f - e);

        if (t >= 1.0f - 1e-4f)
        {
            GearSpin_Clear();
            return;
        }
        break;
    }
    default: break;
    }

    g_gspin.angle = WrapAngleRad(g_gspin.angle + g_gspin.vel * dt);
}

static float GearSpin_GetAngle(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0.0f;
    if (g_gspin.hid != hid) return 0.0f;
    if (g_gspin.phase == GSPIN_NONE) return 0.0f;
    return g_gspin.angle;
}

// ---------------- digital (Windows keydown) state ----------------
static std::array<uint8_t, 256> g_digLast{};

static int VkFromHid(uint16_t hid)
{
    if (hid >= 4 && hid <= 29) return 'A' + (int)hid - 4;
    if (hid >= 30 && hid <= 38) return '1' + (int)hid - 30;
    if (hid == 39) return '0';

    switch (hid)
    {
    case 41: return VK_ESCAPE;
    case 42: return VK_BACK;
    case 43: return VK_TAB;
    case 44: return VK_SPACE;
    case 40: return VK_RETURN;

    case 53: return VK_OEM_3;
    case 45: return VK_OEM_MINUS;
    case 46: return VK_OEM_PLUS;
    case 47: return VK_OEM_4;
    case 48: return VK_OEM_6;
    case 49: return VK_OEM_5;
    case 51: return VK_OEM_1;
    case 52: return VK_OEM_7;
    case 54: return VK_OEM_COMMA;
    case 55: return VK_OEM_PERIOD;
    case 56: return VK_OEM_2;

    case 58: return VK_F1;
    case 59: return VK_F2;
    case 60: return VK_F3;
    case 61: return VK_F4;
    case 62: return VK_F5;
    case 63: return VK_F6;
    case 64: return VK_F7;
    case 65: return VK_F8;
    case 66: return VK_F9;
    case 67: return VK_F10;
    case 68: return VK_F11;
    case 69: return VK_F12;

    case 74: return VK_HOME;
    case 75: return VK_PRIOR;
    case 76: return VK_DELETE;
    case 77: return VK_END;
    case 78: return VK_NEXT;

    case 80: return VK_LEFT;
    case 79: return VK_RIGHT;
    case 81: return VK_DOWN;
    case 82: return VK_UP;

    case 224: return VK_LCONTROL;
    case 225: return VK_LSHIFT;
    case 226: return VK_LMENU;
    case 227: return VK_LWIN;
    case 229: return VK_RSHIFT;
    case 230: return VK_RMENU;

    case 57: return VK_CAPITAL;

    default: return 0;
    }
}

static bool IsDigitalDownByHid(uint16_t hid)
{
    int vk = VkFromHid(hid);
    if (!vk) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// ------------------------------------------------

int KeyboardRender_GetAnimatingHids(uint16_t* outHids, int cap)
{
    if (!outHids || cap <= 0) return 0;

    DWORD now = GetTickCount();

    // --- Gear wow spin: keep spinning only while the key remains selected and Override is enabled ---
    if (g_gspin.phase != GSPIN_NONE && g_gspin.hid != 0)
    {
        bool keep =
            (g_renderSelectedHid == g_gspin.hid) &&
            KeySettings_GetUseUnique(g_gspin.hid);

        if (!keep)
            GearSpin_BeginStop(now);
    }

    GearSpin_Tick(now);

    constexpr DWORD APPEAR_MS = 180;
    constexpr DWORD SPIN_MS = 1000;
    constexpr DWORD DISAPPEAR_MS = 160;

    constexpr DWORD SEL_IN_MS = 80;
    constexpr DWORD SEL_OUT_MS = 170;

    int n = 0;

    auto pushUnique = [&](uint16_t hid)
        {
            for (int i = 0; i < n; ++i) if (outHids[i] == hid) return;
            if (n < cap) outHids[n++] = hid;
        };

    for (uint16_t hid = 1; hid < 256; ++hid)
    {
        // digital state changes
        {
            bool down = IsDigitalDownByHid(hid);
            uint8_t cur = down ? 1u : 0u;
            if (cur != g_digLast[hid])
            {
                g_digLast[hid] = cur;
                pushUnique(hid);
            }
        }

        // gear state (fast-path)
        bool overrideOn = KeySettings_GetUseUnique(hid);
        GearAnim_NotifyOverrideState(hid, overrideOn, now);

        // gear anim check
        {
            uint8_t mode = g_gearMode[hid];
            if (mode != GEAR_NONE)
            {
                DWORD dt = now - g_gearStartTick[hid];
                if (mode == GEAR_APPEAR)
                {
                    DWORD end = (APPEAR_MS > SPIN_MS) ? APPEAR_MS : SPIN_MS;
                    if (dt >= end) g_gearMode[hid] = GEAR_NONE;
                    else pushUnique(hid);
                }
                else if (mode == GEAR_DISAPPEAR)
                {
                    if (dt >= DISAPPEAR_MS) g_gearMode[hid] = GEAR_NONE;
                    else pushUnique(hid);
                }
            }
        }

        // selection anim check
        {
            uint8_t mode = g_selMode[hid];
            if (mode != SEL_NONE)
            {
                DWORD dt = now - g_selStartTick[hid];
                DWORD end = (mode == SEL_IN) ? SEL_IN_MS : SEL_OUT_MS;
                if (dt >= end) g_selMode[hid] = SEL_NONE;
                else pushUnique(hid);
            }
        }

        // impact flash check
        if (g_impactActive[hid])
        {
            // check if finished
            if (ImpactAnim_GetAlpha(hid, now) > 0.001f)
                pushUnique(hid);
        }
    }

    // Ensure wow-spin key is redrawn every tick while active (including idle + stop)
    if (g_gspin.phase != GSPIN_NONE && g_gspin.hid != 0)
        pushUnique(g_gspin.hid);

    return n;
}

// ------------------------------------------------

float KeyboardRender_ReadAnalog01(uint16_t hid)
{
    if (hid == 0) return 0.0f;
    if (hid < 256)
        return (float)BackendUI_GetAnalogMilli(hid) / 1000.0f;
    return 0.0f;
}

static Color GpColorFromColorRef(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void DrawKeyLabelTextAA(HDC hdc, const RECT& rc, const wchar_t* text, COLORREF color)
{
    if (!text || !*text) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    LOGFONTW lf{};
    if (!hFont || GetObjectW(hFont, sizeof(lf), &lf) == 0)
    {
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        lf.lfHeight = -12;
        lf.lfWeight = FW_NORMAL;
    }

    Font font(hdc, &lf);
    SolidBrush br(GpColorFromColorRef(color, 255));

    RectF r((float)rc.left, (float)rc.top,
        (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingNone);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    g.DrawString(text, -1, &font, r, &fmt, &br);
}

static int FindIconIndexByAction(BindAction a)
{
    int n = RemapIcons_Count();
    for (int i = 0; i < n; ++i)
        if (RemapIcons_Get(i).action == a) return i;
    return -1;
}

static int GetStyleVariantForPad(int padIndex, int totalPads)
{
    totalPads = std::clamp(totalPads, 1, 4);
    padIndex = std::clamp(padIndex, 0, 3);
    if (totalPads <= 1) return 0;
    return std::clamp(Bindings_GetPadStyleVariant(padIndex), 1, 4);
}

struct BoundIconEntry
{
    int iconIdx = -1;
    int styleVariant = 0;
};

struct SuppressedBindingState
{
    bool enabled = false;
    uint16_t hid = 0;
    int padIndex = 0;
    BindAction action{};
};

static SuppressedBindingState g_suppressedBinding;

void KeyboardRender_SetSuppressedBinding(uint16_t hid, int padIndex, BindAction action)
{
    g_suppressedBinding.enabled = (hid != 0);
    g_suppressedBinding.hid = hid;
    g_suppressedBinding.padIndex = std::clamp(padIndex, 0, 3);
    g_suppressedBinding.action = action;
}

void KeyboardRender_ClearSuppressedBinding()
{
    g_suppressedBinding = SuppressedBindingState{};
}

static int CollectDisplayedIconsByHid(uint16_t hid, BoundIconEntry out[4])
{
    if (out)
    {
        for (int i = 0; i < 4; ++i) out[i] = BoundIconEntry{};
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
            if (g_suppressedBinding.enabled &&
                g_suppressedBinding.hid == hid &&
                g_suppressedBinding.padIndex == pad &&
                g_suppressedBinding.action == act)
            {
                continue;
            }

            int iconIdx = FindIconIndexByAction(act);
            if (iconIdx < 0) continue;

            if (out)
            {
                out[count].iconIdx = iconIdx;
                out[count].styleVariant = GetStyleVariantForPad(pad, pads);
            }
            ++count;
        }
    }
    return count;
}

// ---------------- Bound icon cache ----------------
struct CachedGlyph
{
    int size = 0;
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    void* bits = nullptr;
};

static std::unordered_map<uint64_t, CachedGlyph> g_glyphCache;

static uint64_t MakeGlyphKey(int iconIdx, int size, int styleVariant)
{
    styleVariant = std::clamp(styleVariant, 0, 15);
    return (uint64_t)(uint32_t)iconIdx |
        ((uint64_t)(uint32_t)styleVariant << 16) |
        ((uint64_t)(uint32_t)size << 32);
}

static void Glyph_Free(CachedGlyph& g)
{
    if (g.dc)
    {
        if (g.oldBmp) SelectObject(g.dc, g.oldBmp);
        g.oldBmp = nullptr;
    }
    if (g.bmp)
    {
        DeleteObject(g.bmp);
        g.bmp = nullptr;
    }
    if (g.dc)
    {
        DeleteDC(g.dc);
        g.dc = nullptr;
    }
    g.bits = nullptr;
    g.size = 0;
}

static CachedGlyph* Glyph_GetOrCreate(int iconIdx, int size, int styleVariant)
{
    if (iconIdx < 0 || size <= 0) return nullptr;

    uint64_t key = MakeGlyphKey(iconIdx, size, styleVariant);
    auto it = g_glyphCache.find(key);
    if (it != g_glyphCache.end())
        return &it->second;

    CachedGlyph cg;
    cg.size = size;

    HDC screen = GetDC(nullptr);
    cg.dc = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    cg.bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &cg.bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (!cg.dc || !cg.bmp || !cg.bits)
    {
        Glyph_Free(cg);
        return nullptr;
    }

    cg.oldBmp = SelectObject(cg.dc, cg.bmp);

    std::memset(cg.bits, 0, (size_t)size * (size_t)size * 4);

    RECT rc{ 0, 0, size, size };
    RemapIcons_DrawGlyphAA(cg.dc, rc, iconIdx, false, 0.075f, styleVariant);

    auto [insIt, ok] = g_glyphCache.emplace(key, cg);
    if (!ok)
    {
        Glyph_Free(cg);
        return nullptr;
    }

    return &insIt->second;
}

static bool DrawBoundIconOnKey_Cached(HWND hwndForDpi, HDC hdc, const RECT& keyRectForIcon, uint16_t hid)
{
    BoundIconEntry entries[4]{};
    int iconCount = CollectDisplayedIconsByHid(hid, entries);
    if (iconCount <= 0)
        return false;

    int iw = keyRectForIcon.right - keyRectForIcon.left;
    int ih = keyRectForIcon.bottom - keyRectForIcon.top;
    if (iw <= 10 || ih <= 10)
        return true;

    int want = (int)Settings_GetBoundKeyIconSizePx();
    int baseSize = WinUtil_ScalePx(hwndForDpi, want);
    baseSize = std::clamp(baseSize, 8, std::min(iw, ih));

    auto drawIconAt = [&](const BoundIconEntry& e, int x, int y, int size)
    {
        size = std::clamp(size, 8, std::min(iw, ih));
        CachedGlyph* cg = Glyph_GetOrCreate(e.iconIdx, size, e.styleVariant);
        if (!cg || !cg->dc || !cg->bmp)
        {
            RECT rcIcon{ x, y, x + size, y + size };
            RemapIcons_DrawGlyphAA(hdc, rcIcon, e.iconIdx, false, 0.075f, e.styleVariant);
            return;
        }

        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(hdc, x, y, size, size, cg->dc, 0, 0, size, size, bf);
    };

    int gap = std::clamp(WinUtil_ScalePx(hwndForDpi, 2), 1, 5);

    if (iconCount == 1)
    {
        int size = std::clamp(baseSize, 8, std::min(iw, ih));
        int x = (keyRectForIcon.left + keyRectForIcon.right - size) / 2;
        int y = (keyRectForIcon.top + keyRectForIcon.bottom - size) / 2;
        drawIconAt(entries[0], x, y, size);
        return true;
    }

    if (iconCount == 2)
    {
        int cellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(cellW, ih));
        int y = keyRectForIcon.top + (ih - size) / 2;
        int x0 = keyRectForIcon.left + (cellW - size) / 2;
        int x1 = keyRectForIcon.left + cellW + gap + (cellW - size) / 2;
        drawIconAt(entries[0], x0, y, size);
        drawIconAt(entries[1], x1, y, size);
        return true;
    }

    if (iconCount == 3)
    {
        int rowH = std::max(8, (ih - gap) / 2);
        int topCellW = std::max(8, (iw - gap) / 2);
        int size = std::clamp(baseSize, 8, std::min(topCellW, rowH));

        int topY = keyRectForIcon.top + (rowH - size) / 2;
        int x0 = keyRectForIcon.left + (topCellW - size) / 2;
        int x1 = keyRectForIcon.left + topCellW + gap + (topCellW - size) / 2;
        drawIconAt(entries[0], x0, topY, size);
        drawIconAt(entries[1], x1, topY, size);

        int botY = keyRectForIcon.top + rowH + gap + (rowH - size) / 2;
        int x2 = keyRectForIcon.left + (iw - size) / 2;
        drawIconAt(entries[2], x2, botY, size);
        return true;
    }

    // 4+ icons: compact 2x2 grid
    int cellW = std::max(8, (iw - gap) / 2);
    int cellH = std::max(8, (ih - gap) / 2);
    int size = std::clamp(baseSize, 8, std::min(cellW, cellH));
    for (int i = 0; i < 4 && i < iconCount; ++i)
    {
        int col = i % 2;
        int row = i / 2;
        int cellX = keyRectForIcon.left + col * (cellW + gap);
        int cellY = keyRectForIcon.top + row * (cellH + gap);
        int x = cellX + (cellW - size) / 2;
        int y = cellY + (cellH - size) / 2;
        drawIconAt(entries[i], x, y, size);
    }
    return true;
}

// ---------------- brushes ----------------
static HBRUSH BrushKeyBg()
{
    return UiTheme::Brush_ControlBg();
}

static HBRUSH BrushKeyInnerBg()
{
    static HBRUSH b = CreateSolidBrush(KEY_INNER_BG);
    return b;
}

static HBRUSH BrushFill()
{
    static HBRUSH b = CreateSolidBrush(UiTheme::Color_Accent());
    return b;
}

static void DrawOverrideGearMarkerAA(HWND hwndForDpi, HDC hdc, const RECT& innerKeyRect, bool selected, uint16_t hid)
{
    constexpr int   sizePx96 = 11;
    constexpr int   insetPx96 = 0;

    constexpr int   teeth = 6;
    constexpr float toothWidth01 = 0.5f;
    constexpr float innerRatio = 0.6f;
    constexpr float holeRatio = 0.23f;
    constexpr float outlineW = 1.2f;

    constexpr DWORD APPEAR_MS = 180;
    constexpr DWORD SPIN_MS = 1000;
    constexpr DWORD DISAPPEAR_MS = 160;

    int d = WinUtil_ScalePx(hwndForDpi, sizePx96);
    d = std::clamp(d, 7, 24);

    int pad = WinUtil_ScalePx(hwndForDpi, insetPx96);
    pad = std::clamp(pad, 0, 24);

    DWORD now = GetTickCount();

    float scale = 1.0f;
    float ang = 0.0f;

    if (hid < 256)
    {
        uint8_t mode = g_gearMode[hid];
        if (mode != GEAR_NONE)
        {
            DWORD dt = now - g_gearStartTick[hid];

            if (mode == GEAR_APPEAR)
            {
                scale = EaseOutCubic((float)dt / (float)APPEAR_MS);

                float tR = EaseOutCubic((float)dt / (float)SPIN_MS);
                constexpr float totalAngle = 4.0f * 3.14159265f;
                ang = totalAngle * tR;
            }
            else if (mode == GEAR_DISAPPEAR)
            {
                scale = 1.0f - EaseOutCubic((float)dt / (float)DISAPPEAR_MS);
            }
        }
    }

    // NEW: user-triggered "wow spin" (fast burst -> slow idle) while editing key
    ang += GearSpin_GetAngle(hid);

    if (scale <= 0.001f) return;

    int x = innerKeyRect.right - pad - d;
    int y = innerKeyRect.top + pad;

    if (x < innerKeyRect.left + 1) x = innerKeyRect.left + 1;
    if (y < innerKeyRect.top + 1) y = innerKeyRect.top + 1;

    RectF r((float)x, (float)y, (float)d, (float)d);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    COLORREF fillC = selected ? RGB(255, 190, 40) : RGB(255, 170, 0);
    COLORREF outlineC = RGB(15, 15, 15);

    Color fill = GpColorFromColorRef(fillC, 255);
    Color outline = GpColorFromColorRef(outlineC, 220);

    const float cx = r.X + r.Width * 0.5f;
    const float cy = r.Y + r.Height * 0.5f;

    const float tw01 = std::clamp(toothWidth01, 0.10f, 0.90f);
    const float period = 3.14159265f * 2.0f / (float)teeth;
    const float toothW = period * tw01;
    const float gapW = (period - toothW) * 0.5f;

    const float inR = std::clamp(innerRatio, 0.25f, 0.95f);

    const float R_tooth = r.Width * 0.50f * scale;
    const float R_root = R_tooth * inR;
    const float R_hole = R_tooth * std::clamp(holeRatio, 0.10f, 0.45f);

    std::array<PointF, teeth * 3> pts{};
    int n = 0;

    auto add = [&](float a, float rr)
        {
            pts[(size_t)n++] = PointF(cx + rr * cosf(a + ang), cy + rr * sinf(a + ang));
        };

    for (int i = 0; i < teeth; ++i)
    {
        float a0 = (float)i * period;
        add(a0, R_root);
        add(a0 + gapW, R_tooth);
        add(a0 + gapW + toothW, R_tooth);
    }

    GraphicsPath gear;
    if (n >= 3)
        gear.AddPolygon(pts.data(), n);

    SolidBrush br(fill);
    Pen pen(outline, outlineW);
    pen.SetLineJoin(LineJoinRound);

    g.FillPath(&br, &gear);
    g.DrawPath(&pen, &gear);

    SolidBrush hole(GpColorFromColorRef(KEY_INNER_BG, 255));
    g.FillEllipse(&hole, cx - R_hole, cy - R_hole, R_hole * 2.0f, R_hole * 2.0f);
    g.DrawEllipse(&pen, cx - R_hole, cy - R_hole, R_hole * 2.0f, R_hole * 2.0f);
}

static void DrawSelectionGlowAA(HDC hdc, const RECT& rc, float t)
{
    t = Clamp01(t);
    if (t <= 0.001f) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    const BYTE a0 = (BYTE)std::clamp((int)lroundf(38.0f * t), 0, 255);
    const BYTE a1 = (BYTE)std::clamp((int)lroundf(70.0f * t), 0, 255);
    const BYTE a2 = (BYTE)std::clamp((int)lroundf(120.0f * t), 0, 255);

    Color c0(a0, 255, 170, 90);
    Color c1(a1, 255, 170, 90);
    Color c2(a2, 255, 170, 90);

    RectF r((float)rc.left + 0.5f, (float)rc.top + 0.5f,
        (float)(rc.right - rc.left - 1),
        (float)(rc.bottom - rc.top - 1));

    Pen p0(c0, 7.0f); p0.SetLineJoin(LineJoinRound);
    Pen p1(c1, 4.0f); p1.SetLineJoin(LineJoinRound);
    Pen p2(c2, 2.0f); p2.SetLineJoin(LineJoinRound);

    g.DrawRectangle(&p0, r);
    g.DrawRectangle(&p1, r);
    g.DrawRectangle(&p2, r);
}

static void DrawDigitalIndicatorAA(HWND hwndForDpi, HDC hdc, const RECT& innerKeyRect)
{
    int d = WinUtil_ScalePx(hwndForDpi, 7);
    d = std::clamp(d, 5, 12);

    int pad = WinUtil_ScalePx(hwndForDpi, 3);
    pad = std::clamp(pad, 1, 8);

    int x = innerKeyRect.left + pad;
    int y = innerKeyRect.bottom - pad - d;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    Color fill(230, 120, 210, 140);
    Color outline(220, 10, 10, 10);

    SolidBrush br(fill);
    Pen pen(outline, 1.2f);

    g.FillEllipse(&br, (REAL)x, (REAL)y, (REAL)d, (REAL)d);
    g.DrawEllipse(&pen, (REAL)x, (REAL)y, (REAL)d, (REAL)d);
}

static void DrawKey_Impl(const DRAWITEMSTRUCT* dis, uint16_t hid, bool selected, float v01)
{
    HWND hBtn = dis->hwndItem;
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    uint16_t actualHid = (uint16_t)GetWindowLongPtrW(hBtn, GWLP_USERDATA);

    const bool isRealKeyForAnalogAndIcon = (hid != 0);
    const bool isRealKeyActual = (actualHid != 0);

    if (v01 < 0.0f)
        v01 = KeyboardRender_ReadAnalog01(hid);

    v01 = std::clamp(v01, 0.0f, 1.0f);

    DWORD now = GetTickCount();

    // 1. Selection anim
    float selT = 0.0f;
    if (isRealKeyActual && actualHid < 256)
    {
        SelAnim_Notify(actualHid, selected, now);
        selT = SelAnim_GetT(actualHid, selected, now);
    }

    // 2. Impact flash anim
    float flashAlpha = 0.0f;
    if (isRealKeyForAnalogAndIcon && actualHid < 256)
    {
        ImpactAnim_NotifyValue(actualHid, v01, now);
        flashAlpha = ImpactAnim_GetAlpha(actualHid, now);
    }

    FillRect(hdc, &rc, UiTheme::Brush_ControlBg());

    if (selT > 0.0f)
        DrawSelectionGlowAA(hdc, rc, selT);

    {
        COLORREF base = UiTheme::Color_Border();
        COLORREF selC = RGB(255, 170, 90);
        COLORREF c = (selT > 0.0f) ? LerpColor(base, selC, selT) : base;

        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        SetDCPenColor(hdc, c);
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
    }

    RECT inner = rc;
    InflateRect(&inner, -3, -3);

    {
        static HBRUSH b = CreateSolidBrush(KEY_INNER_BG);
        FillRect(hdc, &inner, b);
    }

    if (isRealKeyForAnalogAndIcon && v01 > 0.0f)
    {
        static HBRUSH bFill = CreateSolidBrush(UiTheme::Color_Accent());

        int hh = inner.bottom - inner.top;
        int fh = (int)std::lround(hh * v01);
        if (fh > 0)
        {
            RECT fill = inner;
            fill.bottom = inner.top + fh;
            FillRect(hdc, &fill, bFill);

            // IMPACT FLASH overlay (white gradient at the top of the fill)
            if (flashAlpha > 0.01f)
            {
                Graphics g(hdc);
                g.SetCompositingQuality(CompositingQualityHighQuality);

                int a = (int)(flashAlpha * 200.0f); // max 200/255 alpha
                Color cTop(a, 255, 255, 255);
                Color cBot(0, 255, 255, 255);

                RectF rFill((REAL)fill.left, (REAL)fill.top, (REAL)(fill.right - fill.left), (REAL)(fill.bottom - fill.top));
                LinearGradientBrush br(rFill, cTop, cBot, LinearGradientModeVertical);
                g.FillRectangle(&br, rFill);
            }
        }
    }

    RECT iconArea = rc;
    InflateRect(&iconArea, -1, -1);

    bool bound = false;
    if (isRealKeyForAnalogAndIcon)
        bound = DrawBoundIconOnKey_Cached(hBtn, hdc, iconArea, hid);

    if (!bound)
    {
        wchar_t text[64]{};
        GetWindowTextW(hBtn, text, 63);

        COLORREF c = disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text();
        DrawKeyLabelTextAA(hdc, inner, text, c);
    }

    if (isRealKeyForAnalogAndIcon && isRealKeyActual && actualHid < 256)
    {
        if (IsDigitalDownByHid(actualHid))
            DrawDigitalIndicatorAA(hBtn, hdc, inner);
    }

    if (isRealKeyActual && actualHid < 256)
    {
        bool overrideOn = KeySettings_GetUseUnique(actualHid);
        GearAnim_NotifyOverrideState(actualHid, overrideOn, now);

        if (overrideOn || g_gearMode[actualHid] == GEAR_DISAPPEAR || g_gearMode[actualHid] == GEAR_APPEAR)
            DrawOverrideGearMarkerAA(hBtn, hdc, inner, selected, actualHid);
    }
}

void KeyboardRender_DrawKey(const DRAWITEMSTRUCT* dis, uint16_t hid, bool selected, float v01)
{
    if (!dis) return;

    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1)
        return;

    HDC outDC = dis->hDC;
    HDC memDC = CreateCompatibleDC(outDC);
    if (!memDC)
    {
        DrawKey_Impl(dis, hid, selected, v01);
        return;
    }

    HBITMAP bmp = CreateCompatibleBitmap(outDC, w, h);
    if (!bmp)
    {
        DeleteDC(memDC);
        DrawKey_Impl(dis, hid, selected, v01);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, bmp);

    DRAWITEMSTRUCT di = *dis;
    di.hDC = memDC;
    di.rcItem = RECT{ 0,0,w,h };

    DrawKey_Impl(&di, hid, selected, v01);

    BitBlt(outDC, rc.left, rc.top, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

void KeyboardRender_NotifySelectedHid(uint16_t hid)
{
    if (hid >= 256) hid = 0;

    g_renderSelectedHid = hid;

    // If user leaves the spinning key, begin smooth stop
    if (g_gspin.phase != GSPIN_NONE && g_gspin.hid != 0 && g_gspin.hid != hid)
    {
        GearSpin_BeginStop(GetTickCount());
    }
}

void KeyboardRender_OnGearClicked(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return;

    // Only meaningful if Override is enabled (gear visible as "override marker")
    if (!KeySettings_GetUseUnique(hid)) return;

    // Do NOT hard-require g_renderSelectedHid==hid here.
    // UI tries to select the key before calling this, but in some edge cases
    // selection sync may lag by one message/tick. We still want the burst to start.
    GearSpin_Start(hid, GetTickCount());
}
