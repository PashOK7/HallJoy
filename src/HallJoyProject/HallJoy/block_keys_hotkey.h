#pragma once
#include <windows.h>
#include "block_keys_policy.h"
namespace halljoy::block_keys {
// Register replacement first under the spare ID. A conflict must not destroy
// the previously working shortcut. IDs are private to HallJoy's main window.
class HotkeyRegistration {
    HWND owner_ = nullptr;
    unsigned chord_ = 0;
    int id_ = 0x4b10;
public:
    unsigned Chord() const noexcept { return chord_; }
    bool Matches(WPARAM id, LPARAM detail) const noexcept {
        return chord_ && id == static_cast<WPARAM>(id_) &&
            LOWORD(detail) == (chord_ >> 8) && HIWORD(detail) == (chord_ & 255);
    }
    DWORD Apply(HWND owner, unsigned chord) noexcept {
        if (!owner || !ValidShortcut(chord)) return ERROR_INVALID_PARAMETER;
        if (owner_ == owner && chord_ == chord) return ERROR_SUCCESS;
        const int next = id_ == 0x4b10 ? 0x4b11 : 0x4b10;
        if (chord && !RegisterHotKey(owner, next, (chord >> 8) | MOD_NOREPEAT, chord & 255))
            return GetLastError();
        if (chord_ && owner_) UnregisterHotKey(owner_, id_);
        owner_ = owner; chord_ = chord; id_ = next;
        return ERROR_SUCCESS;
    }
    void Stop() noexcept {
        if (chord_ && owner_) UnregisterHotKey(owner_, id_);
        owner_ = nullptr; chord_ = 0;
    }
    bool Reserves(unsigned vk) const noexcept {
        if (!chord_) return false;
        const auto mods = chord_ >> 8;
        if (vk == (chord_ & 255)) {
            unsigned down = 0;
            if (GetAsyncKeyState(VK_MENU) & 0x8000) down |= MOD_ALT;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) down |= MOD_CONTROL;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) down |= MOD_SHIFT;
            if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) down |= MOD_WIN;
            return down == mods; // A bound letter stays blocked outside its chord.
        }
        return
            ((mods & MOD_ALT) && (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)) ||
            ((mods & MOD_CONTROL) && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) ||
            ((mods & MOD_SHIFT) && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)) ||
            ((mods & MOD_WIN) && (vk == VK_LWIN || vk == VK_RWIN));
    }
};
}
