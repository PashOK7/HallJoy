#pragma once
#include <cstdint>
#include <string_view>
namespace halljoy::layout_identity {
struct Entry { std::string_view protocol, product; std::uint64_t token; const wchar_t* preset; };
inline constexpr Entry entries[] = {
    {"aula-w669", "SI2825HEARGB", 0x4322889C453BB371ull, L"Aula WIN 60 HE Standard ANSI"},
    {"aula-w669", "SI2825KRT12HEARGB", 0x4322889C453BB371ull, L"Aula WIN 60 HE Standard ANSI"},
    {"aula-w669", "SI2825KR-AHEARGB", 0x4322889C453BB371ull, L"Aula WIN 60 HE Standard ANSI"},
    {"aula-w669", "SI2825KZHEARGB", 0x4322889C453BB371ull, L"Aula WIN 60 HE Standard ANSI"},
    {"aula-w669", "SI2828HEARGB", 0x70ED6CA4FED0C9F6ull, L"Aula WIN 68 HE Standard ANSI"},
    {"aula-w669", "SI2828KZHEARGB", 0x70ED6CA4FED0C9F6ull, L"Aula WIN 68 HE Standard ANSI"},
    {"aula-rm6x21", "0A021902", 0x17CDB2AE65D3A375ull, L"Aula WIN 60 HE MAX ANSI"},
    {"aula-w669", "7272USHEXYXK673JCARGB", 0xF3B7ECFB2D628746ull, L"Redragon K673WB-RGB-M ANSI"},
    {"aula-w669", "7272UKHEXYXBJCARGB", 0xCE8DCE2AD47DDF55ull, L"Redragon K673RGB-M ISO"},
    {"hex80", "HEX80-ANSI", 0xC93945C2B6C8C51Dull, L"ATK Hex80 ANSI"},
    {"ipi-addressed", "110000000040", 0x798EDA38A47F0E95ull, L"IPI Aurora75 PRO ANSI"},
    {"ipi-addressed", "110000000006", 0x36A3739BD6AEEEF8ull, L"IPI flash68 ANSI"},
    {"ipi-addressed", "110000000023", 0xBEE2B020B2BFBA27ull, L"IPI QBZ65 + AURORA65 + AURORA65W + RAIN65 ANSI"},
    {"ipi-addressed", "110000000010", 0xBEE2B020B2BFBA27ull, L"IPI QBZ65 + AURORA65 + AURORA65W + RAIN65 ANSI"},
    {"ipi-addressed", "120000000003", 0xBEE2B020B2BFBA27ull, L"IPI QBZ65 + AURORA65 + AURORA65W + RAIN65 ANSI"},
    {"ipi-addressed", "11000000001F", 0xBEE2B020B2BFBA27ull, L"IPI QBZ65 + AURORA65 + AURORA65W + RAIN65 ANSI"},
    {"ipi-addressed", "11000000002C", 0x0FEFA7117D763FE5ull, L"IPI QBZ75 + Aurora 75 ANSI"},
    {"ipi-addressed", "110000000013", 0x0FEFA7117D763FE5ull, L"IPI QBZ75 + Aurora 75 ANSI"},
};
inline constexpr std::uint64_t Token(std::string_view protocol, std::string_view product) noexcept {
    for (const auto& e : entries) if (e.protocol==protocol && e.product==product) return e.token;
    return 0;
}
inline constexpr const wchar_t* Match(std::uint64_t token) noexcept {
    for (const auto& e : entries) if (e.token==token) return e.preset;
    return nullptr;
}
}
