#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


main = read(HALL / "main.cpp")
shared = read(HALL / "vigem_output_shared.h")
channel_h = read(HALL / "vigem_output_channel.h")
channel_cpp = read(HALL / "vigem_output_channel.cpp")
protocol = read(HALL / "vigem_output_process_protocol.h")
client_h = read(HALL / "vigem_output_process_client.h")
client_cpp = read(HALL / "vigem_output_process_client.cpp")
host = read(HALL / "vigem_output_process_host.cpp")
self_test = read(HALL / "vigem_output_self_host_test.cpp")
supervisor = read(HALL / "process_generation_supervisor.cpp")
backend = read(HALL / "backend.cpp")
project = read(HALL / "HallJoy.vcxproj")
filters = read(HALL / "HallJoy.vcxproj.filters")
native_runner = read(REPO / "tools" / "run_native_backend_checks.py")
exact_runner = read(REPO / "tools" / "run_vigem_output_self_host_test.ps1")
design = read(REPO / "docs" / "v1.4" /
              "VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_DESIGN_2026-08-22.md")
decisions = read(REPO / "docs" / "v1.4" / "DECISIONS.md")

require(main.index("VigemOutputHost_TryRunCommand") <
        main.index("VigemOutputSelfHostTest_TryRunCommand") <
        main.index("AppDeps_TryRunEmbeddedInstallerVerificationCommand") <
        main.index("AnalogHost_TryRunCommand") <
        main.index("StabilityTrace_Init()"),
        "output host and exact test dispatch before every normal startup path")

require("argc != 10" in host and "argv[1]" in host and
        "kOutputHostArgument" in host and "ParseExactHostCommand" in host,
        "child accepts one exact internal command shape")
require("ValidateSharedState" in host and "GetProcessId(resources.ownerProcess)" in host and
        "ActiveOutputGeneration" in host and "IsProcessInJob" in host,
        "child validates ABI, owner identity, generation and containment")
require("#if defined(HALLJOY_ANALOG_SIMULATOR)" in host and
        "RunFakeHost" in host and "kUnsupportedTransportExit" in host,
        "fake transport is simulator-only and production fails closed")
require("#if !defined(HALLJOY_ANALOG_SIMULATOR)" in client_cpp and
        "request.mode != OutputHostMode::Real" in client_cpp and
        "OutputGenerationOutcome::Unsupported" in client_cpp,
        "production session rejects every simulator-only transport mode")
require("CreateFileMappingW(INVALID_HANDLE_VALUE" in client_cpp and
        "CreateEventW(&inherited, FALSE, FALSE, nullptr)" in client_cpp and
        "CreateEventW(&inherited, TRUE, FALSE, nullptr)" in client_cpp,
        "session uses process-lifetime unnamed mapping/wake/stop objects")
require("supervision.launch.inheritedHandles = {\n        mapping_, wakeEvent_, childStopEvent_, ownerProcess_\n    };" in client_cpp,
        "exact child allowlist contains only four intended unnamed handles")
require("PROC_THREAD_ATTRIBUTE_HANDLE_LIST" in supervisor and
        supervisor.index("CREATE_SUSPENDED") < supervisor.index("AssignProcessToJobObject") <
        supervisor.index("ResumeThread"),
        "session composes the already-proved contained F1 owner")

require("childTelemetrySequence" in shared and "childGeneration" in shared and
        "reservedHealth[3]" in shared and "sizeof(SharedStateV1) == 640u" in shared,
        "transactional generation telemetry consumes reserved space without ABI growth")
require("BeginChildTelemetryUpdate" in channel_cpp and
        "EndChildTelemetryUpdate" in channel_cpp and
        "before == after" in channel_cpp and "(after & 1u) == 0u" in channel_cpp,
        "parent accepts only a complete even child telemetry transaction")
require("ReadChildTelemetry" in channel_h and "ChildPublishStarting" in channel_h and
        "ChildPublishStopped" in channel_h,
        "child-health ownership is expressed through one channel API")

