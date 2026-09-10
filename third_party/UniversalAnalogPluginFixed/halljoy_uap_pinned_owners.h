#pragma once

#include "halljoy_uap_cabi_guard.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace halljoy::uap
{
    template<typename Owner, std::size_t Capacity>
    struct PinnedOwnerList final
    {
        std::array<Owner, Capacity> owners{};
        std::size_t count = 0;
        std::size_t required_count = 0;
    };

    // Copy only ref-counted owners while the registry is locked. The returned
    // pins keep every selected object alive after this function releases the
    // registry mutex, so callers may wait on per-object locks independently.
    template<std::size_t Capacity, typename Mutex, typename Container>
    auto PinOwners(Mutex& registry_mutex, const Container& source, std::size_t requested)
    {
        static_assert(Capacity != 0);
        using Owner = typename Container::value_type;

        PinnedOwnerList<Owner, Capacity> pinned{};
        LockGuard<Mutex> registry_lock(registry_mutex);
        pinned.required_count = source.size();
        pinned.count = (std::min)({ requested, Capacity, pinned.required_count });
        std::copy_n(source.begin(), pinned.count, pinned.owners.begin());
        return pinned;
    }

    // Move-only lease over caller-owned reusable storage. Destroying the lease
    // clears every copied ref-counted owner, so growing buffers may retain their
    // allocation but can never retain a removed device between snapshots.
    template<typename Owner>
    class PinnedOwnerLease final
    {
    public:
        PinnedOwnerLease() = default;
        PinnedOwnerLease(Owner* owner_buffer, std::size_t copied,
            std::size_t required) noexcept
            : owners(owner_buffer), count(copied), required_count(required)
        {
        }

        PinnedOwnerLease(PinnedOwnerLease&& other) noexcept
            : owners(std::exchange(other.owners, nullptr)),
              count(std::exchange(other.count, 0)),
              required_count(std::exchange(other.required_count, 0))
        {
        }

        PinnedOwnerLease& operator=(PinnedOwnerLease&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                owners = std::exchange(other.owners, nullptr);
                count = std::exchange(other.count, 0);
                required_count = std::exchange(other.required_count, 0);
            }
            return *this;
        }

        ~PinnedOwnerLease() noexcept
        {
            Reset();
        }

        PinnedOwnerLease(const PinnedOwnerLease&) = delete;
        PinnedOwnerLease& operator=(const PinnedOwnerLease&) = delete;

        void Reset() noexcept
        {
            while (count != 0)
                owners[--count] = Owner{};
            owners = nullptr;
            required_count = 0;
        }

        Owner* owners = nullptr;
        std::size_t count = 0;
        std::size_t required_count = 0;
    };

    // Observe demand separately so the caller can grow reusable storage before
    // entering the capture transaction. This lock scope performs no allocation.
    template<typename Mutex, typename Container>
    std::size_t QueryOwnerCount(Mutex& registry_mutex, const Container& source)
    {
        LockGuard<Mutex> registry_lock(registry_mutex);
        return source.size();
    }

    // Copy ref-counted owners into caller-provisioned storage while the registry
    // is locked. Storage allocation/growth is deliberately outside this helper.
    template<typename Mutex, typename Container>
    auto PinOwnersInto(Mutex& registry_mutex, const Container& source,
        std::size_t requested, typename Container::value_type* owner_buffer,
        std::size_t owner_capacity)
    {
        using Owner = typename Container::value_type;
        if (!owner_buffer)
            owner_capacity = 0;

        LockGuard<Mutex> registry_lock(registry_mutex);
        const std::size_t required = source.size();
        const std::size_t copied =
            (std::min)({ requested, owner_capacity, required });
        if (copied != 0)
            std::copy_n(source.begin(), copied, owner_buffer);
        return PinnedOwnerLease<Owner>(owner_buffer, copied, required);
    }
}
