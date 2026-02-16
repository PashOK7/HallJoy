// settings.h
#pragma once
#include <windows.h>

// Input deadzones for analog key readings (0..1):
// - Low: everything below becomes 0, remaining range is rescaled
// - High: everything above becomes 1
void Settings_SetInputDeadzoneLow(float v01);   // 0..0.99 typically
float Settings_GetInputDeadzoneLow();

void Settings_SetInputDeadzoneHigh(float v01);  // 0.01..1.0 typically
float Settings_GetInputDeadzoneHigh();

// Global curve vertical endpoints (for graph / backend curve):
// - AntiDeadzone: output floor at Start point (Y0)
// - OutputCap: output ceiling at End point (Y3)
void Settings_SetInputAntiDeadzone(float v01);  // 0..0.99
float Settings_GetInputAntiDeadzone();

void Settings_SetInputOutputCap(float v01);     // 0.01..1.0
float Settings_GetInputOutputCap();

// Global Bezier control points (CP1/CP2) in normalized [0..1] graph space.
void Settings_SetInputBezierCp1X(float v01); // 0..1
float Settings_GetInputBezierCp1X();

void Settings_SetInputBezierCp1Y(float v01); // 0..1
float Settings_GetInputBezierCp1Y();

void Settings_SetInputBezierCp2X(float v01); // 0..1
float Settings_GetInputBezierCp2X();

void Settings_SetInputBezierCp2Y(float v01); // 0..1
float Settings_GetInputBezierCp2Y();

// Global Bezier control point weights (0..1)
void Settings_SetInputBezierCp1W(float v01); // 0..1
float Settings_GetInputBezierCp1W();

void Settings_SetInputBezierCp2W(float v01); // 0..1
float Settings_GetInputBezierCp2W();

// Global curve mode (must match UI modes)
// 0 = Smooth (Bezier)
// 1 = Linear (Segments)
void Settings_SetInputCurveMode(UINT mode);
UINT Settings_GetInputCurveMode();

// Global invert (x = 1 - x) before curve evaluation.
void Settings_SetInputInvert(bool on);
bool Settings_GetInputInvert();

// Apply current input deadzones to value in [0..1]
float Settings_ApplyInputDeadzones(float v01);

// Keyboard polling / update tick
void Settings_SetPollingMs(UINT ms); // 1..20
UINT Settings_GetPollingMs();

// UI refresh timer interval (ms)
void Settings_SetUIRefreshMs(UINT ms); // 1..200
UINT Settings_GetUIRefreshMs();

// Number of virtual X360 gamepads to expose through ViGEm (1..4).
void Settings_SetVirtualGamepadCount(int count);
int Settings_GetVirtualGamepadCount();
void Settings_SetVirtualGamepadsEnabled(bool on);
bool Settings_GetVirtualGamepadsEnabled();

// ---------------- NEW: Snappy Joystick ----------------
//
// Behavior (per axis):
// - If both directions are pressed, use the larger value (no cancellation).
// - If values are equal, use the direction that was pressed last.
void Settings_SetSnappyJoystick(bool on);
bool Settings_GetSnappyJoystick();

// Last Key Priority for opposite directions on the same axis.
// When both directions are pressed, the most recently pressed direction wins.
void Settings_SetLastKeyPriority(bool on);
bool Settings_GetLastKeyPriority();
void Settings_SetLastKeyPrioritySensitivity(float v01); // 0.02..0.95
float Settings_GetLastKeyPrioritySensitivity();

// Block physical keyboard events for keys that are currently bound to gamepad inputs.
void Settings_SetBlockBoundKeys(bool on);
bool Settings_GetBlockBoundKeys();

// Allow digital compatibility fallback when analog SDK stream is unavailable.
void Settings_SetDigitalFallbackInput(bool on);
bool Settings_GetDigitalFallbackInput();

// Aula native HID communication strategy.
enum SettingsAulaCommMode : UINT
{
    SettingsAulaCommMode_98Only = 0,               // stable digital+analog stream
    SettingsAulaCommMode_94Passive = 1,            // parse incoming 0x94, no active 0x94 writes
    SettingsAulaCommMode_94ActiveExperimental = 2, // actively bootstrap/poll 0x94 (can disable digital on some firmware)
};
void Settings_SetAulaCommMode(UINT mode);
UINT Settings_GetAulaCommMode();

// Block physical mouse events when mouse->stick mode is active.
void Settings_SetBlockMouseInput(bool on);
bool Settings_GetBlockMouseInput();

// Mouse -> gamepad stick emulation
// When enabled, mouse delta is mapped to selected stick axes.
void Settings_SetMouseToStickEnabled(bool on);
bool Settings_GetMouseToStickEnabled();

// 0 = Left stick, 1 = Right stick
void Settings_SetMouseToStickTarget(int target);
int Settings_GetMouseToStickTarget();

// Sensitivity multiplier in range [0.1 .. 8.0]
void Settings_SetMouseToStickSensitivity(float v);
float Settings_GetMouseToStickSensitivity();

// Error-to-stick response gain/shape [0.2 .. 3.0], 1.0 = default
void Settings_SetMouseToStickAggressiveness(float v);
float Settings_GetMouseToStickAggressiveness();

// Maximum target offset relative to radius [0.0 .. 6.0], 2.5 = default.
// Controls only "distance from center" behavior, not directional assist.
void Settings_SetMouseToStickMaxOffset(float v);
float Settings_GetMouseToStickMaxOffset();

// Follower speed multiplier [0.2 .. 3.0], 1.0 = default
void Settings_SetMouseToStickFollowSpeed(float v);
float Settings_GetMouseToStickFollowSpeed();

// ---------------- DEBUG / TUNING (temporary) ----------------
// All sizes are in "96-DPI pixels" (unscaled). UI scales them via WinUtil_ScalePx().

void Settings_SetRemapButtonSizePx(UINT px);    // size of icon buttons on Remap page
UINT Settings_GetRemapButtonSizePx();

void Settings_SetDragIconSizePx(UINT px);       // size of ghost icon while dragging
UINT Settings_GetDragIconSizePx();

void Settings_SetBoundKeyIconSizePx(UINT px);   // size of icon drawn on keyboard key when bound (square)
UINT Settings_GetBoundKeyIconSizePx();

void Settings_SetBoundKeyIconBacking(bool on);  // extra opaque backing plate under icon (bound keys)
bool Settings_GetBoundKeyIconBacking();

// Main window size (pixels in current process DPI context).
void Settings_SetMainWindowWidthPx(int px);
int Settings_GetMainWindowWidthPx();

void Settings_SetMainWindowHeightPx(int px);
int Settings_GetMainWindowHeightPx();

// Main window top-left position in virtual-screen coordinates.
// INT_MIN means "not set yet".
void Settings_SetMainWindowPosXPx(int px);
int Settings_GetMainWindowPosXPx();

void Settings_SetMainWindowPosYPx(int px);
int Settings_GetMainWindowPosYPx();
