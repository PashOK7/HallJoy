#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_pinned_owners.h"

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t kValues = 256;
    constexpr std::size_t kCapacity = 8;

    struct FakeDevice final
    {
        explicit FakeDevice(std::atomic<std::uint64_t>& destroyed_counter)
            : destroyed(destroyed_counter)
        {
        }

        ~FakeDevice()
        {
            destroyed.fetch_add(1, std::memory_order_relaxed);
        }

        void Publish(std::uint32_t next_generation)
        {
            std::lock_guard<std::recursive_mutex> lock(snapshot_mutex);
            generation = next_generation;
            values.fill(static_cast<float>(next_generation % 1024u));
        }

        std::pair<std::uint32_t, std::array<float, kValues>> CopySnapshot()
        {
            std::lock_guard<std::recursive_mutex> lock(snapshot_mutex);
            return { generation, values };
        }

        std::recursive_mutex snapshot_mutex;
        std::uint32_t generation = 0;
        std::array<float, kValues> values{};
        std::atomic<std::uint64_t>& destroyed;
    };
}

int main()
{
    using Owner = std::shared_ptr<FakeDevice>;
    using Registry = std::vector<Owner>;

    std::atomic<std::uint64_t> destroyed{ 0 };
    std::recursive_mutex registry_mutex;
    Registry registry;

    // Capacity/request clamping is part of the production helper contract.
    for (std::size_t i = 0; i < kCapacity + 4; ++i)
        registry.emplace_back(std::make_shared<FakeDevice>(destroyed));
    {
        const auto all = halljoy::uap::PinOwners<kCapacity>(registry_mutex, registry, 100);
        assert(all.count == kCapacity);
        const auto three = halljoy::uap::PinOwners<kCapacity>(registry_mutex, registry, 3);
        assert(three.count == 3);
    }
    {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex);
        registry.clear();
    }
    assert(destroyed.load(std::memory_order_relaxed) == kCapacity + 4);

    // A reader pins the sole owner and then blocks on the device mutex. Registry
    // removal must still complete, while destruction must wait for the reader.
    registry.emplace_back(std::make_shared<FakeDevice>(destroyed));
    const auto destroyed_before_blocked_read = destroyed.load(std::memory_order_relaxed);
    std::mutex phase_mutex;
    std::condition_variable phase_cv;
    bool owner_pinned = false;
    std::unique_lock<std::recursive_mutex> block_snapshot(registry.front()->snapshot_mutex);
    std::thread blocked_reader([&]() {
        auto pinned = halljoy::uap::PinOwners<kCapacity>(registry_mutex, registry, 1);
        assert(pinned.count == 1);
        {
            std::lock_guard<std::mutex> lock(phase_mutex);
            owner_pinned = true;
        }
        phase_cv.notify_one();
        const auto snapshot = pinned.owners[0]->CopySnapshot();
        assert(snapshot.first == 0);
    });
    {
        std::unique_lock<std::mutex> lock(phase_mutex);
        phase_cv.wait(lock, [&]() { return owner_pinned; });
    }
    {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex);
        registry.clear();
    }
    assert(destroyed.load(std::memory_order_relaxed) == destroyed_before_blocked_read);
    block_snapshot.unlock();
    blocked_reader.join();
    assert(destroyed.load(std::memory_order_relaxed) == destroyed_before_blocked_read + 1);

    // Per-device locking must publish a coherent generation and 256-value body
    // while production-style owner capture happens concurrently.
    registry.emplace_back(std::make_shared<FakeDevice>(destroyed));
    std::atomic_bool writer_done{ false };
    std::thread writer([&]() {
        for (std::uint32_t generation = 1; generation <= 50000; ++generation)
            registry.front()->Publish(generation);
        writer_done.store(true, std::memory_order_release);
    });
    std::uint64_t coherent_reads = 0;
    do
    {
        const auto pinned = halljoy::uap::PinOwners<kCapacity>(registry_mutex, registry, 1);
        assert(pinned.count == 1);
        const auto [generation, values] = pinned.owners[0]->CopySnapshot();
        const float expected = static_cast<float>(generation % 1024u);
        for (const float value : values)
            assert(value == expected);
        ++coherent_reads;
    } while (!writer_done.load(std::memory_order_acquire) || coherent_reads < 50000);
    writer.join();
    {
        std::lock_guard<std::recursive_mutex> lock(registry_mutex);
        registry.clear();
    }

    // Repeated erase-after-pin cycles make an owner lifetime regression visible
    // to ASan and verify that each object is destroyed exactly once.
    constexpr std::uint64_t lifetime_cycles = 100000;
    const auto destroyed_before_cycles = destroyed.load(std::memory_order_relaxed);
    for (std::uint64_t cycle = 0; cycle < lifetime_cycles; ++cycle)
    {
        registry.emplace_back(std::make_shared<FakeDevice>(destroyed));
        {
            const auto pinned = halljoy::uap::PinOwners<kCapacity>(registry_mutex, registry, 1);
            assert(pinned.count == 1);
            std::lock_guard<std::recursive_mutex> lock(registry_mutex);
            registry.clear();
            assert(destroyed.load(std::memory_order_relaxed) == destroyed_before_cycles + cycle);
        }
        assert(destroyed.load(std::memory_order_relaxed) == destroyed_before_cycles + cycle + 1);
    }

    const Registry empty_registry;
    const auto empty = halljoy::uap::PinOwners<kCapacity>(registry_mutex, empty_registry, 8);
    assert(empty.count == 0);

    std::cout << "UAP_SNAPSHOT_PINNING_TEST=PASS coherent_reads=" << coherent_reads
              << " lifetime_cycles=" << lifetime_cycles
              << " blocked_removal=1 exact_destruction=1\n";
    return 0;
}
