#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include "irok_nd75_protocol.h"

namespace na87diag {
using Report = irok_nd75::Report;
using Mask = std::array<std::uint8_t, 22>;
constexpr unsigned Slots = 132;
enum class Command { Identity, Capability, Subscribe, Unsubscribe, Map, Configuration };

inline Report Request(Command kind, std::uint8_t id, const Mask& mask = {},
    unsigned row = 0, unsigned column = 0) {
    Report r{};
    r[0] = id;
    if (kind == Command::Identity) r[1] = 0x0d;
    else if (kind == Command::Map) r[1] = 0x10;
    else {
        r[1] = 0x21; r[5] = 0x18;
        switch (kind) {
        case Command::Capability: r[6] = 4; break;
        case Command::Subscribe:
            r[6] = 2;
            if (std::any_of(mask.begin(), mask.end(), [](auto b) { return b > 63; }))
                throw std::invalid_argument("mask outside six-row matrix");
            std::copy(mask.begin(), mask.end(), r.begin() + 7);
            break;
        case Command::Unsubscribe: r[6] = 3; break;
        case Command::Configuration:
            if (row >= 6 || column >= 22) throw std::invalid_argument("configuration coordinate");
            r[6] = 5; r[7] = static_cast<std::uint8_t>(row);
            r[8] = static_cast<std::uint8_t>(column); break;
        default: throw std::invalid_argument("command");
        }
    }
    return r;
}

// The transport admits only the exact shapes produced above. No generic send API.
inline bool Allowed(const Report& r) {
    try {
        if (r[1] == 0x0d) return r == Request(Command::Identity, r[0]);
        if (r[1] == 0x10) return r == Request(Command::Map, r[0]);
        if (r[1] != 0x21) return false;
        if (r[6] == 4) return r == Request(Command::Capability, r[0]);
        if (r[6] == 3) return r == Request(Command::Unsubscribe, r[0]);
        if (r[6] == 5) return r == Request(Command::Configuration, r[0], {}, r[7], r[8]);
        if (r[6] == 2) {
            Mask m{}; std::copy_n(r.begin() + 7, 22, m.begin());
            return r == Request(Command::Subscribe, r[0], m);
        }
    } catch (const std::invalid_argument&) {}
    return false;
}

inline Mask All() { Mask m{}; m.fill(63); return m; }
inline Mask Only(unsigned slot) {
    if (slot >= Slots) throw std::invalid_argument("slot");
    Mask m{}; m[slot % 22] = static_cast<std::uint8_t>(1u << (slot / 22)); return m;
}
inline Report Normalized(Report r) { r[0] = 1; return r; }
inline bool IsNa87(const irok_nd75::DeviceInfo& info) {
    return std::string_view(info.controller.data()) == "M484" &&
        std::string_view(info.product.data()) == "GK8260HERGB";
}

struct Metrics {
    std::array<unsigned, Slots> positive{}, zero{}, changes{};
    std::array<int, Slots> last{};
    unsigned events = 0, invalid = 0;
    Metrics() { last.fill(-1); }
    void Feed(const Report& raw) {
        auto r = Normalized(raw);
        // Count only the known event envelope. Other replies stay in the raw log.
        if (r[2] || r[3] || r[4] || (r[5] != 3 && r[5] != 4)) return;
        irok_nd75::LiveEvent e{};
        if (!irok_nd75::DecodeLiveEvent(r.data(), r.size(), &e)) return;
        ++events;
        if (e.travel > 40) { ++invalid; return; }
        const unsigned slot = e.row * 22u + e.column;
        if (e.travel) ++positive[slot]; else ++zero[slot];
        if (last[slot] != e.travel) ++changes[slot];
        last[slot] = e.travel;
    }
    std::optional<unsigned> Learn() const {
        std::optional<unsigned> found;
        for (unsigned i = 0; i < Slots; ++i) if (positive[i]) {
            if (found || positive[i] < 2) return {};
            found = i;
        }
        return found;
    }
};
}
