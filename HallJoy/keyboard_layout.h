#pragma once
#include <cstdint>
#include <string>
#include <vector>

constexpr int KEYBOARD_MARGIN_X = 12;
constexpr int KEYBOARD_MARGIN_Y = 12;
constexpr int KEYBOARD_ROW_PITCH_Y = 46;
constexpr int KEYBOARD_KEY_H = 40;
constexpr int KEYBOARD_KEY_MIN_DIM = 18;
constexpr int KEYBOARD_KEY_MAX_DIM = 600;

struct KeyDef
{
    const wchar_t* label;
    uint16_t hid;
    int row;
    int x;
    int w;
    int h = KEYBOARD_KEY_H;
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
bool KeyboardLayout_AddKey(uint16_t hid, const wchar_t* label, int row, int x, int w);
bool KeyboardLayout_RemoveKey(int idx);
bool KeyboardLayout_SetKeyLabel(int idx, const wchar_t* label);
bool KeyboardLayout_SetKeyHid(int idx, uint16_t hid);
bool KeyboardLayout_SaveActivePreset();
bool KeyboardLayout_CreatePreset(const wchar_t* name, int* outIndex = nullptr);
bool KeyboardLayout_DeletePreset(int idx);
bool KeyboardLayout_GetPresetSnapshot(int presetIdx, std::vector<KeyDef>& outKeys, std::vector<std::wstring>& outLabels, bool* outUniformSpacing, int* outUniformGap);
bool KeyboardLayout_StorePresetSnapshot(int presetIdx, const std::vector<KeyDef>& keys, const std::vector<std::wstring>& labels, bool applyIfActive, bool uniformSpacing, int uniformGap);

bool KeyboardLayout_LoadFromIni(const wchar_t* path);
void KeyboardLayout_SaveToIni(const wchar_t* path);
