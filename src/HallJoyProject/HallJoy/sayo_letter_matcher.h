#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace halljoy::sayo
{

// Correlates independent O3C physical-edge and keyboard-letter streams. This
// object intentionally owns no HID mapping: callers apply an association only
// after it returns one unambiguous physical index.
class SayoLetterMatcher final
{
public:
    static constexpr std::uint8_t kNoCandidate = 0xffu;
    static constexpr std::size_t kPhysicalIndexCount = 16u;
    static constexpr std::uint64_t kNoTimestamp = ~std::uint64_t{ 0 };

    void Reset() noexcept
    {
        pendingSinceMs_.fill(kNoTimestamp);
        down_.fill(false);
    }

    void ObservePhysicalEdge(std::uint8_t index, bool down, std::uint64_t nowMs) noexcept
    {
        if (index >= kPhysicalIndexCount)
            return;
        down_[index] = down;
        pendingSinceMs_[index] = down ? nowMs : kNoTimestamp;
    }

    void Expire(std::uint64_t nowMs, std::uint64_t windowMs) noexcept
    {
        for (std::size_t index = 0; index < kPhysicalIndexCount; ++index)
        {
            const std::uint64_t since = pendingSinceMs_[index];
            if (since == kNoTimestamp || nowMs < since)
                continue;
            if (nowMs - since > windowMs)
                pendingSinceMs_[index] = kNoTimestamp;
        }
    }

    [[nodiscard]] std::uint8_t TakeUniqueCandidate(
        std::uint64_t nowMs, std::uint64_t windowMs) noexcept
    {
        Expire(nowMs, windowMs);
        std::uint8_t candidate = kNoCandidate;
        for (std::size_t index = 0; index < kPhysicalIndexCount; ++index)
        {
            const std::uint64_t since = pendingSinceMs_[index];
            if (!down_[index] || since == kNoTimestamp || nowMs < since)
                continue;
            if (candidate != kNoCandidate)
                return kNoCandidate;
            candidate = static_cast<std::uint8_t>(index);
        }
        if (candidate != kNoCandidate)
            pendingSinceMs_[candidate] = kNoTimestamp;
        return candidate;
    }

private:
    std::array<std::uint64_t, kPhysicalIndexCount> pendingSinceMs_{};
    std::array<bool, kPhysicalIndexCount> down_{};
};

} // namespace halljoy::sayo
