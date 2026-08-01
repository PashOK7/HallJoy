#!/usr/bin/env python3
"""Verify the V14-11D exact HID-interface ownership chain."""

from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
PLUGIN = REPO / "third_party" / "UniversalAnalogPluginFixed"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="strict")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def require_gate_before_open(source: str, start: str, gate: str, opening: str, label: str) -> None:
    begin = source.find(start)
    gate_at = source.find(gate, begin)
    open_at = source.find(opening, begin)
    require(begin >= 0 and gate_at >= begin and open_at >= begin and gate_at < open_at,
            f"{label} rejects foreign exact claims before opening HID")


def main() -> int:
    claim = read(PLUGIN / "halljoy_native_hid_claim.h")
    registry = read(HALL / "native_hid_interface_claim_registry.h")
    routing_h = read(HALL / "native_analog_routing.h")
    routing = read(HALL / "native_analog_routing.cpp")
    main_cpp = read(PLUGIN / "main.cpp")
    patch = read(PLUGIN / "tools" / "Apply-Soup-Madlions-Fix.ps1")
    overlay = read(PLUGIN / "overlay" / "Soup" / "soup" / "hwHid.cpp")
    mad = read(HALL / "mad68pr_backend.cpp")
    hex80 = read(HALL / "hex80_backend.cpp")
    addressed = read(HALL / "addressed_analog_backend.cpp")
    spark = read(HALL / "backend_sparklink.inc")
    sayo = read(HALL / "backend_sayo.inc")
    test = read(PROJECT / "tests" / "native_hid_interface_claim_test.cpp")
    runner = read(REPO / "tools" / "run_native_backend_checks.py")
    build = read(REPO / "tools" / "build.ps1")

    require(all(token in claim for token in (
        "NormalizeUnit", "FingerprintWide", "FingerprintUtf8",
        "MakeInterfaceClaimToken", "TokenListContains", "utf16_units")),
        "one shared implementation owns normalization, hashing and exact token parsing")
    require("unit == static_cast<std::uint16_t>('/')" in claim and
            "unit >= static_cast<std::uint16_t>('A')" in claim,
            "path separator and ASCII case variants normalize identically")
    require("left.size() != right.size()" in claim and "list.find(';', begin)" in claim,
            "environment membership is exact-token rather than substring matching")

    require(all(token in registry for token in (
        "InterfaceClaimRegistry", "MakeInterfaceClaimToken(interface_path)",
        "existing->protocol == protocol", "claim.token == token", "ProtocolHasClaims")),
        "production registry is first-claim-wins per exact interface token")
    require("std::sort(claims_.begin(), claims_.end()" in registry,
            "published claim ordering is deterministic across enumeration order")
    require("const wchar_t* interfacePath" in routing_h and
            "InterfaceClaimRegistry<NativeAnalogProtocol>" in routing and
            "HALLJOY_UAP_NATIVE_HID_PATHS" in routing,
            "native routing publishes exact interface claims through the shared registry")

    production_chain = "\n".join((routing_h, routing, main_cpp, patch, overlay))
    require("HALLJOY_UAP_NATIVE_HID_IDS" not in production_chain and
            "vid_%04x&pid_%04x" not in production_chain,
            "legacy coarse VID/PID ownership is absent from the production chain")

    require_gate_before_open(mad, "EnumerateBrandCandidates", "NativeAnalogRouting_IsClaimed(",
                             "auto metadata = OpenPath", "MAD68")
    require_gate_before_open(hex80, "EnumerateCandidates", "NativeAnalogRouting_IsClaimed(",
                             "ScopedHandle metadata(CreateFileW", "Hex80")
    require_gate_before_open(addressed, "EnumerateCandidates", "NativeAnalogRouting_IsClaimed(",
                             "auto metadata = OpenPath", "Addressed")
    require_gate_before_open(spark, "SparkTryOpenDevice", "NativeAnalogRouting_IsClaimed(",
                             "HANDLE h = CreateFileW", "SparkLink")
    require_gate_before_open(sayo, "SayoCollectMatchingDevices", "NativeAnalogRouting_IsClaimed(",
                             "HANDLE h = CreateFileW", "Sayo")

    require("path.attrs.ProductID, path.path.c_str()" in mad,
            "MAD68 claims the exact interface it proved")
    require("candidate.path.c_str(), NativeAnalogProtocol::Hex80" in hex80,
            "Hex80 claims the exact interface it proved")
    require("candidate.path.c_str(), NativeAnalogProtocol::Addressed09402" in addressed,
            "Addressed claims the exact interface it proved")
    require("vid, pid, path.c_str(), NativeAnalogProtocol::SparkLink" in spark,
            "SparkLink claims the exact interface it opened")
    require("std::wstring path;" in sayo and "std::all_of(readers.begin(), readers.end()" in sayo and
            "reader.path.c_str()" in sayo,
            "Sayo retains and claims every opened reader interface")
    require("NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol::SparkLink)" in spark and
            "NativeAnalogRouting_ProtocolHasClaims(NativeAnalogProtocol::SayoDepth)" in sayo,
            "SparkLink and Sayo reconnect only through their original exact claims")

    require('extern "C" bool halljoy_should_exclude_hid_interface' in main_cpp and
            '#include "halljoy_native_hid_claim.h"' in main_cpp and
            "MakeInterfaceClaimToken(interface_path)" in main_cpp,
            "plugin pre-open hook uses the shared production fingerprint")
    require("halljoy_uap_native_hid_excluded(kbd.hid.path)" in main_cpp and
            "MakeInterfaceClaimTokenUtf8(interface_path)" in main_cpp,
            "plugin post-open guard independently checks the actual Soup path")
    for source, label in ((patch, "generated Soup patch"), (overlay, "locked Soup overlay")):
        marker = source.find("HallJoy native analogue pre-open exclusion")
        hook = source.find("halljoy_should_exclude_hid_interface(device_interface)", marker)
        opening = source.find("hid.handle = CreateFileW", marker)
        require(marker >= 0 and hook >= marker and opening > hook,
                f"{label} calls the shared hook before CreateFileW")
        require("halljoy_path_contains_ci" not in source,
                f"{label} contains no duplicate path-matching algorithm")

    require("interface0" in test and "interface1" in test and "token0 != token1" in test,
            "portable test separates sibling interfaces with identical VID/PID")
    require("unicode_path_utf8" in test and "static_cast<char>(0xff)" in test and
            'MakeInterfaceClaimTokenUtf8("\\xc0\\xaf")' in test,
            "portable test covers Unicode equivalence and malformed UTF-8 replacement")
    require("generation < 10000" in test and "synthetic_paths = 300000" in test and
            "TokenListContains" in test and "claims_per_generation" in test,
            "portable test covers reorder/reconnect, exact parsing and collision smoke")
    require("native_hid_interface_claim_test.cpp" in runner,
            "portable runner executes the exact-interface stress test")
    require(all(token in build for token in (
        "halljoy_native_hid_claim.h", "native_hid_interface_claim_registry.h",
        "native_hid_interface_claim_test.cpp", "native_hid_interface_claim_static_audit.py")),
        "official build requires every V14-11D regression asset")

    print("NATIVE_HID_INTERFACE_CLAIM_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
