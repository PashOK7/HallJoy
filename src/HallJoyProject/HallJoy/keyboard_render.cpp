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
#include <string>
#include <vector>

#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#include "keyboard_render.h"
#include "keyboard_canvas.h"
#include "key_shape_win.h"
#include "analog_key_codes.h"
#include "digital_keyboard_state.h"
#include "backend.h"
#include "settings.h"
#include "ui_theme.h"

#include "binding_actions.h"
#include "bindings.h"
#include "remap_icons.h"
#include "win_util.h"
#include "key_settings.h"

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

static std::array<uint8_t, halljoy::keycode::kCount> g_selLast{};
static std::array<uint8_t, halljoy::keycode::kCount> g_selMode{};
static std::array<DWORD, halljoy::keycode::kCount>   g_selStartTick{};

static void SelAnim_Notify(uint16_t hid, bool selected, DWORD now)
{
    if (!halljoy::keycode::IsSupported(hid)) return;

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
    if (!halljoy::keycode::IsSupported(hid)) return selected ? 1.0f : 0.0f;

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

static std::array<uint8_t, halljoy::keycode::kCount>  g_lastOverride{};
static std::array<uint8_t, halljoy::keycode::kCount>  g_gearMode{};
static std::array<DWORD, halljoy::keycode::kCount>    g_gearStartTick{};

// NEW: current selected HID (from UI) so renderer can run "gear wow spin" only while editing that key
static uint16_t g_renderSelectedHid = 0;

static void GearAnim_NotifyOverrideState(uint16_t hid, bool overrideOn, DWORD now)
{
    if (!halljoy::keycode::IsSupported(hid)) return;

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
static std::array<bool, halljoy::keycode::kCount>  g_impactWasFull{};
static std::array<DWORD, halljoy::keycode::kCount> g_impactStartTick{};
static std::array<bool, halljoy::keycode::kCount>  g_impactActive{};

static void ImpactAnim_NotifyValue(uint16_t hid, float v01, DWORD now)
{
    if (!halljoy::keycode::IsSupported(hid)) return;

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
    if (!halljoy::keycode::IsSupported(hid)) return 0.0f;
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
    if (!halljoy::keycode::IsSupported(hid)) return;

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
    if (!halljoy::keycode::IsSupported(hid)) return 0.0f;
    if (g_gspin.hid != hid) return 0.0f;
    if (g_gspin.phase == GSPIN_NONE) return 0.0f;
    return g_gspin.angle;
}

// ---------------- digital (Windows keydown) state ----------------
static std::array<uint8_t, halljoy::keycode::kCount> g_digLast{};

static bool IsDigitalDownByHid(uint16_t hid)
{
    return halljoy::digital_keyboard::state.IsDown(hid);
}

// ------------------------------------------------

int KeyboardRender_GetAnimatingHids(uint16_t* outHids, int cap)
{
    if (!outHids || cap <= 0) return 0;
    halljoy::digital_keyboard::state.Advance(GetTickCount64());

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

    for (uint16_t hid = 1; hid < halljoy::keycode::kCount; ++hid)
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

        // Every drawable code animates, including extended vendor keys such as
        // Keychron RGB (0x404) and Wooting Profile/Mode keys. A shorter allow
        // list left their impact flash frozen until an unrelated repaint.
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
    if (halljoy::keycode::IsSupported(hid))
        return (float)BackendUI_GetAnalogMilli(hid) / 1000.0f;
    return 0.0f;
}

static int FindIconIndexByAction(BindAction a)
{
    if (BindingAction_IsMidi(a)) return RemapIcons_Count() + BindingAction_MidiAddress(a);
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

    int pads = Backend_GetVirtualGamepadsEnabled() ? std::clamp(Backend_GetVirtualGamepadCount(), 1, 4) : 0;
    int count = 0;
    for (int pad = 0; pad < pads; ++pad)
    {
        if (count >= 4) break;
        BindAction actions[4]{};
        int actionCount = BindingActions_CollectByHidForPad(pad, hid, actions, 4);
        for (int ai = 0; ai < actionCount && count < 4; ++ai)
        {
            BindAction act = actions[ai];
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
#ifndef HALLJOY_DISABLE_MIDI
    const auto midi = halljoy::midi::Get();
    const unsigned encoded = halljoy::keycode::IsSupported(hid) ? midi->keys[hid] : 0;
    if (midi->enabled && encoded && count < 4 &&
        !(g_suppressedBinding.enabled && g_suppressedBinding.hid == hid &&
          g_suppressedBinding.action == BindingAction_Midi(encoded - 1))) {
        if (out) out[count] = {RemapIcons_Count() + static_cast<int>(encoded - 1), 0};
        ++count;
    }
#endif
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
static constexpr size_t kGlyphCacheMaxEntries = 256;

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

static void GlyphCache_EvictOneIfFull()
{
    if (g_glyphCache.size() < kGlyphCacheMaxEntries)
        return;
    auto oldest = g_glyphCache.begin();
    Glyph_Free(oldest->second);
    g_glyphCache.erase(oldest);
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

    GlyphCache_EvictOneIfFull();
    auto [insIt, ok] = g_glyphCache.emplace(key, cg);
    if (!ok)
    {
        Glyph_Free(cg);
        return nullptr;
    }

    return &insIt->second;
}

// =============================================================================
// Direct2D key renderer
// =============================================================================
// One drawing path for the keyboard canvas (ID2D1HwndRenderTarget) and for
// owner-draw users such as the layout editor (ID2D1DCRenderTarget). Fills on
// whole pixels are aliased (crisp, and pixels outside a key stay untouched);
// strokes, text, the gear and the fractional analog edge are anti-aliased.

namespace {

D2D1_COLOR_F Rgb(COLORREF c, float alpha = 1.0f)
{
    return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f, alpha);
}

template <class T>
void SafeRelease(T*& p)
{
    if (p) { p->Release(); p = nullptr; }
}

IDWriteFactory* DWrite()
{
    static IDWriteFactory* factory = [] {
        IDWriteFactory* created = nullptr;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&created))))
            created = nullptr;
        return created;
    }();
    return factory;
}

// Device-independent text resources, shared by every target.
IDWriteTextFormat* LabelFormat(float sizePx)
{
    static std::unordered_map<int, IDWriteTextFormat*> formats;
    const int key = (int)lroundf(sizePx * 4.0f);
    auto found = formats.find(key);
    if (found != formats.end()) return found->second;
    IDWriteTextFormat* format = nullptr;
    if (DWrite() && SUCCEEDED(DWrite()->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizePx, L"", &format))) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    formats.emplace(key, format);
    return format;
}

