#pragma once
#include "midi_touch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace halljoy::midi {
struct Note { std::uint8_t velocity = 0, pressure = 0; };
constexpr std::uint32_t Message(unsigned status, unsigned address, unsigned value) noexcept {
    return (status | (address / 128)) | ((address % 128) << 8) | (value << 16);
}
constexpr unsigned Address(std::uint32_t message) noexcept {
    return (message & 15) * 128 + ((message >> 8) & 127);
}
// One realtime owner. No sleeping, driver calls, allocations or digital fallback.
// Timing is observed analog motion on the acquisition consumer, not sensor force.
class Engine {
    std::array<AttackState, keycode::kCount> keys_{};
    std::array<Note, kAddressCount> notes_{};
    std::uint64_t sparseAttacks_ = 0;
    TouchReport touch_{};
    std::unique_ptr<std::array<MotionTrace,keycode::kCount>> traces_;
public:
    // Optional storage is prepared by the control owner before realtime use.
    void EnableDiagnostics() { traces_=std::make_unique<std::array<MotionTrace,keycode::kCount>>(); }
    const auto& Notes() const noexcept { return notes_; }
    std::uint64_t SparseAttacks() const noexcept { return sparseAttacks_; }
    const TouchReport& Touch() const noexcept { return touch_; }
    template<class Emit> void Reset(Emit&& emit) {
        for (unsigned a = 0; a < notes_.size(); ++a)
            if (notes_[a].velocity) emit(Message(0x80, a, 0));
        notes_.fill({}); keys_.fill({});
        if(traces_)traces_->fill({});
    }
    template<class Read, class Emit>
    void Tick(const Profile& config, std::uint64_t nowUs, Read&& read, Emit&& emit,
              AttackSink diagnostics = {}) {
        std::array<Note, kAddressCount> desired{};
        for (std::size_t hid = 1; hid < config.keys.size(); ++hid) {
            auto& key = keys_[hid];
            const unsigned code = config.keys[hid];
            if (!config.enabled || !code || code > kAddressCount) { key = {}; continue; }
            auto input = read(static_cast<std::uint16_t>(hid));
            input.snapshot = true; // This compatibility route samples at the consumer.
            if (!input.available) { key = {}; continue; }
            const bool wasDown = key.down;
            auto* trace=traces_?&(*traces_)[hid]:nullptr;
            if (!ObserveAttack(key,config,nowUs,input,sparseAttacks_,trace)) continue;
            if (!wasDown && key.down) {
                RecordTouch(touch_, key);
                if(trace)diagnostics(key,*trace,config,{0,0,0,static_cast<std::uint16_t>(hid),
                    static_cast<std::uint16_t>(hid),static_cast<std::uint16_t>(code)});
            }
            const int depth=std::clamp(input.milli,0,1000);
            if (key.down && key.velocity) {
                auto& note = desired[code - 1];
                note.velocity = std::max(note.velocity, key.velocity);
                note.pressure = std::max(note.pressure,
                    static_cast<std::uint8_t>((depth * 127 + 500) / 1000));
            }
        }
        // Release before attack. Aliases of one note stay sounding until all release.
        for (unsigned a = 0; a < notes_.size(); ++a)
            if (notes_[a].velocity && !desired[a].velocity) emit(Message(0x80, a, 0));
        for (unsigned a = 0; a < notes_.size(); ++a) {
            if (desired[a].velocity && !notes_[a].velocity)
                emit(Message(0x90, a, desired[a].velocity));
            if (desired[a].velocity && notes_[a].velocity) desired[a].velocity = notes_[a].velocity;
            if (config.depthPressure && desired[a].velocity &&
                (!notes_[a].velocity || desired[a].pressure != notes_[a].pressure))
                emit(Message(0xA0, a, desired[a].pressure));
        }
        notes_ = desired;
    }
};
}
