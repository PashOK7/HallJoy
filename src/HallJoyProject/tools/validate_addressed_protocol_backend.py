from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    backend = (SRC / "addressed_analog_backend.cpp").read_text(encoding="utf-8-sig")
    scheduler_h = (SRC / "addressed_poll_scheduler.h").read_text(encoding="utf-8-sig")
    scheduler = (SRC / "addressed_poll_scheduler.cpp").read_text(encoding="utf-8-sig")
    host = (SRC / "analog_host_client.cpp").read_text(encoding="utf-8-sig")
    app = (SRC / "app.cpp").read_text(encoding="utf-8-sig")
    core = (SRC / "backend.cpp").read_text(encoding="utf-8-sig")
    project = (SRC / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
    filters = (SRC / "HallJoy.vcxproj.filters").read_text(encoding="utf-8-sig")

    for name in (
        "addressed_analog_backend.cpp",
        "addressed_analog_backend.h",
        "addressed_poll_scheduler.cpp",
        "addressed_poll_scheduler.h",
    ):
        require((SRC / name).is_file(), f"missing {name}")

    require(not (SRC / "backend_aula.inc").exists(), "legacy Aula backend still exists")
    for text, label in ((project, "project"), (filters, "filters"), (core, "core"), (app, "app")):
        require("backend_aula" not in text.lower(), f"legacy Aula reference remains in {label}")
    for path in SRC.glob("*.cpp"):
        require("AulaCommMode" not in path.read_text(encoding="utf-8-sig", errors="ignore"),
                f"legacy Aula setting remains in {path.name}")

    ET.parse(SRC / "HallJoy.vcxproj")
    ET.parse(SRC / "HallJoy.vcxproj.filters")

    require("kMaxKeysPerPacket = 9" in scheduler_h, "packet key limit changed")
    require("kMaxPhysicalKeys = 255" in scheduler_h, "scheduler is still model-sized")
    require("MakePacket(0x94, 0x02" in backend, "addressed poll command missing")
    require("MakePacket(0x94, 0x00" not in backend, "service stream command must not be sent")
    require("MakePacket(0x94, 0x05" not in backend, "old registration command must not be sent")
    require("MakePacket(0x83, 0x00" in backend, "read-only key-map query missing")
    require("MakePacket(0x98, 0x02" in backend, "last-key diagnostic stop command missing")
    require("kProtocolUsagePage = 0xFF60" in backend and "kProtocolUsage = 0x0061" in backend,
            "HID fingerprint missing")
    require("attrs.VendorID !=" not in backend and "attrs.ProductID !=" not in backend,
            "backend is still hard-coded to one VID/PID")
    require("ProbeAddressedResponse" in backend and "probe accepted" in backend,
            "capability probe missing")
    require("#if defined(HALLJOY_DIAGNOSTIC)" in backend and "HallJoyAddressedAnalogTrace.log" in backend,
            "Addressed trace must be diagnostic-only")
    require("kBackgroundSlotsPerPacket = 2" in scheduler, "background reservation changed")
    require("FindBestPriority" in scheduler and "CountLive" in scheduler, "priority scheduler incomplete")

    require("kMaxConsecutiveResponseMisses" in backend and "kMaxNoResponseUs" in backend,
            "bounded response recovery policy missing")
    require("recoveredMisses" in backend and "lateResponses" in backend,
            "recovery telemetry missing")
    require("CancelReaderIo" in backend and "ForceCloseReaderHandle" in backend and
            "g_readerExitEvent" in backend,
            "reader shutdown cancellation path missing")
    require('DumpTrace(L"response absent for 28 ms")' not in backend,
            "single-timeout session termination remains")

    prepare_pos = app.index("AddressedAnalog_PrepareProtocolRouting")
    backend_pos = app.index("g_backendReady = Backend_Init")
    start_pos = app.index("AddressedAnalog_Start")
    require(prepare_pos < backend_pos < start_pos,
            "addressed capability routing must precede UAP and active polling must start afterwards")
    require("NativeAnalogProtocol::Addressed09402" in backend and
            "NativeAnalogRouting_Claim" in backend,
            "addressed protocol does not participate in central native arbitration")
    require("RealtimeLoop_NotifyInputChanged" in backend,
            "addressed changes do not wake the common low-latency path")
    require("AddressedAnalog_Stop" in app and "AddressedAnalog_NotifyDeviceChange" in app,
            "app lifecycle integration missing")
    require("AddressedAnalog_GetMilli" in core and "AddressedAnalog_OwnsHid" in core,
            "backend read integration missing")

    require("kUseChildDebugger = false" in host,
            "production child debugger is not disabled")
    require("kUseChildDebugger ? DEBUG_ONLY_THIS_PROCESS : 0u" in host,
            "diagnostic-only child debugger selection missing")
    require("TerminateThread(snapshotBridge" not in host and "TerminateThread(supervisor" not in host,
            "unsafe analog host shutdown TerminateThread remains")
    require("deferred OS cleanup" in host,
            "bounded process-exit fallback missing")
    require("supervisor child start" in host and "supervisor child exit" in host and
            "client shutdown complete" in host,
            "release host lifecycle markers missing from HallJoyAnalogHost.log")

    require("addressed_analog_backend.cpp" in project and "addressed_poll_scheduler.cpp" in project,
            "project does not compile addressed modules")
    require("qbz75_analog_backend.cpp" not in project and "qbz75_poll_scheduler.cpp" not in project,
            "old model-specific files remain in project")

    table_start = backend.index("constexpr std::array<addressed::PollKeyConfig, 82> kCanonicalKeys")
    table_end = backend.index("struct ScopedHandle", table_start)
    table = backend[table_start:table_end]
    pairs = re.findall(r"\{\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)\s*\}", table)
    require(len(pairs) == 82, f"expected 82 canonical fallback positions, got {len(pairs)}")
    mapping = {int(key, 16): int(hid, 16) for key, hid in pairs}
    require(mapping[0x1E] == 0x1A and mapping[0x2B] == 0x04 and
            mapping[0x2C] == 0x16 and mapping[0x2D] == 0x07,
            "verified WASD fallback mapping changed")

    print("Addressed analogue universal-routing static validation passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
