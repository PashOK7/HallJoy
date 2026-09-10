#!/usr/bin/env python3
"""The RM-16 UI bridge must stay posted, bounded, and cancellation-aware."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "HallJoy" / "engine_runtime_ui_bridge.cpp").read_text(encoding="utf-8")
header = (root / "HallJoy" / "engine_runtime_ui_bridge.h").read_text(encoding="utf-8")

for token in (
    "PostMessageW(g_window, g_message",
    "WaitForSingleObject(completion, kUiAcknowledgementTimeoutMs)",
    "kUiAcknowledgementTimeoutMs = 5000u",
    "RequestState::Cancelled",
    "void CancelPending() noexcept",
    "bool Dispatch(std::uintptr_t token) noexcept",
):
    assert token in source, token
assert "using Handler = bool (*)(Operation operation, std::uint32_t& nativeError) noexcept;" in header
print("Engine runtime UI bridge static audit passed")
