#pragma once
#include <array>
#include <cstdint>

// One shortcut system for every application command (Block Bound Keys toggle,
// Pause/Resume). A key counts as held while its digital event OR its analog
// depth says so; the two signals of one physical key never produce two presses.
// Gamepad-bound keys whose digital event is blocked (by HallJoy or keyboard
// firmware) therefore still work through analog depth, and chords may mix
// both, e.g. a digital Ctrl with an analog W.
//
// Packed shortcut: bits 0..7 main key HID usage (4..231), bits 8..11 required
// modifiers using the Windows MOD_* values. Zero means unassigned.
namespace halljoy::shortcuts {

constexpr unsigned kAlt = 1, kCtrl = 2, kShift = 4, kWin = 8;
constexpr unsigned kEscape = 41;
constexpr unsigned kAnalogOnMilli = 120, kAnalogOffMilli = 60;
constexpr unsigned kCaptureCancelled = 0x10000u;

constexpr unsigned Key(unsigned s) noexcept { return s & 255u; }
constexpr unsigned Mods(unsigned s) noexcept { return (s >> 8) & 15u; }
constexpr unsigned Make(unsigned key, unsigned mods) noexcept { return (key & 255u) | ((mods & 15u) << 8); }
constexpr bool IsModifier(unsigned hid) noexcept { return hid >= 224 && hid <= 231; }
constexpr unsigned ModifierBit(unsigned hid) noexcept {
    switch (hid) {
    case 224: case 228: return kCtrl;
    case 225: case 229: return kShift;
    case 226: case 230: return kAlt;
    case 227: case 231: return kWin;
    default: return 0;
    }
}
constexpr bool ValidKey(unsigned hid) noexcept { return hid >= 4 && hid <= 231; }
// A lone modifier is a valid shortcut; a modifier cannot also require modifiers.
constexpr bool Valid(unsigned s) noexcept {
    if (!s) return true;
    if (s >> 12) return false;
    const unsigned key = Key(s);
    return ValidKey(key) && !(IsModifier(key) && Mods(s));
}

enum class Action : std::uint8_t { None = 0, BlockToggle, PauseToggle, Pause, Resume,
    NextProfile, AutoProfiles, ProfileSlot0 };
// Game profile commands: next profile, return to automatic, and one slot per
// profile that has its own shortcut (game_profile_rules.h kMaxProfileShortcuts).
constexpr unsigned kProfileSlots = 12;
constexpr unsigned kActionCount = 6 + kProfileSlots;
constexpr Action ProfileSlot(unsigned i) noexcept { return static_cast<Action>(static_cast<unsigned>(Action::ProfileSlot0) + i); }
constexpr int ProfileSlotIndex(Action a) noexcept {
    const unsigned v = static_cast<unsigned>(a), first = static_cast<unsigned>(Action::ProfileSlot0);
    return v >= first && v < first + kProfileSlots ? static_cast<int>(v - first) : -1;
}
using Bindings = std::array<unsigned, kActionCount>; // index = Action - 1

class Engine {
public:
    // Digital key event from the low-level hook. Returns true when this
    // digital event belongs to a press that triggered a command and must not
    // reach other applications. Repeats and the release follow the down.
    template <class Applicable, class IsBound, class Emit>
    bool Digital(unsigned hid, bool down, const Bindings& bindings,
                 Applicable&& applicable, IsBound&& isBound, Emit&& emit) noexcept {
        if (!InRange(hid)) return false;
        const bool was = Pressed(hid);
        if (down) {
            if (!digital_[hid]) {
                digital_[hid] = 1;
                latched_[hid] = 0; // a new physical press
                if (!was) OnPress(hid, bindings, applicable, isBound, emit);
            }
            if (!route_[hid]) route_[hid] = consumed_[hid] ? 2 : 1;
            return route_[hid] == 2;
        }
        const bool swallow = route_[hid] == 2;
        route_[hid] = 0;
        digital_[hid] = 0;
        if (was && !Pressed(hid)) OnRelease(hid);
        return swallow;
    }

    // Analog depth sample in milli-units with press/release hysteresis.
    template <class Applicable, class IsBound, class Emit>
    void Analog(unsigned hid, unsigned milli, const Bindings& bindings,
                Applicable&& applicable, IsBound&& isBound, Emit&& emit) noexcept {
        if (!InRange(hid)) return;
        const bool next = analog_[hid] ? milli > kAnalogOffMilli : milli >= kAnalogOnMilli;
        if (milli <= kAnalogOffMilli) latched_[hid] = 0;
        if (next == static_cast<bool>(analog_[hid])) return;
        const bool was = Pressed(hid);
        analog_[hid] = next ? 1 : 0;
        // The key that issued a command can end its digital press first while
        // it is still deep (e.g. Resume restarts analog sampling within ms);
        // that tail is the same press, not a new one.
        if (!was && next && !latched_[hid]) OnPress(hid, bindings, applicable, isBound, emit);
        else if (was && !Pressed(hid)) OnRelease(hid);
    }