IDWriteTextLayout* LabelLayout(const wchar_t* text, float sizePx, float w, float h)
{
    // Called for every labelled key in every frame: look up by a hash of the
    // text and box without building a string; the stored text is compared in
    // full, so a hash collision only costs a miss, never a wrong label.
    struct Entry { std::wstring text; int size, width, height; IDWriteTextLayout* layout; };
    static std::unordered_multimap<std::uint64_t, Entry> layouts;
    const int size = (int)lroundf(sizePx * 4), width = (int)w, height = (int)h;
    std::uint64_t hash = 1469598103934665603ull;
    for (const wchar_t* c = text; *c; ++c) { hash ^= static_cast<std::uint16_t>(*c); hash *= 1099511628211ull; }
    for (int v : { size, width, height }) { hash ^= static_cast<std::uint32_t>(v); hash *= 1099511628211ull; }
    const auto range = layouts.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
        if (it->second.size == size && it->second.width == width && it->second.height == height &&
            it->second.text == text)
            return it->second.layout;
    if (layouts.size() > 2048) {
        for (auto& entry : layouts) SafeRelease(entry.second.layout);
        layouts.clear();
    }
    IDWriteTextLayout* layout = nullptr;
    IDWriteTextFormat* format = LabelFormat(sizePx);
    if (!format || FAILED(DWrite()->CreateTextLayout(text, (UINT32)wcslen(text), format, w, h, &layout)))
        layout = nullptr;
    layouts.emplace(hash, Entry{ text, size, width, height, layout });
    return layout;
}

// Per-target resources. A target releases its entry before it is destroyed.
// Brushes are never mutated after creation: changing a brush that a pending
// Direct2D batch still references forces that batch to flush, and the old
// "one brush, SetColor per primitive" pattern flushed several times per key.
struct TargetResources {
    std::unordered_map<std::uint32_t, ID2D1SolidColorBrush*> solid;   // by RGBA8
    std::array<ID2D1LinearGradientBrush*, 64> flash{};                // by opacity level
    std::unordered_map<uint64_t, ID2D1Bitmap*> glyphs;
};
std::unordered_map<ID2D1RenderTarget*, TargetResources> g_targets;

TargetResources& Res(ID2D1RenderTarget* rt)
{
    return g_targets[rt];
}

