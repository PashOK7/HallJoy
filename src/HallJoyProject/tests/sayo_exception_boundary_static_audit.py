#!/usr/bin/env python3
"""Verify Sayo C++/SEH containment and early-reader-exit publication."""

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
    runner = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")

    body = function_body(sayo, "static DWORD SayoReaderThreadBody(SayoReader* reader)")
    cpp_entry = function_body(sayo, "static DWORD SayoReaderThreadProcCpp(")
    os_entry = function_body(sayo, "static DWORD WINAPI SayoReaderThreadProc(LPVOID param) noexcept")
    fault = function_body(sayo, "static void SayoReaderOnCppFault(")
    completion = function_body(sayo, "static void SayoReaderOnCppCompletion(")
    structured = function_body(sayo, "static void SayoReaderOnStructuredFault(")
    start = function_body(sayo, "static bool SayoStart()")

    require("RunWorkerEntryBarrier" in cpp_entry and "SayoReaderThreadBody" in cpp_entry and
            "SayoReaderOnCppFault" in cpp_entry and "SayoReaderOnCppCompletion" in cpp_entry,
            "reader C++ entry uses the common allocation-free barrier")
    require("__try" in os_entry and "__except" in os_entry and
            "SayoReaderOnStructuredFault" in os_entry,
            "reader OS entry preserves a separate SEH boundary")
    require("SayoReleasePublishedInput()" in fault and "SetEvent(g_sayoStopEvent)" in fault,
            "fault neutralizes input and stops the whole reader group")
    require("g_sayoReaderFaultRecord[index] = record" in fault and
            "g_sayoReaderFaultKind[index].store" in fault,
            "fault diagnostics are retained per reader")
    require("SayoPublishReaderCompletion(index)" in completion and "worker.exit" in completion,
            "every C++ exit publishes reader completion")
    require("g_sayoReaderSehCode[index].store" in structured and
            "SayoReaderOnCppFault" in structured and "SayoReaderOnCppCompletion" in structured,
            "SEH exit publishes the same fault and completion state")
    require("g_sayoReaderStartupPublishing" in start and "stage=reader_early_exit" in start and
            "g_sayoReaderFaultKind" in start,
            "startup cannot publish a dead or faulted reader group")
    require("reader_group.exit" in sayo and "all_readers_exited=1" in sayo,
            "post-start loss of all readers clears connected input")
    require("--halljoy-test-sayo-reader-cpp-fault" in body,
            "runtime C++ exception injection is simulator-only")
    require("InjectSayoReaderCppFault" in runner and "Sayo reader C++ fault" in runner,
            "simulator runner verifies contained reader exceptions")

    print("SAYO_EXCEPTION_BOUNDARY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
