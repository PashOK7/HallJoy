#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
namespace halljoy::mg75pro {
constexpr std::uint16_t kVendorId = 0x1ca5, kProductId = 0x0807,
                        kUsagePage = 0xffa0, kUsage = 1;
constexpr std::size_t kReportBytes = 65, kSlots = 126;
constexpr std::uint16_t kFn = 0x409, kRange = 3500;
using Report = std::array<std::uint8_t, kReportBytes>;
using Values = std::array<std::uint16_t, 63>;
// Exact MG75 Pro 1.1.0 factory matrix at file offset 0x1F720.
constexpr std::array<std::uint16_t, kSlots> kFactoryActions = {
    {41, 58,  59, 60,  61,    62, 63, 64, 65, 66,  67,  68,  69, 70, 76, 0,
     0,  0,   0,  0,   0,     53, 30, 31, 32, 33,  34,  35,  36, 37, 38, 39,
     45, 46,  42, 73,  0,     0,  0,  0,  0,  0,   43,  20,  26, 8,  21, 23,
     28, 24,  12, 18,  19,    47, 48, 49, 75, 0,   0,   0,   0,  0,  0,  57,
     4,  22,  7,  9,   10,    11, 13, 14, 15, 51,  52,  0,   40, 78, 0,  0,
     0,  0,   0,  0,   225,   0,  29, 27, 6,  25,  5,   17,  16, 54, 55, 56,
     0,  229, 82, 0,   0,     0,  0,  0,  0,  224, 227, 226, 0,  0,  0,  44,
     0,  0,   0,  230, 61441, 80, 81, 79, 0,  0,   0,   0,   0,  0}};
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
  return vid == kVendorId && pid == kProductId && name == L"IROK MG75 PRO";
}
inline std::uint8_t Checksum(const std::uint8_t *p) noexcept {
  return static_cast<std::uint8_t>(0x35 + p[0] + p[1] + p[2] +
                                   (p[1] ? p[3 + p[1]] : 0));
}
inline Report Request(std::uint8_t command,
                      std::initializer_list<std::uint8_t> payload) {
  Report r{};
  if (payload.size() > 60)
    return r;
  r[1] = 0x5c;
  r[2] = static_cast<std::uint8_t>(payload.size());
  r[3] = command;
  std::copy(payload.begin(), payload.end(), r.begin() + 5);
  r[4] = Checksum(r.data() + 1);
  return r;
}
inline Report Travel(unsigned half) {
  return half == 1 || half == 2
             ? Request(0x12, {2, static_cast<std::uint8_t>(half), 255, 255})
             : Report{};
}
using Keys = std::array<std::uint8_t, 14>;
using Assignments = std::array<std::uint16_t, 14>;
inline Report Layout(const Keys &keys) {
  Report r{};
  r[1] = 0x5c;
  r[2] = 57;
  r[3] = 0x23;
  for (std::size_t i = 0; i < keys.size(); ++i)
    r[6 + 4 * i] = keys[i];
  r[4] = Checksum(r.data() + 1);
  return r;
}
inline Report Factory(unsigned firstRow) {
  return Request(0x2b, {0, static_cast<std::uint8_t>(firstRow),
                        static_cast<std::uint8_t>(firstRow + 1)});
}
// One request at a time; a failed/late exchange poisons the session. No attempt
// to guess continuation ownership or reuse a partial response on a new half.
struct Frame {
  std::array<std::uint8_t, 256> bytes{};
  std::size_t size = 0, received = 0;
  bool failed = false;
  bool Push(const Report &r, std::size_t n, std::uint8_t command) noexcept {
    if (failed || n != kReportBytes || r[0] != 0 || (size && received >= size))
      return failed = true, false;
    if (received == 0) {
      if (r[1] != 0x5c || r[2] > 252 || r[3] != (command | 0x80))
        return failed = true, false;
      size = 4 + r[2];
      if (size < 5)
        return failed = true, false;
    }
    const auto count = std::min<std::size_t>(64, size - received);
    std::copy_n(r.begin() + 1, count, bytes.begin() + received);
    received += count;
    if (received == size &&
        (bytes[4] != 0 || bytes[3] != Checksum(bytes.data())))
      return failed = true, false;
    return true;
  }
  bool Complete() const noexcept { return !failed && size && received == size; }
};
inline bool ParseTravel(const Frame &f, Values &out) noexcept {
  if (!f.Complete() || f.size != 132 || f.bytes[2] != 0x92 || f.bytes[5] != 2)
    return false;
  Values next{};
  for (std::size_t i = 0; i < next.size(); ++i) {
    next[i] = static_cast<std::uint16_t>(f.bytes[6 + 2 * i] |
                                         (f.bytes[7 + 2 * i] << 8));
    if (next[i] > 6000)
      return false;
  }
  out = next;
  return true;
}
inline bool ParseLayout(const Frame &f, const Keys &keys,
                        Assignments &out) noexcept {
  if (!f.Complete() || f.size != 61 || f.bytes[2] != 0xa3)
    return false;
  Assignments next{};
  for (std::size_t i = 0; i < keys.size(); ++i) {
    if (!keys[i])
      continue;
    const auto offset = 5 + 4 * i;
    if (f.bytes[offset] != keys[i] || f.bytes[offset + 1] != 0)
      return false;
    next[i] = Decode(static_cast<std::uint16_t>(f.bytes[offset + 2] |
                                                (f.bytes[offset + 3] << 8)));
  }
  out = next;
  return true;
}
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
inline std::uint16_t Normalize(std::uint16_t raw, unsigned range = kRange) noexcept {
  if (!range) return 0;
  return static_cast<std::uint16_t>(std::min<std::uint32_t>(
      1000, (static_cast<std::uint32_t>(raw) * 1000 + range / 2) / range));
}
} // namespace halljoy::mg75pro
