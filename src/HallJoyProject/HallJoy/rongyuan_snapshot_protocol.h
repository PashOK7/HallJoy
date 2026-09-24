#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace halljoy::rongyuan {
using Report = std::array<std::uint8_t, 65>;
using Values = std::array<std::uint16_t, 32>;
using Matrix = std::array<std::uint8_t, 512>;
constexpr std::uint16_t kVendorId = 0x3151, kProductId = 0x5030;
constexpr unsigned kSlots = 128;
constexpr std::uint16_t Decode(const std::uint8_t *p) noexcept {
  return p[0] == 0 && p[1] == 0 && p[3] == 0 ? p[2]
         : p[0] == 10 && p[1] == 1           ? 0x409
                                             : 0;
}
// Exact manufacturer classes, not a driver-family fallback. Encoder and
// firmware-only actions intentionally do not become physical keyboard keys.
inline constexpr Matrix kMatrix2819 = {
    {0,  0, 41,  0, 0,  0, 53,  0, 0,  0,  43,  0, 0, 0, 57, 0, 0, 0, 225, 0,
     0,  0, 224, 0, 0,  0, 58,  0, 0,  0,  30,  0, 0, 0, 20, 0, 0, 0, 4,   0,
     0,  0, 0,   0, 0,  0, 227, 0, 0,  0,  59,  0, 0, 0, 31, 0, 0, 0, 26,  0,
     0,  0, 22,  0, 0,  0, 29,  0, 0,  0,  226, 0, 0, 0, 60, 0, 0, 0, 32,  0,
     0,  0, 8,   0, 0,  0, 7,   0, 0,  0,  27,  0, 0, 0, 0,  0, 0, 0, 61,  0,
     0,  0, 33,  0, 0,  0, 21,  0, 0,  0,  9,   0, 0, 0, 6,  0, 0, 0, 0,   0,
     0,  0, 62,  0, 0,  0, 34,  0, 0,  0,  23,  0, 0, 0, 10, 0, 0, 0, 25,  0,
     0,  0, 0,   0, 0,  0, 63,  0, 0,  0,  35,  0, 0, 0, 28, 0, 0, 0, 11,  0,
     0,  0, 5,   0, 0,  0, 44,  0, 0,  0,  64,  0, 0, 0, 36, 0, 0, 0, 24,  0,
     0,  0, 13,  0, 0,  0, 17,  0, 0,  0,  0,   0, 0, 0, 65, 0, 0, 0, 37,  0,
     0,  0, 12,  0, 0,  0, 14,  0, 0,  0,  16,  0, 0, 0, 0,  0, 0, 0, 66,  0,
     0,  0, 38,  0, 0,  0, 18,  0, 0,  0,  15,  0, 0, 0, 54, 0, 0, 0, 230, 0,
     0,  0, 67,  0, 0,  0, 39,  0, 0,  0,  19,  0, 0, 0, 51, 0, 0, 0, 55,  0,
     10, 1, 0,   0, 0,  0, 68,  0, 0,  0,  45,  0, 0, 0, 47, 0, 0, 0, 52,  0,
     0,  0, 56,  0, 0,  0, 228, 0, 0,  0,  69,  0, 0, 0, 46, 0, 0, 0, 48,  0,
     0,  0, 0,   0, 0,  0, 229, 0, 0,  0,  80,  0, 0, 0, 76, 0, 0, 0, 42,  0,
     0,  0, 49,  0, 0,  0, 40,  0, 0,  0,  82,  0, 0, 0, 81, 0, 0, 0, 0,   0,
     0,  0, 74,  0, 0,  0, 75,  0, 0,  0,  78,  0, 0, 0, 77, 0, 0, 0, 79,  0,
     3,  0, 234, 0, 3,  0, 233, 0, 10, 14, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 13, 2, 2,   0, 13, 2,  1,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0,  0, 0,   0, 0,  0, 0,   0, 0,  0,  0,   0}};
