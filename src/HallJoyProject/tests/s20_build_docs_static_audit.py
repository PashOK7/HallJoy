#!/usr/bin/env python3
"""Prevent regression of the S20 x64/W4/build-documentation contract."""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PROJECT_ROOT = ROOT / "src" / "HallJoyProject"
HALL = PROJECT_ROOT / "HallJoy"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
BASELINE_WARNINGS = "4100;4127;4324;4505;%(DisableSpecificWarnings)"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    project_path = HALL / "HallJoy.vcxproj"
    project = read(project_path)
    solution = read(PROJECT_ROOT / "HallJoy.sln")
    build = read(ROOT / "tools" / "build.ps1")
    runner = read(ROOT / "tools" / "run_native_backend_checks.py")
    testing = read(PROJECT_ROOT / "TESTING.md")
    readme = read(PROJECT_ROOT / "README.md")
    build_readme = read(PROJECT_ROOT / "BUILD_README_RU.txt")
    historical = read(ROOT / "docs" / "validation" / "VALIDATION_PACKAGE_V3_9_0.txt")
    release_risks = read(ROOT / "docs" / "v1.4" / "RISK_REGISTER.md")
    imported_risks = read(ROOT / "docs" / "stability" / "RISK_REGISTER_RU.md")
    roadmap = read(ROOT / "docs" / "v1.4" / "ROADMAP.md")
    evidence = ROOT / "docs" / "stability" / "tests" / "V14-12G_S20_BUILD_DOCS_2026-08-01.txt"

    tree = ET.parse(project_path)
    project_configs = {
        node.attrib["Include"]
        for node in tree.findall(".//m:ProjectConfiguration", NS)
    }
    require(project_configs == {"Debug|x64", "Release|x64"},
            f"supported project configurations changed: {sorted(project_configs)}")
    require("|Win32" not in project and "lib\\release\\Win32" not in project,
            "Win32 configuration/library reference returned")
    require("Debug|x86" not in solution and "Release|x86" not in solution and "|Win32" not in solution,
            "solution advertises an unsupported x86/Win32 target")

    warning_levels = [node.text for node in tree.findall(".//m:WarningLevel", NS)]
    require(warning_levels == ["Level4", "Level4"],
            f"both x64 configurations must use Level4: {warning_levels}")
    warning_baselines = [node.text for node in tree.findall(".//m:DisableSpecificWarnings", NS)]
    require(warning_baselines == [BASELINE_WARNINGS, BASELINE_WARNINGS],
            f"documented warning baseline changed: {warning_baselines}")
    require("Level3" not in project, "Warning Level 3 returned")
    debug_group = next(
        node for node in tree.findall("./m:PropertyGroup", NS)
        if node.attrib.get("Condition", "").endswith("'Debug|x64'")
    )
    require(debug_group.findtext("m:UseDebugLibraries", namespaces=NS) == "false",
            "Debug|x64 must use the release static CRT required by bundled ViGEm")
    debug_items = next(
        node for node in tree.findall("./m:ItemDefinitionGroup", NS)
        if node.attrib.get("Condition", "").endswith("'Debug|x64'")
    )
    debug_definitions = debug_items.findtext("m:ClCompile/m:PreprocessorDefinitions", namespaces=NS) or ""
    debug_definition_tokens = set(debug_definitions.split(";"))
    require("HALLJOY_DEBUG_BUILD" in debug_definition_tokens and "_DEBUG" not in debug_definition_tokens,
            "Debug|x64 must keep HallJoy debug features without selecting the incompatible debug CRT")

    library_dirs = [node.text or "" for node in tree.findall(".//m:AdditionalLibraryDirectories", NS)]
    require(len(library_dirs) == 2 and all("ViGEmClient\\lib\\release\\x64" in value for value in library_dirs),
            "each supported configuration must link the x64 ViGEm client")
    require((PROJECT_ROOT / "third_party" / "ViGEmClient" / "lib" / "release" / "x64" / "ViGEmClient.lib").is_file(),
            "bundled x64 ViGEmClient.lib is missing")

    require("Unexpected production compiler/linker warnings were emitted." in build and
            "warning LNK4099:.*ViGEmClient\\.pdb" in build,
            "official build no longer rejects warnings outside the one linker baseline")
    require("run_native_backend_checks.py" in build and "--require-compiler" in build,
            "official build no longer requires the unified automated gate")
    require("validate_addressed_protocol_backend.py" in runner,
            "rewritten Addressed validator is absent from the unified runner")

    for stale in ("V11.0.2", "bench.ps1", "run_tests.ps1", "HallJoyTests", "ensure_blend2d.ps1"):
        require(stale not in testing, f"stale testing reference returned: {stale}")
    require("run_native_backend_checks.py --require-compiler" in testing and
            "run_release_qualification.ps1" in testing and
            "Aula WIN 60 HE MAX remains firmware-proven" in testing,
            "TESTING.md does not describe the current automated/hardware gates")

    require("Madlions V6 SafeHID branch" not in readme and
            "Settings saved next to the executable" not in readme,
            "project README returned to the V6/storage story")
    require("Windows x64 only" in readme and "build\\output\\HallJoy.exe" in readme and
            "%LOCALAPPDATA%\\HallJoy" in readme,
            "project README is missing the current target/output/storage contract")
    require("BUILD.cmd" in build_readme and "build\\output\\HallJoy.exe" in build_readme and
            "Win32/x86 не поддерживается" in build_readme,
            "Russian build guide is missing the current x64 build contract")

    require(historical.startswith("HISTORICAL RECORD ONLY - NOT CURRENT v1.4 RELEASE EVIDENCE"),
            "v3.9 validation record lacks the historical warning")
    require("Automated validation completed" not in historical and
            "does not prove bounded worker lifecycle" in historical,
            "historical validation still overclaims current lifecycle evidence")

    status_pattern = re.compile(
        r"^\| `(?P<id>HJ-AUD-[^`]+)` \|.*?\| (?P<status>Open|Implemented|Partial|Verified) \|",
        re.MULTILINE,
    )
    release_statuses = {match.group("id"): match.group("status") for match in status_pattern.finditer(release_risks)}
    imported_statuses = {match.group("id"): match.group("status") for match in status_pattern.finditer(imported_risks)}
    require(release_statuses == imported_statuses and len(release_statuses) == 45,
            "authoritative and imported risk registers diverged")
    require(sum(status == "Open" for status in release_statuses.values()) == 0 and
            sum(status == "Implemented" for status in release_statuses.values()) == 3 and
            sum(status == "Partial" for status in release_statuses.values()) == 1 and
            sum(status == "Verified" for status in release_statuses.values()) == 41,
            "post-S20 risk counts changed")
    require(all(release_statuses.get(f"HJ-AUD-P3-00{number}") == "Verified" for number in range(1, 7)),
            "one of the six S20 audit risks is no longer Verified")
    require("S20 is complete; S21 qualification is next" in roadmap and evidence.is_file(),
            "S20 completion/evidence is missing from authoritative documentation")

    print("S20 x64/W4/build-documentation static audit passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
