#pragma once
#include <cstdint>
namespace halljoy::aula_rm_family {
inline constexpr std::uint64_t Win60Pro=0x57494E363050524Full;
inline constexpr std::uint64_t Win68=0x57494E363850524Full;
inline constexpr std::uint64_t Hero68Pro=0x484552363850524Full;
inline const wchar_t* Match(std::uint64_t token) {
    if(token==Win60Pro)return L"Aula WIN 60 HE PRO ANSI";
    if(token==Win68)return L"Aula WIN 68 HE PRO / MAX ANSI";
    if(token==Hero68Pro)return L"Aula HERO 68 HE PRO ANSI";
    return nullptr;
}
}
