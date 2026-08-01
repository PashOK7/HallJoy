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
#include <limits>

#include "settings_ini.h"
#include "settings.h"
#include "key_settings.h"
#include "ini_util.h"
#include "keyboard_layout.h"
#include "global_profiles.h"
#include "overlay_server.h"

static float ClampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool IniWriteFloat1000(const wchar_t* section, const wchar_t* key, float v, const wchar_t* path)
{
    int iv = (int)lroundf(v * 1000.0f);
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", iv);
    return WritePrivateProfileStringW(section, key, buf, path) != FALSE;
}

static float IniReadFloat1000(const wchar_t* section, const wchar_t* key, float def, const wchar_t* path)
{
    int defI = (int)lroundf(def * 1000.0f);
    int iv = GetPrivateProfileIntW(section, key, defI, path);
    return (float)iv / 1000.0f;
}

static bool IniWriteU32(const wchar_t* section, const wchar_t* key, UINT v, const wchar_t* path)
{
    wchar_t buf[32]{};
    swprintf_s(buf, L"%u", (unsigned)v);
    return WritePrivateProfileStringW(section, key, buf, path) != FALSE;
}

static UINT IniReadU32(const wchar_t* section, const wchar_t* key, UINT def, const wchar_t* path)
{
    return (UINT)GetPrivateProfileIntW(section, key, (int)def, path);
}

static bool IniWriteI32(const wchar_t* section, const wchar_t* key, int v, const wchar_t* path)
{
    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", v);
    return WritePrivateProfileStringW(section, key, buf, path) != FALSE;
}

static int IniReadI32(const wchar_t* section, const wchar_t* key, int def, const wchar_t* path)
{
    wchar_t buf[64]{};
    GetPrivateProfileStringW(section, key, L"", buf, (DWORD)_countof(buf), path);
    if (buf[0] == 0)
        return def;
    return _wtoi(buf);
}

static int OverlayClampStrengthPercent(int value)
{
    return std::clamp(value, 0, 100);
}

static int OverlayLegacyStrengthToNormalized(int value, float legacyValueAt50Percent)
{
    if (legacyValueAt50Percent <= 0.0f)
        return OverlayClampStrengthPercent(value);
    return OverlayClampStrengthPercent((int)lroundf((float)value * 50.0f / legacyValueAt50Percent));
}

static int OverlayLegacyRimLightToNormalized(int value)
{
    return OverlayClampStrengthPercent((int)lroundf((float)value * 50.0f / 359.0f));
}

static int OverlayLegacyGlassToMaterialScale(int value)
{
    if (value <= 0)
        return 50;
    return OverlayClampStrengthPercent(50 + (value / 2));
}

