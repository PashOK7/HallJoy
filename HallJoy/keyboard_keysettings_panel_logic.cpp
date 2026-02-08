// keyboard_keysettings_panel_logic.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <unordered_map>

#include "keyboard_keysettings_panel_internal.h"
#include "settings.h"
#include "key_settings.h"

// Shared curve math (single source of truth with UI graph + backend)
#include "curve_math.h"

// PremiumCombo replaces WC_COMBOBOXW, so CB_* messages no longer apply.
#include "premium_combo.h"

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;

static void RedrawNoErase(HWND h)
{
    if (!h) return;
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
}

// -----------------------------------------------------------------------------
// FIX: keep toggle visual state in sync with selected HID (override/invert)
// -----------------------------------------------------------------------------
static KspToggleAnimState* KspToggle_GetState(HWND hBtn)
{
    if (!hBtn) return nullptr;
    return (KspToggleAnimState*)GetPropW(hBtn, KSP_TOGGLE_ANIM_PROP);
}

static void KspToggle_EnsureState(HWND hBtn)
{
    if (!hBtn) return;
    if (KspToggle_GetState(hBtn)) return;

    auto* st = new KspToggleAnimState();
    SetPropW(hBtn, KSP_TOGGLE_ANIM_PROP, (HANDLE)st);
}

