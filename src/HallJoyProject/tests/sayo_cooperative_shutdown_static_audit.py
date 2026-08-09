#!/usr/bin/env python3
"""Verify the V14-06E Sayo reader-group cooperative-shutdown contract."""

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
    sayo = (HALL / "backend_sayo.inc").read_text(encoding="utf-8-sig")
    backend = (HALL / "backend.cpp").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    start = function_body(sayo, "static bool SayoStart()")
    service_start = function_body(sayo, "static bool SayoStartService()")
    stop = function_body(sayo, "static halljoy::lifecycle::StopResult SayoStopLocked()")
    reader = function_body(sayo, "static DWORD SayoReaderThreadBody(SayoReader* reader)")

    require("TerminateThread" not in sayo,
            "Sayo has no forced thread termination")
    require("BeginStart" in start and "ConfirmRunning" in start and "FailStartBeforeWorker" in start,
            "reader group publishes one truthful internal generation")
    require("std::timed_mutex" in sayo and "try_lock_for" in sayo and
            "stop.lock_timeout" in sayo and "g_sayoRestartBlocked" in start,
            "shutdown cannot wait indefinitely behind device discovery")
    require("SetEvent(g_sayoStopEvent)" in stop and "CancelIoEx(reader.handle" in stop and
            stop.index("CancelIoEx") < stop.index("WaitForMultipleObjects"),
            "stop wakes and cancels every reader before joining")
    require("SayoReleasePublishedInput()" in stop and
            sayo.count("SayoReleasePublishedInput()") >= 2,
            "stop publishes neutral input before and after the reader join")
    require("WaitForMultipleObjects" in stop and "TRUE, 3000" in stop,
            "all readers share one three-second join deadline")
    require("ObserveWorkerJoin" in stop and "MarkPoisoned" in stop,
            "reader-group join uses the common poison policy")
    require("reader_group_retained=1" in stop and "stop_event_retained=1" in stop and
            "restart_blocked=1" in stop,
            "incomplete join retains group ownership and blocks restart")
    require(stop.index("if (!observedJoin.Completed())") < stop.index("CloseHandle(reader.thread)"),
            "reader handles close only after complete group join")
    require("SayoStart();" in service_start and "return true;" in service_start and
            "&SayoStartService" in backend,
            "outer registry remains running for later hotplug readers")
    require("const auto stopped = SayoStop();" in backend and "stopped.RestartSafe()" in backend,
            "descriptor forwards truthful completion to the registry")
    require("nativeBackendsStopped" in app and "component=native-analog dependent_cleanup_skipped=1" in app,
            "application blocks dependent teardown after Sayo poison")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in reader and
            "--halljoy-test-sayo-stop-timeout" in reader,
            "runtime timeout injection is simulator-only")
    require("InjectSayoStopTimeout" in runner and "Sayo-timeout" in runner,
            "simulator runner verifies Sayo process containment")

    print("SAYO_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
