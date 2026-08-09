#!/usr/bin/env python3
"""Guard HallJoy's Windows UAP HID receive path against timer polling regressions."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / "third_party" / "UniversalAnalogPluginFixed" / "overlay" / "Soup" / "soup" / "hwHid.hpp"
SOURCE = HEADER.with_suffix(".cpp")

header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")

receive_start = source.index("const Buffer<>& hwHid::receiveReportWithReportId() noexcept")
receive_end = source.index("const Buffer<>& hwHid::receiveReportWithoutReportId() noexcept", receive_start)
receive = source[receive_start:receive_end]

kick_start = source.index("void hwHid::kickOffRead() noexcept", source.index("#if SOUP_WINDOWS", receive_end))
kick_end = source.index("#elif SOUP_MACOS", kick_start)
kick = source[kick_start:kick_end]

assert "OVERLAPPED read_overlapped{};" in header
assert "GetOverlappedResult(handle, &read_overlapped, &bytes_read, TRUE)" in receive
assert "\n\t\t\t\tSleep(" not in receive
assert "ReadFile(handle, read_buffer.data()" in kick
assert "CancelIoEx(handle, &read_overlapped)" in source

print("UAP HID receive event audit passed")
