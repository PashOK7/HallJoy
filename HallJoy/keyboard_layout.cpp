#include "keyboard_layout.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <cwctype>

#include "win_util.h"

namespace fs = std::filesystem;

namespace
{
    struct PresetDef
    {
        const wchar_t* name;
        const KeyDef* keys;
        int count;
    };

    struct PresetStore
    {
        std::wstring name;
        std::wstring filePath;
        std::vector<KeyDef> keys;
        std::vector<std::wstring> labels;
        bool uniformSpacing = false;
        int uniformGap = 8;
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

    static const KeyDef g_generic100Keys[] =
    {
        {L"Esc",   41, 0,    0, 42},
        {L"F1",    58, 0,   64, 46},
        {L"F2",    59, 0,  116, 46},
        {L"F3",    60, 0,  168, 46},
        {L"F4",    61, 0,  220, 46},
        {L"F5",    62, 0,  290, 46},
        {L"F6",    63, 0,  342, 46},
        {L"F7",    64, 0,  394, 46},
        {L"F8",    65, 0,  446, 46},
        {L"F9",    66, 0,  512, 46},
        {L"F10",   67, 0,  564, 46},
        {L"F11",   68, 0,  616, 46},
        {L"F12",   69, 0,  668, 46},
        {L"PrtSc", 70, 0,  732, 42},
        {L"ScrLk", 71, 0,  780, 42},
        {L"Pause", 72, 0,  828, 42},

        {L"`",     53, 1,    0, 42},
        {L"1",     30, 1,   48, 42},
        {L"2",     31, 1,   96, 42},
        {L"3",     32, 1,  144, 42},
        {L"4",     33, 1,  192, 42},
        {L"5",     34, 1,  240, 42},
        {L"6",     35, 1,  288, 42},
        {L"7",     36, 1,  336, 42},
        {L"8",     37, 1,  384, 42},
        {L"9",     38, 1,  432, 42},
        {L"0",     39, 1,  480, 42},
        {L"-",     45, 1,  528, 42},
        {L"=",     46, 1,  576, 42},
        {L"Back",  42, 1,  624, 90},
        {L"Ins",   73, 1,  732, 42},
        {L"Home",  74, 1,  780, 42},
        {L"PgUp",  75, 1,  828, 42},
        {L"Num",   83, 1,  900, 42},
        {L"/",     84, 1,  948, 42},
        {L"*",     85, 1,  996, 42},
        {L"-",     86, 1, 1044, 42},

        {L"Tab",   43, 2,    0, 74},
        {L"Q",     20, 2,   80, 42},
        {L"W",     26, 2,  128, 42},
        {L"E",      8, 2,  176, 42},
        {L"R",     21, 2,  224, 42},
        {L"T",     23, 2,  272, 42},
        {L"Y",     28, 2,  320, 42},
        {L"U",     24, 2,  368, 42},
        {L"I",     12, 2,  416, 42},
        {L"O",     18, 2,  464, 42},
        {L"P",     19, 2,  512, 42},
        {L"[",     47, 2,  560, 42},
        {L"]",     48, 2,  608, 42},
        {L"\\",    49, 2,  656, 58},
        {L"Del",   76, 2,  732, 42},
        {L"End",   77, 2,  780, 42},
        {L"PgDn",  78, 2,  828, 42},
        {L"7",     95, 2,  900, 42},
        {L"8",     96, 2,  948, 42},
        {L"9",     97, 2,  996, 42},
        {L"Num+",  87, 2, 1044, 42, 88},

        {L"Caps",  57, 3,    0, 84},
        {L"A",      4, 3,   90, 42},
        {L"S",     22, 3,  138, 42},
        {L"D",      7, 3,  186, 42},
        {L"F",      9, 3,  234, 42},
        {L"G",     10, 3,  282, 42},
        {L"H",     11, 3,  330, 42},
        {L"J",     13, 3,  378, 42},
        {L"K",     14, 3,  426, 42},
        {L"L",     15, 3,  474, 42},
        {L";",     51, 3,  522, 42},
        {L"'",     52, 3,  570, 42},
        {L"Enter", 40, 3,  618, 96},
        {L"4",     92, 3,  900, 42},
        {L"5",     93, 3,  948, 42},
        {L"6",     94, 3,  996, 42},

        {L"Shift",225, 4,    0,106},
        {L"Z",     29, 4,  112, 42},
        {L"X",     27, 4,  160, 42},
        {L"C",      6, 4,  208, 42},
        {L"V",     25, 4,  256, 42},
        {L"B",      5, 4,  304, 42},
        {L"N",     17, 4,  352, 42},
        {L"M",     16, 4,  400, 42},
        {L",",     54, 4,  448, 42},
        {L".",     55, 4,  496, 42},
        {L"/",     56, 4,  544, 42},
        {L"Shift",229, 4,  592,122},
        {L"Up",    82, 4,  780, 42},
        {L"1",     89, 4,  900, 42},
        {L"2",     90, 4,  948, 42},
        {L"3",     91, 4,  996, 42},
        {L"NEnt",  88, 4, 1044, 42, 88},

        {L"Ctrl", 224, 5,    0, 54},
        {L"Win",  227, 5,   60, 54},
        {L"Alt",  226, 5,  120, 54},
        {L"Space", 44, 5,  180,354},
        {L"Alt",  230, 5,  540, 54},
        {L"Menu", 101, 5,  601, 54},
        {L"Ctrl", 228, 5,  661, 54},
        {L"Left",  80, 5,  732, 42},
        {L"Down",  81, 5,  780, 42},
        {L"Right", 79, 5,  828, 42},
        {L"0",     98, 5,  900, 90},
        {L".",     99, 5,  996, 42},
    };

