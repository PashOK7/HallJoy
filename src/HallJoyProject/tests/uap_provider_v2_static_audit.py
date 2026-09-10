#!/usr/bin/env python3
"""Static gate for the read-only full-key UAP Provider V2 snapshot."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
HALL = ROOT / "HallJoy"
UAP = REPO / "third_party" / "UniversalAnalogPluginFixed"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise SystemExit(f"FAIL: missing function {signature!r}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:index + 1]
    raise SystemExit(f"FAIL: unterminated function {signature!r}")


plugin = read(UAP / "main.cpp")
contract = read(UAP / "halljoy_analog_provider_v2_contract.h")
uap_contract = read(UAP / "halljoy_uap_provider_v2.h")
projection = read(UAP / "halljoy_uap_provider_v2_projection.h")
shared = read(HALL / "analog_host_shared.h")
client = read(HALL / "analog_host_client.cpp")
client_header = read(HALL / "analog_host_client.h")
parent_snapshot = read(HALL / "uap_parent_snapshot.cpp")
main = read(HALL / "main.cpp")
backend = read(HALL / "backend.cpp")
runner = read(REPO / "tools" / "run_native_backend_checks.py")
build = read(REPO / "tools" / "build.ps1")
abi_runtime = read(REPO / "tools" / "check_private_uap_abi.py")
projection_test = read(ROOT / "tests" / "uap_provider_v2_projection_test.cpp")
exact_exe_smoke = read(REPO / "tools" / "run_uap_provider_v2_dual_capture_smoke.ps1")
design = read(REPO / "docs" / "v1.4" / "UAP_PROVIDER_V2_SNAPSHOT_DESIGN_2026-08-22.md")

require(contract, "KeyNamespace::UsbHidUsage", "shared contract owns USB identity")
require(contract, "KeyNamespace::UapExtended", "shared contract owns UAP identity")
require(uap_contract, "IdentityFromLegacyCode", "UAP uses one explicit identity translation")
require(uap_contract, "UsbHidKey(0x0Cu", "media codes become consumer-page usages")
require(uap_contract, "UapExtendedKey(code)", "OEM and Fn remain namespaced")
require(plugin, "provider_key_values", "worker retains values before the 256-key projection")
require(plugin, "halljoy_get_provider_snapshot_v2", "plugin exports the immutable V2 snapshot")
require(plugin, "halljoy_get_dual_snapshot_v2",
        "plugin exports one pinned legacy plus V2 snapshot")
require(plugin, "HallJoyUapProviderV2::BuildSnapshot(",
        "plugin uses the production-linked projection core")
require(plugin, "pinned_devices.required_count",
        "plugin reports registry demand beyond pinned transport capacity")
provider_builder = function_body(plugin, "static bool halljoy_build_provider_snapshot_v2")
require(provider_builder, "QueryOwnerCount",
        "Provider V2 observes exact registry demand before storage growth")
require(provider_builder, "PinOwnersInto",
        "Provider V2 pins the complete caller-independent owner generation")
require(provider_builder, "LockSetView",
        "Provider V2 device locks use preallocated dynamic RAII storage")
if "PinOwners<HallJoyUapProviderV2::kMaxDevices>" in provider_builder:
    raise SystemExit("FAIL: Provider V2 still has the fixed eight-owner pin window")
print("PASS: Provider V2 fixed eight-owner pin window is absent")
require(provider_builder, "kNegotiatedDeviceSafetyLimit",
        "negotiated producer rejects abusive capacity instead of truncating silently")
require(plugin, "source->snapshot_generation == 0 || source->snapshot_timestamp_us == 0",
        "provider rejects constructor-zero snapshots before first acquisition")
require(projection, "AnalogSampleFlag_Owned", "plugin projection publishes explicit ownership")
if "output_device.flags |= AnalogDeviceFlag_LayoutProven" in plugin:
    raise SystemExit("FAIL: UAP invents physical layout proof for the provider superset")
print("PASS: UAP does not invent physical layout proof")
if "providerV2Samples" in shared or "providerV2Devices" in shared:
    raise SystemExit("FAIL: fixed Provider V2 payload remains in shared IPC")
print("PASS: shared IPC contains only bounded Provider V2 control metadata")
require(shared, "providerV2PlaneDualCoherent",
        "isolated IPC labels only same-generation variable-plane publications")
require(client_header, "AnalogHostClient_CaptureTickSnapshot",
        "parent exposes one dense plus optional V2 tick capture")
require(client, "result.denseValues.begin()",
        "parent copies the dense plane inside the capture transaction")
require(client, "CaptureProviderPlaneIntoBroker",
        "snapshot bridge copies Provider V2 into parent-owned storage")
require(client, "before != after || (after & 1)",
        "parent rejects a generation changed during either view copy")
require(client, 'Resolve(api.module, "halljoy_get_dual_snapshot_v2", api.getDualSnapshotV2)',
        "child requires the exact dual-view private export")
require(client, "shared->providerV2PlaneDualCoherent == 1",
        "parent rejects separately captured or failed dual views")
require(main, "--halljoy-test-uap-provider-v2-dual-capture",
        "exact production image exposes the hardware-required IPC gate")
require(main, "providerLease.DeviceCount() > 0 &&",
        "exact-image broker gate cannot pass on an empty provider")
require(exact_exe_smoke, "UAP_PROVIDER_V2_EXACT_EXE_DUAL_CAPTURE=PASS",
        "bounded runner verifies exact-image exit, reap and immutable hash")
require(parent_snapshot, "ValidateSnapshot(",
        "shared child/parent validator validates Provider V2 structure")
require(parent_snapshot, "DenseAggregateMismatch",
        "parent independently validates the merged compatibility plane")
require(parent_snapshot, "ValidateDualViews(",
        "child and parent share one complete dual-view validator")
require(projection, "ProjectOrdinaryUsbHidDense",
        "adapter has one deterministic ordinary-HID compatibility projection")
require(projection, "ProjectCapturedDeviceOrdinaryUsbHidDense",
        "dual export compares every captured legacy device while pinned")
require(projection_test, "ordinary_hid_equivalence=1",
        "portable behavioral gate compares legacy and V2 ordinary HID values")
require(projection_test, "hidden_truncation_rejected=1",
        "portable behavioral gate rejects hidden device truncation")
require(projection_test, "negotiated_12_devices=1",
        "portable behavioral gate accepts an exact twelve-device generation")
require(runner, "uap_provider_v2_projection_test.cpp",
        "unified native runner executes the production projection gate")
require(build, "halljoy_uap_provider_v2_projection.h",
        "official build requires the projection source")
require(build, "uap_provider_v2_projection_test.cpp",
        "official build requires the behavioral projection gate")
require(abi_runtime, "halljoy_get_provider_snapshot_v2",
        "exact built DLL gate calls the Provider V2 export")
require(abi_runtime, "validate_provider_snapshot",
        "exact built DLL gate validates V2 identity, capacity and value semantics")
require(abi_runtime, "validate_dual_view",
        "exact built DLL gate independently compares all dual-view cells")
require(abi_runtime, "dual_view_equivalent=1",
        "exact built DLL gate reports same-generation equivalence")
require(abi_runtime, "negotiated_capacity=1",
        "exact DLL sizes provider and dual storage from reported demand")
require(abi_runtime, "provider V2 exposed stale data after unload",
        "exact built DLL gate rejects inactive and stale V2 generations")
require(backend, "WootingSafe_CaptureTickSnapshot(&s_uapTickSnapshot)",
        "Backend_Tick performs one immutable parent capture")
require(backend, "AnalogHostClient_AcquireProviderV2Snapshot",
        "Backend_Tick acquires the immutable dynamic Provider V2 lease")
require(backend, "cache.hasAuthoritativeProviderV2 = true",
        "qualified UAP route is explicitly enabled only after V2 projection")
require(backend, "cache.providerV2Raw = providerV2Shadow.providerRaw",
        "qualified UAP reads select the immutable Provider V2 broker projection")
require(backend, "ReadProviderV2ShadowRaw01Cached",
        "legacy dense/per-key compatibility remains an independent shadow")
require(backend, "!cache.hasAuthoritativeProviderV2 &&",
        "legacy dense fallback is explicit and cannot replace an available V2 route")
print("PASS: production consumes Provider V2 while dense compatibility remains shadow-only")
require(design, "activation-depth latency", "design rejects activation-depth digital learning")
require(design, "LayoutProven", "design separates provider ownership from physical layout proof")

print("UAP_PROVIDER_V2_STATIC_AUDIT=PASS")
