#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream> // for fast file writing

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "profile_ini.h"
#include "bindings.h"
#include "ini_util.h"

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static uint16_t ReadU16(const wchar_t* section, const wchar_t* key, uint16_t def, const wchar_t* path)
{
    return (uint16_t)GetPrivateProfileIntW(section, key, def, path);
}

static bool IsSep(wchar_t c)
{
    return (c == L',' || c == L';' || c == L' ' || c == L'\t' || c == L'\r' || c == L'\n');
}

static void ParseHidList256(const wchar_t* s, std::vector<uint16_t>& out)
{
    out.clear();
    if (!s) return;

    const wchar_t* p = s;
    while (*p)
    {
        while (*p && IsSep(*p)) ++p;
        if (!*p) break;

        wchar_t* end = nullptr;

        // FIX: base=0 allows "0x.." as well as decimal
        unsigned long v = wcstoul(p, &end, 0);

        if (end == p)
        {
            ++p;
            continue;
        }

        if (v > 0 && v < 256)
            out.push_back((uint16_t)v);

        p = end;
    }

    // remove duplicates
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

static std::wstring MaskToCsv(GameButton b)
{
    std::wstring s;

    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunk(b, chunk);
        if (!bits) continue;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        while (bits)
        {
            unsigned long idx = 0;
            _BitScanForward64(&idx, bits);
            bits &= (bits - 1);

            uint16_t hid = (uint16_t)(chunk * 64 + (int)idx);
            if (!s.empty()) s += L",";
            s += std::to_wstring((unsigned)hid);
        }
#else
        for (int bit = 0; bit < 64; ++bit)
        {
            if (bits & (1ULL << bit))
            {
                uint16_t hid = (uint16_t)(chunk * 64 + bit);
                if (!s.empty()) s += L",";
                s += std::to_wstring((unsigned)hid);
            }
        }
#endif
    }

    return s;
}

// NEW: Optimized saving using streams instead of slow WritePrivateProfileString
static bool Profile_SaveIni_Internal(const wchar_t* path)
{
    std::wofstream f(path, std::ios::out | std::ios::trunc);
    if (!f.is_open()) return false;

    // [Axes]
    f << L"[Axes]\n";
    auto wAxis = [&](Axis a, const wchar_t* name)
        {
            AxisBinding b = Bindings_GetAxis(a);
            f << name << L"_Minus=" << b.minusHid << L"\n";
            f << name << L"_Plus=" << b.plusHid << L"\n";
        };
    wAxis(Axis::LX, L"LX");
    wAxis(Axis::LY, L"LY");
    wAxis(Axis::RX, L"RX");
    wAxis(Axis::RY, L"RY");
    f << L"\n";

    // [Triggers]
    f << L"[Triggers]\n";
    f << L"LT=" << Bindings_GetTrigger(Trigger::LT) << L"\n";
    f << L"RT=" << Bindings_GetTrigger(Trigger::RT) << L"\n";
    f << L"\n";

    // [Buttons]
    f << L"[Buttons]\n";
    auto wBtn = [&](GameButton b, const wchar_t* name)
        {
            std::wstring csv = MaskToCsv(b);
            f << name << L"=" << csv << L"\n";
        };

    wBtn(GameButton::A, L"A");
    wBtn(GameButton::B, L"B");
    wBtn(GameButton::X, L"X");
    wBtn(GameButton::Y, L"Y");
    wBtn(GameButton::LB, L"LB");
    wBtn(GameButton::RB, L"RB");
    wBtn(GameButton::Back, L"Back");
    wBtn(GameButton::Start, L"Start");
    wBtn(GameButton::Guide, L"Guide");
    wBtn(GameButton::LS, L"LS");
    wBtn(GameButton::RS, L"RS");

    wBtn(GameButton::DpadUp, L"DpadUp");
    wBtn(GameButton::DpadDown, L"DpadDown");
    wBtn(GameButton::DpadLeft, L"DpadLeft");
    wBtn(GameButton::DpadRight, L"DpadRight");
    f << L"\n";

    return true;
}

bool Profile_SaveIni(const wchar_t* path)
{
    std::wstring tmp = std::wstring(path) + L".tmp";

    // Save to tmp file first
    if (!Profile_SaveIni_Internal(tmp.c_str()))
    {
        DeleteFileW(tmp.c_str());
        return false;
    }

    // Atomic replace
    return IniUtil_AtomicReplace(tmp.c_str(), path);
}

