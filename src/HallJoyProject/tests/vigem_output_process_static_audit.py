#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
shared = (hall / "vigem_output_shared.h").read_text(encoding="utf-8")
channel_h = (hall / "vigem_output_channel.h").read_text(encoding="utf-8")
channel_cpp = (hall / "vigem_output_channel.cpp").read_text(encoding="utf-8")
backend = (hall / "backend.cpp").read_text(encoding="utf-8")
app = (hall / "app.cpp").read_text(encoding="utf-8")
runtime = (hall / "vigem_output_runtime.cpp").read_text(encoding="utf-8")
runtime_supervisor = (hall / "runtime_supervisor.cpp").read_text(encoding="utf-8")
project = (hall / "HallJoy.vcxproj").read_text(encoding="utf-8")
runner = (root.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
process_test = (root / "tests" / "vigem_output_channel_process_test.cpp").read_text(encoding="utf-8")
architecture = (root.parents[1] / "docs" / "v1.4" / "VIGEM_OUTPUT_PROCESS_ARCHITECTURE_2026-08-21.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


require("struct XusbReportV1" in shared and "sizeof(XusbReportV1) == 12u" in shared,
        "IPC owns an exact SDK-independent XUSB report domain")
require("sizeof(SharedStateV1) == 640u" in shared and
        "std::is_trivially_copyable_v<SharedStateV1>" in shared,
        "shared ABI has fixed size and POD copy semantics")
require("SlotState::Writing" in channel_cpp and "SlotState::Reading" in channel_cpp,
        "payload access is protected by exclusive slot ownership")
require("#if defined(_WIN32)" in channel_cpp and
        "InterlockedCompareExchange(" in channel_cpp and
        "InterlockedCompareExchange64(" in channel_cpp and
        "InterlockedExchangeAdd64(" in channel_cpp,
        "Windows cross-process slot/control transitions use documented Interlocked operations")
require("publisherLeaseCount" in shared and "PublicationQuiescent" in channel_cpp and
        "AtomicIncrement(shared.publisherLeaseCount)" in channel_cpp and
        channel_cpp.count("AtomicDecrement(shared.publisherLeaseCount)") >= 4,
        "non-blocking publisher lease closes the disable-to-publication race")
require(channel_cpp.count(
            "AtomicLoad(shared.configurationGeneration) != outputGeneration") >= 2 and
        channel_cpp.count("AtomicLoad(shared.requestedPadCount) != padCount") >= 2 and
        "TryPublishSnapshot(shared, reports, kMaxPads, 0x0fu, 110u) ==\n        PublishResult::Inactive" in
            (root / "tests" / "vigem_output_channel_test.cpp").read_text(encoding="utf-8"),
        "configuration boundaries cannot publish a topology into the wrong generation")
require("childReportedState" in shared and "childLastError" in shared and
        "restartCount" not in shared,
        "child telemetry cannot impersonate parent-authoritative lifecycle state")
require("seqlock" not in channel_cpp.lower(),
        "implementation does not read concurrently-written plain payload through a seqlock")
require(all(token not in channel_cpp for token in ("Sleep(", "WaitFor", "mutex", "condition_variable")),
        "realtime publication implementation contains no wait or lock primitive")
require("expectedOutputGeneration" in channel_cpp and
        "candidate.outputGeneration != expectedOutputGeneration" in channel_cpp,
        "consumer rejects stale child generations")
require("vigem_output_channel.cpp" in project and "vigem_output_shared.h" in project,
        "new protocol compiles in the production MSVC project")
require("vigem_output_channel_test.cpp" in runner,
        "portable native runner executes the production channel implementation")
require("vigem_output_channel_process_test.cpp" in runner and
        "CreateFileMappingW(INVALID_HANDLE_VALUE" in process_test and
        "CreateProcessW(" in process_test and
        "kNonce ^ 1u" in process_test and
        "kPublicationCount = 100000u" in process_test,
        "Windows gate proves nonce rejection and 100,000 snapshots across real processes")
require("vigem_output_channel" not in backend and
        '"vigem_output_runtime.h"' in backend and
        "g_vigemOutputRuntime.TryPublish" in backend and
        "VigemOutputThreadProc" not in backend,
        "production backend routes only through the isolated output runtime")
require("g_vigemOutputRecoveryBlocked" in backend and
        "watchdog.recovery_blocked" in backend and
        "action=restart_halljoy" in backend and
        "retrying on next watchdog tick" not in app and
        "g_vigemRecoveryFailureLogged" not in app and
        "outputFailureLogged" in runtime_supervisor,
        "unsafe output recovery becomes one-shot fail-closed under the runtime supervisor")
require("process_protected=%d" in runtime and
        "job_protected=%d" in runtime and
        "process_identity=%d" in runtime and
        "ownership_error=%lu" in runtime,
        "generation-end evidence records protected ownership and process identity")
require("claimed-slot" in architecture and "formal C++ data race" in architecture,
        "architecture records why claimed slots replaced the proposed plain seqlock")

print("VIGEM_OUTPUT_PROCESS_STATIC_AUDIT=PASS")
