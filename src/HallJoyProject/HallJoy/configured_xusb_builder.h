#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "analog_key_codes.h"
#include "bindings.h"
#include "virtual_controller_frame.h"

namespace halljoy::configured_xusb
{
constexpr std::size_t kAxisCount = 4u;
constexpr std::size_t kTriggerCount = 2u;
constexpr std::size_t kButtonCount = 15u;

// One immutable per-tick configuration view. The realtime owner captures this
// once and gives the identical value to the qualified and shadow builders.
struct PadConfiguration final
{
    std::array<AxisBinding, kAxisCount> axes{};
    std::array<std::uint16_t, kTriggerCount> triggers{};
    std::array<std::array<std::uint64_t, keycode::kMaskChunkCount>,
        kButtonCount> buttonMasks{};
    bool snappyJoystick = false;
    bool lastKeyPriority = false;
    float lastKeyPrioritySensitivity = 0.02f;
};

// Values are already filtered through the one production curve snapshot. This
// component performs no provider read, settings read, allocation, wait or I/O.
struct InputValues final
{
    std::array<float, keycode::kCount> filtered{};
    bool mouseEnabled = false;
    std::uint8_t mouseTarget = 0; // 0 = left stick, 1 = right stick
    float mouseX = 0.0f;
    float mouseY = 0.0f;
};

// Stateful SOCD/last-key behavior is explicit. Qualified and shadow paths must
// own different instances so evaluating one can never perturb the other.
struct BuilderState final
{
    std::array<std::uint8_t, kAxisCount> previousMinusDown{};
    std::array<std::uint8_t, kAxisCount> previousPlusDown{};
    std::array<std::int8_t, kAxisCount> lastDirection{};
    std::array<float, kAxisCount> minusValley{};
    std::array<float, kAxisCount> plusValley{};
};

controller::VirtualControllerFrameV1 BuildReport(
    const PadConfiguration& configuration,
    const InputValues& input,
    BuilderState& state) noexcept;
}