require("completedStopGeneration == request.generation" in client_cpp and
        "kChildDiagnosticNeutralApplied" in client_cpp and
        "kChildDiagnosticTargetsRemoved" in client_cpp and
        "IncompletePlannedStop" in client_cpp,
        "generic process stop is qualified by output-specific completion")
require("Process signalling can win" in client_cpp and
        "readyGeneration == request.generation" in client_cpp,
        "post-reap stable telemetry resolves the ready/exit polling race")

for token in (
    "FakeExitBeforeReady",
    "FakeExitAfterReady",
    "FakeExitBeforeSnapshotRead",
    "FakeExitDuringUpdate",
    "FakeExitAfterAcknowledgement",
    "FakeExitDuringStop",
    "FakeStallAfterReady",
):
    require(token in protocol and token in self_test, f"exact suite covers {token}")
require("GetModuleFileNameW(nullptr" in self_test and
        "OutputGenerationOutcome::ProgressTimeout" in self_test and
        "OutputGenerationOutcome::IncompletePlannedStop" in self_test and
        "newestCheckpoint" in self_test and "ChildIsReaped" in self_test,
        "actual HallJoy image proves O3, O4, newest equivalence and reap")
require("generations=9" in self_test and "o4_boundaries=6" in self_test,
        "exact suite publishes a stable scenario count")
require("RunExactRuntimeStressSuite" in self_test and
        "kRequiredPublications = 100000u" in self_test and
        "kTransitions = 100u" in self_test and
        "WaitRuntimeApplied" in self_test and "ChildIsReaped(childPid)" in self_test,
        "exact runtime stress proves publication, topology, lifecycle and final equivalence")

publish_body = client_cpp[client_cpp.index("OutputPublishResult OutputProcessSession::TryPublish"):]
publish_body = publish_body[:publish_body.index("bool OutputProcessSession::ReadTelemetry")]
require(all(token not in publish_body for token in
            ("WaitFor", "Sleep(", "Create", "CloseHandle", "MapView")),
        "realtime publication path contains no lifecycle wait or resource operation")
require("SetEvent(wakeEvent_)" in publish_body and "WakeFailed" in publish_body,
        "wake failure is explicit rather than hidden after publication")

new_runtime = client_cpp + host + self_test
for forbidden in ("vigem_target_", "vigem_client_", "vigem_alloc(",
                  "vigem_connect("):
    require(forbidden not in new_runtime,
            f"F1.1 fake transport contains no real ViGEm call: {forbidden}")
require("vigem_output_runtime.h" in backend and
        "g_vigemOutputRuntime.TryPublish" in backend and
        "VigemOutputThreadProc" not in backend,
        "production backend is routed only through the proved process runtime")

for filename in (
    "vigem_output_process_protocol.h",
    "vigem_output_process_client.h",
    "vigem_output_process_host.h",
    "vigem_output_self_host_test.h",
    "vigem_output_process_client.cpp",
    "vigem_output_process_host.cpp",
    "vigem_output_self_host_test.cpp",
):
    require(filename in project and filename in filters,
            f"production project/filter includes {filename}")
require("vigem_output_telemetry_test.cpp" in native_runner,
        "native compiler suite includes transactional telemetry stress")
require("--halljoy-test-vigem-output-self-host" in exact_runner and
        "WaitForExit(30000)" in exact_runner and "survivors=0" in exact_runner and
        "HallJoyV14Simulator.exe" in exact_runner,
        "bounded exact-EXE runner enforces exit, artifact and zero survivors")
require("--halljoy-test-vigem-output-runtime-stress" in exact_runner and
        "WaitForExit(180000)" in exact_runner and
        "publications_min=100000" in exact_runner and "generations=101" in exact_runner,
        "bounded exact-EXE runner exposes the O5 runtime stress gate")

for option in ("Option A", "Option B", "Option C", "Option D"):
    require(option in design, f"written design compares {option}")
require("Option D" in design and "Chosen option" in design and
        "D-056" in decisions,
        "selected self-host/session architecture was recorded before code")

print("VIGEM_OUTPUT_SELF_HOST_STATIC_AUDIT=PASS")
