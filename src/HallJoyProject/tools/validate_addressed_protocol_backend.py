#!/usr/bin/env python3
"""Validate the current catalog-driven Addressed 09/94/02 backend."""

from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = Path(__file__).resolve().parents[3]
SRC = PROJECT_ROOT / "HallJoy"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    required = (
        "addressed_analog_backend.cpp",
        "addressed_analog_backend.h",
        "addressed_poll_scheduler.cpp",
        "addressed_poll_scheduler.h",
        "native_analog_backend_registry.cpp",
        "native_analog_backends.def",
    )
    for name in required:
        require((SRC / name).is_file(), f"missing {name}")

    backend = read(SRC / "addressed_analog_backend.cpp")
    scheduler_h = read(SRC / "addressed_poll_scheduler.h")
    scheduler = read(SRC / "addressed_poll_scheduler.cpp")
    app = read(SRC / "app.cpp")
    core = read(SRC / "backend.cpp")
    catalog = read(SRC / "native_analog_backends.def")
    project = read(SRC / "HallJoy.vcxproj")
    filters = read(SRC / "HallJoy.vcxproj.filters")
    runner = read(REPO_ROOT / "tools" / "run_native_backend_checks.py")

    ET.parse(SRC / "HallJoy.vcxproj")
    ET.parse(SRC / "HallJoy.vcxproj.filters")

    require(not (SRC / "backend_aula.inc").exists(), "legacy Aula backend still exists")
    for text, label in ((project, "project"), (filters, "filters"), (core, "core"), (app, "app")):
        require("backend_aula" not in text.lower(), f"legacy Aula reference remains in {label}")

    require("kMaxKeysPerPacket = 9" in scheduler_h, "packet key limit changed")
    require("kMaxPhysicalKeys = 255" in scheduler_h, "scheduler is still model-sized")
    require("kBackgroundSlotsPerPacket = 2" in scheduler, "background reservation changed")
    require("FindBestPriority" in scheduler and "CountLive" in scheduler,
            "priority/background scheduler is incomplete")

    require("MakePacket(0x94, 0x02" in backend, "read-only addressed poll command missing")
    require("MakePacket(0x83, 0x00" in backend, "read-only key-map query missing")
    require("MakePacket(0x98, 0x02" in backend, "last-key diagnostic stop command missing")
    require("MakePacket(0x94, 0x00" not in backend, "service stream command must not be sent")
    require("MakePacket(0x94, 0x05" not in backend, "old registration command must not be sent")
    require("kProtocolUsagePage = 0xFF60" in backend and "kProtocolUsage = 0x0061" in backend,
            "HID usage fingerprint changed")
    require("attrs.VendorID !=" not in backend and "attrs.ProductID !=" not in backend,
            "backend must remain capability-probed rather than VID/PID hard-coded")
    require("ProbeAddressedResponse" in backend and "probe accepted" in backend,
            "capability proof is missing")
    require("NativeAnalogRouting_Claim" in backend,
            "exact HID interface claim is missing")

    require("kMaxConsecutiveResponseMisses" in backend and "kMaxNoResponseUs" in backend,
            "bounded response-recovery policy is missing")
    require("recoveredMisses" in backend and "lateResponses" in backend,
            "response-recovery telemetry is missing")
    require("CancelReaderIo" in backend and "g_readerExitEvent" in backend,
            "reader cancellation/completion path is missing")
    require("ForceCloseReaderHandle" not in backend,
            "cross-thread force-close must not return to Addressed shutdown")
    require("AddressedAnalog_StopGeneration" in backend and
            "halljoy::lifecycle::StopResult" in backend and
            "thread_handle_retained=1" in backend and "restart_blocked=1" in backend,
            "bounded join with truthful resource retention is missing")
    require('DumpTrace(L"response absent for 28 ms")' not in backend,
            "single-timeout session termination returned")

    require("AddressedAnalog_GetNativeBackendDescriptor" in catalog,
            "Addressed descriptor is absent from the central catalog")
    require('"addressed-099402"' in backend and
            "NativeAnalogProtocol::Addressed09402" in backend and
            "NativeAnalogStartPhase::AfterRealtime" in backend,
            "Addressed descriptor identity/start phase changed")
    require("&AddressedAnalog_PrepareProtocolRouting" in backend and
            "&AddressedAnalog_Start" in backend and
            "&AddressedAnalog_GetMilli" in backend,
            "Addressed descriptor callbacks are incomplete")
    require("NativeAnalogBackends_PrepareRouting" in app and
            "NativeAnalogBackends_StartPhase" in app and
            "NativeAnalogBackends_StopAll" in app and
            "NativeAnalogBackends_NotifyDeviceChange" in app,
            "production app lifecycle does not use the central catalog")
    require("AddressedAnalog_PrepareProtocolRouting" not in app and
            "AddressedAnalog_Start" not in app,
            "production startup bypasses the central catalog")
    require("NativeAnalogBackends_ReadMilli" in core,
            "common analogue read path bypasses the central catalog")
    require("RealtimeLoop_NotifyInputChanged" in backend,
            "Addressed publications do not wake the common realtime path")

    require("addressed_analog_backend.cpp" in project and
            "addressed_poll_scheduler.cpp" in project,
            "project does not compile Addressed modules")
    require("validate_addressed_protocol_backend.py" in runner,
            "current Addressed validator is not part of the unified runner")

    table_start = backend.index("constexpr std::array<addressed::PollKeyConfig, 82> kCanonicalKeys")
    table_end = backend.index("struct ScopedHandle", table_start)
    pairs = re.findall(r"\{\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)\s*\}",
                       backend[table_start:table_end])
    require(len(pairs) == 82, f"expected 82 canonical fallback positions, got {len(pairs)}")
    mapping = {int(key, 16): int(hid, 16) for key, hid in pairs}
    require(mapping.get(0x1E) == 0x1A and mapping.get(0x2B) == 0x04 and
            mapping.get(0x2C) == 0x16 and mapping.get(0x2D) == 0x07,
            "verified WASD fallback mapping changed")

    print("Addressed analogue catalog/lifecycle static validation passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
