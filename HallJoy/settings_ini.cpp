// settings_ini.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <cstdint>
#include <vector>
#include <utility>
#include <cmath>
#include <unordered_set>
#include <algorithm>

#include "settings_ini.h"
#include "settings.h"
#include "key_settings.h"
#include "ini_util.h"
#include "keyboard_layout.h"

static float ClampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void IniWriteFloat1000(const wchar_t* section, const wchar_t* key, float v, const wchar_t* path)
{
    int iv = (int)lroundf(v * 1000.0f);
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", iv);
    WritePrivateProfileStringW(section, key, buf, path);
}

static float IniReadFloat1000(const wchar_t* section, const wchar_t* key, float def, const wchar_t* path)
{
    int defI = (int)lroundf(def * 1000.0f);
    int iv = GetPrivateProfileIntW(section, key, defI, path);
    return (float)iv / 1000.0f;
}

static void IniWriteU32(const wchar_t* section, const wchar_t* key, UINT v, const wchar_t* path)
{
    wchar_t buf[32]{};
    swprintf_s(buf, L"%u", (unsigned)v);
    WritePrivateProfileStringW(section, key, buf, path);
}

static UINT IniReadU32(const wchar_t* section, const wchar_t* key, UINT def, const wchar_t* path)
{
    return (UINT)GetPrivateProfileIntW(section, key, (int)def, path);
}

static void IniWriteI32(const wchar_t* section, const wchar_t* key, int v, const wchar_t* path)
{
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", v);
    WritePrivateProfileStringW(section, key, buf, path);
}

static bool ReadSectionKeys(const wchar_t* section, const wchar_t* path, std::vector<std::wstring>& keysOut)
{
    keysOut.clear();
    if (!section || !path) return false;

    // GetPrivateProfileSectionW truncates output if buffer is too small.
    // If truncated, return value is typically (cap - 2).
    // We grow the buffer until it fits or we hit a sane limit.
    DWORD cap = 64 * 1024;
    const DWORD CAP_MAX = 4 * 1024 * 1024; // 4 MB safety cap

    std::vector<wchar_t> buf;

    for (;;)
    {
        buf.assign(cap, 0);

        DWORD n = GetPrivateProfileSectionW(section, buf.data(), cap, path);
        if (n == 0)
        {
            // section missing or empty
            return false;
        }

        // If truncated, n will be close to cap-2 (no guaranteed contract, but this is the common behavior).
        // Also ensure double-null termination exists.
        bool likelyTruncated = (n >= cap - 2);

        if (!likelyTruncated)
        {
            // parse normally
            const wchar_t* p = buf.data();
            while (*p)
            {
                const wchar_t* eq = wcschr(p, L'=');
                if (eq && eq > p)
                    keysOut.emplace_back(p, (size_t)(eq - p));

                p += wcslen(p) + 1;
            }
            return true;
        }

        // grow buffer
        if (cap >= CAP_MAX)
        {
            // still truncated at maximum size -> fail fast (better than silently losing settings)
            return false;
        }

        cap = std::min<DWORD>(cap * 2, CAP_MAX);
    }
}

