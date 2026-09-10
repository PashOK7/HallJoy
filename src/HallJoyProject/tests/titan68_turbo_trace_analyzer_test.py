#!/usr/bin/env python3
"""Executable regression test for the Titan68 diagnostic trace analyser."""

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
ANALYSER = ROOT / "tools" / "analyze_titan68_turbo_trace.py"
FIXTURE = Path(__file__).resolve().parent / "data" / "titan68_turbo_trace_valid.log"

result = subprocess.run(
    [sys.executable, str(ANALYSER), str(FIXTURE)],
    capture_output=True, text=True, check=False,
)
if result.returncode != 0:
    raise SystemExit("trace analyser rejected valid fixture:\n" + result.stdout + result.stderr)
for expected in (
    "VERDICT: REPORT-07 RAW12 OBSERVED",
    "TX: calibration_enter=1; simulation_enter=1; simulation_exit=1",
    "key_index=17: samples=2 min=1170 max=4050",
):
    if expected not in result.stdout:
        raise SystemExit("missing expected analyser output: " + expected + "\n" + result.stdout)
print("TITAN68 TURBO TRACE ANALYSER TEST PASSED")
