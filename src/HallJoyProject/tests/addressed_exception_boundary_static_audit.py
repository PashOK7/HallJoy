#!/usr/bin/env python3
"""Verify S02B.3 exception containment and S06 Addressed I/O ownership."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "HallJoy" / "addressed_analog_backend.cpp"
APP = ROOT / "HallJoy" / "app.cpp"
RUNNER = ROOT.parents[1] / "tools" / "run_analog_simulator.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    require(start >= 0, f"found function signature: {signature}")
    brace = text.find("{", start)
    require(brace >= 0, f"found function body: {signature}")
    depth = 0
    for index in range(brace, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:index]
    raise SystemExit(f"FAIL: unterminated function: {signature}")


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8-sig", errors="strict")
    app = APP.read_text(encoding="utf-8-sig", errors="strict")
    runner = RUNNER.read_text(encoding="utf-8-sig", errors="strict")

    require('#include "worker_exception_barrier.h"' in source,
            "Addressed includes the common allocation-free exception barrier")
    require("void ResetPublished(const DeviceProfile* profile = nullptr) noexcept" in source,
            "neutral publication reset is noexcept at exception boundaries")

    reader_body = function_body(source, "std::uint32_t ReaderLoopBody(const HidPath* path)")
    require("HidIoOperation operation(handle);" in reader_body and
            "PublishResponse(buffer.data(), transferred, NowUs());" in reader_body,
            "reader HID algorithm remains in a separate body")
    require("ScopedRegisteredReaderHandle registeredHandle(handle);" in reader_body,
            "registered reader handle is released during stack unwinding")
    require("HidIoOperation operation(handle);" in reader_body and
            "operation.CancelAndDrain" in reader_body,
            "reader owns OVERLAPPED lifetime through confirmed cancellation reap")
    require("!g_stop.load(std::memory_order_acquire)" in reader_body and
            "!g_deviceChanged.load(std::memory_order_acquire)" in reader_body,
            "a completion racing stop cannot republish non-neutral input")

    reader_entry = function_body(source, "void AddressedReaderEntry(const HidPath* path) noexcept")
    for token in (
        "RunWorkerEntryBarrier",
        "ReaderLoopBody",
        "AddressedReaderOnFault",
        "AddressedReaderOnCompletion",
        "0xE0520004u",
    ):
        require(token in reader_entry, f"reader barrier wires {token}")

    reader_fault = function_body(source, "void AddressedReaderOnFault(")
    require("g_readerFaultRecord = record" in reader_fault and
            "g_readerFaultKind.store(record.kind" in reader_fault,
            "reader fault stores a fixed diagnostic record")
    require("g_deviceChanged.store(true" in reader_fault and
            "ResetPublished();" in reader_fault,
            "reader fault terminates the session and publishes neutral input")
    require("OutputDebugStringA(record.message" in reader_fault,
            "reader fault emits allocation-free diagnostics")

    reader_completion = function_body(source, "void AddressedReaderOnCompletion(")
    require("SetEvent(g_readerExitEvent)" in reader_completion,
            "every reader exit signals the owner completion event")

    session = function_body(source, "bool RunSession(const HidPath& path, const DeviceProfile& profile)")
    require("std::thread reader;" in session,
            "reader owner exists before the protected session block")
    require("reader = std::thread(AddressedReaderEntry, &path);" in session,
            "session starts only the noexcept reader entry")
    require(session.find("try") < session.find("reader = std::thread(AddressedReaderEntry, &path)"),
            "thread construction is inside the protected session block")
    require("catch (...)" in session and "if (reader.joinable()) reader.join();" in session,
            "exceptional session cleanup reaps a joinable reader")
    require(session.rfind("if (reader.joinable()) reader.join();") < session.rfind("throw;"),
            "reader is joined before the exception is rethrown")
    require("g_scheduler = nullptr;" in session and "g_pendingSendUs = 0;" in session,
            "exceptional cleanup clears stack-backed scheduler and pending token")
    require("cancellation_reap_delayed" in session and
            "owner=reader resources_retained=1" in session,
            "delayed cancellation retains the reader-owned handle and stack resources")
    require("ForceCloseReaderHandle" not in source and
            'L"force_close"' not in source,
            "no thread closes the reader HID handle to force pending I/O completion")

    worker_body = function_body(source, "std::uint32_t AddressedWorkerBody()")
    require("RunSession(path, profile);" in worker_body,
            "main Addressed algorithm remains in a separate worker body")

    worker_entry = function_body(source, "unsigned __stdcall AddressedWorkerEntry(void*) noexcept")
    for token in (
        "RunWorkerEntryBarrier",
        "AddressedWorkerBody",
        "AddressedWorkerOnFault",
        "AddressedWorkerOnCompletion",
        "0xE0520005u",
    ):
        require(token in worker_entry, f"main worker barrier wires {token}")

    worker_fault = function_body(source, "void AddressedWorkerOnFault(")
    require("g_workerFaultRecord = record" in worker_fault and
            "g_workerFaultKind.store(record.kind" in worker_fault,
            "main worker fault stores a fixed diagnostic record")
    require("g_stop.store(true" in worker_fault and
            "g_deviceChanged.store(true" in worker_fault and
            "ResetPublished();" in worker_fault,
            "main worker fault closes publication and future polling")

    completion = function_body(source, "void AddressedWorkerOnCompletion(")
    require("g_running.store(false" in completion and
            "g_workerExited.store(true" in completion,
            "every main worker exit publishes completion")

    start = function_body(source, "bool AddressedAnalog_Start()")
    require("std::lock_guard<std::mutex> serviceLock(g_serviceMutex)" in start,
            "Addressed start and stop transitions are serialized")
    require("g_workerFaultRecord = {}" in start and
            "g_readerFaultRecord = {}" in start and
            "g_workerExited.store(false" in start,
            "new generation resets old fault and completion state")
    require("_beginthreadex" in start and "AddressedWorkerEntry" in start,
            "start creates a natively waitable noexcept main worker")
    require("WaitForSingleObject(g_threadHandle, 0)" in start,
            "start rejects a worker that completed during startup")

    stop = function_body(source, "halljoy::lifecycle::StopResult AddressedAnalog_StopGeneration(")
    require("SetEvent(g_wakeEvent)" in stop and "CancelReaderIo();" in stop and
            stop.index("CancelReaderIo();") < stop.index("WaitForSingleObject(g_threadHandle"),
            "stop wakes both layers and requests reader cancellation before joining")
    require("WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs)" in stop and
            "ObserveWorkerJoin" in stop,
            "main worker join is bounded and uses the common truthful policy")
    require("thread_handle_retained=1" in stop and
            "signal_handles_retained=1" in stop and "restart_blocked=1" in stop,
            "incomplete stop retains generation resources and blocks restart")
    require(stop.index("if (wait != WAIT_OBJECT_0)") < stop.index("CloseHandle(g_threadHandle)"),
            "main thread handle closes only after confirmed completion")
    require(stop.index("if (wait != WAIT_OBJECT_0)") < stop.index("CloseHandle(g_readerExitEvent)"),
            "reader completion event closes only after the main generation joins")

    descriptor = function_body(source, "const NativeAnalogBackendDescriptor& AddressedAnalog_GetNativeBackendDescriptor()")
    require("return AddressedAnalog_StopGeneration(generation);" in descriptor,
            "native registry receives the real Addressed stop result")
    require("nativeBackendsStopped" in app and
            "component=native-analog dependent_cleanup_skipped=1" in app,
            "application contains an incomplete Addressed generation before dependent teardown")
    require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in worker_body and
            "--halljoy-test-addressed-stop-timeout" in source,
            "runtime timeout injection is simulator-only")
    require("InjectAddressedStopTimeout" in runner and "expected 2" in runner,
            "simulator runner verifies Addressed process containment")

    require("TerminateThread" not in source,
            "Addressed still does not use TerminateThread")

    print("ADDRESSED_EXCEPTION_BOUNDARY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