inline constexpr Matrix kMatrix2642 = {
    {0, 0, 41,  0, 0,  0, 53,  0, 0, 0, 43,  0, 0, 0, 57,  0, 0, 0, 225, 0,
     0, 0, 224, 0, 0,  0, 58,  0, 0, 0, 30,  0, 0, 0, 20,  0, 0, 0, 4,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 59,  0, 0, 0, 31,  0, 0, 0, 26,  0,
     0, 0, 22,  0, 0,  0, 29,  0, 0, 0, 227, 0, 0, 0, 60,  0, 0, 0, 32,  0,
     0, 0, 8,   0, 0,  0, 7,   0, 0, 0, 27,  0, 0, 0, 226, 0, 0, 0, 61,  0,
     0, 0, 33,  0, 0,  0, 21,  0, 0, 0, 9,   0, 0, 0, 6,   0, 0, 0, 0,   0,
     0, 0, 62,  0, 0,  0, 34,  0, 0, 0, 23,  0, 0, 0, 10,  0, 0, 0, 25,  0,
     0, 0, 0,   0, 0,  0, 63,  0, 0, 0, 35,  0, 0, 0, 28,  0, 0, 0, 11,  0,
     0, 0, 5,   0, 0,  0, 44,  0, 0, 0, 64,  0, 0, 0, 36,  0, 0, 0, 24,  0,
     0, 0, 13,  0, 0,  0, 17,  0, 0, 0, 0,   0, 0, 0, 65,  0, 0, 0, 37,  0,
     0, 0, 12,  0, 0,  0, 14,  0, 0, 0, 16,  0, 0, 0, 0,   0, 0, 0, 66,  0,
     0, 0, 38,  0, 0,  0, 18,  0, 0, 0, 15,  0, 0, 0, 54,  0, 0, 0, 0,   0,
     0, 0, 67,  0, 0,  0, 39,  0, 0, 0, 19,  0, 0, 0, 51,  0, 0, 0, 55,  0,
     0, 0, 230, 0, 0,  0, 68,  0, 0, 0, 45,  0, 0, 0, 47,  0, 0, 0, 52,  0,
     0, 0, 56,  0, 10, 1, 0,   0, 0, 0, 69,  0, 0, 0, 46,  0, 0, 0, 48,  0,
     0, 0, 0,   0, 0,  0, 229, 0, 0, 0, 228, 0, 0, 0, 76,  0, 0, 0, 42,  0,
     0, 0, 49,  0, 0,  0, 40,  0, 0, 0, 0,   0, 0, 0, 80,  0, 0, 0, 70,  0,
     0, 0, 74,  0, 0,  0, 77,  0, 0, 0, 0,   0, 0, 0, 82,  0, 0, 0, 81,  0,
     0, 0, 72,  0, 0,  0, 75,  0, 0, 0, 78,  0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 79,  0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0}};
inline constexpr Matrix kMatrix2959 = {
    {0, 0, 41,  0, 0,  0, 53,  0, 0, 0, 43,  0, 0, 0, 57, 0, 0, 0, 225, 0,
     0, 0, 224, 0, 0,  0, 58,  0, 0, 0, 30,  0, 0, 0, 20, 0, 0, 0, 4,   0,
     0, 0, 0,   0, 0,  0, 227, 0, 0, 0, 59,  0, 0, 0, 31, 0, 0, 0, 26,  0,
     0, 0, 22,  0, 0,  0, 29,  0, 0, 0, 226, 0, 0, 0, 60, 0, 0, 0, 32,  0,
     0, 0, 8,   0, 0,  0, 7,   0, 0, 0, 27,  0, 0, 0, 0,  0, 0, 0, 61,  0,
     0, 0, 33,  0, 0,  0, 21,  0, 0, 0, 9,   0, 0, 0, 6,  0, 0, 0, 0,   0,
     0, 0, 62,  0, 0,  0, 34,  0, 0, 0, 23,  0, 0, 0, 10, 0, 0, 0, 25,  0,
     0, 0, 0,   0, 0,  0, 63,  0, 0, 0, 35,  0, 0, 0, 28, 0, 0, 0, 11,  0,
     0, 0, 5,   0, 0,  0, 44,  0, 0, 0, 64,  0, 0, 0, 36, 0, 0, 0, 24,  0,
     0, 0, 13,  0, 0,  0, 17,  0, 0, 0, 0,   0, 0, 0, 65, 0, 0, 0, 37,  0,
     0, 0, 12,  0, 0,  0, 14,  0, 0, 0, 16,  0, 0, 0, 0,  0, 0, 0, 66,  0,
     0, 0, 38,  0, 0,  0, 18,  0, 0, 0, 15,  0, 0, 0, 54, 0, 0, 0, 0,   0,
     0, 0, 67,  0, 0,  0, 39,  0, 0, 0, 19,  0, 0, 0, 51, 0, 0, 0, 55,  0,
     0, 0, 230, 0, 0,  0, 68,  0, 0, 0, 45,  0, 0, 0, 47, 0, 0, 0, 52,  0,
     0, 0, 56,  0, 10, 1, 0,   0, 0, 0, 69,  0, 0, 0, 46, 0, 0, 0, 48,  0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 228, 0, 0, 0, 76, 0, 0, 0, 42,  0,
     0, 0, 49,  0, 0,  0, 40,  0, 0, 0, 229, 0, 0, 0, 80, 0, 0, 0, 70,  0,
     0, 0, 74,  0, 0,  0, 77,  0, 0, 0, 0,   0, 0, 0, 82, 0, 0, 0, 81,  0,
     0, 0, 72,  0, 0,  0, 75,  0, 0, 0, 78,  0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 79,  0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,   0,
     0, 0, 0,   0, 0,  0, 0,   0, 0, 0, 0,   0}};