static void KeySettingsIni_SaveToSettingsIni(const wchar_t* path)
{
    // rewrite the whole section
    WritePrivateProfileStringW(L"KeyDeadzone", nullptr, nullptr, path);

    std::vector<std::pair<uint16_t, KeyDeadzone>> all;
    KeySettings_Enumerate(all);

    for (const auto& kv : all)
    {
        uint16_t hid = kv.first;
        const KeyDeadzone& ks = kv.second;

        wchar_t kUse[64], kInv[64], kMode[64];
        wchar_t kLow[64], kHigh[64], kADZ[64], kCap[64];
        wchar_t kC1X[64], kC1Y[64], kC2X[64], kC2Y[64];
        wchar_t kC1W[64], kC2W[64];

        swprintf_s(kUse, L"%u_Use", (unsigned)hid);
        swprintf_s(kInv, L"%u_Inv", (unsigned)hid);
        swprintf_s(kMode, L"%u_Mode", (unsigned)hid);

        swprintf_s(kLow, L"%u_L", (unsigned)hid);
        swprintf_s(kHigh, L"%u_H", (unsigned)hid);
        swprintf_s(kADZ, L"%u_ADZ", (unsigned)hid);
        swprintf_s(kCap, L"%u_Cap", (unsigned)hid);

        swprintf_s(kC1X, L"%u_C1X", (unsigned)hid);
        swprintf_s(kC1Y, L"%u_C1Y", (unsigned)hid);
        swprintf_s(kC2X, L"%u_C2X", (unsigned)hid);
        swprintf_s(kC2Y, L"%u_C2Y", (unsigned)hid);

        swprintf_s(kC1W, L"%u_C1W", (unsigned)hid);
        swprintf_s(kC2W, L"%u_C2W", (unsigned)hid);

        IniWriteI32(L"KeyDeadzone", kUse, ks.useUnique ? 1 : 0, path);

        if (ks.invert) IniWriteI32(L"KeyDeadzone", kInv, 1, path);
        if (ks.curveMode != 0) IniWriteI32(L"KeyDeadzone", kMode, (int)ks.curveMode, path);

        IniWriteI32(L"KeyDeadzone", kLow, (int)lroundf(ks.low * 1000.0f), path);
        IniWriteI32(L"KeyDeadzone", kHigh, (int)lroundf(ks.high * 1000.0f), path);

        if (ks.antiDeadzone > 0.001f)
            IniWriteI32(L"KeyDeadzone", kADZ, (int)lroundf(ks.antiDeadzone * 1000.0f), path);

        if (ks.outputCap < 0.999f)
            IniWriteI32(L"KeyDeadzone", kCap, (int)lroundf(ks.outputCap * 1000.0f), path);

        IniWriteI32(L"KeyDeadzone", kC1X, (int)lroundf(ks.cp1_x * 1000.0f), path);
        IniWriteI32(L"KeyDeadzone", kC1Y, (int)lroundf(ks.cp1_y * 1000.0f), path);
        IniWriteI32(L"KeyDeadzone", kC2X, (int)lroundf(ks.cp2_x * 1000.0f), path);
        IniWriteI32(L"KeyDeadzone", kC2Y, (int)lroundf(ks.cp2_y * 1000.0f), path);

        float w1 = ClampF(ks.cp1_w, 0.0f, 1.0f);
        float w2 = ClampF(ks.cp2_w, 0.0f, 1.0f);
        IniWriteI32(L"KeyDeadzone", kC1W, (int)lroundf(w1 * 1000.0f), path);
        IniWriteI32(L"KeyDeadzone", kC2W, (int)lroundf(w2 * 1000.0f), path);
    }
}

