"""Integration guards for visual-only, repeatable Remap onboarding."""
from pathlib import Path

hall = Path(__file__).resolve().parents[1] / "HallJoy"
source = (hall / "remap_panel.cpp").read_text(encoding="utf-8-sig")
keys = (hall / "keyboard_page_main.cpp").read_text(encoding="utf-8-sig")
start = source.index("static bool Remap_HasAnyBindings()")
hint = source[start:source.index("static LRESULT CALLBACK IconSubclassProc", start)]
for forbidden in ("BindingActions_Apply", "BindingActions_Append", "SaveBindings", "SetCapture(", "Settings_Set", "SetDragHoverHid"):
    assert forbidden not in hint, forbidden
for guard in ("BINDINGS_MAX_GAMEPADS", "Bindings_GetAxisForPad", "Bindings_GetTriggerForPad", "Bindings_GetButtonForPad",
              "GetCapture()", "Remap_HasAnyBindings()", "st->hintActive", "IsWindowVisible(panel)",
              "GetForegroundWindow()", "EqualRect", "KillTimer(panel, BIND_HINT_TIMER_ID)", "frame.finished"):
    assert guard in hint, guard
assert "Remap_StopBindingHint(hPanel, st);" in source[source.index("static LRESULT CALLBACK IconSubclassProc"):]
assert "msg == WM_MOUSEWHEEL || msg == WM_VSCROLL || msg == WM_CANCELMODE" in source
assert "case WM_SHOWWINDOW:\n        if (!wParam) Remap_StopBindingHint" in source
assert "case WM_DESTROY:\n        Remap_StopBindingHint" in source
click = keys[keys.index("if (msg == WM_LBUTTONUP && g_activeSubTab == 0"):]
assert click.index("DefSubclassProc(") < click.index("RemapPanel_ShowBindingHint(")
assert "WS_EX_TRANSPARENT | WS_EX_NOACTIVATE" in source
assert "(capture && !previewCapture)" in hint
assert "GetParent(capture) == st->hKeyboardHost" in hint
assert "== 0x1A) // HID W" in hint
assert "BindAction::Axis_LY_Plus) continue;" in hint
assert "if (!key) return;" in hint
assert hint.index("st->hintActive ||") < hint.index("st->hintStarted =")
print("REMAP_HINT_STATIC_AUDIT=PASS")
