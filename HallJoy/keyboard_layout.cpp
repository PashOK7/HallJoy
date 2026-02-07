#include "keyboard_layout.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace
{
    struct PresetDef
    {
        const wchar_t* name;
        const KeyDef* keys;
        int count;
    };

    static const KeyDef g_a75Keys[] =
    {
        {L"Esc", 41, 0,   0, 42},
        {L"F1",  58, 0,  48, 46},
        {L"F2",  59, 0, 100, 46},
        {L"F3",  60, 0, 152, 46},
        {L"F4",  61, 0, 204, 46},
        {L"F5",  62, 0, 256, 46},
        {L"F6",  63, 0, 308, 46},
        {L"F7",  64, 0, 360, 46},
        {L"F8",  65, 0, 412, 46},
        {L"F9",  66, 0, 464, 46},
        {L"F10", 67, 0, 516, 46},
        {L"F11", 68, 0, 568, 46},
        {L"F12", 69, 0, 620, 46},
        {L"Del", 76, 0, 672, 42},

        {L"`",   53, 1,   0, 42},
        {L"1",   30, 1,  48, 42},
        {L"2",   31, 1,  96, 42},
        {L"3",   32, 1, 144, 42},
        {L"4",   33, 1, 192, 42},
        {L"5",   34, 1, 240, 42},
        {L"6",   35, 1, 288, 42},
        {L"7",   36, 1, 336, 42},
        {L"8",   37, 1, 384, 42},
        {L"9",   38, 1, 432, 42},
        {L"0",   39, 1, 480, 42},
        {L"-",   45, 1, 528, 42},
        {L"=",   46, 1, 576, 42},
        {L"Back",42, 1, 624, 90},
        {L"Home",74, 1, 720, 50},

        {L"Tab", 43, 2,   0, 74},
        {L"Q",   20, 2,  80, 42},
        {L"W",   26, 2, 128, 42},
        {L"E",    8, 2, 176, 42},
        {L"R",   21, 2, 224, 42},
        {L"T",   23, 2, 272, 42},
        {L"Y",   28, 2, 320, 42},
        {L"U",   24, 2, 368, 42},
        {L"I",   12, 2, 416, 42},
        {L"O",   18, 2, 464, 42},
        {L"P",   19, 2, 512, 42},
        {L"[",   47, 2, 560, 42},
        {L"]",   48, 2, 608, 42},
        {L"\\",  49, 2, 656, 58},
        {L"PgUp",75, 2, 720, 50},

        {L"Caps",57, 3,   0, 84},
        {L"A",    4, 3,  90, 42},
        {L"S",   22, 3, 138, 42},
        {L"D",    7, 3, 186, 42},
        {L"F",    9, 3, 234, 42},
        {L"G",   10, 3, 282, 42},
        {L"H",   11, 3, 330, 42},
        {L"J",   13, 3, 378, 42},
        {L"K",   14, 3, 426, 42},
        {L"L",   15, 3, 474, 42},
        {L";",   51, 3, 522, 42},
        {L"'",   52, 3, 570, 42},
        {L"Enter",40,3, 618, 94},
        {L"PgDn",78, 3, 720, 50},

        {L"Shift",225,4,   0,106},
        {L"Z",     29,4, 112,42},
        {L"X",     27,4, 160,42},
        {L"C",      6,4, 208,42},
        {L"V",     25,4, 256,42},
        {L"B",      5,4, 304,42},
        {L"N",     17,4, 352,42},
        {L"M",     16,4, 400,42},
        {L",",     54,4, 448,42},
        {L".",     55,4, 496,42},
        {L"/",     56,4, 544,42},
        {L"Shift",229,4, 592,74},
        {L"Up",    82,4, 672,42},
        {L"End",   77,4, 720,50},

        {L"Ctrl", 224,5,   0,54},
        {L"Win",  227,5,  60,54},
        {L"Alt",  226,5, 120,54},
        {L"Space", 44,5, 180,294},
        {L"Alt",  230,5, 480,42},
        {L"FN",     0,5, 528,42},
        {L"FN2",    0,5, 576,42},
        {L"Left",  80,5, 624,42},
        {L"Down",  81,5, 672,42},
        {L"Right", 79,5, 720,42},
    };

    static std::array<KeyDef, sizeof(g_a75Keys) / sizeof(g_a75Keys[0])> g_generic75Keys = [] {
        std::array<KeyDef, sizeof(g_a75Keys) / sizeof(g_a75Keys[0])> out{};
        for (size_t i = 0; i < out.size(); ++i) out[i] = g_a75Keys[i];

        // Slightly tighter navigation block to offer a second preset option.
        for (auto& k : out)
        {
            if (k.hid == 74 || k.hid == 75 || k.hid == 76 || k.hid == 77 || k.hid == 78)
                k.x -= 20;
        }
        return out;
    }();

    static const PresetDef g_presets[] =
    {
        { L"DrunkDeer A75 Pro", g_a75Keys, (int)(sizeof(g_a75Keys) / sizeof(g_a75Keys[0])) },
        { L"Generic 75% ANSI", g_generic75Keys.data(), (int)g_generic75Keys.size() },
    };

    static std::vector<KeyDef> g_activeKeys;
    static std::vector<std::wstring> g_ownedLabels;
    static int g_currentPresetIdx = 0;
    static bool g_customEdited = false;

    static int ClampPreset(int idx)
    {
        int n = (int)(sizeof(g_presets) / sizeof(g_presets[0]));
        if (n <= 0) return 0;
        if (idx < 0) return 0;
        if (idx >= n) return n - 1;
        return idx;
    }

    static void EnsureInit()
    {
        if (!g_activeKeys.empty()) return;
        g_currentPresetIdx = ClampPreset(g_currentPresetIdx);
        const PresetDef& p = g_presets[g_currentPresetIdx];
        g_activeKeys.assign(p.keys, p.keys + p.count);
        g_ownedLabels.clear();
        g_customEdited = false;
    }

    static void SetCustomActive(std::vector<KeyDef>& src, std::vector<std::wstring>& labels)
    {
        g_ownedLabels = std::move(labels);
        g_activeKeys = std::move(src);
        for (size_t i = 0; i < g_activeKeys.size() && i < g_ownedLabels.size(); ++i)
            g_activeKeys[i].label = g_ownedLabels[i].c_str();
        g_customEdited = true;
    }

    static int IniReadInt(const wchar_t* section, const wchar_t* key, int def, const wchar_t* path)
    {
        return GetPrivateProfileIntW(section, key, def, path);
    }
}

