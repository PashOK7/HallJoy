#pragma once
#include "jingtai_v1_profiles.h"
#include <string_view>
// WLMOUSE Ying75 ("WLKB YING 75", 36A7:F887): JingTai KB2 firmware on the same
// 0x5C protocol as the IROK/MG75 family (travel 0x12/0x92, factory map 0x2B).
// Evidence: official XS117_YING75_App_v1.0.2_20250423b.bin (SHA-256
// e7a6583ecb8373eb3d4436a68a24e83b6962a12fab17126475e89cd232c69e7a), Windows
// factory matrix at file offset 0x4B8 (the macOS copy at 0xC8 differs only by
// swapped Left Win/Alt). 84 keys, 6x21 slots, Fn = 0xF001.
// Range: the firmware's seven switch lookup tables end at 3300 (five) or 3400
// (two) um; 3300 never leaves full travel unreachable.
namespace halljoy::wlmouse_ying75 {
constexpr std::uint16_t kVendorId = 0x36a7, kProductId = 0xf887;
inline constexpr halljoy::jingtai_v1::Model kModel{"YING75-36A7-F887", L"WLMOUSE Ying75", {{
    41,58,59,60,61,62,63,64,65,66,67,68,69,70,71,76,0,0,0,0,0,
    53,30,31,32,33,34,35,36,37,38,39,45,46,42,74,0,0,0,0,0,0,
    43,20,26,8,21,23,28,24,12,18,19,47,48,49,75,0,0,0,0,0,0,
    57,4,22,7,9,10,11,13,14,15,51,52,0,40,78,0,0,0,0,0,0,
    225,0,29,27,6,25,5,17,16,54,55,56,0,229,82,77,0,0,0,0,0,
    224,227,226,0,0,0,44,0,0,0,230,61441,228,80,81,79,0,0,0,0,0}}, 84, 3300};
inline bool ExactModel(unsigned vid, unsigned pid, std::wstring_view name) noexcept {
    return vid == kVendorId && pid == kProductId && name == L"WLKB YING 75";
}
} // namespace halljoy::wlmouse_ying75