static void KeySettingsIni_LoadFromSettingsIni(const wchar_t* path)
{
    KeySettings_ClearAll();

    std::vector<std::wstring> keys;
    if (!ReadSectionKeys(L"KeyDeadzone", path, keys)) return;

    std::unordered_set<uint16_t> hids;
    hids.reserve(keys.size());

    for (const auto& k : keys)
    {
        size_t us = k.find(L'_');
        std::wstring prefix = (us == std::wstring::npos) ? k : k.substr(0, us);
        int hidI = _wtoi(prefix.c_str());
        if (hidI > 0 && hidI <= 65535)
            hids.insert((uint16_t)hidI);
    }

    for (uint16_t hid : hids)
    {
        wchar_t kUse[64], kInv[64], kMode[64];
        wchar_t kLow[64], kHigh[64], kADZ[64], kCap[64];
        wchar_t kC1X[64], kC1Y[64], kC2X[64], kC2Y[64];
        wchar_t kC1W[64], kC2W[64];

        swprintf_s(kUse, L"%u_Use", (unsigned)hid);
        swprintf_s(kInv, L"%u_Inv", (unsigned)hid);
        swprintf_s(kMode, L"%u_Mode", (unsigned)hid);

        swprintf_s(kLow, L"%u_L", (unsigned)hid);
        swprintf_s(kHigh, L"%u_H", (unsigned)hid);
        swprintf_s(kADZ, L"%u_ADZ", (unsigned)hid);
        swprintf_s(kCap, L"%u_Cap", (unsigned)hid);

        swprintf_s(kC1X, L"%u_C1X", (unsigned)hid);
        swprintf_s(kC1Y, L"%u_C1Y", (unsigned)hid);
        swprintf_s(kC2X, L"%u_C2X", (unsigned)hid);
        swprintf_s(kC2Y, L"%u_C2Y", (unsigned)hid);

        swprintf_s(kC1W, L"%u_C1W", (unsigned)hid);
        swprintf_s(kC2W, L"%u_C2W", (unsigned)hid);

        int use = GetPrivateProfileIntW(L"KeyDeadzone", kUse, 0, path);
        int inv = GetPrivateProfileIntW(L"KeyDeadzone", kInv, 0, path);
        int mode = GetPrivateProfileIntW(L"KeyDeadzone", kMode, 0, path);

        int lowM = GetPrivateProfileIntW(L"KeyDeadzone", kLow, 80, path);
        int higM = GetPrivateProfileIntW(L"KeyDeadzone", kHigh, 980, path);

        int adzM = GetPrivateProfileIntW(L"KeyDeadzone", kADZ, 0, path);
        int capM = GetPrivateProfileIntW(L"KeyDeadzone", kCap, 1000, path);

        int c1x = GetPrivateProfileIntW(L"KeyDeadzone", kC1X, 380, path);
        int c1y = GetPrivateProfileIntW(L"KeyDeadzone", kC1Y, 330, path);
        int c2x = GetPrivateProfileIntW(L"KeyDeadzone", kC2X, 680, path);
        int c2y = GetPrivateProfileIntW(L"KeyDeadzone", kC2Y, 660, path);

        int c1w = GetPrivateProfileIntW(L"KeyDeadzone", kC1W, 1000, path);
        int c2w = GetPrivateProfileIntW(L"KeyDeadzone", kC2W, 1000, path);

        KeyDeadzone ks;
        ks.useUnique = (use != 0);
        ks.invert = (inv != 0);
        ks.curveMode = (uint8_t)((mode == 0) ? 0 : 1);

        ks.low = (float)lowM / 1000.0f;
        ks.high = (float)higM / 1000.0f;
        ks.antiDeadzone = (float)adzM / 1000.0f;
        ks.outputCap = (float)capM / 1000.0f;

        ks.cp1_x = (float)c1x / 1000.0f;
        ks.cp1_y = (float)c1y / 1000.0f;
        ks.cp2_x = (float)c2x / 1000.0f;
        ks.cp2_y = (float)c2y / 1000.0f;

        ks.cp1_w = ClampF((float)c1w / 1000.0f, 0.0f, 1.0f);
        ks.cp2_w = ClampF((float)c2w / 1000.0f, 0.0f, 1.0f);

        KeySettings_Set(hid, ks);
    }
}

