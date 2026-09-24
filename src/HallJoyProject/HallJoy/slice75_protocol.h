#pragma once
#include "mg75_pro_protocol.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
namespace halljoy::slice75 {
constexpr std::uint16_t kVendorId = 0x1ca3, kProductId = 0x0701,
                        kUsagePage = 0xffa0, kUsage = 1;
constexpr std::size_t kReportBytes = 65, kSlots = 126;
constexpr std::uint16_t kFn = 0x409, kRange = 3300;
using Report = std::array<std::uint8_t, kReportBytes>;
using Values = std::array<std::uint16_t, 63>;
// Slice75 HE 1.1.7.3 Windows factory matrix, file offset 0x4B8.
constexpr std::array<std::uint16_t, kSlots> kFactoryActions = {
    {41, 58,  59, 60, 61,    62,  63, 64, 65, 66,  67,  68,  69, 73, 76, 0,
     0,  0,   0,  0,  0,     53,  30, 31, 32, 33,  34,  35,  36, 37, 38, 39,
     45, 46,  42, 75, 0,     0,   0,  0,  0,  0,   43,  20,  26, 8,  21, 23,
     28, 24,  12, 18, 19,    47,  48, 49, 78, 0,   0,   0,   0,  0,  0,  57,
     4,  22,  7,  9,  10,    11,  13, 14, 15, 51,  52,  0,   40, 0,  0,  0,
     0,  0,   0,  0,  225,   0,   29, 27, 6,  25,  5,   17,  16, 54, 55, 56,
     0,  229, 82, 0,  0,     0,   0,  0,  0,  224, 227, 226, 0,  0,  0,  44,
     0,  0,   0,  0,  61441, 228, 80, 81, 79, 0,   0,   0,   0,  0}};
constexpr std::uint16_t Decode(std::uint16_t action) noexcept {
  return action == 0xf001 ? kFn : action <= 0xff ? action : 0;
}
constexpr std::uint8_t Selector(std::size_t slot) noexcept {
  return kFactoryActions[slot] == 0xf001
             ? 1
             : static_cast<std::uint8_t>(kFactoryActions[slot]);
}
inline bool ExactModel(unsigned vid, unsigned pid,
                       std::wstring_view name) noexcept {
  return vid == kVendorId && pid == kProductId && name == L"SLICE75 HE";
}
using halljoy::mg75pro::Assignments;
using halljoy::mg75pro::Checksum;
using halljoy::mg75pro::Factory;
using halljoy::mg75pro::Frame;
using halljoy::mg75pro::Keys;
using halljoy::mg75pro::Layout;
using halljoy::mg75pro::ParseLayout;
using halljoy::mg75pro::ParseTravel;
using halljoy::mg75pro::Request;
using halljoy::mg75pro::Travel;
inline bool MatchFactory(const Frame &f, unsigned row) noexcept {
  if (row > 4 || row % 2 || !f.Complete() || f.size != 49 ||
      f.bytes[2] != 0xab || f.bytes[5] != row || f.bytes[27] != row + 1)
    return false;
  for (unsigned c = 0; c < 21; ++c)
    if (f.bytes[6 + c] != Selector(row * 21 + c) ||
        f.bytes[28 + c] != Selector((row + 1) * 21 + c))
      return false;
  return true;
}
inline std::uint16_t Normalize(std::uint16_t raw) noexcept {
  return static_cast<std::uint16_t>(std::min<std::uint32_t>(
      1000, (static_cast<std::uint32_t>(raw) * 1000 + kRange / 2) / kRange));
}
} // namespace halljoy::slice75
