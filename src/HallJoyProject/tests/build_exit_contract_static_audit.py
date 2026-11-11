#!/usr/bin/env python3
"""Verify that the public batch wrapper preserves build stage exit status."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
BATCH = ROOT / "BUILD.cmd"
BUILD = ROOT / "tools" / "build.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    batch = BATCH.read_text(encoding="utf-8-sig")
    build = BUILD.read_text(encoding="utf-8-sig")
    require('set "EXITCODE=%ERRORLEVEL%"' in batch,
            "batch captures PowerShell exit code before any later command")
    require('exit /b %EXITCODE%' in batch,
            "batch returns the captured failing stage code")
    require(batch.replace('\r\n', '\n').rstrip().endswith('if "%~1"=="" pause\nexit /b 0'),
            "successful batch completion explicitly returns zero; argument-driven checks do not pause")
    require('"%~dp0tools\\build_windows_installer.ps1" %*' in batch,
            "default build packages the Windows installer and forwards arguments")
    require(not (ROOT / "Build-App.cmd").exists() and not (ROOT / "Build-Windows.cmd").exists(),
            "one public BUILD.cmd entry replaces duplicate build wrappers")
    require('$buildExitCode = $LASTEXITCODE' in build and
            'if ($buildExitCode -ne 0) { throw "HallJoy build failed: $buildExitCode" }' in build,
            "MSBuild result is captured immediately and fails closed")
    require('$vigemSelfTest.ExitCode -ne 0' in build and "exit 0" in build,
            "post-build self-test is checked and only final success returns zero")
    print("BUILD_EXIT_CONTRACT_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
