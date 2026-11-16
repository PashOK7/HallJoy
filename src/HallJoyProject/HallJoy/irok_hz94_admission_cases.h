#pragma once
#include "irok_hz94_profile.h"
#include <array>
namespace irok_hz94 {
constexpr unsigned kAdmissionCaseCount = 8;
constexpr unsigned kAdmissionSessionCaseCount = 12; // Also incomplete/conflicting/absent/malformed metadata transactions.
inline irok_nd75::PositionToHid AdmissionCase(unsigned scenario, const irok_nd75::PositionToHid& factory = kFactoryMap) {
    auto map = factory;
    unsigned filled = 0;
    switch (scenario) {
    case 1: // Log38 aggregates: all 94 exact keys, ten extra assignments.
    case 2: // Extra bytes need not be HID codes; never become physical channels.
        for (std::size_t pos = 0; pos < map.size() && filled < 10; ++pos)
            if (!factory[pos]) { map[pos] = scenario == 1 ? 4 : 255; ++filled; }
        break;
    case 3: map.fill(4); break; // Every assignment aliased, including unused slots.
    case 4: map.fill(0); break; // All digital keys disabled; sensors still have physical positions.
    case 5: map.fill(0); map[3 * 22 + 2] = 4; break;
    case 6: for (std::size_t pos=0; pos<map.size(); ++pos) if (factory[pos]) map[pos]=255; break;
    case 7: map[3 * 22 + 3] = 4; break; // Two physical keys assigned to A.
    default: break;
    }
    return map;
}
}
