#pragma once

#include "aula_win60he_protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aula_win60he
{
constexpr std::uint64_t kDiagnosticHealthWindowUs = 5'000'000ull;
constexpr std::size_t kDiagnosticLatencyBuckets = 7u;

struct DiagnosticActiveValue
{
    std::uint8_t hid = 0;
    std::uint8_t row = 0;
    std::uint8_t column = 0;
    std::uint16_t travelUm = 0;
};

struct DiagnosticObservation
{
    std::array<DiagnosticActiveValue, kExpectedPublishableDefaultKeys> active{};
    std::size_t activeCount = 0;
    std::uint16_t minimumPositiveUm = 0;
    std::uint16_t maximumUm = 0;
    bool firstNonzero = false;
    bool newActiveMaximum = false;
    bool firstTenPlus = false;
};

struct DiagnosticWindow
{
    std::uint64_t elapsedUs = 0;
    std::uint64_t updates = 0;
    std::uint64_t changedUpdates = 0;
    std::uint64_t nonzeroUpdates = 0;
    std::uint64_t intervalSamples = 0;
    std::uint64_t intervalSumUs = 0;
    std::uint64_t transactionSumUs = 0;
    std::uint32_t minimumIntervalUs = 0;
    std::uint32_t maximumIntervalUs = 0;
    std::uint32_t minimumTransactionUs = 0;
    std::uint32_t maximumTransactionUs = 0;
    std::array<std::uint64_t, kDiagnosticLatencyBuckets> transactionBuckets{};
    std::array<std::uint64_t, 5> activeBuckets{};
    std::uint32_t maximumActiveKeys = 0;
    std::uint32_t currentActiveKeys = 0;
    std::uint64_t pressTransitions = 0;
    std::uint64_t releaseToZeroTransitions = 0;
    std::uint16_t minimumPositiveUm = 0;
    std::uint16_t maximumUm = 0;

    [[nodiscard]] std::uint64_t RateMilliHz() const noexcept;
    [[nodiscard]] std::uint64_t AverageIntervalUs() const noexcept;
    [[nodiscard]] std::uint64_t AverageTransactionUs() const noexcept;
};

class DiagnosticMetrics final
{
public:
    void Begin(std::uint64_t nowUs) noexcept;
    DiagnosticObservation Observe(
        const KeyMap& map,
        const TravelMatrix& travel,
        bool changed,
        std::uint64_t completedUs,
        std::uint32_t transactionUs) noexcept;
    [[nodiscard]] bool WindowReady(std::uint64_t nowUs) const noexcept;
    DiagnosticWindow TakeWindow(std::uint64_t nowUs) noexcept;
    [[nodiscard]] DiagnosticWindow Lifetime(std::uint64_t nowUs) const noexcept;
    [[nodiscard]] const std::array<std::uint16_t, 256>& MaximumByHid() const noexcept;
    [[nodiscard]] std::uint64_t TotalUpdates() const noexcept;
    [[nodiscard]] std::uint32_t MaximumActiveKeys() const noexcept;
    [[nodiscard]] std::uint32_t ObservedHids() const noexcept;

private:
    static void AddSample(
        DiagnosticWindow* target,
        std::uint32_t activeKeys,
        bool changed,
        std::uint16_t minimumPositiveUm,
        std::uint16_t maximumUm,
        std::uint32_t intervalUs,
        std::uint32_t transactionUs,
        bool pressTransition,
        bool releaseToZeroTransition) noexcept;

    std::uint64_t beginUs_ = 0;
    std::uint64_t windowBeginUs_ = 0;
    std::uint64_t previousCompletionUs_ = 0;
    DiagnosticWindow lifetime_{};
    DiagnosticWindow window_{};
    std::array<std::uint16_t, 256> maximumByHid_{};
    std::uint32_t observedHids_ = 0;
    std::uint32_t previousActiveKeys_ = 0;
    bool sawNonzero_ = false;
    bool sawTenPlus_ = false;
};
}
