#include "keyboard_layout.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <cwctype>
#include <memory>
#include <mutex>

#include "app_paths.h"
#include "analog_key_codes.h"
#include "file_name_policy.h"
#include "ini_util.h"
#include "bounded_ini.h"
#include "backend.h"
#include "first_run_layout.h"
#include "imported_layouts.h"
#include "keychron_catalog_layouts.h"
#include "keychron_catalog_presets.h"
#include "lemokey_catalog_layouts.h"
#include "drunkdeer_catalog_layouts.h"
#include "generated/layout_pipeline/layouts.h"
#include "generated/layout_pipeline/identities.h"

namespace fs = std::filesystem;

namespace
{
    struct PresetDef
    {
        const wchar_t* name;
        const KeyDef* keys;
        int count;
        const wchar_t* brand;
    };

    struct PresetStore
    {
        std::wstring name;
        std::wstring filePath;
        std::wstring brand;
        std::vector<KeyDef> keys;
        std::vector<std::wstring> labels;
        bool uniformSpacing = false;
        int uniformGap = 8;
        int builtinGeometryRevision = 0;
    };

    // Revision 1 was an unreleased, visually incorrect 87 -> 88 migration.
    // Revision 2 establishes the visually validated 87 px tall-key contract.
    static constexpr int kBuiltinGeometryRevision = 2;

