#pragma once

#include "worker_lifecycle.h"

#include <cstdint>

namespace halljoy::lifecycle
{

enum class JoinWaitStatus : std::uint8_t
{
    Joined,
    TimedOut,
    Failed,
};

[[nodiscard]] constexpr StopResult ObserveWorkerJoin(
    GenerationId generation,
    JoinWaitStatus status,
    std::uint32_t nativeError = 0) noexcept
{
    if (status == JoinWaitStatus::Joined)
        return { StopStatus::Joined, WorkerState::Joined, generation, {} };

    const LifecycleErrorCode code = status == JoinWaitStatus::TimedOut
        ? LifecycleErrorCode::StopTimedOut
        : LifecycleErrorCode::PrimitiveFailed;
    return {
        status == JoinWaitStatus::TimedOut ? StopStatus::TimedOut : StopStatus::Faulted,
        WorkerState::Faulted,
        generation,
        { code, LifecycleOperation::ConfirmJoined,
            WorkerState::StopRequested, WorkerState::Faulted,
            generation, generation, nativeError },
    };
}

} // namespace halljoy::lifecycle
