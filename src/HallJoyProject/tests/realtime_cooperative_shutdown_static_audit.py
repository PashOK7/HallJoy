#!/usr/bin/env python3
"""Verify the V14-06 realtime cooperative-shutdown ownership contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


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
    realtime = (HALL / "realtime_loop.cpp").read_text(encoding="utf-8-sig")
    header = (HALL / "realtime_loop.h").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    main_cpp = (HALL / "main.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
    simulator_runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    stop = function_body(realtime, "halljoy::lifecycle::StopResult RealtimeLoop_Stop()")
    thread_body = function_body(realtime, "static DWORD RealtimeThreadBody()")

    require("TerminateThread" not in realtime,
            "realtime loop has no forced thread termination")
    require("StopResult RealtimeLoop_Stop()" in header,
            "public stop contract reports exact completion")
    require("WakeByAddressAll" in stop and stop.index("WakeByAddressAll") < stop.index("WaitForSingleObject"),
            "stop wakes the address wait before joining")
    require("ObserveWorkerJoin" in stop,
            "production stop uses the fault-injected join policy")
    require("MarkPoisoned" in stop and "handle_retained=1" in stop and "restart_blocked=1" in stop,
            "incomplete join retains ownership and poisons restart")
    require(stop.index("if (!observedJoin.Completed())") < stop.index("CloseHandle(g_thread)"),
            "thread handle closes only after confirmed completion")
    require("RealtimeThreadResources resources{}" in thread_body,
            "worker owns MMCSS, timer and multimedia-period cleanup")
    require("Backend_Tick();" in thread_body,
            "realtime algorithm remains in the worker body")
    require("realtimeStop.RestartSafe()" in app and
            app.index("realtimeStop.RestartSafe()") < app.index("Backend_Shutdown();", app.index("realtimeStop.RestartSafe()")),
            "backend teardown is guarded by confirmed realtime join")
    require("stopped.RestartSafe()" in app,
            "watchdog blocks restart after an incomplete stop")
    require("App_RequiresImmediateProcessExit()" in main_cpp and "TerminateProcess" in main_cpp,
            "poisoned process exit skips unsafe CRT teardown")
    require("worker_join_policy_test.cpp" in runner,
            "join timeout/failure policy is in the portable gate")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in thread_body and
            "--halljoy-test-realtime-stop-timeout" in thread_body,
            "runtime timeout injection is simulator-only")
    require("InjectRealtimeStopTimeout" in simulator_runner and
            "process_exit.poisoned" in simulator_runner and "expected 2" in simulator_runner,
            "simulator runner verifies poisoned process containment")

    print("REALTIME_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
