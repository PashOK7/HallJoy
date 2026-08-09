#!/usr/bin/env python3
"""Verify the V14-08A startup transaction and durable publication contracts."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TOOLS = ROOT / "tools"

app = (HALL / "app.cpp").read_text(encoding="utf-8-sig")
realtime = (HALL / "realtime_loop.cpp").read_text(encoding="utf-8-sig")
wake = (HALL / "input_wake_sequence.h").read_text(encoding="utf-8-sig")
publication = (HALL / "publication_generation.h").read_text(encoding="utf-8-sig")
curve = (HALL / "backend_curve.cpp").read_text(encoding="utf-8-sig")
registry_h = (HALL / "native_analog_backend_registry.h").read_text(encoding="utf-8-sig")
registry_cpp = (HALL / "native_analog_backend_registry.cpp").read_text(encoding="utf-8-sig")
runner = (TOOLS / "run_analog_simulator.ps1").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


rollback = app[app.index("static bool AppRollbackBackendStartup"):app.index(
    "static bool AppStartBackendDependents")]
start = app[app.index("static bool AppStartBackendDependents"):app.index(
    "static void AppShutdownNoThrow")]

require("NativeAnalogPhaseStartResult" in registry_h and
        "TransactionSafe()" in registry_h,
        "native phase start distinguishes absence from transaction failure")
require("presentBefore" in registry_cpp and "requiredFailures" in registry_cpp and
        "unavailable" in registry_cpp and "rejected" in registry_cpp,
        "native phase result classifies required, optional and ownership failures")
require("startup.transaction.begin" in start and "startup.transaction.commit" in start,
        "backend dependent startup has explicit begin/commit publication")
require("if (!RealtimeLoop_Start())" in start and
        start.index("RealtimeLoop_Start()") < start.index(
            "NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)"),
        "realtime success is required before dependent native phases")
require(start.index("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)") >
        start.index("if (!rawInputRegistered)"),
        "Raw Input is required before the AfterRawInput phase")
require(rollback.index("NativeAnalogStartPhase::AfterRawInput") <
        rollback.index("NativeAnalogStartPhase::AfterRealtime") <
        rollback.index("RealtimeLoop_Stop()") < rollback.index("Backend_Shutdown()"),
        "startup rollback releases acquired stages in strict reverse order")
require("dependent_cleanup_skipped=1" in rollback and
        "g_immediateProcessExitRequired.store(true" in rollback,
        "incomplete rollback poisons process cleanup")
require("--halljoy-test-realtime-start-failure" in realtime and
        "--halljoy-test-native-phase-start-failure" in start and
        "#if defined(HALLJOY_ANALOG_SIMULATOR)" in start,
        "startup fault injection is simulator-only")
require("InjectRealtimeStartFailure" in runner and
        "InjectNativePhaseStartFailure" in runner,
        "scenario runner exposes both startup rollback injections")

require("std::memory_order_release" in wake and "std::memory_order_acquire" in wake,
        "input notification sequence has release/acquire publication")
require("g_inputWakeSequence.Pending(consumedInputSequence)" in realtime and
        realtime.index("g_inputWakeSequence.Pending(consumedInputSequence)") <
        realtime.index("WaitOnAddress("),
        "durable input sequence is checked before every address wait")
require("g_inputNotifySequence.store(0" not in realtime and
        "g_inputConsumedSequence.store(0" not in realtime,
        "worker restart no longer erases pre-start notifications")
require(all(marker not in realtime for marker in
            ("g_inputEvent", "ResetEvent(waitInputEvent)", "SetEvent(eventHandle)")),
        "manual-reset input event remains absent")

require("memory_order_release" in publication and "memory_order_acquire" in publication,
        "curve generation publishes settings with release/acquire ordering")
require("halljoy::publication::Generation" in curve and
        "g_curveCacheGeneration.Publish()" in curve and
        "g_curveCacheGeneration.Observe()" in curve,
        "production curve cache consumes the ordered generation primitive")

print("STARTUP_WAKE_TRANSACTION_STATIC_AUDIT=PASS")
