#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace halljoy::uap
{
    template<typename Result, typename Function, typename OnFault>
    Result CAbiInvoke(Result fallback, Function&& function, OnFault&& on_fault) noexcept
    {
        try
        {
            return std::forward<Function>(function)();
        }
        catch (...)
        {
            try
            {
                std::forward<OnFault>(on_fault)();
            }
            catch (...)
            {
            }
            return fallback;
        }
    }

    template<typename Function, typename OnFault>
    void CAbiInvokeVoid(Function&& function, OnFault&& on_fault) noexcept
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (...)
        {
            try
            {
                std::forward<OnFault>(on_fault)();
            }
            catch (...)
            {
            }
        }
    }

    template<typename Mutex>
    class LockGuard final
    {
    public:
        explicit LockGuard(Mutex& mutex) : mutex_(mutex)
        {
            mutex_.lock();
        }

        ~LockGuard() noexcept
        {
            mutex_.unlock();
        }

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

    private:
        Mutex& mutex_;
    };

    // Allocation-free RAII ownership for a deterministic set of per-device
    // locks. A partially acquired set is still fully released on unwind.
    template<typename Mutex, std::size_t Capacity>
    class LockSet final
    {
    public:
        bool Acquire(Mutex& mutex) noexcept
        {
            if (count_ == Capacity)
                return false;
            mutex.lock();
            mutexes_[count_++] = &mutex;
            return true;
        }

        ~LockSet() noexcept
        {
            while (count_ != 0)
                mutexes_[--count_]->unlock();
        }

        LockSet() = default;
        LockSet(const LockSet&) = delete;
        LockSet& operator=(const LockSet&) = delete;

    private:
        std::array<Mutex*, Capacity> mutexes_{};
        std::size_t count_ = 0;
    };

    // Dynamic-capacity counterpart whose pointer storage is provisioned before
    // any device lock is acquired. It performs no allocation and releases a
    // partially or fully acquired set in strict reverse order.
    template<typename Mutex>
    class LockSetView final
    {
    public:
        LockSetView(Mutex** storage, std::size_t capacity) noexcept
            : mutexes_(storage), capacity_(storage ? capacity : 0)
        {
        }

        bool Acquire(Mutex& mutex) noexcept
        {
            if (count_ == capacity_)
                return false;
            mutex.lock();
            mutexes_[count_++] = &mutex;
            return true;
        }

        ~LockSetView() noexcept
        {
            while (count_ != 0)
                mutexes_[--count_]->unlock();
        }

        LockSetView(const LockSetView&) = delete;
        LockSetView& operator=(const LockSetView&) = delete;

    private:
        Mutex** mutexes_ = nullptr;
        std::size_t capacity_ = 0;
        std::size_t count_ = 0;
    };
}
