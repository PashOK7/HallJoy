#!/usr/bin/env python3
"""Verify S02B.1 exception barriers for simple native backend workers."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def source(name: str) -> str:
    return (HALL / name).read_text(encoding="utf-8-sig", errors="strict")


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


def audit_backend(
    filename: str,
    body_name: str,
    entry_name: str,
    fault_name: str,
    completion_name: str,
    start_signature: str,
    thread_expression: str,
    safe_state_tokens: tuple[str, ...],
) -> None:
    text = source(filename)
    require('#include "worker_exception_barrier.h"' in text,
            f"{filename} includes the common exception barrier")
    require(re.search(rf"std::uint32_t\s+{re.escape(body_name)}\s*\(\)", text) is not None,
            f"{filename} keeps the worker algorithm in a uint32 body")
    require(re.search(rf"void\s+{re.escape(entry_name)}\s*\(\)\s*noexcept", text) is not None,
            f"{filename} worker entry is noexcept")

    entry = function_body(text, f"void {entry_name}() noexcept")
    require("RunWorkerEntryBarrier" in entry,
            f"{filename} worker entry invokes the common barrier")
    require(body_name in entry and fault_name in entry and completion_name in entry,
            f"{filename} barrier wires body, fault and completion callbacks")

    fault = function_body(text, f"void {fault_name}(")
    require("g_workerFaultRecord = record" in fault and
            "g_workerFaultKind.store(record.kind" in fault,
            f"{filename} stores a fixed exception record")
    require("g_stop.store(true" in fault,
            f"{filename} fault path closes the worker loop")
    for token in safe_state_tokens:
        require(token in fault, f"{filename} fault path publishes safe state: {token}")
    require("OutputDebugStringA(record.message" in fault,
            f"{filename} fault path emits allocation-free diagnostics")

    completion = function_body(text, f"void {completion_name}(")
    require("g_running.store(false" in completion,
            f"{filename} completion always clears running publication")

    start = function_body(text, start_signature)
    require("g_thread.joinable()" in start and "g_thread.join()" in start,
            f"{filename} reaps a completed generation before replacement")
    require(start.find("g_thread.joinable()") < start.find(thread_expression),
            f"{filename} joins before assigning a new std::thread")
    require("g_workerFaultRecord = {}" in start and
            "WorkerExceptionKind::None" in start,
            f"{filename} resets fault state for a new generation")
    require(thread_expression in start,
            f"{filename} starts the noexcept worker entry")


def main() -> int:
    audit_backend(
        "mad68pr_backend.cpp",
        "Mad68WorkerBody",
        "Mad68WorkerEntry",
        "Mad68WorkerOnFault",
        "Mad68WorkerOnCompletion",
        "bool Mad68ProR_Start()",
        "std::thread(Mad68WorkerEntry)",
        (
            "ResetSessionPublished();",
            "g_devicePresent.store(false",
            "g_firmwareVersion.store(0",
            "g_productId.store(0",
            "g_uiState.store(static_cast<int>(UiState::Stopped)",
        ),
    )
    mad = source("mad68pr_backend.cpp")
    require("void ResetSessionPublished() noexcept" in mad,
            "MAD68 exceptional reset is noexcept")
    require("for (auto& down : g_physicalDown)" in mad and
            "for (auto& down : g_digitalDown)" in mad,
            "MAD68 fault clears target digital state")

    hex80 = source("hex80_backend.cpp")
    require('#include "worker_exception_barrier.h"' in hex80,
            "hex80_backend.cpp includes the common exception barrier")
    require(re.search(r"std::uint32_t\s+Hex80WorkerBody\s*\(\)", hex80) is not None,
            "hex80_backend.cpp keeps the worker algorithm in a uint32 body")
    require("unsigned __stdcall Hex80WorkerEntry(void*) noexcept" in hex80,
            "hex80_backend.cpp native worker entry is noexcept")
    hex_entry = function_body(hex80, "unsigned __stdcall Hex80WorkerEntry(void*) noexcept")
    require("RunWorkerEntryBarrier" in hex_entry and
            all(token in hex_entry for token in (
                "Hex80WorkerBody", "Hex80WorkerOnFault", "Hex80WorkerOnCompletion")),
            "hex80_backend.cpp native entry wires the common barrier")
    hex_fault = function_body(hex80, "void Hex80WorkerOnFault(")
    require("g_workerFaultRecord = record" in hex_fault and
            "g_workerFaultKind.store(record.kind" in hex_fault,
            "hex80_backend.cpp stores a fixed exception record")
    for token in (
        "g_stop.store(true", "g_connected.store(false", "g_present.store(false",
        "g_detectedPid.store(0", "g_activePid.store(0",
        "g_activeVersion.store(0", "ClearPublishedValues();",
    ):
        require(token in hex_fault,
                f"hex80_backend.cpp fault path publishes safe state: {token}")
    require("OutputDebugStringA(record.message" in hex_fault,
            "hex80_backend.cpp fault path emits allocation-free diagnostics")
    hex_completion = function_body(hex80, "void Hex80WorkerOnCompletion(")
    require("g_running.store(false" in hex_completion,
            "hex80_backend.cpp completion always clears running publication")
    hex_start = function_body(hex80, "bool Hex80_Start()")
    require("g_workerFaultRecord = {}" in hex_start and
            "WorkerExceptionKind::None" in hex_start,
            "hex80_backend.cpp resets fault state for a new generation")
    require("_beginthreadex" in hex_start and "Hex80WorkerEntry" in hex_start,
            "hex80_backend.cpp starts the noexcept entry through a waitable native handle")
    require("void ClearPublishedValues() noexcept" in hex80,
            "Hex80 exceptional publication reset is noexcept")

    addressed = source("addressed_analog_backend.cpp")
    require("std::thread reader;" in addressed and
            "reader = std::thread(AddressedReaderEntry, &path)" in addressed,
            "Addressed nested reader is now owned by the later S02B.3 boundary package")

    print("NATIVE_WORKER_EXCEPTION_BARRIER_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
