#!/usr/bin/env python3
"""Verify S02B.3 Addressed main/reader exception containment."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "HallJoy" / "addressed_analog_backend.cpp"


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

    worker_body = function_body(source, "std::uint32_t AddressedWorkerBody()")
    require("RunSession(path, profile);" in worker_body,
            "main Addressed algorithm remains in a separate worker body")

    worker_entry = function_body(source, "void AddressedWorkerEntry() noexcept")
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
    require("g_thread.joinable()" in start and "g_thread.join()" in start,
            "a completed main generation is reaped before replacement")
    require(start.find("g_thread.joinable()") < start.find("std::thread(AddressedWorkerEntry)"),
            "join occurs before assigning the next std::thread")
    require("g_workerFaultRecord = {}" in start and
            "g_readerFaultRecord = {}" in start and
            "g_workerExited.store(false" in start,
            "new generation resets old fault and completion state")
    require("std::thread(AddressedWorkerEntry)" in start,
            "start uses the noexcept main worker entry")
    require("g_workerExited.load(std::memory_order_acquire)" in start,
            "start rejects a worker that completed during startup")

    require("ForceCloseReaderHandle();" in session,
            "existing force-close fallback remains unchanged and deferred")
    require("TerminateThread" not in source,
            "Addressed still does not use TerminateThread")

    print("ADDRESSED_EXCEPTION_BOUNDARY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
