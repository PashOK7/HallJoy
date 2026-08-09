#!/usr/bin/env python3
"""Verify the V14-12H normal-operation release qualification contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RUNNER = ROOT / "tools" / "run_release_qualification.ps1"
BUILD = ROOT / "tools" / "build.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


runner = RUNNER.read_text(encoding="utf-8-sig")
build = BUILD.read_text(encoding="utf-8-sig")

require("[ValidateRange(1, 1000)]" in runner and "[int]$Cycles" in runner,
        "runner supports a bounded 1..1000 normal-cycle qualification")
require("[int]$ProgressEvery" in runner and "($cycle % $ProgressEvery)" in runner,
        "long runs expose bounded periodic progress without per-cycle console spam")
require("Get-Process -Name 'HallJoy'" in runner and "Refusing to start" in runner,
        "runner refuses to collide with an existing HallJoy session")
require("[IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe'" in runner,
        "runner enforces the final HallJoy.exe artifact name")
require("Start-Process -FilePath $ExePath" in runner and
        not any(token in runner for token in (
            "--halljoy-test-", "--analog-simulator", "InjectRealtimeStartFailure")),
        "qualification launches production with no simulator or fault arguments")
require("PostClose" in runner and "0x0010" in runner and "ShutdownTimeoutSeconds" in runner,
        "every cycle uses bounded graceful WM_CLOSE shutdown")
require("$process.ExitCode -ne 0" in runner and "Wait-NoHallJoyProcess" in runner,
        "every cycle requires exit zero and no remaining HallJoy process")
require("Get-HallJoyStateSnapshot" in runner and "Write-StateSnapshot" in runner and
        "Assert-SameStateSnapshot" in runner and "state-before.json" in runner and
        "state-after.json" in runner,
        "LocalAppData file set and SHA-256 values are persisted and invariant")
require("forbiddenProductionLogs" in runner and "HallJoyStabilityTrace.log" in runner and
        "HallJoyDiagnostic.log" in runner and "HallJoyCrash.txt" in runner,
        "each cycle enforces the final zero-continuous-log and crash-only policy")
require("continuous_log_files = 0" in runner and "crash_report = $false" in runner and
        "trace_sha256" not in runner and "worker\\.stats" not in runner,
        "machine evidence records quiet production without stale trace-derived counters")
require("checkpoint.json" in runner and "Write-QualificationCheckpoint" in runner and
        "Status 'failed'" in runner and "Status 'passed'" in runner,
        "machine-readable checkpoints survive progress, success, and failure")
require('build\\evidence\\release-qualification' in runner and
        '$output "release-qualification' not in runner,
        "default cycle evidence survives official build/output cleanup")
require("fault_injection = $false" in runner and "summary.json" in runner and
        "status = 'passed'" in runner,
        "machine-readable final evidence labels a successful normal-operation run")
require("run_release_qualification.ps1" in build,
        "official build manifest requires the qualification runner")

print("RELEASE_QUALIFICATION_RUNNER_STATIC_AUDIT=PASS")
