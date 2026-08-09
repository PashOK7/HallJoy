#include "aula_win60he_diagnostic_metrics.h"

#include <algorithm>
#include <limits>

namespace aula_win60he
{
namespace
{
std::size_t LatencyBucket(std::uint32_t value) noexcept
{
    constexpr std::array<std::uint32_t, kDiagnosticLatencyBuckets - 1u> limits{
        2'000u, 4'000u, 8'000u, 16'000u, 33'000u, 100'000u };
    for (std::size_t index = 0; index < limits.size(); ++index)
        if (value <= limits[index])
            return index;
    return limits.size();
}

std::size_t ActiveBucket(std::uint32_t activeKeys) noexcept
{
    if (activeKeys == 0) return 0;
    if (activeKeys == 1) return 1;
    if (activeKeys <= 4) return 2;
    if (activeKeys <= 9) return 3;
    return 4;
}

void UpdateMinimum(std::uint32_t* target, std::uint32_t value) noexcept
{
    if (target && value != 0 && (*target == 0 || value < *target))
        *target = value;
}

void UpdateMinimum(std::uint16_t* target, std::uint16_t value) noexcept
{
    if (target && value != 0 && (*target == 0 || value < *target))
        *target = value;
}
}

std::uint64_t DiagnosticWindow::RateMilliHz() const noexcept
{
    if (elapsedUs == 0 || updates == 0) return 0;
    return updates * 1'000'000'000ull / elapsedUs;
}

std::uint64_t DiagnosticWindow::AverageIntervalUs() const noexcept
{
    return intervalSamples != 0 ? intervalSumUs / intervalSamples : 0;
}

std::uint64_t DiagnosticWindow::AverageTransactionUs() const noexcept
{
    return updates != 0 ? transactionSumUs / updates : 0;
}

void DiagnosticMetrics::Begin(std::uint64_t nowUs) noexcept
{
    *this = DiagnosticMetrics{};
    beginUs_ = nowUs;
    windowBeginUs_ = nowUs;
}

void DiagnosticMetrics::AddSample(
    DiagnosticWindow* target,
    std::uint32_t activeKeys,
    bool changed,
    std::uint16_t minimumPositiveUm,
    std::uint16_t maximumUm,
    std::uint32_t intervalUs,
    std::uint32_t transactionUs,
    bool pressTransition,
    bool releaseToZeroTransition) noexcept
{
    if (!target) return;
    ++target->updates;
    if (changed) ++target->changedUpdates;
    if (activeKeys != 0) ++target->nonzeroUpdates;
    target->currentActiveKeys = activeKeys;
    target->maximumActiveKeys = std::max(target->maximumActiveKeys, activeKeys);
    ++target->activeBuckets[ActiveBucket(activeKeys)];
    if (pressTransition) ++target->pressTransitions;
    if (releaseToZeroTransition) ++target->releaseToZeroTransitions;
    UpdateMinimum(&target->minimumPositiveUm, minimumPositiveUm);
    target->maximumUm = std::max(target->maximumUm, maximumUm);
    if (intervalUs != 0)
    {
        ++target->intervalSamples;
        target->intervalSumUs += intervalUs;
        UpdateMinimum(&target->minimumIntervalUs, intervalUs);
        target->maximumIntervalUs = std::max(target->maximumIntervalUs, intervalUs);
    }
    target->transactionSumUs += transactionUs;
    UpdateMinimum(&target->minimumTransactionUs, transactionUs);
    target->maximumTransactionUs = std::max(target->maximumTransactionUs, transactionUs);
    ++target->transactionBuckets[LatencyBucket(transactionUs)];
}

DiagnosticObservation DiagnosticMetrics::Observe(
    const KeyMap& map,
    const TravelMatrix& travel,
    bool changed,
    std::uint64_t completedUs,
    std::uint32_t transactionUs) noexcept
{
    DiagnosticObservation observation{};
    std::array<std::uint16_t, 256> currentByHid{};
    std::array<std::uint8_t, 256> rows{};
    std::array<std::uint8_t, 256> columns{};
    for (std::size_t row = 0; row < kRows; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            const std::uint8_t hid = map[row][column];
            if (!IsPublishableKeyboardUsage(hid)) continue;
            const std::uint16_t value = travel[row][column];
            if (value > currentByHid[hid])
            {
                currentByHid[hid] = value;
                rows[hid] = static_cast<std::uint8_t>(row);
                columns[hid] = static_cast<std::uint8_t>(column);
            }
        }
    }

