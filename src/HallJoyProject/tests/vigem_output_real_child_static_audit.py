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


shared = read(HALL / "vigem_output_shared.h")
channel_h = read(HALL / "vigem_output_channel.h")
channel_cpp = read(HALL / "vigem_output_channel.cpp")
client_h = read(HALL / "vigem_output_process_client.h")
client_cpp = read(HALL / "vigem_output_process_client.cpp")
transport_h = read(HALL / "vigem_child_transport.h")
transport_cpp = read(HALL / "vigem_child_transport.cpp")
host = read(HALL / "vigem_output_process_host.cpp")
protocol = read(HALL / "vigem_output_process_protocol.h")
self_test = read(HALL / "vigem_output_self_host_test.cpp")
backend = read(HALL / "backend.cpp")
project = read(HALL / "HallJoy.vcxproj")
filters = read(HALL / "HallJoy.vcxproj.filters")
transport_test = read(ROOT / "tests" / "vigem_child_transport_test.cpp")
native_runner = read(REPO / "tools" / "run_native_backend_checks.py")
real_runner = read(REPO / "tools" / "run_vigem_output_real_child_test.ps1")
design = read(REPO / "docs" / "v1.4" /
              "VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_DESIGN_2026-08-22.md")
decisions = read(REPO / "docs" / "v1.4" / "DECISIONS.md")

for option in ("Option A", "Option B", "Option C", "Option D"):
    require(option in design, f"written F2 design compares {option}")
require("Option D" in design and "D-057" in decisions,
        "F2 selection was recorded before production code")

require("requestedPadCount" in shared and "reservedControl[4]" in shared and
        "sizeof(SharedStateV1) == 640u" in shared,
        "pad count consumes reserved control capacity without ABI growth")
require("BeginOutputGeneration" in channel_h and "padCount" in channel_h and
        "ReadOutputConfiguration" in channel_h,
        "generation configuration has an explicit channel API")
require("AtomicStore(shared.requestedPadCount" in channel_cpp and
        "AtomicStore(shared.configurationGeneration" in channel_cpp and
        "candidate.generation != expectedGeneration" in channel_cpp,
        "parent publishes and child validates generation-bound configuration")
require("appliedConfigurationGeneration" in channel_cpp and
        "appliedConfigurationGeneration != generation" in channel_cpp,
        "Ready acknowledges only the matching immutable configuration")
require("std::uint32_t padCount = 0" in client_h and
        "request.padCount > kMaxPads" in client_cpp and
        "request.generation, request.padCount" in client_cpp,
        "session rejects invalid target count before child launch")

for token in (
    "ClientAllocate", "ClientConnect", "TargetAllocate", "TargetAdd",
    "InitialNeutral",
    "ReportUpdate", "NeutralUpdate", "TargetRemove",
):
    require(token in transport_h, f"transport reports exact {token} phase")
require("std::array<TargetRecord, kMaxPads>" in transport_h and
        "const VigemApiV1& api_" in transport_h,
        "transport has one fixed-capacity owner and immutable API dependency")
require("&vigem_alloc" in transport_cpp and "&vigem_target_x360_update" in transport_cpp and
        "HALLJOY_VIGEM_TRANSPORT_FAKE_ONLY" in transport_cpp,
        "production table is real while portable tests can exclude SDK linkage")

cleanup = transport_cpp[transport_cpp.index(
    "VigemTransportResult VigemChildTransport::Cleanup"):]
cleanup = cleanup[:cleanup.index("VigemTransportResult VigemChildTransport::Stop")]
require(cleanup.index("api_.targetUpdate") < cleanup.index("api_.targetRemove") <
        cleanup.index("api_.targetFree") < cleanup.index("api_.clientDisconnect") <
        cleanup.index("api_.clientFree"),
        "planned cleanup orders neutral, remove, free, disconnect and client free")
require("for (std::uint32_t index = 0; index < targetCount_; ++index)" in cleanup and
        "RecordFirstFailure" in cleanup and
        "result.neutralApplied = false" in cleanup and
        "result.targetsRemoved = false" in cleanup,
        "cleanup continues every target while retaining the first exact failure")

require("RunRealHost" in host and "VigemChildTransport transport(RealVigemApi())" in host and
        "transport.Start(configuration.padCount)" in host and
        "transport.Apply(snapshot)" in host and "transport.Stop()" in host,
        "exact output child owns all real transport actions")
require("launch.mode == OutputHostMode::Real" in host and
        "exitCode = RunRealHost(launch)" in host,
        "real mode is routed through the early exact-image host")
require("completionFlags |= kChildDiagnosticNeutralApplied" in host and
        "completionFlags |= kChildDiagnosticTargetsRemoved" in host and
        "ChildPublishStopped" in host and "ChildPublishFault" in host,
        "real stop distinguishes complete and partial cleanup")
require("vigem_child_transport" not in backend and
        "vigem_output_runtime.h" in backend and
        "g_vigemOutputRuntime.TryPublish" in backend and
        "VigemOutputThreadProc" not in backend,
        "production backend reaches real transport only through the child runtime")

for token in (
    "failClientAllocate", "connectError", "failTargetAllocate",
    "failTargetAdd", "failReportUpdate", "failNeutralUpdate",
    "failTargetRemove", "AssertReleased",
):
    require(token in transport_test, f"deterministic failure matrix covers {token}")
require("partial_start_edges=8" in transport_test and
        "initial_neutral_edges=4" in transport_test and
        "neutral_edges=4" in transport_test and "remove_edges=4" in transport_test,
        "transport test publishes stable exhaustive scenario counts")
require("HALLJOY_VIGEM_TRANSPORT_FAKE_ONLY" in native_runner and
        "vigem_child_transport_test.cpp" in native_runner and
        "vigem_child_transport.cpp" in native_runner,
        "portable compiler gate runs the real transport state machine with fake API")

require("kOutputRealSelfTestArgument" in protocol and
        "RunExactRealChildSuite" in self_test and
        "OutputHostMode::Real" in self_test and
        "appliedConfigurationGeneration != generation" in self_test,
        "actual simulator image drives the real child and configuration ack")
require("--halljoy-test-vigem-output-real-child" in real_runner and
        "Get-Service -Name 'ViGEmBus'" in real_runner and
        "baselineDeviceKey" in real_runner and "pnp_baseline" in real_runner and
        "survivors=0" in real_runner and "WaitForExit(30000)" in real_runner,
        "bounded real-driver runner enforces service, PnP baseline and zero survivors")

for filename in ("vigem_child_transport.h", "vigem_child_transport.cpp"):
    require(filename in project and filename in filters,
            f"MSVC project/filter includes {filename}")

print("VIGEM_OUTPUT_REAL_CHILD_STATIC_AUDIT=PASS")