    static const PresetDef g_builtinPresets[] =
    {
        { L"DrunkDeer A75 Pro", g_a75Keys, (int)(sizeof(g_a75Keys) / sizeof(g_a75Keys[0])) },
        { L"Generic 100% ANSI", g_generic100Keys, (int)(sizeof(g_generic100Keys) / sizeof(g_generic100Keys[0])) },
    };

    static std::vector<PresetStore> g_presets;
    static std::vector<KeyDef> g_activeKeys;
    static std::vector<KeyDef> g_activeRenderKeys;
    static std::vector<std::wstring> g_ownedLabels;
    static bool g_activeUniformSpacing = false;
    static int g_activeUniformGap = 8;
    static int g_currentPresetIdx = 0;
    static bool g_customEdited = false;

    static int ClampUniformGap(int v);
    static int ClampKeyDim(int v)
    {
        return std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
    }

    static std::wstring SanitizeFileName(const std::wstring& in)
    {
        std::wstring s = in;
        while (!s.empty() && s.front() == L' ') s.erase(s.begin());
        while (!s.empty() && s.back() == L' ') s.pop_back();
        const wchar_t* bad = L"<>:\"/\\|?*";
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (wcschr(bad, s[i]) || s[i] < 32)
                s[i] = L'_';
        }
        while (!s.empty() && (s.back() == L'.' || s.back() == L' ')) s.pop_back();
        if (s.empty()) s = L"Layout";
        return s;
    }

    static std::wstring GetLayoutsDir()
    {
        static std::wstring dir = WinUtil_BuildPathNearExe(L"Layouts");
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }

    static std::wstring BuildPresetPath(const std::wstring& name)
    {
        fs::path p = fs::path(GetLayoutsDir()) / (SanitizeFileName(name) + L".ini");
        return p.wstring();
    }

    static int ClampPreset(int idx)
    {
        int n = (int)g_presets.size();
        if (n <= 0) return 0;
        if (idx < 0) return 0;
        if (idx >= n) return n - 1;
        return idx;
    }

    static bool ParseIntClamped(const std::wstring& text, int minV, int maxV, int& out)
    {
        if (text.empty()) return false;
        wchar_t* end = nullptr;
        long v = wcstol(text.c_str(), &end, 10);
        if (!end || *end != 0) return false;
        out = (int)std::clamp(v, (long)minV, (long)maxV);
        return true;
    }

