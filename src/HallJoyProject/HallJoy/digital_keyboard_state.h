#pragma once
#include "analog_key_codes.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace halljoy::digital_keyboard
{
// UI-thread owned. Windows event ingress translates identity once; consumers
// query the canonical key domain without a second key/VK support table.
class State final
{
    std::unordered_map<std::uintptr_t, std::bitset<keycode::kCount>> devices_;
    std::array<std::size_t, keycode::kCount> held_{};
    struct Pulse { std::uintptr_t device; std::uint16_t key; std::uint64_t until; };
    std::vector<Pulse> pulses_;
public:
    void ObservePulse(std::uintptr_t device, std::uint16_t key, std::uint64_t until)
    {
        if (!keycode::IsSupported(key)) return;
        for (auto& pulse : pulses_)
            if (pulse.device == device && pulse.key == key)
            {
                pulse.until = until;
                return;
            }
        pulses_.push_back({device, key, until});
        Observe(device, key, true);
    }
    void Advance(std::uint64_t now)
    {
        for (auto it = pulses_.begin(); it != pulses_.end();)
            if (now >= it->until)
            {
                Observe(it->device, it->key, false);
                it = pulses_.erase(it);
            }
            else ++it;
    }
    void Observe(std::uintptr_t device, std::uint16_t key, bool down)
    {
        if (!keycode::IsSupported(key)) return;
        auto found = devices_.find(device);
        if (found == devices_.end())
        {
            if (!down) return;
            found = devices_.try_emplace(device).first;
        }
        auto& keys = found->second;
        if (keys.test(key) == down) return; // repeats are not new owners
        keys.set(key, down);
        if (down) ++held_[key];
        else --held_[key];
        if (keys.none()) devices_.erase(found);
    }
    void Remove(std::uintptr_t device) noexcept
    {
        pulses_.erase(std::remove_if(pulses_.begin(), pulses_.end(),
            [device](const Pulse& p) { return p.device == device; }), pulses_.end());
        const auto found = devices_.find(device);
        if (found == devices_.end()) return;
        for (std::size_t key = 1; key < held_.size(); ++key)
            if (found->second.test(key)) --held_[key];
        devices_.erase(found);
    }
    bool IsDown(std::uint16_t key) const noexcept
    {
        return keycode::IsSupported(key) && held_[key] != 0;
    }
    void Reset() noexcept { devices_.clear(); pulses_.clear(); held_.fill(0); }
};
inline State state;
}