    static bool ReadOptionalLayoutInteger(const wchar_t* path, const wchar_t* key,
        int minimum, int maximum, int defaultValue, int& out)
    {
        std::int32_t parsed = defaultValue;
        if (!halljoy::ini::ReadSigned(path, L"LayoutPreset", key, minimum, maximum,
                defaultValue, parsed))
            return false;
        out = static_cast<int>(parsed);
        return true;
    }

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
        {L"FN",     halljoy::keycode::kFn,5, 528,42},
        {L"FN2",    halljoy::keycode::kOem1,5, 576,42},
        {L"Left",  80,5, 624,42},
        {L"Down",  81,5, 672,42},
        {L"Right", 79,5, 720,42},
    };

    // Exact DrunkDeer G65 ANSI geometry. Fn and Menu/Fn2 use Soup/UAP's
    // established extended codes so they remain independently bindable without
    // colliding with an unrelated USB HID usage.
    static const KeyDef g_g65Keys[] =
    {
        {L"Esc", 41,0,   0,42}, {L"1",30,0,  48,42},
        {L"2",31,0,  96,42}, {L"3",32,0, 144,42},
        {L"4",33,0, 192,42}, {L"5",34,0, 240,42},
        {L"6",35,0, 288,42}, {L"7",36,0, 336,42},
        {L"8",37,0, 384,42}, {L"9",38,0, 432,42},
        {L"0",39,0, 480,42}, {L"-",45,0, 528,42},
        {L"=",46,0, 576,42}, {L"Back",42,0,624,90},
        {L"Del",76,0, 720,42},

        {L"Tab",43,1,   0,74}, {L"Q",20,1,  80,42},
        {L"W",26,1, 128,42}, {L"E",8,1, 176,42},
        {L"R",21,1, 224,42}, {L"T",23,1, 272,42},
        {L"Y",28,1, 320,42}, {L"U",24,1, 368,42},
        {L"I",12,1, 416,42}, {L"O",18,1, 464,42},
        {L"P",19,1, 512,42}, {L"[",47,1, 560,42},
        {L"]",48,1, 608,42}, {L"\\",49,1,656,58},
        {L"End",77,1, 720,42},

        {L"Caps",57,2,   0,84}, {L"A",4,2,  90,42},
        {L"S",22,2, 138,42}, {L"D",7,2, 186,42},
        {L"F",9,2, 234,42}, {L"G",10,2, 282,42},
        {L"H",11,2, 330,42}, {L"J",13,2, 378,42},
        {L"K",14,2, 426,42}, {L"L",15,2, 474,42},
        {L";",51,2, 522,42}, {L"'",52,2, 570,42},
        {L"Enter",40,2,618,94}, {L"PgUp",75,2,720,42},

        {L"Shift",225,3,  0,106}, {L"Z",29,3,112,42},
        {L"X",27,3,160,42}, {L"C",6,3,208,42},
        {L"V",25,3,256,42}, {L"B",5,3,304,42},
        {L"N",17,3,352,42}, {L"M",16,3,400,42},
        {L",",54,3,448,42}, {L".",55,3,496,42},
        {L"/",56,3,544,42}, {L"Shift",229,3,592,74},
        {L"Up",82,3,672,42}, {L"PgDn",78,3,720,42},

        {L"Ctrl",224,4,  0,54}, {L"Win",227,4, 60,54},
        {L"Alt",226,4,120,54}, {L"Space",44,4,180,294},
        {L"Alt",230,4,480,42},
        {L"Fn",halljoy::keycode::kFn,4,528,42},
        {L"Menu",halljoy::keycode::kOem1,4,576,42},
        {L"Left",80,4,624,42}, {L"Down",81,4,672,42},
        {L"Right",79,4,720,42},
    };

    // Compact optional view for users who only need the four analogue movement
    // keys. This changes presentation only; the native backend continues to
    // publish every physical G65 key.
    static const KeyDef g_wasdOnlyKeys[] =
    {
        {L"W", 26, 0, 48, 42},
        {L"A",  4, 1,  0, 42},
        {L"S", 22, 1, 48, 42},
        {L"D",  7, 1, 96, 42},
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
        {L"Num+",  87, 2, 1044, 42, 87},

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
        {L"NEnt",  88, 4, 1044, 42, 87},

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
        { L"DrunkDeer A75 Pro", g_drunkdeer_A75Pro, (int)std::size(g_drunkdeer_A75Pro), L"DrunkDeer" },
        { L"DrunkDeer G65 ANSI", g_drunkdeer_G65, (int)std::size(g_drunkdeer_G65), L"DrunkDeer" },
        { L"WASD Only", g_wasdOnlyKeys, (int)(sizeof(g_wasdOnlyKeys) / sizeof(g_wasdOnlyKeys[0])), L"Other" },
        { L"Generic 100% ANSI", g_generic100Keys, (int)(sizeof(g_generic100Keys) / sizeof(g_generic100Keys[0])), L"Other" },
        { L"Keychron K4 HE ANSI - Imported", g_imported_k4, (int)std::size(g_imported_k4), L"Keychron" },
        { L"Keychron Q1 HE ANSI - Imported", g_imported_q1, (int)std::size(g_imported_q1), L"Keychron" },
        HALLJOY_KEYCHRON_CATALOG_PRESETS
        { L"Lemokey P1 HE ANSI", g_lemokey_p1_ansi, (int)std::size(g_lemokey_p1_ansi), L"Lemokey" },
        { L"Lemokey P1 HE ISO", g_lemokey_p1_iso, (int)std::size(g_lemokey_p1_iso), L"Lemokey" },
        { L"DrunkDeer A75 ANSI", g_drunkdeer_A75Ansi, (int)std::size(g_drunkdeer_A75Ansi), L"DrunkDeer" },
        { L"DrunkDeer A75 ISO", g_drunkdeer_A75Iso, (int)std::size(g_drunkdeer_A75Iso), L"DrunkDeer" },
        { L"DrunkDeer G60 ANSI", g_drunkdeer_G60, (int)std::size(g_drunkdeer_G60), L"DrunkDeer" },
        { L"DrunkDeer G75 ANSI", g_drunkdeer_G75Ansi, (int)std::size(g_drunkdeer_G75Ansi), L"DrunkDeer" },
        { L"DrunkDeer G75 JIS", g_drunkdeer_G75Jis, (int)std::size(g_drunkdeer_G75Jis), L"DrunkDeer" },
#include "generated/layout_pipeline/presets.inc"
    };

    // Reviewed geometry aliases, not protocol aliases. Keep original definitions
    // to recognize unedited legacy files; customized old models remain independent.
    struct LayoutMerge { const wchar_t* first; const wchar_t* second; const wchar_t* name; };
    static constexpr LayoutMerge g_layoutMerges[] = {
        {L"Keychron K2 HE ANSI", L"Keychron K3 HE ANSI", L"Keychron K2 HE + K3 HE ANSI"},
        {L"Keychron K2 HE ISO", L"Keychron K3 HE ISO", L"Keychron K2 HE + K3 HE ISO"},
        {L"Keychron Q1 HE ANSI - Imported", L"Keychron Q1 HE 8K ANSI", L"Keychron Q1 HE + Q1 HE 8K ANSI"},
        {L"Keychron Q1 HE ISO", L"Keychron Q1 HE 8K ISO", L"Keychron Q1 HE + Q1 HE 8K ISO"},
        {L"Keychron Q3 HE ANSI", L"Keychron Q3 HE 8K ANSI", L"Keychron Q3 HE + Q3 HE 8K ANSI"},
        {L"Keychron Q3 HE ISO", L"Keychron Q3 HE 8K ISO", L"Keychron Q3 HE + Q3 HE 8K ISO"},
        {L"Keychron Q3 HE JIS", L"Keychron Q3 HE 8K JIS", L"Keychron Q3 HE + Q3 HE 8K JIS"},
        {L"Keychron Q5 HE ANSI", L"Keychron Q5 HE 8K ANSI", L"Keychron Q5 HE + Q5 HE 8K ANSI"},
        {L"Keychron Q6 HE ANSI", L"Keychron Q6 HE 8K ANSI", L"Keychron Q6 HE + Q6 HE 8K ANSI"},
        {L"Wooting 60HE ANSI", L"Wooting 60HE+ ANSI", L"Wooting 60HE + 60HE+ ANSI"},
        {L"Wooting 60HE ISO", L"Wooting 60HE+ ISO", L"Wooting 60HE + 60HE+ ISO"},
        {L"Wooting Two ANSI", L"Wooting Two HE ANSI", L"Wooting Two + Two HE ANSI"},
        {L"Wooting Two ISO", L"Wooting Two HE ISO", L"Wooting Two + Two HE ISO"},
    };
    static const LayoutMerge* MergeFor(const std::wstring& name) {
        for (const auto& merge : g_layoutMerges)
            if (FileNamePolicy_Equivalent(name, merge.first) || FileNamePolicy_Equivalent(name, merge.second)) return &merge;
        return nullptr;
    }
    static bool IsUneditedMergedLegacy(const PresetStore& p) {
        if (!MergeFor(p.name) || p.uniformSpacing || p.uniformGap != 8) return false;
        for (const auto& b : g_builtinPresets) {
            if (!FileNamePolicy_Equivalent(p.name, b.name)) continue;
            if (p.brand != b.brand || p.keys.size() != (size_t)b.count || p.labels.size() != p.keys.size()) return false;
            for (size_t i = 0; i < p.keys.size(); ++i) {
                const auto& k = p.keys[i]; const auto& old = b.keys[i];
                if (k.hid != old.hid || k.row != old.row || k.y != old.y || k.x != old.x ||
                    k.w != old.w || k.h != old.h || k.notchW != old.notchW || k.notchY != old.notchY ||
                    p.labels[i] != (old.label ? old.label : L"")) return false;
            }
            return true;
        }
        return false;
    }

    static std::vector<PresetStore> g_presets;
    static uint64_t g_catalogRevision = 0;
    static std::vector<KeyDef> g_activeKeys;
    static std::vector<KeyDef> g_activeRenderKeys;
    static std::vector<std::wstring> g_ownedLabels;
    static std::atomic<std::shared_ptr<const KeyboardLayoutSnapshot>> g_renderSnapshot{ nullptr };
    static std::atomic<std::shared_ptr<const KeyboardLayoutSnapshot>> g_overlaySnapshot{ nullptr };
    static std::wstring g_overlayPresetName;
    static int FindPresetByName(const std::wstring& name);
    static std::once_flag g_initOnce;
    static bool g_activeUniformSpacing = false;
    static int g_activeUniformGap = 8;
    static int g_currentPresetIdx = 0;
    static bool g_customEdited = false;
    static halljoy::layout_selection::FirstRun g_firstRunLayout;

    static int ClampUniformGap(int v);
    static int ClampKeyDim(int v)
    {
        return std::clamp(v, KEYBOARD_KEY_MIN_DIM, KEYBOARD_KEY_MAX_DIM);
    }

    static std::wstring GetLayoutsDir()
    {
        const std::wstring& dir = AppPaths_LayoutsDir();
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }

    static std::wstring BuildPresetPath(const std::wstring& name)
    {
        std::wstring path;
        if (!FileNamePolicy_BuildChildPath(GetLayoutsDir(), name, L".ini", path))
            return {};
        return path;
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
        halljoy::ini::ReadFile inputFile(path);
        if (!inputFile) return false;
        std::wstring countText;
        std::uint32_t parsedCount = 0;
        if (!halljoy::ini::Read(path, L"LayoutPreset", L"Count", countText) ||
            !halljoy::ini::Unsigned(countText, static_cast<std::uint32_t>(halljoy::ini::kMaxLayoutKeys), parsedCount) ||
            parsedCount == 0) return false;
        const int count = static_cast<int>(parsedCount);

        std::vector<KeyDef> keys;
        std::vector<std::wstring> labels;
        keys.reserve((size_t)count);
        labels.reserve((size_t)count);

        for (int i = 0; i < count; ++i)
        {
            wchar_t k[64]{};
            swprintf_s(k, L"K%d", i);
            wchar_t packed[1024]{};
            if (GetPrivateProfileStringW(L"LayoutPreset", k, L"", packed, (DWORD)_countof(packed), path) >= _countof(packed) - 1) return false;

            KeyDef kd{};
            std::wstring label;
            if (ParsePackedKeyEntry(packed, kd, label))
            {
                swprintf_s(k, L"Y%d", i);
                if (!ReadOptionalLayoutInteger(path, k, -1, 4000, -1, kd.y)) return false;
                swprintf_s(k, L"NotchW%d", i);
                if (!ReadOptionalLayoutInteger(path, k, 0, 600, 0, kd.notchW)) return false;
                swprintf_s(k, L"NotchY%d", i);
                if (!ReadOptionalLayoutInteger(path, k, 0, 600, 0, kd.notchY) || !KeyboardLayout_ValidShape(kd)) return false;
                keys.push_back(kd);
                labels.push_back(std::move(label));
            }
            else return false;
        }

        if (keys.empty()) return false;

        out.name = fs::path(path).stem().wstring();
        out.filePath = path;
        wchar_t brand[128]{};
        if (GetPrivateProfileStringW(L"LayoutPreset", L"Brand", L"", brand, (DWORD)std::size(brand), path) >= std::size(brand) - 1)
            return false;
        out.brand = brand;
        // Only exact built-in identities have a legacy category. Never guess a
        // user's manufacturer from an arbitrary filename.
        if (out.brand.empty()) {
            out.brand = L"Custom";
            for (const auto& builtin : g_builtinPresets)
                if (FileNamePolicy_Equivalent(out.name, builtin.name)) out.brand = builtin.brand;
        }
        out.keys = std::move(keys);
        out.labels = std::move(labels);
        int uniformSpacing = 0;
        int uniformGap = 8;
        int geometryRevision = 0;
        if (!ReadOptionalLayoutInteger(path, L"UniformSpacing", 0, 1, 0, uniformSpacing) ||
            !ReadOptionalLayoutInteger(path, L"UniformGap", 0, 4096, 8, uniformGap) ||
            !ReadOptionalLayoutInteger(path, L"BuiltinGeometryRevision", 0, INT_MAX, 0, geometryRevision))
            return false;
        out.uniformSpacing = uniformSpacing != 0;
        out.uniformGap = ClampUniformGap(uniformGap);
        out.builtinGeometryRevision = geometryRevision;
        for (size_t i = 0; i < out.keys.size() && i < out.labels.size(); ++i)
            out.keys[i].label = out.labels[i].c_str();
        return true;
    }

    struct LayoutPresetSaveContext
    {
        const PresetStore* preset = nullptr;
    };

    static bool LayoutPresetTransactionWrite(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<LayoutPresetSaveContext*>(rawContext);
        const PresetStore& p = *context->preset;
        bool ok = WritePrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"1", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"LayoutPreset", temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"LayoutPreset", nullptr, nullptr, temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"LayoutPreset", L"Brand", p.brand.c_str(), temporaryPath) != FALSE;

        wchar_t v[64]{};
        swprintf_s(v, L"%d", (int)p.keys.size());
        ok &= WritePrivateProfileStringW(L"LayoutPreset", L"Count", v, temporaryPath) != FALSE;
        ok &= WritePrivateProfileStringW(L"LayoutPreset", L"UniformSpacing", p.uniformSpacing ? L"1" : L"0", temporaryPath) != FALSE;
        swprintf_s(v, L"%d", ClampUniformGap(p.uniformGap));
        ok &= WritePrivateProfileStringW(L"LayoutPreset", L"UniformGap", v, temporaryPath) != FALSE;
        swprintf_s(v, L"%d", p.builtinGeometryRevision > 0 ? p.builtinGeometryRevision : 0);
        ok &= WritePrivateProfileStringW(L"LayoutPreset", L"BuiltinGeometryRevision", v, temporaryPath) != FALSE;

        for (int i = 0; i < (int)p.keys.size(); ++i)
        {
            const KeyDef& k = p.keys[i];
            wchar_t key[64]{};
            swprintf_s(key, L"K%d", i);
            std::wstring packed = BuildPackedKeyEntry(k);
            ok &= WritePrivateProfileStringW(L"LayoutPreset", key, packed.c_str(), temporaryPath) != FALSE;
            swprintf_s(key, L"Y%d", i);
            swprintf_s(v, L"%d", k.y);
            ok &= WritePrivateProfileStringW(L"LayoutPreset", key, v, temporaryPath) != FALSE;
            // The section was cleared above: rectangles need no extra writes.
            if (k.notchW) {
                swprintf_s(key, L"NotchW%d", i);
                swprintf_s(v, L"%d", k.notchW);
                ok &= WritePrivateProfileStringW(L"LayoutPreset", key, v, temporaryPath) != FALSE;
                swprintf_s(key, L"NotchY%d", i);
                swprintf_s(v, L"%d", k.notchY);
                ok &= WritePrivateProfileStringW(L"LayoutPreset", key, v, temporaryPath) != FALSE;
            }
        }
        if (!ok && errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return ok;
    }

    static bool LayoutPresetTransactionValidate(const wchar_t* temporaryPath, void* rawContext, DWORD* errorOut)
    {
        auto* context = static_cast<LayoutPresetSaveContext*>(rawContext);
        const PresetStore& p = *context->preset;
        wchar_t schema[32]{};
        wchar_t kind[32]{};
        GetPrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"{missing}", schema, (DWORD)_countof(schema), temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"{missing}", kind, (DWORD)_countof(kind), temporaryPath);
        bool ok = wcscmp(schema, L"1") == 0 && wcscmp(kind, L"LayoutPreset") == 0;
        wchar_t brand[128]{};
        GetPrivateProfileStringW(L"LayoutPreset", L"Brand", L"", brand, (DWORD)std::size(brand), temporaryPath);
        ok &= p.brand == brand;
        int count = -1;
        int uniformSpacing = -1;
        int uniformGap = -1;
        int geometryRevision = -1;
        ok &= ReadOptionalLayoutInteger(temporaryPath, L"Count", 0,
            static_cast<int>(halljoy::ini::kMaxLayoutKeys), -1, count) &&
            count == static_cast<int>(p.keys.size());
        ok &= ReadOptionalLayoutInteger(temporaryPath, L"UniformSpacing", 0, 1, -1, uniformSpacing) &&
            uniformSpacing == (p.uniformSpacing ? 1 : 0);
        ok &= ReadOptionalLayoutInteger(temporaryPath, L"UniformGap", 0, 4096, -1, uniformGap) &&
            uniformGap == ClampUniformGap(p.uniformGap);
        ok &= ReadOptionalLayoutInteger(temporaryPath, L"BuiltinGeometryRevision", 0, INT_MAX, -1, geometryRevision) &&
            geometryRevision == (p.builtinGeometryRevision > 0 ? p.builtinGeometryRevision : 0);

        for (int i = 0; ok && i < (int)p.keys.size(); ++i)
        {
            wchar_t key[64]{};
            swprintf_s(key, L"K%d", i);
            wchar_t packed[4096]{};
            GetPrivateProfileStringW(L"LayoutPreset", key, L"{missing}", packed, (DWORD)_countof(packed), temporaryPath);
            ok &= BuildPackedKeyEntry(p.keys[(size_t)i]) == packed;
            swprintf_s(key, L"Y%d", i);
            int y = -2;
            ok &= ReadOptionalLayoutInteger(temporaryPath, key, -1, 4000, -2, y) && y == p.keys[(size_t)i].y;
            int notch = -1;
            swprintf_s(key, L"NotchW%d", i);
            ok &= ReadOptionalLayoutInteger(temporaryPath, key, 0, 600, 0, notch) && notch == p.keys[i].notchW;
            swprintf_s(key, L"NotchY%d", i);
            ok &= ReadOptionalLayoutInteger(temporaryPath, key, 0, 600, 0, notch) && notch == p.keys[i].notchY;
        }
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }

    static bool SavePresetFile(const PresetStore& p)
    {
        if (p.filePath.empty()) return false;
        for (const auto& key : p.keys)
            if (!KeyboardLayout_ValidShape(key)) return false;
        fs::path dir = fs::path(p.filePath).parent_path();
        std::error_code ec;
        fs::create_directories(dir, ec);

        LayoutPresetSaveContext context{ &p };
        const auto result = IniUtil_SaveAtomic(
            p.filePath.c_str(),
            LayoutPresetTransactionWrite,
            LayoutPresetTransactionValidate,
            &context);
        if (!result.Succeeded())
        {
            IniUtil_ReportSaveFailure(L"layout preset", p.filePath.c_str(), result);
            return false;
        }
        return true;
    }

    static bool UpgradeUneditedDrunkDeer(PresetStore& p)
    {
        static_assert(std::size(g_a75Keys)==std::size(g_drunkdeer_A75Pro));
        static_assert(std::size(g_g65Keys)==std::size(g_drunkdeer_G65));
        const KeyDef* old = nullptr;
        const KeyDef* updated = nullptr;
        size_t count = 0;
        if (FileNamePolicy_Equivalent(p.name,L"DrunkDeer A75 Pro")) {
            old=g_a75Keys; updated=g_drunkdeer_A75Pro; count=std::size(g_a75Keys);
        } else if (FileNamePolicy_Equivalent(p.name,L"DrunkDeer G65 ANSI")) {
            old=g_g65Keys; updated=g_drunkdeer_G65; count=std::size(g_g65Keys);
        }
        if (!old || p.uniformSpacing || p.keys.size()!=count || p.labels.size()!=count) return false;
        for (size_t i=0;i<count;++i) {
            const auto& a=p.keys[i]; const auto& b=old[i];
            if (a.hid!=b.hid || a.x!=b.x || a.w!=b.w || a.h!=b.h ||
                (a.y>=0?a.y:a.row*KEYBOARD_ROW_PITCH_Y)!=(b.y>=0?b.y:b.row*KEYBOARD_ROW_PITCH_Y) ||
                a.notchW!=b.notchW || a.notchY!=b.notchY || p.labels[i]!=b.label) return false;
        }
        // Only byte-for-byte-equivalent effective legacy geometry is upgraded.
        // Any user edit keeps its layout authoritative; no new backup files.
        p.keys.assign(updated,updated+count);
        for (size_t i=0;i<count;++i) p.labels[i]=updated[i].label;
        for (size_t i=0;i<count;++i) p.keys[i].label=p.labels[i].c_str();
        return true;
    }

    static bool ApplyBuiltinGeometryMigrations(PresetStore& preset)
    {
        const bool drunkdeerA75 = FileNamePolicy_Equivalent(
            preset.name, L"DrunkDeer A75 Pro");
        const bool generic100 = FileNamePolicy_Equivalent(preset.name, L"Generic 100% ANSI");
        if (!drunkdeerA75 && !generic100)
            return false;

        bool changed = false;
        if (drunkdeerA75)
        {
            for (std::size_t index = 0; index < preset.keys.size() &&
                index < preset.labels.size(); ++index)
            {
                if (preset.keys[index].hid != 0) continue;
                if (_wcsicmp(preset.labels[index].c_str(), L"FN") == 0)
                {
                    preset.keys[index].hid = halljoy::keycode::kFn;
                    changed = true;
                }
                else if (_wcsicmp(preset.labels[index].c_str(), L"FN2") == 0)
                {
                    preset.keys[index].hid = halljoy::keycode::kOem1;
                    changed = true;
                }
            }
        }

        if (preset.builtinGeometryRevision >= kBuiltinGeometryRevision)
            return changed;

        // Reset reloads the persisted preset. Repair only the two known tall
        // numpad usages and only their old 88 px value; preserve every other
        // user edit and never expand an already-correct 87 px key.
        for (auto& key : preset.keys)
        {
            if ((key.hid == 87 || key.hid == 88) && key.h == 88)
            {
                key.h = 87;
                changed = true;
            }
        }
        preset.builtinGeometryRevision = kBuiltinGeometryRevision;
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

    static void RefreshOverlaySnapshot()
    {
        const int idx = FindPresetByName(g_overlayPresetName);
        if (idx < 0)
        {
            g_overlayPresetName.clear();
            g_overlaySnapshot.store(nullptr, std::memory_order_release);
            BackendUI_SetOverlayTrackedHids(nullptr, 0);
            return;
        }
        const auto& preset = g_presets[idx];
        auto snapshot = std::make_shared<KeyboardLayoutSnapshot>();
        snapshot->keys = preset.keys;
        snapshot->labels = preset.labels;
        EnsureActiveLabelsBound(snapshot->keys, snapshot->labels);
        if (preset.uniformSpacing)
        {
            std::vector<int> displayX;
            BuildUniformDisplayX(snapshot->keys, ClampUniformGap(preset.uniformGap), displayX);
            for (size_t i = 0; i < snapshot->keys.size(); ++i)
                snapshot->keys[i].x = displayX[i];
        }
        std::vector<uint16_t> tracked;
        tracked.reserve(snapshot->keys.size());
        for (const auto& key : snapshot->keys) tracked.push_back(key.hid);
        BackendUI_SetOverlayTrackedHids(tracked.data(), (int)tracked.size());
        g_overlaySnapshot.store(std::move(snapshot), std::memory_order_release);
    }

    static void RefreshActiveRenderKeys()
    {
        g_activeRenderKeys = g_activeKeys;
        if (g_activeUniformSpacing && !g_activeRenderKeys.empty())
        {
            std::vector<int> displayX;
            BuildUniformDisplayX(g_activeRenderKeys, ClampUniformGap(g_activeUniformGap), displayX);
            for (size_t i = 0; i < g_activeRenderKeys.size() && i < displayX.size(); ++i)
                g_activeRenderKeys[i].x = displayX[i];
        }

        auto snapshot = std::make_shared<KeyboardLayoutSnapshot>();
        snapshot->keys = g_activeRenderKeys;
        snapshot->labels.resize(snapshot->keys.size());
        for (size_t i = 0; i < snapshot->keys.size(); ++i)
            snapshot->labels[i] = snapshot->keys[i].label ? snapshot->keys[i].label : L"";
        for (size_t i = 0; i < snapshot->keys.size(); ++i)
            snapshot->keys[i].label = snapshot->labels[i].c_str();

        std::shared_ptr<const KeyboardLayoutSnapshot> published = std::move(snapshot);
        g_renderSnapshot.store(std::move(published), std::memory_order_release);
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

    static int FindExactPresetByName(const std::wstring& name)
    {
        for (int i = 0; i < (int)g_presets.size(); ++i)
        {
            if (FileNamePolicy_Equivalent(g_presets[i].name, name))
                return i;
        }
        return -1;
    }

    static int FindPresetByName(const std::wstring& name)
    {
        const int exact = FindExactPresetByName(name);
        if (exact >= 0) return exact; // Never redirect a user's edited legacy model.
        const auto* merge = MergeFor(name);
        return merge ? FindExactPresetByName(merge->name) : -1;
    }

    static const wchar_t* ResolveSavedPresetName(const wchar_t* name)
    {
        return name && FileNamePolicy_Equivalent(name, L"Keychron K4 HE")
            ? L"Keychron K4 HE ANSI - Imported" : (name ? name : L"");
    }

    static void AddOrReplacePreset(const PresetStore& p)
    {
        ++g_catalogRevision;
        int idx = FindExactPresetByName(p.name);
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
            const auto* merge = MergeFor(b.name);
            if (merge && FindExactPresetByName(merge->name) >= 0) continue;
            PresetStore p{};
            p.name = merge ? merge->name : b.name;
            p.brand = b.brand;
            p.filePath = BuildPresetPath(p.name);
            p.keys.assign(b.keys, b.keys + b.count);
            p.builtinGeometryRevision = kBuiltinGeometryRevision;
            p.labels.clear();
            p.labels.reserve((size_t)b.count);
            for (const auto& k : p.keys) p.labels.emplace_back(k.label ? k.label : L"");
            if (merge) for (size_t i = 0; i < p.keys.size(); ++i)
                if (p.keys[i].hid == 1027 && (p.labels[i] == L"Cortana" || p.labels[i] == L"Assistant"))
                    p.labels[i] = L"Assistant";
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
            // Retired preset: keep any historical file recoverable on disk,
            // but never let it re-enter the catalogue or overwrite the replacement.
            if (FileNamePolicy_Equivalent(e.path().stem().wstring(), L"Keychron K4 HE")) continue;

            PresetStore p{};
            if (LoadPresetFile(e.path().c_str(), p))
            {
                // Leave legacy files recoverable on disk, but do not re-register
                // untouched duplicates or overwrite an edited combined preset.
                if (IsUneditedMergedLegacy(p)) continue;
                const bool legacyChanged=ApplyBuiltinGeometryMigrations(p);
                if (UpgradeUneditedDrunkDeer(p) || legacyChanged)
                    SavePresetFile(p);
                AddOrReplacePreset(p);
            }
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
        std::call_once(g_initOnce, []()
        {
            // Register every shipped preset for both fresh and existing users.
            // Files loaded afterwards intentionally override same-name built-ins,
            // preserving user edits while newly shipped presets remain discoverable.
            AddBuiltinDefaults();
            LoadPresetsFromDir();
            EnsurePresetFilesExist();
            ActivatePreset(0);
        });
    }
}

std::shared_ptr<const KeyboardLayoutSnapshot> KeyboardLayout_GetSnapshot()
{
    EnsureInit();
    return g_renderSnapshot.load(std::memory_order_acquire);
}

int KeyboardLayout_Count()
{
    EnsureInit();
    return (int)g_activeKeys.size();
}

std::shared_ptr<const KeyboardLayoutSnapshot> KeyboardLayout_GetOverlaySnapshot()
{
    EnsureInit();
    auto selected = g_overlaySnapshot.load(std::memory_order_acquire);
    return selected ? selected : g_renderSnapshot.load(std::memory_order_acquire);
}

int KeyboardLayout_GetOverlayPresetIndex()
{
    EnsureInit();
    return FindPresetByName(g_overlayPresetName);
}

const wchar_t* KeyboardLayout_GetOverlayPresetName()
{
    EnsureInit();
    return g_overlayPresetName.c_str();
}

void KeyboardLayout_SetOverlayPresetIndex(int idx)
{
    EnsureInit();
    g_overlayPresetName = idx >= 0 && idx < (int)g_presets.size() ? g_presets[idx].name : L"";
    RefreshOverlaySnapshot();
}

void KeyboardLayout_SetOverlayPresetName(const wchar_t* name)
{
    EnsureInit();
    KeyboardLayout_SetOverlayPresetIndex(FindPresetByName(ResolveSavedPresetName(name)));
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

std::wstring KeyboardLayout_GetPresetBrand(int idx)
{
    EnsureInit();
    if (idx < 0 || idx >= (int)g_presets.size()) return L"Custom";
    return g_presets[idx].brand.empty() ? L"Custom" : g_presets[idx].brand;
}

uint64_t KeyboardLayout_GetCatalogRevision()
{
    EnsureInit();
    return g_catalogRevision;
}

std::wstring KeyboardLayout_GetPresetDisplayName(int idx)
{
    std::wstring name = KeyboardLayout_GetPresetName(idx);
    for (const auto& merge : g_layoutMerges) if (name == merge.name) {
        const auto separator = name.find(L" + ");
        if (separator != std::wstring::npos) name.replace(separator, 3, L" / ");
        break;
    }
    const std::wstring suffix = L" - Imported";
    if (KeyboardLayout_GetPresetBrand(idx) == L"Keychron" && name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        name.resize(name.size() - suffix.size());
    return name;
}

std::wstring KeyboardLayout_GetPresetModel(int idx)
{
    auto name = KeyboardLayout_GetPresetDisplayName(idx);
    const auto prefix = KeyboardLayout_GetPresetBrand(idx) + L" ";
    if (name.compare(0, prefix.size(), prefix) == 0) name.erase(0, prefix.size());
    return name;
}

int KeyboardLayout_GetCurrentPresetIndex()
{
    EnsureInit();
    return g_currentPresetIdx;
}

void KeyboardLayout_SetPresetIndex(int idx)
{
    g_firstRunLayout.Cancel(); // User choice always wins, including the default.
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
    if (ClampKeyDim(w) <= k.notchW) return false;
    k.row = std::clamp(row, 0, 20);
    k.x = std::clamp(x, 0, 4000);
    k.w = ClampKeyDim(w);
    RefreshActiveRenderKeys();
    g_customEdited = true;
    return true;
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool KeyboardLayout_TestFirstRunSelection()
{
    EnsureInit();
    const auto previousPolicy = g_firstRunLayout;
    const int previousIndex = g_currentPresetIdx;
    const int previousOverlayIndex = KeyboardLayout_GetOverlayPresetIndex();
    const int replacement = FindPresetByName(ResolveSavedPresetName(L"Keychron K4 HE"));
    if (replacement < 0 || FindPresetByName(L"Keychron K4 HE") >= 0) return false;
    KeyboardLayout_SetOverlayPresetName(L"Keychron K4 HE");
    const bool retiredOverlayResolved = KeyboardLayout_GetOverlayPresetIndex() == replacement;
    KeyboardLayout_SetOverlayPresetIndex(previousOverlayIndex);
    BackendAnalogTelemetry t{};
    t.pluginHostReady = true; t.pluginHostSnapshotGeneration = 1;
    t.deviceCount = t.pluginDeviceCount = t.pluginHostDenseDeviceCount = 1;
    auto& d = t.pluginDevices[0];
    d.present = true; d.flags = BackendAnalogDeviceFlag_Connected;
    d.vendorId = 0x3434; d.productId = 0x0E40; d.usagePage = 0xFF60; d.usage = 0x61;
    d.rows = 6; d.columns = 19;
    bool ok = retiredOverlayResolved;
    // Exercise production alias resolution, not a duplicated test-only mapper.
    for (const auto& merge : g_layoutMerges) {
        const int combined = FindExactPresetByName(merge.name);
        const int oldFirst = FindExactPresetByName(merge.first);
        const int oldSecond = FindExactPresetByName(merge.second);
        const int expectedSecond = oldSecond >= 0 ? oldSecond : combined;
        PresetStore savedLegacy;
        if (oldFirst >= 0) savedLegacy = g_presets[oldFirst];
        ok &= combined >= 0 && FindPresetByName(merge.first) == (oldFirst >= 0 ? oldFirst : combined) &&
            FindPresetByName(merge.second) == expectedSecond;
        if (combined < 0) return false;
        KeyboardLayout_SetOverlayPresetName(merge.second);
        ok &= KeyboardLayout_GetOverlayPresetIndex() == expectedSecond;
        const PresetDef* first = nullptr; const PresetDef* second = nullptr;
        for (const auto& b : g_builtinPresets) {
            if (std::wstring(b.name) == merge.first) first = &b;
            if (std::wstring(b.name) == merge.second) second = &b;
        }
        if (!first || !second || first->count != second->count) return false;
        for (int i = 0; i < first->count; ++i) {
            const auto& a = first->keys[i];
            const auto end = second->keys + second->count;
            const auto* b = std::find_if(second->keys, end, [&](const KeyDef& k) { return k.hid == a.hid; });
            ok &= b != end && a.x == b->x && KeyboardLayout_KeyY(a) == KeyboardLayout_KeyY(*b) &&
                a.w == b->w && a.h == b->h && a.notchW == b->notchW && a.notchY == b->notchY;
        }
        PresetStore legacy{};
        legacy.name = first->name; legacy.brand = first->brand;
        legacy.keys.assign(first->keys, first->keys + first->count);
        for (const auto& key : legacy.keys) legacy.labels.emplace_back(key.label);
        ok &= IsUneditedMergedLegacy(legacy);
        if (AppPaths_Mode() != AppDataMode::SimulatorOverride) return false;
        legacy.filePath = (fs::path(AppPaths_SettingsIni()).parent_path() / (legacy.name + L".ini")).wstring();
        PresetStore roundtrip;
        ok &= SavePresetFile(legacy) && LoadPresetFile(legacy.filePath.c_str(), roundtrip) &&
            IsUneditedMergedLegacy(roundtrip);
        legacy.keys[0].x += 1;
        ok &= !IsUneditedMergedLegacy(legacy);
        ok &= SavePresetFile(legacy) && LoadPresetFile(legacy.filePath.c_str(), roundtrip) &&
            !IsUneditedMergedLegacy(roundtrip) && roundtrip.keys[0].x == legacy.keys[0].x;
        const auto count = g_presets.size();
        AddOrReplacePreset(legacy);
        const int edited = oldFirst >= 0 ? oldFirst : (int)count;
        ok &= g_presets.size() == count + (oldFirst < 0 ? 1 : 0) && FindPresetByName(merge.first) == edited &&
            FindPresetByName(merge.second) == expectedSecond && g_presets[edited].keys[0].x == legacy.keys[0].x;
        if (oldFirst >= 0) AddOrReplacePreset(savedLegacy);
        else { g_presets.pop_back(); ++g_catalogRevision; }
        legacy.keys[0].x -= 1; legacy.labels[0] = L"User label";
        ok &= !IsUneditedMergedLegacy(legacy);
        legacy.labels[0] = first->keys[0].label; legacy.uniformSpacing = true;
        ok &= !IsUneditedMergedLegacy(legacy);
    }
    KeyboardLayout_SetOverlayPresetIndex(previousOverlayIndex);
    KeyboardLayout_ArmFirstRunSelection();
    ok &= !KeyboardLayout_TryFirstRunSelection(false, t) && g_firstRunLayout.Pending();
    t.deviceCount = 0;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t) && g_firstRunLayout.Pending();
    t.deviceCount = 1;
    ok &= KeyboardLayout_TryFirstRunSelection(true, t);
    ok &= g_currentPresetIdx == FindPresetByName(L"Keychron K4 HE ANSI - Imported");
    ok &= KeyboardLayout_Count() == 100;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t);
    KeyboardLayout_ArmFirstRunSelection();
    KeyboardLayout_SetPresetIndex(previousIndex);
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t);
    KeyboardLayout_ArmFirstRunSelection();
    d.productId = 0x0B11; d.columns = 15; // ISO selects its own compound outline.
    ok &= KeyboardLayout_TryFirstRunSelection(true, t) && !g_firstRunLayout.Pending();
    ok &= g_currentPresetIdx == FindPresetByName(L"Keychron Q1 HE ISO");
    KeyboardLayout_ArmFirstRunSelection();
    d.productId = 0x0B10;
    t.deviceCount = t.pluginDeviceCount = t.pluginHostDenseDeviceCount = 2;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t) && !g_firstRunLayout.Pending();
    t.deviceCount = t.pluginDeviceCount = t.pluginHostDenseDeviceCount = 1;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t); // hotplug after ambiguity
    KeyboardLayout_ArmFirstRunSelection();
    ok &= KeyboardLayout_TryFirstRunSelection(true, t);
    ok &= g_currentPresetIdx == FindPresetByName(L"Keychron Q1 HE ANSI - Imported");
    ok &= KeyboardLayout_Count() == 81;
    KeyboardLayout_ArmFirstRunSelection();
    t.deviceCount = t.pluginDeviceCount = t.pluginHostDenseDeviceCount = 0;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t) && !g_firstRunLayout.Pending();
    t.deviceCount = t.pluginDeviceCount = t.pluginHostDenseDeviceCount = 1;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t);
    KeyboardLayout_ArmFirstRunSelection();
    d.usagePage = 1;
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t);
    d.usagePage = 0xFF60;
    KeyboardLayout_ArmFirstRunSelection();
    KeyboardLayout_LoadFromIni(AppPaths_SettingsIni().c_str());
    ok &= !KeyboardLayout_TryFirstRunSelection(true, t); // Saved settings win.
    d.vendorId=0x352d; d.rows=6; d.columns=21;
    d.usagePage=0xff00; d.usage=1;
    constexpr int ddCounts[]={82,82,83,61,68,84,86};
    for (unsigned i=1;i<=7;++i) {
        const auto model=static_cast<halljoy::drunkdeer_identity::Model>(i);
        d.productId=halljoy::drunkdeer_identity::Product(model);
        strcpy_s(d.name,halljoy::drunkdeer_identity::Name(model));
        d.flags=BackendAnalogDeviceFlag_Connected;
        KeyboardLayout_ArmFirstRunSelection();
        ok &= !KeyboardLayout_TryFirstRunSelection(true,t); // plausible name alone is insufficient
        d.flags|=BackendAnalogDeviceFlag_VerifiedModel;
        KeyboardLayout_ArmFirstRunSelection();
        ok &= KeyboardLayout_TryFirstRunSelection(true,t);
        ok &= KeyboardLayout_Count()==ddCounts[i-1];
        ok &= KeyboardLayout_GetPresetBrand(g_currentPresetIdx)==L"DrunkDeer";
        ok &= !KeyboardLayout_TryFirstRunSelection(true,t); // no repeated hotplug switch
    }
    t.pluginDeviceCount=0; t.pluginHostDenseDeviceCount=0;
    t.deviceCount=1; t.nativeProtocolCount=1;
    auto& native=t.nativeProtocols[0]; native.connected=true;
    for (const auto& entry : halljoy::layout_identity::entries) {
        native.verifiedLayoutToken=0;
        KeyboardLayout_ArmFirstRunSelection();
        ok &= !KeyboardLayout_TryFirstRunSelection(true,t); // PID/caption alone cannot select.
        native.verifiedLayoutToken=entry.token;
        KeyboardLayout_ArmFirstRunSelection();
        ok &= KeyboardLayout_TryFirstRunSelection(true,t);
        ok &= std::wstring(KeyboardLayout_GetPresetName(g_currentPresetIdx))==entry.preset;
        ok &= !KeyboardLayout_TryFirstRunSelection(true,t);
        t.deviceCount=2; t.nativeProtocolCount=2; t.nativeProtocols[1].connected=true;
        KeyboardLayout_ArmFirstRunSelection();
        ok &= !KeyboardLayout_TryFirstRunSelection(true,t);
        t.deviceCount=1; t.nativeProtocolCount=1; t.nativeProtocols[1].connected=false;
    }
    native.verifiedLayoutToken=~std::uint64_t{0};
    KeyboardLayout_ArmFirstRunSelection();
    ok &= !KeyboardLayout_TryFirstRunSelection(true,t);
    for (int variant=0;variant<2;++variant) {
        PresetStore legacy;
        const auto* old=variant?g_g65Keys:g_a75Keys;
        const size_t count=variant?std::size(g_g65Keys):std::size(g_a75Keys);
        legacy.name=variant?L"DrunkDeer G65 ANSI":L"DrunkDeer A75 Pro";
        legacy.keys.assign(old,old+count);
        for (const auto& key:legacy.keys) legacy.labels.emplace_back(key.label);
        auto edited=legacy;
        edited.keys[0].x+=1;
        ok &= !UpgradeUneditedDrunkDeer(edited) && edited.keys[0].x==1;
        edited=legacy; edited.labels[0]=L"Custom Esc";
        ok &= !UpgradeUneditedDrunkDeer(edited);
        ok &= UpgradeUneditedDrunkDeer(legacy);
        ok &= legacy.keys[0].h==42 && legacy.keys[0].y==0;
        ok &= !UpgradeUneditedDrunkDeer(legacy); // idempotent
    }
    ActivatePreset(previousIndex);
    g_firstRunLayout = previousPolicy;
    if (AppPaths_Mode() != AppDataMode::SimulatorOverride) return false;
    PresetStore metadata = g_presets[0];
    metadata.name = L"Brand Roundtrip";
    metadata.filePath = (fs::path(AppPaths_SettingsIni()).parent_path() / L"brand-roundtrip.ini").wstring();
    metadata.brand = L"Redragon";
    ok &= SavePresetFile(metadata);
    PresetStore loaded;
    ok &= LoadPresetFile(metadata.filePath.c_str(), loaded) && loaded.brand == L"Redragon";
    metadata.keys[0].w = 66; metadata.keys[0].h = 86;
    metadata.keys[0].notchW = 12; metadata.keys[0].notchY = 40;
    ok &= SavePresetFile(metadata);
    ok &= LoadPresetFile(metadata.filePath.c_str(), loaded);
    ok &= loaded.keys[0].notchW == 12 && loaded.keys[0].notchY == 40;
    metadata.keys[0].notchW = 66;
    ok &= !SavePresetFile(metadata); // Invalid edit cannot replace a valid file.
    ok &= LoadPresetFile(metadata.filePath.c_str(), loaded) && loaded.keys[0].notchW == 12;
    metadata.keys[0].notchW = 12;
    metadata.brand.clear();
    ok &= SavePresetFile(metadata);
    ok &= LoadPresetFile(metadata.filePath.c_str(), loaded) && loaded.brand == L"Custom";
    ok &= KeyboardLayout_GetPresetBrand(2) == L"Other";
    ok &= KeyboardLayout_GetPresetBrand(replacement) == L"Keychron";
    ok &= KeyboardLayout_GetPresetModel(replacement) == L"K4 HE ANSI";
    return ok;
}
#endif

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

    if (g_activeKeys.size() >= halljoy::ini::kMaxLayoutKeys) return false;
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

    PresetStore candidate = g_presets[idx];
    candidate.keys = g_activeKeys;
    candidate.labels = g_ownedLabels;
    candidate.uniformSpacing = g_activeUniformSpacing;
    candidate.uniformGap = ClampUniformGap(g_activeUniformGap);
    EnsureActiveLabelsBound(candidate.keys, candidate.labels);
    if (candidate.filePath.empty())
        candidate.filePath = BuildPresetPath(candidate.name);

    if (!SavePresetFile(candidate))
        return false;

    g_presets[idx] = std::move(candidate);
    BindPresetLabels(g_presets[idx]);
    RefreshOverlaySnapshot();
    g_customEdited = false;
    return true;
}