int KeyboardLayout_Count()
{
    EnsureInit();
    return (int)g_activeKeys.size();
}

const KeyDef* KeyboardLayout_Data()
{
    EnsureInit();
    return g_activeKeys.empty() ? nullptr : g_activeKeys.data();
}

int KeyboardLayout_GetPresetCount()
{
    return (int)(sizeof(g_presets) / sizeof(g_presets[0]));
}

const wchar_t* KeyboardLayout_GetPresetName(int idx)
{
    idx = ClampPreset(idx);
    return g_presets[idx].name;
}

int KeyboardLayout_GetCurrentPresetIndex()
{
    EnsureInit();
    return g_currentPresetIdx;
}

void KeyboardLayout_SetPresetIndex(int idx)
{
    EnsureInit();
    idx = ClampPreset(idx);
    g_currentPresetIdx = idx;
    const PresetDef& p = g_presets[idx];
    g_activeKeys.assign(p.keys, p.keys + p.count);
    g_ownedLabels.clear();
    g_customEdited = false;
}

void KeyboardLayout_ResetActiveToPreset()
{
    KeyboardLayout_SetPresetIndex(g_currentPresetIdx);
}

bool KeyboardLayout_SetKeyGeometry(int idx, int row, int x, int w)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;

    KeyDef& k = g_activeKeys[idx];
    k.row = std::clamp(row, 0, 20);
    k.x = std::clamp(x, 0, 4000);
    k.w = std::clamp(w, 18, 600);
    g_customEdited = true;
    return true;
}

