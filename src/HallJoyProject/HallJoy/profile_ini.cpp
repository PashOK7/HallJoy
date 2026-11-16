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
#include "ini_write_batch.h"
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

template<class Put>
static bool WriteMidiProfile(const halljoy::midi::Profile& midi, Put&& put) {
    bool ok = put(L"Midi", L"Enabled", midi.enabled ? L"1" : L"0");
    ok &= put(L"Midi", L"Octave", std::to_wstring(midi.paletteOctave));
    ok &= put(L"Midi", L"Channel", std::to_wstring(midi.paletteChannel));
    ok &= put(L"Midi", L"PortUtf16", halljoy::midi::EncodePort(midi.port));
    if(midi.touchVersion==1)ok &= put(L"Midi", L"Touch", std::to_wstring(midi.touch));
    ok &= put(L"Midi", L"AttackMilli", std::to_wstring(midi.attackMilli));
    ok &= put(L"Midi", L"DepthPressure", midi.depthPressure ? L"1" : L"0");
    ok &= put(L"Midi", L"TouchVersion", std::to_wstring(midi.touchVersion));
    ok &= put(L"Midi", L"ReleaseMilli", std::to_wstring(midi.releaseMilli));
    ok &= put(L"Midi", L"NoiseMilli", std::to_wstring(midi.noiseMilli));
    ok &= put(L"Midi",L"VelocityAxisMaximum",std::to_wstring(midi.velocityAxisMaximum));
    if(midi.touchVersion==1) {
        ok &= put(L"Midi", L"FullSpeedMilli", std::to_wstring(midi.fullSpeedMilli));
        for(unsigned i=1;i<4;++i)
            ok &= put(L"Midi",(L"Velocity"+std::to_wstring(i*25)).c_str(),std::to_wstring(midi.velocityCurve[i]));
    }else{
        for(unsigned i=0;i<5;++i) {
            ok &= put(L"Midi",(L"VelocityX"+std::to_wstring(i)).c_str(),std::to_wstring(midi.velocitySpeed[i]));
            ok &= put(L"Midi",(L"VelocityY"+std::to_wstring(i)).c_str(),std::to_wstring(midi.velocityCurve[i]));
        }
    }
    for (std::size_t hid = 1; hid < midi.keys.size(); ++hid)
        if (midi.keys[hid]) ok &= put(L"Midi", (L"K" + std::to_wstring(hid)).c_str(), std::to_wstring(midi.keys[hid]));
    return ok;
}

