#include "configured_xusb_builder.h"
#include "xusb_output_adapter.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
void BindButton(halljoy::configured_xusb::PadConfiguration& configuration,
    std::size_t button, std::uint16_t key)
{
    configuration.buttonMasks[button][key / 64u] |=
        std::uint64_t{ 1 } << (key % 64u);
}
}

int main()
{
    using namespace halljoy::configured_xusb;
    PadConfiguration configuration{};
    configuration.axes[0] = { 4, 7 };
    configuration.axes[1] = { 22, 26 };
    configuration.axes[2] = { 27, 29 };
    configuration.axes[3] = { 6, 25 };
    configuration.triggers = { 8, 9 };
    BindButton(configuration, 0, 10); // A
    BindButton(configuration, 8, halljoy::keycode::kFn); // Guide
    BindButton(configuration, 14, 12); // D-pad right

    InputValues input{};
    BuilderState state{};
    const auto neutral = BuildReport(configuration, input, state);
    assert(neutral.buttons == 0 && neutral.leftTrigger == 0 &&
        neutral.rightTrigger == 0 && neutral.leftStickX == 0 &&
        neutral.leftStickY == 0 && neutral.rightStickX == 0 &&
        neutral.rightStickY == 0);

    input.filtered[4] = std::numeric_limits<float>::quiet_NaN();
    input.filtered[8] = std::numeric_limits<float>::infinity();
    input.mouseEnabled = true;
    input.mouseX = -std::numeric_limits<float>::infinity();
    input.mouseY = std::numeric_limits<float>::quiet_NaN();
    const auto nonFinite = BuildReport(configuration, input, state);
    assert(nonFinite.leftStickX == 0 && nonFinite.leftStickY == 0 &&
        nonFinite.leftTrigger == 0 && nonFinite.buttons == 0);
    input = {};

    input.filtered[4] = 0.25f;
    input.filtered[7] = 0.75f;
    input.filtered[26] = 1.0f;
    input.filtered[27] = 0.5f;
    input.filtered[25] = 0.25f;
    input.filtered[8] = 0.5f;
    input.filtered[9] = 1.0f;
    input.filtered[10] = 0.1f;
    input.filtered[halljoy::keycode::kFn] = 0.7f;
    input.filtered[12] = 0.09f;
    const auto complete = BuildReport(configuration, input, state);
    assert(complete.leftStickX == 16384);
    assert(complete.leftStickY == 32767);
    assert(complete.rightStickX == -16384);
    assert(complete.rightStickY == 8192);
    assert(complete.leftTrigger == 128 && complete.rightTrigger == 255);
    assert(complete.buttons ==
        (halljoy::controller::ButtonMask(halljoy::controller::ButtonV1::South) |
         halljoy::controller::ButtonMask(halljoy::controller::ButtonV1::Home)));
    const auto completeXusb = halljoy::xusb_output::ToReport(complete);
    assert(completeXusb.buttons == (0x1000u | 0x0400u));
    assert(completeXusb.leftTrigger == 128 && completeXusb.rightTrigger == 255);
    assert(completeXusb.thumbLX == 16384 && completeXusb.thumbLY == 32767 &&
        completeXusb.thumbRX == -16384 && completeXusb.thumbRY == 8192);

    input.mouseEnabled = true;
    input.mouseTarget = 1;
    input.mouseX = 0.8f;
    input.mouseY = -0.1f;
    const auto mouse = BuildReport(configuration, input, state);
    assert(mouse.rightStickX == static_cast<std::int16_t>(std::lround(0.8f * 32767.0f)));
    assert(mouse.rightStickY == 8192); // keyboard remains stronger than mouse

    PadConfiguration priority = configuration;
    priority.snappyJoystick = true;
    priority.lastKeyPriority = true;
    priority.lastKeyPrioritySensitivity = 0.2f;
    InputValues paired{};
    BuilderState qualifiedState{};
    BuilderState shadowState{};
    paired.filtered[4] = 0.7f;
    auto qualified = BuildReport(priority, paired, qualifiedState);
    auto shadow = BuildReport(priority, paired, shadowState);
    assert(halljoy::controller::FramesEqual(qualified, shadow) &&
        qualified.leftStickX < 0);
    paired.filtered[7] = 0.7f;
    qualified = BuildReport(priority, paired, qualifiedState);
    shadow = BuildReport(priority, paired, shadowState);
    assert(halljoy::controller::FramesEqual(qualified, shadow) &&
        qualified.leftStickX > 0);
    paired.filtered[7] = 0.3f;
    qualified = BuildReport(priority, paired, qualifiedState);
    shadow = BuildReport(priority, paired, shadowState);
    assert(halljoy::controller::FramesEqual(qualified, shadow));
    paired.filtered[7] = 0.55f; // analog re-trigger without a digital edge
    qualified = BuildReport(priority, paired, qualifiedState);
    shadow = BuildReport(priority, paired, shadowState);
    assert(halljoy::controller::FramesEqual(qualified, shadow) &&
        qualified.leftStickX > 0);
    assert(halljoy::xusb_output::ReportsEqual(
        halljoy::xusb_output::ToReport(qualified),
        halljoy::xusb_output::ToReport(shadow)));

    InputValues divergent = paired;
    divergent.filtered[8] = 1.0f;
    BuilderState divergentState = shadowState;
    const auto changed = BuildReport(priority, divergent, divergentState);
    assert(changed.leftTrigger == 255);
    assert(changed.buttons == shadow.buttons &&
        changed.leftStickX == shadow.leftStickX &&
        changed.leftStickY == shadow.leftStickY &&
        changed.rightStickX == shadow.rightStickX &&
        changed.rightStickY == shadow.rightStickY);

    // Replay fixture: a generation/profile transition starts from explicit
    // neutral BuilderState and therefore cannot inherit last direction.
    InputValues released{};
    const auto releasedFrame = BuildReport(priority, released, qualifiedState);
    assert(releasedFrame.leftStickX == 0);
    PadConfiguration replacement = priority;
    replacement.axes[0] = { 7, 4 };
    BuilderState replacementState{};
    const auto replacementFrame = BuildReport(replacement, paired, replacementState);
    assert(replacementFrame.leftStickX > 0);

    std::cout << "CONFIGURED_XUSB_BUILDER_TEST=PASS all_fields=1 "
        "explicit_state=1 paired_equivalence=1 divergence_localized=1 "
        "neutral_frame=1 nonfinite_neutral=1 exact_xusb_adapter=1 replay_reset=1\n";
    return 0;
}
