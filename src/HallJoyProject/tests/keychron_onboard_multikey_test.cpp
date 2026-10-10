#include "../HallJoy/keychron_onboard_mapper.h"
#include "../HallJoy/keychron_onboard_profile.h"
#include "../HallJoy/configured_xusb_builder.h"
#include <array>
#include <cassert>
#include <cstring>
#include <random>

// Several keys per stick direction: the C firmware mapper must match the host
// builder exactly, including the most-pressed rule and the release fallback.
int main() {
    namespace ref = halljoy::configured_xusb;
    std::mt19937 random(0x4d4b3931);
    constexpr float edges[] = {0, .0001f, .09999f, .1f, .10001f, .25f, .5f, .75f, 1, -1, 2};

    for (unsigned round = 0; round < 12; ++round) {
        hjo_mapping p{};
        memset(p.axes, HJO_UNBOUND, sizeof(p.axes));
        p.sensitivity = .15f;
        ref::PadConfiguration config{};
        config.lastKeyPrioritySensitivity = p.sensitivity;
        // Random distinct slots per direction; the HID of slot s is s + 1.
        for (unsigned a = 0; a < 4; ++a)
            for (unsigned side = 0; side < 2; ++side) {
                const unsigned count = 1 + random() % HJO_AXIS_KEYS;
                for (unsigned k = 0; k < count; ++k) {
                    uint8_t slot;
                    bool duplicate;
                    do {
                        slot = static_cast<uint8_t>(random() % HJO_SLOTS);
                        duplicate = false;
                        for (unsigned j = 0; j < k; ++j) duplicate |= p.axes[a][side][j] == slot;
                    } while (duplicate);
                    p.axes[a][side][k] = slot;
                    auto& hids = side == 0 ? config.axes[a].minusHids : config.axes[a].plusHids;
                    hids[k] = static_cast<std::uint16_t>(slot + 1);
                }
            }
        assert(hjo_mapping_valid(&p));

        for (unsigned flags = 0; flags < 4; ++flags) {
            p.flags = static_cast<uint8_t>(flags);
            config.snappyJoystick = flags & HJO_SNAP;
            config.lastKeyPriority = flags & HJO_LKP;
            hjo_mapper_state state{};
            ref::BuilderState referenceState{};
            for (unsigned tick = 0; tick < 4000; ++tick) {
                std::array<float, HJO_SLOTS> values{};
                ref::InputValues input{};
                for (unsigned k = 0; k < HJO_SLOTS; ++k) {
                    const float v = tick % 2 ? static_cast<float>(random() % 65536) / 65535.0f
                                             : edges[random() % (sizeof(edges) / sizeof(edges[0]))];
                    values[k] = v;
                    input.filtered[k + 1] = v;
                }
                hjo_pad result{};
                hjo_map(&p, values.data(), &state, &result);
                const auto expected = ref::BuildReport(config, input, referenceState);
                assert(result.axes[0] == expected.leftStickX && result.axes[1] == expected.leftStickY);
                assert(result.axes[2] == expected.rightStickX && result.axes[3] == expected.rightStickY);
                assert(result.buttons == expected.buttons);
                for (unsigned a = 0; a < 4; ++a) {
                    assert(state.direction[a] == referenceState.lastDirection[a]);
                    assert(state.minus_valley[a] == referenceState.minusValley[a]);
                    assert(state.plus_valley[a] == referenceState.plusValley[a]);
                }
            }
        }
    }

    // Two keys with the same value on one side do not add up.
    {
        hjo_mapping p{};
        memset(p.axes, HJO_UNBOUND, sizeof(p.axes));
        p.sensitivity = .15f;
        p.axes[0][0][0] = 4; p.axes[0][0][1] = 5;
        std::array<float, HJO_SLOTS> values{};
        values[4] = .5f; values[5] = .5f;
        hjo_mapper_state state{};
        hjo_pad out{};
        hjo_map(&p, values.data(), &state, &out);
        assert(out.axes[0] == -16384 || out.axes[0] == -16383);
    }

    // Validation: a duplicate key or an out-of-range key is refused.
    {
        hjo_mapping p{};
        memset(p.axes, HJO_UNBOUND, sizeof(p.axes));
        p.sensitivity = .15f;
        p.axes[1][1][0] = 9; p.axes[1][1][3] = 9;
        assert(!hjo_mapping_valid(&p));
        p.axes[1][1][3] = HJO_UNBOUND;
        assert(hjo_mapping_valid(&p));
        p.axes[1][1][3] = HJO_SLOTS;
        assert(!hjo_mapping_valid(&p));
    }
    return 0;
}
