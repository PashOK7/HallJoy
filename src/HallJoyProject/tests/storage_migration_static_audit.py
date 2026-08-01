#!/usr/bin/env python3
"""Static gate for V14-09C data-root migration and filename hardening."""

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


paths = read(HALL / "app_paths.cpp")
paths_header = read(HALL / "app_paths.h")
policy = read(HALL / "file_name_policy.cpp")
layout = read(HALL / "keyboard_layout.cpp")
curves = read(HALL / "keyboard_profiles.cpp")
globals_cpp = read(HALL / "global_profiles.cpp")
subpages = read(HALL / "keyboard_subpages.cpp")
app = read(HALL / "app.cpp")
project = read(HALL / "HallJoy.vcxproj")
runner = read(REPO / "tools" / "run_analog_simulator.ps1")
runtime_test = read(REPO / "tools" / "run_storage_migration_test.ps1")
build = read(REPO / "tools" / "build.ps1")

require(paths, "FOLDERID_LocalAppData", "production data root uses LocalAppData")
require(paths, 'L"HallJoy.portable"', "portable mode requires an explicit marker")
require(paths, "DirectoryIsWritable(g_paths.legacyRoot)", "portable marker also requires a writable EXE directory")
require(paths, "FILE_ATTRIBUTE_REPARSE_POINT", "data and migration roots reject reparse aliases")
require(paths, 'L"MigrationBackups"', "legacy data receives a one-time backup")
require(paths, "CopyFileTransactional", "migration copies use the common transaction")
require(paths, "ExistingMarkerIsComplete", "completed source-specific migration is replay-safe")
require(paths, "source_preserved=1", "migration records that legacy files are retained")
require(app, "if (!AppPaths_Initialize())", "application blocks startup after unsafe or failed migration")

require(policy, "NormalizeString(", "filename stems use Unicode NFC normalization")
require(policy, "LCMapStringEx(", "collision keys use invariant case folding")
for reserved in ("con", "prn", "aux", "nul", "clock$", "com", "lpt"):
    require(policy, f'L"{reserved}"', f"reserved basename {reserved} is handled")
require(policy, "FileNamePolicy_MaxStemLength()", "filename stems have one maximum length")
require(policy, "IsDirectChild", "constructed paths must remain direct children")
require(policy, "FileNamePolicy_MakeUniqueChildPath", "collisions receive a checked suffix")

require(layout, "AppPaths_LayoutsDir()", "layout presets use the centralized writable root")
require(layout, "FileNamePolicy_NormalizeStem", "layout creation uses the common filename policy")
require(curves, "AppPaths_CurvePresetsDir()", "curve presets use the centralized writable root")
require(curves, "FileNamePolicy_MakeUniqueChildPath", "curve creation uses collision-safe suffixing")
require(globals_cpp, "FileNamePolicy_NormalizeStem", "global profiles use the common filename policy")
require(globals_cpp, "FindExistingProfilePath", "Unicode/case aliases resolve existing profile files")
require(subpages, "AppPaths_LayoutsDir()", "Open Layouts Folder targets the writable root")

require(runner, "--halljoy-test-data-root", "simulator state is isolated from user LocalAppData")
require(runner, "--halljoy-test-storage-policy", "simulator executes the real Windows filename policy")
require(runtime_test, "sourceHashes", "runtime migration test proves source preservation")
require(runtime_test, "MigrationBackups", "runtime migration test verifies backup hashes")
require(runtime_test, "migration.skip", "runtime migration test proves one-time replay")
require(runtime_test, "foreach ($stage in @('prepare', 'write', 'flush', 'validate', 'replace'))", "runtime migration test covers every transaction stage")
require(runtime_test, "RequireStorageMigrationFailure", "runtime migration faults must abort initialization")
require(runner, "expected 1", "migration failure runner verifies the blocked-start exit code")
require(runtime_test, "HallJoy.portable", "runtime test exercises marker-selected portable mode")
require(build, "$preservedRuntimeNames", "official build preserves legacy and portable user state")
for runtime_name in ("settings.ini", "bindings.ini", "GlobalProfiles", "Layouts", "CurvePresets", "HallJoy.portable"):
    require(build, f"'{runtime_name}'", f"official build preserves {runtime_name}")
require(project, '<ClCompile Include="file_name_policy.cpp" />', "filename policy is part of every MSVC target")
require(paths_header, "AppPaths_DataRoot", "central data root is public to storage consumers")

if 'WinUtil_BuildPathNearExe(L"Layouts")' in layout or 'WinUtil_BuildPathNearExe(L"CurvePresets")' in curves:
    raise SystemExit("FAIL: a preset writer still prefers the executable directory")
print("PASS: preset writers no longer implicitly prefer the executable directory")

print("STORAGE_MIGRATION_STATIC_AUDIT=PASS")