std::uint32_t PackRgba8(const D2D1_COLOR_F& c)
{
    const auto q = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return (q(c.r) << 24) | (q(c.g) << 16) | (q(c.b) << 8) | q(c.a);
}

ID2D1SolidColorBrush* Brush(ID2D1RenderTarget* rt, const D2D1_COLOR_F& color)
{
    auto& solid = Res(rt).solid;
    const std::uint32_t key = PackRgba8(color);
    const auto found = solid.find(key);
    if (found != solid.end()) return found->second;
    if (solid.size() > 1024) {   // e.g. many glow fade levels; rebuild lazily
        for (auto& entry : solid) SafeRelease(entry.second);
        solid.clear();
    }
    ID2D1SolidColorBrush* brush = nullptr;
    const auto quantized = D2D1::ColorF(((key >> 24) & 255) / 255.0f, ((key >> 16) & 255) / 255.0f,
        ((key >> 8) & 255) / 255.0f, (key & 255) / 255.0f);
    if (FAILED(rt->CreateSolidColorBrush(quantized, &brush))) brush = nullptr;
    solid.emplace(key, brush);
    return brush;
}

// Vertical white -> transparent gradient over the unit square, at a fixed
// opacity level; callers map it onto a rectangle with the target transform.
ID2D1LinearGradientBrush* FlashBrush(ID2D1RenderTarget* rt, float opacity)
{
    auto& flash = Res(rt).flash;
    const std::size_t level = static_cast<std::size_t>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * (flash.size() - 1)));
    if (!flash[level]) {
        const D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, D2D1::ColorF(1, 1, 1, 1) }, { 1.0f, D2D1::ColorF(1, 1, 1, 0) } };
        ID2D1GradientStopCollection* collection = nullptr;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &collection))) {
            rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(), D2D1::Point2F(0, 1)),
                D2D1::BrushProperties(static_cast<float>(level) / (flash.size() - 1)), collection, &flash[level]);
            collection->Release();
        }
    }
    return flash[level];
}

ID2D1Bitmap* GlyphBitmap(ID2D1RenderTarget* rt, int iconIdx, int size, int styleVariant)
{
    auto& glyphs = Res(rt).glyphs;
    const uint64_t key = MakeGlyphKey(iconIdx, size, styleVariant);
    auto found = glyphs.find(key);
    if (found != glyphs.end()) return found->second;
    ID2D1Bitmap* bitmap = nullptr;
    // The GDI+ glyph is rendered once into a premultiplied 32-bit DIB.
    if (CachedGlyph* glyph = Glyph_GetOrCreate(iconIdx, size, styleVariant); glyph && glyph->bits) {
        const auto props = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(rt->CreateBitmap(D2D1::SizeU(size, size), glyph->bits, size * 4, props, &bitmap)))
            bitmap = nullptr;
    }
    if (glyphs.size() > 512) {
        for (auto& entry : glyphs) SafeRelease(entry.second);
        glyphs.clear();
    }
    glyphs.emplace(key, bitmap);
    return bitmap;
}

ID2D1PathGeometry* Polygon(const POINT* points, int count, float offset)
{
    ID2D1Factory* factory = halljoy::keyboard_canvas::Factory();
    ID2D1PathGeometry* path = nullptr;
    if (!factory || FAILED(factory->CreatePathGeometry(&path))) return nullptr;
    ID2D1GeometrySink* sink = nullptr;
    if (SUCCEEDED(path->Open(&sink))) {
        sink->BeginFigure(D2D1::Point2F(points[0].x + offset, points[0].y + offset), D2D1_FIGURE_BEGIN_FILLED);
        for (int i = 1; i < count; ++i) sink->AddLine(D2D1::Point2F(points[i].x + offset, points[i].y + offset));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        sink->Release();
    }
    return path;
}

ID2D1StrokeStyle* RoundJoin()
{
    static ID2D1StrokeStyle* style = [] {
        ID2D1StrokeStyle* created = nullptr;
        const auto props = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
            D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_ROUND);
        if (ID2D1Factory* factory = halljoy::keyboard_canvas::Factory())
            if (FAILED(factory->CreateStrokeStyle(props, nullptr, 0, &created))) created = nullptr;
        return created;
    }();
    return style;
}

D2D1_RECT_F RectF(const RECT& r) { return D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom); }

