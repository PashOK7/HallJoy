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

int KeyboardLayout_GetPresetCount();
const wchar_t* KeyboardLayout_GetPresetName(int idx);
int KeyboardLayout_GetCurrentPresetIndex();
void KeyboardLayout_SetPresetIndex(int idx);
void KeyboardLayout_ResetActiveToPreset();
bool KeyboardLayout_SetKeyGeometry(int idx, int row, int x, int w);
bool KeyboardLayout_GetKey(int idx, KeyDef& out);

bool KeyboardLayout_LoadFromIni(const wchar_t* path);
void KeyboardLayout_SaveToIni(const wchar_t* path);

constexpr int KEYBOARD_MARGIN_X = 12;
constexpr int KEYBOARD_MARGIN_Y = 12;
constexpr int KEYBOARD_ROW_PITCH_Y = 46;
constexpr int KEYBOARD_KEY_H = 40;
