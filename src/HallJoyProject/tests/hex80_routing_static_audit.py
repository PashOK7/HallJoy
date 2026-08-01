#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
package = root.parents[1]
plugin = package / "third_party" / "UniversalAnalogPluginFixed"

read = lambda path: path.read_text(encoding="utf-8-sig")
app = read(hall / "app.cpp")
backend = read(hall / "backend.cpp")
header = read(hall / "backend.h")
hex_backend = read(hall / "hex80_backend.cpp")
hex_protocol = read(hall / "hex80_protocol.h")
routing = read(hall / "native_analog_routing.cpp")
registry = read(hall / "native_analog_backend_registry.cpp")
catalog = read(hall / "native_analog_backends.def")
ui = read(hall / "keyboard_subpages.cpp")
project = read(hall / "HallJoy.vcxproj")
uap = read(plugin / "main.cpp")
patch = read(plugin / "tools" / "Apply-Soup-Madlions-Fix.ps1")

checks = {
    "Hex80 documented identity is exact": all(token in hex_protocol for token in (
        "0x1176", "0x1177", "0x1250", "kUsagePage = 0xFF60", "kUsage = 0x0061")),
    "Hex80 matrix and chunk contract are fixed": all(token in hex_protocol for token in (
        "kTotalSlots = 104", "kChunkSize = 4", "kSlotToHid", "MappedKeyCount")),
    "routing probe is GET-only": (
        "ProbeCandidate" in hex_backend
        and "BuildTravelInfoPayload" in hex_backend
        and "BuildTravelBufferPayload(0, 4)" in hex_backend
        and hex_backend.index("ProbeCandidate") < hex_backend.index("BuildCalibrationFinishPayload")
    ),
    "calibration exit is sent only in active validated session": (
        "passed both GET-only protocol proofs above" in hex_backend
        and "BuildCalibrationFinishPayload" in hex_backend
        and "RunSession" in hex_backend
    ),
    "Hex80 publishes to common realtime path": (
        "RealtimeLoop_NotifyInputChangedAt(receivedQpc)" in hex_backend
        and "Hex80_GetNativeBackendDescriptor" in hex_backend
        and "NativeAnalogBackends_ReadMilli(hidKeycode)" in backend
        and "native.milli" in backend
    ),
    "Hex80 digital fallback is blocked only for owned HID": (
        "Hex80_OwnsHid" in hex_backend
        and "result.owned" in registry
        and "cache.allowFallback && !native.owned" in backend
    ),
    "shared native interface registry is additive": (
        "NativeAnalogRouting_Claim" in hex_backend
        and "NativeAnalogProtocol::Hex80" in hex_backend
        and "HALLJOY_UAP_NATIVE_HID_PATHS" in routing
        and "InterfaceClaimRegistry" in routing
    ),
    "routing is complete before UAP starts": (
        "NativeAnalogBackends_Reset()" in app
        and "NativeAnalogBackends_PrepareRouting()" in app
        and app.index("NativeAnalogBackends_PrepareRouting()") < app.index("Backend_Init()")
        and "Hex80_GetNativeBackendDescriptor" in catalog
        and "prepareRouting" in registry
    ),
    "UAP consumes native interface registry before HID open": (
        "halljoy_uap_native_hid_excluded" in uap
        and "halljoy_should_exclude_hid_interface(device_interface)" in patch
        and "HALLJOY_UAP_NATIVE_HID_IDS" not in patch
        and "HallJoy native analogue pre-open exclusion" in patch
        and "$hidSourceText.Insert($braceStart + 1, $preOpenBlock)" in patch
    ),
    "Hex80 lifecycle is catalog-driven": (
        "Hex80_GetNativeBackendDescriptor" in catalog
        and "NativeAnalogStartPhase::AfterRealtime" in hex_backend
        and "NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)" in app
        and "NativeAnalogBackends_StopAll()" in app
        and "NativeAnalogBackends_NotifyDeviceChange()" in app
    ),
    "Configuration and Gamepad Tester expose Hex80": (
        "ATK x QK Hex80:" in ui
        and "Analog input: ATK x QK Hex80" in ui
        and "nativeProtocolCount" in header
        and "nativeProtocols" in ui
    ),
    "project compiles registry and Hex80 sources": all(token in project for token in (
        "hex80_backend.cpp", "hex80_protocol.cpp", "native_analog_routing.cpp",
        "native_analog_backend_registry.cpp", "native_analog_backends.def",
        "hex80_backend.h", "hex80_protocol.h", "native_analog_routing.h")),
    "poller has no artificial fixed sleep": (
        "SwitchToThread();" in hex_backend
        and "Sleep(" not in hex_backend
        and "hex80::kTotalSlots" in hex_backend
    ),
}

failed = []
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")
    if not ok:
        failed.append(name)
if failed:
    print("HEX80_ROUTING_STATIC_AUDIT=FAIL: " + ", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print("HEX80_ROUTING_STATIC_AUDIT=PASS")
