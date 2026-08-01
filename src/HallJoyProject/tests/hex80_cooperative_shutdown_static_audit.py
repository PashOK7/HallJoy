#!/usr/bin/env python3
"""Verify the V14-12C/S07 Hex80 cooperative-shutdown contract."""

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
    hex80 = (HALL / "hex80_backend.cpp").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(
        encoding="utf-8"
    )

    timed_io = function_body(hex80, "bool RunTimedIo(")
    request = function_body(hex80, "bool Request(")
    session = function_body(hex80, "bool RunSession(const Candidate& candidate)")
    active_scope = function_body(hex80, "class ScopedActiveSessionHandle")
    cancel = function_body(hex80, "void CancelActiveSessionIo()")
    start = function_body(hex80, "bool Hex80_Start()")
    stop = function_body(
        hex80, "halljoy::lifecycle::StopResult Hex80_StopGeneration("
    )
    worker = function_body(hex80, "std::uint32_t Hex80WorkerBody()")
    descriptor = function_body(
        hex80, "const NativeAnalogBackendDescriptor& Hex80_GetNativeBackendDescriptor()"
    )

    require("std::thread" not in hex80 and "TerminateThread" not in hex80,
            "Hex80 has no unbounded std::thread join or forced termination")
    require("HidIoOperation operation(handle);" in timed_io and
            "operation.CancelAndDrain" in timed_io,
            "each pending HID operation remains owned through terminal reap")
    require("g_stop.load(std::memory_order_acquire)" in request,
            "the request loop observes cooperative stop before issuing another read")
    request_call = session.index("const bool received = session.Request(")
    post_request_stop = session.index(
        "if (g_stop.load(std::memory_order_acquire)) return true;", request_call
    )
    publish = session.index("g_milli[entry.hid].exchange", request_call)
    require(request_call < post_request_stop < publish,
            "a completed request cannot publish input after stop begins")
    require("Session session(candidate);" in session and
            "ScopedActiveSessionHandle activeSession(session.Handle());" in session and
            session.index("Session session(candidate);") <
            session.index("ScopedActiveSessionHandle activeSession(session.Handle());"),
            "active registration unwinds before the worker-owned Session closes HID")
    require("CloseHandle" not in active_scope and "CloseHandle" not in cancel and
            "CancelIoEx(g_activeSessionHandle, nullptr)" in cancel,
            "owner threads may cancel but never close the active worker HID handle")

    require("std::lock_guard<std::mutex> serviceLock(g_serviceMutex)" in start and
            "_beginthreadex" in start and "g_threadHandle" in start,
            "start is serialized and creates a waitable native generation")
    require("SetEvent(g_wakeEvent)" in stop and "CancelActiveSessionIo();" in stop and
            stop.index("CancelActiveSessionIo();") <
            stop.index("WaitForSingleObject(g_threadHandle"),
            "stop wakes and cancels active I/O before joining")
    require("WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs)" in stop and
            "ObserveWorkerJoin" in stop,
            "Hex80 stop has one bounded truthful join")
    require("thread_handle_retained=1" in stop and
            "wake_event_retained=1" in stop and
            "active_hid_retained=%d" in stop and "restart_blocked=1" in stop,
            "incomplete stop records and retains all generation resources")
    require(stop.index("if (wait != WAIT_OBJECT_0)") <
            stop.index("CloseHandle(g_threadHandle)"),
            "thread handle closes only after confirmed worker completion")
    require(stop.index("if (wait != WAIT_OBJECT_0)") <
            stop.index("CloseHandle(g_wakeEvent)"),
            "wake event closes only after confirmed worker completion")
    require("return Hex80_StopGeneration(generation);" in descriptor,
            "native registry receives the real Hex80 stop result")
    require("nativeBackendsStopped" in app and
            "component=native-analog dependent_cleanup_skipped=1" in app,
            "application contains an incomplete Hex80 generation")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in worker and
            "--halljoy-test-hex80-stop-timeout" in hex80,
            "runtime timeout injection is simulator-only")
    require("InjectHex80StopTimeout" in runner and "expected 2" in runner,
            "simulator runner verifies Hex80 process containment")

    print("HEX80_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
