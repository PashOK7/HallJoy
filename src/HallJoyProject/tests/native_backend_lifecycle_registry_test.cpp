#include "../HallJoy/native_backend_lifecycle_registry.h"
#include "../HallJoy/native_analog_backend.h"

#include <cassert>

using namespace halljoy::lifecycle;

int main()
{
    BackendLifecycleRegistry<3> registry;
    constexpr std::uint64_t owner = 17;

    auto start = registry.BeginStart(0, owner);
    assert(start.invokeBackend);
    assert(start.result.status == StartStatus::Starting);
    const GenerationId first = start.result.generation;
    assert(first.Value() == 1);

    auto wrongOwner = registry.BeginStart(0, owner + 1);
    assert(!wrongOwner.invokeBackend);
    assert(wrongOwner.result.status == StartStatus::Rejected);
    assert(registry.GetSnapshot(0).lastError.code == LifecycleErrorCode::WrongThread);

    auto running = registry.CompleteStart(0, owner, first, true);
    assert(running.IsRunning());
    start = registry.BeginStart(0, owner);
    assert(!start.invokeBackend);
    assert(start.result.status == StartStatus::AlreadyRunning);

    auto stop = registry.BeginStop(0, owner);
    assert(stop.invokeBackend);
    assert(stop.result.status == StopStatus::StopRequested);
    auto timedOut = NativeAnalogBackendStopFailed(first,
        LifecycleErrorCode::StopTimedOut, 1460);
    auto poisoned = registry.CompleteStop(0, owner, first, timedOut);
    assert(poisoned.status == StopStatus::TimedOut);
    assert(poisoned.state == WorkerState::Poisoned);
    assert(poisoned.error.native_error == 1460);
    assert(!registry.BeginStart(0, owner).invokeBackend);
    assert(registry.GetSnapshot(0).state == WorkerState::Poisoned);

    start = registry.BeginStart(1, owner);
    assert(start.invokeBackend && start.result.generation.Value() == 1);
    auto failed = registry.CompleteStart(1, owner, start.result.generation, false, 5);
    assert(failed.status == StartStatus::Failed);
    assert(registry.GetSnapshot(1).state == WorkerState::Stopped);

    start = registry.BeginStart(1, owner);
    assert(start.invokeBackend && start.result.generation.Value() == 2);
    assert(registry.CompleteStart(1, owner, start.result.generation, true).IsRunning());
    stop = registry.BeginStop(1, owner);
    assert(stop.invokeBackend);
    auto joined = registry.CompleteStop(1, owner, stop.result.generation,
        NativeAnalogBackendStopJoined(stop.result.generation));
    assert(joined.status == StopStatus::Joined);
    assert(joined.RestartSafe());

    start = registry.BeginStart(1, owner);
    assert(start.invokeBackend && start.result.generation.Value() == 3);
    assert(registry.CompleteStart(1, owner, start.result.generation, true).IsRunning());
    stop = registry.BeginStop(1, owner);
    const GenerationId stale(stop.result.generation.Value() - 1);
    poisoned = registry.CompleteStop(1, owner, stop.result.generation,
        NativeAnalogBackendStopJoined(stale));
    assert(poisoned.state == WorkerState::Poisoned);
    assert(poisoned.error.code == LifecycleErrorCode::BackendContractViolation);

    start = registry.BeginStart(2, owner);
    assert(start.invokeBackend);
    assert(registry.CompleteStart(2, owner, start.result.generation, true).IsRunning());
    stop = registry.BeginStop(2, owner);
    auto faulted = NativeAnalogBackendStopFailed(stop.result.generation,
        LifecycleErrorCode::WorkerFaulted, 31);
    poisoned = registry.CompleteStop(2, owner, stop.result.generation, faulted);
    assert(poisoned.status == StopStatus::Poisoned);
    assert(poisoned.state == WorkerState::Poisoned);
    assert(poisoned.error.code == LifecycleErrorCode::WorkerFaulted);
    assert(poisoned.error.native_error == 31);
    assert(!registry.BeginStart(2, owner).invokeBackend);
    return 0;
}
