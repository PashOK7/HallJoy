#pragma once
#include <cstdint>
#include <memory>
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
    int y = -1; // -1: legacy row placement; otherwise exact layout pixels
    // ISO/JIS Enter: remove the bottom-left corner of the bounding rectangle.
    // Both zero means rectangular. Otherwise 0 < notchW < w, 0 < notchY < h.
    int notchW = 0;
    int notchY = 0;
};

inline bool KeyboardLayout_ValidShape(const KeyDef& key)
{
    return (key.notchW == 0 && key.notchY == 0) ||
        (key.notchW > 0 && key.notchW < key.w && key.notchY > 0 && key.notchY < key.h);
}

inline bool KeyboardLayout_Contains(const KeyDef& key, double x, double y)
{
    return x >= 0 && y >= 0 && x < key.w && y < key.h &&
        (!key.notchW || x >= key.notchW || y < key.notchY);
}

inline int KeyboardLayout_KeyY(const KeyDef& key)
{
    return key.y >= 0 ? key.y : key.row * KEYBOARD_ROW_PITCH_Y;
}

struct KeyboardLayoutSnapshot
{
    std::vector<KeyDef> keys;
    std::vector<std::wstring> labels;
};

std::shared_ptr<const KeyboardLayoutSnapshot> KeyboardLayout_GetSnapshot();
// Worker-safe immutable view. Selection/catalog mutations remain on the UI thread.
std::shared_ptr<const KeyboardLayoutSnapshot> KeyboardLayout_GetOverlaySnapshot();
int KeyboardLayout_GetOverlayPresetIndex(); // -1: follow the main preview
void KeyboardLayout_SetOverlayPresetIndex(int idx);
void KeyboardLayout_SetOverlayPresetName(const wchar_t* name);
const wchar_t* KeyboardLayout_GetOverlayPresetName(); // empty: follow
int KeyboardLayout_Count();
const KeyDef* KeyboardLayout_Data();

int KeyboardLayout_GetPresetCount();
uint64_t KeyboardLayout_GetCatalogRevision();
const wchar_t* KeyboardLayout_GetPresetName(int idx);
std::wstring KeyboardLayout_GetPresetDisplayName(int idx);
std::wstring KeyboardLayout_GetPresetBrand(int idx);
std::wstring KeyboardLayout_GetPresetModel(int idx);
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
bool KeyboardLayout_CreatePreset(const wchar_t* name, int* outIndex = nullptr, int sourcePreset = -1, bool activate = true, const wchar_t* brand = nullptr);
bool KeyboardLayout_DeletePreset(int idx);
bool KeyboardLayout_GetPresetSnapshot(int presetIdx, std::vector<KeyDef>& outKeys, std::vector<std::wstring>& outLabels, bool* outUniformSpacing, int* outUniformGap);
bool KeyboardLayout_StorePresetSnapshot(int presetIdx, const std::vector<KeyDef>& keys, const std::vector<std::wstring>& labels, bool applyIfActive, bool uniformSpacing, int uniformGap);

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool KeyboardLayout_TestFirstRunSelection();
bool KeyboardLayout_TestSaveActivePresetToPath(const wchar_t* path);
bool KeyboardLayout_TestReadPresetY(const wchar_t* path, int index, int* y);
#endif

bool KeyboardLayout_LoadFromIni(const wchar_t* path);
bool KeyboardLayout_SaveToIni(const wchar_t* path);
struct BackendAnalogTelemetry;
void KeyboardLayout_ArmFirstRunSelection();
bool KeyboardLayout_TryFirstRunSelection(bool searchCompleted, const BackendAnalogTelemetry& telemetry);
