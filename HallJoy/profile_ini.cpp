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

static std::wstring MaskToCsvForPad(int padIndex, GameButton b)
{
    std::wstring s;

    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunkForPad(padIndex, b, chunk);
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

    f << L"[General]\n";
    f << L"Pads=" << BINDINGS_MAX_GAMEPADS << L"\n\n";

    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
    {
        const int padNum = pad + 1;

        // [PadN_Axes]
        f << L"[Pad" << padNum << L"_Axes]\n";
        auto wAxis = [&](Axis a, const wchar_t* name)
            {
                AxisBinding b = Bindings_GetAxisForPad(pad, a);
                f << name << L"_Minus=" << b.minusHid << L"\n";
                f << name << L"_Plus=" << b.plusHid << L"\n";
            };
        wAxis(Axis::LX, L"LX");
        wAxis(Axis::LY, L"LY");
        wAxis(Axis::RX, L"RX");
        wAxis(Axis::RY, L"RY");
        f << L"\n";

        // [PadN_Triggers]
        f << L"[Pad" << padNum << L"_Triggers]\n";
        f << L"LT=" << Bindings_GetTriggerForPad(pad, Trigger::LT) << L"\n";
        f << L"RT=" << Bindings_GetTriggerForPad(pad, Trigger::RT) << L"\n";
        f << L"\n";

        // [PadN_Buttons]
        f << L"[Pad" << padNum << L"_Buttons]\n";
        auto wBtn = [&](GameButton b, const wchar_t* name)
            {
                std::wstring csv = MaskToCsvForPad(pad, b);
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
    }

    return true;
}

bool Profile_SaveIni(const wchar_t* path)
{
    if (!path) return false;

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

static void LoadButtonCsvForPad(int padIndex, const wchar_t* section, GameButton b, const wchar_t* keyName, const wchar_t* path)
{
    wchar_t buf[2048]{};
    DWORD n = GetPrivateProfileStringW(section, keyName, L"", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])), path);

    if (n == 0 || buf[0] == 0)
        return;

    std::vector<uint16_t> hids;
    ParseHidList256(buf, hids);
    for (uint16_t hid : hids)
        Bindings_AddButtonHidForPad(padIndex, b, hid);
}

// NEW: full reset of axes/triggers + button masks (HID<256)
static void ResetAllBindingsBeforeLoad()
{
    // Axes/triggers could contain HID>=256; clear those explicitly for each pad.
    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
    {
        Bindings_SetAxisMinusForPad(pad, Axis::LX, 0); Bindings_SetAxisPlusForPad(pad, Axis::LX, 0);
        Bindings_SetAxisMinusForPad(pad, Axis::LY, 0); Bindings_SetAxisPlusForPad(pad, Axis::LY, 0);
        Bindings_SetAxisMinusForPad(pad, Axis::RX, 0); Bindings_SetAxisPlusForPad(pad, Axis::RX, 0);
        Bindings_SetAxisMinusForPad(pad, Axis::RY, 0); Bindings_SetAxisPlusForPad(pad, Axis::RY, 0);

        Bindings_SetTriggerForPad(pad, Trigger::LT, 0);
        Bindings_SetTriggerForPad(pad, Trigger::RT, 0);
    }

    // Clear button masks + any axis/trigger references for HID<256 (fast enough, only 255 ops)
    for (uint16_t hid = 1; hid < 256; ++hid)
        Bindings_ClearHid(hid);
}

static void LoadPadBindingsFromSections(int padIndex, const wchar_t* axesSection, const wchar_t* triggersSection, const wchar_t* buttonsSection, const wchar_t* path)
{
    auto rAxis = [&](Axis a, const wchar_t* name)
        {
            std::wstring k1 = std::wstring(name) + L"_Minus";
            std::wstring k2 = std::wstring(name) + L"_Plus";
            uint16_t minusHid = ReadU16(axesSection, k1.c_str(), 0, path);
            uint16_t plusHid = ReadU16(axesSection, k2.c_str(), 0, path);
            Bindings_SetAxisMinusForPad(padIndex, a, minusHid);
            Bindings_SetAxisPlusForPad(padIndex, a, plusHid);
        };

    rAxis(Axis::LX, L"LX");
    rAxis(Axis::LY, L"LY");
    rAxis(Axis::RX, L"RX");
    rAxis(Axis::RY, L"RY");

    Bindings_SetTriggerForPad(padIndex, Trigger::LT, ReadU16(triggersSection, L"LT", 0, path));
    Bindings_SetTriggerForPad(padIndex, Trigger::RT, ReadU16(triggersSection, L"RT", 0, path));

    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::A, L"A", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::B, L"B", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::X, L"X", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::Y, L"Y", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::LB, L"LB", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::RB, L"RB", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::Back, L"Back", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::Start, L"Start", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::Guide, L"Guide", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::LS, L"LS", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::RS, L"RS", path);

    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::DpadUp, L"DpadUp", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::DpadDown, L"DpadDown", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::DpadLeft, L"DpadLeft", path);
    LoadButtonCsvForPad(padIndex, buttonsSection, GameButton::DpadRight, L"DpadRight", path);
}

bool Profile_LoadIni(const wchar_t* path)
{
    if (!path) return false;

    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;

    ResetAllBindingsBeforeLoad();

    for (int pad = 0; pad < BINDINGS_MAX_GAMEPADS; ++pad)
    {
        wchar_t secAxes[32]{};
        wchar_t secTriggers[32]{};
        wchar_t secButtons[32]{};
        swprintf_s(secAxes, L"Pad%d_Axes", pad + 1);
        swprintf_s(secTriggers, L"Pad%d_Triggers", pad + 1);
        swprintf_s(secButtons, L"Pad%d_Buttons", pad + 1);
        LoadPadBindingsFromSections(pad, secAxes, secTriggers, secButtons, path);
    }

    return true;
}
