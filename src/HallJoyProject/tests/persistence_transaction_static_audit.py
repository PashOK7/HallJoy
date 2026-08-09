#!/usr/bin/env python3
"""Static gate for V14-09A/B transactional persistence."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(name: str) -> str:
    return (HALL / name).read_text(encoding="utf-8")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


transaction = read("transactional_file_store.h")
ini = read("ini_util.cpp")
settings = read("settings_ini.cpp")
profile = read("profile_ini.cpp")
global_profiles = read("global_profiles.cpp")
subpages = read("keyboard_subpages.cpp")
layout = read("keyboard_layout.cpp")
curves = read("keyboard_profiles.cpp")
app = read("app.cpp")
project = read("HallJoy.vcxproj")
simulator_runner = (REPO / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

ordered = [
    "adapter.Prepare()",
    "adapter.Write()",
    "adapter.Flush()",
    "adapter.Validate()",
    "adapter.Replace()",
]
positions = [transaction.find(marker) for marker in ordered]
if any(position < 0 for position in positions) or positions != sorted(positions):
    raise SystemExit("FAIL: transaction stages are missing or out of order")
print("PASS: prepare/write/flush/validate/replace order is explicit")

require(transaction, "adapter.Cleanup();", "every failed stage cleans its temporary file")
require(ini, "CREATE_NEW", "temporary file creation rejects collisions")
require(ini, 'temporary_ = destination_ + suffix;', "temporary file stays beside destination")
require(ini, 'L".halljoy-new-', "temporary file name is process/thread/sequence unique")
require(ini, "FlushFileBuffers(file)", "durability uses checked physical flush")
require(ini, "MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH", "commit is an atomic write-through replace")
require(ini, "DeleteFileW(temporary_.c_str())", "failed transactions remove temporary files")
require(ini, 'L"save.failure"', "failures publish critical stability evidence")
require(ini, "MessageBoxW", "production UI reports save failures")

for stage in ("prepare", "write", "flush", "validate", "replace"):
    require(ini, f"--halljoy-test-persistence-failure-{stage}", f"simulator injects {stage} failure")

if '+ L".tmp"' in settings or '+ L".tmp"' in profile:
    raise SystemExit("FAIL: settings/profile still use a shared fixed .tmp path")
print("PASS: settings/profile no longer use shared fixed .tmp paths")

require(settings, "IniUtil_SaveAtomic(path, SettingsTransactionWrite, SettingsTransactionValidate", "settings use the common transaction")
require(settings, "IniUtil_CopyExistingForUpdate", "overlay update preserves unrelated base settings")
require(settings, 'L"HallJoyPersistence", L"SchemaVersion"', "settings write a schema marker")
require(settings, "SettingsTransactionValidate", "settings are parsed back before commit")
require(settings, "ok &= KeyboardLayout_SaveToIni", "embedded layout selection writes are checked")
require(profile, "f.flush();", "bindings stream is explicitly flushed")
require(profile, "return !f.fail();", "bindings stream close state is checked")
require(profile, "ProfileTransactionValidate", "bindings are parsed back before commit")
require(profile, "IniUtil_ReportSaveFailure", "bindings failure reaches the user-facing reporter")
require(global_profiles, "ActiveProfileTransactionValidate", "active-profile marker update is read back")
require(global_profiles, "IniUtil_SaveAtomic", "active-profile marker update is atomic")
require(subpages, "if (!previousSettingsSaved || !previousBindingsSaved)", "profile switching aborts after a failed old-profile save")
require(subpages, "if (!newSettingsSaved || !newBindingsSaved)", "profile creation rejects a partial new profile")
require(subpages, "if (!settingsSaved || !bindingsSaved)", "manual profile save keeps dirty state after failure")

if 'std::wstring tmp = path + L".tmp"' in layout or 'std::wstring tmp = path + L".tmp"' in curves:
    raise SystemExit("FAIL: layout/curve saves still use a shared fixed .tmp path")
print("PASS: layout/curve saves no longer use shared fixed .tmp paths")

require(layout, "LayoutPresetTransactionWrite", "layout preset has a checked transaction writer")
require(layout, "LayoutPresetTransactionValidate", "layout preset is parsed back before commit")
require(layout, "IniUtil_SaveAtomic(", "layout preset uses the common transaction")
require(layout, 'IniUtil_ReportSaveFailure(L"layout preset"', "layout save failures reach the central reporter")
require(layout, "PresetStore candidate = g_presets[idx];", "active layout memory stages a candidate before save")
require(layout, "g_presets[idx] = std::move(candidate);", "active layout memory commits only after file save")
require(layout, "PresetStore candidate = g_presets[presetIdx];", "layout editor stages a candidate before save")
require(layout, "g_presets[presetIdx] = std::move(candidate);", "layout editor memory commits only after file save")
require(layout, 'L"Keychron K4 HE", g_keychronK4HeKeys', "Keychron K4 HE ships as a distinct built-in layout")

layout_init = layout.find("static void EnsureInit()")
layout_init_order = [
    layout.find("AddBuiltinDefaults();", layout_init),
    layout.find("LoadPresetsFromDir();", layout_init),
    layout.find("EnsurePresetFilesExist();", layout_init),
    layout.find("ActivatePreset(0);", layout_init),
]
if any(position < 0 for position in layout_init_order) or layout_init_order != sorted(layout_init_order):
    raise SystemExit("FAIL: built-in layout merge order is missing or unsafe")
print("PASS: built-ins load first, user files override them, and preset zero remains the default")

keychron_begin = layout.find("static const KeyDef g_keychronK4HeKeys[]")
keychron_end = layout.find("static const PresetDef g_builtinPresets[]", keychron_begin)
keychron_body = layout[keychron_begin:keychron_end]
if keychron_body.count("{L\"") != 100:
    raise SystemExit("FAIL: Keychron K4 HE built-in must contain exactly 100 keys")
print("PASS: Keychron K4 HE built-in contains the validated 100-key geometry")

for preset_name, body in (
    ("Generic 100% ANSI", layout[layout.find("static const KeyDef g_generic100Keys[]"):keychron_begin]),
    ("Keychron K4 HE", keychron_body),
):
    for label, hid, row in (("Num+", 87, 2), ("NEnt", 88, 4)):
        matching_lines = [line for line in body.splitlines() if f'{{L"{label}",' in line]
        if len(matching_lines) != 1 or f'{hid}, {row},' not in matching_lines[0] or not matching_lines[0].rstrip().endswith(', 87},'):
            raise SystemExit(f"FAIL: {preset_name} {label} must retain the visually validated height of 87 px")
        print(f"PASS: {preset_name} {label} retains the visually validated height of 87 px")

require(layout, "static constexpr int kBuiltinGeometryRevision = 2;", "corrected tall-key migration supersedes the bad revision 1")
require(layout, "(key.hid == 87 || key.hid == 88) && key.h == 88", "only the old 88 px tall-key value is selected")
require(layout, "key.h = 87;", "old persisted tall keys migrate to the visually validated 87 px")
require(layout, 'L"BuiltinGeometryRevision"', "layout files persist the corrected geometry revision")
if "(key.hid == 87 || key.hid == 88) && key.h == 87" in layout or "key.h = 88;" in layout:
    raise SystemExit("FAIL: obsolete 87-to-88 tall-key migration must not return")
print("PASS: no migration can overwrite the visually validated 87 px height")

require(curves, "CurvePresetTransactionWrite", "curve preset has a checked transaction writer")
require(curves, "CurvePresetTransactionValidate", "curve preset is parsed back before commit")
require(curves, "CurveStateTransactionWrite", "curve UI state has a checked transaction writer")
require(curves, "CurveStateTransactionValidate", "curve UI state is parsed back before commit")
require(curves, 'IniUtil_ReportSaveFailure(L"curve preset"', "curve preset failures reach the central reporter")
require(curves, 'IniUtil_ReportSaveFailure(L"curve state"', "curve state failures reach the central reporter")
require(curves, "g_activeName = previousName;", "curve active state rolls memory back after a failed save")
require(subpages, "DeleteFileW(newPath.wstring().c_str());", "failed active curve rename removes its newly-created file")
require(subpages, "Rename incomplete: new preset saved, but old preset could not be deleted.", "partial curve rename is reported truthfully")

for marker, description in (
    ("KeyboardLayout_TestSaveActivePresetToPath", "simulator exercises layout preset transactions"),
    ("KeyboardProfiles::SavePreset(curveProbe", "simulator exercises curve preset transactions"),
    ("KeyboardProfiles::TestSaveStateToPath", "simulator exercises curve state transactions"),
):
    require(app, marker, description)

for kind in ("layout preset", "curve preset", "curve state"):
    require(simulator_runner, f"kind={kind} stage=$InjectPersistenceFailure", f"runner requires {kind} failure evidence")
for probe in ("KNOWN_GOOD_LAYOUT_PROBE", "KNOWN_GOOD_CURVE_PROBE", "KNOWN_GOOD_CURVE_STATE_PROBE"):
    require(simulator_runner, probe, f"runner hashes {probe.removeprefix('KNOWN_GOOD_').lower()}")

require(project, '<ClInclude Include="transactional_file_store.h" />', "transaction header is part of the MSVC project")

print("PERSISTENCE_TRANSACTION_STATIC_AUDIT=PASS")
