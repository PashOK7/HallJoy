#include "addressed_poll_scheduler.h"

#include <algorithm>
#include <cmath>

namespace addressed
{
namespace
{
constexpr std::uint16_t kActiveThresholdMilli = 8;
constexpr std::uint16_t kMeaningfulDeltaMilli = 2;
constexpr std::uint16_t kMeaningfulRawDelta = 12;
constexpr std::uint64_t kMovingWindowUs = 30000;
constexpr std::uint64_t kRecentWindowUs = 100000;
constexpr std::size_t kBackgroundSlotsPerPacket = 2;
}

PollScheduler::PollScheduler(const PollKeyConfig* keys, std::size_t count)
{
    count_ = std::min(count, keys_.size());
    for (std::size_t i = 0; i < count_; ++i)
        keys_[i].config = keys[i];
}

void PollScheduler::Reset(std::uint64_t nowUs)
{
    deadlineMisses_ = 0;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const auto config = keys_[i].config;
        const bool bound = keys_[i].bound;
        keys_[i] = KeyState{};
        keys_[i].config = config;
        keys_[i].bound = bound;
        keys_[i].lastPollUs = nowUs;
    }
}

void PollScheduler::SetBound(std::uint16_t hidUsage, bool bound)
{
    if (!hidUsage) return;
    for (std::size_t i = 0; i < count_; ++i)
    {
        if (keys_[i].config.hidUsage == hidUsage)
            keys_[i].bound = bound;
    }
}

void PollScheduler::OnSample(
    std::uint8_t keyId,
    std::uint16_t raw,
    std::uint16_t milli,
    std::uint64_t nowUs)
{
    for (std::size_t i = 0; i < count_; ++i)
    {
        auto& key = keys_[i];
        if (key.config.keyId != keyId) continue;

        const bool changed = !key.initialised ||
            std::abs(static_cast<int>(milli) - static_cast<int>(key.milli)) >= kMeaningfulDeltaMilli ||
            std::abs(static_cast<int>(raw) - static_cast<int>(key.raw)) >= kMeaningfulRawDelta;

        key.raw = raw;
        key.milli = milli;
        key.lastPollUs = nowUs;
        key.initialised = true;
        if (changed)
        {
            key.lastChangeUs = nowUs;
            key.recentUntilUs = nowUs + kRecentWindowUs;
        }
        if (milli >= kActiveThresholdMilli)
            key.recentUntilUs = std::max(key.recentUntilUs, nowUs + kRecentWindowUs);
        return;
    }
}

PollClass PollScheduler::Classify(const KeyState& key, std::uint64_t nowUs) const
{
    if (key.bound) return PollClass::Bound;
    if (key.initialised && key.lastChangeUs && nowUs - key.lastChangeUs <= kMovingWindowUs)
        return PollClass::Moving;
    if (key.milli >= kActiveThresholdMilli)
        return PollClass::Active;
    if (key.initialised && nowUs < key.recentUntilUs)
        return PollClass::Recent;
    return PollClass::Background;
}

std::uint64_t PollScheduler::TargetIntervalUs(PollClass cls)
{
    switch (cls)
    {
    case PollClass::Bound: return 1500;
    case PollClass::Moving: return 2500;
    case PollClass::Active: return 5000;
    case PollClass::Recent: return 8000;
    case PollClass::Background: return 45000;
    }
    return 45000;
}

std::uint64_t PollScheduler::MaxAgeUs(PollClass cls)
{
    switch (cls)
    {
    case PollClass::Bound: return 5000;
    case PollClass::Moving: return 8000;
    case PollClass::Active: return 10000;
    case PollClass::Recent: return 15000;
    case PollClass::Background: return 50000;
    }
    return 50000;
}

double PollScheduler::ClassWeight(PollClass cls)
{
    switch (cls)
    {
    case PollClass::Bound: return 1.35;
    case PollClass::Moving: return 1.15;
    case PollClass::Active: return 1.00;
    case PollClass::Recent: return 0.80;
    case PollClass::Background: return 0.25;
    }
    return 0.25;
}

