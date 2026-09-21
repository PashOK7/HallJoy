#pragma once
#include <cstdint>
#include <cstddef>
namespace halljoy::sparklink {
// JingTai V2 Fn1: official SDK mapping and MG75 Max 1.1.3 factory tables.
constexpr std::uint16_t kFn = 0x409;
constexpr std::size_t kKeyCount = 0x410;
constexpr std::uint16_t DecodeKey(std::uint16_t code) noexcept {
    return code == 0xF101 ? kFn : code <= 0xFF ? code : 0;
}
}