// Pixel-aligned 1 px outline of a rectangle (GDI Rectangle equivalent).
// Four pixel-aligned fills cover exactly the pixels of a half-pixel 1 px
// stroke, without Direct2D's geometry stroking (a hot path for every key).
void Outline(ID2D1RenderTarget* rt, const RECT& r, const D2D1_COLOR_F& color)
{
    if (r.right - r.left < 2 || r.bottom - r.top < 2) {
        rt->FillRectangle(RectF(r), Brush(rt, color));
        return;
    }
    auto* brush = Brush(rt, color);
    const float l = (float)r.left, t = (float)r.top, rr = (float)r.right, b = (float)r.bottom;
    rt->FillRectangle(D2D1::RectF(l, t, rr, t + 1.0f), brush);
    rt->FillRectangle(D2D1::RectF(l, b - 1.0f, rr, b), brush);
    rt->FillRectangle(D2D1::RectF(l, t + 1.0f, l + 1.0f, b - 1.0f), brush);
    rt->FillRectangle(D2D1::RectF(rr - 1.0f, t + 1.0f, rr, b - 1.0f), brush);
}

void DrawBoundIcons(ID2D1RenderTarget* rt, UINT dpi, const RECT& area, uint16_t hid, int iconCount,
    const BoundIconEntry* entries)
{
    const int iw = area.right - area.left, ih = area.bottom - area.top;
    if (iw <= 10 || ih <= 10) return;
    const int baseSize = std::clamp(MulDiv((int)Settings_GetBoundKeyIconSizePx(), (int)dpi, 96), 8, std::min(iw, ih));
    const int gap = std::clamp(MulDiv(2, (int)dpi, 96), 1, 5);
    auto drawAt = [&](const BoundIconEntry& e, int x, int y, int size) {
        size = std::clamp(size, 8, std::min(iw, ih));
        if (ID2D1Bitmap* bitmap = GlyphBitmap(rt, e.iconIdx, size, e.styleVariant))
            rt->DrawBitmap(bitmap, D2D1::RectF((float)x, (float)y, (float)(x + size), (float)(y + size)));
    };
    // Same placement as BuildMiniIconRectsForButton (hit testing must agree).
    if (iconCount == 1) {
        const int size = std::clamp(baseSize, 8, std::min(iw, ih));
        drawAt(entries[0], (area.left + area.right - size) / 2, (area.top + area.bottom - size) / 2, size);
    } else if (iconCount == 2) {
        const int cellW = std::max(8, (iw - gap) / 2);
        const int size = std::clamp(baseSize, 8, std::min(cellW, ih));
        const int y = area.top + (ih - size) / 2;
        drawAt(entries[0], area.left + (cellW - size) / 2, y, size);
        drawAt(entries[1], area.left + cellW + gap + (cellW - size) / 2, y, size);
    } else if (iconCount == 3) {
        const int rowH = std::max(8, (ih - gap) / 2), topCellW = std::max(8, (iw - gap) / 2);
        const int size = std::clamp(baseSize, 8, std::min(topCellW, rowH));
        const int topY = area.top + (rowH - size) / 2;
        drawAt(entries[0], area.left + (topCellW - size) / 2, topY, size);
        drawAt(entries[1], area.left + topCellW + gap + (topCellW - size) / 2, topY, size);
        drawAt(entries[2], area.left + (iw - size) / 2, area.top + rowH + gap + (rowH - size) / 2, size);
    } else {
        const int cellW = std::max(8, (iw - gap) / 2), cellH = std::max(8, (ih - gap) / 2);
        const int size = std::clamp(baseSize, 8, std::min(cellW, cellH));
        for (int i = 0; i < 4 && i < iconCount; ++i) {
            const int cellX = area.left + (i % 2) * (cellW + gap), cellY = area.top + (i / 2) * (cellH + gap);
            drawAt(entries[i], cellX + (cellW - size) / 2, cellY + (cellH - size) / 2, size);
        }
    }
}

// Green dot: Windows sees this key as pressed (Raw Input keyboard state).
void DrawDigitalIndicatorAA(ID2D1RenderTarget* rt, UINT dpi, const RECT& inner)
{
    const int d = std::clamp(MulDiv(7, (int)dpi, 96), 5, 12);
    const int pad = std::clamp(MulDiv(3, (int)dpi, 96), 1, 8);
    const float x = (float)(inner.left + pad), y = (float)(inner.bottom - pad - d), r = d * 0.5f;
    const auto dot = D2D1::Ellipse(D2D1::Point2F(x + r, y + r), r, r);
    rt->FillEllipse(dot, Brush(rt, D2D1::ColorF(120 / 255.0f, 210 / 255.0f, 140 / 255.0f, 230 / 255.0f)));
    rt->DrawEllipse(dot, Brush(rt, D2D1::ColorF(10 / 255.0f, 10 / 255.0f, 10 / 255.0f, 220 / 255.0f)), 1.2f);
}

