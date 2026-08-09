#!/usr/bin/env python3
"""Static regression guards for the v1.4 pre-release UI audit."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = (ROOT / "HallJoy" / "keyboard_ui.cpp").read_text(encoding="utf-8")
PAGE_MAIN = (ROOT / "HallJoy" / "keyboard_page_main.cpp").read_text(encoding="utf-8")
PAGES = (ROOT / "HallJoy" / "keyboard_subpages.cpp").read_text(encoding="utf-8")
COMBO = (ROOT / "HallJoy" / "premium_combo_core.cpp").read_text(encoding="utf-8")
COMBO_PAINT = (ROOT / "HallJoy" / "premium_combo_paint.cpp").read_text(encoding="utf-8")
COMBO_ANIM = (ROOT / "HallJoy" / "premium_combo_anim.cpp").read_text(encoding="utf-8")
TAB = (ROOT / "HallJoy" / "tab_dark.h").read_text(encoding="utf-8")
REMAP = (ROOT / "HallJoy" / "remap_panel.cpp").read_text(encoding="utf-8")
OVERLAY = (ROOT / "HallJoy" / "overlay_server.cpp").read_text(encoding="utf-8")
SETTINGS_INI = (ROOT / "HallJoy" / "settings_ini.cpp").read_text(encoding="utf-8")
KEY_SETTINGS = (ROOT / "HallJoy" / "keyboard_keysettings_panel.cpp").read_text(encoding="utf-8")
KEY_GRAPH = (ROOT / "HallJoy" / "keyboard_keysettings_panel_graph.cpp").read_text(encoding="utf-8")
SURFACE = (ROOT / "HallJoy" / "custom_page_surface.cpp").read_text(encoding="utf-8")


def section(text: str, begin: str, end: str) -> str:
    start = text.index(begin)
    stop = text.index(end, start)
    return text[start:stop]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


config_mouse = section(PAGES, "static bool Config_HandleCustomControlsMouse", "static void Config_OffsetAllChildren")
reset_button = section(PAGES, "static void Global_DrawActionButton", "static void Global_RequestSave")
timer = UI[UI.index("void KeyboardUI_OnTimerTick"):]
dirty_keys = section(timer, "for (int chunk", "// NEW: gear anim")

require("g_activeSubTab == 1 || g_activeSubTab == 2" in timer,
        "live telemetry must be gated to the two visible consumer tabs")
require("analogHash != s_lastAnalogHash" in timer,
        "unchanged telemetry must not schedule a repaint")
require("InvalidateDirtyBits(bits, chunk);" in dirty_keys and "g_activeSubTab" not in dirty_keys,
        "the always-visible keyboard preview must invalidate on every tab")
require(timer.index("// Gamepad bars are animation-rate UI") < timer.index("HashGamepadReports()"),
        "gamepad report sampling must run at UI cadence outside the 100 ms telemetry gate")
require("Config_CustomStatusRect" in PAGES and "InvalidateRect(hWnd, &status, FALSE)" in PAGES,
        "configuration telemetry must invalidate only its live status region")

config_cache = section(PAGES, "static void Config_RenderCacheContent", "static void Config_MarkSurfaceDirty")
config_proc = PAGES[PAGES.index("LRESULT CALLBACK KeyboardSubpages_ConfigPageProc"):]
config_paint = section(config_proc, "case WM_PAINT:", "case WM_CTLCOLORSTATIC:")
require("KeySettingsPanel_DrawGraphRetainedContent(hdc, rc)" in config_cache and
        "KeySettingsPanel_DrawGraph(" not in config_cache,
        "the Configuration retained page cache must never capture the live analog marker")
require("KeySettingsPanel_DrawGraphViewportOverlay(memDC, rc)" in config_paint and
        config_paint.index("CustomPageSurface_Present") <
        config_paint.index("KeySettingsPanel_DrawGraphViewportOverlay"),
        "the live graph layer must be composed after presenting retained content")
viewport_overlay = section(KEY_GRAPH, "static void DrawGraphViewportOverlay", "void Ksp_GraphDrawRetainedContent")
require(viewport_overlay.index("DrawLiveMarkerOverlay") < viewport_overlay.index("DrawHandle"),
        "graph handles must remain above the live marker after splitting retained and viewport layers")
config_marker_tick = section(UI, "static void TickConfigLiveMarker", "// NEW: gear animation invalidation")
require("KeySettingsPanel_GetGraphRect" in config_marker_tick and
        "Config_MarkSurfaceDirty" not in config_marker_tick,
        "live analog changes must invalidate only the graph viewport, never rebuild the page cache")

require("PremiumCombo::Create(hWnd" in PAGES and "ID_SPARK_POLL_MODE" in PAGES,
        "Spark poll mode must be a real PremiumCombo child")
require("Config_OpenPopupAnchor" in config_mouse and "ID_SPARK_POLL_MODE" in config_mouse,
        "retained Spark faces must open the real popup controller through content hit-testing")
require(PAGES.count("Config_DrawCustomCombo(") >= 3,
        "Spark closed faces must be part of the retained Configuration frame")
require("WM_GETDLGCODE" in COMBO and "VK_F4" in COMBO and "VK_UP" in COMBO,
        "PremiumCombo keyboard contract must include dialog arrows and open/navigation keys")
popup_proc = section(COMBO, "LRESULT CALLBACK PremiumComboInternal::PopupProc", "LRESULT CALLBACK PremiumComboInternal::ComboProc")
require("case WM_MOUSEWHEEL:" in popup_proc and
        "SendMessageW(st->hwnd, WM_MOUSEWHEEL, wParam, lParam)" in popup_proc,
        "PremiumCombo popup must route pointer wheel input to the controller-owned list scroll state")
combo_wheel = section(COMBO, "case WM_MOUSEWHEEL:", "case WM_CHAR:")
combo_logic = (ROOT / "HallJoy" / "premium_combo_logic.cpp").read_text(encoding="utf-8")
wheel_scroll = combo_logic[combo_logic.index("bool PremiumComboInternal::ScrollPopupWheel"):]
require("ScrollPopupWheel(st, delta)" in combo_wheel and "MoveHot" not in combo_wheel,
        "popup wheel input must scroll the list viewport rather than move the hot option")
require("GetMaxScrollTop(st)" in wheel_scroll and "if (maxTop <= 0)" in wheel_scroll and
        "st->scrollTop = nextTop" in wheel_scroll and "st->curSel" not in wheel_scroll and
        "st->hotIndex" not in wheel_scroll,
        "wheel scrolling must be overflow-only and must not change selection/highlight state")

require("DrawFocusRect" not in reset_button,
        "reset action must not use the dotted GDI focus rectangle")
require("accent{" not in reset_button,
        "reset action must not draw the rejected left accent strip")
require("PaintTabControlBuffered" in TAB and "CreateCompatibleBitmap" in TAB,
        "tab row must commit through a buffered paint path")
require("const bool selectionChanged" in TAB and
        "msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || selectionChanged" in TAB,
        "focused tab row must not repaint for unrelated keyboard input")

require("CustomPageSurface_HandleScrollMessage" in SURFACE and
        "CustomPageSurface_Present" in SURFACE and
        "CustomPageSurface_ClientToContent" in SURFACE,
        "one shared controller must own wheel, thumb, retained presentation, and content coordinates")
require(PAGES.count("CustomPageSurface_HandleScrollMessage") >= 12 and
        "CustomPageSurface_HandleScrollMessage" in REMAP,
        "Tester, Overlay, Global, Mouse, Configuration, and Remap must use the shared scroll controller")
require(PAGES.count("CustomPageSurface_Present") >= 4 and
        "CustomPageSurface_Present" in REMAP,
        "scroll-heavy settings pages must present the same retained cache viewport")
remap_layout = section(REMAP, "static void ApplyRemapSizing", "// ---------------- Drag tick")
require("DeferWindowPos" not in remap_layout and "SetWindowPos" not in remap_layout and
        "CustomPageSurface_SetContentHeight" in remap_layout,
        "Remap scrolling must update one surface rather than move its icon HWND grid")
global_layout = section(PAGES, "static void Global_Layout", "static void Global_RenderContent")
require("DeferWindowPos" not in global_layout and "SetWindowPos" not in global_layout and
        "rcGlobalProfile" in global_layout,
        "Global settings layout must retain content rectangles rather than move children")
require(PAGES.count("PremiumCombo::PaintRetainedFace") >= 3 and
        "PremiumCombo::PaintRetainedFace" in KEY_SETTINGS and
        "void PremiumCombo::PaintRetainedFace" in COMBO_PAINT,
        "all retained combo faces must reuse the exact PremiumCombo renderer")
require("GetFocus() == hwndCombo" not in COMBO_PAINT,
        "PremiumCombo must not add a second bright focus outline when its popup opens")
require("MsgDropStateChanged" in COMBO_ANIM and
        PAGES.count("msg == PremiumCombo::MsgDropStateChanged()") >= 2 and
        "ShowWindow((HWND)lParam, SW_HIDE)" in PAGES,
        "popup controller HWNDs must hide again when a retained combo closes")
require('L"  • unsaved"' not in PAGES and 'L"×"' not in REMAP and
        "DrawDisableGamepadPowerGlyph" in REMAP,
        "retained UI must use the save icon and vector power glyph instead of encoding-sensitive text")
remap_mouse_down = section(REMAP, "case WM_LBUTTONDOWN:", "case WM_MOUSEMOVE:")
require("Remap_IsRetainedIconHit(st, hit)" in remap_mouse_down and
        "if (hit >= REMAP_ICON_ID_BASE)" not in remap_mouse_down,
        "gamepad power hits must not be swallowed by the broader retained-icon ID range")
require("flatIndex >= 0 && flatIndex < (int)st->iconBtns.size()" in REMAP,
        "retained Remap icon hit classification must be bounded by the actual icon collection")

binding_transaction = section(UI, "bool KeyboardUI_SaveBindingsAfterUserChange", "static uint64_t HashAnalogTelemetry")
require("Profile_SaveIni(AppPaths_ActiveBindingsIni().c_str())" in binding_transaction and
        "GlobalProfiles_SetDirty(true)" in binding_transaction and
        "PostMessageW(g_hPageGlobal, WM_APP_KEYBOARD_GLOBAL_PROFILE_DIRTY" in binding_transaction,
        "a user binding mutation must persist and mark/refresh the active global profile atomically")
require("Profile_SaveIni(AppPaths_ActiveBindingsIni().c_str())" not in PAGE_MAIN and
        "Profile_SaveIni(AppPaths_ActiveBindingsIni().c_str())" not in REMAP and
        PAGE_MAIN.count("KeyboardUI_SaveBindingsAfterUserChange(") >= 8 and
        REMAP.count("KeyboardUI_SaveBindingsAfterUserChange(") >= 2,
        "Remap mutation paths must use the shared binding/profile dirty transaction")
require('GlobalProfiles_IsDirty() ? L"Global profile - unsaved"' in PAGES and
        "PremiumCombo::ExtraIconKind::Save" in PAGES,
        "dirty global profiles must expose both an unsaved status and the canonical save action")

overlay_activate = section(PAGES, "static void OverlayCustom_Activate", "static LRESULT OverlayCustom_PageProc")
require(PAGES.count("OverlayCustomKind::Combo") >= 6 and
        "OverlayCustom_InitCombos" in PAGES and
        "PremiumCombo::PaintRetainedFace(combo" in PAGES,
        "Input Overlay direction, depth, and font selectors must use retained PremiumCombo faces")
require("OverlayCustom_OpenComboAnchor(hWnd, st, id)" in overlay_activate and
        "OverlayServer_SetFillDirection" not in overlay_activate and
        "OverlayServer_SetUseRawDepth" not in overlay_activate and
        "OverlayServer_SetLabelFontIndex" not in overlay_activate,
        "Input Overlay selector clicks must open popups instead of cycling values")
require("HIWORD(wParam) == CBN_SELCHANGE" in PAGES and
        "combo == OverlayCustom_ComboForId(st, id)" in PAGES,
        "Input Overlay values must commit only from real combo selection notifications")

spark_combos = section(PAGES, "static bool Config_UpdateSparkCombos(HWND hWnd, ConfigPageState* st)\n{",
                       "static void Config_SetCustomChildrenVisible")
require("DeferWindowPos" not in spark_combos and "SetWindowPos" not in spark_combos and
        "ShowWindow(combo, SW_HIDE)" in spark_combos,
        "Configuration Spark controls must remain hidden popup anchors during scrolling")
key_combo_layout = section(KEY_SETTINGS, "void KeySettingsPanel_UpdateCustomControlsLayout", "void KeySettingsPanel_DrawControls")
require("DeferWindowPos" not in key_combo_layout and "SetWindowPos" not in key_combo_layout and
        "KeySettingsPanel_CloseCustomPopups" in key_combo_layout,
        "Configuration mode/profile anchors must never move during scroll")

require("g_overlaySmoothingStrengthPercent{ 15 }" in OVERLAY and
        "stg.smoothing===undefined?15:stg.smoothing" in OVERLAY,
        "fresh Input Overlay smoothing must default to 15 percent in native and web state")
require("OverlayServer_GetEffectStrengthPercent(OverlayEffect_Smoothing)" in SETTINGS_INI and
        'IniReadI32(L"InputOverlay", L"StrengthSmoothing", overlaySmoothDef, path)' in SETTINGS_INI,
        "missing smoothing keys must use the new default while stored profiles remain authoritative")

require("Config_DrawLiveStatus" in PAGES and "Live diagnostics: Gamepad Tester" in PAGES,
        "Configuration must point to the canonical diagnostics surface")
require(PAGES.count("BuildAnalogDiagnosticsLines(") == 3,
        "route-complete diagnostics builder must be declared, defined, and consumed once")
tester = section(PAGES, "struct TesterPageState", "// ============================================================================\n// Input Overlay page")
require("CustomPageSurface_DrawScrollbar" in tester and "case WM_MOUSEWHEEL" in tester and
        "CustomPageScrollController" in tester and "CustomPageSurface_HandleScrollMessage" in tester,
        "route-complete Tester diagnostics must remain reachable through common scrolling")

print("pre_release_ui_static_audit: PASS")
