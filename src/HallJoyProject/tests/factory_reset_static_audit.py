#!/usr/bin/env python3
"""Static safety/UI gate for HallJoy's recoverable factory reset."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


factory = read(HALL / "factory_reset.cpp")
header = read(HALL / "factory_reset.h")
app = read(HALL / "app.cpp")
main = read(HALL / "main.cpp")
subpages = read(HALL / "keyboard_subpages.cpp")
keyboard_page = read(HALL / "keyboard_page_main.cpp")
remap = read(HALL / "remap_panel.cpp")
project = read(HALL / "HallJoy.vcxproj")
filters = read(HALL / "HallJoy.vcxproj.filters")

for leaf in ("settings.ini", "bindings.ini", "GlobalProfiles", "Layouts", "CurvePresets"):
    require(factory, f'L"{leaf}"', f"reset transaction includes {leaf}")

require(factory, "IniUtil_SaveAtomic(", "reset request marker is atomically persisted")
require(factory, "RequestValidate", "reset request marker is parsed back before commit")
require(factory, "FILE_ATTRIBUTE_REPARSE_POINT", "reset refuses reparse-point targets")
require(factory, "MOVEFILE_WRITE_THROUGH", "state moves and rollback are write-through")
require(factory, "moved.rbegin()", "partial moves roll back in reverse order")
require(factory, "rollbackComplete", "rollback completion is reported truthfully")
require(factory, "apply.rollback", "rollback result is written to stability evidence")
require(factory, "apply.commit", "successful reset is written to stability evidence")
require(factory, "#if defined(HALLJOY_ANALOG_SIMULATOR)", "fault injection is simulator-only")
require(factory, "--halljoy-test-factory-reset-fail-after-three-moves", "runtime gate can force a partial-move rollback")

if "RemoveDirectoryW(root.c_str())" in factory or "SHFileOperation" in factory:
    raise SystemExit("FAIL: factory reset contains a broad recursive/deletion operation")
print("PASS: factory reset has no broad recursive delete")

delete_marker = factory.find("DeleteFileW(requestPath.c_str())")
create_defaults = factory.find("createdDirectories.push_back(directory)")
if delete_marker < 0 or create_defaults < 0 or delete_marker < create_defaults:
    raise SystemExit("FAIL: reset request marker is removed before fresh directories exist")
print("PASS: marker is removed only after fresh state directories exist")

apply_position = app.find("FactoryReset_ApplyPending()")
load_position = app.find("SettingsIni_Load(")
if apply_position < 0 or load_position < 0 or apply_position > load_position:
    raise SystemExit("FAIL: pending reset is not applied before settings load")
print("PASS: pending reset applies before any settings load")

for marker, description in (
    ("App_TakeRelaunchRequest()", "shutdown consumes the relaunch request"),
    ("StabilityTrace_Shutdown(result)", "stability trace shuts down before relaunch"),
    ("App_DisarmShutdownWatchdog()", "shutdown watchdog is disarmed before relaunch"),
    ("App_RelaunchSelf()", "the clean process performs the relaunch"),
):
    require(main, marker, description)

ordered = [
    main.find("App_TakeRelaunchRequest()"),
    main.find("StabilityTrace_Shutdown(result)"),
    main.find("App_DisarmShutdownWatchdog()"),
    main.find("App_RelaunchSelf()"),
]
if any(position < 0 for position in ordered) or ordered != sorted(ordered):
    raise SystemExit("FAIL: restart can race final shutdown/log teardown")
print("PASS: restart occurs only after complete shutdown/log teardown")

global_start = subpages.find("// Global settings page")
global_end = subpages.find("// Mouse settings page", global_start + 40)
if global_start < 0 or global_end < 0:
    raise SystemExit("FAIL: Global settings page boundaries were not found")
global_page = subpages[global_start:global_end]

for marker, description in (
    ("Reset All Settings", "Global settings exposes the reset action"),
    ("BS_OWNERDRAW", "reset action uses the existing owner-draw button system"),
    ("Global_DrawActionButton", "reset action shares the Global button renderer"),
    ("MB_YESNO", "reset requires explicit confirmation"),
    ("MB_DEFBUTTON2", "destructive confirmation defaults to No"),
    ("FactoryReset_Request", "confirmed action persists the reset request"),
    ("WM_APP_FACTORY_RESET_RESTART", "confirmed action requests graceful restart"),
    ("CustomPageSurface surface", "Global settings uses the shared scrollbar surface"),
    ("CustomPageSurface_DrawScrollbar", "Global settings draws the common themed scrollbar"),
    ("Global_SetScrollY", "Global settings owns bounded scroll state"),
    ("WM_MOUSEWHEEL", "Global settings supports wheel scrolling"),
    ("scrollDrag", "Global settings supports scrollbar thumb dragging"),
    ("danger ? RGB(108, 35, 43)", "danger action has a red resting fill"),
):
    require(global_page, marker, description)

for tab in ("Remap", "Configuration", "Gamepad Tester", "Global settings", "Input Overlay", "Mouse settings"):
    require(keyboard_page, f'L"{tab}"', f"tab inventory includes {tab}")

require(remap, "Remap_SetScrollY", "Remap owns themed scrolling")
require(global_page, "Global_SetScrollY", "Global settings owns themed scrolling")
require(subpages, "OverlayPage_SetScrollY", "Input Overlay owns themed scrolling")
require(subpages, "Config_SetScrollY", "Configuration owns themed scrolling")
require(subpages, "MouseCustom_PageProc", "Mouse settings owns themed scrolling")
require(subpages, "int cardH = std::max(1, availH / rows);", "Gamepad Tester adapts cards instead of overflowing")

if subpages.count("FactoryReset_Request(&error)") != 1:
    raise SystemExit("FAIL: factory reset UI action exists outside the intended Global page")
print("PASS: factory reset action exists only on the Global settings page")

for name in ("settings", "bindings", "profiles", "layouts", "curve presets"):
    if name not in global_page.lower():
        raise SystemExit(f"FAIL: confirmation does not disclose reset scope: {name}")
print("PASS: confirmation discloses the complete reset scope")

for source in (project, filters):
    require(source, 'factory_reset.cpp', "factory reset source is registered in the MSVC project")
    require(source, 'factory_reset.h', "factory reset header is registered in the MSVC project")

runner = REPO / "tools" / "run_factory_reset_test.ps1"
if not runner.is_file():
    raise SystemExit("FAIL: runtime factory reset runner is missing")
require(read(runner), "fail-after-three-moves", "runtime runner exercises partial rollback")
require(read(runner), "FactoryResetBackups", "runtime runner verifies the recoverable backup")

print("FACTORY_RESET_STATIC_AUDIT=PASS")
