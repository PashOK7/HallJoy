#include "../HallJoy/worker_join_policy.h"

#include <cassert>

using namespace halljoy::lifecycle;

int main()
{
    const GenerationId generation(7);

    const auto joined = ObserveWorkerJoin(generation, JoinWaitStatus::Joined);
    assert(joined.status == StopStatus::Joined);
    assert(joined.RestartSafe());

    const auto timedOut = ObserveWorkerJoin(
        generation, JoinWaitStatus::TimedOut, 1460);
    assert(timedOut.status == StopStatus::TimedOut);
    assert(!timedOut.RestartSafe());
    assert(timedOut.error.code == LifecycleErrorCode::StopTimedOut);
    assert(timedOut.error.native_error == 1460);

    const auto failed = ObserveWorkerJoin(
        generation, JoinWaitStatus::Failed, 6);
    assert(failed.status == StopStatus::Faulted);
    assert(!failed.RestartSafe());
    assert(failed.error.code == LifecycleErrorCode::PrimitiveFailed);
    assert(failed.error.native_error == 6);

    WorkerLifecycle lifecycle;
    const auto starting = lifecycle.BeginStart();
    assert(lifecycle.ConfirmRunning(starting.generation).IsRunning());
    assert(lifecycle.RequestStop(starting.generation).status == StopStatus::StopRequested);
    const auto poisoned = lifecycle.MarkPoisoned(starting.generation,
        LifecycleOperation::ConfirmJoined, timedOut.error.code,
        timedOut.error.native_error);
    assert(poisoned.state == WorkerState::Poisoned);
    assert(!lifecycle.BeginStart().Accepted());
    return 0;
}