static bool OverlaySettingsIni_SaveToSettingsIni(const wchar_t* path)
{
    bool ok = true;
    ok &= IniWriteI32(L"InputOverlay", L"StrengthScaleVersion", 5, path);
    ok &= IniWriteI32(L"InputOverlay", L"AutoStart", OverlayServer_GetAutoStart() ? 1 : 0, path);
    ok &= IniWriteI32(L"InputOverlay", L"UseRawDepth", OverlayServer_GetUseRawDepth() ? 1 : 0, path);
    ok &= IniWriteU32(L"InputOverlay", L"Port", OverlayServer_GetConfiguredPort(), path);
    ok &= IniWriteU32(L"InputOverlay", L"FillDirection", (UINT)OverlayServer_GetFillDirection(), path);
    ok &= IniWriteU32(L"InputOverlay", L"EffectFlags", OverlayServer_GetEffectFlags(), path);
    ok &= IniWriteU32(L"InputOverlay", L"AccentColor", OverlayServer_GetAccentColor(), path);
    ok &= IniWriteI32(L"InputOverlay", L"RefreshMs", OverlayServer_GetRefreshIntervalMs(), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthSmoothing", OverlayServer_GetEffectStrengthPercent(OverlayEffect_Smoothing), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthGlass", OverlayServer_GetEffectStrengthPercent(OverlayEffect_Glass), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthBloom", OverlayServer_GetEffectStrengthPercent(OverlayEffect_Bloom), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthEdgeSweep", OverlayServer_GetEffectStrengthPercent(OverlayEffect_EdgeSweep), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthMicroScale", OverlayServer_GetEffectStrengthPercent(OverlayEffect_MicroScale), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthLabelContrast", OverlayServer_GetEffectStrengthPercent(OverlayEffect_LabelContrast), path);
    ok &= IniWriteI32(L"InputOverlay", L"StrengthGlassRimLight", OverlayServer_GetEffectStrengthPercent(OverlayEffect_GlassRimLight), path);
    ok &= IniWriteI32(L"InputOverlay", L"LabelFont", OverlayServer_GetLabelFontIndex(), path);
    ok &= IniWriteI32(L"InputOverlay", L"LabelSizePx", OverlayServer_GetLabelSizePx(), path);
    ok &= IniWriteI32(L"InputOverlay", L"LabelShadow", OverlayServer_GetLabelShadowPercent(), path);
    ok &= IniWriteU32(L"InputOverlay", L"LabelColor", OverlayServer_GetLabelColor(), path);
    return ok;
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

static bool KeySettingsIni_SaveToSettingsIni(const wchar_t* path)
{
    bool ok = true;
    // rewrite the whole section
    ok &= WritePrivateProfileStringW(L"KeyDeadzone", nullptr, nullptr, path) != FALSE;

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

        ok &= IniWriteI32(L"KeyDeadzone", kUse, ks.useUnique ? 1 : 0, path);

        if (ks.invert) ok &= IniWriteI32(L"KeyDeadzone", kInv, 1, path);
        if (ks.curveMode != 0) ok &= IniWriteI32(L"KeyDeadzone", kMode, (int)ks.curveMode, path);

        ok &= IniWriteI32(L"KeyDeadzone", kLow, (int)lroundf(ks.low * 1000.0f), path);
        ok &= IniWriteI32(L"KeyDeadzone", kHigh, (int)lroundf(ks.high * 1000.0f), path);

        if (ks.antiDeadzone > 0.001f)
            ok &= IniWriteI32(L"KeyDeadzone", kADZ, (int)lroundf(ks.antiDeadzone * 1000.0f), path);

        if (ks.outputCap < 0.999f)
            ok &= IniWriteI32(L"KeyDeadzone", kCap, (int)lroundf(ks.outputCap * 1000.0f), path);

        ok &= IniWriteI32(L"KeyDeadzone", kC1X, (int)lroundf(ks.cp1_x * 1000.0f), path);
        ok &= IniWriteI32(L"KeyDeadzone", kC1Y, (int)lroundf(ks.cp1_y * 1000.0f), path);
        ok &= IniWriteI32(L"KeyDeadzone", kC2X, (int)lroundf(ks.cp2_x * 1000.0f), path);
        ok &= IniWriteI32(L"KeyDeadzone", kC2Y, (int)lroundf(ks.cp2_y * 1000.0f), path);

        float w1 = ClampF(ks.cp1_w, 0.0f, 1.0f);
        float w2 = ClampF(ks.cp2_w, 0.0f, 1.0f);
        ok &= IniWriteI32(L"KeyDeadzone", kC1W, (int)lroundf(w1 * 1000.0f), path);
        ok &= IniWriteI32(L"KeyDeadzone", kC2W, (int)lroundf(w2 * 1000.0f), path);
    }
    return ok;
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
        int higM = GetPrivateProfileIntW(L"KeyDeadzone", kHigh, 900, path);

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

static bool SettingsIni_Load_Core(const wchar_t* path, bool loadWindow, bool loadLayout, bool loadActiveProfileKey)
{
    if (!path) return false;

    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;

    const bool profileOnly = (!loadWindow && !loadLayout && !loadActiveProfileKey);

    // For profile loading, never inherit current runtime values for missing keys.
    // Profile must be self-contained; missing values fall back to stable defaults.
    float lowDef = profileOnly ? 0.08f : Settings_GetInputDeadzoneLow();
    float highDef = profileOnly ? 0.90f : Settings_GetInputDeadzoneHigh();
    float adzDef = profileOnly ? 0.0f : Settings_GetInputAntiDeadzone();
    float capDef = profileOnly ? 1.0f : Settings_GetInputOutputCap();
    float c1xDef = profileOnly ? 0.38f : Settings_GetInputBezierCp1X();
    float c1yDef = profileOnly ? 0.33f : Settings_GetInputBezierCp1Y();
    float c2xDef = profileOnly ? 0.68f : Settings_GetInputBezierCp2X();
    float c2yDef = profileOnly ? 0.66f : Settings_GetInputBezierCp2Y();
    float c1wDef = profileOnly ? 1.0f : Settings_GetInputBezierCp1W();
    float c2wDef = profileOnly ? 1.0f : Settings_GetInputBezierCp2W();
    UINT curveModeDef = profileOnly ? 1u : Settings_GetInputCurveMode();
    int invertDef = profileOnly ? 0 : (Settings_GetInputInvert() ? 1 : 0);
    int snappyDef = profileOnly ? 0 : (Settings_GetSnappyJoystick() ? 1 : 0);
    int lkpDef = profileOnly ? 0 : (Settings_GetLastKeyPriority() ? 1 : 0);
    float lkpSensDef = profileOnly ? 0.12f : Settings_GetLastKeyPrioritySensitivity();
    int blockDef = profileOnly ? 0 : (Settings_GetBlockBoundKeys() ? 1 : 0);
    int blockMouseDef = profileOnly ? 0 : (Settings_GetBlockMouseInput() ? 1 : 0);
    UINT pollDef = profileOnly ? 1u : Settings_GetPollingMs();
    UINT uiDef = profileOnly ? 1u : Settings_GetUIRefreshMs();
    int padsDef = profileOnly ? 1 : Settings_GetVirtualGamepadCount();
    int padsEnabledDef = profileOnly ? 1 : (Settings_GetVirtualGamepadsEnabled() ? 1 : 0);
    int fallbackDef = profileOnly ? 0 : (Settings_GetDigitalFallbackInput() ? 1 : 0);
    UINT sparkPollModeDef = profileOnly ? 0u : Settings_GetSparkPollMode();
    UINT sparkRowLimitDef = profileOnly ? 0u : Settings_GetSparkRowLimit();
    int mouseToStickEnabledDef = profileOnly ? 0 : (Settings_GetMouseToStickEnabled() ? 1 : 0);
    int mouseToStickTargetDef = profileOnly ? 1 : Settings_GetMouseToStickTarget();
    float mouseToStickSensDef = profileOnly ? 1.0f : Settings_GetMouseToStickSensitivity();
    float mouseToStickAggDef = profileOnly ? 1.0f : Settings_GetMouseToStickAggressiveness();
    float mouseToStickMaxOffsetDef = profileOnly ? 2.5f : Settings_GetMouseToStickMaxOffset();
    float mouseToStickFollowSpeedDef = profileOnly ? 1.0f : Settings_GetMouseToStickFollowSpeed();
    int overlayAutoStartDef = OverlayServer_GetAutoStart() ? 1 : 0;
    int overlayUseRawDepthDef = OverlayServer_GetUseRawDepth() ? 1 : 0;
    UINT overlayPortDef = OverlayServer_GetConfiguredPort();
    UINT overlayFillDirectionDef = (UINT)OverlayServer_GetFillDirection();
    UINT overlayEffectsDef = OverlayServer_GetEffectFlags();
    UINT overlayAccentDef = OverlayServer_GetAccentColor();
    int overlayRefreshDef = OverlayServer_GetRefreshIntervalMs();
    int overlaySmoothDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_Smoothing);
    int overlayGlassDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_Glass);
    int overlayBloomDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_Bloom);
    int overlayEdgeDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_EdgeSweep);
    int overlayScaleDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_MicroScale);
    int overlayLabelDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_LabelContrast);
    int overlayRimLightDef = OverlayServer_GetEffectStrengthPercent(OverlayEffect_GlassRimLight);
    int overlayLabelFontDef = OverlayServer_GetLabelFontIndex();
    int overlayLabelSizeDef = OverlayServer_GetLabelSizePx();
    int overlayLabelShadowDef = OverlayServer_GetLabelShadowPercent();
    UINT overlayLabelColorDef = OverlayServer_GetLabelColor();

    float low = IniReadFloat1000(L"Input", L"DeadzoneLow", lowDef, path);
    float high = IniReadFloat1000(L"Input", L"DeadzoneHigh", highDef, path);

    float adz = IniReadFloat1000(L"Input", L"AntiDeadzone", adzDef, path);
    float cap = IniReadFloat1000(L"Input", L"OutputCap", capDef, path);

    float c1x = IniReadFloat1000(L"Input", L"Cp1X", c1xDef, path);
    float c1y = IniReadFloat1000(L"Input", L"Cp1Y", c1yDef, path);
    float c2x = IniReadFloat1000(L"Input", L"Cp2X", c2xDef, path);
    float c2y = IniReadFloat1000(L"Input", L"Cp2Y", c2yDef, path);

    float c1w = IniReadFloat1000(L"Input", L"Cp1W", c1wDef, path);
    float c2w = IniReadFloat1000(L"Input", L"Cp2W", c2wDef, path);

    UINT curveMode = IniReadU32(L"Input", L"CurveMode", curveModeDef, path);
    int invert = GetPrivateProfileIntW(L"Input", L"Invert", invertDef, path);
    int snappy = GetPrivateProfileIntW(L"Input", L"SnappyJoystick", snappyDef, path);
    int lastKeyPriority = GetPrivateProfileIntW(L"Input", L"LastKeyPriority", lkpDef, path);
    float lastKeyPrioritySensitivity = IniReadFloat1000(
        L"Input", L"LastKeyPrioritySensitivity",
        lkpSensDef, path);
    int blockBoundKeys = GetPrivateProfileIntW(L"Input", L"BlockBoundKeys", blockDef, path);
    int blockMouseInput = GetPrivateProfileIntW(L"Input", L"BlockMouseInput", blockMouseDef, path);

    UINT poll = IniReadU32(L"Main", L"PollingMs", pollDef, path);
    UINT uiMs = IniReadU32(L"Main", L"UIRefreshMs", uiDef, path);
    int vpadCount = GetPrivateProfileIntW(L"Main", L"VirtualGamepads", padsDef, path);
    int vpadEnabled = GetPrivateProfileIntW(L"Main", L"VirtualGamepadsEnabled", padsEnabledDef, path);
    int digitalFallbackInput = GetPrivateProfileIntW(L"Main", L"DigitalFallbackInput", fallbackDef, path);
    UINT sparkPollMode = IniReadU32(L"Main", L"SparkPollMode", sparkPollModeDef, path);
    UINT sparkRowLimit = IniReadU32(L"Main", L"SparkRowLimit", sparkRowLimitDef, path);
    int mouseToStickEnabled = GetPrivateProfileIntW(L"Main", L"MouseToStickEnabled", mouseToStickEnabledDef, path);
    int mouseToStickTarget = GetPrivateProfileIntW(L"Main", L"MouseToStickTarget", mouseToStickTargetDef, path);
    float mouseToStickSensitivity = IniReadFloat1000(L"Main", L"MouseToStickSensitivity", mouseToStickSensDef, path);
    float mouseToStickAggressiveness = IniReadFloat1000(L"Main", L"MouseToStickAggressiveness", mouseToStickAggDef, path);
    float mouseToStickMaxOffset = IniReadFloat1000(L"Main", L"MouseToStickMaxOffset", mouseToStickMaxOffsetDef, path);
    float mouseToStickFollowSpeed = IniReadFloat1000(L"Main", L"MouseToStickFollowSpeed", mouseToStickFollowSpeedDef, path);
    int overlayAutoStart = overlayAutoStartDef;
    int overlayUseRawDepth = overlayUseRawDepthDef;
    UINT overlayPort = overlayPortDef;
    UINT overlayFillDirection = overlayFillDirectionDef;
    UINT overlayEffects = overlayEffectsDef;
    UINT overlayAccent = overlayAccentDef;
    int overlayRefresh = overlayRefreshDef;
    int overlaySmooth = overlaySmoothDef;
    int overlayGlass = overlayGlassDef;
    int overlayBloom = overlayBloomDef;
    int overlayEdge = overlayEdgeDef;
    int overlayScale = overlayScaleDef;
    int overlayLabel = overlayLabelDef;
    int overlayRimLight = overlayRimLightDef;
    int overlayLabelFont = overlayLabelFontDef;
    int overlayLabelSize = overlayLabelSizeDef;
    int overlayLabelShadow = overlayLabelShadowDef;
    UINT overlayLabelColor = overlayLabelColorDef;
    if (!profileOnly)
    {
        int overlayStrengthScaleVersion = GetPrivateProfileIntW(L"InputOverlay", L"StrengthScaleVersion", 0, path);
        overlayAutoStart = GetPrivateProfileIntW(L"InputOverlay", L"AutoStart", overlayAutoStartDef, path);
        overlayUseRawDepth = GetPrivateProfileIntW(L"InputOverlay", L"UseRawDepth", overlayUseRawDepthDef, path);
        overlayPort = IniReadU32(L"InputOverlay", L"Port", overlayPortDef, path);
        overlayFillDirection = IniReadU32(L"InputOverlay", L"FillDirection", overlayFillDirectionDef, path);
        overlayEffects = IniReadU32(L"InputOverlay", L"EffectFlags", overlayEffectsDef, path);
        overlayAccent = IniReadU32(L"InputOverlay", L"AccentColor", overlayAccentDef, path);
        overlayRefresh = IniReadI32(L"InputOverlay", L"RefreshMs", overlayRefreshDef, path);
        overlaySmooth = IniReadI32(L"InputOverlay", L"StrengthSmoothing", overlaySmoothDef, path);
        overlayGlass = IniReadI32(L"InputOverlay", L"StrengthGlass", overlayGlassDef, path);
        overlayBloom = IniReadI32(L"InputOverlay", L"StrengthBloom", overlayBloomDef, path);
        overlayEdge = IniReadI32(L"InputOverlay", L"StrengthEdgeSweep", overlayEdgeDef, path);
        overlayScale = IniReadI32(L"InputOverlay", L"StrengthMicroScale", overlayScaleDef, path);
        overlayLabel = IniReadI32(L"InputOverlay", L"StrengthLabelContrast", overlayLabelDef, path);
        overlayRimLight = IniReadI32(L"InputOverlay", L"StrengthGlassRimLight", overlayRimLightDef, path);
        overlayLabelFont = IniReadI32(L"InputOverlay", L"LabelFont", overlayLabelFontDef, path);
        overlayLabelSize = IniReadI32(L"InputOverlay", L"LabelSizePx", overlayLabelSizeDef, path);
        overlayLabelShadow = IniReadI32(L"InputOverlay", L"LabelShadow", overlayLabelShadowDef, path);
        overlayLabelColor = IniReadU32(L"InputOverlay", L"LabelColor", overlayLabelColorDef, path);
        if (overlayStrengthScaleVersion < 2)
        {
            overlayGlass = OverlayClampStrengthPercent(overlayGlass);
            overlayBloom = OverlayLegacyStrengthToNormalized(overlayBloom, 204.0f);
            overlayEdge = OverlayLegacyStrengthToNormalized(overlayEdge, 33.0f);
            overlayScale = OverlayLegacyStrengthToNormalized(overlayScale, 269.0f);
            overlayLabel = OverlayClampStrengthPercent(overlayLabel);
            overlayRimLight = overlayRimLightDef;
        }
        if (overlayStrengthScaleVersion < 3)
        {
            overlayEffects |= OverlayEffect_GlassRimLight;
            overlayRimLight = overlayRimLightDef;
        }
        if (overlayStrengthScaleVersion == 3)
        {
            overlayRimLight = OverlayLegacyRimLightToNormalized(overlayRimLight);
        }
        if (overlayStrengthScaleVersion < 5)
        {
            overlayGlass = OverlayLegacyGlassToMaterialScale(overlayGlass);
        }
    }
    int winW = Settings_GetMainWindowWidthPx();
    int winH = Settings_GetMainWindowHeightPx();
    int winX = std::numeric_limits<int>::min();
    int winY = std::numeric_limits<int>::min();
    if (loadWindow)
    {
        winW = GetPrivateProfileIntW(L"Window", L"Width", Settings_GetMainWindowWidthPx(), path);
        winH = GetPrivateProfileIntW(L"Window", L"Height", Settings_GetMainWindowHeightPx(), path);
        winX = IniReadI32(L"Window", L"PosX", std::numeric_limits<int>::min(), path);
        winY = IniReadI32(L"Window", L"PosY", std::numeric_limits<int>::min(), path);
    }

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
    Settings_SetLastKeyPriority(lastKeyPriority != 0);
    Settings_SetLastKeyPrioritySensitivity(lastKeyPrioritySensitivity);
    Settings_SetBlockBoundKeys(blockBoundKeys != 0);
    Settings_SetBlockMouseInput(blockMouseInput != 0);

    Settings_SetPollingMs(poll);
    Settings_SetUIRefreshMs(uiMs);
    Settings_SetVirtualGamepadCount(vpadCount);
    Settings_SetVirtualGamepadsEnabled(vpadEnabled != 0);
    Settings_SetDigitalFallbackInput(digitalFallbackInput != 0);
    Settings_SetSparkPollMode(sparkPollMode);
    Settings_SetSparkRowLimit(sparkRowLimit);
    Settings_SetMouseToStickEnabled(mouseToStickEnabled != 0);
    Settings_SetMouseToStickTarget(mouseToStickTarget);
    Settings_SetMouseToStickSensitivity(mouseToStickSensitivity);
    Settings_SetMouseToStickAggressiveness(mouseToStickAggressiveness);
    Settings_SetMouseToStickMaxOffset(mouseToStickMaxOffset);
    Settings_SetMouseToStickFollowSpeed(mouseToStickFollowSpeed);
    if (!profileOnly)
    {
        OverlayServer_SetAutoStart(overlayAutoStart != 0);
        OverlayServer_SetUseRawDepth(overlayUseRawDepth != 0);
        OverlayServer_SetConfiguredPort((uint16_t)std::clamp<UINT>(overlayPort, 1u, 65535u));
        OverlayServer_SetFillDirection(overlayFillDirection == (UINT)OverlayFillDirection::TopDown
            ? OverlayFillDirection::TopDown
            : OverlayFillDirection::BottomUp);
        OverlayServer_SetEffectFlags(overlayEffects);
        OverlayServer_SetAccentColor(overlayAccent);
        OverlayServer_SetRefreshIntervalMs(overlayRefresh);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_Smoothing, overlaySmooth);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_Glass, overlayGlass);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_Bloom, overlayBloom);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_EdgeSweep, overlayEdge);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_MicroScale, overlayScale);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_LabelContrast, overlayLabel);
        OverlayServer_SetEffectStrengthPercent(OverlayEffect_GlassRimLight, overlayRimLight);
        OverlayServer_SetLabelFontIndex(overlayLabelFont);
        OverlayServer_SetLabelSizePx(overlayLabelSize);
        OverlayServer_SetLabelShadowPercent(overlayLabelShadow);
        OverlayServer_SetLabelColor(overlayLabelColor);
    }
    if (loadWindow)
    {
        Settings_SetMainWindowWidthPx(winW);
        Settings_SetMainWindowHeightPx(winH);
        Settings_SetMainWindowPosXPx(winX);
        Settings_SetMainWindowPosYPx(winY);
    }

    if (loadActiveProfileKey)
        GlobalProfiles_InitFromSettingsIni(path);

    KeySettingsIni_LoadFromSettingsIni(path);
    if (loadLayout)
        KeyboardLayout_LoadFromIni(path);
    return true;
}

