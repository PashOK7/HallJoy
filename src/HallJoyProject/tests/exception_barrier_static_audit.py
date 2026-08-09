#!/usr/bin/env python3
"""Verify S02 exception barriers and exceptional-path cleanup contracts."""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
RUNNER = ROOT.parents[1] / "tools" / "run_native_backend_checks.py"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def text(name: str) -> str:
    return (HALL / name).read_text(encoding="utf-8-sig", errors="strict")


def main() -> int:
    barrier_path = HALL / "worker_exception_barrier.h"
    require(barrier_path.is_file(), "allocation-free worker exception barrier exists")
    barrier = barrier_path.read_text(encoding="utf-8")
    require("RunWorkerEntryBarrier" in barrier, "common worker barrier entry exists")
    require("catch (const std::exception&" in barrier, "std::exception is contained")
    require("catch (...)" in barrier, "unknown exceptions are contained")
    require("WorkerExceptionRecord" in barrier and "kMessageCapacity" in barrier,
            "fault record is fixed-size")
    require("std::is_trivially_copyable_v<WorkerExceptionRecord>" in barrier,
            "fault record is trivially copyable")
    require("std::is_nothrow_invocable_v<OnFault" in barrier and
            "std::is_nothrow_invocable_v<OnCompletion" in barrier,
            "fault and completion callbacks are compile-time noexcept")

    workers = {
        "realtime_loop.cpp": ("RealtimeThreadBody", "ThreadProc", "RealtimeThreadOnFault"),
        "debug_log.cpp": ("DebugLogWriterThreadBody", "DebugLogWriterThreadProc", "DebugLogWriterOnFault"),
        "overlay_server.cpp": ("OverlayThreadBody", "OverlayThreadProc", "OverlayThreadOnFault"),
    }
    for name, (body, entry, fault) in workers.items():
        source = text(name)
        require('#include "worker_exception_barrier.h"' in source,
                f"{name} includes common barrier")
        require(body in source, f"{name} keeps worker algorithm in a separate body")
        require(re.search(rf"{re.escape(entry)}\s*\([^)]*\)\s*noexcept", source) is not None,
                f"{name} OS thread entry is noexcept")
        require("RunWorkerEntryBarrier" in source, f"{name} invokes common barrier")
        require(fault in source, f"{name} has a dedicated fault publisher")

    realtime = text("realtime_loop.cpp")
    backend_h = text("backend.h")
    backend_cpp = text("backend.cpp")
    require("Backend_ResetPublishedStateAfterRealtimeFault() noexcept" in backend_h,
            "realtime fail-safe reset is declared noexcept")
    require("void Backend_ResetPublishedStateAfterRealtimeFault() noexcept" in backend_cpp,
            "realtime fail-safe reset is implemented")
    require("Backend_ResetPublishedStateAfterRealtimeFault();" in realtime,
            "realtime exception path clears published values")
    require("g_run.store(false" in realtime and "g_threadAlive.store(false" in realtime,
            "realtime exception/completion closes running publications")
    require("const bool healthy" in realtime and "g_realtimeFaultKind.load" in realtime,
            "realtime repeated start cannot report a faulted generation as healthy")
    require("~RealtimeThreadResources() noexcept" in realtime,
            "realtime OS resources unwind through noexcept RAII")

    debug = text("debug_log.cpp")
    require("g_logReady.store(false" in debug and "g_stopWriter.store(true" in debug,
            "debug writer fault closes producer gate")
    require("const bool ownsResources" in debug,
            "debug shutdown reaps resources after writer fault")
    require("g_writerThread || g_writeEvent || g_logFile != INVALID_HANDLE_VALUE" in debug,
            "debug init cannot replace an unreaped writer generation")

    overlay = text("overlay_server.cpp")
    require("g_overlayRunning.store(false" in overlay and "g_overlayPort.store(0" in overlay,
            "overlay fault clears running publications")
    require("g_overlayWsaStarted.exchange(false" in overlay,
            "overlay WSA ownership has balanced exceptional cleanup")
    require("OverlayReapCompletedGenerationLocked" in overlay and
            "WaitForSingleObject(g_overlayThread, 0)" in overlay and
            "g_overlayLifecycle.ConfirmJoined(generation)" in overlay,
            "overlay restart reaps only an observed completed generation")

    project = ET.parse(HALL / "HallJoy.vcxproj").getroot()
    includes = {node.attrib.get("Include") for node in project.findall(".//m:ClInclude", NS)}
    compiles = {node.attrib.get("Include") for node in project.findall(".//m:ClCompile", NS)}
    require("worker_exception_barrier.h" in includes,
            "exception barrier header is visible in MSVC project")
    require("worker_exception_barrier.cpp" not in compiles,
            "barrier adds no production object or static initializer")

    runner = RUNNER.read_text(encoding="utf-8")
    require("worker_exception_barrier_test.cpp" in runner,
            "portable exception barrier test is in the common gate")
    require((ROOT / "tests" / "worker_exception_barrier_test.cpp").is_file(),
            "portable exception barrier test exists")

    print("EXCEPTION_BARRIER_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