static void KspToggle_Sync(HWND hBtn, bool checked, bool forceSnap)
{
    if (!hBtn) return;

    // Sync actual checkbox state (so accessibility / state is correct)
    SendMessageW(hBtn, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);

    KspToggle_EnsureState(hBtn);
    auto* st = KspToggle_GetState(hBtn);
    if (!st) { InvalidateRect(hBtn, nullptr, FALSE); return; }

    float target = checked ? 1.0f : 0.0f;
    DWORD now = GetTickCount();

    // First time: snap (no animation), to avoid weird "boot animation"
    if (!st->initialized)
    {
        st->initialized = true;
        st->checked = checked;
        st->running = false;
        st->t = target;
        st->from = target;
        st->to = target;
        st->startTick = now;
        KillTimer(hBtn, 1);
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    // Force snap (used when hiding / hard reset)
    if (forceSnap)
    {
        st->checked = checked;
        st->running = false;
        st->t = target;
        st->from = target;
        st->to = target;
        st->startTick = now;
        KillTimer(hBtn, 1);
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    // Already going to the same target -> don't restart animation every sync tick
    if (st->running)
    {
        st->checked = checked;

        if (std::fabs(st->to - target) <= 1e-4f)
        {
            // keep running as-is
            return;
        }

        // retarget smoothly
        st->from = st->t;
        st->to = target;
        st->startTick = now;
        st->durationMs = 140;
        st->running = true;

        SetTimer(hBtn, 1, 15, nullptr);
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    // Not running: if already at target -> nothing to do
    if (std::fabs(st->t - target) <= 1e-4f && st->checked == checked)
        return;

    // Start animation from current visual t -> target
    st->checked = checked;
    st->from = st->t;
    st->to = target;
    st->startTick = now;
    st->durationMs = 140;
    st->running = true;

    SetTimer(hBtn, 1, 15, nullptr);
    InvalidateRect(hBtn, nullptr, FALSE);
}

// -----------------------------------------------------------------------------
// Undo/Redo
// -----------------------------------------------------------------------------
static bool NearlyEq(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

static bool SameKs(const KeyDeadzone& a, const KeyDeadzone& b)
{
    return a.useUnique == b.useUnique &&
        a.invert == b.invert &&
        a.curveMode == b.curveMode &&
        NearlyEq(a.low, b.low) &&
        NearlyEq(a.high, b.high) &&
        NearlyEq(a.antiDeadzone, b.antiDeadzone) &&
        NearlyEq(a.outputCap, b.outputCap) &&
        NearlyEq(a.cp1_x, b.cp1_x) &&
        NearlyEq(a.cp1_y, b.cp1_y) &&
        NearlyEq(a.cp2_x, b.cp2_x) &&
        NearlyEq(a.cp2_y, b.cp2_y) &&
        NearlyEq(a.cp1_w, b.cp1_w) &&
        NearlyEq(a.cp2_w, b.cp2_w);
}

struct UndoHistory
{
    std::vector<KeyDeadzone> undo;
    std::vector<KeyDeadzone> redo;
    bool inContinuousCapture = false;
};

static UndoHistory g_histGlobal;
static std::unordered_map<uint16_t, UndoHistory> g_histKey;

static bool g_applyingHistory = false;

bool Ksp_IsKeySelected()
{
    return g_kspSelectedHid != 0;
}

bool Ksp_IsOverrideOnForKey()
{
    if (!Ksp_IsKeySelected()) return false;
    return KeySettings_Get(g_kspSelectedHid).useUnique;
}

bool Ksp_EditingUniqueKey()
{
    return Ksp_IsKeySelected() && Ksp_IsOverrideOnForKey();
}

static UndoHistory& HistForActiveTarget()
{
    if (Ksp_EditingUniqueKey() && g_kspSelectedHid != 0)
        return g_histKey[g_kspSelectedHid];

    return g_histGlobal;
}

static void Hist_ClearRedo(UndoHistory& h)
{
    h.redo.clear();
}

static void Hist_Cap(std::vector<KeyDeadzone>& v)
{
    const size_t MAX = 50;
    if (v.size() > MAX)
        v.erase(v.begin(), v.begin() + (v.size() - MAX));
}

static void Hist_PushUndo(UndoHistory& h, const KeyDeadzone& snapshot)
{
    if (!h.undo.empty() && SameKs(h.undo.back(), snapshot)) return;
    h.undo.push_back(snapshot);
    Hist_Cap(h.undo);
}

static void Hist_PushRedo(UndoHistory& h, const KeyDeadzone& snapshot)
{
    if (!h.redo.empty() && SameKs(h.redo.back(), snapshot)) return;
    h.redo.push_back(snapshot);
    Hist_Cap(h.redo);
}

static bool GraphCaptureActive()
{
    if (!g_kspParent) return false;
    return GetCapture() == g_kspParent;
}

static KeyDeadzone NormalizeForStorage(const KeyDeadzone& in)
{
    KeyDeadzone ks = in;
    ks.low = std::clamp(ks.low, 0.0f, 0.999f);
    ks.high = std::clamp(ks.high, 0.001f, 1.0f);
    if (ks.high < ks.low + 0.01f) ks.high = std::clamp(ks.low + 0.01f, 0.01f, 1.0f);

    ks.antiDeadzone = std::clamp(ks.antiDeadzone, 0.0f, 0.99f);
    ks.outputCap = std::clamp(ks.outputCap, 0.01f, 1.0f);
    if (ks.outputCap < ks.antiDeadzone + 0.01f)
        ks.outputCap = std::clamp(ks.antiDeadzone + 0.01f, 0.01f, 1.0f);

    ks.cp1_x = std::clamp(ks.cp1_x, 0.0f, 1.0f);
    ks.cp1_y = std::clamp(ks.cp1_y, 0.0f, 1.0f);
    ks.cp2_x = std::clamp(ks.cp2_x, 0.0f, 1.0f);
    ks.cp2_y = std::clamp(ks.cp2_y, 0.0f, 1.0f);

    ks.cp1_w = std::clamp(ks.cp1_w, 0.0f, 1.0f);
    ks.cp2_w = std::clamp(ks.cp2_w, 0.0f, 1.0f);

    float minGap = 0.01f;
    ks.cp1_x = std::clamp(ks.cp1_x, ks.low + minGap, ks.high - minGap);
    ks.cp2_x = std::clamp(ks.cp2_x, ks.cp1_x + minGap, ks.high - minGap);

    ks.curveMode = (uint8_t)((ks.curveMode == 0) ? 0 : 1);
    return ks;
}

static void ApplyToStorage_NoUndo(const KeyDeadzone& in)
{
    KeyDeadzone ks = NormalizeForStorage(in);

    if (!Ksp_EditingUniqueKey())
    {
        Settings_SetInputInvert(ks.invert);
        Settings_SetInputDeadzoneLow(ks.low);
        Settings_SetInputDeadzoneHigh(ks.high);
        Settings_SetInputAntiDeadzone(ks.antiDeadzone);
        Settings_SetInputOutputCap(ks.outputCap);
        Settings_SetInputBezierCp1X(ks.cp1_x);
        Settings_SetInputBezierCp1Y(ks.cp1_y);
        Settings_SetInputBezierCp2X(ks.cp2_x);
        Settings_SetInputBezierCp2Y(ks.cp2_y);
        Settings_SetInputBezierCp1W(ks.cp1_w);
        Settings_SetInputBezierCp2W(ks.cp2_w);
        Settings_SetInputCurveMode((UINT)ks.curveMode);
    }
    else
    {
        KeySettings_Set(g_kspSelectedHid, ks);
    }
}

bool Ksp_Undo()
{
    UndoHistory& h = HistForActiveTarget();
    if (h.undo.empty()) return false;

    h.inContinuousCapture = false;
    KeyDeadzone cur = Ksp_GetActiveSettings();
    KeyDeadzone prev = h.undo.back();
    h.undo.pop_back();

    Hist_PushRedo(h, cur);

    g_applyingHistory = true;
    ApplyToStorage_NoUndo(prev);
    g_applyingHistory = false;
    return true;
}

bool Ksp_Redo()
{
    UndoHistory& h = HistForActiveTarget();
    if (h.redo.empty()) return false;

    h.inContinuousCapture = false;
    KeyDeadzone cur = Ksp_GetActiveSettings();
    KeyDeadzone nxt = h.redo.back();
    h.redo.pop_back();

    Hist_PushUndo(h, cur);

    g_applyingHistory = true;
    ApplyToStorage_NoUndo(nxt);
    g_applyingHistory = false;
    return true;
}

// -----------------------------------------------------------------------------
// Mode (no Preset anymore)
// -----------------------------------------------------------------------------
KspGraphMode Ksp_GetActiveMode()
{
    if (Ksp_EditingUniqueKey())
    {
        uint8_t m = KeySettings_Get(g_kspSelectedHid).curveMode;
        return (m == 0) ? KspGraphMode::SmoothBezier : KspGraphMode::LinearSegments;
    }
    UINT m = Settings_GetInputCurveMode();
    return (m == 0) ? KspGraphMode::SmoothBezier : KspGraphMode::LinearSegments;
}

void Ksp_SetActiveMode(KspGraphMode m)
{
    KeyDeadzone ks = Ksp_GetActiveSettings();
    uint8_t newMode = (uint8_t)((m == KspGraphMode::SmoothBezier) ? 0 : 1);
    bool modeChanged = (ks.curveMode != newMode);
    ks.curveMode = newMode;
    if (modeChanged && newMode == 0)
    {
        ks.cp1_w = 0.5f;
        ks.cp2_w = 0.5f;
    }
    Ksp_SaveActiveSettings(ks);
}

// Presets REMOVED:
KspCurvePreset Ksp_GetSelectedPreset() { return KspCurvePreset::Linear; }
void Ksp_SetSelectedPreset(KspCurvePreset) {}
void Ksp_ApplyPresetToActive(KspCurvePreset) {}

// -----------------------------------------------------------------------------
// Active settings get/save
// -----------------------------------------------------------------------------
KeyDeadzone Ksp_GetActiveSettings()
{
    if (!Ksp_EditingUniqueKey())
    {
        KeyDeadzone ks{};
        ks.useUnique = true;
        ks.invert = Settings_GetInputInvert();
        ks.low = Settings_GetInputDeadzoneLow();
        ks.high = Settings_GetInputDeadzoneHigh();
        ks.antiDeadzone = Settings_GetInputAntiDeadzone();
        ks.outputCap = Settings_GetInputOutputCap();
        ks.cp1_x = Settings_GetInputBezierCp1X();
        ks.cp1_y = Settings_GetInputBezierCp1Y();
        ks.cp2_x = Settings_GetInputBezierCp2X();
        ks.cp2_y = Settings_GetInputBezierCp2Y();
        ks.cp1_w = Settings_GetInputBezierCp1W();
        ks.cp2_w = Settings_GetInputBezierCp2W();
        ks.curveMode = (uint8_t)Settings_GetInputCurveMode();
        return ks;
    }
    return KeySettings_Get(g_kspSelectedHid);
}

void Ksp_SaveActiveSettings(const KeyDeadzone& in)
{
    KeyDeadzone ks = NormalizeForStorage(in);

    if (g_applyingHistory)
    {
        ApplyToStorage_NoUndo(ks);
        return;
    }

    KeyDeadzone old = Ksp_GetActiveSettings();
    if (!SameKs(old, ks))
    {
        UndoHistory& hist = HistForActiveTarget();
        bool cap = GraphCaptureActive();

        if (cap)
        {
            if (!hist.inContinuousCapture)
            {
                Hist_PushUndo(hist, old);
                Hist_ClearRedo(hist);
                hist.inContinuousCapture = true;
            }
        }
        else
        {
            hist.inContinuousCapture = false;
            Hist_PushUndo(hist, old);
            Hist_ClearRedo(hist);
        }
    }

    ApplyToStorage_NoUndo(ks);
}

// -----------------------------------------------------------------------------
// Sync UI
// -----------------------------------------------------------------------------
static void ComputeYMinMaxFromPointsLocal(const KeyDeadzone& ks, float& outYMin, float& outYMax)
{
    outYMin = std::min({ ks.antiDeadzone, ks.outputCap, ks.cp1_y, ks.cp2_y });
    outYMax = std::max({ ks.antiDeadzone, ks.outputCap, ks.cp1_y, ks.cp2_y });
    outYMin = std::clamp(outYMin, 0.0f, 1.0f);
    outYMax = std::clamp(outYMax, 0.0f, 1.0f);
    if (outYMax < outYMin) std::swap(outYMin, outYMax);
}

// IMPORTANT FIX:
// Previously this file estimated Min/Max using a non-rational cubic Bezier derivative,
// which diverges from the real curve (rational Bezier with weights).
// Now we compute Min/Max by sampling the SAME rational curve math as graph/backend.
static void ComputeYMinMaxFromRationalBezierLocal(const KeyDeadzone& ks, float& outYMin, float& outYMax)
{
    CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(ks);

    float yMin = 1.0f;
    float yMax = 0.0f;

    constexpr int N = 140;
    for (int i = 0; i <= N; ++i)
    {
        float t = (float)i / (float)N;
        CurveMath::Vec2 p = CurveMath::EvalRationalBezier(c, t);
        yMin = std::min(yMin, p.y);
        yMax = std::max(yMax, p.y);
    }

    outYMin = std::clamp(yMin, 0.0f, 1.0f);
    outYMax = std::clamp(yMax, 0.0f, 1.0f);
    if (outYMax < outYMin) std::swap(outYMin, outYMax);
}

void Ksp_SyncUI()
{
    if (!g_kspParent) return;

    if (!GraphCaptureActive())
    {
        UndoHistory& hist = HistForActiveTarget();
        hist.inContinuousCapture = false;
    }

    // g_kspComboCurve is removed, don't update it
    HWND controls[] = { g_kspChkUnique, g_kspChkInvert, g_kspTxtInfo, g_kspComboMode };
    for (HWND h : controls) if (h) SendMessageW(h, WM_SETREDRAW, FALSE, 0);

    if (Ksp_IsKeySelected())
    {
        ShowWindow(g_kspChkUnique, SW_SHOW);
        bool on = Ksp_IsOverrideOnForKey();
        KspToggle_Sync(g_kspChkUnique, on, false); // animate on selection change too
    }
    else
    {
        KspToggle_Sync(g_kspChkUnique, false, true);
        ShowWindow(g_kspChkUnique, SW_HIDE);
    }

    KeyDeadzone ks = Ksp_GetActiveSettings();
    KspToggle_Sync(g_kspChkInvert, ks.invert, false); // animate on selection change too

    if (g_kspComboMode)
        PremiumCombo::SetCurSel(g_kspComboMode, (int)Ksp_GetActiveMode(), false);

    float shownAnti = 0.0f, shownCap = 1.0f;
    if (Ksp_GetActiveMode() == KspGraphMode::LinearSegments)
        ComputeYMinMaxFromPointsLocal(ks, shownAnti, shownCap);
    else
        ComputeYMinMaxFromRationalBezierLocal(ks, shownAnti, shownCap);

    auto pct = [](float f) { return f * 100.0f; };

    wchar_t buf[256]{};
    if (!Ksp_IsKeySelected())
        swprintf_s(buf, L"Global | DZ: %.1f%% | Act: %.1f%% | Min: %.1f%% | Max: %.1f%%",
            pct(ks.low), pct(ks.high), pct(shownAnti), pct(shownCap));
    else if (!Ksp_EditingUniqueKey())
        swprintf_s(buf, L"Editing Global via Key 0x%02X | Min: %.1f%% | Max: %.1f%%",
            g_kspSelectedHid, pct(shownAnti), pct(shownCap));
    else
        swprintf_s(buf, L"Key 0x%02X | DZ: %.1f%% | Act: %.1f%% | Min: %.1f%% | Max: %.1f%%",
            g_kspSelectedHid, pct(ks.low), pct(ks.high), pct(shownAnti), pct(shownCap));

    if (g_kspTxtInfo)
        SetWindowTextW(g_kspTxtInfo, buf);

    for (HWND h : controls) if (h) SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    for (HWND h : controls) RedrawNoErase(h);
}

void Ksp_RequestSave(HWND parent)
{
    HWND root = GetAncestor(parent, GA_ROOT);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}
