#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_device_identity.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    using halljoy::uap::DeviceIdentity;
    using halljoy::uap::DeviceIdentityInput;

    DeviceIdentityInput Input(std::string_view path)
    {
        return DeviceIdentityInput{
            0x373b, 0x1058, 0xff60, 0x61, path, "Madlions", "MAD68 HE"
        };
    }

    std::uint64_t NextRandom(std::uint64_t& state)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
}

int main()
{
    using halljoy::uap::MakeDeviceIdentity;
    using halljoy::uap::MakeDeviceIdentityBase;

    const auto first = MakeDeviceIdentity(Input(R"(\\?\HID#VID_373B&PID_1058#A)"), 0);
    const auto same = MakeDeviceIdentity(Input(R"(\\?\hid#vid_373b&pid_1058#a)"), 999);
    const auto slash_variant = MakeDeviceIdentity(Input("//?/HID#VID_373B&PID_1058#A"), 17);
    assert(first.id != 0 && first.duplicate_safe);
    assert(first.id == 0x10411b4549e44638ull); // v2 persisted-ID golden vector
    assert(first.id == same.id);
    assert(first.id == slash_variant.id);

    const auto second = MakeDeviceIdentity(Input(R"(\\?\HID#VID_373B&PID_1058#B)"), 0);
    assert(second.id == 0x24ccda9629c08b41ull); // distinct identical-device path
    assert(second.id != first.id && second.duplicate_safe);

    auto changed_descriptor = Input(R"(\\?\HID#VID_373B&PID_1058#A)");
    changed_descriptor.usage = 0x62;
    assert(MakeDeviceIdentity(changed_descriptor, 0).id != first.id);

    auto split_a = Input({});
    split_a.manufacturer = "ab";
    split_a.device_name = "c";
    auto split_b = Input({});
    split_b.manufacturer = "a";
    split_b.device_name = "bc";
    assert(MakeDeviceIdentityBase(split_a) != MakeDeviceIdentityBase(split_b));

    std::unordered_set<std::uint64_t> fallback_ids;
    const auto fallback_input = Input({});
    assert(MakeDeviceIdentity(fallback_input, 0).id == 0x0842e891bc305f4cull);
    assert(MakeDeviceIdentity(fallback_input, 1).id == 0x5a6fc9c7cecb690eull);
    for (std::uint32_t occurrence = 0; occurrence != 1024; ++occurrence)
    {
        const DeviceIdentity identity = MakeDeviceIdentity(fallback_input, occurrence);
        assert(identity.id != 0 && !identity.duplicate_safe);
        assert(fallback_ids.insert(identity.id).second);
    }

    constexpr std::size_t identical_device_count = 8;
    std::array<std::string, identical_device_count> paths{};
    std::array<std::uint64_t, identical_device_count> expected{};
    for (std::size_t index = 0; index != identical_device_count; ++index)
    {
        paths[index] = R"(\\?\HID#VID_373B&PID_1058#INSTANCE_)" + std::to_string(index);
        expected[index] = MakeDeviceIdentity(Input(paths[index]), 0).id;
    }

    std::array<unsigned, identical_device_count> order{};
    std::iota(order.begin(), order.end(), 0u);
    std::uint64_t permutations = 0;
    do
    {
        std::unordered_map<std::uint64_t, std::uint32_t> occurrences;
        std::unordered_set<std::uint64_t> generation_ids;
        for (const unsigned index : order)
        {
            const auto input = Input(paths[index]);
            const auto base = MakeDeviceIdentityBase(input);
            const auto identity = MakeDeviceIdentity(input, occurrences[base]++);
            assert(identity.id == expected[index]);
            assert(identity.duplicate_safe);
            assert(generation_ids.insert(identity.id).second);
        }
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    assert(permutations == 40320);

    // Reconnect simulation: arbitrary subsets and enumeration orders preserve
    // each physical path's ID across 100,000 fresh discovery generations.
    std::uint64_t random_state = 0x6a09e667f3bcc909ull;
    for (std::uint32_t generation = 0; generation != 100000; ++generation)
    {
        std::shuffle(order.begin(), order.end(), std::mt19937_64(NextRandom(random_state)));
        const std::uint64_t present_mask = NextRandom(random_state) | 1u;
        for (const unsigned index : order)
        {
            if ((present_mask & (1ull << index)) == 0)
                continue;
            assert(MakeDeviceIdentity(Input(paths[index]), generation).id == expected[index]);
        }
    }

    // Collision smoke over a large deterministic population of otherwise
    // identical fake devices. This is not a mathematical collision proof, but
    // catches truncation, missing-field and weak-normalization regressions.
    std::unordered_set<std::uint64_t> generated_ids;
    generated_ids.reserve(250000);
    for (std::uint32_t index = 0; index != 250000; ++index)
    {
        const std::string path = R"(\\?\HID#VID_373B&PID_1058#STRESS_)" + std::to_string(index);
        const auto identity = MakeDeviceIdentity(Input(path), index);
        assert(identity.duplicate_safe && identity.id != 0);
        assert(generated_ids.insert(identity.id).second);
    }

    std::cout << "UAP_DEVICE_IDENTITY_TEST=PASS permutations=" << permutations
              << " reconnect_generations=100000 stress_devices=" << generated_ids.size()
              << " fallback_occurrences=" << fallback_ids.size() << '\n';
    return 0;
}
