#include "../HallJoy/native_hid_interface_claim_registry.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    enum class Protocol : std::uint8_t
    {
        Mad68 = 1,
        Hex80 = 2,
        Spark = 3,
    };
}

int main()
{
    using halljoy::native_hid::InterfaceClaimRegistry;
    using halljoy::native_hid::MakeInterfaceClaimToken;
    using halljoy::native_hid::MakeInterfaceClaimTokenUtf8;
    using halljoy::native_hid::TokenListContains;

    const std::wstring interface0 =
        LR"(\\?\HID#VID_1CA6&PID_0529&MI_00#ABC#{4D1E55B2-F16F-11CF-88CB-001111000030})";
    const std::wstring interface1 =
        LR"(\\?\hid#vid_1ca6&pid_0529&mi_01#abc#{4d1e55b2-f16f-11cf-88cb-001111000030})";
    const std::wstring interface0_variant =
        LR"(//?/hid#vid_1ca6&pid_0529&mi_00#abc#{4d1e55b2-f16f-11cf-88cb-001111000030})";

    const std::string token0 = MakeInterfaceClaimToken(interface0);
    const std::string token1 = MakeInterfaceClaimToken(interface1);
    assert(token0 == "path_b8d4f30578af404c_0000004a");
    assert(token1 == "path_6d38c6d7b7d70fdd_0000004a");
    assert(MakeInterfaceClaimToken(interface0_variant) == token0);
    assert(MakeInterfaceClaimTokenUtf8(
        R"(\\?\hid#vid_1ca6&pid_0529&mi_00#abc#{4d1e55b2-f16f-11cf-88cb-001111000030})") == token0);
    assert(token0 != token1);
    assert(token0.size() == 30);

    const std::wstring unicode_path = L"\\\\?\\hid#serial_\u00e9_\U0001f680";
    const std::string unicode_path_utf8 =
        "\\\\?\\hid#serial_\xc3\xa9_" "\xf0\x9f\x9a\x80";
    assert(MakeInterfaceClaimToken(unicode_path) ==
        MakeInterfaceClaimTokenUtf8(unicode_path_utf8));
    const std::wstring replacement(1, static_cast<wchar_t>(0xfffd));
    assert(MakeInterfaceClaimTokenUtf8(std::string(1, static_cast<char>(0xff))) ==
        MakeInterfaceClaimToken(replacement));
    assert(MakeInterfaceClaimTokenUtf8("\xc0\xaf") ==
        MakeInterfaceClaimToken(replacement + replacement));

    InterfaceClaimRegistry<Protocol> registry;
    assert(registry.Claim(0x1ca6, 0x0529, interface0, Protocol::Spark));
    assert(registry.IsClaimed(interface0_variant));
    assert(registry.IsClaimedBy(interface0, Protocol::Spark));
    assert(!registry.IsClaimed(interface1));
    assert(!registry.IsClaimedBy(interface1, Protocol::Spark));
    assert(registry.Claim(0x1ca6, 0x0529, interface1, Protocol::Hex80));
    assert(registry.IsClaimedBy(interface1, Protocol::Hex80));
    assert(!registry.Claim(0x1ca6, 0x0529, interface1, Protocol::Mad68));
    assert(registry.Claim(0x1ca6, 0x0529, interface1, Protocol::Hex80));
    assert(registry.ProtocolHasClaims(Protocol::Spark));
    assert(registry.ProtocolHasClaims(Protocol::Hex80));
    assert(!registry.ProtocolHasClaims(Protocol::Mad68));
    assert(registry.Claims().size() == 2);
    assert(!registry.Claim(0, 0x0529, interface0, Protocol::Spark));
    assert(!registry.Claim(0x1ca6, 0, interface0, Protocol::Spark));
    assert(!registry.Claim(0x1ca6, 0x0529, L"", Protocol::Spark));

    const std::string list = token1 + ";" + token0;
    assert(TokenListContains(list, token0));
    assert(TokenListContains(list, token1));
    assert(TokenListContains("PATH_B8D4F30578AF404C_0000004A", token0));
    assert(!TokenListContains(list, token0.substr(0, token0.size() - 1)));
    assert(!TokenListContains(token0 + "0", token0));
    assert(!TokenListContains(";garbage;;", token0));
    assert(!TokenListContains(list, ""));

    // Identical VID/PID composite interfaces must remain independently owned
    // through enumeration reorders and reconnect generations.
    std::vector<std::wstring> interfaces;
    for (std::uint32_t device = 0; device < 32; ++device)
    {
        for (std::uint32_t iface = 0; iface < 8; ++iface)
        {
            interfaces.push_back(LR"(\\?\hid#vid_1ca6&pid_0529&mi_)" +
                std::to_wstring(iface) + L"#device_" + std::to_wstring(device));
        }
    }
    std::mt19937 random(0x1411d);
    std::uint64_t reorder_generations = 0;
    constexpr std::size_t claims_per_generation = 32;
    for (std::uint32_t generation = 0; generation < 10000; ++generation)
    {
        std::shuffle(interfaces.begin(), interfaces.end(), random);
        InterfaceClaimRegistry<Protocol> generation_registry;
        for (std::size_t i = 0; i < claims_per_generation; ++i)
        {
            const Protocol owner = (i % 2) == 0 ? Protocol::Spark : Protocol::Hex80;
            assert(generation_registry.Claim(0x1ca6, 0x0529, interfaces[i], owner));
            assert(generation_registry.IsClaimedBy(interfaces[i], owner));
        }
        assert(generation_registry.Claims().size() == claims_per_generation);
        ++reorder_generations;
    }

    std::unordered_set<std::string> collision_guard;
    constexpr std::uint32_t synthetic_paths = 300000;
    collision_guard.reserve(synthetic_paths);
    for (std::uint32_t index = 0; index < synthetic_paths; ++index)
    {
        const std::wstring path = LR"(\\?\hid#vid_373b&pid_1109&mi_01#synthetic_)" +
            std::to_wstring(index) + L"#{4d1e55b2-f16f-11cf-88cb-001111000030}";
        assert(collision_guard.emplace(MakeInterfaceClaimToken(path)).second);
    }

    registry.Reset();
    assert(registry.Claims().empty());
    assert(!registry.IsClaimed(interface0));

    std::cout << "NATIVE_HID_INTERFACE_CLAIM_TEST=PASS composite_interfaces="
              << interfaces.size() << " reorder_generations=" << reorder_generations
              << " claims_per_generation=" << claims_per_generation
              << " synthetic_paths=" << synthetic_paths
              << " exact_token_matching=1\n";
    return 0;
}