    // Keys already down when the hook starts were delivered to Windows.
    void SeedDigital(unsigned hid) noexcept {
        if (!InRange(hid)) return;
        digital_[hid] = 1;
        if (!route_[hid]) route_[hid] = 1;
        stale_[hid] = 1;
    }

    // Analog samples stop when the engine pauses; a frozen "held" sample must
    // not hide the next ordinary press (for example the Resume key).
    void ResetAnalog() noexcept {
        for (unsigned hid = 0; hid < 256; ++hid) {
            if (!analog_[hid]) continue;
            analog_[hid] = 0;
            if (!Pressed(hid)) OnRelease(hid);
        }
    }

    void BeginCapture() noexcept {
        capturing_ = true;
        result_ = 0;
        pendingModifier_ = 0;
        // A key held when capture starts must be released before it counts.
        for (unsigned hid = 0; hid < 256; ++hid) stale_[hid] = Pressed(hid) ? 1 : 0;
    }
    void CancelCapture() noexcept { capturing_ = false; pendingModifier_ = 0; }
    bool Capturing() const noexcept { return capturing_; }
    // 0 while nothing was captured, kCaptureCancelled after Esc, otherwise the
    // packed shortcut. Reading consumes the result.
    unsigned TakeCapture() noexcept { const unsigned r = result_; result_ = 0; return r; }

    bool Pressed(unsigned hid) const noexcept { return InRange(hid) && (digital_[hid] || analog_[hid]); }

    // Keys whose analog depth must be sampled for the current bindings.
    template <class Visit>
    void ForEachAnalogKey(const Bindings& bindings, Visit&& visit) const noexcept {
        if (capturing_) {
            for (unsigned hid = 4; hid <= 231; ++hid) visit(hid);
            return;
        }
        bool any = false;
        std::array<bool, 256> seen{};
        for (unsigned s : bindings) {
            if (!s || !Valid(s)) continue;
            any = true;
            if (!seen[Key(s)]) { seen[Key(s)] = true; visit(Key(s)); }
        }
        // Modifiers matter for chords and for rejecting unrelated modifiers.
        if (any) for (unsigned hid = 224; hid <= 231; ++hid) if (!seen[hid]) visit(hid);
    }

private:
    static constexpr bool InRange(unsigned hid) noexcept { return hid > 0 && hid < 256; }

    template <class IsBound>
    bool ModifiersMatch(unsigned required, unsigned mainHid, IsBound& isBound) const noexcept {
        unsigned held = 0;
        for (unsigned hid = 224; hid <= 231; ++hid) {
            if (hid == mainHid || !Pressed(hid)) continue;
            const unsigned bit = ModifierBit(hid);
            if (required & bit) { held |= bit; continue; }
            // A gamepad-bound modifier (for example Shift used as sprint) is a
            // game control, not part of the chord. Any other modifier changes it.
            if (!isBound(hid)) return false;
        }
        return held == required;
    }

    template <class Applicable, class IsBound, class Emit>
    void OnPress(unsigned hid, const Bindings& bindings, Applicable& applicable,
                 IsBound& isBound, Emit& emit) noexcept {
        if (capturing_) {
            if (stale_[hid]) return;
            if (IsModifier(hid)) {
                if (!pendingModifier_) pendingModifier_ = hid;
                return;
            }
            unsigned mods = 0;
            for (unsigned m = 224; m <= 231; ++m) if (Pressed(m)) mods |= ModifierBit(m);
            result_ = (hid == kEscape && !mods) ? kCaptureCancelled : Make(hid, mods);
            capturing_ = false;
            pendingModifier_ = 0;
            return;
        }
        if (stale_[hid]) return;
        for (unsigned i = 0; i < kActionCount; ++i) {
            const unsigned s = bindings[i];
            if (!s || !Valid(s) || Key(s) != hid || !ModifiersMatch(Mods(s), hid, isBound)) continue;
            const auto action = static_cast<Action>(i + 1);
            if (!applicable(action)) continue;
            consumed_[hid] = 1;
            latched_[hid] = 1; // until analog is seen released (or a new digital press)
            emit(action);
            return; // One physical press issues at most one command.
        }
    }

    void OnRelease(unsigned hid) noexcept {
        consumed_[hid] = 0;
        stale_[hid] = 0;
        if (capturing_ && pendingModifier_ == hid) {
            result_ = Make(hid, 0);
            capturing_ = false;
            pendingModifier_ = 0;
        }
    }

    std::array<std::uint8_t, 256> digital_{}, analog_{}, route_{}, consumed_{}, stale_{}, latched_{};
    bool capturing_ = false;
    unsigned result_ = 0;
    unsigned pendingModifier_ = 0;
};

} // namespace halljoy::shortcuts
