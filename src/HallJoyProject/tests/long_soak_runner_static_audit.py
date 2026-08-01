#!/usr/bin/env python3
"""Verify the V14-12H long production soak contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RUNNER = ROOT / "tools" / "run_long_soak.ps1"
BUILD = ROOT / "tools" / "build.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


runner = RUNNER.read_text(encoding="utf-8-sig")
build = BUILD.read_text(encoding="utf-8-sig")

require("[ValidateRange(1, 1440)]" in runner and "[int]$DurationMinutes = 480" in runner,
        "soak duration is bounded to 1..1440 minutes and defaults to eight hours")
require("[ValidateRange(1, 60)]" in runner and "[int]$SampleSeconds" in runner,
        "resource sampling interval is bounded to at most sixty seconds")
require("[int]$ProgressMinutes = 5" in runner and "Soak progress:" in runner,
        "long runs emit periodic human-readable progress")
require("[ValidateRange(1, 300)]" in runner and "[int]$WarmupSeconds = 10" in runner and
        "WarmupSeconds must be shorter" in runner and "baseline_elapsed_seconds" in runner,
        "leak gates use an explicit bounded post-startup warm-up baseline")
require("[IO.Path]::GetFileName($ExePath) -ne 'HallJoy.exe'" in runner and
        "Get-Process -Name 'HallJoy'" in runner and "Refusing to start" in runner,
        "runner enforces HallJoy.exe and refuses an existing HallJoy session")
require("Start-Process @startParameters" in runner and
        not any(token in runner for token in (
            "--halljoy-test-", "--analog-simulator", "InjectRealtimeStartFailure")),
        "soak launches the production path with no simulator or fault arguments")
require("HandleCount" in runner and "PrivateMemorySize64" in runner and
        "GetGuiResources" in runner and "TotalProcessorTime" in runner and
        "samples.csv" in runner,
        "soak continuously persists handle, GUI, memory, and CPU samples")
require("checkpoint.json" in runner and "Write-SoakCheckpoint" in runner and
        "Status 'failed'" in runner and "Status 'passed'" in runner,
        "soak checkpoints preserve progress and terminal status")
require("state-before.json" in runner and "state-after.json" in runner and
        "Get-ChangedStateFiles" in runner and "Get-FileHash" in runner,
        "user-state file names and SHA-256 values are persisted and invariant")
require("PostClose" in runner and "0x0010" in runner and "ShutdownTimeoutSeconds" in runner and
        "$process.ExitCode -ne 0" in runner and "Wait-NoHallJoyProcess" in runner,
        "soak ends by bounded graceful WM_CLOSE with exit zero and no survivor")
require("\\[level=ERROR\\]" in runner and "event=capped" in runner and
        "analyze_stability_trace.py" in runner and "$analysisExitCode -ge 2" in runner,
        "trace errors, capping, and analyzer FAIL are release-blocking")
require("$handleGrowth -gt 32" in runner and "$privateGrowth -gt 268435456" in runner,
        "fixed handle and private-memory growth gates prevent silent leaks")
require("check_overlay_responsiveness.py" in runner and "OverlayProbeMinutes" in runner,
        "optional overlay soak repeatedly proves HTTP responsiveness")
require("spark_route_queries" in runner and "spark_route_ok" in runner and
        "spark_route_fail" in runner and "summary.json" in runner,
        "final summary retains SparkLink routing health and machine-readable evidence")
require("run_long_soak.ps1" in build,
        "official build manifest requires the long-soak runner")

print("LONG_SOAK_RUNNER_STATIC_AUDIT=PASS")
