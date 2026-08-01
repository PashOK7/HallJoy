#pragma once

#include "halljoy_uap_cabi_guard.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace halljoy::uap
{
    template<typename Owner, std::size_t Capacity>
    struct PinnedOwnerList final
    {
        std::array<Owner, Capacity> owners{};
        std::size_t count = 0;
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
        pinned.count = (std::min)({ requested, Capacity, source.size() });
        std::copy_n(source.begin(), pinned.count, pinned.owners.begin());
        return pinned;
    }
}