    static std::wstring EscapePackedField(const wchar_t* s)
    {
        std::wstring out;
        if (!s) return out;
        for (const wchar_t* p = s; *p; ++p)
        {
            const wchar_t c = *p;
            if (c == L'|' || c == L'\\')
            {
                out.push_back(L'\\');
                out.push_back(c);
            }
            else if (c == L'\n')
            {
                out.append(L"\\n");
            }
            else if (c == L'\r')
            {
                out.append(L"\\r");
            }
            else
            {
                out.push_back(c);
            }
        }
        return out;
    }

    static bool ParsePackedKeyEntry(const wchar_t* packed, KeyDef& outKey, std::wstring& outLabel)
    {
        if (!packed || !packed[0]) return false;

        std::vector<std::wstring> f;
        f.emplace_back();
        bool esc = false;
        for (const wchar_t* p = packed; *p; ++p)
        {
            const wchar_t c = *p;
            if (esc)
            {
                if (c == L'n') f.back().push_back(L'\n');
                else if (c == L'r') f.back().push_back(L'\r');
                else f.back().push_back(c);
                esc = false;
                continue;
            }
            if (c == L'\\')
            {
                esc = true;
                continue;
            }
            if (c == L'|')
            {
                f.emplace_back();
                continue;
            }
            f.back().push_back(c);
        }
        if (esc) f.back().push_back(L'\\');

        if (f.size() < 5) return false;

        int hid = -1;
        int row = 0;
        int x = 0;
        int w = 42;
        int h = KEYBOARD_KEY_H;
        if (!ParseIntClamped(f[0], 0, 65535, hid)) return false;
        if (!ParseIntClamped(f[1], 0, 20, row)) return false;
        if (!ParseIntClamped(f[2], 0, 4000, x)) return false;
        if (!ParseIntClamped(f[3], KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM, w)) return false;
        if (f.size() >= 6)
        {
            if (!ParseIntClamped(f[4], KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM, h)) return false;
        }

        outKey = {};
        outKey.hid = (uint16_t)hid;
        outKey.row = row;
        outKey.x = x;
        outKey.w = ClampKeyDim(w);
        outKey.h = ClampKeyDim(h);
        outKey.label = nullptr;
        if (f.size() <= 5)
        {
            outLabel = f[4];
        }
        else
        {
            outLabel = f[5];
            for (size_t i = 6; i < f.size(); ++i)
            {
                outLabel.push_back(L'|');
                outLabel += f[i];
            }
        }
        return true;
    }

    static std::wstring BuildPackedKeyEntry(const KeyDef& k)
    {
        wchar_t head[96]{};
        swprintf_s(head, L"%u|%d|%d|%d|%d|", (unsigned)k.hid, k.row, k.x, k.w, ClampKeyDim(k.h));
        std::wstring out = head;
        out += EscapePackedField(k.label ? k.label : L"");
        return out;
    }

    static bool LoadPresetFile(const wchar_t* path, PresetStore& out)
    {
        if (!path || !path[0]) return false;
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return false;

        int count = GetPrivateProfileIntW(L"LayoutPreset", L"Count", 0, path);
        if (count <= 0) return false;

        std::vector<KeyDef> keys;
        std::vector<std::wstring> labels;
        keys.reserve((size_t)count);
        labels.reserve((size_t)count);

        for (int i = 0; i < count; ++i)
        {
            wchar_t k[64]{};
            swprintf_s(k, L"K%d", i);
            wchar_t packed[1024]{};
            GetPrivateProfileStringW(L"LayoutPreset", k, L"", packed, (DWORD)_countof(packed), path);

            KeyDef kd{};
            std::wstring label;
            if (ParsePackedKeyEntry(packed, kd, label))
            {
                keys.push_back(kd);
                labels.push_back(std::move(label));
            }
        }

        if (keys.empty()) return false;

        out.name = fs::path(path).stem().wstring();
        out.filePath = path;
        out.keys = std::move(keys);
        out.labels = std::move(labels);
        out.uniformSpacing = (GetPrivateProfileIntW(L"LayoutPreset", L"UniformSpacing", 0, path) != 0);
        out.uniformGap = ClampUniformGap(GetPrivateProfileIntW(L"LayoutPreset", L"UniformGap", 8, path));
        for (size_t i = 0; i < out.keys.size() && i < out.labels.size(); ++i)
            out.keys[i].label = out.labels[i].c_str();
        return true;
    }

