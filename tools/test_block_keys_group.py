"""Source integration guards for the retained Configuration child group.

These check production event/layout wiring, not visual appearance.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "src/HallJoyProject/HallJoy/keyboard_subpages.cpp").read_text(encoding="utf-8-sig")


def body(marker, text=source):
    start = text.index("{", text.index(marker))
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


assert "if (!Settings_GetBlockBoundKeys()) return 0;" in body("static int Config_BlockOptionsSpace")
draw = source[source.index('L"Block Bound Keys", Settings_GetBlockBoundKeys(), true);'):]
group = body("if (Settings_GetBlockBoundKeys())", draw)
for control in ("Config_BlockAllowRect", "Config_BlockShortcutRect", "Config_BlockShortcutClearRect"):
    assert control in group
    mouse = body("static bool Config_HandleCustomControlsMouse")
    line = next(line for line in mouse.splitlines() if "hit(" + control in line)
    assert "Settings_GetBlockBoundKeys() &&" in line
assert "kText" not in group  # The latched privilege warning remains independent.
page = body("LRESULT CALLBACK KeyboardSubpages_ConfigPageProc")
for message in ("WM_APP_BLOCK_KEYS_CHANGED", "WM_APP_CONFIG_PROFILE_APPLIED"):
    event = body("if (msg == " + message + ")", page)
    assert "Config_RecalcContentHeight" in event and "Config_MarkSurfaceDirty" in event
toggle = body("if (LOWORD(wParam) == (UINT)ID_BLOCK_BOUND_KEYS", page)
assert "Config_RecalcContentHeight" in toggle
assert "Config_EndBlockShortcutCapture" in toggle
assert "st->blockShortcutCapturing && !Settings_GetBlockBoundKeys()" in page
assert "Ignore queued clicks belonging to controls that have just collapsed" in page
assert "Settings_SetBlockKeysHotkey" not in toggle  # Hiding never clears the binding.
print("BLOCK_KEYS_GROUP_SOURCE_TEST=PASS")