struct Model {
  unsigned board;
  const char *product;
  const wchar_t *name;
  unsigned rangeUm;
  const Matrix *matrix;
};
inline constexpr Model kModels[] = {
    {2819, "M1V5HE-2819", L"MonsGeek M1 V5 HE", 3600, &kMatrix2819},
    {2642, "G84HE-2642", L"EPOMAKER G84 HE", 3500, &kMatrix2642},
    {2959, "G84HE-2959", L"EPOMAKER G84 HE", 3500, &kMatrix2959}};
inline const Model *Find(unsigned board) noexcept {
  for (const auto &m : kModels)
    if (m.board == board)
      return &m;
  return nullptr;
}
inline unsigned Board(const Report &r) noexcept {
  return r[0] == 0 && r[1] == 0x8f
             ? unsigned(r[2]) | (unsigned(r[3]) << 8) | (unsigned(r[4]) << 16) |
                   (unsigned(r[5]) << 24)
             : 0;
}
inline Report Request(std::uint8_t cmd, std::uint8_t a = 0, std::uint8_t b = 0,
                      std::uint8_t page = 0) noexcept {
  Report r{};
  r[1] = cmd;
  r[2] = a;
  r[3] = b;
  r[4] = page;
  unsigned sum = 0;
  for (unsigned i = 1; i <= 7; ++i)
    sum += r[i];
  r[8] = static_cast<std::uint8_t>(255 - sum);
  // Read-only matrix commands consume only the first eight payload bytes.
  // Their response replaces all 64 bytes in the shared firmware buffer.
  // Invalid initial tail prevents accepting a partially produced snapshot.
  if (cmd == 0xe5 || cmd == 0x8a)
    std::fill(r.begin() + 9, r.end(), std::uint8_t{255});
  return r;
}
inline unsigned Units(unsigned version, const Report &features) noexcept {
  if (features[1] == 0xe6 && features[2] == 0xaa) {
    constexpr unsigned units[] = {100, 200, 1000};
    return features[3] < 3 ? units[features[3]] : 0;
  }
  return version >= 0x500 ? 200 : version >= 0x300 ? 100 : 10;
}
inline bool ParseTravel(const Report &r, unsigned units, Values &out) noexcept {
  if (r[0] != 0 || !units)
    return false;
  Values v{};
  for (unsigned i = 0; i < 32; ++i) {
    v[i] = std::uint16_t(r[1 + 2 * i] | (r[2 + 2 * i] << 8));
    if (v[i] > 6 * units)
      return false;
  }
  out = v;
  return true;
}
inline bool ValidAssignments(const Report &r) noexcept {
  if (r[0])
    return false;
  for (unsigned i = 1; i < 65; i += 4)
    if (r[i] > 0x20 || r[i + 3] == 255 || (r[i] == 0 && r[i + 3] != 0))
      return false;
  return true;
}
inline std::uint16_t Normalize(unsigned value, unsigned units,
                               unsigned rangeUm) noexcept {
  if (!units || !rangeUm)
    return 0;
  const auto divisor = std::uint64_t(units) * rangeUm;
  return static_cast<std::uint16_t>(std::min<std::uint64_t>(
      1000, (std::uint64_t(value) * 1000000 + divisor / 2) / divisor));
}
} // namespace halljoy::rongyuan