    static bool SavePresetFile(const PresetStore& p)
    {
        if (p.filePath.empty()) return false;
        fs::path dir = fs::path(p.filePath).parent_path();
        std::error_code ec;
        fs::create_directories(dir, ec);

        WritePrivateProfileStringW(L"LayoutPreset", nullptr, nullptr, p.filePath.c_str());

        wchar_t v[64]{};
        swprintf_s(v, L"%d", (int)p.keys.size());
        WritePrivateProfileStringW(L"LayoutPreset", L"Count", v, p.filePath.c_str());
        WritePrivateProfileStringW(L"LayoutPreset", L"UniformSpacing", p.uniformSpacing ? L"1" : L"0", p.filePath.c_str());
        swprintf_s(v, L"%d", ClampUniformGap(p.uniformGap));
        WritePrivateProfileStringW(L"LayoutPreset", L"UniformGap", v, p.filePath.c_str());

        for (int i = 0; i < (int)p.keys.size(); ++i)
        {
            const KeyDef& k = p.keys[i];
            wchar_t key[64]{};
            swprintf_s(key, L"K%d", i);
            std::wstring packed = BuildPackedKeyEntry(k);
            WritePrivateProfileStringW(L"LayoutPreset", key, packed.c_str(), p.filePath.c_str());
        }
        return true;
    }

    static void EnsureActiveLabelsBound(std::vector<KeyDef>& keys, std::vector<std::wstring>& labels)
    {
        for (size_t i = 0; i < keys.size() && i < labels.size(); ++i)
            keys[i].label = labels[i].c_str();
    }

    static int ClampUniformGap(int v)
    {
        return std::clamp(v, 0, 120);
    }

    static void BuildUniformDisplayX(const std::vector<KeyDef>& keys, int gap, std::vector<int>& outX)
    {
        outX.resize(keys.size());
        for (size_t i = 0; i < keys.size(); ++i)
            outX[i] = keys[i].x;

        for (int row = 0; row <= 20; ++row)
        {
            std::vector<int> ids;
            ids.reserve(keys.size());
            for (int i = 0; i < (int)keys.size(); ++i)
            {
                if (keys[(size_t)i].row == row)
                    ids.push_back(i);
            }
            if (ids.empty()) continue;

            std::sort(ids.begin(), ids.end(), [&](int a, int b)
            {
                if (keys[(size_t)a].x != keys[(size_t)b].x) return keys[(size_t)a].x < keys[(size_t)b].x;
                return a < b;
            });

            int x = keys[(size_t)ids[0]].x;
            for (int id : ids)
            {
                outX[(size_t)id] = x;
                x += keys[(size_t)id].w + gap;
            }
        }
    }

    static void RefreshActiveRenderKeys()
    {
        g_activeRenderKeys = g_activeKeys;
        if (!g_activeUniformSpacing || g_activeRenderKeys.empty())
            return;

        std::vector<int> displayX;
        BuildUniformDisplayX(g_activeRenderKeys, ClampUniformGap(g_activeUniformGap), displayX);
        for (size_t i = 0; i < g_activeRenderKeys.size() && i < displayX.size(); ++i)
            g_activeRenderKeys[i].x = displayX[i];
    }

    static void BindPresetLabels(PresetStore& p)
    {
        if (p.labels.size() < p.keys.size())
        {
            p.labels.resize(p.keys.size());
            for (size_t i = 0; i < p.keys.size(); ++i)
            {
                if (p.labels[i].empty() && p.keys[i].label)
                    p.labels[i] = p.keys[i].label;
            }
        }
        for (size_t i = 0; i < p.keys.size() && i < p.labels.size(); ++i)
            p.keys[i].label = p.labels[i].c_str();
    }

