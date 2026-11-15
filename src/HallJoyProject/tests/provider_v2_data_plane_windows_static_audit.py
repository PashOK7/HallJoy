#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require(text: str, needle: str, purpose: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL: {purpose}: missing {needle!r}")


header = read(HALL / "provider_v2_data_plane_windows.h")
source = read(HALL / "provider_v2_data_plane_windows.cpp")
client = read(HALL / "analog_host_client.cpp")
client_header = read(HALL / "analog_host_client.h")
shared = read(HALL / "analog_host_shared.h")
main = read(HALL / "main.cpp")
project = read(HALL / "HallJoy.vcxproj")
build = read(REPO / "tools" / "build.ps1")
runner = read(REPO / "tools" / "run_native_backend_checks.py")
exact_smoke = read(REPO / "tools" / "run_uap_provider_v2_dual_capture_smoke.ps1")
production_smoke = read(REPO / "tools" / "run_production_smoke.ps1")
abi_runtime = read(REPO / "tools" / "check_private_uap_abi.py")
backend = read(HALL / "backend.cpp")
capture = client.split(
    "bool AnalogHostClient_CaptureProviderV2PlaneHeader(", 1)[1].split(
        "WootingAnalogResult AnalogHostClient_Uninitialise()", 1)[0]

require(source, "DuplicateHandle", "parent derives least-right section handles")
require(source, "&parentRead, FILE_MAP_READ, FALSE", "parent handle has read rights only")
require(source, "&childWrite, FILE_MAP_READ | FILE_MAP_WRITE, TRUE",
        "child writer handle alone is inheritable")
require(source, "MapViewOfFile(parentRead, FILE_MAP_READ",
        "parent payload view is read-only")
require(source, "MapViewOfFile(parentRead, FILE_MAP_WRITE",
        "creation gate proves parent write mapping is rejected")
require(header, "std::atomic<std::uint32_t> readers_",
        "future realtime readers retain the mapping during copy")
require(client, "PrepareProviderPlaneForChild", "supervisor owns mapping replacement")
require(client, "old_child_reaped=1", "replacement records the child-reaped boundary")
require(client, "CloseWriterHandleInParent", "parent drops transient child write handle")
require(client, "provider_plane.resize_accepted", "exact demand drives controlled restart")
require(client, "kHostExitProviderPlaneResize", "child has a dedicated demand exit")
require(client, "ApplyProviderPlaneDemandAfterReap", "demand is trusted only after reap")
require(client_header, "AnalogHostClient_CaptureProviderV2PlaneHeader",
        "parent exposes an independently validated transport oracle")
require(capture, "const volatile LONG64* const sequenceAddress",
        "parent revalidates commit sequence with a read-only load")
if "const_cast<volatile std::uint64_t*>(&view.commit->sequence)" in capture:
    raise SystemExit(
        "FAIL: parent uses a write-requiring RMW operation on its read-only payload view")
require(shared, "ProviderPlane_ResizeRequested", "bounded control plane labels demand")
require(shared, "providerV2PlaneTransactionToken", "commits are token-bound")
require(main, "telemetry.providerV2PlaneParentReadOnly",
        "exact production-image gate requires least-right ownership")
require(main, "telemetry.restartCount >= 1",
        "exact hardware gate proves zero-capacity negotiation restart")
require(project, "provider_v2_data_plane_windows.cpp",
        "Windows owner is production compiled")
require(build, "provider_v2_data_plane_windows_static_audit.py",
        "official build requires the B2i-b ownership audit")
require(build, "provider_v2_data_plane_windows_test.cpp",
        "official source manifest requires the Windows process regression")
require(runner, '"provider_v2_data_plane_windows"',
        "native runner executes the real Windows process regression")
require(build, "Official build's physical-UAP runtime gate requires every HallJoy instance to be closed",
        "official runtime gate refuses concurrent physical-UAP owners")
require(exact_smoke, "Physical-UAP self-test requires every other HallJoy instance to be closed",
        "exact hardware smoke refuses concurrent physical-UAP owners")
require(production_smoke, "Physical-UAP production smoke requires every HallJoy instance to be closed",
        "production smoke refuses concurrent physical-UAP owners")
require(abi_runtime, "assert_no_halljoy_processes()",
        "leaf ABI runtime gate refuses concurrent physical-UAP owners")
require(abi_runtime, "Recheck immediately before the first operation that can open hardware",
        "leaf ABI gate closes the inactive-test to hardware-open gap")

require(backend, "AnalogHostClient_AcquireProviderV2Snapshot",
        "B2i-c shadow reads only the parent-owned immutable broker")
if "providerV2Samples" in shared:
    raise SystemExit("FAIL: B2i-c retained the fixed rollback payload")

print("PASS: Windows data plane is separately owned and feeds the live shadow")
