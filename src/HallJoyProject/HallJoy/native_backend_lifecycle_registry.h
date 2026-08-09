#pragma once

#include "worker_lifecycle.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace halljoy::lifecycle
{

template <std::size_t Capacity>
class BackendLifecycleRegistry
{
public:
    struct StartDecision
    {
        StartResult result{};
        bool invokeBackend = false;
    };

    struct StopDecision
    {
        StopResult result{};
        bool invokeBackend = false;
    };

    struct Snapshot
    {
        WorkerState state = WorkerState::Stopped;
        GenerationId generation{};
        LifecycleError lastError{};
        std::uint64_t ownerToken = 0;
    };

    [[nodiscard]] StartDecision BeginStart(std::size_t index, std::uint64_t owner) noexcept
    {
        if (!Validate(index, owner, LifecycleOperation::BeginStart))
            return {};
        ownerErrors_[index] = {};
        const StartResult result = entries_[index].BeginStart();
        return { result, result.status == StartStatus::Starting };
    }

    [[nodiscard]] StartResult CompleteStart(
        std::size_t index, std::uint64_t owner, GenerationId generation,
        bool backendStarted, std::uint32_t nativeError = 0) noexcept
    {
        if (!Validate(index, owner, LifecycleOperation::ConfirmRunning))
            return {};
        ownerErrors_[index] = {};
        return backendStarted
            ? entries_[index].ConfirmRunning(generation)
            : entries_[index].FailStartBeforeWorker(generation, nativeError);
    }

    [[nodiscard]] StopDecision BeginStop(std::size_t index, std::uint64_t owner) noexcept
    {
        if (!Validate(index, owner, LifecycleOperation::RequestStop))
            return {};
        ownerErrors_[index] = {};
        auto& entry = entries_[index];
        const WorkerState before = entry.State();
        const GenerationId generation = entry.Generation();
        const StopResult result = entry.RequestStop(
            before == WorkerState::Stopped || before == WorkerState::Joined
                ? GenerationId{} : generation);
        return { result, result.status == StopStatus::StopRequested &&
            before != WorkerState::StopRequested };
    }

    [[nodiscard]] StopResult CompleteStop(
        std::size_t index, std::uint64_t owner, GenerationId generation,
        const StopResult& backendResult) noexcept
    {
        if (!Validate(index, owner, LifecycleOperation::ConfirmJoined))
            return {};
        ownerErrors_[index] = {};
        auto& entry = entries_[index];
        if (generation != entry.Generation() || backendResult.generation != generation)
        {
            return entry.MarkPoisoned(entry.Generation(), LifecycleOperation::ConfirmJoined,
                LifecycleErrorCode::BackendContractViolation);
        }
        if (backendResult.status == StopStatus::Joined && backendResult.RestartSafe())
            return entry.ConfirmJoined(generation);

        const LifecycleErrorCode reason = backendResult.status == StopStatus::TimedOut
            ? LifecycleErrorCode::StopTimedOut
            : (backendResult.error.code == LifecycleErrorCode::None
                ? LifecycleErrorCode::BackendContractViolation
                : backendResult.error.code);
        return entry.MarkPoisoned(generation, LifecycleOperation::ConfirmJoined,
            reason, backendResult.error.native_error);
    }

    [[nodiscard]] Snapshot GetSnapshot(std::size_t index) const noexcept
    {
        if (index >= Capacity)
            return {};
        const auto& entry = entries_[index];
        return { entry.State(), entry.Generation(),
            ownerErrors_[index].HasError() ? ownerErrors_[index] : entry.LastError(), owner_ };
    }

private:
    [[nodiscard]] bool Validate(
        std::size_t index, std::uint64_t owner, LifecycleOperation operation) noexcept
    {
        if (index >= Capacity || owner == 0)
            return false;
        if (owner_ == 0)
            owner_ = owner;
        if (owner_ == owner)
            return true;

        const GenerationId generation = entries_[index].Generation();
        ownerErrors_[index] = { LifecycleErrorCode::WrongThread, operation,
            entries_[index].State(), entries_[index].State(), generation, generation, 0 };
        return false;
    }

    std::array<WorkerLifecycle, Capacity> entries_{};
    std::array<LifecycleError, Capacity> ownerErrors_{};
    std::uint64_t owner_ = 0;
};

} // namespace halljoy::lifecycle