static bool ReadMidiProfile(const wchar_t* path, halljoy::midi::Profile& midi) {
    // A pre-MIDI profile starts from the current factory curve on first enable.
    wchar_t presence[4]{};
    if (!GetPrivateProfileSectionW(L"Midi", presence, 4, path)) return true;
    auto number = [&](const wchar_t* key, unsigned maximum, unsigned fallback, unsigned& out) {
        std::wstring value;
        if (!halljoy::ini::Read(path, L"Midi", key, value)) return false;
        if (value.empty()) { out = fallback; return true; }
        std::uint32_t parsed = 0;
        if (!halljoy::ini::Unsigned(value, maximum, parsed)) return false;
        out = parsed; return true;
    };
    unsigned enabled = 0, channel = 0, attack = 200, pressure = 1;
    std::int32_t octave = 4;
    std::wstring encodedPort;
    if (!number(L"Enabled", 1, 0, enabled) || !halljoy::ini::ReadSigned(path, L"Midi", L"Octave", -1, 9, 4, octave) ||
        !number(L"Channel", 15, 0, channel) ||
        !number(L"AttackMilli", 1000, 200, attack) || attack < 1 ||
        !number(L"DepthPressure", 1, 1, pressure) ||
        !halljoy::ini::Read(path, L"Midi", L"PortUtf16", encodedPort) ||
        !halljoy::midi::DecodePort(encodedPort, midi.port))
        return false;
    midi.enabled = enabled != 0; midi.paletteOctave = octave;
    midi.paletteChannel = channel; midi.attackMilli = attack; midi.depthPressure = pressure != 0;
    unsigned touchVersion = 1, release = 50, noise = 2;
    if (!number(L"TouchVersion", 2, 1, touchVersion) ||
        !number(L"ReleaseMilli", 999, 50, release) ||
        !number(L"NoiseMilli", 50, 2, noise) ||
        !number(L"VelocityAxisMaximum", 1000000000,
            halljoy::midi::kDefaultVelocityAxisMaximum, midi.velocityAxisMaximum)) return false;
    midi.touchVersion = touchVersion;
    midi.releaseMilli = static_cast<int>(release);
    midi.noiseMilli = static_cast<int>(noise);
    if(touchVersion==1) {
        unsigned touch=1;
        if(attack<100 || attack>800 || !number(L"Touch",2,1,touch) ||
           !number(L"FullSpeedMilli",1000000000,0,midi.fullSpeedMilli))return false;
        midi.touch=static_cast<int>(touch);
        midi.velocityCurve={0,500,707,866,1000};
        for(unsigned i=1;i<4;++i) {
            unsigned point=0;
            if(!number((L"Velocity"+std::to_wstring(i*25)).c_str(),1000,
                static_cast<unsigned>(midi.velocityCurve[i]),point))return false;
            midi.velocityCurve[i]=static_cast<int>(point);
        }
    }else{
        // Existing v2 files with incomplete coordinates used these fallbacks.
        // The writer always saves all five coordinates of a new curve.
        constexpr unsigned legacySpeedDefaults[] = {0, 25000, 50000, 75000, 100000};
        for(unsigned i=0;i<5;++i) {
            unsigned level=0;
            if(!number((L"VelocityX"+std::to_wstring(i)).c_str(),1000000000,legacySpeedDefaults[i],midi.velocitySpeed[i]) ||
               !number((L"VelocityY"+std::to_wstring(i)).c_str(),1000,static_cast<unsigned>(midi.velocityCurve[i]),level))return false;
            midi.velocityCurve[i]=static_cast<int>(level);
        }
    }
    if (!halljoy::midi::ValidTouch(midi)) return false;
    for (std::size_t hid = 1; hid < midi.keys.size(); ++hid) {
        unsigned value = 0;
        if (!number((L"K" + std::to_wstring(hid)).c_str(), halljoy::midi::kAddressCount, 0, value)) return false;
        midi.keys[hid] = static_cast<std::uint16_t>(value);
    }
    return true;
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
                const AxisBinding b = Bindings_GetAxisForPad(pad, a);
                f << name << L"_Minus=" << b.minusHids[0] << L"\n";
                f << name << L"_Plus=" << b.plusHids[0] << L"\n";
                for (std::size_t i = 1; i < b.minusHids.size(); ++i)
                    if (b.minusHids[i]) f << name << L"_Minus" << (i + 1) << L"=" << b.minusHids[i] << L"\n";
                for (std::size_t i = 1; i < b.plusHids.size(); ++i)
                    if (b.plusHids[i]) f << name << L"_Plus" << (i + 1) << L"=" << b.plusHids[i] << L"\n";
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

    f << L"[Midi]\n";
    const auto midi = halljoy::midi::Get();
    WriteMidiProfile(*midi, [&](const wchar_t*, const wchar_t* key, const std::wstring& value) {
        if (!value.empty()) f << key << L"=" << value << L"\n";
        return true;
    });
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
// One axis side: the first key stays in <Axis>_Minus/_Plus (earlier HallJoy
// versions read exactly that), further keys in <Axis>_Minus2.._Minus8 and
// <Axis>_Plus2.._Plus8. Keys are kept in order, unique, without gaps.
bool ReadAxisSide(const wchar_t* path, const wchar_t* section, const std::wstring& base,
    bool required, bool& recognized, AxisKeys& out) {
    uint16_t first = 0;
    if (!ReadCode(path, section, base.c_str(), first, required, recognized)) return false;
    AxisKeys keys{};
    std::size_t count = 0;
    const auto push = [&](uint16_t hid) {
        if (!hid || std::find(keys.begin(), keys.begin() + count, hid) != keys.begin() + count) return true;
        if (count >= keys.size()) return false;
        keys[count++] = hid;
        return true;
    };
    if (!push(first)) return false;
    for (int i = 2; i <= BINDINGS_MAX_AXIS_KEYS; ++i) {
        const auto key = base + std::to_wstring(i);
        wchar_t presence[32]{};
        GetPrivateProfileStringW(section, key.c_str(), L"{missing}", presence, 32, path);
        if (wcscmp(presence, L"{missing}") == 0) continue;
        uint16_t hid = 0;
        if (!ReadCode(path, section, key.c_str(), hid, false, recognized) || !push(hid)) return false;
    }
    out = keys;
    return true;
}

// Writes every key slot of one axis side; unused extra slots are written empty
// so a stale extra key from an earlier file can never survive.
template <class Put>
bool PutAxisSide(const std::wstring& section, const std::wstring& base, const AxisKeys& keys, Put&& put) {
    bool ok = put(section, base, std::to_wstring(keys[0]));
    for (std::size_t i = 1; i < keys.size(); ++i)
        ok &= put(section, base + std::to_wstring(i + 1), keys[i] ? std::to_wstring(keys[i]) : std::wstring{});
    return ok;
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
    // The lease pins the file: serve every key from one parse per section.
    halljoy::ini::ReadSnapshot snapshot(path);
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
            if (!ReadAxisSide(path, axes.c_str(), std::wstring(kAxes[a])+L"_Minus", bundled, recognized, candidate.axes[p][a].minusHids) ||
                !ReadAxisSide(path, axes.c_str(), std::wstring(kAxes[a])+L"_Plus", bundled, recognized, candidate.axes[p][a].plusHids)) return false;
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
    auto midi = std::make_shared<halljoy::midi::Profile>();
    if (!recognized || !ReadMidiProfile(path, *midi)) return false;
    candidate.midi = std::move(midi);
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
    const auto midi = halljoy::midi::Get();
    bool midiOk = halljoy::ini::WriteBatch::Put(L"Midi", nullptr, nullptr, path) != FALSE;
    midiOk &= WriteMidiProfile(*midi, [&](const wchar_t* section, const wchar_t* key, const std::wstring& value) {
        return halljoy::ini::WriteBatch::Put(section, key, value.c_str(), path) != FALSE;
    });
    bool ok = halljoy::ini::WriteBatch::Put(L"HallJoyProfile", L"BundleVersion", L"1", path) != FALSE;
    const auto pads = std::to_wstring(BINDINGS_MAX_GAMEPADS);
    ok &= halljoy::ini::WriteBatch::Put(L"General", L"Pads", pads.c_str(), path) != FALSE;
    for (int p=0; p<BINDINGS_MAX_GAMEPADS; ++p) {
        const auto prefix = L"Pad" + std::to_wstring(p+1);
        const auto axes = prefix + L"_Axes", triggers = prefix + L"_Triggers", buttons = prefix + L"_Buttons";
        for (int a=0; a<4; ++a) {
            const auto binding = Bindings_GetAxisForPad(p, static_cast<Axis>(a));
            const auto put = [&](const std::wstring& section, const std::wstring& key, const std::wstring& value) {
                return halljoy::ini::WriteBatch::Put(section.c_str(), key.c_str(), value.c_str(), path) != FALSE;
            };
            ok &= PutAxisSide(axes, std::wstring(kAxes[a])+L"_Minus", binding.minusHids, put);
            ok &= PutAxisSide(axes, std::wstring(kAxes[a])+L"_Plus", binding.plusHids, put);
        }
        for (int t=0; t<2; ++t)
            ok &= halljoy::ini::WriteBatch::Put(triggers.c_str(), t == 0 ? L"LT" : L"RT",
                std::to_wstring(Bindings_GetTriggerForPad(p, static_cast<Trigger>(t))).c_str(), path) != FALSE;
        for (int b=0; b<15; ++b)
            ok &= halljoy::ini::WriteBatch::Put(buttons.c_str(), kButtons[b],
                MaskToCsvForPad(p, static_cast<GameButton>(b)).c_str(), path) != FALSE;
    }
    return ok && midiOk;
}

bool Profile_WriteBindingsSnapshot(const wchar_t* path, const BindingsSnapshot& snapshot) {
    auto put = [&](const std::wstring& section, const wchar_t* key, const std::wstring& value) {
        return WritePrivateProfileStringW(section.c_str(), key, value.c_str(), path) != FALSE;
    };
    bool ok = WritePrivateProfileStringW(L"Midi", nullptr, nullptr, path) != FALSE;
    ok &= WriteMidiProfile(*snapshot.midi, [&](const wchar_t* section, const wchar_t* key, const std::wstring& value) {
        return put(section, key, value);
    });
    ok &= put(L"HallJoyProfile", L"BundleVersion", L"1");
    ok &= put(L"General", L"Pads", std::to_wstring(BINDINGS_MAX_GAMEPADS));
    for (int p=0; p<BINDINGS_MAX_GAMEPADS; ++p) {
        const auto prefix = L"Pad" + std::to_wstring(p+1);
        const auto axes = prefix + L"_Axes", triggers = prefix + L"_Triggers", buttons = prefix + L"_Buttons";
        for (int a=0; a<4; ++a) {
            const auto write = [&](const std::wstring& section, const std::wstring& key, const std::wstring& value) {
                return put(section, key.c_str(), value);
            };
            ok &= PutAxisSide(axes, std::wstring(kAxes[a])+L"_Minus", snapshot.axes[p][a].minusHids, write);
            ok &= PutAxisSide(axes, std::wstring(kAxes[a])+L"_Plus", snapshot.axes[p][a].plusHids, write);
        }
        ok &= put(triggers, L"LT", std::to_wstring(snapshot.triggers[p][0]));
        ok &= put(triggers, L"RT", std::to_wstring(snapshot.triggers[p][1]));
        for (int b=0; b<15; ++b) {
            std::wstring csv;
            const auto& mask = snapshot.buttons[p][b];
            for (std::size_t chunk=0; chunk<mask.size(); ++chunk)
                for (unsigned bit=0; bit<64; ++bit)
                    if (mask[chunk] & (uint64_t{1} << bit)) {
                        if (!csv.empty()) csv += L",";
                        csv += std::to_wstring(chunk*64 + bit);
                    }
            ok &= put(buttons, kButtons[b], csv);
        }
    }
    return ok;
}