void DrawGear(ID2D1RenderTarget* rt, UINT dpi, const RECT& inner, bool selected, uint16_t hid)
{
    constexpr int teeth = 6;
    constexpr float toothWidth01 = 0.5f, innerRatio = 0.6f, holeRatio = 0.23f, outlineW = 1.2f;
    constexpr DWORD APPEAR_MS = 180, SPIN_MS = 1000, DISAPPEAR_MS = 160;
    const int d = std::clamp(MulDiv(11, (int)dpi, 96), 7, 24);
    const DWORD now = GetTickCount();
    float scale = 1.0f, ang = 0.0f;
    if (halljoy::keycode::IsSupported(hid) && g_gearMode[hid] != GEAR_NONE) {
        const DWORD dt = now - g_gearStartTick[hid];
        if (g_gearMode[hid] == GEAR_APPEAR) {
            scale = EaseOutCubic((float)dt / (float)APPEAR_MS);
            ang = 4.0f * 3.14159265f * EaseOutCubic((float)dt / (float)SPIN_MS);
        } else if (g_gearMode[hid] == GEAR_DISAPPEAR) {
            scale = 1.0f - EaseOutCubic((float)dt / (float)DISAPPEAR_MS);
        }
    }
    ang += GearSpin_GetAngle(hid);
    if (scale <= 0.001f) return;
    const int x = std::max<int>(inner.right - d, inner.left + 1), y = inner.top + 1;
    const float cx = x + d * 0.5f, cy = y + d * 0.5f;
    const float period = 3.14159265f * 2.0f / teeth, toothW = period * toothWidth01, gapW = (period - toothW) * 0.5f;
    const float rTooth = d * 0.5f * scale, rRoot = rTooth * innerRatio, rHole = rTooth * holeRatio;
    ID2D1PathGeometry* gear = nullptr;
    if (FAILED(halljoy::keyboard_canvas::Factory()->CreatePathGeometry(&gear))) return;
    ID2D1GeometrySink* sink = nullptr;
    if (SUCCEEDED(gear->Open(&sink))) {
        auto at = [&](float a, float r) { return D2D1::Point2F(cx + r * cosf(a + ang), cy + r * sinf(a + ang)); };
        sink->BeginFigure(at(0, rRoot), D2D1_FIGURE_BEGIN_FILLED);
        for (int i = 0; i < teeth; ++i) {
            const float a0 = i * period;
            if (i) sink->AddLine(at(a0, rRoot));
            sink->AddLine(at(a0 + gapW, rTooth));
            sink->AddLine(at(a0 + gapW + toothW, rTooth));
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        sink->Release();
    }
    const auto outline = D2D1::ColorF(15 / 255.0f, 15 / 255.0f, 15 / 255.0f, 220 / 255.0f);
    rt->FillGeometry(gear, Brush(rt, Rgb(selected ? RGB(255, 190, 40) : RGB(255, 170, 0))));
    rt->DrawGeometry(gear, Brush(rt, outline), outlineW, RoundJoin());
    const auto hole = D2D1::Ellipse(D2D1::Point2F(cx, cy), rHole, rHole);
    rt->FillEllipse(hole, Brush(rt, Rgb(KEY_INNER_BG)));
    rt->DrawEllipse(hole, Brush(rt, outline), outlineW);
    gear->Release();
}

} // namespace

void KeyboardRender_ReleaseTargetResources(ID2D1RenderTarget* rt)
{
    auto found = g_targets.find(rt);
    if (found == g_targets.end()) return;
    for (auto& entry : found->second.solid) SafeRelease(entry.second);
    for (auto& brush : found->second.flash) SafeRelease(brush);
    for (auto& entry : found->second.glyphs) SafeRelease(entry.second);
    g_targets.erase(found);
}

