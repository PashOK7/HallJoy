#!/usr/bin/env python3
"""Verify S02B.2 SparkLink C++/SEH worker exception containment."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
SOURCE = HALL / "backend_sparklink.inc"


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
            "SparkLink includes the common allocation-free exception barrier")
    require("static DWORD SparkPollThreadProcImpl()" in source,
            "SparkLink polling algorithm remains in a separate body")
    require("static DWORD SparkPollThreadProcCpp() noexcept" in source,
            "SparkLink has a noexcept C++ boundary")
    require("static DWORD WINAPI SparkPollThreadProc(LPVOID) noexcept" in source,
            "SparkLink OS entry is noexcept")

    cpp_entry = function_body(source, "static DWORD SparkPollThreadProcCpp() noexcept")
    require("RunWorkerEntryBarrier" in cpp_entry,
            "SparkLink C++ boundary invokes the common barrier")
    for token in (
        "SparkPollThreadProcImpl",
        "SparkWorkerOnCppFault",
        "SparkWorkerOnCppCompletion",
        "0xE0520003u",
    ):
        require(token in cpp_entry, f"SparkLink C++ boundary wires {token}")

    os_entry = function_body(source, "static DWORD WINAPI SparkPollThreadProc(LPVOID) noexcept")
    require("__try" in os_entry and "__except (EXCEPTION_EXECUTE_HANDLER)" in os_entry,
            "existing structured-exception boundary is preserved")
    require("SparkPollThreadProcCpp()" in os_entry,
            "SEH wrapper delegates normal execution to the C++ barrier")
    require("SparkWorkerOnStructuredFault(code)" in os_entry,
            "SEH path publishes the same fail-safe state")
    require("OutputDebugStringA" in os_entry and "DebugLog_Write" not in os_entry,
            "SEH boundary avoids the normal logging pipeline")

    release = function_body(source, "static bool SparkReleasePublishedInput() noexcept")
    require("m.exchange(0" in release,
            "SparkLink exceptional cleanup releases all published analogue values")
    require("g_sparkConnected.store(false" in release,
            "SparkLink exceptional cleanup clears connected publication")
    require("g_sparkLastPacketMs.store(0" in release,
            "SparkLink exceptional cleanup clears freshness publication")
    require("RealtimeLoop_NotifyInputChanged();" in release,
            "SparkLink exceptional cleanup wakes realtime for the neutral snapshot")

    cpp_fault = function_body(source, "static void SparkWorkerOnCppFault(")
    require("g_sparkWorkerFaultRecord = record" in cpp_fault and
            "g_sparkWorkerFaultKind.store(record.kind" in cpp_fault,
            "SparkLink stores a fixed C++ fault record")
    require("g_sparkWriteCapable.store(false" in cpp_fault,
            "SparkLink fault closes further HID writes")
    require("SparkReleasePublishedInput();" in cpp_fault,
            "SparkLink C++ fault publishes neutral state")
    require("OutputDebugStringA(record.message" in cpp_fault,
            "SparkLink C++ fault emits allocation-free diagnostics")

    completion = function_body(source, "static void SparkWorkerOnCppCompletion(")
    require("SparkReleasePublishedInput();" in completion,
            "all SparkLink exits publish neutral state")
    require("g_sparkWorkerExited.store(true" in completion,
            "all C++ exits publish generation completion")

    seh_fault = function_body(source, "static void SparkWorkerOnStructuredFault(DWORD code) noexcept")
    require("CopyWorkerExceptionMessage(record, \"structured exception\")" in seh_fault,
            "SEH fault uses the fixed fault record")
    require("g_sparkWorkerSehCode.store(code" in seh_fault,
            "SEH code is retained separately")
    require("SparkReleasePublishedInput();" in seh_fault and
            "g_sparkWorkerExited.store(true" in seh_fault,
            "SEH fault publishes neutral state and completion")

    start = function_body(source, "static bool SparkStart()")
    require("g_sparkWorkerFaultRecord = {}" in start and
            "WorkerExceptionKind::None" in start and
            "g_sparkWorkerSehCode.store(0" in start,
            "new SparkLink generation resets old fault diagnostics")
    require("g_sparkWorkerExited.store(false" in start,
            "new SparkLink generation starts as not completed")
    require(start.find("g_sparkWorkerExited.store(false") < start.find("CreateThread"),
            "completion handshake is reset before thread creation")
    require(start.count("g_sparkWorkerExited.load(std::memory_order_acquire)") >= 2,
            "SparkLink start checks early worker exit before and after publication")
    require("poller exited during startup" in start and
            "poller exited while publishing startup" in start,
            "early exit cannot be reported as a successful connection")

    stop = function_body(source, "static halljoy::lifecycle::StopResult SparkStopLocked()")
    require("g_sparkWorkerExited.store(true" in stop,
            "owner-side reap publishes completed generation")
    require("TerminateThread" not in source,
            "SparkLink has no forced thread termination")

    print("SPARKLINK_EXCEPTION_BOUNDARY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
