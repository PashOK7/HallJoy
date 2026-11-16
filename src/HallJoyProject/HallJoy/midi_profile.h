#pragma once
#include "analog_key_codes.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace halljoy::midi {
constexpr unsigned kNoteCount = 128, kChannelCount = 16;
constexpr unsigned kAddressCount = kNoteCount * kChannelCount;
constexpr unsigned kDefaultVelocityAxisMaximum = 70000;
// Zero is unbound; 1..2048 encode (channel * 128 + note) + 1.
struct Profile {
    bool enabled = false;
    int paletteOctave = 4;
    int paletteChannel = 0;
    std::wstring port; // Exact Windows port name, empty means monitor only.
    int touch = 1; // Version 1 profile compatibility only.
    int attackMilli = 200;
    bool depthPressure = true;
    unsigned touchVersion = 2;
    int releaseMilli = 50;
    int noiseMilli = 2;
    unsigned fullSpeedMilli = 0; // Version 1 profile compatibility only.
    // Version 2 stores independent absolute X coordinates and normalized Y values.
    std::array<unsigned, 5> velocitySpeed{0, kDefaultVelocityAxisMaximum / 4,
        kDefaultVelocityAxisMaximum / 2, kDefaultVelocityAxisMaximum * 3 / 4,
        kDefaultVelocityAxisMaximum};
    std::array<int, 5> velocityCurve{0, 250, 500, 750, 1000};
    unsigned velocityAxisMaximum = kDefaultVelocityAxisMaximum; // View only, in permille travel/second.
    std::array<std::uint16_t, keycode::kCount> keys{};
};
inline const std::shared_ptr<const Profile> defaults = std::make_shared<const Profile>();
inline std::atomic<std::shared_ptr<const Profile>> profile{defaults};
inline std::shared_ptr<const Profile> Get() noexcept { return profile.load(std::memory_order_acquire); }
using ChangeCallback = void (*)() noexcept;
inline std::atomic<ChangeCallback> onChanged{nullptr};
// Independent runtime notification; the Remap UI may install/remove onChanged.
inline std::atomic<ChangeCallback> onEnabledChanged{nullptr};
inline void Publish(std::shared_ptr<const Profile> next) noexcept {
#ifdef HALLJOY_DISABLE_MIDI
    profile.store(std::move(next), std::memory_order_release);
#else
    const bool enabled = next->enabled;
    const auto previous = profile.exchange(std::move(next), std::memory_order_acq_rel);
    if (previous->enabled != enabled)
        if (auto callback = onEnabledChanged.load(std::memory_order_acquire)) callback();
    if (auto callback = onChanged.load(std::memory_order_acquire)) callback();
#endif
}
inline std::wstring EncodePort(const std::wstring& port) {
    static constexpr wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring encoded;
    for (const wchar_t value : port) for (int shift : {12, 8, 4, 0}) encoded += hex[(value >> shift) & 15];
    return encoded;
}
inline bool DecodePort(const std::wstring& encoded, std::wstring& port) {
    if (encoded.size() > 1020 || encoded.size() % 4) return false;
    std::wstring candidate;
    for (std::size_t i = 0; i < encoded.size(); i += 4) {
        unsigned value = 0;
        for (unsigned j = 0; j < 4; ++j) {
            const auto digit = encoded[i + j];
            if (digit >= L'0' && digit <= L'9') value = value * 16 + digit - L'0';
            else if (digit >= L'A' && digit <= L'F') value = value * 16 + digit - L'A' + 10;
            else return false;
        }
        if (value < 32 || value == 127) return false;
        candidate += static_cast<wchar_t>(value);
    }
    port = std::move(candidate); return true;
}
inline unsigned FullVelocitySpeed(const Profile& value) noexcept {
    if(value.touchVersion==2)return value.velocitySpeed.back();
    if (value.fullSpeedMilli) return value.fullSpeedMilli;
    return value.touch == 0 ? 15000u : value.touch == 2 ? 65000u : 35000u;
}
inline unsigned LegacyCurveMilli(unsigned speed,const Profile& value) noexcept {
    const std::uint64_t reference=(std::max)(1u,FullVelocitySpeed(value));
    const unsigned normalized=static_cast<unsigned>(std::min<std::uint64_t>(1000,
        (std::uint64_t(speed)*1000+reference/2)/reference));
    const unsigned segment=(std::min)(3u,normalized/250),offset=normalized-segment*250;
    const int low=value.velocityCurve[segment],high=value.velocityCurve[segment+1];
    return static_cast<unsigned>(low+((high-low)*int(offset)+125)/250);
}
inline unsigned CurveSpeed(const Profile& value,unsigned point) noexcept {
    if(value.touchVersion==2)return value.velocitySpeed[point];
    return static_cast<unsigned>(std::uint64_t((std::max)(4u,FullVelocitySpeed(value)))*point/4);
}
inline int CurveLevel(const Profile& value,unsigned point) noexcept {
    if(value.touchVersion==2)return value.velocityCurve[point];
    return static_cast<int>((1000+126*LegacyCurveMilli(CurveSpeed(value,point),value)+63)/127);
}
inline unsigned CurveAxisMaximum(const Profile& value) noexcept {
    return value.velocityAxisMaximum;
}
inline void MakeEditableCurve(Profile& value) noexcept {
    if(value.touchVersion==2)return;
    std::array<unsigned,5> speed{};
    std::array<int,5> level{};
    for(unsigned i=0;i<5;++i){speed[i]=CurveSpeed(value,i);level[i]=CurveLevel(value,i);}
    value.velocityAxisMaximum=CurveAxisMaximum(value);
    value.velocitySpeed=speed;value.velocityCurve=level;value.touchVersion=2;
}
inline bool ValidTouch(const Profile& value) noexcept {
    if ((value.touchVersion!=1 && value.touchVersion!=2) || value.attackMilli<1 ||
        value.attackMilli>1000 || value.releaseMilli<0 || value.releaseMilli>=value.attackMilli ||
        value.noiseMilli<0 || value.noiseMilli>50 ||
        value.noiseMilli>=value.attackMilli-value.releaseMilli) return false;
    if(value.touchVersion==1 && (value.fullSpeedMilli>1000000000u ||
        value.velocityCurve.front()!=0 || value.velocityCurve.back()!=1000))return false;
    for(unsigned i=0;i<5;++i) {
        if(value.velocityCurve[i]<0 || value.velocityCurve[i]>1000 ||
           (i && value.velocityCurve[i]<value.velocityCurve[i-1]))return false;
        if(value.touchVersion==2 && (value.velocitySpeed[i]>1000000000u ||
           (i && value.velocitySpeed[i]<=value.velocitySpeed[i-1])))return false;
    }
    return value.velocityAxisMaximum > 0 && value.velocityAxisMaximum <= 1000000000u;

}
inline bool SamePerformance(const Profile& a, const Profile& b) noexcept {
    const bool curve=a.touchVersion==b.touchVersion && a.velocityCurve==b.velocityCurve &&
        (a.touchVersion==1 ? a.touch==b.touch && a.fullSpeedMilli==b.fullSpeedMilli
                          : a.velocitySpeed==b.velocitySpeed);
    return curve && a.enabled==b.enabled && a.port==b.port && a.keys==b.keys &&
        a.attackMilli==b.attackMilli && a.depthPressure==b.depthPressure &&
        a.releaseMilli==b.releaseMilli && a.noiseMilli==b.noiseMilli;
}
#ifdef HALLJOY_DISABLE_MIDI
constexpr bool NeedsAnalog() noexcept { return false; }
#else
inline bool NeedsAnalog() noexcept { return Get()->enabled; }
#endif
inline void Apply(const Profile& value) {
    std::shared_ptr<const Profile> next = std::make_shared<const Profile>(value);
    Publish(std::move(next));
}
template<class Change> void Edit(Change&& change) {
    Profile next = *Get();
    change(next);
    Apply(next);
}
inline std::uint16_t Binding(std::uint16_t hid) noexcept {
    const auto value = Get();
    return keycode::IsSupported(hid) ? value->keys[hid] : 0;
}
inline bool IsBound(std::uint16_t hid) noexcept {
    const auto value = Get();
    return value->enabled && keycode::IsSupported(hid) && value->keys[hid] != 0;
}
inline void Bind(std::uint16_t hid, unsigned encoded) {
    if (!keycode::IsSupported(hid) || encoded > kAddressCount) return;
    Edit([&](Profile& value) { value.keys[hid] = static_cast<std::uint16_t>(encoded); });
}
inline std::wstring NoteName(unsigned note) {
    static constexpr const wchar_t* names[] = {L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B"};
    if (note >= kNoteCount) return L"";
    return std::wstring(names[note % 12]) + std::to_wstring(static_cast<int>(note / 12) - 1);
}
// Common chromatic QWERTY piano map; physical HID positions work across OS layouts.
inline void PianoPreset(Profile& value) {
    static constexpr std::uint16_t lower[] = {0x1D,0x16,0x1B,0x07,0x06,0x19,0x0A,0x05,0x0B,0x11,0x0D,0x10};
    static constexpr std::uint16_t upper[] = {0x14,0x1F,0x1A,0x20,0x08,0x15,0x22,0x17,0x23,0x1C,0x24,0x18,0x0C};
    const unsigned base = (value.paletteOctave + 1) * 12;
    value.keys.fill(0);
    for (unsigned i = 0; i < 12 && base + i < 128; ++i)
        value.keys[lower[i]] = static_cast<std::uint16_t>(value.paletteChannel * 128 + base + i + 1);
    for (unsigned i = 0; i < 13 && base + i + 12 < 128; ++i)
        value.keys[upper[i]] = static_cast<std::uint16_t>(value.paletteChannel * 128 + base + i + 13);
}
}
