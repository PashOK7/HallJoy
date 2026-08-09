#include "../HallJoy/publication_generation.h"

#include <atomic>
#include <cassert>
#include <thread>

int main()
{
    halljoy::publication::Generation generation;
    std::atomic<int> payload{ 0 };
    const std::uint64_t initial = generation.Observe();

    std::thread writer([&]() {
        payload.store(42, std::memory_order_relaxed);
        generation.Publish();
    });

    while (generation.Observe() == initial)
        std::this_thread::yield();
    assert(payload.load(std::memory_order_relaxed) == 42);
    writer.join();
    return 0;
}