    for (std::size_t hid = 1; hid < currentByHid.size(); ++hid)
    {
        const std::uint16_t value = currentByHid[hid];
        if (value == 0) continue;
        if (observation.activeCount < observation.active.size())
        {
            auto& active = observation.active[observation.activeCount++];
            active.hid = static_cast<std::uint8_t>(hid);
            active.row = rows[hid];
            active.column = columns[hid];
            active.travelUm = value;
        }
        UpdateMinimum(&observation.minimumPositiveUm, value);
        observation.maximumUm = std::max(observation.maximumUm, value);
        if (maximumByHid_[hid] == 0) ++observedHids_;
        maximumByHid_[hid] = std::max(maximumByHid_[hid], value);
    }

    const auto activeKeys = static_cast<std::uint32_t>(observation.activeCount);
    observation.firstNonzero = activeKeys != 0 && !sawNonzero_;
    observation.newActiveMaximum = activeKeys > lifetime_.maximumActiveKeys;
    observation.firstTenPlus = activeKeys >= 10u && !sawTenPlus_;
    sawNonzero_ = sawNonzero_ || activeKeys != 0;
    sawTenPlus_ = sawTenPlus_ || activeKeys >= 10u;
    const bool pressTransition = previousActiveKeys_ == 0 && activeKeys != 0;
    const bool releaseToZeroTransition = previousActiveKeys_ != 0 && activeKeys == 0;
    const std::uint32_t intervalUs = previousCompletionUs_ != 0 && completedUs > previousCompletionUs_
        ? static_cast<std::uint32_t>(std::min<std::uint64_t>(
            completedUs - previousCompletionUs_, std::numeric_limits<std::uint32_t>::max()))
        : 0u;
    previousCompletionUs_ = completedUs;
    previousActiveKeys_ = activeKeys;
    AddSample(&lifetime_, activeKeys, changed, observation.minimumPositiveUm,
        observation.maximumUm, intervalUs, transactionUs,
        pressTransition, releaseToZeroTransition);
    AddSample(&window_, activeKeys, changed, observation.minimumPositiveUm,
        observation.maximumUm, intervalUs, transactionUs,
        pressTransition, releaseToZeroTransition);
    return observation;
}

bool DiagnosticMetrics::WindowReady(std::uint64_t nowUs) const noexcept
{
    return nowUs >= windowBeginUs_ &&
        nowUs - windowBeginUs_ >= kDiagnosticHealthWindowUs;
}

DiagnosticWindow DiagnosticMetrics::TakeWindow(std::uint64_t nowUs) noexcept
{
    DiagnosticWindow result = window_;
    result.elapsedUs = nowUs >= windowBeginUs_ ? nowUs - windowBeginUs_ : 0;
    window_ = DiagnosticWindow{};
    window_.currentActiveKeys = previousActiveKeys_;
    windowBeginUs_ = nowUs;
    return result;
}

DiagnosticWindow DiagnosticMetrics::Lifetime(std::uint64_t nowUs) const noexcept
{
    DiagnosticWindow result = lifetime_;
    result.elapsedUs = nowUs >= beginUs_ ? nowUs - beginUs_ : 0;
    return result;
}

const std::array<std::uint16_t, 256>& DiagnosticMetrics::MaximumByHid() const noexcept
{
    return maximumByHid_;
}

std::uint64_t DiagnosticMetrics::TotalUpdates() const noexcept
{
    return lifetime_.updates;
}

std::uint32_t DiagnosticMetrics::MaximumActiveKeys() const noexcept
{
    return lifetime_.maximumActiveKeys;
}

std::uint32_t DiagnosticMetrics::ObservedHids() const noexcept
{
    return observedHids_;
}
}
