#pragma once
#include <cstdint>

// Per-virtual-pad output scheduler. The first changed report after an idle
// period is sent immediately. Further changes inside the minimum interval are
// coalesced into the newest report and become due at a fixed deadline measured
// from the previous successful send. No input sample is fabricated or dropped:
// the backend rebuilds the report at the deadline and sends the latest state.
class VigemOutputScheduler
{
public:
    enum class Decision : std::uint8_t
    {
        NoChange,
        SendNow,
        DeferUntilDeadline,
    };

    void Configure(std::uint64_t minimumIntervalTicks)
    {
        minimumIntervalTicks_ = minimumIntervalTicks > 0 ? minimumIntervalTicks : 1;
        Reset();
    }

    void Reset()
    {
        sentValid_ = false;
        deferred_ = false;
        lastSentTick_ = 0;
        dueTick_ = 0;
    }

    Decision Evaluate(bool reportChanged, std::uint64_t nowTick)
    {
        if (!reportChanged)
        {
            // The newest state returned to the last successfully sent report.
            // Any previously deferred update is therefore obsolete.
            deferred_ = false;
            dueTick_ = 0;
            return Decision::NoChange;
        }

        if (!sentValid_ || nowTick >= SaturatingAdd(lastSentTick_, minimumIntervalTicks_))
        {
            deferred_ = false;
            dueTick_ = 0;
            return Decision::SendNow;
        }

        deferred_ = true;
        dueTick_ = SaturatingAdd(lastSentTick_, minimumIntervalTicks_);
        return Decision::DeferUntilDeadline;
    }

    void MarkSent(std::uint64_t sendTick)
    {
        sentValid_ = true;
        deferred_ = false;
        lastSentTick_ = sendTick;
        dueTick_ = 0;
    }

    bool HasDeferred() const { return deferred_; }
    std::uint64_t DueTick() const { return deferred_ ? dueTick_ : 0; }
    std::uint64_t LastSentTick() const { return sentValid_ ? lastSentTick_ : 0; }

private:
    static std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b)
    {
        const std::uint64_t sum = a + b;
        return sum < a ? UINT64_MAX : sum;
    }

    std::uint64_t minimumIntervalTicks_ = 1;
    std::uint64_t lastSentTick_ = 0;
    std::uint64_t dueTick_ = 0;
    bool sentValid_ = false;
    bool deferred_ = false;
};
