#pragma once
#include <array>
#include <cstdint>
#include <atomic>

namespace halljoy::input_privilege
{
// Only the documented UIPI denial establishes a message-access barrier.
// Timeout, destroyed HWND, queue quota, and token-query failure do not.
constexpr bool PostingDeniedByUipi(bool posted, unsigned error) noexcept
{
    return !posted && error == 5; // ERROR_ACCESS_DENIED
}
// One realtime producer, one UI consumer. Retain a press even if the entire
// down/up cycle occurs between UI ticks. No allocation, locks, or extra HID I/O.
class AnalogPressMailbox
{
    std::array<bool, 256> armed_{}; // realtime producer only
    std::array<std::atomic<std::uint64_t>, 256> latest_{};
public:
    void Observe(unsigned key, unsigned depth, std::uint64_t now) noexcept
    {
        if (key < 4 || key >= 232) return; // standard keyboard usages, not vendor/Fn codes
        if (depth < 100) armed_[key] = true;
        else if (depth >= 900 && armed_[key]) {
            armed_[key] = false;
            latest_[key].store(now, std::memory_order_release);
        }
    }
    std::uint64_t Latest(unsigned key) const noexcept
    {
        return key < latest_.size() ? latest_[key].load(std::memory_order_acquire) : 0;
    }
};
inline AnalogPressMailbox analogPresses;
// UI-thread only. Bounded evidence, never a record of typed text. Windows does
// not report a UIPI rejection: this is deliberately conservative inference.
class Detector
{
    struct Key {
        std::uint64_t raw = 0, hook = 0, pending = 0, seen = 0;
    };
    std::array<Key, 256> keys_{};
    unsigned missing_ = 0;
    std::uint64_t firstMissing_ = 0;
public:
    // Process-lifetime latch: only a fresh HallJoy process starts without it.
    bool warning = false;
    void Digital(unsigned key, bool hook, std::uint64_t now) noexcept
    {
        if (warning || key >= keys_.size()) return;
        (hook ? keys_[key].hook : keys_[key].raw) = now;
    }
    void ResetSession() noexcept
    {
        for (auto& key : keys_) {
            key.pending = key.raw = key.hook = 0;
        }
        missing_ = 0;
        firstMissing_ = 0;
    }
    void Sample(unsigned code, std::uint64_t pressTime, std::uint64_t now,
                bool higher, bool expectHook) noexcept
    {
        if (warning || code >= keys_.size()) return;
        auto& key = keys_[code];
        if (pressTime > key.seen) {
            key.seen = pressTime;
            if (!key.pending) key.pending = pressTime;
        }
        // Wait for either digital channel to arrive, not for a held analog key.
        if (!key.pending || now < key.pending || now - key.pending < 250) return;
        const auto began = key.pending;
        key.pending = 0;
        const auto recent = [&](std::uint64_t time) {
            return time != 0 && time <= now && time + 250 >= began;
        };
        const bool raw = recent(key.raw);
        // A working blocking hook may itself suppress subsequent Raw Input.
        const bool complete = expectHook ? recent(key.hook) : raw;
        if (complete) {
            missing_ = 0;
        } else if (higher) {
            if (!missing_ || now - firstMissing_ > 10000) {
                missing_ = 0; firstMissing_ = now;
            }
            if (++missing_ >= 3) warning = true;
        }
    }
};
inline Detector detector;
inline constexpr unsigned kChangedMessage = 0x8000 + 364;
inline constexpr wchar_t kText[] =
    L"For correct operation of keypress indicators and Block Bound Keys, "
    L"we recommend restarting HallJoy as administrator.";
}
