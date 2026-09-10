#!/usr/bin/env python3
"""RM-16 must expose only an owner-routed, non-blocking Pause/Resume UI."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
app = (root / "HallJoy" / "app.cpp").read_text(encoding="utf-8")
subpages = (root / "HallJoy" / "keyboard_subpages.cpp").read_text(encoding="utf-8")
owner = (root / "HallJoy" / "engine_runtime_owner.cpp").read_text(encoding="utf-8")
transaction = (root / "HallJoy" / "engine_runtime_transaction.h").read_text(encoding="utf-8")

for token in (
    "WM_APP_ENGINE_RUNTIME_TOGGLE",
    "g_enginePauseWasExplicit",
    "EngineRuntimeOwner_RequestPause()",
    "EngineRuntimeOwner_RequestResume()",
    "EngineRuntimeOwner_Start(BuildEngineRuntimeOperations(hwnd))",
    "EngineRuntimeOwner_Stop()",
    "g_engineUiInputPassThrough",
):
    assert token in app, token
for token in (
    "GLOB_ID_ENGINE_RUNTIME",
    "Global_EngineRuntimeButtonText",
    "Global_EngineRuntimeButtonEnabled",
    "WM_APP_ENGINE_RUNTIME_TOGGLE",
    "EngineRuntimeOwner_Snapshot()",
):
    assert token in subpages, token
assert transaction.index("operations.CloseAdmission(nativeError)") < transaction.index("operations.ReleaseUiInput(nativeError)")
assert transaction.index("operations.StopNativeProviders(nativeError)") < transaction.index("operations.ReleaseBackendLeases(nativeError)")
assert "std::try_to_lock" in owner
ui = (root / "HallJoy" / "keyboard_ui.cpp").read_text(encoding="utf-8")
assert "g_operations.stateChanged(g_operations.context)" in owner
assert "previousState != static_cast<std::uint8_t>(snapshot.state)" in owner
assert "PostMessageW(static_cast<HWND>(context), WM_APP_ENGINE_RUNTIME_STATE_CHANGED" in app
assert "KeyboardUI_OnEngineStateChanged();" in app
state_handler = subpages.split("if (msg == WM_APP_ENGINE_RUNTIME_STATE_CHANGED)", 1)[1].split("return 0;", 1)[0]
assert "CustomPageSurface_MarkDirty" in state_handler
assert "InvalidateRect(g_hPageGlobal, nullptr, FALSE)" not in ui
assert 'L"Resume HallJoy"' in subpages and 'L"Paused"' in subpages
assert "x + S(hWnd, 210), y + S(hWnd, 32)" in subpages
assert "Live diagnostics: Gamepad Tester" not in subpages
assert 'SetTimer(hWnd, GLOBAL_PAUSE_PULSE_TIMER, 33, nullptr)' in subpages
assert 'IsIconic(GetAncestor(hWnd, GA_ROOT))' in subpages
assert 'IntersectRect(&intersection, &client, &glow)' in subpages
pulse = subpages.split('if (st && wParam == GLOBAL_PAUSE_PULSE_TIMER)', 1)[1].split('if (st && wParam == TOAST_TIMER_ID)', 1)[0]
assert 'InvalidateRect(hWnd, &rc, FALSE)' in pulse
assert 'CustomPageSurface_MarkDirty' not in pulse
assert 'Global_DrawPulse(hWnd, memDC, st)' in subpages
print("Runtime pause UI static audit passed")
