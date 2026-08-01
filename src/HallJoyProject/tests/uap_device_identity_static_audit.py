#!/usr/bin/env python3
"""Verify the V14-11B path-stable private-UAP device identity contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
PLUGIN = REPO / "third_party" / "UniversalAnalogPluginFixed"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    identity = (PLUGIN / "halljoy_uap_device_identity.h").read_text(encoding="utf-8-sig")
    main_cpp = (PLUGIN / "main.cpp").read_text(encoding="utf-8-sig")
    ui = (ROOT / "HallJoy" / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
    runner = (REPO / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
    build = (REPO / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
    test = (ROOT / "tests" / "uap_device_identity_test.cpp").read_text(encoding="utf-8-sig")

    require("HallJoy/UAP/device-id/v2" in identity and "hid_path" in identity,
            "identity has a versioned HID-path domain")
    require("NormalizeAscii" in identity and "value == '/'" in identity,
            "Windows path case and separator variants normalize identically")
    require("HashU64(hash, static_cast<std::uint64_t>(value.size()))" in identity,
            "identity strings are length-framed")
    require("if (!input.hid_path.empty())" in identity and
            "DeviceIdentity{ base, true }" in identity,
            "a real HID path is duplicate-safe and occurrence-independent")
    require("fallback_occurrence" in identity and "DeviceIdentity{" in identity and
            "false" in identity,
            "pathless metadata uses an explicitly non-stable occurrence fallback")

    require('#include "halljoy_uap_device_identity.h"' in main_cpp,
            "production plugin includes the tested identity implementation")
    require("SafeHID v11 stable-identity deadline-paced telemetry" in main_cpp,
            "plugin build identity exposes the stable-ID generation")
    require("kbd.hid.path" in main_cpp and "MakeDeviceIdentity(identity_input, occurrence)" in main_cpp,
            "discovery assigns IDs from the actual Soup HID path")
    require("make_device_identity_base" not in main_cpp and "halljoy_hash_bytes" not in main_cpp,
            "legacy enumeration-only identity implementation is removed")
    require(main_cpp.count("if (duplicate_safe_id)") == 1 and
            main_cpp.count("if (dev->duplicate_safe_id)") == 1,
            "telemetry and dense snapshots publish duplicate-safe only when true")

    require("stable HID interface path" in ui and
            "enumeration fallback (path unavailable)" in ui,
            "UI exposes stable versus fallback identity truthfully")
    require("permutations == 40320" in test and "generation != 100000" in test and
            "index != 250000" in test and "occurrence != 1024" in test,
            "portable test covers permutations, reconnects, collision smoke and fallback")
    require("0x10411b4549e44638ull" in test and "0x5a6fc9c7cecb690eull" in test,
            "persisted-ID golden vectors prevent accidental algorithm drift")
    require("uap_device_identity_test.cpp" in runner,
            "portable runner executes the identity stress test")
    require("stable-identity deadline-paced telemetry" in
            (REPO / "tools" / "check_private_uap_abi.py").read_text(encoding="utf-8-sig"),
            "real ABI gate rejects a stale pre-V11 private plugin")
    require("halljoy_uap_device_identity.h" in build and
            "uap_device_identity_static_audit.py" in build and
            "uap_device_identity_test.cpp" in build,
            "official build requires every V14-11B regression asset")

    print("UAP_DEVICE_IDENTITY_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
