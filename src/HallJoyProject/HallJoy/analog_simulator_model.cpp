#include "analog_simulator_model.h"

#include <algorithm>

namespace halljoy::analog_simulator
{
namespace
{
std::uint16_t Ramp(std::uint32_t elapsed, std::uint32_t begin, std::uint32_t duration,
    std::uint16_t from, std::uint16_t to) noexcept
{
    const std::uint32_t position = std::min(elapsed - begin, duration);
    const std::int32_t delta = static_cast<std::int32_t>(to) - from;
    return static_cast<std::uint16_t>(
        static_cast<std::int32_t>(from) + delta * static_cast<std::int32_t>(position) /
        static_cast<std::int32_t>(duration));
}
}

Snapshot Evaluate(std::uint32_t elapsedMs) noexcept
{
    Snapshot out{};

    if (elapsedMs < 500)
    {
        out.phase = Phase::Neutral;
    }
    else if (elapsedMs < 1500)
    {
        out.phase = Phase::WRamp;
        out.milli[kHidW] = Ramp(elapsedMs, 500, 1000, 0, 1000);
    }
    else if (elapsedMs < 2000)
    {
        out.phase = Phase::WHold;
        out.milli[kHidW] = 1000;
    }
    else if (elapsedMs < 2500)
    {
        out.phase = Phase::WRelease;
        out.milli[kHidW] = Ramp(elapsedMs, 2000, 500, 1000, 0);
    }
    else if (elapsedMs < 3000)
    {
        out.phase = Phase::OpposingWS;
        out.milli[kHidW] = 750;
        out.milli[kHidS] = 750;
    }
    else if (elapsedMs < 3500)
    {
        out.phase = Phase::OpposingAD;
        out.milli[kHidA] = 650;
        out.milli[kHidD] = 650;
    }
    else if (elapsedMs < 4000)
    {
        out.phase = Phase::Diagonal;
        out.milli[kHidW] = 800;
        out.milli[kHidD] = 600;
    }
    else if (elapsedMs < 4500)
    {
        out.phase = Phase::Disconnected;
        out.present = false;
        out.connected = false;
    }
    else if (elapsedMs < 5000)
    {
        out.phase = Phase::Reconnected;
    }
    else if (elapsedMs < 5500)
    {
        out.phase = Phase::PostReconnectInput;
        out.milli[kHidW] = 900;
    }
    else if (elapsedMs < 6000)
    {
        out.phase = Phase::SourceFault;
        out.connected = false;
        out.faulted = true;
    }
    else if (elapsedMs < 6500)
    {
        out.phase = Phase::Recovered;
    }
    else
    {
        out.phase = Phase::Complete;
    }

    return out;
}

const wchar_t* PhaseName(Phase phase) noexcept
{
    switch (phase)
    {
    case Phase::Neutral: return L"neutral";
    case Phase::WRamp: return L"w-ramp";
    case Phase::WHold: return L"w-hold";
    case Phase::WRelease: return L"w-release";
    case Phase::OpposingWS: return L"opposing-ws";
    case Phase::OpposingAD: return L"opposing-ad";
    case Phase::Diagonal: return L"diagonal";
    case Phase::Disconnected: return L"disconnected";
    case Phase::Reconnected: return L"reconnected";
    case Phase::PostReconnectInput: return L"post-reconnect-input";
    case Phase::SourceFault: return L"source-fault";
    case Phase::Recovered: return L"recovered";
    case Phase::Complete: return L"complete";
    }
    return L"unknown";
}
}