void KeyboardRender_DrawTextD2D(ID2D1RenderTarget* rt, const wchar_t* text, const RECT& area, COLORREF color,
    float sizePx)
{
    if (!rt || !text || !*text) return;
    const float w = (float)(area.right - area.left), h = (float)(area.bottom - area.top);
    if (w <= 0 || h <= 0) return;
    if (IDWriteTextLayout* layout = LabelLayout(text, sizePx, w, h))
        rt->DrawTextLayout(D2D1::Point2F((float)area.left, (float)area.top), layout, Brush(rt, Rgb(color)),
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

bool KeyboardRender_AnyAnimationActive()
{
    if (g_gspin.phase != GSPIN_NONE) return true;
    const DWORD now = GetTickCount();
    for (uint16_t hid = 1; hid < halljoy::keycode::kCount; ++hid) {
        if (g_gearMode[hid] != GEAR_NONE || g_selMode[hid] != SEL_NONE) return true;
        if (g_impactActive[hid] && ImpactAnim_GetAlpha(hid, now) > 0.001f) return true;
    }
    return false;
}

void KeyboardRender_DrawKeyD2D(ID2D1RenderTarget* rt, const KeyboardRenderKey& key)
{
    if (!rt) return;
    RECT rc = key.rc;
    if (rc.right - rc.left <= 1 || rc.bottom - rc.top <= 1) return;
    const POINT notch = key.notch;
    const uint16_t hid = key.hid, actualHid = key.actualHid;
    const bool realKey = hid != 0, realActual = actualHid != 0;
    float v01 = key.v01 < 0.0f ? KeyboardRender_ReadAnalog01(hid) : key.v01;
    v01 = std::clamp(v01, 0.0f, 1.0f);
    const DWORD now = GetTickCount();
    // One DPI query per key (or none when the caller supplies it per frame).
    const UINT dpi = key.dpi ? key.dpi : WinUtil_GetDpiForWindowCompat(key.dpiWindow);

    float selT = 0.0f, flashAlpha = 0.0f;
    if (realActual && halljoy::keycode::IsSupported(actualHid)) {
        SelAnim_Notify(actualHid, key.selected, now);
        selT = SelAnim_GetT(actualHid, key.selected, now);
    }
    if (realKey && halljoy::keycode::IsSupported(actualHid)) {
        ImpactAnim_NotifyValue(actualHid, v01, now);
        flashAlpha = ImpactAnim_GetAlpha(actualHid, now);
    }

    // Everything for this key stays inside its own contour. Only the selection
    // glow (strokes up to 7 px wide) and compound shapes reach outside the
    // rectangle; every other element is drawn inside it, so a plain key needs
    // no clip push/pop.
    ID2D1PathGeometry* outer = nullptr;
    ID2D1Layer* outerLayer = nullptr;
    const bool clipToKey = notch.x != 0 || selT > 0.001f;
    if (clipToKey) rt->PushAxisAlignedClip(RectF(rc), D2D1_ANTIALIAS_MODE_ALIASED);
    if (notch.x) {
        const auto points = KeyShape_Points(rc, notch);
        outer = Polygon(points.data(), (int)points.size(), 0.0f);
        if (outer && SUCCEEDED(rt->CreateLayer(&outerLayer)))
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), outer, D2D1_ANTIALIAS_MODE_ALIASED), outerLayer);
    }
    // One anti-aliasing mode for the whole key (mode switches split batches).
    // Pixel-aligned rectangles and half-pixel 1 px lines render exactly the
    // same per-primitive as aliased; only the fractional fill edge blends.
    if (rt->GetAntialiasMode() != D2D1_ANTIALIAS_MODE_PER_PRIMITIVE)
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    rt->FillRectangle(RectF(rc), Brush(rt, Rgb(UiTheme::Color_ControlBg())));

    if (selT > 0.001f) {
        const D2D1_RECT_F r = D2D1::RectF(rc.left + 0.5f, rc.top + 0.5f, rc.right - 0.5f, rc.bottom - 0.5f);
        const float alphas[3] = { 38.0f, 70.0f, 120.0f }, widths[3] = { 7.0f, 4.0f, 2.0f };
        ID2D1PathGeometry* contour = nullptr;
        if (notch.x) {
            RECT border = rc;
            --border.right; --border.bottom;
            const auto points = KeyShape_Points(border, notch);
            contour = Polygon(points.data(), (int)points.size(), 0.5f);
        }
        for (int i = 0; i < 3; ++i) {
            auto* brush = Brush(rt, Rgb(RGB(255, 170, 90), alphas[i] * selT / 255.0f));
            if (contour) rt->DrawGeometry(contour, brush, widths[i], RoundJoin());
            else rt->DrawRectangle(r, brush, widths[i], RoundJoin());
        }
        SafeRelease(contour);
    }
    const COLORREF borderC = selT > 0.0f ? LerpColor(UiTheme::Color_Border(), RGB(255, 170, 90), selT)
                                         : UiTheme::Color_Border();
    if (!notch.x) Outline(rt, rc, Rgb(borderC));

    RECT inner = rc;
    InflateRect(&inner, -3, -3);
    ID2D1PathGeometry* innerShape = nullptr;
    ID2D1Layer* innerLayer = nullptr;
    if (notch.x) {
        const int inset = KeyShape_InnerInset(rc, notch);
        inner = rc;
        InflateRect(&inner, -inset, -inset);
        const auto contour = KeyShape_Points(inner, POINT{ notch.x, notch.y - 2 * inset });
        innerShape = Polygon(contour.data(), (int)contour.size(), 0.0f);
        if (innerShape && SUCCEEDED(rt->CreateLayer(&innerLayer)))
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), innerShape, D2D1_ANTIALIAS_MODE_ALIASED), innerLayer);
    }
    rt->FillRectangle(RectF(inner), Brush(rt, Rgb(KEY_INNER_BG)));
    if (realKey && v01 > 0.0f) {
        // Fractional depth: the bottom edge is anti-aliased instead of jumping
        // a whole pixel, so slow presses move smoothly.
        const float top = (float)inner.top, bottom = top + (inner.bottom - inner.top) * v01;
        const D2D1_RECT_F fill = D2D1::RectF((float)inner.left, top, (float)inner.right, bottom);
        // Same pixels as an anti-aliased fractional rectangle, but only
        // pixel-aligned primitives (the anti-aliased one was the slowest
        // operation of a whole frame): whole rows solid, then the partial row
        // at alpha = its coverage, which is exactly what anti-aliasing produces.
        const float rows = bottom - top;
        const float whole = std::floor(rows);
        const float partial = rows - whole;
        if (whole > 0.0f)
            rt->FillRectangle(D2D1::RectF(fill.left, top, fill.right, top + whole), Brush(rt, Rgb(UiTheme::Color_Accent())));
        if (partial > 0.002f)
            rt->FillRectangle(D2D1::RectF(fill.left, top + whole, fill.right, top + whole + 1.0f),
                Brush(rt, Rgb(UiTheme::Color_Accent(), partial)));
        const float flashRows = whole + (partial > 0.002f ? 1.0f : 0.0f); // pixel-aligned height
        if (flashAlpha > 0.01f && flashRows > 0.0f) {
            if (auto* flash = FlashBrush(rt, flashAlpha * 200.0f / 255.0f)) {
                // Map the unit-square gradient onto the fill rectangle.
                D2D1_MATRIX_3X2_F saved{};
                rt->GetTransform(&saved);
                rt->SetTransform(D2D1::Matrix3x2F::Scale(fill.right - fill.left, flashRows) *
                    D2D1::Matrix3x2F::Translation(fill.left, top) * saved);
                rt->FillRectangle(D2D1::RectF(0, 0, 1, 1), flash);
                rt->SetTransform(saved);
            }
        }
    }
    if (innerLayer) { rt->PopLayer(); innerLayer->Release(); }
    SafeRelease(innerShape);

    RECT iconArea = rc;
    if (notch.x) {
        RECT border = rc;
        --border.right; --border.bottom;
        const auto points = KeyShape_Points(border, notch);
        if (ID2D1PathGeometry* path = Polygon(points.data(), (int)points.size(), 0.5f)) {
            rt->DrawGeometry(path, Brush(rt, Rgb(key.selected ? RGB(255, 170, 90) : UiTheme::Color_Border())), 1.0f);
            path->Release();
        }
        inner.left += notch.x;
        iconArea.left += notch.x;
    }
    InflateRect(&iconArea, -1, -1);

    BoundIconEntry entries[4]{};
    const int iconCount = realKey ? CollectDisplayedIconsByHid(hid, entries) : 0;
    if (iconCount > 0) {
        DrawBoundIcons(rt, dpi, iconArea, hid, iconCount, entries);
    } else if (key.label && *key.label) {
        const float size = (float)std::clamp(MulDiv(12, (int)dpi, 96), 8, 40);
        const float w = (float)(inner.right - inner.left), h = (float)(inner.bottom - inner.top);
        if (IDWriteTextLayout* layout = LabelLayout(key.label, size, w, h)) {
            rt->DrawTextLayout(D2D1::Point2F((float)inner.left, (float)inner.top), layout,
                Brush(rt, Rgb(key.disabled ? UiTheme::Color_TextMuted() : UiTheme::Color_Text())),
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }

    if (realKey && realActual && halljoy::keycode::IsStandardHid(actualHid) && IsDigitalDownByHid(actualHid)) {
        DrawDigitalIndicatorAA(rt, dpi, inner);
    }

    if (realActual && halljoy::keycode::IsSupported(actualHid)) {
        const bool overrideOn = KeySettings_GetUseUnique(actualHid);
        GearAnim_NotifyOverrideState(actualHid, overrideOn, now);
        if (overrideOn || g_gearMode[actualHid] == GEAR_DISAPPEAR || g_gearMode[actualHid] == GEAR_APPEAR)
            DrawGear(rt, dpi, inner, key.selected, actualHid);
    }

    if (key.dropHover) {
        RECT r = rc;
        const int inset = notch.x ? std::min(2, KeyShape_InnerInset(rc, notch)) : 2;
        InflateRect(&r, -inset, -inset);
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        if (notch.x) {
            const auto points = KeyShape_Points(r, POINT{ notch.x, notch.y - 2 * inset });
            if (ID2D1PathGeometry* path = Polygon(points.data(), (int)points.size(), 0.0f)) {
                rt->DrawGeometry(path, Brush(rt, Rgb(RGB(60, 200, 120))), 3.0f);
                path->Release();
            }
        } else {
            rt->DrawRectangle(RectF(r), Brush(rt, Rgb(RGB(60, 200, 120))), 3.0f);
        }
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE); // crisp 3 px outline only
    }

    if (outerLayer) { rt->PopLayer(); outerLayer->Release(); }
    SafeRelease(outer);
    if (clipToKey) rt->PopAxisAlignedClip();
}

