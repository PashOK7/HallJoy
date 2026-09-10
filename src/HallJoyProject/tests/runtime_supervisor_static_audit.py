#!/usr/bin/env python3
"""Verify that runtime recovery has one non-UI owner and a safe teardown edge."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    require(start >= 0, f"found function: {signature}")
    opening = source.find("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise SystemExit(f"FAIL: unterminated function: {signature}")


def main() -> int:
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    supervisor = (HALL / "runtime_supervisor.cpp").read_text(encoding="utf-8-sig")
    header = (HALL / "runtime_supervisor.h").read_text(encoding="utf-8-sig")
    project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
    transaction = (HALL / "engine_runtime_transaction.h").read_text(encoding="utf-8-sig")

    startup = body(app, "static bool AppStartBackendDependents(")
    rollback = body(app, "static bool AppRollbackBackendStartup(")
    shutdown = body(app, "static void AppShutdownNoThrow(")
    timer = body(app, "case WM_TIMER:")
    stop = body(supervisor, "halljoy::lifecycle::StopResult RuntimeSupervisor_Stop()")
    cpp_entry = body(supervisor, "DWORD RuntimeSupervisorThreadProcCpp() noexcept")
    os_entry = body(supervisor, "DWORD WINAPI RuntimeSupervisorThreadProc(void*) noexcept")

    require(all(token in project for token in (
        'ClCompile Include="runtime_supervisor.cpp"',
        'ClInclude Include="runtime_supervisor.h"')),
        "runtime supervisor is compiled in the product project")
    require(all(token in header for token in (
        "RuntimeSupervisor_Start", "RuntimeSupervisor_Stop", "RuntimeSupervisor_IsRunning")),
        "supervisor exposes bounded lifecycle/status surface")
    require("progress.runtimeSupervisor = true" in startup and
            "RuntimeSupervisor_Start()" in startup and
            startup.index("RuntimeSupervisor_Start()") > startup.index("RealtimeLoop_Start()"),
            "supervisor starts only after realtime dependency startup")
    require("RuntimeSupervisor_Stop()" in rollback and
            rollback.index("RuntimeSupervisor_Stop()") < rollback.index("RealtimeLoop_Stop()"),
            "rollback releases supervisor ownership before realtime")
    require("EngineRuntimeOwner_Stop()" in shutdown and
            shutdown.index("EngineRuntimeOwner_Stop()") < shutdown.index("OverlayServer_Stop()") and
            transaction.index("operations.StopRecoverySupervisor(nativeError)") <
            transaction.index("operations.StopRealtime(nativeError)"),
            "shutdown joins supervisor before any recovered dependency")
    require("Backend_EnsureOutputRuntimeHealthy()" not in timer and
            "RealtimeLoop_Start()" not in timer and
            "RealtimeLoop_Stop()" not in timer,
            "UI timer is no longer a realtime/output recovery owner")
    require(all(token in supervisor for token in (
        "RealtimeLoop_IsRunning()", "RealtimeLoop_Stop()", "RealtimeLoop_Start()",
        "Backend_EnsureOutputRuntimeHealthy()", "kRuntimeSupervisorPeriodMs")),
        "single supervisor owns bounded realtime and output recovery")
    require("ObserveWorkerJoin(" in stop and "MarkPoisoned" in stop and
            "thread_handle_retained=1" in stop and
            stop.index("if (!observedJoin.Completed())") < stop.index("CloseHandle(g_thread)"),
            "incomplete supervisor join retains handles and blocks restart")
    require("TerminateThread" not in supervisor,
            "supervisor never force-terminates a thread")
    require("try" in cpp_entry and "catch (...)" in cpp_entry and
            "RuntimeSupervisorBody()" in cpp_entry and
            "__try" not in cpp_entry,
            "C++ exception boundary is separate from the OS entry")
    require("__try" in os_entry and "__except (EXCEPTION_EXECUTE_HANDLER)" in os_entry and
            "RuntimeSupervisorThreadProcCpp()" in os_entry and "catch" not in os_entry,
            "SEH wrapper calls only the C++-safe entry")
    require("kShutdownWatchdogTimeoutMs = 30000" in app and
            "final process" in app and "bounded output/UAP containment" in app,
            "outer shutdown deadline covers bounded supervisor and output/UAP containment")

    print("RUNTIME_SUPERVISOR_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