int PollScheduler::FindBestClass(PollClass wanted, std::uint64_t nowUs) const
{
    int best = -1;
    double bestScore = -1.0;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const auto& key = keys_[i];
        if (key.selected || Classify(key, nowUs) != wanted) continue;
        if (!key.initialised) return static_cast<int>(i);

        const std::uint64_t age = nowUs >= key.lastPollUs ? nowUs - key.lastPollUs : 0;
        const double score = static_cast<double>(age) / static_cast<double>(TargetIntervalUs(wanted));
        if (score > bestScore)
        {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int PollScheduler::FindBestLive(std::uint64_t nowUs) const
{
    int best = -1;
    double bestScore = -1.0;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const auto& key = keys_[i];
        if (key.selected) continue;
        const PollClass cls = Classify(key, nowUs);
        if (cls != PollClass::Moving && cls != PollClass::Active && cls != PollClass::Recent)
            continue;

        const std::uint64_t age = nowUs >= key.lastPollUs ? nowUs - key.lastPollUs : 0;
        const double score = ClassWeight(cls) * static_cast<double>(age) /
            static_cast<double>(TargetIntervalUs(cls));
        if (score > bestScore)
        {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int PollScheduler::FindBestPriority(std::uint64_t nowUs) const
{
    int best = -1;
    double bestScore = -1.0;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const auto& key = keys_[i];
        if (key.selected) continue;
        const PollClass cls = Classify(key, nowUs);
        if (cls == PollClass::Background) continue;

        const std::uint64_t age = nowUs >= key.lastPollUs ? nowUs - key.lastPollUs : 0;
        const double deadlineRatio = static_cast<double>(age) / static_cast<double>(MaxAgeUs(cls));
        const double targetRatio = static_cast<double>(age) / static_cast<double>(TargetIntervalUs(cls));
        // A missed maximum age dominates the normal weighted target score.
        const double score = (deadlineRatio >= 1.0 ? 1000.0 + deadlineRatio : 0.0) +
            ClassWeight(cls) * targetRatio;
        if (score > bestScore)
        {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

std::size_t PollScheduler::CountLive(std::uint64_t nowUs) const
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const PollClass cls = Classify(keys_[i], nowUs);
        if (cls == PollClass::Moving || cls == PollClass::Active || cls == PollClass::Recent)
            ++count;
    }
    return count;
}

bool PollScheduler::Add(PollPlan& plan, int index, std::uint64_t nowUs)
{
    if (index < 0 || plan.count >= plan.keyIds.size()) return false;
    auto& key = keys_[static_cast<std::size_t>(index)];
    if (key.selected) return false;

    const PollClass cls = Classify(key, nowUs);
    const std::uint64_t age = nowUs >= key.lastPollUs ? nowUs - key.lastPollUs : 0;
    if (key.initialised && age >= MaxAgeUs(cls)) ++deadlineMisses_;

    key.selected = true;
    plan.keyIds[plan.count] = key.config.keyId;
    plan.classes[plan.count] = cls;
    ++plan.count;
    return true;
}

PollPlan PollScheduler::BuildPlan(std::uint64_t nowUs)
{
    PollPlan plan{};
    for (std::size_t i = 0; i < count_; ++i) keys_[i].selected = false;

    // Initialise every physical position once. This takes as many packets as the discovered profile needs
    // and avoids publishing unknown/stale values after connect or reconnect.
    for (std::size_t i = 0; i < count_ && plan.count < kMaxKeysPerPacket; ++i)
    {
        if (!keys_[i].initialised)
            Add(plan, static_cast<int>(i), nowUs);
    }
    if (plan.count != 0) return plan;

    // Two background slots per packet sustain about 20 Hz over the complete
    // 82-key matrix at ~850 packets/s. They are reserved before high-priority
    // work so any number of binds can never starve matrix discovery.
    for (std::size_t i = 0; i < kBackgroundSlotsPerPacket; ++i)
    {
        if (!Add(plan, FindBestClass(PollClass::Background, nowUs), nowUs)) break;
    }

    // Non-bound keys that were moving, held or recently released receive a
    // guaranteed share. The share grows with the live set, but bound keys still
    // own most of the seven non-background slots.
    const std::size_t liveCount = CountLive(nowUs);
    std::size_t liveReserve = 0;
    if (liveCount != 0)
        liveReserve = 1 + std::min<std::size_t>(1, (liveCount - 1) / 9);
    for (std::size_t i = 0; i < liveReserve && plan.count < kMaxKeysPerPacket; ++i)
    {
        if (!Add(plan, FindBestLive(nowUs), nowUs)) break;
    }

    // Fill the remaining high-priority capacity by weighted earliest-deadline
    // selection. Bound keys have the shortest target interval and highest
    // weight, while old active/recent keys can still overtake them before stale.
    while (plan.count < kMaxKeysPerPacket)
    {
        if (!Add(plan, FindBestPriority(nowUs), nowUs)) break;
    }

    // Empty high-priority capacity is lent to the background sweep.
    while (plan.count < kMaxKeysPerPacket)
    {
        if (!Add(plan, FindBestClass(PollClass::Background, nowUs), nowUs)) break;
    }

    return plan;
}

PollSchedulerStats PollScheduler::GetStats(std::uint64_t nowUs) const
{
    PollSchedulerStats out{};
    out.deadlineMisses = deadlineMisses_;
    for (std::size_t i = 0; i < count_; ++i)
    {
        const auto& key = keys_[i];
        const PollClass cls = Classify(key, nowUs);
        const std::uint64_t age = key.initialised && nowUs >= key.lastPollUs ? nowUs - key.lastPollUs : 0;
        switch (cls)
        {
        case PollClass::Bound:
            ++out.boundCount;
            out.maxBoundAgeUs = std::max(out.maxBoundAgeUs, age);
            break;
        case PollClass::Moving:
            ++out.movingCount;
            out.maxActiveAgeUs = std::max(out.maxActiveAgeUs, age);
            break;
        case PollClass::Active:
            ++out.activeCount;
            out.maxActiveAgeUs = std::max(out.maxActiveAgeUs, age);
            break;
        case PollClass::Recent:
            ++out.recentCount;
            out.maxActiveAgeUs = std::max(out.maxActiveAgeUs, age);
            break;
        case PollClass::Background:
            ++out.backgroundCount;
            out.maxBackgroundAgeUs = std::max(out.maxBackgroundAgeUs, age);
            break;
        }
    }
    return out;
}

std::uint16_t PollScheduler::HidForKeyId(std::uint8_t keyId) const
{
    for (std::size_t i = 0; i < count_; ++i)
    {
        if (keys_[i].config.keyId == keyId)
            return keys_[i].config.hidUsage;
    }
    return 0;
}

bool PollScheduler::IsKnownHid(std::uint16_t hidUsage) const
{
    if (!hidUsage) return false;
    for (std::size_t i = 0; i < count_; ++i)
    {
        if (keys_[i].config.hidUsage == hidUsage)
            return true;
    }
    return false;
}
} // namespace addressed
