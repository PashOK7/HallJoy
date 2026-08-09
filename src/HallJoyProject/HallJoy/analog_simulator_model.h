#pragma once

#include <array>
#include <cstdint>

namespace halljoy::analog_simulator
{
constexpr std::uint16_t kHidA = 0x04;
constexpr std::uint16_t kHidD = 0x07;
constexpr std::uint16_t kHidS = 0x16;
constexpr std::uint16_t kHidW = 0x1A;
constexpr std::uint32_t kScenarioDurationMs = 7000;

enum class Phase : std::uint8_t
{
    Neutral,
    WRamp,
    WHold,
    WRelease,
    OpposingWS,
    OpposingAD,
    Diagonal,
    Disconnected,
    Reconnected,
    PostReconnectInput,
    SourceFault,
    Recovered,
    Complete,
};

struct Snapshot
{
    std::array<std::uint16_t, 256> milli{};
    Phase phase = Phase::Neutral;
    bool present = true;
    bool connected = true;
    bool faulted = false;
};

Snapshot Evaluate(std::uint32_t elapsedMs) noexcept;
const wchar_t* PhaseName(Phase phase) noexcept;
}
