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

// ---------------- NEW: Snappy Joystick ----------------
//
// Behavior (per axis):
// - If both directions are pressed, use the larger value (no cancellation).
// - If values are equal, use the direction that was pressed last.
void Settings_SetSnappyJoystick(bool on);
bool Settings_GetSnappyJoystick();

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