    static void ActivatePreset(int idx)
    {
        idx = ClampPreset(idx);
        g_currentPresetIdx = idx;

        const PresetStore& p = g_presets[idx];
        g_ownedLabels = p.labels;
        g_activeKeys = p.keys;
        g_activeUniformSpacing = p.uniformSpacing;
        g_activeUniformGap = ClampUniformGap(p.uniformGap);
        EnsureActiveLabelsBound(g_activeKeys, g_ownedLabels);
        RefreshActiveRenderKeys();
        g_customEdited = false;
    }

    static int FindPresetByName(const std::wstring& name)
    {
        for (int i = 0; i < (int)g_presets.size(); ++i)
        {
            if (_wcsicmp(g_presets[i].name.c_str(), name.c_str()) == 0)
                return i;
        }
        return -1;
    }

    static void AddOrReplacePreset(const PresetStore& p)
    {
        int idx = FindPresetByName(p.name);
        if (idx >= 0)
        {
            g_presets[idx] = p;
            BindPresetLabels(g_presets[idx]);
        }
        else
        {
            g_presets.push_back(p);
            BindPresetLabels(g_presets.back());
        }
    }

    static void AddBuiltinDefaults()
    {
        for (const auto& b : g_builtinPresets)
        {
            PresetStore p{};
            p.name = b.name;
            p.filePath = BuildPresetPath(p.name);
            p.keys.assign(b.keys, b.keys + b.count);
            p.labels.clear();
            p.labels.reserve((size_t)b.count);
            for (const auto& k : p.keys) p.labels.emplace_back(k.label ? k.label : L"");
            EnsureActiveLabelsBound(p.keys, p.labels);
            AddOrReplacePreset(p);
        }
    }

    static void LoadPresetsFromDir()
    {
        std::error_code ec;
        fs::path dir = GetLayoutsDir();
        if (!fs::exists(dir, ec)) return;

        for (const auto& e : fs::directory_iterator(dir, ec))
        {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            if (_wcsicmp(e.path().extension().c_str(), L".ini") != 0) continue;

            PresetStore p{};
            if (LoadPresetFile(e.path().c_str(), p))
                AddOrReplacePreset(p);
        }
    }

    static void EnsurePresetFilesExist()
    {
        for (auto& p : g_presets)
        {
            BindPresetLabels(p);
            if (p.filePath.empty())
                p.filePath = BuildPresetPath(p.name);
            if (GetFileAttributesW(p.filePath.c_str()) == INVALID_FILE_ATTRIBUTES)
                SavePresetFile(p);
        }
    }

    static void EnsureInit()
    {
        if (!g_presets.empty()) return;
        LoadPresetsFromDir();
        if (g_presets.empty())
        {
            AddBuiltinDefaults();
            EnsurePresetFilesExist();
        }
        ActivatePreset(0);
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
    return g_activeRenderKeys.empty() ? nullptr : g_activeRenderKeys.data();
}

int KeyboardLayout_GetPresetCount()
{
    EnsureInit();
    return (int)g_presets.size();
}

const wchar_t* KeyboardLayout_GetPresetName(int idx)
{
    EnsureInit();
    idx = ClampPreset(idx);
    return g_presets[idx].name.c_str();
}

int KeyboardLayout_GetCurrentPresetIndex()
{
    EnsureInit();
    return g_currentPresetIdx;
}

void KeyboardLayout_SetPresetIndex(int idx)
{
    EnsureInit();
    ActivatePreset(idx);
}

void KeyboardLayout_ResetActiveToPreset()
{
    EnsureInit();
    ActivatePreset(g_currentPresetIdx);
}

bool KeyboardLayout_SetKeyGeometry(int idx, int row, int x, int w)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;

