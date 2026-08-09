#pragma once

#include <cstdint>

// Pseudo HID codes reserved for mouse inputs in bindings.
// Kept in <256 range so existing button bitmask/profile format works unchanged.
static constexpr uint16_t kMouseBindHidLButton = 240;
static constexpr uint16_t kMouseBindHidRButton = 241;
static constexpr uint16_t kMouseBindHidMButton = 242;
static constexpr uint16_t kMouseBindHidX1 = 243;
static constexpr uint16_t kMouseBindHidX2 = 244;
static constexpr uint16_t kMouseBindHidWheelUp = 245;
static constexpr uint16_t kMouseBindHidWheelDown = 246;

static inline bool MouseBind_IsPseudoHid(uint16_t hid)
{
    return hid >= kMouseBindHidLButton && hid <= kMouseBindHidWheelDown;
}

static inline const wchar_t* MouseBind_Name(uint16_t hid)
{
    switch (hid)
    {
    case kMouseBindHidLButton: return L"Mouse Left";
    case kMouseBindHidRButton: return L"Mouse Right";
    case kMouseBindHidMButton: return L"Mouse Middle";
    case kMouseBindHidX1: return L"Mouse X1";
    case kMouseBindHidX2: return L"Mouse X2";
    case kMouseBindHidWheelUp: return L"Mouse Wheel Up";
    case kMouseBindHidWheelDown: return L"Mouse Wheel Down";
    default: return L"Unknown Mouse Input";
    }
}
