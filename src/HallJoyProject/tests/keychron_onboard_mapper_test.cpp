#include "../HallJoy/keychron_onboard_mapper.h"
#include "../HallJoy/configured_xusb_builder.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <random>

int main() {
    namespace ref = halljoy::configured_xusb;
    std::mt19937 random(0x484a4b34);
    hjo_mapping p{};
    p.sensitivity = .15f;
    ref::PadConfiguration config{};
    config.lastKeyPrioritySensitivity = p.sensitivity;
    for (unsigned a = 0; a < 4; ++a) {
        p.axes[a][0] = static_cast<uint8_t>(2 * a);
        p.axes[a][1] = static_cast<uint8_t>(2 * a + 1);
        config.axes[a] = {static_cast<uint16_t>(2 * a + 1), static_cast<uint16_t>(2 * a + 2)};
    }
    p.triggers[0] = 8; p.triggers[1] = 9;
    config.triggers = {9, 10};
    for (unsigned k = 0; k < HJO_SLOTS; ++k) {
        p.buttons[k] = static_cast<uint16_t>(random() & 0x7fffu);
        for (unsigned b = 0; b < 15; ++b)
            if (p.buttons[k] & (1u << b)) config.buttonMasks[b][(k + 1) / 64] |= uint64_t{1} << ((k + 1) % 64);
    }
    assert(hjo_mapping_valid(&p));
    for (unsigned flags = 0; flags < 4; ++flags) {
        p.flags = static_cast<uint8_t>(flags);
        config.snappyJoystick = flags & HJO_SNAP;
        config.lastKeyPriority = flags & HJO_LKP;
        hjo_mapper_state state{};
        ref::BuilderState referenceState{};
        for (unsigned tick = 0; tick < 10000; ++tick) {
            std::array<float, HJO_SLOTS> values{};
            ref::InputValues input{};
            constexpr float edges[] = {0, .0001f, .09999f, .1f, .10001f, .5f, 1, -1, 2};
            for (unsigned k = 0; k < HJO_SLOTS; ++k) {
                float v = tick % 2 ? static_cast<float>(random() % 65536) / 65535.0f : edges[random() % 9];
                values[k] = v; input.filtered[k + 1] = v;
            }
            hjo_pad result{};
            hjo_map(&p, values.data(), &state, &result);
            const auto expected = ref::BuildReport(config, input, referenceState);
            assert(result.axes[0] == expected.leftStickX && result.axes[1] == expected.leftStickY);
            assert(result.axes[2] == expected.rightStickX && result.axes[3] == expected.rightStickY);
            assert(result.triggers[0] == expected.leftTrigger && result.triggers[1] == expected.rightTrigger);
            assert(result.buttons == expected.buttons);
            for (unsigned a = 0; a < 4; ++a) {
                assert(state.direction[a] == referenceState.lastDirection[a]);
                assert(state.previous_minus[a] == referenceState.previousMinusDown[a]);
                assert(state.previous_plus[a] == referenceState.previousPlusDown[a]);
                assert(state.minus_valley[a] == referenceState.minusValley[a]);
                assert(state.plus_valley[a] == referenceState.plusValley[a]);
            }
        }
    }
    p.axes[0][0] = 114; assert(!hjo_mapping_valid(&p));
    p.axes[0][0] = HJO_UNBOUND; assert(hjo_mapping_valid(&p));
    p.sensitivity = std::numeric_limits<float>::quiet_NaN(); assert(!hjo_mapping_valid(&p));
    assert(hjo_xinput_buttons(1) == 0x1000);
    assert(hjo_xinput_buttons(0x7fff) == 0xf7ff);
    std::cout << "40000 onboard/production report and state comparisons PASS\n";
}
