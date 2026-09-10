#!/usr/bin/env python3
"""Verify the fail-closed input/topology gate used by the RM-16 owner."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
BACKEND = (HALL / "backend.cpp").read_text(encoding="utf-8")
HEADER = (HALL / "backend.h").read_text(encoding="utf-8")


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    assert start >= 0, f"missing function: {signature}"
    opening = source.find("{", start)
    assert opening >= 0, f"missing body: {signature}"
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


def require(condition: bool, message: str) -> None:
    assert condition, message
    print(f"PASS: {message}")


tick = body(BACKEND, "void Backend_Tick()")
device_change = body(BACKEND, "void Backend_NotifyDeviceChange()")
key_event = body(BACKEND, "void Backend_NotifyKeyboardEvent(")
mouse_delta = body(BACKEND, "void Backend_AddMouseDelta(")
mouse_button = body(BACKEND, "void Backend_SetMouseBindButtonState(")
mouse_wheel = body(BACKEND, "void Backend_PulseMouseBindWheel(")

require("Backend_SetRuntimeAdmission" in HEADER and
        "Backend_IsRuntimeAdmissionOpen" in HEADER,
        "backend exposes one explicit runtime-admission contract")
require("g_runtimeAdmission{ true }" in BACKEND and
        "void Backend_SetRuntimeAdmission(bool admitted) noexcept" in BACKEND and
        "bool Backend_IsRuntimeAdmissionOpen() noexcept" in BACKEND,
        "admission state is explicit and atomic-backed")
for name, source in (
    ("realtime output", tick),
    ("topology refresh", device_change),
    ("keyboard callback", key_event),
    ("mouse delta callback", mouse_delta),
    ("mouse button callback", mouse_button),
    ("mouse wheel callback", mouse_wheel),
):
    require("g_runtimeAdmission.load(std::memory_order_acquire)" in source,
            f"closed admission rejects {name}")

print("RUNTIME_ADMISSION_STATIC_AUDIT=PASS")
