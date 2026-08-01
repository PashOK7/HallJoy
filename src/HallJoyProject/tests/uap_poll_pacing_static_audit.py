#!/usr/bin/env python3
"""Verify the V14-11A deadline-paced UAP worker contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
PLUGIN = REPO / "third_party" / "UniversalAnalogPluginFixed"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    require(start >= 0, f"found function: {signature}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:index + 1]
    raise SystemExit(f"FAIL: unterminated function: {signature}")


def main() -> int:
    main_cpp = (PLUGIN / "main.cpp").read_text(encoding="utf-8-sig")
    policy = (PLUGIN / "halljoy_uap_poll_pacing.h").read_text(encoding="utf-8-sig")
    telemetry = (PLUGIN / "halljoy_plugin_telemetry.h").read_text(encoding="utf-8-sig")
    backend = (ROOT / "HallJoy" / "backend.h").read_text(encoding="utf-8-sig")
    ui = (ROOT / "HallJoy" / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
    runner = (REPO / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
    build = (REPO / "tools" / "build.ps1").read_text(encoding="utf-8-sig")

    sun_files = sorted(PLUGIN.glob("*.sun"))
    require(len(sun_files) == 6, "all six private UAP targets are covered")
    for sun in sun_files:
        text = sun.read_text(encoding="utf-8-sig")
        require("-DUAP_POLL_TARGET_US=1000" in text, f"{sun.name} targets at most 1 kHz")
        require("UAP_POLL_SLEEP_MS" not in text, f"{sun.name} has no fixed post-poll sleep")

    require("kDefaultTargetIntervalUs = 1000" in policy and
            "kDefaultMaximumFailureBackoffUs = 64000" in policy,
            "policy fixes the 1 ms target and 64 ms failure cap")
    require("cycle_started_us_ + target_interval_us_" in policy and
            "if (now_us >= deadline)" in policy,
            "successful transactions wait only to their cycle deadline")
    require("failure_streak_" in policy and "doubled" in policy,
            "failed transactions use bounded exponential backoff")

    worker = function_body(main_cpp, "static void start_device_worker(Device& dev)")
    require("const bool is_poll_device = kbd.isPoll()" in worker,
            "worker uses the transport's poll classification")
    require(worker.count("if (is_poll_device)") == 2 and
            "PollPacingPolicy" in worker and "CompleteCycle" in worker,
            "deadline policy wraps only poll-device updates")
    require("std::this_thread::sleep_for" in worker and
            "kbd.madlions.consecutive_failed_reports == 0" in worker,
            "runtime applies microsecond waits and Madlions transient-error backoff")
    require("UAP_POLL_SLEEP_MS" not in main_cpp,
            "legacy additive polling sleep is absent from runtime source")

    require("DeviceFlag_DeadlinePacedWorker = 1u << 6" in telemetry and
            "BackendAnalogDeviceFlag_DeadlinePacedWorker = 1u << 6" in backend,
            "plugin and UI telemetry agree on the deadline-paced flag")
    require("deadline-paced background HID polling" in ui and
            "unthrottled background HID polling (diagnostic)" in ui,
            "UI distinguishes production pacing from diagnostic unthrottled mode")
    require("uap_poll_pacing_test.cpp" in runner,
            "portable runner includes the deterministic rate/CPU model")
    require("uap_poll_pacing_static_audit.py" in build and
            "halljoy_uap_poll_pacing.h" in build and
            "uap_poll_pacing_test.cpp" in build,
            "official build requires every V14-11A regression asset")

    print("UAP_POLL_PACING_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
