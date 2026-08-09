#pragma once

#include <atomic>
#include <cstdint>

namespace halljoy::publication
{
    // The release increment publishes all preceding setting writes. A worker
    // that observes the new generation with acquire semantics can safely
    // rebuild its thread-local snapshot from those settings.
    class Generation final
    {
    public:
        [[nodiscard]] std::uint64_t Observe() const noexcept
        {
            return value_.load(std::memory_order_acquire);
        }

        void Publish() noexcept
        {
            value_.fetch_add(1u, std::memory_order_release);
        }

    private:
        std::atomic<std::uint64_t> value_{ 1 };
    };
}
