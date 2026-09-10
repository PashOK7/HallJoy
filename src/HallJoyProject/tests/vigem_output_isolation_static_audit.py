#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]
BACKEND = (HALL / "backend.cpp").read_text(encoding="utf-8")
RUNTIME = (HALL / "vigem_output_runtime.cpp").read_text(encoding="utf-8")
RUNTIME_H = (HALL / "vigem_output_runtime.h").read_text(encoding="utf-8")
CLIENT = (HALL / "vigem_output_process_client.cpp").read_text(encoding="utf-8")
SUPERVISOR = (HALL / "process_generation_supervisor.cpp").read_text(encoding="utf-8")
TRANSPORT = (HALL / "vigem_child_transport.cpp").read_text(encoding="utf-8")
PROJECT = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
FILTERS = (HALL / "HallJoy.vcxproj.filters").read_text(encoding="utf-8")
RUNNER = (REPO / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8")
APP = (HALL / "app.cpp").read_text(encoding="utf-8")
HEADER = (HALL / "backend.h").read_text(encoding="utf-8")
RUNTIME_SUPERVISOR = (HALL / "runtime_supervisor.cpp").read_text(encoding="utf-8")


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    assert start >= 0, f"missing function: {signature}"
    brace = source.find("{", start)
    assert brace >= 0
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def require(condition: bool, message: str) -> None:
    assert condition, message
    print(f"PASS: {message}")


tick = body(BACKEND, "void Backend_Tick()")
fault_reset = body(BACKEND,
                   "void Backend_ResetPublishedStateAfterRealtimeFault() noexcept")
shutdown = body(BACKEND, "bool Backend_Shutdown()")
recover = body(BACKEND, "bool Backend_EnsureOutputRuntimeHealthy()")
runtime_publish = body(RUNTIME, "OutputPublishResult OutputRuntime::TryPublish(")
runtime_stop = body(RUNTIME, "bool OutputRuntime::Stop(")
owner = body(RUNTIME, "DWORD OwnerThreadBody() noexcept")
prepare = body(CLIENT, "bool PrepareOutputTermination(")
run_one = body(CLIENT, "OutputGenerationResult OutputProcessSession::RunOne(")
supervise = body(SUPERVISOR,
                 "SupervisionResult ProcessGenerationSupervisor::RunOne(")

legacy_tokens = (
    "VigemOutputThreadProc", "VigemOutputThreadBody",
    "g_vigemOutputMailbox", "g_vigemOutputWakeEvent",
    "g_vigemOutputThread", "Vigem_ReconnectThrottled",
    "Vigem_Create(", "Vigem_Destroy(",
)
for token in legacy_tokens:
    require(token not in BACKEND, f"legacy production owner is absent: {token}")

for token in ("vigem_target_", "vigem_connect(", "vigem_alloc("):
    require(token not in BACKEND,
            f"backend performs no direct ViGEm SDK action: {token}")
require(TRANSPORT.count("vigem_target_x360_update") == 1,
        "real child transport has the sole ViGEm update call site")
require("g_vigemOutputRuntime.TryPublish" in tick and
        all(token not in tick for token in ("WaitFor", "Sleep(", "CreateThread")),
        "Backend_Tick only performs bounded process-channel publication")
require("outputReports[index] = ToOutputReport(report)" in tick and
        "const std::uint32_t validMask = (1u << outputCount) - 1u" in tick,
        "production publishes a newest complete configured-pad snapshot")
require("g_vigemOutputRuntime.TryPublish" in fault_reset and
        "validMask" in fault_reset and "VIGEM_ERROR" not in fault_reset,
        "realtime fault publishes neutral without entering the driver")
require("OutputProcessSession session" in RUNTIME and
        "request.supervisorStopEvent = commandEvent" in owner and
        "session.RunOne(request)" in owner,
        "one parent owner thread supervises the process session")
require(owner.find("session.RunOne(request)") <
        owner.find("StoreGenerationResult(generation, result)"),
        "one synchronous owner records a generation only after reap")
require("prepareTermination = &PrepareOutputTermination" in CLIENT and
        "DisableOutputGeneration" in prepare and
        "PublicationQuiescent" not in prepare and
        run_one.find("result.process = supervisor_->RunOne(supervision)") <
        run_one.find("quiescenceDeadline") <
        run_one.find("result.restartSafe =", run_one.find("quiescenceDeadline")),
        "generation admission closes before stop and producer drain follows reap")
require(supervise.find("(void)prepareTermination();") <
        supervise.find("StopAndReap(request.childStopEvent"),
        "planned child stop is prepared before neutral/remove/reap")
force_positions = [
    supervise.find("(void)prepareTermination();", supervise.find("WAIT_FAILED")),
    supervise.find("(void)prepareTermination();",
                   supervise.find("if (!mustForce)")),
]
require(all(position >= 0 for position in force_positions) and
        all(position < supervise.find("ForceAndReap(", position)
            for position in force_positions),
        "wait failure and progress/fault recovery prepare publication before force")
require("accessAdmission" in RUNTIME and "accessLeases.fetch_add" in RUNTIME and
        "WaitForSessionLeases" in runtime_stop,
        "outer session lease prevents close racing a preempted publisher")
require(runtime_stop.find("WaitForSingleObject(self.ownerThread") <
        runtime_stop.find("self.session.Shutdown(error)") <
        runtime_stop.find("CloseHandle(self.commandEvent)"),
        "shutdown joins owner and drains access before closing session resources")
require("desiredRevision" in owner and
        "revision == desiredRevision.load" in owner and
        "previousStartedGeneration" in owner,
        "configuration changes coalesce into one non-overlapping generation")
require("kRecoveryBackoffMs" in owner and
        "if (!result.restartSafe && !RebuildSession())" in owner,
        "unexpected output failure has bounded non-overlapping recovery")
require("Backend_EnsureOutputRuntimeHealthy" in HEADER and
        "Backend_EnsureOutputRuntimeHealthy()" not in APP and
        "Backend_EnsureOutputRuntimeHealthy()" in RUNTIME_SUPERVISOR and
        "OutputRuntimeState::Faulted" in recover,
        "runtime supervisor, never the UI, rebuilds only a terminally faulted process session")
require(shutdown.find("VigemOutput_Stop()") <
        shutdown.find("g_wootingReady.store") and
        "dependent_cleanup_skipped=1" in shutdown,
        "backend teardown stops on an unconfirmed output-owner join")
require("TryPublish(" in RUNTIME_H and
        "TryPublish(" in runtime_publish and
        all(token not in runtime_publish for token in
            ("WaitFor", "Sleep(", "CreateThread", "CloseHandle")),
        "realtime runtime entry contains no lifecycle wait or resource close")
require("InjectVigemUpdateStall" in RUNNER and
        "InjectVigemOutputInvalidThreadHandle" in RUNNER and
        "InjectVigemOutputWakeCloseUse" in RUNNER,
        "routed O1/O2/stalled-update scenarios remain exposed")
require('ClInclude Include="vigem_output_runtime.h"' in PROJECT and
        'ClCompile Include="vigem_output_runtime.cpp"' in PROJECT and
        "vigem_output_runtime.h" in FILTERS and
        "vigem_output_runtime.cpp" in FILTERS,
        "production project and filters compile the sole runtime owner")

print("VIGEM_OUTPUT_ISOLATION_STATIC_AUDIT=PASS")
