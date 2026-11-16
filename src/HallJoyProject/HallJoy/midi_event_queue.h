#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <utility>
namespace halljoy::midi {
// Exactly one producer and one consumer. Failed Push leaves unread events intact.
// Note edges are never overwritten by a latest-state update.
template<class Event, std::size_t Capacity> class EventQueue {
    static_assert(Capacity > 1);
    std::array<Event, Capacity> events_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
public:
    bool Push(Event event) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        events_[head] = std::move(event);
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool Pop(Event& out) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        out = std::move(events_[tail]);
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return true;
    }
    // Only after both owners are stopped.
    void Clear() noexcept { Event event; while (Pop(event)) {} }
};
}