bool KeyboardLayout_GetKey(int idx, KeyDef& out)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;
    out = g_activeKeys[idx];
    return true;
}

bool KeyboardLayout_LoadFromIni(const wchar_t* path)
{
    if (!path) return false;
    EnsureInit();

    int preset = IniReadInt(L"KeyboardLayout", L"Preset", g_currentPresetIdx, path);
    KeyboardLayout_SetPresetIndex(preset);

    int custom = IniReadInt(L"KeyboardLayout", L"Custom", 0, path);
    int count = IniReadInt(L"KeyboardLayout", L"Count", 0, path);

    if (!custom || count <= 0)
        return true;

    std::vector<KeyDef> loaded;
    std::vector<std::wstring> labels;
    loaded.reserve((size_t)count);
    labels.reserve((size_t)count);

    for (int i = 0; i < count; ++i)
    {
        wchar_t key[64]{};

        swprintf_s(key, L"K%d_Hid", i);
        int hid = IniReadInt(L"KeyboardLayout", key, -1, path);

        swprintf_s(key, L"K%d_Row", i);
        int row = IniReadInt(L"KeyboardLayout", key, 0, path);

        swprintf_s(key, L"K%d_X", i);
        int x = IniReadInt(L"KeyboardLayout", key, 0, path);

        swprintf_s(key, L"K%d_W", i);
        int w = IniReadInt(L"KeyboardLayout", key, 42, path);

        swprintf_s(key, L"K%d_Label", i);
        wchar_t labelBuf[64]{};
        GetPrivateProfileStringW(L"KeyboardLayout", key, L"Key", labelBuf, (DWORD)(sizeof(labelBuf) / sizeof(labelBuf[0])), path);

        if (hid < 0) continue;

        labels.emplace_back(labelBuf);

        KeyDef kd{};
        kd.hid = (uint16_t)std::clamp(hid, 0, 65535);
        kd.row = std::clamp(row, 0, 20);
        kd.x = std::clamp(x, 0, 4000);
        kd.w = std::clamp(w, 18, 600);
        kd.label = nullptr;

        loaded.push_back(kd);
    }

    if (!loaded.empty())
        SetCustomActive(loaded, labels);

    return true;
}

void KeyboardLayout_SaveToIni(const wchar_t* path)
{
    if (!path) return;
    EnsureInit();

    WritePrivateProfileStringW(L"KeyboardLayout", nullptr, nullptr, path);

    wchar_t v[64]{};
    swprintf_s(v, L"%d", g_currentPresetIdx);
    WritePrivateProfileStringW(L"KeyboardLayout", L"Preset", v, path);

    swprintf_s(v, L"%d", g_customEdited ? 1 : 0);
    WritePrivateProfileStringW(L"KeyboardLayout", L"Custom", v, path);

    swprintf_s(v, L"%d", (int)g_activeKeys.size());
    WritePrivateProfileStringW(L"KeyboardLayout", L"Count", v, path);

    for (int i = 0; i < (int)g_activeKeys.size(); ++i)
    {
        const KeyDef& k = g_activeKeys[i];
        wchar_t key[64]{};

        swprintf_s(key, L"K%d_Hid", i);
        swprintf_s(v, L"%u", (unsigned)k.hid);
        WritePrivateProfileStringW(L"KeyboardLayout", key, v, path);

        swprintf_s(key, L"K%d_Row", i);
        swprintf_s(v, L"%d", k.row);
        WritePrivateProfileStringW(L"KeyboardLayout", key, v, path);

        swprintf_s(key, L"K%d_X", i);
        swprintf_s(v, L"%d", k.x);
        WritePrivateProfileStringW(L"KeyboardLayout", key, v, path);

        swprintf_s(key, L"K%d_W", i);
        swprintf_s(v, L"%d", k.w);
        WritePrivateProfileStringW(L"KeyboardLayout", key, v, path);

        swprintf_s(key, L"K%d_Label", i);
        WritePrivateProfileStringW(L"KeyboardLayout", key, k.label ? k.label : L"", path);
    }
}
