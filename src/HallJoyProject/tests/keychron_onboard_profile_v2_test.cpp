#include "../HallJoy/keychron_onboard_profile.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

// HJP2 (several keys per stick direction) next to HJP1 (one key), both decodable.
int main() {
    static_assert(HJO_PROFILE_BYTES == 5044 && HJO_PROFILE_BYTES_V2 == 5100);
    assert(hjo_axis_extra_offset(3, 1, 7) == HJO_AXIS_EXTRA_OFFSET + 55);

    hjo_profile source{};
    source.mapping.sensitivity = .15f;
    memset(source.mapping.axes, HJO_UNBOUND, sizeof(source.mapping.axes));
    memset(source.mapping.triggers, HJO_UNBOUND, sizeof(source.mapping.triggers));
    source.mapping.axes[0][0][0] = 0;
    source.mapping.axes[0][0][1] = 40;   // second key, same direction
    source.mapping.axes[3][1][0] = 113;
    source.mapping.axes[3][1][1] = 50;
    source.mapping.axes[3][1][2] = 7;
    source.mapping.buttons[113] = 0x4001;
    for (auto& c : source.curves) {
        c.x[0] = 0; c.x[1] = .3f; c.x[2] = .7f; c.x[3] = 1;
        memcpy(c.y, c.x, sizeof(c.x)); c.weight[0] = c.weight[1] = 1;
    }

    // Multi-key profile: HJP2, 5100 bytes, CRC at the end.
    std::array<uint8_t, HJO_PROFILE_BYTES_MAX> wire{};
    const unsigned size = hjo_profile_encode(wire.data(), wire.size(), &source);
    assert(size == HJO_PROFILE_BYTES_V2);
    assert(!memcmp(wire.data(), "HJP2", 4));
    hjo_profile target{};
    assert(hjo_profile_decode(&target, wire.data(), size));
    assert(target.mapping.axes[0][0][1] == 40 && target.mapping.axes[3][1][2] == 7);
    assert(target.mapping.axes[0][0][2] == HJO_UNBOUND && target.mapping.axes[1][0][0] == HJO_UNBOUND);
    assert(target.mapping.buttons[113] == 0x4001);
    assert(hjo_curve_apply(&target.curves[113], .5f) == hjo_curve_apply(&source.curves[113], .5f));

    // A decoded HJP2 stays HJP2 when re-encoded (same bytes).
    std::array<uint8_t, HJO_PROFILE_BYTES_MAX> again{};
    assert(hjo_profile_encode(again.data(), again.size(), &target) == size);
    assert(!memcmp(again.data(), wire.data(), size));

    // Wrong declared size for the magic is refused.
    assert(!hjo_profile_decode(&target, wire.data(), HJO_PROFILE_BYTES));

    // Corrupt extension byte (CRC not fixed): refused and atomic.
    const hjo_profile before = target;
    auto corrupt = wire;
    corrupt[hjo_axis_extra_offset(0, 0, 1)] ^= 0x01;
    assert(!hjo_profile_decode(&target, corrupt.data(), size));
    assert(!memcmp(&before, &target, sizeof(target)));

    // Extension entry out of range with a fixed CRC: refused.
    corrupt = wire;
    corrupt[hjo_axis_extra_offset(2, 0, 3)] = HJO_SLOTS;
    hjk4_put32(corrupt.data() + size - 4, hjk4_crc32(corrupt.data(), size - 4));
    assert(!hjo_profile_decode(&target, corrupt.data(), size));
    assert(!memcmp(&before, &target, sizeof(target)));

    // Duplicate key in one direction with a fixed CRC: refused.
    corrupt = wire;
    corrupt[hjo_axis_extra_offset(3, 1, 2)] = 113;
    hjk4_put32(corrupt.data() + size - 4, hjk4_crc32(corrupt.data(), size - 4));
    assert(!hjo_profile_decode(&target, corrupt.data(), size));

    // Single-key profile: HJP1, 5044 bytes, accepted by the older layout too.
    hjo_profile single{};
    single.mapping.sensitivity = .15f;
    memset(single.mapping.axes, HJO_UNBOUND, sizeof(single.mapping.axes));
    memset(single.mapping.triggers, HJO_UNBOUND, sizeof(single.mapping.triggers));
    single.mapping.axes[1][0][0] = 4;
    for (auto& c : single.curves) {
        c.x[0] = 0; c.x[1] = .3f; c.x[2] = .7f; c.x[3] = 1;
        memcpy(c.y, c.x, sizeof(c.x)); c.weight[0] = c.weight[1] = 1;
    }
    std::array<uint8_t, HJO_PROFILE_BYTES_MAX> legacy{};
    assert(hjo_profile_encode(legacy.data(), legacy.size(), &single) == HJO_PROFILE_BYTES);
    assert(!memcmp(legacy.data(), "HJP1", 4));
    hjo_profile decoded{};
    assert(hjo_profile_decode(&decoded, legacy.data(), HJO_PROFILE_BYTES));
    assert(decoded.mapping.axes[1][0][0] == 4 && decoded.mapping.axes[1][0][1] == HJO_UNBOUND);

    // Physical-key check (firmware COMMIT): only bound entries are checked.
    {
        static auto physicalBelow100 = [](unsigned slot) { return slot < 100 ? 1 : 0; };
        std::array<uint8_t, HJO_PROFILE_BYTES_MAX> v2{};
        hjo_profile clean{};
        clean.mapping.sensitivity = .15f;
        memset(clean.mapping.axes, HJO_UNBOUND, sizeof(clean.mapping.axes));
        memset(clean.mapping.triggers, HJO_UNBOUND, sizeof(clean.mapping.triggers));
        for (auto& c : clean.curves) {
            c.x[0] = 0; c.x[1] = .3f; c.x[2] = .7f; c.x[3] = 1;
            memcpy(c.y, c.x, sizeof(c.x)); c.weight[0] = c.weight[1] = 1;
        }
        clean.mapping.axes[0][0][0] = 5;
        assert(hjo_profile_encode(v2.data(), v2.size(), &clean) == HJO_PROFILE_BYTES);
        uint8_t slot = 0, kind = 0;
        assert(hjo_profile_first_unphysical(v2.data(), HJO_PROFILE_BYTES, physicalBelow100, &slot, &kind) == 0);

        // Extra stick key on a non-physical slot is reported as kind 2; a CRC byte
        // holding the same number is never read as a key.
        clean.mapping.axes[1][1][0] = 3;
        clean.mapping.axes[1][1][1] = 102;
        assert(hjo_profile_encode(v2.data(), v2.size(), &clean) == HJO_PROFILE_BYTES_V2);
        v2[HJO_PROFILE_BYTES_V2 - 4] = 102;
        assert(hjo_profile_first_unphysical(v2.data(), HJO_PROFILE_BYTES_V2, physicalBelow100, &slot, &kind) == 1);
        assert(slot == 102 && kind == 2);

        // Non-physical button is kind 3; non-physical first stick key is kind 0.
        hjo_profile button = clean;
        button.mapping.axes[1][1][1] = HJO_UNBOUND;
        button.mapping.buttons[113] = 0x0001;
        assert(hjo_profile_encode(v2.data(), v2.size(), &button) == HJO_PROFILE_BYTES);
        assert(hjo_profile_first_unphysical(v2.data(), HJO_PROFILE_BYTES, physicalBelow100, &slot, &kind) == 1);
        assert(slot == 113 && kind == 3);
        hjo_profile stick = clean;
        stick.mapping.axes[1][1][1] = HJO_UNBOUND;
        stick.mapping.axes[2][0][0] = 113;
        assert(hjo_profile_encode(v2.data(), v2.size(), &stick) == HJO_PROFILE_BYTES);
        assert(hjo_profile_first_unphysical(v2.data(), HJO_PROFILE_BYTES, physicalBelow100, &slot, &kind) == 1);
        assert(slot == 113 && kind == 0);
    }

    std::cout << "onboard profile HJP2 multi-key round trip PASS\n";
    return 0;
}