    KeyDef& k = g_activeKeys[idx];
    k.row = std::clamp(row, 0, 20);
    k.x = std::clamp(x, 0, 4000);
    k.w = ClampKeyDim(w);
    RefreshActiveRenderKeys();
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

bool KeyboardLayout_AddKey(uint16_t hid, const wchar_t* label, int row, int x, int w)
{
    EnsureInit();

    KeyDef kd{};
    kd.hid = hid;
    kd.row = std::clamp(row, 0, 20);
    kd.x = std::clamp(x, 0, 4000);
    kd.w = ClampKeyDim(w);
    kd.h = KEYBOARD_KEY_H;

    g_ownedLabels.emplace_back((label && label[0]) ? label : L"Key");
    kd.label = g_ownedLabels.back().c_str();
    g_activeKeys.push_back(kd);

    // Rebind label pointers in case vector reallocated.
    for (size_t i = 0; i < g_activeKeys.size() && i < g_ownedLabels.size(); ++i)
        g_activeKeys[i].label = g_ownedLabels[i].c_str();

    RefreshActiveRenderKeys();
    g_customEdited = true;
    return true;
}

bool KeyboardLayout_RemoveKey(int idx)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;

    g_activeKeys.erase(g_activeKeys.begin() + idx);
    if (idx < (int)g_ownedLabels.size())
        g_ownedLabels.erase(g_ownedLabels.begin() + idx);

    for (size_t i = 0; i < g_activeKeys.size() && i < g_ownedLabels.size(); ++i)
        g_activeKeys[i].label = g_ownedLabels[i].c_str();

    RefreshActiveRenderKeys();
    g_customEdited = true;
    return true;
}

bool KeyboardLayout_SetKeyLabel(int idx, const wchar_t* label)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;
    if (idx >= (int)g_ownedLabels.size()) return false;

    g_ownedLabels[idx] = (label && label[0]) ? label : L"Key";
    g_activeKeys[idx].label = g_ownedLabels[idx].c_str();
    RefreshActiveRenderKeys();
    g_customEdited = true;
    return true;
}

bool KeyboardLayout_SetKeyHid(int idx, uint16_t hid)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_activeKeys.size()) return false;
    g_activeKeys[idx].hid = hid;
    RefreshActiveRenderKeys();
    g_customEdited = true;
    return true;
}

bool KeyboardLayout_SaveActivePreset()
{
    EnsureInit();
    int idx = ClampPreset(g_currentPresetIdx);
    if (idx < 0 || idx >= (int)g_presets.size()) return false;

    PresetStore& p = g_presets[idx];
    p.keys = g_activeKeys;
    p.labels = g_ownedLabels;
    p.uniformSpacing = g_activeUniformSpacing;
    p.uniformGap = ClampUniformGap(g_activeUniformGap);
    EnsureActiveLabelsBound(p.keys, p.labels);
    if (p.filePath.empty())
        p.filePath = BuildPresetPath(p.name);

    if (!SavePresetFile(p))
        return false;

    g_customEdited = false;
    return true;
}

bool KeyboardLayout_CreatePreset(const wchar_t* name, int* outIndex)
{
    EnsureInit();
    if (!name || !name[0]) return false;

    std::wstring n = name;
    while (!n.empty() && iswspace(n.front())) n.erase(n.begin());
    while (!n.empty() && iswspace(n.back())) n.pop_back();
    if (n.empty()) return false;

    n = SanitizeFileName(n);
    if (n.empty()) return false;
    if (FindPresetByName(n) >= 0) return false;

    PresetStore p{};
    p.name = n;
    p.filePath = BuildPresetPath(p.name);
    p.keys = g_activeKeys;
    p.labels = g_ownedLabels;
    p.uniformSpacing = g_activeUniformSpacing;
    p.uniformGap = ClampUniformGap(g_activeUniformGap);
    EnsureActiveLabelsBound(p.keys, p.labels);

    if (!SavePresetFile(p))
        return false;

    g_presets.push_back(p);
    BindPresetLabels(g_presets.back());
    int idx = (int)g_presets.size() - 1;
    ActivatePreset(idx);
    if (outIndex) *outIndex = idx;
    return true;
}

bool KeyboardLayout_DeletePreset(int idx)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_presets.size()) return false;
    if ((int)g_presets.size() <= 1) return false;

    const std::wstring path = g_presets[idx].filePath;
    if (!path.empty())
    {
        std::error_code ec;
        bool existed = fs::exists(path, ec);
        if (ec) return false;
        if (existed && !fs::remove(path, ec))
            return false;
        if (ec) return false;
    }

    const bool deletingCurrent = (idx == g_currentPresetIdx);
    g_presets.erase(g_presets.begin() + idx);

    if (deletingCurrent)
    {
        int next = std::clamp(idx, 0, (int)g_presets.size() - 1);
        ActivatePreset(next);
    }
    else if (idx < g_currentPresetIdx)
    {
        g_currentPresetIdx--;
    }

    return true;
}