// Owner-draw entry point (layout editor and any classic WM_DRAWITEM user):
// the same Direct2D drawing bound to the item's DC.
void KeyboardRender_DrawKey(const DRAWITEMSTRUCT* dis, uint16_t hid, bool selected, float v01)
{
    if (!dis) return;
    const RECT rc = dis->rcItem;
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1) return;
    // Premultiplied target: pixels the key does not cover (the notch of a
    // compound key) stay transparent and keep the neighbour's pixels. The GDI
    // clip on the destination guards the same area a second time.
    static ID2D1DCRenderTarget* target = nullptr;
    if (!target) {
        ID2D1Factory* factory = halljoy::keyboard_canvas::Factory();
        const auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        if (!factory || FAILED(factory->CreateDCRenderTarget(&props, &target))) { target = nullptr; return; }
    }
    const POINT notch = KeyShape_Get(dis->hwndItem);
    struct RestoreOutputClip {
        HDC dc; int saved;
        ~RestoreOutputClip() { if (saved) RestoreDC(dc, saved); }
    } clip{ dis->hDC, notch.x ? SaveDC(dis->hDC) : 0 };
    if (notch.x && (!clip.saved || ExcludeClipRect(dis->hDC, rc.left, rc.top + notch.y,
        rc.left + notch.x, rc.bottom) == ERROR)) return;
    if (FAILED(target->BindDC(dis->hDC, &rc))) return;
    KeyboardRenderKey key{};
    key.dpiWindow = dis->hwndItem;
    key.rc = RECT{ 0, 0, w, h };
    key.notch = notch;
    key.hid = hid;
    key.actualHid = (uint16_t)GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA);
    key.selected = selected;
    key.disabled = (dis->itemState & ODS_DISABLED) != 0;
    key.v01 = v01;
    wchar_t text[64]{};
    GetWindowTextW(dis->hwndItem, text, 63);
    key.label = text;
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    KeyboardRender_DrawKeyD2D(target, key);
    if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
        KeyboardRender_ReleaseTargetResources(target);
        target->Release();
        target = nullptr;
    }
}


void KeyboardRender_NotifySelectedHid(uint16_t hid)
{
    if (!halljoy::keycode::IsSupported(hid)) hid = 0;

    g_renderSelectedHid = hid;

    // If user leaves the spinning key, begin smooth stop
    if (g_gspin.phase != GSPIN_NONE && g_gspin.hid != 0 && g_gspin.hid != hid)
    {
        GearSpin_BeginStop(GetTickCount());
    }
}

void KeyboardRender_OnGearClicked(uint16_t hid)
{
    if (!halljoy::keycode::IsSupported(hid)) return;

    // Only meaningful if Override is enabled (gear visible as "override marker")
    if (!KeySettings_GetUseUnique(hid)) return;

    // Do NOT hard-require g_renderSelectedHid==hid here.
    // UI tries to select the key before calling this, but in some edge cases
    // selection sync may lag by one message/tick. We still want the burst to start.
    GearSpin_Start(hid, GetTickCount());
}
