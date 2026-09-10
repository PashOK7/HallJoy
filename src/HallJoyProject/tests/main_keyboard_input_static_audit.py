"""Wiring guard; behavioural coverage lives in the private-desktop event suite."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]/'HallJoy'
app = (root/'app.cpp').read_text(encoding='utf-8-sig')
ui = (root/'keyboard_subpages.cpp').read_text(encoding='utf-8-sig')
policy = (root/'main_keyboard_input.h').read_text(encoding='utf-8-sig')
gate = app.index('if (!halljoy::main_input::Allow(msg,hwnd)) continue;')
assert gate < app.index('TranslateMessage(&msg);') < app.index('DispatchMessageW(&msg);')
assert 'WM_KEYFIRST' in policy and 'WM_KEYLAST' in policy
assert 'GetFocus() != message.hwnd' in policy
assert 'KeyboardLayoutEditorHost' in policy and 'GW_OWNER' in policy
assert 'OverlayCustom_IsEdit(st->focusId) ? 1 : 0' in ui
assert 'st && st->blockShortcutCapturing ? 1 : 0' in ui
config = ui[ui.index('LRESULT CALLBACK KeyboardSubpages_ConfigPageProc'):]
assert 'KeySettingsPanel_HandleKey(hWnd' not in config
assert 'Ctrl+S = save preset' not in config
assert 'main window keyboard admission checks failed' in ui
render = (root/'keyboard_render.cpp').read_text(encoding='utf-8-sig')
assert 'SelectClipPath(hdc,RGN_AND)' in render
assert 'if (shapeDC) RestoreDC(hdc,shapeDC);' in render
print('MAIN_KEYBOARD_INPUT_STATIC_AUDIT=PASS')
