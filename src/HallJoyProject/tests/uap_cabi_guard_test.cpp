#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_cabi_guard.h"

#include <cassert>
#include <stdexcept>

namespace
{
    struct TestMutex
    {
        int depth = 0;
        void lock() noexcept { ++depth; }
        void unlock() noexcept { --depth; }
    };
}

int main()
{
    bool faulted = false;
    const int value = halljoy::uap::CAbiInvoke<int>(-7, []() -> int {
        throw std::runtime_error("injected C ABI fault");
    }, [&]() noexcept { faulted = true; });
    assert(value == -7 && faulted);

    const int nestedFault = halljoy::uap::CAbiInvoke<int>(-8, []() -> int {
        throw std::runtime_error("injected operation fault");
    }, []() { throw std::runtime_error("injected fault-handler fault"); });
    assert(nestedFault == -8);

    faulted = false;
    halljoy::uap::CAbiInvokeVoid([]() { throw 2; }, [&]() noexcept { faulted = true; });
    assert(faulted);

    faulted = false;
    const int normal = halljoy::uap::CAbiInvoke<int>(-7, [] { return 42; },
        [&]() noexcept { faulted = true; });
    assert(normal == 42 && !faulted);

    TestMutex mutex;
    try
    {
        halljoy::uap::LockGuard<TestMutex> lock(mutex);
        assert(mutex.depth == 1);
        throw 1;
    }
    catch (...)
    {
    }
    assert(mutex.depth == 0);
    return 0;
}
