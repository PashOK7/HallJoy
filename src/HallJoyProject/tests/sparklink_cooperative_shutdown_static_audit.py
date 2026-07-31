#!/usr/bin/env python3
"""Verify the V14-06D SparkLink cooperative-shutdown contract."""

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
    spark = (HALL / "backend_sparklink.inc").read_text(encoding="utf-8-sig")
    backend = (HALL / "backend.cpp").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    start = function_body(spark, "static bool SparkStart()")
    stop_locked = function_body(spark, "static halljoy::lifecycle::StopResult SparkStopLocked()")
    worker = function_body(spark, "static DWORD SparkPollThreadProcImpl()")
    service_start = function_body(spark, "static bool SparkStartService()")
    service_stop = function_body(spark, "static halljoy::lifecycle::StopResult SparkStopService()")
    hotplug_tick = function_body(spark, "static void SparkTickHotplug(ULONGLONG nowMs)")

    require("TerminateThread" not in spark,
            "SparkLink has no forced thread termination")
    require("BeginStart" in start and "ConfirmRunning" in start and "FailStartBeforeWorker" in start,
            "worker start publishes a truthful internal generation")
    require("g_sparkLifecycleMutex" in start and "SparkStopLocked" in start,
            "hotplug start/stop transitions are serialized")
    require("std::timed_mutex" in spark and "try_lock_for" in spark and
            "stop.lock_timeout" in spark and "g_sparkRestartBlocked" in start,
            "shutdown cannot wait indefinitely behind a blocking hotplug probe")
    require("SetEvent(g_sparkStopEvent)" in stop_locked and "CancelIoEx(g_sparkHandle" in stop_locked and
            stop_locked.index("CancelIoEx") < stop_locked.index("WaitForSingleObject"),
            "stop signals the event and cancels HID I/O before joining")
    require("ObserveWorkerJoin" in stop_locked,
            "stop uses the common join policy")
    require("MarkPoisoned" in stop_locked and "thread_handle_retained=1" in stop_locked and
            "stop_event_retained=%d" in stop_locked and "restart_blocked=1" in stop_locked,
            "incomplete join retains resources and poisons restart")
    require(stop_locked.index("if (!observedJoin.Completed())") < stop_locked.index("CloseHandle(g_sparkThread)"),
            "thread handle closes only after confirmed completion")
    require(stop_locked.index("if (!observedJoin.Completed())") < stop_locked.index("CloseHandle(g_sparkHandle)"),
            "HID handle closes only after confirmed completion")
    require("const auto stopped = SparkStopService();" in backend and "stopped.RestartSafe()" in backend,
            "descriptor forwards truthful stop completion to the registry")
    require("SparkStart();" in service_start and "return true;" in service_start and
            "&SparkStartService" in backend,
            "outer registry remains running for later hotplug workers")
    require("g_sparkServiceStopRequested.store(true" in service_stop and
            "g_sparkServiceRunning.store(false" in service_stop and
            service_stop.index("g_sparkServiceRunning.store(false") < service_stop.index("SparkStop()"),
            "outer service gate closes before the active poller is stopped")
    require("SparkServiceAllowsStart()" in start and "SparkServiceAllowsStart()" in hotplug_tick,
            "worker start and hotplug tick both honor outer service ownership")
    require("SparkTickHotplug" in service_stop and "test.service_stop_probe" in service_stop and
            "--halljoy-test-spark-service-shutdown" in worker,
            "simulator probes reconnect after poller stop while service is closed")
    require("nativeBackendsStopped" in app and "component=native-analog dependent_cleanup_skipped=1" in app,
            "application blocks dependent teardown after native poison")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in worker and
            "--halljoy-test-spark-stop-timeout" in worker,
            "runtime timeout injection is simulator-only")
    require("InjectSparkStopTimeout" in runner and "expected 2" in runner,
            "simulator runner verifies SparkLink process containment")
    require("InjectSparkShutdownRace" in runner and "reconnect after service stop" in runner,
            "simulator runner rejects reconnect after outer service stop")

    print("SPARKLINK_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
