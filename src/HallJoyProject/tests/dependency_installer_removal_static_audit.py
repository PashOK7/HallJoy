#!/usr/bin/env python3
"""Verify that S18 removed privileged/downloaded installer execution."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TESTS = ROOT / "src" / "HallJoyProject" / "tests"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


deps = (HALL / "app_deps.cpp").read_text(encoding="utf-8-sig")
header = (HALL / "app_deps.h").read_text(encoding="utf-8-sig")
policy = (HALL / "dependency_guidance_policy.h").read_text(encoding="utf-8-sig")
app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
build = (ROOT / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
lock = json.loads((ROOT / "tools" / "dependency-lock.json").read_text(encoding="utf-8"))

for forbidden in (
    "URLDownloadToFile", "WinHttp", "WinVerifyTrust", "ShellExecute",
    "runas", "msiexec", "WaitForSingleObject", "releases/latest",
    "DownloadLatestAssetToTemp", "RunInstallerElevatedAndWait",
):
    require(forbidden not in deps, f"removed installer primitive: {forbidden}")

require("AppDeps_ShowMissingDependencyGuidance" in header and
        "AppDeps_TryInstallMissingDependencies" not in header,
        "public recovery contract is guidance, not installation")
require("ManualInstallRequired" in header and "Installed" not in header,
        "result cannot falsely report an in-process installation")
require("https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0" in policy and
        "/latest" not in policy,
        "manual dependency page is an exact official version")
require(lock["runtimeDependencies"]["vigemBus"] == {
            "version": "1.22.0",
            "releasePage": "https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0",
            "installationPolicy": "manual-only",
        }, "central dependency lock pins the same manual-only release")
require("never downloads, starts, or elevates an installer" in deps and
        "automatic_installer=disabled" in deps,
        "UI and trace state the no-installer security boundary")
require("DependencyGuidanceResult::ManualInstallRequired" in app and
        "continue in degraded mode" in app,
        "manual action never masquerades as backend recovery")
require("dependency_guidance_policy.h" in project,
        "production project owns the immutable guidance policy")
require("dependency_guidance_policy_test.cpp" in runner and
        (TESTS / "dependency_guidance_policy_test.cpp").is_file(),
        "portable gate covers all guidance-plan combinations")
require("dependency_installer_removal_static_audit.py" in build and
        "dependency_guidance_policy_test.cpp" in build,
        "official build requires the S18 regression assets")

print("DEPENDENCY_INSTALLER_REMOVAL_STATIC_AUDIT=PASS")
