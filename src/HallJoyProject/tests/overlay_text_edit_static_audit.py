"""Verify active retained Input Overlay editor wiring, not obsolete HWND code."""
from pathlib import Path
hall = Path(__file__).resolve().parents[1] / "HallJoy"
source = (hall / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
start = source.index("static LRESULT OverlayCustom_PageProc")
page = source[start:source.index("LRESULT CALLBACK KeyboardSubpages_InputOverlayPageProc", start)]
assert "s.pop_back()" not in page and "s.push_back" not in page
for event in ("WM_CONTEXTMENU", "WM_LBUTTONDBLCLK", "WM_KILLFOCUS", "WM_GETDLGCODE"):
    assert event in page
assert "GetFocus() == hWnd && OverlayCustom_IsEdit(st->focusId)" in page
assert "if (wParam < 32 || (GetKeyState(VK_CONTROL) & 0x8000)) return 0;" in page
assert "st->selectingText && GetCapture() == hWnd" in page
assert "st->editor.MoveTo(OverlayCustom_EditHit" in page
apply = source[source.index("static void OverlayCustom_ApplyEdit"):source.index("static size_t OverlayCustom_EditHit")]
assert "halljoy::overlay_edit::Value" in apply
assert "if (changed)" in apply and "OverlayCustom_RequestSave" in apply
assert "RegisterHotKey" not in page and 'CreateWindowW(L"EDIT"' not in page
assert "OverlayCustom_DrawEditFeedback(hWnd, memDC, st);" in page
assert "GlobalSize(data)" in source and "length < 128" in source
assert "Enter a port from 1 to 65535 before starting." in source
print("OVERLAY_TEXT_EDIT_STATIC_AUDIT=PASS")