bool KeyboardLayout_GetPresetSnapshot(int presetIdx, std::vector<KeyDef>& outKeys, std::vector<std::wstring>& outLabels, bool* outUniformSpacing, int* outUniformGap)
{
    EnsureInit();
    if (presetIdx < 0 || presetIdx >= (int)g_presets.size())
        return false;

    const PresetStore& p = g_presets[presetIdx];
    outKeys = p.keys;
    outLabels = p.labels;
    if (outUniformSpacing) *outUniformSpacing = p.uniformSpacing;
    if (outUniformGap) *outUniformGap = ClampUniformGap(p.uniformGap);

    if (outLabels.size() < outKeys.size())
    {
        outLabels.resize(outKeys.size());
        for (size_t i = 0; i < outKeys.size(); ++i)
            outLabels[i] = outKeys[i].label ? outKeys[i].label : L"";
    }

    EnsureActiveLabelsBound(outKeys, outLabels);
    return true;
}

bool KeyboardLayout_StorePresetSnapshot(int presetIdx, const std::vector<KeyDef>& keys, const std::vector<std::wstring>& labels, bool applyIfActive, bool uniformSpacing, int uniformGap)
{
    EnsureInit();
    if (presetIdx < 0 || presetIdx >= (int)g_presets.size())
        return false;
    if (keys.empty())
        return false;

    PresetStore& p = g_presets[presetIdx];
    p.keys = keys;
    p.labels.resize(keys.size());
    p.uniformSpacing = uniformSpacing;
    p.uniformGap = ClampUniformGap(uniformGap);

    for (size_t i = 0; i < p.keys.size(); ++i)
    {
        const KeyDef& in = keys[i];
        p.keys[i].hid = (uint16_t)std::clamp((int)in.hid, 0, 65535);
        p.keys[i].row = std::clamp(in.row, 0, 20);
        p.keys[i].x = std::clamp(in.x, 0, 4000);
        p.keys[i].w = ClampKeyDim(in.w);
        p.keys[i].h = ClampKeyDim(in.h);

        if (i < labels.size() && !labels[i].empty())
            p.labels[i] = labels[i];
        else if (in.label && in.label[0])
            p.labels[i] = in.label;
        else
            p.labels[i] = L"Key";
    }

    EnsureActiveLabelsBound(p.keys, p.labels);
    if (p.filePath.empty())
        p.filePath = BuildPresetPath(p.name);

    if (!SavePresetFile(p))
        return false;

    if (applyIfActive && presetIdx == g_currentPresetIdx)
    {
        g_activeKeys = p.keys;
        g_ownedLabels = p.labels;
        g_activeUniformSpacing = p.uniformSpacing;
        g_activeUniformGap = ClampUniformGap(p.uniformGap);
        EnsureActiveLabelsBound(g_activeKeys, g_ownedLabels);
        RefreshActiveRenderKeys();
        g_customEdited = false;
    }

    return true;
}

bool KeyboardLayout_LoadFromIni(const wchar_t* path)
{
    if (!path) return false;
    EnsureInit();

    wchar_t nameBuf[128]{};
    GetPrivateProfileStringW(L"KeyboardLayout", L"PresetName", L"", nameBuf, 128, path);
    if (nameBuf[0])
    {
        int idx = FindPresetByName(nameBuf);
        if (idx >= 0)
        {
            ActivatePreset(idx);
            return true;
        }
    }

    ActivatePreset(0);
    return true;
}

void KeyboardLayout_SaveToIni(const wchar_t* path)
{
    if (!path) return;
    EnsureInit();

    WritePrivateProfileStringW(L"KeyboardLayout", nullptr, nullptr, path);
    WritePrivateProfileStringW(L"KeyboardLayout", L"PresetName", g_presets[ClampPreset(g_currentPresetIdx)].name.c_str(), path);
}
