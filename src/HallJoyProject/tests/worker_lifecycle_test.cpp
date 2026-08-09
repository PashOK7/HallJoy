#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

#include "../HallJoy/worker_lifecycle.h"

using namespace halljoy::lifecycle;

namespace
{

constexpr std::array<WorkerState, 7> kStates{
    WorkerState::Stopped,
    WorkerState::Starting,
    WorkerState::Running,
    WorkerState::StopRequested,
    WorkerState::Joined,
    WorkerState::Faulted,
    WorkerState::Poisoned,
};

constexpr bool ExpectedTransition(WorkerState from, WorkerState to)
{
    switch (from)
    {
    case WorkerState::Stopped:
        return to == WorkerState::Starting;
    case WorkerState::Starting:
        return to == WorkerState::Stopped || to == WorkerState::Running ||
            to == WorkerState::StopRequested || to == WorkerState::Faulted ||
            to == WorkerState::Poisoned;
    case WorkerState::Running:
        return to == WorkerState::StopRequested || to == WorkerState::Faulted ||
            to == WorkerState::Poisoned;
    case WorkerState::StopRequested:
        return to == WorkerState::Joined || to == WorkerState::Faulted ||
            to == WorkerState::Poisoned;
    case WorkerState::Joined:
        return to == WorkerState::Starting;
    case WorkerState::Faulted:
        return to == WorkerState::StopRequested || to == WorkerState::Joined ||
            to == WorkerState::Poisoned;
    case WorkerState::Poisoned:
        return false;
    }
    return false;
}

void VerifyTransitionMatrix()
{
    for (const WorkerState from : kStates)
    {
        for (const WorkerState to : kStates)
            assert(WorkerLifecycle::CanTransition(from, to) == ExpectedTransition(from, to));
    }
}

void VerifyNormalLifecycleAndRestart()
{
    WorkerLifecycle lifecycle;
    assert(lifecycle.State() == WorkerState::Stopped);
    assert(!lifecycle.Generation().IsValid());
    assert(lifecycle.RestartSafe());

    const StartResult begin = lifecycle.BeginStart();
    assert(begin.status == StartStatus::Starting);
    assert(begin.Accepted());
    assert(begin.generation == GenerationId(1));
    assert(lifecycle.State() == WorkerState::Starting);

    const StartResult duplicateBegin = lifecycle.BeginStart();
    assert(duplicateBegin.status == StartStatus::AlreadyStarting);
    assert(duplicateBegin.generation == GenerationId(1));

    const StartResult running = lifecycle.ConfirmRunning(begin.generation);
    assert(running.status == StartStatus::Running);
    assert(running.IsRunning());
    assert(lifecycle.ConfirmRunning(begin.generation).status == StartStatus::AlreadyRunning);

    const StopResult requested = lifecycle.RequestStop(begin.generation);
    assert(requested.status == StopStatus::StopRequested);
    assert(!requested.Completed());
    assert(!requested.RestartSafe());
    assert(lifecycle.RequestStop(begin.generation).status == StopStatus::StopRequested);

    const StopResult joined = lifecycle.ConfirmJoined(begin.generation);
    assert(joined.status == StopStatus::Joined);
    assert(joined.Completed());
    assert(joined.RestartSafe());
    assert(lifecycle.ConfirmJoined(begin.generation).status == StopStatus::Joined);
    assert(lifecycle.RequestStop().status == StopStatus::AlreadyStopped);

    const StartResult restarted = lifecycle.BeginStart();
    assert(restarted.status == StartStatus::Starting);
    assert(restarted.generation == GenerationId(2));
}

void VerifyStopBeforeStartAndFailedCreation()
{
    WorkerLifecycle neverStarted;
    const StopResult stopped = neverStarted.RequestStop();
    assert(stopped.status == StopStatus::AlreadyStopped);
    assert(stopped.Completed());
    assert(stopped.RestartSafe());

    WorkerLifecycle failed;
    const GenerationId generation = failed.BeginStart().generation;
    const StartResult failure = failed.FailStartBeforeWorker(generation, 1234);
    assert(failure.status == StartStatus::Failed);
    assert(failure.state == WorkerState::Stopped);
    assert(failure.error.code == LifecycleErrorCode::StartFailed);
    assert(failure.error.native_error == 1234);
    assert(failed.RestartSafe());
    assert(failed.Generation() == generation); // failed generations are never reused
    assert(failed.RequestStop(generation).status == StopStatus::AlreadyStopped);
    assert(failed.RequestStop().status == StopStatus::AlreadyStopped);
    assert(failed.BeginStart().generation == GenerationId(2));
}

void VerifyStaleAndInvalidGenerationRejection()
{
    WorkerLifecycle lifecycle;
    const GenerationId active = lifecycle.BeginStart().generation;

    const StartResult invalid = lifecycle.ConfirmRunning({});
    assert(invalid.status == StartStatus::Rejected);
    assert(invalid.error.code == LifecycleErrorCode::InvalidGeneration);
    assert(lifecycle.State() == WorkerState::Starting);

    const StartResult stale = lifecycle.ConfirmRunning(GenerationId(active.Value() + 1));
    assert(stale.status == StartStatus::Rejected);
    assert(stale.error.code == LifecycleErrorCode::StaleGeneration);
    assert(stale.error.active_generation == active);
    assert(lifecycle.State() == WorkerState::Starting);

    const StopResult staleStop = lifecycle.RequestStop(GenerationId(active.Value() + 10));
    assert(staleStop.status == StopStatus::Rejected);
    assert(staleStop.error.code == LifecycleErrorCode::StaleGeneration);
    assert(lifecycle.State() == WorkerState::Starting);
}

void VerifyFaultCleanupPreservesDiagnostic()
{
    WorkerLifecycle lifecycle;
    const GenerationId generation = lifecycle.BeginStart().generation;
    assert(lifecycle.ConfirmRunning(generation).IsRunning());

    const TransitionResult fault = lifecycle.MarkFaulted(
        generation, LifecycleOperation::MarkFaulted, 0xdead);
    assert(fault.status == TransitionStatus::Applied);
    assert(fault.state == WorkerState::Faulted);
    assert(fault.error.code == LifecycleErrorCode::WorkerFaulted);
    assert(fault.error.native_error == 0xdead);

    const TransitionResult duplicate = lifecycle.MarkFaulted(
        generation, LifecycleOperation::MarkFaulted, 0xbeef);
    assert(duplicate.status == TransitionStatus::NoChange);
    assert(duplicate.error.native_error == 0xdead); // first fault is authoritative

    assert(lifecycle.RequestStop(generation).status == StopStatus::StopRequested);
    assert(lifecycle.LastError().native_error == 0xdead);
    const StopResult joined = lifecycle.ConfirmJoined(generation);
    assert(joined.status == StopStatus::Joined);
    assert(joined.RestartSafe());
    assert(lifecycle.LastError().native_error == 0xdead);
}

void VerifyTimeoutPoisonsGeneration()
{
    WorkerLifecycle lifecycle;
    const GenerationId generation = lifecycle.BeginStart().generation;
    assert(lifecycle.ConfirmRunning(generation).IsRunning());
    assert(lifecycle.RequestStop(generation).status == StopStatus::StopRequested);

    const StopResult timeout = lifecycle.MarkPoisoned(
        generation, LifecycleOperation::MarkPoisoned,
        LifecycleErrorCode::StopTimedOut, 258);
    assert(timeout.status == StopStatus::TimedOut);
    assert(timeout.state == WorkerState::Poisoned);
    assert(timeout.error.code == LifecycleErrorCode::StopTimedOut);
    assert(timeout.error.native_error == 258);
    assert(!timeout.RestartSafe());
    assert(!lifecycle.RestartSafe());

    const StartResult rejectedRestart = lifecycle.BeginStart();
    assert(rejectedRestart.status == StartStatus::Rejected);
    assert(rejectedRestart.error.code == LifecycleErrorCode::InvalidTransition);
    assert(lifecycle.RequestStop(generation).status == StopStatus::Poisoned);
    assert(lifecycle.ConfirmJoined(generation).status == StopStatus::Poisoned);
}

void VerifyEveryAllowedNamedTransition()
{
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.RequestStop(generation).status == StopStatus::StopRequested);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.MarkFaulted(generation, LifecycleOperation::MarkFaulted).Succeeded());
        assert(lifecycle.State() == WorkerState::Faulted);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.MarkPoisoned(generation).state == WorkerState::Poisoned);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.ConfirmRunning(generation).IsRunning());
        assert(lifecycle.MarkPoisoned(generation).state == WorkerState::Poisoned);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.ConfirmRunning(generation).IsRunning());
        assert(lifecycle.RequestStop(generation).status == StopStatus::StopRequested);
        assert(lifecycle.MarkFaulted(generation, LifecycleOperation::MarkFaulted).Succeeded());
        assert(lifecycle.State() == WorkerState::Faulted);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.ConfirmRunning(generation).IsRunning());
        assert(lifecycle.MarkFaulted(generation, LifecycleOperation::MarkFaulted).Succeeded());
        assert(lifecycle.ConfirmJoined(generation).status == StopStatus::Joined);
    }
    {
        WorkerLifecycle lifecycle;
        const GenerationId generation = lifecycle.BeginStart().generation;
        assert(lifecycle.ConfirmRunning(generation).IsRunning());
        assert(lifecycle.MarkFaulted(generation, LifecycleOperation::MarkFaulted).Succeeded());
        assert(lifecycle.MarkPoisoned(generation).state == WorkerState::Poisoned);
    }
}

