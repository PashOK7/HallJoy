#!/usr/bin/env python3
"""Fail closed if the isolated Titan68 Turbo diagnostic loses its safety boundary."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
source = (HALL / "titan68_turbo_diagnostic_backend.cpp").read_text(encoding="utf-8")
header = (HALL / "native_analog_routing.h").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
backend = (HALL / "backend.cpp").read_text(encoding="utf-8")
builder = (ROOT / "tools" / "build_titan68_turbo_diagnostic.ps1").read_text(encoding="utf-8")

checks = {
    "exact official split Titan USB interfaces": all(x in source for x in (
        "kVendorId = 0x28E9", "kProductId = 0x31FD", "kControlUsagePage = 0xFF87",
        "kControlUsage = 0x0020", "kStreamUsagePage = 0xFF88", "kStreamUsage = 0x0021",
        "kControlReportId = 0x06", "kStreamReportId = 0x07",
        "kControlReportBytes = 64", "kStreamReportBytes = 3")),
    "descriptor proof requires paired control and stream interfaces": all(x in source for x in (
        "control.parentKey == stream.parentKey", "caps.InputReportByteLength == kControlReportBytes",
        "caps.InputReportByteLength == kStreamReportBytes", "in6 && out6", "in7")),
    "stream is read separately from control": all(x in source for x in (
        "Session control(pair.control, true), stream(pair.stream, false)",
        "SendAndAwait(control, &stats, 0x36, true)",
        "Listen(stream, &stats, kObservationMs, true)")),
    "only approved simulation opcode and official read-only mapping commands are emitted": all(x in source for x in (
        "BuildModeControl(std::uint8_t command, bool enabled)",
        "SendAndAwait(control, &stats, 0x36, true)",
        "SendAndAwait(control, &stats, 0x36, false)",
        "ReadControlData(session, 0x12, 64, &info)",
        "ReadControlData(session, 0x16, slots * 3, &rect)")) and "SendAndAwait(control, &stats, 0x37," not in source,
    "exact framed command format and checksum": all(x in source for x in (
        "report[4] = 1", "report[8] = enabled ? 1 : 0", "report[5] = static_cast<std::uint8_t>(sum)",
        "report[6] = static_cast<std::uint8_t>(sum >> 8)")),
    "calibration, persistent-write and firmware-control opcodes absent": all(x not in source for x in (
        "SendAndAwait(control, &stats, 0x37,", "0xB0", "0xA8", "0xA9", "HidD_SetFeature")),
    "all received frames and raw12 pairs logged": all(x in source for x in (
        "[titan68.rx]", "[titan68.tx]", "[titan68.travel]", "raw12=%u", "[titan68.summary]")),
    "mapping follows official default-key-rect protocol and is non-blocking": all(x in source for x in (
        "BuildReadControl", "ReadControlChunk", "ReadDefaultKeyMapping",
        "key_rect_size = byte[4] * 3", "[titan68.mapping.slot]",
        "continuing_without_mapping=1", "(void)ReadDefaultKeyMapping(control, &stats)")),
    "simulation exit is mandatory without any calibration command": all(x in source for x in (
        "active complete; sending mandatory 36_00", 
        "const bool simulationExitAck = SendAndAwait(control, &stats, 0x36, false)")),
    "shutdown before active phase emits no control frame": "cancelled_before_control" in source and "if (g_stop.load(std::memory_order_acquire))" in source,
    "failed acknowledgement never repeats probe": all(x in source for x in (
        "bool attempted = false", "attempted = true; (void)RunCandidate(candidate)",
        "if (attempted) break", "not a reason to repeat")),
    "visual analogue uses official mapping and normalized native values": all(x in source for x in (
        "PublishVisualAnalog", "g_hidAtKeyIndex", "g_milli", "g_maxRaw",
        "bool Owns(std::uint16_t hid)", "std::uint16_t Get(std::uint16_t hid)")),
    "diagnostic cannot start or publish ViGEm": all(x in backend for x in (
        "diagnostic.transport_only", "return preUapReady;",
        "realtime fault must not cause its recovery path", "No normal input/output processing belongs")),
    "target-scoped keyboard correlation": "VID_28E9&PID_31FD" in source and "[titan68.raw_key]" in source and "Titan68TurboDiagnostic_RecordRawKeyboardEvent" in app,
    "separate routing identity": "Titan68TurboDiagnostic = 13" in header,
    "one-backend isolated catalog": "!defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)" in catalog and "Titan68TurboDiagnostic_GetNativeBackendDescriptor" in catalog,
    "independent MSBuild image": all(x in project for x in (
        "HallJoyTitan68TurboDiagnostic", "HallJoy-Madlions-Titan68-Turbo-Diagnostic",
        "HALLJOY_TITAN68_TURBO_DIAGNOSTIC")),
    "diagnostic PDB writes are serialized": "<AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>" in project,
    "builder selects single-threaded diagnostic target": all(x in builder for x in (
        "HallJoyTitan68TurboDiagnostic=true", "UseMultiToolTask=false", "/m:1")),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("TITAN68 TURBO DIAGNOSTIC STATIC AUDIT FAILED\n" + "\n".join(f" - {name}" for name in failed))

print("TITAN68 TURBO DIAGNOSTIC STATIC AUDIT PASSED")
print("active allow-list: read-only 12/16 mapping then 36:01, 36:00; calibration is absent and report 07 is visual-only raw12 travel telemetry")
