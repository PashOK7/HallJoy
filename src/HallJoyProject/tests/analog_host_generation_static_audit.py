#!/usr/bin/env python3
"""Verify analog-host parent generation ownership and bounded rollback."""

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
    host = (HALL / "analog_host_client.cpp").read_text(encoding="utf-8-sig")
    backend = (HALL / "backend.cpp").read_text(encoding="utf-8-sig")
    backend_h = (HALL / "backend.h").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    ensure = function_body(host, "bool EnsureClientResources()")
    stop = function_body(host, "WootingAnalogResult AnalogHostClient_Uninitialise()")
    wait_group = function_body(host, "bool WaitForClientWorkers(")
    cleanup = function_body(host, "void CloseClientResourcesLocked() noexcept")
    backend_stop = function_body(backend, "bool Backend_Shutdown()")

    require('#include "worker_lifecycle.h"' in host and
            "WorkerLifecycle lifecycle" in host and "restartBlocked" in host,
            "parent client owns one generation and restart poison")
    require("BeginStart()" in ensure and "ConfirmRunning(start.generation)" in ensure,
            "successful parent start publishes one generation")
    require("RequestStop(start.generation)" in ensure and
            "WaitForClientWorkers(bridge, nullptr, 3000" in ensure,
            "partial start cooperatively reaps the already-created bridge")
    require("ConfirmJoined(start.generation)" in ensure and
            "partial_start.rollback" in ensure,
            "confirmed partial-start rollback releases ownership")
    require("partial_start.poisoned" in ensure and
            "MarkPoisoned(start.generation" in ensure,
            "incomplete partial-start rollback retains a poisoned generation")
    require("WaitForMultipleObjects(count, workers, TRUE" in wait_group,
            "parent workers share one bounded group wait")
    require("WaitForClientWorkers(snapshotBridge, supervisor, 6000" in stop and
            "WaitForClientWorkers(snapshotBridge, supervisor, 4000" in stop,
            "shutdown has bounded graceful and child-job containment phases")
    require("TerminateJobObject" in stop and "stop.timeout" in stop and
            "restartBlocked.store(true" in stop,
            "incomplete parent join terminates the child job and poisons restart")
    require(stop.find("CloseClientResourcesLocked()") > stop.find("if (!cleanThreadShutdown)"),
            "resources close only after confirmed parent-worker completion")
    require("CloseHandle(g_client.snapshotBridgeThread)" in cleanup and
            "CloseHandle(g_client.supervisorThread)" in cleanup and
            "UnmapViewOfFile(g_client.shared)" in cleanup,
            "one owner-side cleanup releases thread and IPC resources")
    require("g_client.started" not in host,
            "lossy started boolean no longer controls generation replacement")
    require("[[nodiscard]] bool Backend_Shutdown();" in backend_h and
            "analog_host_joined=%d" in backend_stop,
            "backend shutdown reports analog-host completion")
    require("backendStopped = Backend_Shutdown()" in app and
            "component=backend analog_host_or_native_join_incomplete=1" in app,
            "application selects process containment after analog-host poison")
    require("InjectAnalogHostBridgeStopTimeout" in runner and
            "InjectAnalogHostSupervisorStartFailure" in runner and
            "partial-start rollback scenario" in runner,
            "simulator covers parent timeout and partial-start rollback")
    require("--halljoy-test-analog-host-bridge-stop-timeout" in host and
            "--halljoy-test-analog-host-supervisor-start-failure" in host,
            "runtime fault injection is simulator-only")

    print("ANALOG_HOST_GENERATION_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
