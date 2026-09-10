#pragma once

#include <cstdint>
#include <type_traits>

namespace halljoy::controller
{
// Protocol-neutral standard gamepad controls. Output adapters decide whether
// South/East/West/North become Xbox A/B/X/Y, DualShock Cross/Circle/Square/
// Triangle, or another transport's usages.
enum class ButtonV1 : std::uint8_t
{
    South = 0,
    East,
    West,
    North,
    LeftShoulder,
    RightShoulder,
    Select,
    Start,
    Home,
    LeftStick,
    RightStick,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    Count,
};

constexpr std::uint64_t ButtonMask(ButtonV1 button) noexcept
{
    return std::uint64_t{ 1 } << static_cast<std::uint8_t>(button);
}

// This is HallJoy's mapping result, not a ViGEm or driver ABI. V1 deliberately
// contains only the standard controls already configurable in production.
// Future touch, motion and extended-axis capabilities can be added in a new
// version without changing provider or mapping identities.
struct VirtualControllerFrameV1 final
{
    std::uint64_t buttons = 0;
    std::uint8_t leftTrigger = 0;
    std::uint8_t rightTrigger = 0;
    std::int16_t leftStickX = 0;
    std::int16_t leftStickY = 0;
    std::int16_t rightStickX = 0;
    std::int16_t rightStickY = 0;
};

constexpr bool FramesEqual(const VirtualControllerFrameV1& left,
    const VirtualControllerFrameV1& right) noexcept
{
    return left.buttons == right.buttons &&
        left.leftTrigger == right.leftTrigger &&
        left.rightTrigger == right.rightTrigger &&
        left.leftStickX == right.leftStickX &&
        left.leftStickY == right.leftStickY &&
        left.rightStickX == right.rightStickX &&
        left.rightStickY == right.rightStickY;
}

static_assert(std::is_trivially_copyable_v<VirtualControllerFrameV1>);
static_assert(static_cast<std::uint8_t>(ButtonV1::Count) == 15u);
}
