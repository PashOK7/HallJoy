#!/usr/bin/env python3
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
source = (HALL / "mchose_ace68_diagnostic_backend.cpp").read_text(encoding="utf-8")
header = (HALL / "native_analog_routing.h").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
builder = (ROOT / "tools" / "build_mchose_ace68_diagnostic.ps1").read_text(encoding="utf-8")

checks = {
    "exact Ace 68 vendor interface": all(x in source for x in ("kVendorId = 0x41E4", "kProductId = 0x2114", "kUsagePage = 0x0001", "kUsage = 0x0000", "kReportBytes = 64")),
    "only official non-mutating query opcodes": all(x in source for x in ("0x03,0,56", "0x04,0,56", "0x05,0,56", "0x08,offset", "0xA0,offset")) and "0xA8" not in source and "0xA9" not in source and "HidD_SetFeature" not in source,
    "canonical M HUB read framing": all(x in source for x in ("BuildReadRequest", "report[4] = request.responseBytes", "55-official-WriteFile")),
    "all full reports logged": "[mchose.ace68.rx] bytes=64 data=%ls" in source and "[mchose.ace68.tx]" in source,
    "read-only baseline precedes writable session": all(x in source for x in ("Session readOnly(candidate, false)", "read_only_passive", "read_write_passive")),
    "physical key correlation is target-scoped": "VID_41E4&PID_2114" in source and "[mchose.ace68.raw_key]" in source and "MchoseAce68Diagnostic_RecordRawKeyboardEvent" in app,
    "A0 never controls gameplay": "raw_be16" in source and "bool Owns(std::uint16_t) { return false; }" in source,
    "AB stops the matrix": "stopping all active probes" in source,
    "unexpected acknowledgement stops the matrix": "unexpectedAck = result == AckResult::UnexpectedAck" in source,
    "two ordinary official transports": all(x in source for x in ("55-official-WriteFile", "55-official-OutputReport", "HidD_SetOutputReport")),
    "diagnostic-only registration": "#if defined(HALLJOY_MCHOSE_ACE68_DIAGNOSTIC)" in catalog,
    "independent routing type": "MchoseAce68Diagnostic = 10" in header,
    "independent MSBuild target": all(x in project for x in ("HallJoyMchoseAce68Diagnostic", "HallJoy-MCHOSE-Ace68-Diagnostic", "HALLJOY_MCHOSE_ACE68_DIAGNOSTIC")),
    "builder selects test target": "HallJoyMchoseAce68Diagnostic=true" in builder,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("MCHOSE ACE68 DIAGNOSTIC STATIC AUDIT FAILED\n" + "\n".join(f" - {name}" for name in failed))
print("MCHOSE ACE68 DIAGNOSTIC STATIC AUDIT PASSED")
print("active allow-list: official read-only 03, 04, 05, 08, A0; framing: official 55 via both ordinary transports")
