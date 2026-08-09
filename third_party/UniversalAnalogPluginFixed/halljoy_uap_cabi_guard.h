#pragma once

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
}