bool SettingsIni_Load(const wchar_t* path)
{
    return SettingsIni_Load_Core(path, true, true, true);
}

bool SettingsIni_LoadProfile(const wchar_t* path)
{
    return SettingsIni_Load_Core(path, false, false, false);
}

// Writes ONLY application settings (settings.ini).
// Curve presets are stored separately by KeyboardProfiles (CurvePresets folder).
static bool SettingsIni_Save_Internal(
    const wchar_t* tmpPath,
    bool saveWindow,
    bool saveLayout,
    bool saveActiveProfileKey,
    const wchar_t* persistenceKind)
{
    if (!tmpPath) return false;
    bool ok = true;

    ok &= IniWriteI32(L"HallJoyPersistence", L"SchemaVersion", 1, tmpPath);
    ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", persistenceKind, tmpPath) != FALSE;

    ok &= IniWriteFloat1000(L"Input", L"DeadzoneLow", Settings_GetInputDeadzoneLow(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"DeadzoneHigh", Settings_GetInputDeadzoneHigh(), tmpPath);

    ok &= IniWriteFloat1000(L"Input", L"AntiDeadzone", Settings_GetInputAntiDeadzone(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"OutputCap", Settings_GetInputOutputCap(), tmpPath);

    ok &= IniWriteFloat1000(L"Input", L"Cp1X", Settings_GetInputBezierCp1X(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"Cp1Y", Settings_GetInputBezierCp1Y(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"Cp2X", Settings_GetInputBezierCp2X(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"Cp2Y", Settings_GetInputBezierCp2Y(), tmpPath);

    ok &= IniWriteFloat1000(L"Input", L"Cp1W", Settings_GetInputBezierCp1W(), tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"Cp2W", Settings_GetInputBezierCp2W(), tmpPath);

    ok &= IniWriteU32(L"Input", L"CurveMode", Settings_GetInputCurveMode(), tmpPath);
    ok &= IniWriteI32(L"Input", L"Invert", Settings_GetInputInvert() ? 1 : 0, tmpPath);
    ok &= IniWriteI32(L"Input", L"SnappyJoystick", Settings_GetSnappyJoystick() ? 1 : 0, tmpPath);
    ok &= IniWriteI32(L"Input", L"LastKeyPriority", Settings_GetLastKeyPriority() ? 1 : 0, tmpPath);
    ok &= IniWriteFloat1000(L"Input", L"LastKeyPrioritySensitivity", Settings_GetLastKeyPrioritySensitivity(), tmpPath);
    ok &= IniWriteI32(L"Input", L"BlockBoundKeys", Settings_GetBlockBoundKeys() ? 1 : 0, tmpPath);
    ok &= IniWriteI32(L"Input", L"BlockMouseInput", Settings_GetBlockMouseInput() ? 1 : 0, tmpPath);

    ok &= IniWriteU32(L"Main", L"PollingMs", Settings_GetPollingMs(), tmpPath);
    ok &= IniWriteU32(L"Main", L"UIRefreshMs", Settings_GetUIRefreshMs(), tmpPath);
    ok &= IniWriteI32(L"Main", L"VirtualGamepads", std::clamp(Settings_GetVirtualGamepadCount(), 1, 4), tmpPath);
    ok &= IniWriteI32(L"Main", L"VirtualGamepadsEnabled", Settings_GetVirtualGamepadsEnabled() ? 1 : 0, tmpPath);
    ok &= IniWriteI32(L"Main", L"DigitalFallbackInput", Settings_GetDigitalFallbackInput() ? 1 : 0, tmpPath);
    ok &= IniWriteU32(L"Main", L"SparkPollMode", Settings_GetSparkPollMode(), tmpPath);
    ok &= IniWriteU32(L"Main", L"SparkRowLimit", Settings_GetSparkRowLimit(), tmpPath);
    ok &= WritePrivateProfileStringW(L"Main", L"SparkMissedHidDebug", nullptr, tmpPath) != FALSE;
    ok &= WritePrivateProfileStringW(L"Main", L"SparkTelemetryDebug", nullptr, tmpPath) != FALSE;
    ok &= WritePrivateProfileStringW(L"Main", L"SparkExperimentFlags", nullptr, tmpPath) != FALSE;
    ok &= WritePrivateProfileStringW(L"Main", L"VendorProtocolMode", nullptr, tmpPath) != FALSE;
    ok &= IniWriteI32(L"Main", L"MouseToStickEnabled", Settings_GetMouseToStickEnabled() ? 1 : 0, tmpPath);
    ok &= IniWriteI32(L"Main", L"MouseToStickTarget", Settings_GetMouseToStickTarget(), tmpPath);
    ok &= IniWriteFloat1000(L"Main", L"MouseToStickSensitivity", Settings_GetMouseToStickSensitivity(), tmpPath);
    ok &= IniWriteFloat1000(L"Main", L"MouseToStickAggressiveness", Settings_GetMouseToStickAggressiveness(), tmpPath);
    ok &= IniWriteFloat1000(L"Main", L"MouseToStickMaxOffset", Settings_GetMouseToStickMaxOffset(), tmpPath);
    ok &= IniWriteFloat1000(L"Main", L"MouseToStickFollowSpeed", Settings_GetMouseToStickFollowSpeed(), tmpPath);
    if (saveWindow)
        ok &= OverlaySettingsIni_SaveToSettingsIni(tmpPath);
    if (saveActiveProfileKey)
        ok &= WritePrivateProfileStringW(L"Main", L"ActiveGlobalProfile", GlobalProfiles_GetActiveName().c_str(), tmpPath) != FALSE;

    if (saveWindow)
    {
        ok &= IniWriteI32(L"Window", L"Width", std::max(0, Settings_GetMainWindowWidthPx()), tmpPath);
        ok &= IniWriteI32(L"Window", L"Height", std::max(0, Settings_GetMainWindowHeightPx()), tmpPath);
        const int winX = Settings_GetMainWindowPosXPx();
        const int winY = Settings_GetMainWindowPosYPx();
        if (winX == std::numeric_limits<int>::min())
            ok &= WritePrivateProfileStringW(L"Window", L"PosX", nullptr, tmpPath) != FALSE;
        else
            ok &= IniWriteI32(L"Window", L"PosX", winX, tmpPath);
        if (winY == std::numeric_limits<int>::min())
            ok &= WritePrivateProfileStringW(L"Window", L"PosY", nullptr, tmpPath) != FALSE;
        else
            ok &= IniWriteI32(L"Window", L"PosY", winY, tmpPath);
    }

    ok &= KeySettingsIni_SaveToSettingsIni(tmpPath);
    if (saveLayout)
        ok &= KeyboardLayout_SaveToIni(tmpPath);
    return ok;
}

namespace
{
    enum class SettingsTransactionKind
    {
        FullSettings,
        ProfileSettings,
        OverlayUpdate,
    };

    struct SettingsTransactionContext
    {
        const wchar_t* destinationPath = nullptr;
        SettingsTransactionKind kind = SettingsTransactionKind::FullSettings;
    };

    const wchar_t* PersistenceKindName(SettingsTransactionKind kind)
    {
        return kind == SettingsTransactionKind::ProfileSettings ? L"ProfileSettings" : L"Settings";
    }

    bool SettingsTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<SettingsTransactionContext*>(rawContext);
        bool ok = false;
        if (context->kind == SettingsTransactionKind::OverlayUpdate)
        {
            if (!IniUtil_CopyExistingForUpdate(context->destinationPath, temporaryPath, errorOut))
                return false;
            ok = IniWriteI32(L"HallJoyPersistence", L"SchemaVersion", 1, temporaryPath);
            ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"Settings", temporaryPath) != FALSE;
            ok &= OverlaySettingsIni_SaveToSettingsIni(temporaryPath);
        }
        else
        {
            const bool full = context->kind == SettingsTransactionKind::FullSettings;
            ok = SettingsIni_Save_Internal(
                temporaryPath,
                full,
                full,
                full,
                PersistenceKindName(context->kind));
        }

        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    bool ReadExpectedIniValue(
        const wchar_t* path,
        const wchar_t* section,
        const wchar_t* key,
        const wchar_t* expected)
    {
        wchar_t value[128]{};
        GetPrivateProfileStringW(section, key, L"{missing}", value, (DWORD)_countof(value), path);
        return wcscmp(value, expected) == 0;
    }

    bool SettingsTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<SettingsTransactionContext*>(rawContext);
        bool ok = ReadExpectedIniValue(temporaryPath, L"HallJoyPersistence", L"SchemaVersion", L"1") &&
            ReadExpectedIniValue(
                temporaryPath,
                L"HallJoyPersistence",
                L"Kind",
                PersistenceKindName(context->kind));

        if (context->kind == SettingsTransactionKind::OverlayUpdate)
        {
            ok &= ReadExpectedIniValue(temporaryPath, L"InputOverlay", L"StrengthScaleVersion", L"5");
            wchar_t refresh[64]{};
            GetPrivateProfileStringW(L"InputOverlay", L"RefreshMs", L"{missing}", refresh, (DWORD)_countof(refresh), temporaryPath);
            ok &= wcscmp(refresh, L"{missing}") != 0;
        }
        else
        {
            wchar_t polling[64]{};
            wchar_t deadzone[64]{};
            GetPrivateProfileStringW(L"Main", L"PollingMs", L"{missing}", polling, (DWORD)_countof(polling), temporaryPath);
            GetPrivateProfileStringW(L"Input", L"DeadzoneLow", L"{missing}", deadzone, (DWORD)_countof(deadzone), temporaryPath);
            ok &= wcscmp(polling, L"{missing}") != 0 && wcscmp(deadzone, L"{missing}") != 0;
        }

        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }

    bool SaveSettingsTransaction(const wchar_t* path, SettingsTransactionKind kind, const wchar_t* displayKind)
    {
        if (!path || !*path) return false;
        SettingsTransactionContext context{ path, kind };
        const auto result = IniUtil_SaveAtomic(path, SettingsTransactionWrite, SettingsTransactionValidate, &context);
        if (!result.Succeeded())
        {
            IniUtil_ReportSaveFailure(displayKind, path, result);
            return false;
        }
        return true;
    }
}

bool SettingsIni_Save(const wchar_t* path)
{
    return SaveSettingsTransaction(path, SettingsTransactionKind::FullSettings, L"settings");
}

bool SettingsIni_SaveProfile(const wchar_t* path)
{
    return SaveSettingsTransaction(path, SettingsTransactionKind::ProfileSettings, L"profile settings");
}

bool SettingsIni_SaveOverlay(const wchar_t* path)
{
    return SaveSettingsTransaction(path, SettingsTransactionKind::OverlayUpdate, L"overlay settings");
}
