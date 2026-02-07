#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <cstdint>

#include <objidl.h>
#include <gdiplus.h>

#include "key_settings.h"

static constexpr int KSP_ID_UNIQUE = 6001;
static constexpr int KSP_ID_INVERT = 6002;
static constexpr int KSP_ID_CURVE = 6004;
static constexpr int KSP_ID_MODE = 6005;

// New profile/preset ID
static constexpr int KSP_ID_PROFILE = 6006;

extern HWND     g_kspParent;
extern uint16_t g_kspSelectedHid;

extern HWND g_kspChkUnique;
extern HWND g_kspChkInvert;
extern HWND g_kspTxtInfo;
extern HWND g_kspLblCurve;
extern HWND g_kspComboCurve;
extern HWND g_kspLblMode;
extern HWND g_kspComboMode;
extern HWND g_kspLblProfile;
extern HWND g_kspComboProfile;

// toggle animation shared state (stored as a window property on the BUTTON HWND)
static constexpr const wchar_t* KSP_TOGGLE_ANIM_PROP = L"KspToggleAnimPtr";

struct KspToggleAnimState
{
    float t = 0.0f;            // current [0..1]
    float from = 0.0f;         // anim start
    float to = 0.0f;           // anim target
    DWORD startTick = 0;
    DWORD durationMs = 140;
    bool running = false;

    bool initialized = false;  // first BM_SETCHECK sets without animation
    bool checked = false;      // last known target state
};

// NEW: Curve Morph Animation State
struct KspCurveMorphState
{
    bool running = false;
    DWORD startTick = 0;
    DWORD durationMs = 250; // morph duration

    // Snapshot of curve BEFORE change
    KeyDeadzone fromKs{};
    // Snapshot of curve AFTER change (target)
    KeyDeadzone toKs{};
};

extern KspCurveMorphState g_kspMorph;

// Triggers morph animation from current visual state to 'newKs'.
// If 'animate' is false, snaps immediately.
void Ksp_StartCurveMorph(const KeyDeadzone& newKs, bool animate = true);

// Returns currently interpolated curve for drawing.
// If not animating, returns target (active) settings.
KeyDeadzone Ksp_GetVisualCurve();

bool      Ksp_IsKeySelected();
bool      Ksp_IsOverrideOnForKey();
bool      Ksp_EditingUniqueKey();

enum class KspGraphMode : int { SmoothBezier = 0, LinearSegments = 1 };
KspGraphMode Ksp_GetActiveMode();
void         Ksp_SetActiveMode(KspGraphMode m);

enum class KspCurvePreset : int { Linear = 0, Dynamic, Precision, Aggressive, Instant, Custom };
KspCurvePreset Ksp_GetSelectedPreset();
void           Ksp_SetSelectedPreset(KspCurvePreset p);
void           Ksp_ApplyPresetToActive(KspCurvePreset p);

KeyDeadzone Ksp_GetActiveSettings();
void        Ksp_SaveActiveSettings(const KeyDeadzone& ks);
void        Ksp_SyncUI();
void        Ksp_RequestSave(HWND parent);

bool Ksp_GraphHandleMouse(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam);
void Ksp_GraphDraw(HDC hdc, const RECT& rc);

bool Ksp_StyleHandleDrawItem(const DRAWITEMSTRUCT* dis);
bool Ksp_StyleHandleMeasureItem(MEASUREITEMSTRUCT* mis);

void Ksp_StyleInstallCombo(HWND hCombo);