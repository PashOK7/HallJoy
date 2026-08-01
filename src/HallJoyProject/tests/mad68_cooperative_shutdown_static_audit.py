#!/usr/bin/env python3
"""Verify the V14-12D/S07 MAD68 cooperative-shutdown and A9 safety contract."""

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


def require_post_read_stop(body: str, read_marker: str, process_marker: str,
                           label: str) -> None:
    read = body.index(read_marker)
    stop = body.index("g_stop.load(std::memory_order_acquire)", read)
    process = body.index(process_marker, read)
    require(read < stop < process, label)


def main() -> int:
    mad = (HALL / "mad68pr_backend.cpp").read_text(encoding="utf-8-sig")
    app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(
        encoding="utf-8"
    )

    session = function_body(mad, "class Session")
    close = function_body(mad, "void Close()")
    cancel = function_body(mad, "void CancelActiveSessionRead()")
    pump = function_body(mad, "void PumpFor(")
    wait_ack = function_body(mad, "bool WaitForAck(")
    wait_release = function_body(mad, "bool WaitForAllReleased(")
    validate = function_body(mad, "bool ValidateStreamAfterActivation(")
    send_command = function_body(mad, "bool SendCommand(")
    run_session = function_body(mad, "bool RunSession(const HidPath& path)")
    start = function_body(mad, "bool Mad68ProR_Start()")
    stop = function_body(
        mad, "halljoy::lifecycle::StopResult Mad68ProR_StopGeneration("
    )
    worker = function_body(mad, "std::uint32_t Mad68WorkerBody()")
    descriptor = function_body(
        mad, "const NativeAnalogBackendDescriptor& Mad68ProR_GetNativeBackendDescriptor()"
    )

    require("std::thread" not in mad and "TerminateThread" not in mad,
            "MAD68 has no unbounded std::thread join or forced termination")
    require("std::unique_ptr<HidIoOperation> pendingRead_" in session and
            "pendingRead_->CancelAndDrain" in session,
            "the persistent overlapped read remains owned through terminal reap")
    require("RegisterActiveSessionRead(this, read_.value)" in session and
            "UnregisterActiveSessionRead(this)" in close and
            close.index("UnregisterActiveSessionRead(this)") <
            close.index("CancelPendingRead()") < close.index("read_.reset()"),
            "session registration is withdrawn before worker-owned cancel/reap/close")
    require("CloseHandle" not in cancel and
            "CancelIoEx(g_activeSessionReadHandle, nullptr)" in cancel,
            "owner stop may cancel but never close the active session read handle")
    require("Session session(path, true);" in run_session,
            "only the live worker session publishes its active read handle")

    require_post_read_stop(
        pump, "session.ReadPayload(", "ProcessPayload(",
        "PumpFor cannot publish a completion after stop")
    require_post_read_stop(
        wait_ack, "session.ReadPayload(", "DecodeControlResponse(",
        "ACK processing cannot consume a completion after stop")
    require_post_read_stop(
        wait_release, "session.ReadPayload(50", "ProcessPayload(",
        "release waiting cannot publish a completion after stop")
    require_post_read_stop(
        validate, "session.ReadPayload(kReadSliceMs", "ProcessPayload(",
        "activation validation cannot publish a completion after stop")
    require_post_read_stop(
        run_session, "session.ReadPayload(kReadSliceMs", "ProcessPayload(",
        "the main session cannot publish a completion after stop")

    require("opcode == mad68pr::kArmOpcode" in send_command and
            "g_stop.load(std::memory_order_acquire)" in send_command,
            "A8 is rejected once cooperative stop begins")
    cleanup = run_session.index("session closing: final best-effort A9")
    require(cleanup > run_session.index("while (!g_stop.load") and
            "session.Send(cleanup, mad68pr::kRestoreInputOpcode)" in
            run_session[cleanup:],
            "worker still sends the mandatory audited A9 after leaving the loop")

    require("std::lock_guard<std::mutex> serviceLock(g_serviceMutex)" in start and
            "_beginthreadex" in start and "g_threadHandle" in start,
            "start is serialized and creates a waitable native generation")
    require("SignalWakeEvent();" in stop and "CancelActiveSessionRead();" in stop and
            stop.index("CancelActiveSessionRead();") <
            stop.index("WaitForSingleObject(g_threadHandle"),
            "stop wakes and cancels the persistent read before joining")
    require("WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs)" in stop and
            "ObserveWorkerJoin" in stop,
            "MAD68 stop has one bounded truthful join")
    require("thread_handle_retained=1" in stop and
            "wake_event_retained=1" in stop and
            "active_session_read_retained=%d" in stop and
            "restart_blocked=1" in stop,
            "incomplete stop records and retains generation ownership")
    require(stop.index("if (wait != WAIT_OBJECT_0)") <
            stop.index("CloseHandle(g_threadHandle)"),
            "thread handle closes only after confirmed worker completion")
    require(stop.index("if (wait != WAIT_OBJECT_0)") <
            stop.index("CloseHandle(g_wakeEvent)"),
            "wake event closes only after confirmed worker completion")
    require("return Mad68ProR_StopGeneration(generation);" in descriptor,
            "native registry receives the real MAD68 stop result")
    require("nativeBackendsStopped" in app and
            "component=native-analog dependent_cleanup_skipped=1" in app,
            "application contains an incomplete MAD68 generation")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in worker and
            "--halljoy-test-mad68-stop-timeout" in mad,
            "runtime timeout injection is simulator-only")
    require("InjectMad68StopTimeout" in runner and "expected 2" in runner,
            "simulator runner verifies MAD68 process containment")

    print("MAD68_COOPERATIVE_SHUTDOWN_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
