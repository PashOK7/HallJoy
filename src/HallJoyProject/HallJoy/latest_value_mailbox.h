#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace halljoy::output
{

// Single-producer/single-consumer latest-value mailbox. Neither side waits for
// the other: contention returns Busy and the caller retries on its own wake or
// tick. The tiny flag protects only a value copy and is never held across I/O.
template <typename T>
class LatestValueMailbox
{
    static_assert(std::is_trivially_copyable_v<T>);

public:
    enum class ReadResult : std::uint8_t
    {
        Busy,
        Unchanged,
        Updated,
    };

    [[nodiscard]] bool TryPublish(const T& value, std::uint64_t* generation = nullptr) noexcept
    {
        if (locked_.test_and_set(std::memory_order_acquire))
            return false;

        value_ = value;
        pending_ = true;
        std::uint64_t next = published_.load(std::memory_order_relaxed) + 1u;
        if (next == 0)
            next = 1;
        published_.store(next, std::memory_order_release);
        locked_.clear(std::memory_order_release);
        if (generation)
            *generation = next;
        return true;
    }

    template <typename Merge>
    [[nodiscard]] bool TryPublishMerged(
        const T& value,
        Merge&& merge,
        std::uint64_t* generation = nullptr) noexcept
    {
        if (locked_.test_and_set(std::memory_order_acquire))
            return false;

        T nextValue = value;
        if (pending_)
            merge(value_, nextValue);
        value_ = nextValue;
        pending_ = true;
        std::uint64_t next = published_.load(std::memory_order_relaxed) + 1u;
        if (next == 0)
            next = 1;
        published_.store(next, std::memory_order_release);
        locked_.clear(std::memory_order_release);
        if (generation)
            *generation = next;
        return true;
    }

    [[nodiscard]] ReadResult TryReadAfter(
        std::uint64_t observed,
        T* value,
        std::uint64_t* generation) noexcept
    {
        if (!value || !generation)
            return ReadResult::Unchanged;
        if (locked_.test_and_set(std::memory_order_acquire))
            return ReadResult::Busy;

        const std::uint64_t current = published_.load(std::memory_order_acquire);
        if (!pending_ || current == observed)
        {
            locked_.clear(std::memory_order_release);
            return ReadResult::Unchanged;
        }

        *value = value_;
        *generation = current;
        pending_ = false;
        locked_.clear(std::memory_order_release);
        return ReadResult::Updated;
    }

    void DiscardPending() noexcept
    {
        while (locked_.test_and_set(std::memory_order_acquire))
        {
        }
        pending_ = false;
        locked_.clear(std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t PublishedGeneration() const noexcept
    {
        return published_.load(std::memory_order_acquire);
    }

private:
    std::atomic_flag locked_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> published_{ 0 };
    bool pending_ = false;
    T value_{};
};

} // namespace halljoy::output
