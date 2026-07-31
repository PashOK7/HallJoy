#include <cassert>
#include <cstdint>

#include "../HallJoy/worker_primitives.h"

using namespace halljoy::lifecycle;

namespace
{

struct FakePrimitives final : WorkerPrimitives
{
    std::uint64_t now_ms = 100;
    bool fail_create_signal = false;
    bool fail_signal = false;
    WaitStatus wait_status = WaitStatus::Signaled;
    bool fail_start = false;
    JoinStatus join_status = JoinStatus::Joined;
    std::uint32_t close_thread_calls = 0;
    std::uint32_t close_wait_calls = 0;
    std::uint32_t last_timeout_ms = 0;
    WorkerEntry last_entry = nullptr;
    void* last_context = nullptr;

    [[nodiscard]] std::uint64_t MonotonicMilliseconds() noexcept override
    {
        return now_ms;
    }

    [[nodiscard]] SignalCreateResult CreateStopSignal() noexcept override
    {
        return fail_create_signal
            ? SignalCreateResult{ PrimitiveStatus::Failed, {}, 5 }
            : SignalCreateResult{ PrimitiveStatus::Succeeded, WaitToken{ 11 }, 0 };
    }

    [[nodiscard]] PrimitiveResult SignalStop(WaitToken token) noexcept override
    {
        assert(token == WaitToken{ 11 });
        return fail_signal
            ? PrimitiveResult{ PrimitiveStatus::Failed, 6 }
            : PrimitiveResult{ PrimitiveStatus::Succeeded, 0 };
    }

    [[nodiscard]] WaitResult WaitForStop(
        WaitToken token, std::uint32_t timeout_ms) noexcept override
    {
        assert(token == WaitToken{ 11 });
        last_timeout_ms = timeout_ms;
        return WaitResult{ wait_status, wait_status == WaitStatus::Failed ? 7u : 0u };
    }

    [[nodiscard]] ThreadStartResult StartThread(
        WorkerEntry entry, void* context) noexcept override
    {
        last_entry = entry;
        last_context = context;
        return fail_start
            ? ThreadStartResult{ ThreadStartStatus::Failed, {}, 8 }
            : ThreadStartResult{ ThreadStartStatus::Started, ThreadToken{ 22 }, 0 };
    }

    [[nodiscard]] JoinResult JoinThread(
        ThreadToken token, std::uint32_t timeout_ms) noexcept override
    {
        assert(token == ThreadToken{ 22 });
        last_timeout_ms = timeout_ms;
        return JoinResult{ join_status, join_status == JoinStatus::Failed ? 9u : 0u };
    }

    void CloseThread(ThreadToken token) noexcept override
    {
        assert(token == ThreadToken{ 22 });
        ++close_thread_calls;
    }

    void CloseWait(WaitToken token) noexcept override
    {
        assert(token == WaitToken{ 11 });
        ++close_wait_calls;
    }
};

void Worker(void* context) noexcept
{
    ++*static_cast<int*>(context);
}

} // namespace

int main()
{
    FakePrimitives fake;
    assert(fake.MonotonicMilliseconds() == 100);
    fake.now_ms = 250;
    assert(fake.MonotonicMilliseconds() == 250);

    const SignalCreateResult signal = fake.CreateStopSignal();
    assert(signal.Succeeded());
    assert(signal.token == WaitToken{ 11 });
    assert(fake.SignalStop(signal.token).Succeeded());

    assert(fake.WaitForStop(signal.token, 25).Signaled());
    assert(fake.last_timeout_ms == 25);
    fake.wait_status = WaitStatus::TimedOut;
    assert(fake.WaitForStop(signal.token, 30).status == WaitStatus::TimedOut);
    fake.wait_status = WaitStatus::Failed;
    const WaitResult waitFailure = fake.WaitForStop(signal.token, 35);
    assert(waitFailure.status == WaitStatus::Failed);
    assert(waitFailure.native_error == 7);

    int workerCalls = 0;
    const ThreadStartResult thread = fake.StartThread(&Worker, &workerCalls);
    assert(thread.Started());
    assert(fake.last_entry == &Worker);
    assert(fake.last_context == &workerCalls);
    fake.last_entry(fake.last_context);
    assert(workerCalls == 1);

    assert(fake.JoinThread(thread.token, 100).Joined());
    fake.join_status = JoinStatus::TimedOut;
    assert(fake.JoinThread(thread.token, 101).status == JoinStatus::TimedOut);
    fake.join_status = JoinStatus::Failed;
    const JoinResult joinFailure = fake.JoinThread(thread.token, 102);
    assert(joinFailure.status == JoinStatus::Failed);
    assert(joinFailure.native_error == 9);

    fake.CloseThread(thread.token);
    fake.CloseWait(signal.token);
    assert(fake.close_thread_calls == 1);
    assert(fake.close_wait_calls == 1);

    fake.fail_create_signal = true;
    const SignalCreateResult createFailure = fake.CreateStopSignal();
    assert(!createFailure.Succeeded());
    assert(createFailure.native_error == 5);

    fake.fail_signal = true;
    const PrimitiveResult signalFailure = fake.SignalStop(signal.token);
    assert(!signalFailure.Succeeded());
    assert(signalFailure.native_error == 6);

    fake.fail_start = true;
    const ThreadStartResult startFailure = fake.StartThread(&Worker, &workerCalls);
    assert(!startFailure.Started());
    assert(startFailure.native_error == 8);
    assert(!startFailure.token.IsValid());

    return 0;
}
