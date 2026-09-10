#!/usr/bin/env python3
"""Guard the experimental IROK ND75/M484 admission and lifecycle contract."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
backend = (root / "HallJoy" / "irok_nd75_backend.cpp").read_text(encoding="utf-8")
protocol = (root / "HallJoy" / "irok_nd75_protocol.cpp").read_text(encoding="utf-8")
catalog = (root / "HallJoy" / "native_analog_backends.def").read_text(encoding="utf-8")
project = (root / "HallJoy" / "HallJoy.vcxproj").read_text(encoding="utf-8")
stability = (root / "HallJoy" / "stability_trace.cpp").read_text(encoding="utf-8")

checks = {
    "exact USB identity is required":
        "candidate.attributes.VendorID != kVendorId" in backend and
        "candidate.attributes.ProductID != kProductId" in backend and
        "constexpr std::uint16_t kVendorId = 0x0416" in backend and
        "constexpr std::uint16_t kProductId = 0x7372" in backend,
    "HID report envelope is exact rather than minimum-sized":
        "InputReportByteLength == irok_nd75::kReportBytes" in backend and
        "OutputReportByteLength == irok_nd75::kReportBytes" in backend,
    "firmware identity must match controller and product":
        '"M484"' in protocol and '"X86HERGB"' in protocol and
        "IsExpectedDevice" in backend,
    "admission validates the asymmetric M484 capability response":
        "BuildCapabilityRequest" in backend and
        "DecodeCapabilityInfo" in backend and
        "kHostAnalogCommand" in protocol and
        "kDeviceAnalogCommand" in protocol and
        "capability_soft_fallback" not in backend and
        "capabilityProved" not in backend and
        "if (!ReceiveCapability(session, &proof.capability))" in backend,
    "only documented read and reversible stream commands are emitted":
        "BuildIdentityRequest" in backend and
        "BuildCapabilityRequest" in backend and
        "BuildSubscriptionRequest" in backend and
        "BuildUnsubscribeRequest" in backend and
        "BuildKeyMapRequest" not in backend and
        "BuildSnapshotRequest" not in backend,
    "all mapped keys are authoritative immediately after subscription":
        "PublishOwnership(proof.map)" in backend and
        "g_owned[hid].store(1" in backend,
    "hotplug worker remains available without a startup device":
        "hotplug_ready=1" in backend and
        "const auto candidates = Enumerate(false, true)" in backend and
        "provenPresent" in backend,
    "stop cancellation is bounded and classified":
        "CancelIoEx(g_activeHandle, nullptr)" in backend and
        "expectedStopCancellation" in backend and
        "kStopTimeoutMs = 3000" in backend,
    "proof I/O is registered for cancellation and observes stop":
        "RegisterActive();" in backend and
        "~Session() { ReleaseActive(); }" in backend and
        "while (!g_stop.load(std::memory_order_acquire)" in backend and
        "if (g_stop.load(std::memory_order_acquire)) return false;" in backend,
    "transport loss neutralizes input and returns to reconnect":
        "ClassifyReadFailure" in backend and
        "ReadFailureAction::EndSession" in backend and
        "transportLost = true" in backend and
        "transport.disconnected" in backend and
        "reconnect_pending=1" in backend,
    "QPC telemetry conversion avoids long-uptime multiplication overflow":
        "(ticks / frequency) * 1000000ull" in backend and
        "(ticks % frequency) * 1000000ull" in backend and
        "ticks) * 1000000ull / frequency" not in backend,
    "experimental backend is compile-time gated":
        "#if defined(HALLJOY_IROK_ND75_EXPERIMENTAL)" in catalog and
        "IrokNd75_GetNativeBackendDescriptor" in catalog,
    "diagnostic build exposes raw, coverage, and range evidence":
        "[irok.nd75.raw]" in backend and
        "[irok.nd75.coverage]" in backend and
        "out_of_range=" in backend,
    "high-rate raw evidence has the large bounded trace profile":
        "HALLJOY_IROK_ND75_DIAGNOSTIC" in project and
        "defined(HALLJOY_IROK_ND75_DIAGNOSTIC)" in stability and
        "64u * 1024u * 1024u" in stability,
}

failed = False
for name, passed in checks.items():
    print(f"{'PASS' if passed else 'FAIL'}: {name}")
    failed |= not passed

if failed:
    raise SystemExit(1)
print("IROK_ND75_BACKEND_STATIC_AUDIT=PASS")