bool SettingsIni_Load(const wchar_t* path)
{
    if (!path) return false;

    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;

    float low = IniReadFloat1000(L"Input", L"DeadzoneLow", Settings_GetInputDeadzoneLow(), path);
    float high = IniReadFloat1000(L"Input", L"DeadzoneHigh", Settings_GetInputDeadzoneHigh(), path);

    float adz = IniReadFloat1000(L"Input", L"AntiDeadzone", Settings_GetInputAntiDeadzone(), path);
    float cap = IniReadFloat1000(L"Input", L"OutputCap", Settings_GetInputOutputCap(), path);

    float c1x = IniReadFloat1000(L"Input", L"Cp1X", Settings_GetInputBezierCp1X(), path);
    float c1y = IniReadFloat1000(L"Input", L"Cp1Y", Settings_GetInputBezierCp1Y(), path);
    float c2x = IniReadFloat1000(L"Input", L"Cp2X", Settings_GetInputBezierCp2X(), path);
    float c2y = IniReadFloat1000(L"Input", L"Cp2Y", Settings_GetInputBezierCp2Y(), path);

    float c1w = IniReadFloat1000(L"Input", L"Cp1W", Settings_GetInputBezierCp1W(), path);
    float c2w = IniReadFloat1000(L"Input", L"Cp2W", Settings_GetInputBezierCp2W(), path);

    UINT curveMode = IniReadU32(L"Input", L"CurveMode", Settings_GetInputCurveMode(), path);
    int invert = GetPrivateProfileIntW(L"Input", L"Invert", 0, path);
    int snappy = GetPrivateProfileIntW(L"Input", L"SnappyJoystick", Settings_GetSnappyJoystick() ? 1 : 0, path);

    UINT poll = IniReadU32(L"Main", L"PollingMs", Settings_GetPollingMs(), path);
    UINT uiMs = IniReadU32(L"Main", L"UIRefreshMs", Settings_GetUIRefreshMs(), path);

    Settings_SetInputDeadzoneLow(low);
    Settings_SetInputDeadzoneHigh(high);

    Settings_SetInputAntiDeadzone(adz);
    Settings_SetInputOutputCap(cap);

    Settings_SetInputBezierCp1X(c1x);
    Settings_SetInputBezierCp1Y(c1y);
    Settings_SetInputBezierCp2X(c2x);
    Settings_SetInputBezierCp2Y(c2y);

    Settings_SetInputBezierCp1W(ClampF(c1w, 0.0f, 1.0f));
    Settings_SetInputBezierCp2W(ClampF(c2w, 0.0f, 1.0f));

    Settings_SetInputCurveMode(curveMode);
    Settings_SetInputInvert(invert != 0);
    Settings_SetSnappyJoystick(snappy != 0);

    Settings_SetPollingMs(poll);
    Settings_SetUIRefreshMs(uiMs);

    KeySettingsIni_LoadFromSettingsIni(path);
    KeyboardLayout_LoadFromIni(path);
    return true;
}

// Writes ONLY application settings (settings.ini).
// Curve presets are stored separately by KeyboardProfiles (CurvePresets folder).
static bool SettingsIni_Save_Internal(const wchar_t* tmpPath)
{
    if (!tmpPath) return false;

    IniWriteFloat1000(L"Input", L"DeadzoneLow", Settings_GetInputDeadzoneLow(), tmpPath);
    IniWriteFloat1000(L"Input", L"DeadzoneHigh", Settings_GetInputDeadzoneHigh(), tmpPath);

    IniWriteFloat1000(L"Input", L"AntiDeadzone", Settings_GetInputAntiDeadzone(), tmpPath);
    IniWriteFloat1000(L"Input", L"OutputCap", Settings_GetInputOutputCap(), tmpPath);

    IniWriteFloat1000(L"Input", L"Cp1X", Settings_GetInputBezierCp1X(), tmpPath);
    IniWriteFloat1000(L"Input", L"Cp1Y", Settings_GetInputBezierCp1Y(), tmpPath);
    IniWriteFloat1000(L"Input", L"Cp2X", Settings_GetInputBezierCp2X(), tmpPath);
    IniWriteFloat1000(L"Input", L"Cp2Y", Settings_GetInputBezierCp2Y(), tmpPath);

    IniWriteFloat1000(L"Input", L"Cp1W", Settings_GetInputBezierCp1W(), tmpPath);
    IniWriteFloat1000(L"Input", L"Cp2W", Settings_GetInputBezierCp2W(), tmpPath);

    IniWriteU32(L"Input", L"CurveMode", Settings_GetInputCurveMode(), tmpPath);
    IniWriteI32(L"Input", L"Invert", Settings_GetInputInvert() ? 1 : 0, tmpPath);
    IniWriteI32(L"Input", L"SnappyJoystick", Settings_GetSnappyJoystick() ? 1 : 0, tmpPath);

    IniWriteU32(L"Main", L"PollingMs", Settings_GetPollingMs(), tmpPath);
    IniWriteU32(L"Main", L"UIRefreshMs", Settings_GetUIRefreshMs(), tmpPath);

    KeySettingsIni_SaveToSettingsIni(tmpPath);
    KeyboardLayout_SaveToIni(tmpPath);
    return true;
}

bool SettingsIni_Save(const wchar_t* path)
{
    if (!path) return false;

    std::wstring tmp = std::wstring(path) + L".tmp";
    DeleteFileW(tmp.c_str());

    if (!SettingsIni_Save_Internal(tmp.c_str()))
    {
        DeleteFileW(tmp.c_str());
        return false;
    }

    return IniUtil_AtomicReplace(tmp.c_str(), path);
}
