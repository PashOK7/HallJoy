// key_settings.h
#pragma once
#include <cstdint>
#include <vector>
#include <utility>

struct KeyDeadzone
{
    bool useUnique = false;

    // NEW: invert input (x = 1 - x). Useful for "default pressed, release on press".
    bool invert = false;

    // Start Point (P0)
    float low = 0.08f;           // X
    float antiDeadzone = 0.0f;   // Y

    // End Point (P3)
    float high = 0.90f;          // X
    float outputCap = 1.0f;      // Y

    // Control Point 1 (P1) - for Bezier curve
    // Default: linear distribution (~1/3)
    float cp1_x = 0.38f;
    float cp1_y = 0.33f;

    // Control Point 2 (P2) - for Bezier curve
    // Default: linear distribution (~2/3)
    float cp2_x = 0.68f;
    float cp2_y = 0.66f;

    // NEW: Control point weights (0..1)
    // 1.0 = CP affects curve normally (current behavior)
    // 0.0 = CP has no effect (CP is treated as "neutral" on the P0->P3 line)
    float cp1_w = 1.0f;
    float cp2_w = 1.0f;

    // NEW: per-key curve mode (used only when useUnique==true)
    // 0 = Smooth (Bezier)
    // 1 = Linear (Segments)
    uint8_t curveMode = 1;
};

// set/get by HID
void KeySettings_Set(uint16_t hid, const KeyDeadzone& s);
KeyDeadzone KeySettings_Get(uint16_t hid);

// Fast-path helper: returns only useUnique flag.
// For HID < 256 this should be lock-free (implementation detail).
// For HID >= 256 it may be slower.
bool KeySettings_GetUseUnique(uint16_t hid);

// helpers
void KeySettings_SetUseUnique(uint16_t hid, bool on);
void KeySettings_SetLow(uint16_t hid, float low);
void KeySettings_SetHigh(uint16_t hid, float high);
void KeySettings_SetAntiDeadzone(uint16_t hid, float val);
void KeySettings_SetOutputCap(uint16_t hid, float val);

// for ini save/load
void KeySettings_ClearAll();
void KeySettings_Enumerate(std::vector<std::pair<uint16_t, KeyDeadzone>>& out);
