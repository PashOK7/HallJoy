#include "configured_xusb_builder.h"

#include <algorithm>
#include <cmath>

namespace halljoy::configured_xusb
{
namespace
{
constexpr float kPressedThreshold = 0.10f;

float Read(const InputValues& input, std::uint16_t key) noexcept
{
    if (!keycode::IsSupported(key))
        return 0.0f;
    return input.filtered[key];
}

bool Pressed(float value) noexcept
{
    return std::isfinite(value) && value >= kPressedThreshold;
}

std::int16_t Stick(float value) noexcept
{
    if (!std::isfinite(value)) value = 0.0f;
    value = std::clamp(value, -1.0f, 1.0f);
    return static_cast<std::int16_t>(std::lround(value * 32767.0f));
}

std::uint8_t Trigger(float value) noexcept
{
    if (!std::isfinite(value)) value = 0.0f;
    value = std::clamp(value, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(std::lround(value * 255.0f));
}

float ResolveAxis(std::size_t axis, float minusValue, float plusValue,
    const PadConfiguration& configuration, BuilderState& state) noexcept
{
    if (!configuration.snappyJoystick && !configuration.lastKeyPriority)
        return plusValue - minusValue;

    const bool minusDown = Pressed(minusValue);
    const bool plusDown = Pressed(plusValue);
    const bool previousMinus = state.previousMinusDown[axis] != 0;
    const bool previousPlus = state.previousPlusDown[axis] != 0;

    if (minusDown && !previousMinus)
        state.lastDirection[axis] = -1;
    if (plusDown && !previousPlus)
        state.lastDirection[axis] = 1;

    if (configuration.lastKeyPriority)
    {
        const float retriggerDelta = std::clamp(
            configuration.lastKeyPrioritySensitivity, 0.02f, 0.95f);
        if (!minusDown)
            state.minusValley[axis] = 1.0f;
        else if (!previousMinus)
            state.minusValley[axis] = minusValue;
        else
        {
            float& valley = state.minusValley[axis];
            valley = std::min(valley, minusValue);
            if (minusValue - valley >= retriggerDelta)
            {
                state.lastDirection[axis] = -1;
                valley = minusValue;
            }
        }

        if (!plusDown)
            state.plusValley[axis] = 1.0f;
        else if (!previousPlus)
            state.plusValley[axis] = plusValue;
        else
        {
            float& valley = state.plusValley[axis];
            valley = std::min(valley, plusValue);
            if (plusValue - valley >= retriggerDelta)
            {
                state.lastDirection[axis] = 1;
                valley = plusValue;
            }
        }
    }

    state.previousMinusDown[axis] = minusDown ? 1u : 0u;
    state.previousPlusDown[axis] = plusDown ? 1u : 0u;

    const float maximum = std::max(minusValue, plusValue);
    if (maximum <= 0.0001f)
        return 0.0f;
    if (configuration.lastKeyPriority)
    {
        if (minusDown && !plusDown)
            return -minusValue;
        if (plusDown && !minusDown)
            return plusValue;
        if (minusDown && plusDown)
        {
            std::int8_t direction = state.lastDirection[axis];
            if (direction == 0)
                direction = plusValue >= minusValue ? 1 : -1;
            const float magnitude = configuration.snappyJoystick
                ? maximum : (direction > 0 ? plusValue : minusValue);
            return direction > 0 ? magnitude : -magnitude;
        }
    }

    if (configuration.snappyJoystick)
    {
        constexpr float kEqualEpsilon = 0.002f;
        const float difference = plusValue - minusValue;
        if (std::fabs(difference) > kEqualEpsilon)
            return difference > 0.0f ? maximum : -maximum;
        if (state.lastDirection[axis] > 0)
            return maximum;
        if (state.lastDirection[axis] < 0)
            return -maximum;
        return 0.0f;
    }
    return plusValue - minusValue;
}

std::int16_t MergeMouse(std::int16_t keyboard, float mouse) noexcept
{
    const std::int16_t mouseAxis = Stick(mouse);
    if (mouseAxis == 0)
        return keyboard;
    return std::abs(static_cast<int>(mouseAxis)) >=
        std::abs(static_cast<int>(keyboard)) ? mouseAxis : keyboard;
}

bool ButtonPressed(const PadConfiguration& configuration,
    const InputValues& input, std::size_t button) noexcept
{
    for (std::size_t chunk = 0; chunk < keycode::kMaskChunkCount; ++chunk)
    {
        std::uint64_t bits = configuration.buttonMasks[button][chunk];
        while (bits != 0)
        {
            std::size_t bit = 0;
            while ((bits & (std::uint64_t{ 1 } << bit)) == 0)
                ++bit;
            bits &= bits - 1u;
            const std::size_t key = chunk * keycode::kMaskChunkBits + bit;
            if (key < input.filtered.size() && Pressed(input.filtered[key]))
                return true;
        }
    }
    return false;
}

}

controller::VirtualControllerFrameV1 BuildReport(
    const PadConfiguration& configuration,
    const InputValues& input,
    BuilderState& state) noexcept
{
    controller::VirtualControllerFrameV1 report{};
    std::array<std::int16_t*, kAxisCount> outputs{
        &report.leftStickX, &report.leftStickY,
        &report.rightStickX, &report.rightStickY
    };
    for (std::size_t axis = 0; axis < kAxisCount; ++axis)
    {
        const AxisBinding& binding = configuration.axes[axis];
        *outputs[axis] = Stick(ResolveAxis(axis, Read(input, binding.minusHid),
            Read(input, binding.plusHid), configuration, state));
    }

    if (input.mouseEnabled)
    {
        if (input.mouseTarget == 0)
        {
            report.leftStickX = MergeMouse(report.leftStickX, input.mouseX);
            report.leftStickY = MergeMouse(report.leftStickY, input.mouseY);
        }
        else
        {
            report.rightStickX = MergeMouse(report.rightStickX, input.mouseX);
            report.rightStickY = MergeMouse(report.rightStickY, input.mouseY);
        }
    }

    report.leftTrigger = Trigger(Read(input, configuration.triggers[0]));
    report.rightTrigger = Trigger(Read(input, configuration.triggers[1]));
    for (std::size_t button = 0; button < kButtonCount; ++button)
    {
        if (ButtonPressed(configuration, input, button))
        {
            report.buttons |= controller::ButtonMask(
                static_cast<controller::ButtonV1>(button));
        }
    }
    return report;
}
}
