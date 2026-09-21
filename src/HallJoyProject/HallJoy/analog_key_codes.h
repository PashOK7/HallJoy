#pragma once

#include <cstddef>
#include <cstdint>
#include "halljoy_wooting_physical.h"

// HallJoy uses USB HID keyboard usages directly for ordinary keys. Soup/UAP
// also defines stable extended codes for physical vendor/layer keys that have
// no USB HID keyboard usage of their own. Keep those codes intact end-to-end;
// assigning them an unrelated byte-sized HID would create collisions.
namespace halljoy::keycode
{
constexpr std::uint16_t kStandardHidCount = 0x100;
constexpr std::uint16_t kOem1 = 0x403;
constexpr std::uint16_t kFn = 0x409;
// Native O3C physical channels; labels may change without changing bindings.
constexpr std::uint16_t kO3cFirst = 0x470;
constexpr bool IsO3c(std::uint16_t code) noexcept { return code>=kO3cFirst && code<kO3cFirst+3; }
constexpr std::size_t kCount = static_cast<std::size_t>(halljoy::wooting_physical::kRightFn) + 1u;
constexpr std::size_t kMaskChunkBits = 64u;
constexpr std::size_t kMaskChunkCount =
    (kCount + kMaskChunkBits - 1u) / kMaskChunkBits;

constexpr bool IsSupported(std::uint16_t code) noexcept
{
    return code != 0 && static_cast<std::size_t>(code) < kCount;
}

constexpr bool IsStandardHid(std::uint16_t code) noexcept
{
    return code != 0 && code < kStandardHidCount;
}
}
