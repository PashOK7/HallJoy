#!/usr/bin/env python3
"""Verify the V14-11C registry-pin/snapshot-lock separation contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
PLUGIN = REPO / "third_party" / "UniversalAnalogPluginFixed"


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
    main_cpp = (PLUGIN / "main.cpp").read_text(encoding="utf-8-sig")
    helper = (PLUGIN / "halljoy_uap_pinned_owners.h").read_text(encoding="utf-8-sig")
    test = (ROOT / "tests" / "uap_snapshot_pinning_test.cpp").read_text(encoding="utf-8-sig")
    runner = (REPO / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
    build = (REPO / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
    abi_check = (REPO / "tools" / "check_private_uap_abi.py").read_text(encoding="utf-8-sig")

    require("std::vector<std::shared_ptr<Device>> devices" in main_cpp,
            "registry owns devices through ref-counted pins")
    require("std::vector<soup::UniquePtr<Device>> devices" not in main_cpp,
            "old exclusive registry ownership is absent")
    require("LockGuard<Mutex> registry_lock(registry_mutex)" in helper and
            "std::copy_n(source.begin(), pinned.count" in helper,
            "production helper copies only bounded owners under registry lock")

    telemetry = function_body(main_cpp, "SOUP_CEXPORT uint32_t halljoy_get_device_telemetry")
    dense = function_body(main_cpp, "SOUP_CEXPORT uint32_t halljoy_get_dense_snapshots")
    for name, body in (("telemetry", telemetry), ("dense snapshot", dense)):
        require("PinOwners<" in body and "pinned_devices.owners[i]" in body,
                f"{name} export pins owners before per-device access")
        require("devices_lock(devices_mtx)" not in body,
                f"{name} export has no nested global lock scope")
    require("snapshot_lock(dev->snapshot_mtx)" in dense and
            dense.index("PinOwners<") < dense.index("snapshot_lock(dev->snapshot_mtx)"),
            "dense value copy occurs after bounded registry capture")

    removal = function_body(main_cpp, "static void remove_stopped_devices()")
    unload = function_body(main_cpp, "static bool halljoy_unload_impl(uint32_t timeout_ms)")
    require("std::shared_ptr<Device> stopped" in removal and "stopped = dev" in removal,
            "hotplug removal pins callback/device lifetime outside registry lock")
    require("std::vector<std::shared_ptr<Device>> workers" in unload and
            "workers.assign(devices.begin(), devices.end())" in unload,
            "bounded unload pins every worker used outside registry lock")
    require("std::make_shared<Device>" in main_cpp,
            "device discovery publishes shared owners")

    require("blocked_removal=1" in test and "lifetime_cycles = 100000" in test and
            "coherent_reads" in test,
            "portable test covers blocked copy, exact lifetime and coherent snapshots")
    require("uap_snapshot_pinning_test.cpp" in runner,
            "portable runner includes the exact production helper test")
    require("uap_snapshot_pinning_static_audit.py" in build and
            "halljoy_uap_pinned_owners.h" in build and
            "uap_snapshot_pinning_test.cpp" in build,
            "official build requires every V14-11C regression asset")
    require("pinned-snapshot stable-identity deadline-paced telemetry" in main_cpp and
            "pinned-snapshot stable-identity deadline-paced telemetry" in abi_check,
            "ABI runtime gate identifies the V14-11C private plugin")

    print("UAP_SNAPSHOT_PINNING_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
