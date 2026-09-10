#!/usr/bin/env python3
"""Verify that dynamic installer execution stays removed and the replacement is pinned."""

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TESTS = ROOT / "src" / "HallJoyProject" / "tests"
PINNED_SHA256 = "89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A"
PINNED_SIZE = 6_278_576
PINNED_NAME = "ViGEmBus_1.22.0_x64_x86_arm64.exe"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


deps = (HALL / "app_deps.cpp").read_text(encoding="utf-8-sig")
header = (HALL / "app_deps.h").read_text(encoding="utf-8-sig")
installer = (HALL / "embedded_vigem_installer.cpp").read_text(encoding="utf-8-sig")
installer_header = (HALL / "embedded_vigem_installer.h").read_text(encoding="utf-8-sig")
policy = (HALL / "dependency_guidance_policy.h").read_text(encoding="utf-8-sig")
app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
main = (HALL / "main.cpp").read_text(encoding="utf-8-sig")
resource_header = (HALL / "Resource.h").read_text(encoding="utf-8-sig")
resource_script = (HALL / "HallJoy.rc").read_text(encoding="utf-16")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
build = (ROOT / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
lock = json.loads((ROOT / "tools" / "dependency-lock.json").read_text(encoding="utf-8"))
pinned_binary = ROOT / "src" / "HallJoyProject" / "third_party" / "ViGEmBus" / PINNED_NAME
license_file = pinned_binary.parent / "LICENSE"

for forbidden in (
    "URLDownloadToFile", "WinHttp", "releases/latest",
    "DownloadLatestAssetToTemp", "HttpGetUtf8", "ExtractBrowserDownloadUrls",
):
    require(forbidden not in deps and forbidden not in installer,
            f"legacy dynamic installer primitive remains absent: {forbidden}")

require("IDR_VIGEMBUS_INSTALLER" in resource_header and
        f'RCDATA       "..\\\\third_party\\\\ViGEmBus\\\\{PINNED_NAME}"' in resource_script,
        "the exact ViGEmBus installer is an embedded RCDATA resource")
require(PINNED_SHA256 in policy and str(PINNED_SIZE) in policy,
        "production policy pins exact installer size and SHA-256")
require(pinned_binary.is_file() and pinned_binary.stat().st_size == PINNED_SIZE,
        "the pinned installer has the expected byte size")
require(hashlib.sha256(pinned_binary.read_bytes()).hexdigest().upper() == PINNED_SHA256,
        "the pinned installer has the exact Microsoft WinGet SHA-256")
require(license_file.is_file() and "BSD 3-Clause License" in license_file.read_text(encoding="utf-8"),
        "binary redistribution license is retained")

expected_lock = {
    "version": "1.22.0",
    "releasePage": "https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0",
    "installerUrl": "https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe",
    "installerPath": "src/HallJoyProject/third_party/ViGEmBus/ViGEmBus_1.22.0_x64_x86_arm64.exe",
    "installerSize": PINNED_SIZE,
    "installerSha256": PINNED_SHA256,
    "installationPolicy": "embedded-pinned-one-click",
}
require(lock["runtimeDependencies"]["vigemBus"] == expected_lock,
        "central lock matches the exact embedded installer")

require("FindResourceW" in installer and "SizeofResource" in installer and
        "kExpectedInstallerSize" in installer and "kExpectedInstallerSha256" in installer,
        "runtime validates the embedded resource identity")
require("BCryptGenRandom" in installer and "CreateDirectoryW" in installer and
        "CREATE_NEW" in installer,
        "extraction uses a cryptographically unpredictable create-new path")
require("kHexDigits" in installer and "directory.push_back" in installer and
        "swprintf" not in installer,
        "random temp suffix construction has no fixed formatting buffer")
require("catch (const std::bad_alloc&)" in installer and
        "catch (...)" in installer,
        "public installer entry points contain allocation and unknown exceptions")
transition_body = installer.split("bool TransitionToReadOnlyLock", 1)[1].split(
    "bool PrepareInstallerFile", 1)[0]
require("FILE_SHARE_READ | FILE_SHARE_WRITE" in transition_body and
        "installer.file.Reset();" in transition_body and
        "installer.file.Reset(locked.Release())" in transition_body and
        "FILE_SHARE_DELETE" not in transition_body,
        "writer transitions through a path-retaining bridge to a read-only lock")
require("FlushFileBuffers" in installer and "WinVerifyTrust" in installer and
        "fileInfo.hFile = lockedFile" in installer,
        "extracted bytes are flushed and Authenticode-verified")
require("GENERIC_READ | GENERIC_EXECUTE" in installer and
        "launchProbe" in installer,
        "self-test proves the final lock still permits executable image access")
run_body = installer.split("EmbeddedVigemInstallResult EmbeddedVigemInstaller_Run", 1)[1]
require(run_body.index("HashFileHandle") < run_body.index("ShellExecuteExW") and
        'launch.lpVerb = L"runas"' in run_body,
        "the same locked file is re-hashed immediately before explicit elevation")
require("kInstallerWaitMs" in installer and "MsgWaitForMultipleObjectsEx" in installer and
        "INFINITE" not in installer,
        "installer waiting is bounded and pumps window messages")
require("EmbeddedVigemInstaller_VerifyOnly" in installer_header and
        "--halljoy-verify-embedded-vigem-installer" in deps and
        "AppDeps_TryRunEmbeddedInstallerVerificationCommand" in main,
        "the exact linked resource has a non-executing self-test command")

require("TDF_USE_COMMAND_LINKS" in deps and
        "Install ViGEmBus 1.22.0 (recommended)" in deps and
        "Open the official release page" in deps and
        "Continue without virtual gamepad output" in deps,
        "missing-driver UI has explicit one-click actions")
require("Ctrl+C" not in deps and "copy its full text" not in deps,
        "the broken copy-the-entire-error workflow is absent")
require("ShellExecuteW" in deps and "kPinnedVigemReleasePage" in deps,
        "manual fallback opens the pinned official page directly")
require("InstallCompleted" in header and
        "DependencyGuidanceResult::InstallCompleted" in app and
        "Backend_Init()" in app,
        "successful setup retries backend initialization in the same process")

require("embedded_vigem_installer.cpp" in project and
        "embedded_vigem_installer.h" in project,
        "production project compiles the pinned installer owner")
require("vigemInstallerExpectedSha256" in build and
        "Get-AuthenticodeSignature" in build and
        "--halljoy-verify-embedded-vigem-installer" in build,
        "official build verifies source identity, publisher, and linked resource")
require("dependency_guidance_policy_test.cpp" in runner and
        (TESTS / "dependency_guidance_policy_test.cpp").is_file(),
        "portable policy gate remains part of the unified runner")
require("dependency_installer_removal_static_audit.py" in build and
        "dependency_guidance_policy_test.cpp" in build,
        "official build requires the replacement security and UX gates")

print("DEPENDENCY_INSTALLER_REMOVAL_STATIC_AUDIT=PASS")
