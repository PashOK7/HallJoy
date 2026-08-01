#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace halljoy::uap
{
    // Deadline pacing limits fast vendor-request loops without adding delay
    // after an already-slow transaction. Failures back off from the completed
    // transaction so a temporarily unhealthy USB endpoint cannot spin.
    class PollPacingPolicy
    {
    public:
        static constexpr std::uint32_t kDefaultTargetIntervalUs = 1000;
        static constexpr std::uint32_t kDefaultMaximumFailureBackoffUs = 64000;

        explicit constexpr PollPacingPolicy(
            std::uint32_t target_interval_us = kDefaultTargetIntervalUs,
            std::uint32_t maximum_failure_backoff_us = kDefaultMaximumFailureBackoffUs) noexcept
            : target_interval_us_(target_interval_us),
              maximum_failure_backoff_us_(maximum_failure_backoff_us)
        {
        }

        constexpr void BeginCycle(std::uint64_t now_us) noexcept
        {
            cycle_started_us_ = now_us;
        }

        [[nodiscard]] constexpr std::uint32_t CompleteCycle(
            std::uint64_t now_us,
            bool succeeded) noexcept
        {
            if (target_interval_us_ == 0)
            {
                failure_streak_ = succeeded ? 0u : failure_streak_;
                return 0;
            }

            if (succeeded)
            {
                failure_streak_ = 0;
                const std::uint64_t deadline = cycle_started_us_ >
                    std::numeric_limits<std::uint64_t>::max() - target_interval_us_
                    ? std::numeric_limits<std::uint64_t>::max()
                    : cycle_started_us_ + target_interval_us_;
                if (now_us >= deadline)
                {
                    return 0;
                }
                return static_cast<std::uint32_t>(deadline - now_us);
            }

            if (failure_streak_ != std::numeric_limits<std::uint32_t>::max())
            {
                ++failure_streak_;
            }
            std::uint32_t delay_us = target_interval_us_;
            for (std::uint32_t i = 0; i < failure_streak_ && delay_us < maximum_failure_backoff_us_; ++i)
            {
                const std::uint64_t doubled = static_cast<std::uint64_t>(delay_us) * 2u;
                delay_us = static_cast<std::uint32_t>((std::min)(
                    doubled, static_cast<std::uint64_t>(maximum_failure_backoff_us_)));
            }
            return delay_us;
        }

        [[nodiscard]] constexpr std::uint32_t failure_streak() const noexcept
        {
            return failure_streak_;
        }

    private:
        std::uint32_t target_interval_us_ = kDefaultTargetIntervalUs;
        std::uint32_t maximum_failure_backoff_us_ = kDefaultMaximumFailureBackoffUs;
        std::uint64_t cycle_started_us_ = 0;
        std::uint32_t failure_streak_ = 0;
    };
}
