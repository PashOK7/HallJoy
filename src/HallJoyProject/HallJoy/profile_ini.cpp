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
#include "analog_key_codes.h"
#include "bindings.h"
#include "ini_util.h"
#include "bounded_ini.h"
#include "profile_runtime_gate.h"

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static std::wstring MaskToCsvForPad(int padIndex, GameButton b)
{
    std::wstring s;

    for (int chunk = 0; chunk < Bindings_GetButtonMaskChunkCount(); ++chunk)
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

    f << L"[HallJoyPersistence]\n";
    f << L"SchemaVersion=1\n";
    f << L"Kind=Bindings\n\n";

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

    f.flush();
    if (!f.good())
    {
        f.close();
        return false;
    }
    f.close();
    return !f.fail();
}

namespace
{
    bool ProfileTransactionWrite(const wchar_t* temporaryPath, void*, DWORD* errorOut)
    {
        if (Profile_SaveIni_Internal(temporaryPath))
            return true;
        if (errorOut)
        {
            const DWORD error = GetLastError();
            *errorOut = error != ERROR_SUCCESS ? error : ERROR_WRITE_FAULT;
        }
        return false;
    }

    bool ProfileTransactionValidate(const wchar_t* temporaryPath, void*, DWORD* errorOut)
    {
        wchar_t schema[32]{};
        wchar_t kind[32]{};
        wchar_t pads[32]{};
        wchar_t firstAxis[32]{};
        GetPrivateProfileStringW(L"HallJoyPersistence", L"SchemaVersion", L"{missing}", schema, (DWORD)_countof(schema), temporaryPath);
        GetPrivateProfileStringW(L"HallJoyPersistence", L"Kind", L"{missing}", kind, (DWORD)_countof(kind), temporaryPath);
        GetPrivateProfileStringW(L"General", L"Pads", L"{missing}", pads, (DWORD)_countof(pads), temporaryPath);
        GetPrivateProfileStringW(L"Pad1_Axes", L"LX_Minus", L"{missing}", firstAxis, (DWORD)_countof(firstAxis), temporaryPath);

        BindingsSnapshot parsed;
        std::uint32_t parsedPads = 0;
        const bool ok = Profile_PrepareIni(temporaryPath, parsed) && wcscmp(schema, L"1") == 0 &&
            wcscmp(kind, L"Bindings") == 0 &&
            halljoy::ini::Unsigned(pads, BINDINGS_MAX_GAMEPADS, parsedPads) &&
            parsedPads == BINDINGS_MAX_GAMEPADS &&
            wcscmp(firstAxis, L"{missing}") != 0;
        if (!ok && errorOut) *errorOut = ERROR_INVALID_DATA;
        return ok;
    }
}

bool Profile_SaveIni(const wchar_t* path)
{
    if (!path || !*path) return false;
    const auto result = IniUtil_SaveAtomic(path, ProfileTransactionWrite, ProfileTransactionValidate, nullptr);
    if (!result.Succeeded())
    {
        IniUtil_ReportSaveFailure(L"bindings", path, result);
        return false;
    }
    return true;
}


namespace {
constexpr const wchar_t* kAxes[] = {L"LX", L"LY", L"RX", L"RY"};
constexpr const wchar_t* kButtons[] = {L"A", L"B", L"X", L"Y", L"LB", L"RB",
    L"Back", L"Start", L"Guide", L"LS", L"RS", L"DpadUp", L"DpadDown", L"DpadLeft", L"DpadRight"};
bool ReadCode(const wchar_t* path, const wchar_t* section, const wchar_t* key,
    uint16_t& code, bool required, bool& recognized) {
    std::wstring value;
    if (!halljoy::ini::Read(path, section, key, value)) return false;
    if (value.empty()) { code = 0; return !required; }
    recognized = true;
    std::uint32_t parsed = 0;
    if (!halljoy::ini::Unsigned(value, static_cast<std::uint32_t>(halljoy::keycode::kCount - 1), parsed))
        return false;
    code = static_cast<uint16_t>(parsed); return true;
}
bool ParseCodes(const std::wstring& text,
    std::array<uint64_t, halljoy::keycode::kMaskChunkCount>& mask) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto begin = text.find_first_not_of(L",; \t\r\n", pos);
        if (begin == std::wstring::npos) break;
        auto end = text.find_first_of(L",; \t\r\n", begin);
        if (end == std::wstring::npos) end = text.size();
        std::uint32_t code = 0;
        if (!halljoy::ini::Unsigned(std::wstring_view(text).substr(begin, end-begin),
                static_cast<std::uint32_t>(halljoy::keycode::kCount - 1), code) || !code)
            return false;
        mask[code / 64] |= uint64_t{1} << (code % 64);
        pos = end;
    }
    return true;
}
}

