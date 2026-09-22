#pragma once
#include <array>
#include <cstdint>
// Reads the unique real Windows gamepad with K4 VID/PID. No vendor HID traffic.
// Called only by the dedicated monitor thread, independently of profile/telemetry.
bool KeychronOnboard_ReadOsGamepad(std::array<std::uint8_t,20>& report) noexcept;
