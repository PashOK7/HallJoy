#!/usr/bin/env python3
"""Require honest end-to-end production load profiling coverage."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
RUNNER = REPO / "tools" / "run_input_pipeline_profile.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    text = RUNNER.read_text(encoding="utf-8-sig", errors="strict")
    require("Get-HallJoyProcessInfo" in text and
            all(role in text for role in ("'main'", "'uap-host'", "'diagnostic-watch'")),
            "the complete HallJoy process tree is measured")
    require("Get-ThreadLabels" in text and "main_thread_stages" in text and
            "worker.start" in text and "ui-and-short-lived-workers" in text,
            "trace TIDs split the main process into production pipeline stages")
    require(all(phase in text for phase in (
        "'server-idle'", "'real-overlay'", "'synthetic-32-key-overlay'")),
        "idle, real browser and animated stress phases are distinct")
    require("GetSystemTimes" in text and "system_busy_percent" in text and
            "logical_processors" in text,
            "machine-normalized and system CPU context are recorded")
    require("overlay_metrics" in text and "build_us_avg" in text and
            "send_responses" in text and "client_render_us_avg" in text,
            "server build/send and browser fetch/render telemetry are retained")
    require("spark_avg_route_tx_us" in text and "spark_max_route_tx_us" in text and
            "Physical SparkLink polling was not proven" in text,
            "physical HID route throughput and transaction time are required")
    require("state-before.json" in text and "state-after.json" in text and
            "User state changed" in text,
            "profiling must not mutate user state")
    require("remaining_halljoy_processes = 0" in text and
            "HallJoy left a process" in text,
            "profiling rejects a surviving HallJoy process")
    require("trace_sha256" in text and "overlay_perf_sha256" in text and
            "summary.json" in text,
            "raw evidence is hashed into a machine-readable summary")
    require("--halljoy-test" not in text and "HallJoyV14Simulator" not in text,
            "the profiler uses production hardware without fault injection")
    print("INPUT_PIPELINE_PROFILE_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
