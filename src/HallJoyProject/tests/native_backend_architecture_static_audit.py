#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "src" / "HallJoyProject" / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="replace")


def main() -> int:
    catalog = text(SRC / "native_analog_backends.def")
    getters = re.findall(r"HALLJOY_NATIVE_BACKEND\((\w+)\)", catalog)
    require(len(getters) >= 5, "native catalog lost built-in protocols")
    require(len(getters) == len(set(getters)), "duplicate descriptor getter in catalog")

    all_cpp = "\n".join(text(path) for path in SRC.glob("*.cpp"))
    for getter in getters:
        require(re.search(rf"const\s+NativeAnalogBackendDescriptor&\s+{getter}\s*\(", all_cpp) is not None,
                f"catalog getter has no definition: {getter}")

    app = text(SRC / "app.cpp")
    backend = text(SRC / "backend.cpp")
    ui = text(SRC / "keyboard_subpages.cpp")
    registry = text(SRC / "native_analog_backend_registry.cpp")
    project = text(SRC / "HallJoy.vcxproj")

    require("NativeAnalogBackends_CatalogIsValid" in app, "app does not fail closed on invalid catalog")
    require("NativeAnalogBackends_PrepareRouting" in app, "app does not use catalog routing")
    require("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime)" in app,
            "AfterRealtime catalog phase missing")
    require("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput)" in app,
            "AfterRawInput catalog phase missing")
    require("NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap)" in backend,
            "BeforeUap catalog phase missing")
    require("NativeAnalogBackends_ReadMilli(hidKeycode)" in backend,
            "common native value aggregation missing")
    require("NativeAnalogBackends_GetTelemetry" in backend and "nativeProtocolCount" in ui,
            "generic native UI telemetry route missing")
    require(registry.count('#include "native_analog_backends.def"') >= 2,
            "manifest must generate both getter declarations and catalog entries")
    require("NativeAnalogBackends_CatalogIsValid" in registry and "std::strcmp" in registry,
            "runtime duplicate/descriptor validation missing")

    read_start = backend.index("static float ReadRaw01Cached")
    read_end = backend.index("static float ReadFiltered01Cached", read_start)
    read_body = backend[read_start:read_end]
    for forbidden in ("Hex80_GetMilli", "AddressedAnalog_GetMilli", "Mad68ProR_GetMilli",
                      "g_sparkAnalogMilli[", "g_sayoAnalogMilli["):
        require(forbidden not in read_body, f"device-specific read leaked into common path: {forbidden}")

    native_app = app[app.find("#if defined(HALLJOY_MAD68PR_NATIVE)"):]
    for forbidden in ("Mad68ProR_Start()", "Hex80_Start()", "AddressedAnalog_Start()"):
        require(forbidden not in native_app, f"device-specific lifecycle leaked into universal app path: {forbidden}")

    for required in ("native_analog_backend.h", "native_analog_backend_registry.h",
                     "native_analog_backends.def", "native_analog_backend_registry.cpp"):
        require(required in project, f"MSBuild project missing {required}")

    for doc in ("ARCHITECTURE_OVERVIEW.md", "NEW_PROTOCOL_WORKSHEET.md",
                "ADDING_NATIVE_ANALOG_PROTOCOL.md", "NATIVE_BACKEND_CONTRACT.md",
                "PROTOCOL_REVIEW_CHECKLIST.md", "TESTING_NEW_PROTOCOL.md"):
        require((ROOT / "docs" / "development" / doc).exists(), f"missing developer doc {doc}")
    generator = text(ROOT / "tools" / "new_native_backend.py")
    runner = ROOT / "tools" / "run_native_backend_checks.py"
    require(runner.exists(), "native backend check runner missing")
    require("protocol_header" in generator and "protocol_cpp" in generator and
            "run_native_backend_checks.py" in generator,
            "generator does not scaffold a pure protocol parser/test workflow")
    require((ROOT / ".github" / "PULL_REQUEST_TEMPLATE" / "new-protocol.md").exists(),
            "new-protocol pull request template missing")

    print(f"native backend architecture audit: OK ({len(getters)} registered protocols)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"native backend architecture audit: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