bool KeyboardLayout_CreatePreset(const wchar_t* name, int* outIndex, int sourcePreset, bool activate, const wchar_t* brand)
{
    EnsureInit();
    if (!name || !name[0]) return false;

    std::wstring n = name;
    while (!n.empty() && iswspace(n.front())) n.erase(n.begin());
    while (!n.empty() && iswspace(n.back())) n.pop_back();
    if (n.empty()) return false;

    n = FileNamePolicy_NormalizeStem(n);
    if (n.empty()) return false;
    if (FindPresetByName(n) >= 0) return false;

    PresetStore p{};
    p.name = n;
    p.brand = brand && *brand ? brand : L"Custom";
    if (p.brand.size() > 126 || p.brand.find_first_of(L"\r\n\t") != std::wstring::npos) return false;
    p.filePath = BuildPresetPath(p.name);
    p.keys = g_activeKeys;
    p.labels = g_ownedLabels;
    p.uniformSpacing = g_activeUniformSpacing;
    p.uniformGap = ClampUniformGap(g_activeUniformGap);
    if (sourcePreset >= 0) {
        if (sourcePreset >= (int)g_presets.size()) return false;
        const auto& source = g_presets[sourcePreset];
        p.keys = source.keys; p.labels = source.labels;
        p.uniformSpacing = source.uniformSpacing; p.uniformGap = source.uniformGap;
    }
    EnsureActiveLabelsBound(p.keys, p.labels);

    if (!SavePresetFile(p))
        return false;

    g_presets.push_back(p);
    ++g_catalogRevision;
    BindPresetLabels(g_presets.back());
    int idx = (int)g_presets.size() - 1;
    if (activate) ActivatePreset(idx);
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
    ++g_catalogRevision;
    RefreshOverlaySnapshot();

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

    PresetStore candidate = g_presets[presetIdx];
    if (keys.size() > halljoy::ini::kMaxLayoutKeys) return false;
    candidate.keys = keys;
    candidate.labels.resize(keys.size());
    candidate.uniformSpacing = uniformSpacing;
    candidate.uniformGap = ClampUniformGap(uniformGap);

    for (size_t i = 0; i < candidate.keys.size(); ++i)
    {
        const KeyDef& in = keys[i];
        candidate.keys[i].hid = (uint16_t)std::clamp((int)in.hid, 0, 65535);
        candidate.keys[i].row = std::clamp(in.row, 0, 20);
        candidate.keys[i].y = std::clamp(in.y, -1, 4000);
        candidate.keys[i].x = std::clamp(in.x, 0, 4000);
        candidate.keys[i].w = ClampKeyDim(in.w);
        candidate.keys[i].h = ClampKeyDim(in.h);
        if (!KeyboardLayout_ValidShape(candidate.keys[i])) return false;

        if (i < labels.size() && !labels[i].empty())
            candidate.labels[i] = labels[i];
        else if (in.label && in.label[0])
            candidate.labels[i] = in.label;
        else
            candidate.labels[i] = L"Key";
    }

    EnsureActiveLabelsBound(candidate.keys, candidate.labels);
    if (candidate.filePath.empty())
        candidate.filePath = BuildPresetPath(candidate.name);

    if (!SavePresetFile(candidate))
        return false;

    g_presets[presetIdx] = std::move(candidate);
    BindPresetLabels(g_presets[presetIdx]);
    RefreshOverlaySnapshot();
    const PresetStore& p = g_presets[presetIdx];
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

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool KeyboardLayout_TestReadPresetY(const wchar_t* path, int index, int* y)
{
    PresetStore loaded;
    if (!y || !LoadPresetFile(path, loaded) || index < 0 || index >= (int)loaded.keys.size()) return false;
    *y = KeyboardLayout_KeyY(loaded.keys[index]);
    return true;
}

bool KeyboardLayout_TestSaveActivePresetToPath(const wchar_t* path)
{
    if (!path || !path[0]) return false;
    EnsureInit();

    const int idx = ClampPreset(g_currentPresetIdx);
    if (idx < 0 || idx >= (int)g_presets.size()) return false;

    PresetStore candidate = g_presets[idx];
    candidate.filePath = path;
    candidate.keys = g_activeKeys;
    candidate.labels = g_ownedLabels;
    candidate.uniformSpacing = g_activeUniformSpacing;
    candidate.uniformGap = ClampUniformGap(g_activeUniformGap);
    EnsureActiveLabelsBound(candidate.keys, candidate.labels);
    return SavePresetFile(candidate);
}
#endif

bool KeyboardLayout_LoadFromIni(const wchar_t* path)
{
    g_firstRunLayout.Cancel(); // A saved configuration is never a first run.
    if (!path) return false;
    EnsureInit();

    wchar_t nameBuf[128]{};
    GetPrivateProfileStringW(L"KeyboardLayout", L"PresetName", L"", nameBuf, 128, path);
    if (nameBuf[0])
    {
        int idx = FindPresetByName(ResolveSavedPresetName(nameBuf));
        if (idx >= 0)
        {
            ActivatePreset(idx);
            return true;
        }
    }

    ActivatePreset(0);
    return true;
}

bool KeyboardLayout_SaveToIni(const wchar_t* path)
{
    if (!path) return false;
    EnsureInit();

    bool ok = WritePrivateProfileStringW(L"KeyboardLayout", nullptr, nullptr, path) != FALSE;
    ok &= WritePrivateProfileStringW(
        L"KeyboardLayout",
        L"PresetName",
        g_presets[ClampPreset(g_currentPresetIdx)].name.c_str(),
        path) != FALSE;
    return ok;
}

void KeyboardLayout_ArmFirstRunSelection()
{
    g_firstRunLayout.Arm();
}

bool KeyboardLayout_TryFirstRunSelection(bool searchCompleted, const BackendAnalogTelemetry& t)
{
    if (!g_firstRunLayout.Pending() || !searchCompleted || !t.pluginHostReady ||
        !t.pluginHostSnapshotGeneration || t.pluginHostLastPublishAgeMs > 1000) return false;
    int nativeCount = 0;
    for (int i = 0; i < t.nativeProtocolCount && i < kBackendMaxNativeProtocols; ++i)
        if (t.nativeProtocols[i].connected) ++nativeCount;
    // Backend and host telemetry can arrive on adjacent ticks. Do not consume
    // a provisional zero or partial source list as a completed decision.
    if (t.pluginDeviceCount != t.pluginHostDenseDeviceCount ||
        t.deviceCount != t.pluginDeviceCount + nativeCount) return false;
    const wchar_t* preset = nullptr;
    if (t.deviceCount == 1 && t.pluginDeviceCount == 1)
    {
        const auto& d = t.pluginDevices[0];
        if (d.present && (d.flags & BackendAnalogDeviceFlag_Connected) &&
            !(d.flags & BackendAnalogDeviceFlag_DuplicateSafeId))
        {
            preset = halljoy::layout_selection::Match(d.vendorId, d.productId,
                d.usagePage, d.usage, d.rows, d.columns);
            if (!preset)
                preset = halljoy::layout_selection::MatchDrunkDeer(d.vendorId,d.productId,
                    d.rows,d.columns,(d.flags & BackendAnalogDeviceFlag_VerifiedModel)!=0,
                    std::string_view(d.name,strnlen_s(d.name,sizeof(d.name))));
        }
    }
    if (t.deviceCount == 1 && nativeCount == 1 && t.pluginDeviceCount == 0)
        for (int i=0;i<t.nativeProtocolCount && i<kBackendMaxNativeProtocols;++i)
            if (t.nativeProtocols[i].connected)
                preset = halljoy::layout_identity::Match(t.nativeProtocols[i].verifiedLayoutToken);
    if (!g_firstRunLayout.Consume(true, (unsigned)t.deviceCount, preset != nullptr)) return false;
    EnsureInit();
    const int index = FindPresetByName(preset);
    if (index < 0) return false;
    ActivatePreset(index); // Shared overlay snapshot follows unless explicitly selected.
    return true;
}
