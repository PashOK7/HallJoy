#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
package = root.parents[1]
plugin = package / "third_party" / "UniversalAnalogPluginFixed"

def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")

routing_h = read(hall / "native_analog_routing.h")
routing = read(hall / "native_analog_routing.cpp")
claim_registry = read(hall / "native_hid_interface_claim_registry.h")
contract = read(hall / "native_analog_backend.h")
registry = read(hall / "native_analog_backend_registry.cpp")
catalog = read(hall / "native_analog_backends.def")
app = read(hall / "app.cpp")
main = read(hall / "main.cpp")
backend = read(hall / "backend.cpp")
mad = read(hall / "mad68pr_backend.cpp")
hex80 = read(hall / "hex80_backend.cpp")
addressed = read(hall / "addressed_analog_backend.cpp")
spark = read(hall / "backend_sparklink.inc")
sayo = read(hall / "backend_sayo.inc")
uap = read(plugin / "main.cpp")
patch = read(plugin / "tools" / "Apply-Soup-Madlions-Fix.ps1")

families = {
    "Mad68ProR_GetNativeBackendDescriptor": mad,
    "Hex80_GetNativeBackendDescriptor": hex80,
    "AddressedAnalog_GetNativeBackendDescriptor": addressed,
    "BackendNative_GetSparkDescriptor": backend,
    "BackendNative_GetSayoDescriptor": backend,
}

checks = {
    "routing registry enumerates every native protocol family": all(token in routing_h for token in (
        "Mad68A0", "Hex80", "Addressed09402", "SparkLink", "SayoDepth")),
    "backend contract owns discovery lifecycle read and telemetry": all(token in contract for token in (
        "prepareRouting", "start", "stop", "notifyDeviceChange", "ownsHid", "getMilli", "getTelemetry")),
    "catalog registers every native backend exactly once": all(catalog.count(name) == 1 for name in families),
    "registry is exact first-claim-wins interface ownership": (
        all(token in claim_registry for token in (
            "MakeInterfaceClaimToken", "existing->protocol == protocol", "claim.token == token"))
        and "InterfaceClaimRegistry" in routing
        and "HALLJOY_UAP_NATIVE_HID_PATHS" in routing
        and "HALLJOY_UAP_NATIVE_HID_IDS" not in routing),
    "pre-UAP route order is descriptor-driven and deterministic": (
        "NativeAnalogBackends_PrepareRouting()" in app
        and app.index("NativeAnalogBackends_PrepareRouting()") < app.index("Backend_Init()")
        and "for (std::size_t i = 0; i < std::size(kCatalog); ++i)" in registry
    ),
    "before-UAP native routes start before UAP initialisation": (
        "NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)" in backend
        and backend.index("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)")
            < backend.index("wooting_analog_initialise()")
    ),
    "all native probes respect foreign ownership": all("NativeAnalogRouting_IsClaimed" in source for source in (mad, hex80, addressed, spark, sayo)),
    "all native routes publish a validated claim": all("NativeAnalogRouting_Claim" in source for source in (mad, hex80, addressed, spark, sayo)),
    "UAP exclusion occurs before any Soup HID open": (
        "HallJoy native analogue pre-open exclusion" in patch
        and "$hidSourceText.Insert($braceStart + 1, $preOpenBlock)" in patch
        and "halljoy_should_exclude_hid_interface(device_interface)" in patch
        and "HALLJOY_UAP_NATIVE_HID_IDS" not in patch
        and "halljoy_uap_native_hid_excluded" in uap),
    "Addressed protocol is active and event-driven": (
        "AddressedAnalog_GetNativeBackendDescriptor" in catalog
        and "RealtimeLoop_NotifyInputChanged" in addressed
        and "AddressedAnalog disabled" not in app),
    "independent native routes survive unavailable UAP": (
        "universal native continuation enabled; UAP unavailable for this run" in main
        and "NativeAnalogBackends_AnyProtocolDevicePresent()" in backend),
    "real analogue sources aggregate through one common registry": (
        "NativeAnalogBackends_ReadMilli(hidKeycode)" in backend
        and "result.milli = std::max" in registry
        and "cache.wootingReady && modeCode != 0" in backend),
    "digital fallback remains excluded from authoritative native values": (
        "cache.allowFallback && !native.owned" in backend),
    "generic telemetry reaches UI without per-backend wiring": (
        "nativeProtocolCount" in backend
        and "NativeAnalogBackends_GetTelemetry" in backend
        and "nativeProtocols" in read(hall / "keyboard_subpages.cpp")),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")
if failed:
    print("UNIVERSAL_NATIVE_ROUTING_STATIC_AUDIT=FAIL", file=sys.stderr)
    for name in failed:
        print(f" - {name}", file=sys.stderr)
    raise SystemExit(1)
print("UNIVERSAL_NATIVE_ROUTING_STATIC_AUDIT=PASS")
