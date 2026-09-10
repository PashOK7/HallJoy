from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
PROJECT = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
RUNNER = (ROOT.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
HEADER = (HALL / "configured_xusb_builder.h").read_text(encoding="utf-8")
SOURCE = (HALL / "configured_xusb_builder.cpp").read_text(encoding="utf-8")
FRAME = (HALL / "virtual_controller_frame.h").read_text(encoding="utf-8")
ADAPTER = (HALL / "xusb_output_adapter.cpp").read_text(encoding="utf-8")
BACKEND = (HALL / "backend.cpp").read_text(encoding="utf-8")
TEST = (ROOT / "tests" / "configured_xusb_builder_test.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


require("BuilderState" in HEADER and "BuilderState& state" in HEADER,
        "SOCD/last-key state is explicit")
require("PadConfiguration" in HEADER and "InputValues" in HEADER,
        "configuration and filtered input are explicit immutable arguments")
require("settings.h" not in SOURCE and "Bindings_Get" not in SOURCE,
        "builder cannot reread mutable global settings or bindings")
require("ReadMouseStickSample" not in SOURCE and "AnalogHostClient" not in SOURCE,
        "builder performs no mouse or provider acquisition")
require("controller::VirtualControllerFrameV1 BuildReport" in SOURCE and
        "vigem_output" not in HEADER and "0x1000u" not in SOURCE,
        "mapping builder returns a protocol-neutral controller frame")
require("South = 0" in FRAME and "Home," in FRAME and
        "DpadRight," in FRAME,
        "neutral frame owns semantic standard-gamepad buttons")
require("ButtonV1::South, 0x1000u" in ADAPTER and
        "ButtonV1::Home, 0x0400u" in ADAPTER and
        "ButtonV1::DpadRight, 0x0008u" in ADAPTER,
        "XUSB adapter owns the exact protocol button translation")
require("ReportsEqual" in ADAPTER and "left.thumbRY == right.thumbRY" in ADAPTER,
        "final output comparison names every XUSB field")
require("xusb_output::ToReport(frame)" in BACKEND,
        "qualified production route crosses the explicit XUSB adapter")
require("paired_equivalence=1" in TEST and "divergence_localized=1" in TEST,
        "behavioral gate covers paired state and localized divergence")
require("replacementState" in TEST and "replay_reset=1" in TEST,
        "replay fixture proves generation state does not leak")
require('configured_xusb_builder.cpp' in PROJECT and
        'configured_xusb_builder.h' in PROJECT and
        'virtual_controller_frame.h' in PROJECT and
        'xusb_output_adapter.cpp' in PROJECT and
        'xusb_output_adapter.h' in PROJECT,
        "MSVC project compiles the neutral frame, builder and XUSB adapter")
require('configured_xusb_builder_test.cpp' in RUNNER and
        'configured_xusb_builder.cpp' in RUNNER and
        'xusb_output_adapter.cpp' in RUNNER,
        "unified native runner executes the production builder and adapter")

print("CONFIGURED_XUSB_BUILDER_STATIC_AUDIT=PASS")
