"""Window placement integration and profile-isolation guardrails."""
from pathlib import Path
hall = Path(__file__).resolve().parents[1] / "HallJoy"
app = (hall / "app.cpp").read_text(encoding="utf-8-sig")
ini = (hall / "settings_ini.cpp").read_text(encoding="utf-8-sig")
windows = (hall / "window_placement_windows.h").read_text(encoding="utf-8-sig")
assert "IsWindowRectVisibleOnAnyScreen" not in app
assert "EnumDisplayMonitors" in windows and "info.rcWork" in windows
assert "ConvertWorkspace" in windows and "SetWindowPlacement" in windows
assert "WPF_RESTORETOMAXIMIZED" in windows
for event in ("WM_EXITSIZEMOVE", "WM_DISPLAYCHANGE", "WM_SETTINGCHANGE", "WM_QUERYENDSESSION"):
    assert event in app
assert "if (g_windowMoving) return 0;" in app
assert "windowSaved = SettingsIni_SaveWindow" in app
assert "g_windowPlacementDirty" in app and "g_profileReadyForAutosave && g_windowPlacementDirty" in app
for field in ("PlacementVersion", "Dpi", "Maximized"):
    assert f'L"{field}"' in ini
start = ini.index("if (context->kind == SettingsTransactionKind::WindowUpdate)")
branch = ini[start:ini.index("else if", start)]
assert "IniUtil_CopyExistingForUpdate" in branch and "SettingsIni_WriteWindow" in branch
assert "SettingsIni_Save_Internal" not in branch
assert "Settings_SetMainWindowPlacementMeta(winVersion, winDpi, winMaximized == 1);" in ini
print("WINDOW_PLACEMENT_STATIC_AUDIT=PASS")
