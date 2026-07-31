"""
Minimal direct Hex80 analog reader.

Requirements:
    pip install hidapi

What it does:
    - finds the Hex80 analog HID interface
    - sends a safe recovery packet to leave calibration mode
    - reads travel_max
    - polls the analog matrix in chunks
    - prints live key travel values as normalized depths

This example is intentionally generic and does not depend on any overlay,
game integration, or project-specific code.
"""

from __future__ import annotations

import time

import hid


VID = 0x373B
PIDS = {0x1176, 0x1177, 0x1250}
USAGE_PAGE = 0xFF60
USAGE = 0x61

GET_VALUE = 0x02
SET_VALUE = 0x03
CUSTOM_ID = 0x96

SUB_CALIBRATION_START = 0x18
SUB_CALIBRATION_FINISH = 0x19
SUB_TRAVEL_BUFFER = 0x1C
SUB_TRAVEL_INFO = 0x24

PAYLOAD_SIZE = 128
TOTAL_SLOTS = 104
CHUNK_SIZE = 4
RAW_DEADZONE = 8
DEFAULT_TRAVEL_MAX = 3300

INDEX_TO_KEY = [
    # row 0
    "Escape", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "Print Screen", "Scroll Lock", "Pause", None,
    # row 1
    "Backquote", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Minus", "Equals", "Backspace", "Insert", None, None,
    # row 2
    "Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "Left Bracket", "Right Bracket", "Backslash", "Page Up", None, None,
    # row 3
    "Caps Lock", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Semicolon", "Quote", None, "Enter", "Page Down", None, None,
    # row 4
    "Left Shift", "Z", "X", "C", "V", "B", "N", "M", "Comma", "Period", "Slash", None, "Right Shift", None, "Arrow Up", None, None,
    # row 5
    "Left Ctrl", "Left Meta", "Left Alt", None, None, "Space", None, None, None, "Right Alt", "Fn", "Context Menu", None, "Arrow Left", "Arrow Down", "Arrow Right", None,
    # trailing slots
    None, None,
]


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(value, hi))


def make_packet(prefix: list[int]) -> bytes:
    payload = bytearray(PAYLOAD_SIZE)
    payload[: len(prefix)] = bytes(prefix)
    return b"\x00" + bytes(payload)


def build_buffer_request(offset: int, size: int) -> bytes:
    payload = bytearray(PAYLOAD_SIZE)
    payload[0] = GET_VALUE
    payload[1] = CUSTOM_ID
    payload[2] = SUB_TRAVEL_BUFFER
    payload[5] = (offset >> 8) & 0xFF
    payload[6] = offset & 0xFF
    payload[7] = size & 0xFF
    return b"\x00" + bytes(payload)


def normalize_depth(raw: int, travel_max: int) -> float:
    if raw <= RAW_DEADZONE or travel_max <= RAW_DEADZONE:
        return 0.0
    return round(clamp((raw - RAW_DEADZONE) / float(travel_max - RAW_DEADZONE), 0.0, 1.0), 3)


def find_hex80_path() -> bytes:
    for dev in hid.enumerate():
        if dev["vendor_id"] != VID:
            continue
        if dev["product_id"] not in PIDS:
            continue
        if dev.get("usage_page") != USAGE_PAGE:
            continue
        if dev.get("usage") != USAGE:
            continue
        return dev["path"]
    raise RuntimeError("Hex80 analog interface not found")


def request(device: hid.device, packet: bytes, expected_subcommand: int | None, timeout_ms: int) -> list[int]:
    device.write(packet)
    deadline = time.monotonic() + max(0.001, timeout_ms / 1000.0)
    while time.monotonic() < deadline:
        remaining_ms = max(1, int((deadline - time.monotonic()) * 1000))
        data = device.read(128, timeout_ms=remaining_ms)
        if not data:
            return []
        if expected_subcommand is None:
            return data
        if len(data) >= 3 and data[0] in (GET_VALUE, SET_VALUE):
            if data[1] == CUSTOM_ID and data[2] == expected_subcommand:
                return data
    return []


def read_travel_max(device: hid.device) -> int:
    response = request(device, make_packet([GET_VALUE, CUSTOM_ID, SUB_TRAVEL_INFO]), SUB_TRAVEL_INFO, 80)
    if len(response) >= 5:
        value = (response[3] << 8) | response[4]
        if value > 0:
            return value
    return DEFAULT_TRAVEL_MAX


def decode_chunk(data: list[int], travel_max: int) -> dict[str, dict[str, float | int]]:
    active: dict[str, dict[str, float | int]] = {}

    if len(data) < 8:
        return active
    if data[0] != GET_VALUE or data[1] != CUSTOM_ID or data[2] != SUB_TRAVEL_BUFFER:
        return active

    offset = (data[5] << 8) | data[6]
    size = data[7]
    cursor = 8

    for slot in range(size):
        if cursor + 5 > len(data):
            break

        matrix_index = offset + slot
        key_name = INDEX_TO_KEY[matrix_index] if matrix_index < len(INDEX_TO_KEY) else None
        travel = (data[cursor + 2] << 8) | data[cursor + 3]
        depth = normalize_depth(travel, travel_max)

        if key_name and depth > 0.0:
            active[key_name] = {
                "index": matrix_index,
                "travel": travel,
                "depth": depth,
            }

        cursor += 5

    return active


def main() -> None:
    path = find_hex80_path()
    device = hid.device()
    device.open_path(path)
    device.set_nonblocking(False)

    try:
        # Safe recovery in case the board was previously left in calibration mode.
        request(device, make_packet([SET_VALUE, CUSTOM_ID, SUB_CALIBRATION_FINISH]), SUB_CALIBRATION_FINISH, 80)

        travel_max = read_travel_max(device)
        print(f"travel_max={travel_max}")
        print("Polling Hex80 analog data. Press Ctrl+C to stop.")

        while True:
            active_now: dict[str, dict[str, float | int]] = {}

            for offset in range(0, TOTAL_SLOTS, CHUNK_SIZE):
                size = min(CHUNK_SIZE, TOTAL_SLOTS - offset)
                response = request(device, build_buffer_request(offset, size), SUB_TRAVEL_BUFFER, 10)
                if not response:
                    continue
                active_now.update(decode_chunk(response, travel_max))

            if active_now:
                items = []
                for key_name in sorted(active_now):
                    info = active_now[key_name]
                    items.append(f"{key_name}: depth={info['depth']:.3f} raw={info['travel']} index={info['index']}")
                print(" | ".join(items))

            time.sleep(0.005)

    finally:
        device.close()


if __name__ == "__main__":
    main()
