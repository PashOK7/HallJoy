#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
BACKEND = (HALL / "backend.cpp").read_text(encoding="utf-8")
PROJECT = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
FILTERS = (HALL / "HallJoy.vcxproj.filters").read_text(encoding="utf-8")
RUNNER = (ROOT.parents[1] / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")
CHECKS = (ROOT.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")


def body(signature: str) -> str:
    start = BACKEND.find(signature)
    assert start >= 0, f"missing function: {signature}"
    brace = BACKEND.find("{", start)
    assert brace >= 0
    depth = 0
    for index in range(brace, len(BACKEND)):
        if BACKEND[index] == "{":
            depth += 1
        elif BACKEND[index] == "}":
            depth -= 1
            if depth == 0:
                return BACKEND[brace : index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def require(condition: bool, message: str) -> None:
    assert condition, message
    print(f"PASS: {message}")


send = body("static bool VigemOutput_SendBatch(")
tick = body("void Backend_Tick()")
fault_reset = body("void Backend_ResetPublishedStateAfterRealtimeFault() noexcept")
worker = body("static DWORD VigemOutputThreadBody()")
stop = body("static halljoy::lifecycle::StopResult VigemOutput_Stop()")
shutdown = body("bool Backend_Shutdown()")

require(BACKEND.count("vigem_target_x360_update(") == 1,
        "production has one ViGEm update call site")
require("vigem_target_x360_update(" in send,
        "the update call is owned by the output worker send path")
require("vigem_target_x360_update(" not in tick and "Vigem_ReconnectThrottled(" not in tick,
        "Backend_Tick performs no synchronous ViGEm I/O or reconnect")
require("vigem_target_x360_update(" not in fault_reset and
        "g_vigemEmergencyNeutralRequested.store(true" in fault_reset,
        "realtime fault neutralization is delegated to the output owner")
require("if (enabled && !emergencyNeutral)" in worker and
        worker.find("if (enabled && !emergencyNeutral)") < worker.find("if (emergencyNeutral)"),
        "emergency neutral drops older pending reports and is submitted last")
require("g_vigemOutputMailbox.TryPublishMerged(batch" in tick and
        "VigemOutput_Wake();" in tick,
        "realtime publishes a newest complete batch through the mailbox")
require("TryReadAfter" in worker and "VigemOutput_SendBatch(batch)" in worker,
        "output worker consumes the latest mailbox generation")
require("WaitForSingleObject(g_vigemOutputThread, 3000)" in stop,
        "output worker shutdown has a three-second bound")
require(stop.find("if (!observed.Completed())") < stop.find("CloseHandle(g_vigemOutputThread)"),
        "worker handles close only after confirmed completion")
require(shutdown.find("VigemOutput_Stop()") < shutdown.find("g_wootingReady.store") and
        "dependent_cleanup_skipped=1" in shutdown,
        "backend teardown stops on an unconfirmed output join")
require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in send and
        "--halljoy-test-vigem-update-stall" in send and "Sleep(60000)" in send,
        "stalled-driver injection is simulator-only and exceeds the join bound")
require("InjectVigemUpdateStall" in RUNNER,
        "scenario runner exposes the stalled-driver containment gate")
require("latest_value_mailbox" in CHECKS,
        "portable gate includes latest-value report equivalence")
require('ClInclude Include="latest_value_mailbox.h"' in PROJECT and
        'ClInclude Include="latest_value_mailbox.h"' in FILTERS,
        "latest-value mailbox is visible in the MSVC project")

print("VIGEM_OUTPUT_ISOLATION_STATIC_AUDIT=PASS")