bool Profile_PrepareIni(const wchar_t* path, BindingsSnapshot& out) {
    halljoy::ini::ReadFile file(path);
    if (!file) return false;
    std::wstring schema, kind, bundle;
    if (!halljoy::ini::Read(path, L"HallJoyPersistence", L"SchemaVersion", schema) ||
        !halljoy::ini::Read(path, L"HallJoyPersistence", L"Kind", kind) ||
        !halljoy::ini::Read(path, L"HallJoyProfile", L"BundleVersion", bundle)) return false;
    const bool bundled = !bundle.empty();
    if (bundled && bundle != L"1") return false;
    if (!schema.empty() || !kind.empty()) {
        if (schema != L"1" || (bundled ? (kind != L"Settings" && kind != L"ProfileSettings")
            : kind != L"Bindings")) return false;
    } else if (bundled) return false;
    BindingsSnapshot candidate{};
    bool recognized = false;
    for (int p=0; p<BINDINGS_MAX_GAMEPADS; ++p) {
        const auto prefix = L"Pad" + std::to_wstring(p+1);
        const auto axes = prefix + L"_Axes", triggers = prefix + L"_Triggers", buttons = prefix + L"_Buttons";
        for (const auto* section : {axes.c_str(), triggers.c_str(), buttons.c_str()}) {
            wchar_t probe[4]{};
            const bool present = GetPrivateProfileSectionW(section, probe, 4, path) > 0;
            if (bundled && !present) return false;
        }
        for (int a=0; a<4; ++a) {
            if (!ReadCode(path, axes.c_str(), (std::wstring(kAxes[a])+L"_Minus").c_str(), candidate.axes[p][a].minusHid, bundled, recognized) ||
                !ReadCode(path, axes.c_str(), (std::wstring(kAxes[a])+L"_Plus").c_str(), candidate.axes[p][a].plusHid, bundled, recognized)) return false;
        }
        if (!ReadCode(path, triggers.c_str(), L"LT", candidate.triggers[p][0], bundled, recognized) ||
            !ReadCode(path, triggers.c_str(), L"RT", candidate.triggers[p][1], bundled, recognized)) return false;
        for (int b=0; b<15; ++b) {
            wchar_t presence[32]{};
            GetPrivateProfileStringW(buttons.c_str(), kButtons[b], L"{missing}", presence, 32, path);
            const bool present = wcscmp(presence, L"{missing}") != 0;
            if (bundled && !present) return false;
            recognized |= present;
            std::wstring codes;
            if (!halljoy::ini::Read(path, buttons.c_str(), kButtons[b], codes) ||
                !ParseCodes(codes, candidate.buttons[p][b])) return false;
        }
    }
    if (!recognized) return false;
    out = candidate;
    return true;
}

bool Profile_LoadIni(const wchar_t* path) {
    BindingsSnapshot candidate{};
    if (!Profile_PrepareIni(path, candidate)) return false;
    halljoy::profile_runtime::CommitLease commit;
    if (!commit) return false;
    Bindings_Apply(candidate);
    return true;
}

// Writes into the caller-owned temporary settings file: one atomic replacement
// commits settings and bindings together. Never truncates or replaces the file.
bool Profile_WriteBindingsSections(const wchar_t* path) {
    bool ok = WritePrivateProfileStringW(L"HallJoyProfile", L"BundleVersion", L"1", path) != FALSE;
    const auto pads = std::to_wstring(BINDINGS_MAX_GAMEPADS);
    ok &= WritePrivateProfileStringW(L"General", L"Pads", pads.c_str(), path) != FALSE;
    for (int p=0; p<BINDINGS_MAX_GAMEPADS; ++p) {
        const auto prefix = L"Pad" + std::to_wstring(p+1);
        const auto axes = prefix + L"_Axes", triggers = prefix + L"_Triggers", buttons = prefix + L"_Buttons";
        for (int a=0; a<4; ++a) {
            const auto binding = Bindings_GetAxisForPad(p, static_cast<Axis>(a));
            ok &= WritePrivateProfileStringW(axes.c_str(), (std::wstring(kAxes[a])+L"_Minus").c_str(),
                std::to_wstring(binding.minusHid).c_str(), path) != FALSE;
            ok &= WritePrivateProfileStringW(axes.c_str(), (std::wstring(kAxes[a])+L"_Plus").c_str(),
                std::to_wstring(binding.plusHid).c_str(), path) != FALSE;
        }
        for (int t=0; t<2; ++t)
            ok &= WritePrivateProfileStringW(triggers.c_str(), t == 0 ? L"LT" : L"RT",
                std::to_wstring(Bindings_GetTriggerForPad(p, static_cast<Trigger>(t))).c_str(), path) != FALSE;
        for (int b=0; b<15; ++b)
            ok &= WritePrivateProfileStringW(buttons.c_str(), kButtons[b],
                MaskToCsvForPad(p, static_cast<GameButton>(b)).c_str(), path) != FALSE;
    }
    return ok;
}
