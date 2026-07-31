#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
package = root.parents[1]
plugin = package / "third_party" / "UniversalAnalogPluginFixed"

read = lambda p: p.read_text(encoding="utf-8-sig")
mad = read(hall / "mad68pr_backend.cpp")
mad_h = read(hall / "mad68pr_backend.h")
app = read(hall / "app.cpp")
backend = read(hall / "backend.cpp")
spark = read(hall / "backend_sparklink.inc")
sayo = read(hall / "backend_sayo.inc")
uap = read(plugin / "main.cpp")
patch = read(plugin / "tools" / "Apply-Soup-Madlions-Fix.ps1")
routing = read(hall / "native_analog_routing.cpp")
registry = read(hall / "native_analog_backend_registry.cpp")
catalog = read(hall / "native_analog_backends.def")
hex80 = read(hall / "hex80_backend.cpp")
addressed = read(hall / "addressed_analog_backend.cpp")

checks = {
    "all routing prepares before UAP through manager": (
        "NativeAnalogBackends_PrepareRouting()" in app
        and app.index("NativeAnalogBackends_PrepareRouting()") < app.index("Backend_Init()")
        and "prepareRouting" in registry),
    "MADLIONS enumeration is brand-wide": (
        "item.attrs.VendorID != mad68pr::kVid" in mad
        and "item.attrs.ProductID != mad68pr::kPid" not in mad),
    "MADLIONS unknown PID requires reversible A9 ACK": (
        "ProbeNativeControlProtocol" in mad
        and "kRestoreInputOpcode" in mad
        and "ControlResponseKind::Valid" in mad),
    "MADLIONS exact HID fingerprint retained": (
        "InputReportByteLength == mad68pr::kPayloadBytes + 1u" in mad
        and "OutputReportByteLength == mad68pr::kPayloadBytes + 1u" in mad),
    "MADLIONS native route keeps the 68-key layout boundary": (
        "LooksLikeMad68Family" in mad
        and "layoutCompatible" in mad
        and "Other MADLIONS products remain available to Soup/UAP" in mad),
    "UAP receives dynamic exact VID/PID routing": (
        "HALLJOY_UAP_NATIVE_HID_IDS" in routing
        and "NativeAnalogRouting_Claim" in mad
        and "NativeAnalogRouting_Claim" in hex80
        and "halljoy_uap_native_hid_excluded" in uap
        and "HALLJOY_UAP_NATIVE_HID_IDS" in patch),
    "UAP exclusion is before Soup open": (
        "HallJoy native analogue pre-open exclusion" in patch
        and "$hidSourceText.Insert($braceStart + 1, $preOpenBlock)" in patch),
    "Raw Input targets only routed MADLIONS PIDs": (
        "Mad68ProR_IsRoutedProduct(pid)" in app
        and "pid_1109" not in app),
    "Sayo discovery is VID-wide": (
        "attr.VendorID != kSayoVendorId" in sayo
        and "attr.ProductID != kSayo" not in sayo),
    "Sayo unknown PID requires depth response": (
        "SayoProbeDepthProtocol" in sayo
        and "response[0] != 0x22" in sayo
        and "selectedPid" in sayo
        and "if (!selected)" in sayo),
    "Sayo audited PID keeps priority": (
        "std::find(products.begin(), products.end(), kSayoKnownProductId)" in sayo
        and "audited PID is absent do we send" in sayo),
    "Sayo publishes actual VID/PID": (
        "readers.front().vendorId" in sayo
        and "readers.front().productId" in sayo),
    "Sayo and Spark descriptors run before UAP": (
        "BackendNative_GetSparkDescriptor" in catalog
        and "BackendNative_GetSayoDescriptor" in catalog
        and "NativeAnalogStartPhase::BeforeUap" in backend
        and backend.index("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)")
            < backend.index("wooting_analog_initialise()")),
    "Sayo and Spark only claim proven protocols": (
        "SayoProbeDepthProtocol" in sayo
        and "NativeAnalogProtocol::SayoDepth" in sayo
        and "SparkQueryDeviceInfo(routeProbe)" in spark
        and "NativeAnalogProtocol::SparkLink" in spark),
    "Hex80 exact protocol is independently catalogued": (
        "Hex80_GetNativeBackendDescriptor" in catalog
        and "BuildTravelInfoPayload" in hex80
        and "BuildTravelBufferPayload(0, 4)" in hex80
        and "RealtimeLoop_NotifyInputChangedAt" in hex80),
    "Addressed protocol is restored and independently catalogued": (
        "AddressedAnalog_GetNativeBackendDescriptor" in catalog
        and "NativeAnalogProtocol::Addressed09402" in addressed
        and "RealtimeLoop_NotifyInputChanged" in addressed),
    "Addressed accepts protocol family rather than one VID/PID": (
        "attrs.VendorID !=" not in addressed
        and "attrs.ProductID !=" not in addressed
        and "ProbeAddressedResponse" in addressed),
    "Backend accepts any registered native source": (
        "NativeAnalogBackends_AnyProtocolDevicePresent()" in backend
        and "NativeAnalogBackends_ReadMilli(hidKeycode)" in backend
        and "Mad68ProR_IsProtocolDevicePresent" in mad_h),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")
if failed:
    print("FAILED:", ", ".join(failed), file=sys.stderr)
    raise SystemExit(1)
print("Protocol-family routing static audit passed.")