void VerifyForbiddenOperationsDoNotMutateState()
{
    WorkerLifecycle lifecycle;
    const GenerationId generation = lifecycle.BeginStart().generation;
    assert(lifecycle.ConfirmRunning(generation).IsRunning());

    const StopResult illegalJoin = lifecycle.ConfirmJoined(generation);
    assert(illegalJoin.status == StopStatus::Rejected);
    assert(illegalJoin.error.code == LifecycleErrorCode::InvalidTransition);
    assert(lifecycle.State() == WorkerState::Running);

    const StartResult illegalFailure = lifecycle.FailStartBeforeWorker(generation, 7);
    assert(illegalFailure.status == StartStatus::Rejected);
    assert(illegalFailure.error.code == LifecycleErrorCode::InvalidTransition);
    assert(lifecycle.State() == WorkerState::Running);
}

void VerifyGenerationExhaustion()
{
    WorkerLifecycle lifecycle{ GenerationId(std::numeric_limits<std::uint64_t>::max()) };
    const StartResult result = lifecycle.BeginStart();
    assert(result.status == StartStatus::Rejected);
    assert(result.error.code == LifecycleErrorCode::GenerationExhausted);
    assert(lifecycle.State() == WorkerState::Stopped);
    assert(lifecycle.Generation().Value() == std::numeric_limits<std::uint64_t>::max());
}

} // namespace

int main()
{
    VerifyTransitionMatrix();
    VerifyNormalLifecycleAndRestart();
    VerifyStopBeforeStartAndFailedCreation();
    VerifyStaleAndInvalidGenerationRejection();
    VerifyFaultCleanupPreservesDiagnostic();
    VerifyTimeoutPoisonsGeneration();
    VerifyEveryAllowedNamedTransition();
    VerifyForbiddenOperationsDoNotMutateState();
    VerifyGenerationExhaustion();
    return 0;
}
