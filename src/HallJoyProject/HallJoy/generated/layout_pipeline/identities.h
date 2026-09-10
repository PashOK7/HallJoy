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
