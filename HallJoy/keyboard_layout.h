#pragma once
#include <cstdint>

struct KeyDef
{
    const wchar_t* label;
    uint16_t hid;
    int row;
    int x;
    int w;
};

int KeyboardLayout_Count();
const KeyDef* KeyboardLayout_Data();

constexpr int KEYBOARD_MARGIN_X = 12;
constexpr int KEYBOARD_MARGIN_Y = 12;
constexpr int KEYBOARD_ROW_PITCH_Y = 46;
constexpr int KEYBOARD_KEY_H = 40;