static void LoadButtonCsvOrSingle(GameButton b, const wchar_t* keyName, const wchar_t* path)
{
    // Try CSV string first
    wchar_t buf[2048]{};
    DWORD n = GetPrivateProfileStringW(L"Buttons", keyName, L"", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])), path);

    std::vector<uint16_t> hids;
    if (n > 0 && buf[0] != 0)
    {
        ParseHidList256(buf, hids);
        for (uint16_t hid : hids)
            Bindings_AddButtonHid(b, hid);
        return;
    }

    // Backward compatibility: old single integer format
    uint16_t one = (uint16_t)GetPrivateProfileIntW(L"Buttons", keyName, 0, path);
    if (one > 0 && one < 256)
        Bindings_AddButtonHid(b, one);
}

// NEW: full reset of axes/triggers + button masks (HID<256)
static void ResetAllBindingsBeforeLoad()
{
    // FIX: axes/triggers could contain HID>=256; previous loop did not clear those.
    Bindings_SetAxisMinus(Axis::LX, 0); Bindings_SetAxisPlus(Axis::LX, 0);
    Bindings_SetAxisMinus(Axis::LY, 0); Bindings_SetAxisPlus(Axis::LY, 0);
    Bindings_SetAxisMinus(Axis::RX, 0); Bindings_SetAxisPlus(Axis::RX, 0);
    Bindings_SetAxisMinus(Axis::RY, 0); Bindings_SetAxisPlus(Axis::RY, 0);

    Bindings_SetTrigger(Trigger::LT, 0);
    Bindings_SetTrigger(Trigger::RT, 0);

    // Clear button masks + any axis/trigger references for HID<256 (fast enough, only 255 ops)
    for (uint16_t hid = 1; hid < 256; ++hid)
        Bindings_ClearHid(hid);
}

bool Profile_LoadIni(const wchar_t* path)
{
    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;

    ResetAllBindingsBeforeLoad();

    auto rAxis = [&](Axis a, const wchar_t* name)
        {
            std::wstring k1 = std::wstring(name) + L"_Minus";
            std::wstring k2 = std::wstring(name) + L"_Plus";
            uint16_t minusHid = ReadU16(L"Axes", k1.c_str(), 0, path);
            uint16_t plusHid = ReadU16(L"Axes", k2.c_str(), 0, path);
            Bindings_SetAxisMinus(a, minusHid);
            Bindings_SetAxisPlus(a, plusHid);
        };

    rAxis(Axis::LX, L"LX");
    rAxis(Axis::LY, L"LY");
    rAxis(Axis::RX, L"RX");
    rAxis(Axis::RY, L"RY");

    Bindings_SetTrigger(Trigger::LT, ReadU16(L"Triggers", L"LT", 0, path));
    Bindings_SetTrigger(Trigger::RT, ReadU16(L"Triggers", L"RT", 0, path));

    LoadButtonCsvOrSingle(GameButton::A, L"A", path);
    LoadButtonCsvOrSingle(GameButton::B, L"B", path);
    LoadButtonCsvOrSingle(GameButton::X, L"X", path);
    LoadButtonCsvOrSingle(GameButton::Y, L"Y", path);
    LoadButtonCsvOrSingle(GameButton::LB, L"LB", path);
    LoadButtonCsvOrSingle(GameButton::RB, L"RB", path);
    LoadButtonCsvOrSingle(GameButton::Back, L"Back", path);
    LoadButtonCsvOrSingle(GameButton::Start, L"Start", path);
    LoadButtonCsvOrSingle(GameButton::Guide, L"Guide", path);
    LoadButtonCsvOrSingle(GameButton::LS, L"LS", path);
    LoadButtonCsvOrSingle(GameButton::RS, L"RS", path);

    LoadButtonCsvOrSingle(GameButton::DpadUp, L"DpadUp", path);
    LoadButtonCsvOrSingle(GameButton::DpadDown, L"DpadDown", path);
    LoadButtonCsvOrSingle(GameButton::DpadLeft, L"DpadLeft", path);
    LoadButtonCsvOrSingle(GameButton::DpadRight, L"DpadRight", path);

    return true;
}