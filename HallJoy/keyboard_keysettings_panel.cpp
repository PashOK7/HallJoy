// keyboard_keysettings_panel.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

#include "keyboard_keysettings_panel.h"
#include "keyboard_keysettings_panel_internal.h"
#include "win_util.h"
#include "ui_theme.h"
#include "settings.h"
#include "key_settings.h"
#include "keyboard_profiles.h"

#include "premium_combo.h"

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

HWND     g_kspParent = nullptr;
uint16_t g_kspSelectedHid = 0;

HWND g_kspChkUnique = nullptr;
HWND g_kspChkInvert = nullptr;
HWND g_kspTxtInfo = nullptr;

HWND g_kspLblMode = nullptr;
HWND g_kspComboMode = nullptr;

HWND g_kspLblProfile = nullptr;
HWND g_kspComboProfile = nullptr;

static std::vector<KeyboardProfiles::ProfileInfo> g_profileList;

KspCurveMorphState g_kspMorph;
static constexpr UINT_PTR MORPH_TIMER_ID = 7777;

bool Ksp_Undo();
bool Ksp_Redo();

// ---------- Helpers ----------
static bool NearlyEq(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

static bool IsSameCurve(const KeyDeadzone& a, const KeyDeadzone& b)
{
    if (a.invert != b.invert) return false;
    if (a.curveMode != b.curveMode) return false;

    if (!NearlyEq(a.low, b.low)) return false;
    if (!NearlyEq(a.high, b.high)) return false;
    if (!NearlyEq(a.antiDeadzone, b.antiDeadzone)) return false;
    if (!NearlyEq(a.outputCap, b.outputCap)) return false;

    if (!NearlyEq(a.cp1_x, b.cp1_x)) return false;
    if (!NearlyEq(a.cp1_y, b.cp1_y)) return false;
    if (!NearlyEq(a.cp2_x, b.cp2_x)) return false;
    if (!NearlyEq(a.cp2_y, b.cp2_y)) return false;

    if (!NearlyEq(a.cp1_w, b.cp1_w)) return false;
    if (!NearlyEq(a.cp2_w, b.cp2_w)) return false;

    return true;
}

// Returns preset index in g_profileList, or -1 if none.
static int FindFirstMatchingPresetIndex(const KeyDeadzone& currentKs)
{
    for (int i = 0; i < (int)g_profileList.size(); ++i)
    {
        KeyDeadzone pKs{};
        if (KeyboardProfiles::LoadPreset(g_profileList[i].path, pKs))
        {
            if (IsSameCurve(currentKs, pKs))
                return i; // first match
        }
    }
    return -1;
}

static void RefreshProfileCombo()
{
    if (!g_kspComboProfile) return;

    KeyboardProfiles::RefreshList(g_profileList);

    PremiumCombo::Clear(g_kspComboProfile);

    // Add presets
    for (int i = 0; i < (int)g_profileList.size(); ++i)
    {
        int idx = PremiumCombo::AddString(g_kspComboProfile, g_profileList[i].name.c_str());
        PremiumCombo::SetItemButtonKind(g_kspComboProfile, idx, PremiumCombo::ItemButtonKind::Rename);
        PremiumCombo::SetItemButtonKind(g_kspComboProfile, idx, PremiumCombo::ItemButtonKind::Delete);
    }

    // Add create new row (last, starts with '+', PremiumCombo logic uses that)
    PremiumCombo::AddString(g_kspComboProfile, L"+ Create New Preset...");
}

// This is the exact behavior you requested:
// - On key selection change:
//    * auto-select first matching preset
//    * else show placeholder "Custom" (curSel=-1)
//    * never show Save icon in Custom state
static void ApplyAutoPresetOrCustom_OnKeySelection(const KeyDeadzone& ks)
{
    if (!g_kspComboProfile) return;

    int matchIdx = FindFirstMatchingPresetIndex(ks);

    if (matchIdx >= 0)
    {
        PremiumCombo::SetCurSel(g_kspComboProfile, matchIdx, false);

        KeyboardProfiles::SetActiveProfileName(g_profileList[matchIdx].name);
        KeyboardProfiles::SetDirty(false);

        PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
        return;
    }

    // Custom (no matching preset)
    PremiumCombo::SetCurSel(g_kspComboProfile, -1, false);
    KeyboardProfiles::SetActiveProfileName(L"");
    KeyboardProfiles::SetDirty(false);

    // NO save icon in Custom state (user must create new preset)
    PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
}

// During editing (not selection): keep current selection as-is.
// If a preset is selected, we can show Save icon when curve differs.
// If Custom (curSel==-1), we never show Save icon.
static void UpdateDirtyIcon_ForCurrentSelection(const KeyDeadzone& ks)
{
    if (!g_kspComboProfile) return;

    int sel = PremiumCombo::GetCurSel(g_kspComboProfile);
    int count = PremiumCombo::GetCount(g_kspComboProfile);
    bool isCreateRow = (count > 0 && sel == count - 1);

    if (sel < 0 || isCreateRow)
    {
        // Custom (no selection) or create row: no save icon
        KeyboardProfiles::SetDirty(false);
        PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
        return;
    }

    if (sel >= 0 && sel < (int)g_profileList.size())
    {
        KeyDeadzone pKs{};
        if (!KeyboardProfiles::LoadPreset(g_profileList[sel].path, pKs))
        {
            KeyboardProfiles::SetDirty(false);
            PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
            return;
        }

        bool dirty = !IsSameCurve(ks, pKs);
        KeyboardProfiles::SetActiveProfileName(g_profileList[sel].name);
        KeyboardProfiles::SetDirty(dirty);

        PremiumCombo::SetExtraIcon(g_kspComboProfile,
            dirty ? PremiumCombo::ExtraIconKind::Save : PremiumCombo::ExtraIconKind::None);
        return;
    }

    KeyboardProfiles::SetDirty(false);
    PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
}

static bool ToggleHitTest_SwitchOnly(HWND hBtn, POINT ptClient)
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

// ---------- Toggle animation helpers ----------
static KspToggleAnimState* ToggleAnim_Get(HWND hBtn)
{
    return (KspToggleAnimState*)GetPropW(hBtn, KSP_TOGGLE_ANIM_PROP);
}

static void ToggleAnim_Set(HWND hBtn, KspToggleAnimState* st)
{
    if (st) SetPropW(hBtn, KSP_TOGGLE_ANIM_PROP, (HANDLE)st);
    else RemovePropW(hBtn, KSP_TOGGLE_ANIM_PROP);
}

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static void ToggleAnim_Start(HWND hBtn, bool checked, bool animate)
{
    KspToggleAnimState* st = ToggleAnim_Get(hBtn);
    if (!st)
    {
        st = new KspToggleAnimState();
        ToggleAnim_Set(hBtn, st);
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

static void ToggleAnim_Tick(HWND hBtn)
{
    KspToggleAnimState* st = ToggleAnim_Get(hBtn);
    if (!st || !st->running) { KillTimer(hBtn, 1); return; }

    DWORD now = GetTickCount();
    DWORD dt = now - st->startTick;
    float x = (st->durationMs > 0) ? (float)dt / (float)st->durationMs : 1.0f;
    x = Clamp01(x);

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

static LRESULT CALLBACK ToggleSwitchOnly_SubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_TIMER:
        if (wParam == 1) { ToggleAnim_Tick(hBtn); return 0; }
        break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (!ToggleHitTest_SwitchOnly(hBtn, pt)) { SetFocus(hBtn); return 0; }
        break;
    }

    case WM_SETCURSOR:
    {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hBtn, &pt);
        if (ToggleHitTest_SwitchOnly(hBtn, pt)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
        break;
    }

    case WM_NCDESTROY:
    {
        KillTimer(hBtn, 1);
        if (auto* st = ToggleAnim_Get(hBtn)) { ToggleAnim_Set(hBtn, nullptr); delete st; }
        RemoveWindowSubclass(hBtn, ToggleSwitchOnly_SubclassProc, 2);
        break;
    }
    }
    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

// ---------- Morphing Implementation ----------
static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static float EaseInOut(float t)
{
    t = Clamp01(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static void StopMorphTimer()
{
    if (g_kspParent) KillTimer(g_kspParent, MORPH_TIMER_ID);
}

void Ksp_StartCurveMorph(const KeyDeadzone& newKs, bool animate)
{
    KeyDeadzone visualNow = Ksp_GetVisualCurve();

    g_kspMorph.fromKs = visualNow;
    g_kspMorph.toKs = newKs;

    if (!animate)
    {
        g_kspMorph.running = false;
        g_kspMorph.fromKs = newKs;
        StopMorphTimer();
    }
    else
    {
        g_kspMorph.running = true;
        g_kspMorph.startTick = GetTickCount();
        g_kspMorph.durationMs = 250;
        if (g_kspParent) SetTimer(g_kspParent, MORPH_TIMER_ID, 15, nullptr);
    }

    if (g_kspParent)
    {
        RECT gr{};
        if (KeySettingsPanel_GetGraphRect(g_kspParent, &gr))
            InvalidateRect(g_kspParent, &gr, FALSE);
    }
}

KeyDeadzone Ksp_GetVisualCurve()
{
    if (!g_kspMorph.running)
        return Ksp_GetActiveSettings();

    DWORD now = GetTickCount();
    DWORD dt = now - g_kspMorph.startTick;
    float t = (g_kspMorph.durationMs > 0) ? (float)dt / (float)g_kspMorph.durationMs : 1.0f;
    t = Clamp01(t);

    if (t >= 1.0f)
    {
        g_kspMorph.running = false;
        StopMorphTimer();
        return g_kspMorph.toKs;
    }

    float e = EaseInOut(t);

    const KeyDeadzone& a = g_kspMorph.fromKs;
    const KeyDeadzone& b = g_kspMorph.toKs;

    KeyDeadzone out = b;
    out.low = Lerp(a.low, b.low, e);
    out.high = Lerp(a.high, b.high, e);
    out.antiDeadzone = Lerp(a.antiDeadzone, b.antiDeadzone, e);
    out.outputCap = Lerp(a.outputCap, b.outputCap, e);
    out.cp1_x = Lerp(a.cp1_x, b.cp1_x, e);
    out.cp1_y = Lerp(a.cp1_y, b.cp1_y, e);
    out.cp2_x = Lerp(a.cp2_x, b.cp2_x, e);
    out.cp2_y = Lerp(a.cp2_y, b.cp2_y, e);
    out.cp1_w = Lerp(a.cp1_w, b.cp1_w, e);
    out.cp2_w = Lerp(a.cp2_w, b.cp2_w, e);

    return out;
}

// ---------- Logic ----------
static void EnsureOverrideOnForSelectedKey(HWND parent)
{
    if (!Ksp_IsKeySelected()) return;
    if (Ksp_IsOverrideOnForKey()) return;

    SendMessageW(g_kspChkUnique, BM_SETCHECK, BST_CHECKED, 0);
    ToggleAnim_Start(g_kspChkUnique, true, true);

    KeySettings_SetUseUnique(g_kspSelectedHid, true);
    Ksp_SyncUI();
    InvalidateRect(parent, nullptr, FALSE);
}

static void PromptNewPresetName(HWND owner, std::wstring& outName) { (void)owner; outName = L"New Preset"; }

void KeySettingsPanel_SetSelectedHid(uint16_t hid)
{
    KeyDeadzone visualNow = Ksp_GetVisualCurve();
    g_kspSelectedHid = hid;

    KeyDeadzone newTarget = Ksp_GetActiveSettings();

    ApplyAutoPresetOrCustom_OnKeySelection(newTarget);

    g_kspMorph.fromKs = visualNow;
    g_kspMorph.toKs = newTarget;
    g_kspMorph.running = true;
    g_kspMorph.startTick = GetTickCount();
    g_kspMorph.durationMs = 250;

    if (g_kspParent) SetTimer(g_kspParent, MORPH_TIMER_ID, 15, nullptr);

    Ksp_SyncUI();
    if (g_kspParent) InvalidateRect(g_kspParent, nullptr, FALSE);
}

void KeySettingsPanel_Create(HWND parent, HINSTANCE hInst)
{
    g_kspParent = parent;
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    int x = S(parent, 12);
    int w = S(parent, 520);
    int toggleH = S(parent, 26);

    g_kspChkUnique = CreateWindowW(L"BUTTON", L"Override global settings for this key",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
        x, S(parent, 12), w, toggleH, parent, (HMENU)KSP_ID_UNIQUE, hInst, nullptr);
    SendMessageW(g_kspChkUnique, WM_SETFONT, (WPARAM)hFont, TRUE);

    g_kspChkInvert = CreateWindowW(L"BUTTON", L"Invert axis (press to release)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
        x, S(parent, 40), w, toggleH, parent, (HMENU)KSP_ID_INVERT, hInst, nullptr);
    SendMessageW(g_kspChkInvert, WM_SETFONT, (WPARAM)hFont, TRUE);

    SetWindowSubclass(g_kspChkUnique, ToggleSwitchOnly_SubclassProc, 2, 0);
    SetWindowSubclass(g_kspChkInvert, ToggleSwitchOnly_SubclassProc, 2, 0);

    ToggleAnim_Start(g_kspChkUnique, false, false);
    ToggleAnim_Start(g_kspChkInvert, false, false);

    SetWindowPos(g_kspChkUnique, nullptr, x, S(parent, 12), w, toggleH, SWP_NOZORDER);
    SetWindowPos(g_kspChkInvert, nullptr, x, S(parent, 40), w, toggleH, SWP_NOZORDER);

    g_kspTxtInfo = CreateWindowW(L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, x, S(parent, 70), w, S(parent, 20), parent, nullptr, hInst, nullptr);
    SendMessageW(g_kspTxtInfo, WM_SETFONT, (WPARAM)hFont, TRUE);

    int yControls = S(parent, 252);
    const int labelGap = S(parent, 8);
    const int comboH = S(parent, 28);
    const int comboLeftNudge = S(parent, 8);
    const int modeLabelW = S(parent, 45);
    const int modeComboW = S(parent, 160);
    const int profileLabelW = S(parent, 64);
    const int profileComboW = S(parent, 200);

    int cx = x;
    g_kspLblMode = CreateWindowW(L"STATIC", L"Mode:", WS_CHILD | WS_VISIBLE,
        cx, yControls + S(parent, 6), modeLabelW, S(parent, 20), parent, nullptr, hInst, nullptr);
    SendMessageW(g_kspLblMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    cx += modeLabelW + labelGap - comboLeftNudge;

    g_kspComboMode = PremiumCombo::Create(parent, hInst,
        cx, yControls, modeComboW, comboH, KSP_ID_MODE, WS_CHILD | WS_VISIBLE | WS_TABSTOP);
    PremiumCombo::SetFont(g_kspComboMode, hFont, true);
    PremiumCombo::AddString(g_kspComboMode, L"Smooth (Bezier)");
    PremiumCombo::AddString(g_kspComboMode, L"Linear (Segments)");
    PremiumCombo::SetCurSel(g_kspComboMode, 0, false);
    PremiumCombo::SetDropMaxVisible(g_kspComboMode, 8);
    cx += modeComboW + S(parent, 20);

    g_kspLblProfile = CreateWindowW(L"STATIC", L"Preset:", WS_CHILD | WS_VISIBLE,
        cx, yControls + S(parent, 6), profileLabelW, S(parent, 20), parent, nullptr, hInst, nullptr);
    SendMessageW(g_kspLblProfile, WM_SETFONT, (WPARAM)hFont, TRUE);
    cx += profileLabelW + labelGap - comboLeftNudge;

    g_kspComboProfile = PremiumCombo::Create(parent, hInst,
        cx, yControls, profileComboW, comboH, KSP_ID_PROFILE, WS_CHILD | WS_VISIBLE | WS_TABSTOP);
    PremiumCombo::SetFont(g_kspComboProfile, hFont, true);
    PremiumCombo::SetDropMaxVisible(g_kspComboProfile, 10);

    // NEW: placeholder text shown when curSel == -1
    PremiumCombo::SetPlaceholderText(g_kspComboProfile, L"Custom");

    RefreshProfileCombo();

    // initial: no key selected => use global curve, attempt to auto-match or show Custom
    ApplyAutoPresetOrCustom_OnKeySelection(Ksp_GetActiveSettings());

    KeySettingsPanel_SetSelectedHid(0);
}

void KeySettingsPanel_HandleTimer(HWND parent, UINT_PTR timerId)
{
    if (timerId == MORPH_TIMER_ID)
    {
        RECT gr{};
        if (KeySettingsPanel_GetGraphRect(parent, &gr))
            InvalidateRect(parent, &gr, FALSE);

        if (!g_kspMorph.running) StopMorphTimer();
    }
}

bool KeySettingsPanel_HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam)
{
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == KSP_ID_UNIQUE)
    {
        if (!Ksp_IsKeySelected()) return true;

        bool currentlyOn = Ksp_IsOverrideOnForKey();
        bool on = !currentlyOn;

        SendMessageW(g_kspChkUnique, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        ToggleAnim_Start(g_kspChkUnique, on, true);

        KeySettings_SetUseUnique(g_kspSelectedHid, on);

        UpdateDirtyIcon_ForCurrentSelection(Ksp_GetActiveSettings());

        Ksp_SyncUI();
        InvalidateRect(parent, nullptr, FALSE);
        Ksp_RequestSave(parent);
        return true;
    }

    if (id == KSP_ID_INVERT)
    {
        bool currentlyOn = Ksp_GetActiveSettings().invert;
        bool on = !currentlyOn;

        SendMessageW(g_kspChkInvert, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
        ToggleAnim_Start(g_kspChkInvert, on, true);

        KeyDeadzone ks = Ksp_GetActiveSettings();
        ks.invert = on;

        Ksp_StartCurveMorph(ks, true);
        Ksp_SaveActiveSettings(ks);

        UpdateDirtyIcon_ForCurrentSelection(ks);

        Ksp_SyncUI();
        Ksp_RequestSave(parent);
        return true;
    }

    if (id == KSP_ID_MODE && code == CBN_SELCHANGE)
    {
        int sel = PremiumCombo::GetCurSel(g_kspComboMode);
        if (sel < 0) sel = 0;
        if (sel > 1) sel = 1;

        // BUG #3 FIX: morph must start BEFORE storage changes, otherwise from==to and it snaps.
        KeyDeadzone target = Ksp_GetActiveSettings();
        target.curveMode = (uint8_t)((sel == 0) ? 0 : 1);

        Ksp_StartCurveMorph(target, true);
        Ksp_SaveActiveSettings(target);

        UpdateDirtyIcon_ForCurrentSelection(target);

        InvalidateRect(parent, nullptr, FALSE);
        Ksp_SyncUI();
        Ksp_RequestSave(parent);
        return true;
    }

    if (id == KSP_ID_PROFILE && code == CBN_SELCHANGE)
    {
        int sel = PremiumCombo::GetCurSel(g_kspComboProfile);
        int count = PremiumCombo::GetCount(g_kspComboProfile);

        // last row is "+ Create New Preset..."
        if (count > 0 && sel == count - 1)
        {
            // Config page normally intercepts this and starts inline create.
            // Here we just ensure no Save icon.
            PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
            KeyboardProfiles::SetDirty(false);
            return true;
        }

        if (sel >= 0 && sel < (int)g_profileList.size())
        {
            KeyDeadzone preset{};
            if (KeyboardProfiles::LoadPreset(g_profileList[sel].path, preset))
            {
                EnsureOverrideOnForSelectedKey(parent);

                Ksp_StartCurveMorph(preset, true);
                Ksp_SaveActiveSettings(preset);

                KeyboardProfiles::SetActiveProfileName(g_profileList[sel].name);
                KeyboardProfiles::SetDirty(false);
                PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);

                Ksp_SyncUI();
                InvalidateRect(parent, nullptr, FALSE);
                Ksp_RequestSave(parent);
            }
            return true;
        }

        // sel == -1 => Custom placeholder, no Save icon
        PremiumCombo::SetExtraIcon(g_kspComboProfile, PremiumCombo::ExtraIconKind::None);
        KeyboardProfiles::SetDirty(false);
        KeyboardProfiles::SetActiveProfileName(L"");
        return true;
    }

    if (id == 9999)
    {
        RefreshProfileCombo();

        // After list change, re-auto-select on current curve (as per your spec)
        ApplyAutoPresetOrCustom_OnKeySelection(Ksp_GetActiveSettings());
        return true;
    }

    (void)lParam;
    return false;
}

bool KeySettingsPanel_HandleMouse(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bool res = Ksp_GraphHandleMouse(parent, msg, wParam, lParam);

    // BUG #2 will be fixed in the next file: we will ensure hint updates immediately.
    // For now, we at least ensure the page repaints on wheel events.
    if (res && msg == WM_MOUSEWHEEL)
        InvalidateRect(parent, nullptr, FALSE);

    if (res && (msg == WM_LBUTTONUP || msg == WM_MOUSEWHEEL))
    {
        Ksp_StartCurveMorph(Ksp_GetActiveSettings(), false);
        UpdateDirtyIcon_ForCurrentSelection(Ksp_GetActiveSettings());
    }

    return res;
}

bool KeySettingsPanel_HandleKey(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;

    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (!ctrl) return false;

    if (wParam == 'Z' || wParam == 'Y')
    {
        bool redo = (wParam == 'Y') || (wParam == 'Z' && shift);
        bool ok = redo ? Ksp_Redo() : Ksp_Undo();

        if (ok)
        {
            KeyDeadzone ks = Ksp_GetActiveSettings();
            Ksp_StartCurveMorph(ks, true);

            UpdateDirtyIcon_ForCurrentSelection(ks);

            Ksp_SyncUI();
            InvalidateRect(parent, nullptr, FALSE);
            Ksp_RequestSave(parent);
        }
        return true;
    }
    return false;
}

void KeySettingsPanel_DrawGraph(HDC hdc, const RECT& rc)
{
    Ksp_GraphDraw(hdc, rc);
}

bool KeySettingsPanel_HandleDrawItem(const DRAWITEMSTRUCT* dis)
{
    return Ksp_StyleHandleDrawItem(dis);
}

bool KeySettingsPanel_HandleMeasureItem(MEASUREITEMSTRUCT* mis)
{
    if (mis->CtlID == KSP_ID_PROFILE || mis->CtlID == KSP_ID_MODE)
    {
        HWND ref = g_kspParent ? g_kspParent : GetActiveWindow();
        mis->itemHeight = (UINT)std::clamp(S(ref, 28), 20, 44);
        return true;
    }
    return Ksp_StyleHandleMeasureItem(mis);
}