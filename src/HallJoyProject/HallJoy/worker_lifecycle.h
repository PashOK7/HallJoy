#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

namespace halljoy::lifecycle
{

class GenerationId
{
public:
    constexpr GenerationId() noexcept = default;
    explicit constexpr GenerationId(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value_ != 0; }
    [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return value_; }

    friend constexpr bool operator==(GenerationId, GenerationId) noexcept = default;

private:
    std::uint64_t value_ = 0;
};

static_assert(std::is_trivially_copyable_v<GenerationId>);

// Joined is intentionally distinct from Stopped. It records that a specific
// generation has confirmed worker completion and is therefore safe to replace.
enum class WorkerState : std::uint8_t
{
    Stopped,
    Starting,
    Running,
    StopRequested,
    Joined,
    Faulted,
    Poisoned,
};

enum class LifecycleOperation : std::uint8_t
{
    None,
    BeginStart,
    ConfirmRunning,
    FailStartBeforeWorker,
    RequestStop,
    ConfirmJoined,
    MarkFaulted,
    MarkPoisoned,
};

enum class LifecycleErrorCode : std::uint8_t
{
    None,
    InvalidGeneration,
    StaleGeneration,
    GenerationExhausted,
    InvalidTransition,
    StartFailed,
    WorkerFaulted,
    StopTimedOut,
    PrimitiveFailed,
    WrongThread,
    BackendContractViolation,
};

// A fixed-size, allocation-free diagnostic record. It is safe to copy through
// noexcept lifecycle and C ABI boundaries. native_error stores GetLastError(),
// errno, HRESULT-derived data, or zero when no platform code exists.
struct LifecycleError
{
    LifecycleErrorCode code = LifecycleErrorCode::None;
    LifecycleOperation operation = LifecycleOperation::None;
    WorkerState from = WorkerState::Stopped;
    WorkerState requested = WorkerState::Stopped;
    GenerationId supplied_generation{};
    GenerationId active_generation{};
    std::uint32_t native_error = 0;

    [[nodiscard]] constexpr bool HasError() const noexcept
    {
        return code != LifecycleErrorCode::None;
    }
};

static_assert(std::is_trivially_copyable_v<LifecycleError>);

enum class StartStatus : std::uint8_t
{
    Starting,
    Running,
    AlreadyStarting,
    AlreadyRunning,
    Failed,
    Rejected,
};

struct StartResult
{
    StartStatus status = StartStatus::Rejected;
    WorkerState state = WorkerState::Stopped;
    GenerationId generation{};
    LifecycleError error{};

    [[nodiscard]] constexpr bool Accepted() const noexcept
    {
        return status == StartStatus::Starting || status == StartStatus::Running ||
            status == StartStatus::AlreadyStarting || status == StartStatus::AlreadyRunning;
    }

    [[nodiscard]] constexpr bool IsRunning() const noexcept
    {
        return status == StartStatus::Running || status == StartStatus::AlreadyRunning;
    }
};

static_assert(std::is_trivially_copyable_v<StartResult>);

enum class StopStatus : std::uint8_t
{
    StopRequested,
    Joined,
    AlreadyStopped,
    TimedOut,
    Faulted,
    Poisoned,
    Rejected,
};

struct StopResult
{
    StopStatus status = StopStatus::Rejected;
    WorkerState state = WorkerState::Stopped;
    GenerationId generation{};
    LifecycleError error{};

    [[nodiscard]] constexpr bool Completed() const noexcept
    {
        return status == StopStatus::Joined || status == StopStatus::AlreadyStopped;
    }

    [[nodiscard]] constexpr bool RestartSafe() const noexcept
    {
        return Completed() && (state == WorkerState::Stopped || state == WorkerState::Joined);
    }
};

static_assert(std::is_trivially_copyable_v<StopResult>);

enum class TransitionStatus : std::uint8_t
{
    Applied,
    NoChange,
    Rejected,
};

struct TransitionResult
{
    TransitionStatus status = TransitionStatus::Rejected;
    WorkerState state = WorkerState::Stopped;
    GenerationId generation{};
    LifecycleError error{};

    [[nodiscard]] constexpr bool Succeeded() const noexcept
    {
        return status != TransitionStatus::Rejected;
    }
};

static_assert(std::is_trivially_copyable_v<TransitionResult>);

class WorkerLifecycle
{
public:
    constexpr WorkerLifecycle() noexcept = default;

    // This constructor is useful when restoring a persisted/parent-owned
    // monotonic counter. The machine still starts in the safe Stopped state.
    explicit constexpr WorkerLifecycle(GenerationId last_generation) noexcept
        : generation_(last_generation)
    {
    }

    [[nodiscard]] constexpr WorkerState State() const noexcept { return state_; }
    [[nodiscard]] constexpr GenerationId Generation() const noexcept { return generation_; }
    [[nodiscard]] constexpr const LifecycleError& LastError() const noexcept { return last_error_; }
    [[nodiscard]] constexpr bool RestartSafe() const noexcept
    {
        return state_ == WorkerState::Stopped || state_ == WorkerState::Joined;
    }

    // This table describes state changes only. Idempotent calls are accepted by
    // their named operation but are deliberately not represented as transitions.
    [[nodiscard]] static constexpr bool CanTransition(WorkerState from, WorkerState to) noexcept
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

    [[nodiscard]] constexpr StartResult BeginStart() noexcept
    {
        if (state_ == WorkerState::Starting)
            return MakeStart(StartStatus::AlreadyStarting);
        if (state_ == WorkerState::Running)
            return MakeStart(StartStatus::AlreadyRunning);
        if (state_ != WorkerState::Stopped && state_ != WorkerState::Joined)
            return RejectStart(LifecycleOperation::BeginStart, WorkerState::Starting,
                LifecycleErrorCode::InvalidTransition, generation_);
        if (generation_.Value() == std::numeric_limits<std::uint64_t>::max())
            return RejectStart(LifecycleOperation::BeginStart, WorkerState::Starting,
                LifecycleErrorCode::GenerationExhausted, generation_);

        generation_ = GenerationId(generation_.Value() + 1);
        state_ = WorkerState::Starting;
        last_error_ = {};
        return MakeStart(StartStatus::Starting);
    }

    [[nodiscard]] constexpr StartResult ConfirmRunning(GenerationId generation) noexcept
    {
        if (const LifecycleError error = ValidateGeneration(
                generation, LifecycleOperation::ConfirmRunning, WorkerState::Running);
            error.HasError())
        {
            return MakeStart(StartStatus::Rejected, error);
        }
        if (state_ == WorkerState::Running)
            return MakeStart(StartStatus::AlreadyRunning);
        if (!CanTransition(state_, WorkerState::Running))
            return RejectStart(LifecycleOperation::ConfirmRunning, WorkerState::Running,
                LifecycleErrorCode::InvalidTransition, generation);

        state_ = WorkerState::Running;
        return MakeStart(StartStatus::Running);
    }

    // Use only when thread creation did not succeed and no worker can still
    // access generation-owned resources. A partial start with a possibly live
    // worker must request stop or become Poisoned instead.
    [[nodiscard]] constexpr StartResult FailStartBeforeWorker(
        GenerationId generation, std::uint32_t native_error = 0) noexcept
    {
        if (const LifecycleError error = ValidateGeneration(
                generation, LifecycleOperation::FailStartBeforeWorker, WorkerState::Stopped);
            error.HasError())
        {
            return MakeStart(StartStatus::Rejected, error);
        }
        if (state_ != WorkerState::Starting)
            return RejectStart(LifecycleOperation::FailStartBeforeWorker, WorkerState::Stopped,
                LifecycleErrorCode::InvalidTransition, generation, native_error);

        const WorkerState from = state_;
        state_ = WorkerState::Stopped;
        last_error_ = MakeError(LifecycleErrorCode::StartFailed,
            LifecycleOperation::FailStartBeforeWorker, from, state_, generation, native_error);
        return MakeStart(StartStatus::Failed, last_error_);
    }

    [[nodiscard]] constexpr StopResult RequestStop(GenerationId generation = {}) noexcept
    {
        // A no-argument stop is intentionally idempotent once no worker can be
        // alive. Active states still require the exact generation token.
        if ((state_ == WorkerState::Stopped || state_ == WorkerState::Joined) &&
            !generation.IsValid())
        {
            return MakeStop(StopStatus::AlreadyStopped);
        }

        if (const LifecycleError error = ValidateGeneration(
                generation, LifecycleOperation::RequestStop, WorkerState::StopRequested);
            error.HasError())
        {
            return MakeStop(StopStatus::Rejected, error);
        }

        if (state_ == WorkerState::Stopped || state_ == WorkerState::Joined)
            return MakeStop(StopStatus::AlreadyStopped);
        if (state_ == WorkerState::StopRequested)
            return MakeStop(StopStatus::StopRequested);
        if (state_ == WorkerState::Poisoned)
            return MakeStop(StopStatus::Poisoned, last_error_);
        if (!CanTransition(state_, WorkerState::StopRequested))
            return RejectStop(LifecycleOperation::RequestStop, WorkerState::StopRequested,
                LifecycleErrorCode::InvalidTransition, generation);

        state_ = WorkerState::StopRequested;
        return MakeStop(StopStatus::StopRequested);
    }

    [[nodiscard]] constexpr StopResult ConfirmJoined(GenerationId generation) noexcept
    {
        if (const LifecycleError error = ValidateGeneration(
                generation, LifecycleOperation::ConfirmJoined, WorkerState::Joined);
            error.HasError())
        {
            return MakeStop(StopStatus::Rejected, error);
        }

        if (state_ == WorkerState::Joined)
            return MakeStop(StopStatus::Joined);
        if (state_ == WorkerState::Stopped)
            return MakeStop(StopStatus::AlreadyStopped);
        if (state_ == WorkerState::Poisoned)
            return MakeStop(StopStatus::Poisoned, last_error_);
        if (!CanTransition(state_, WorkerState::Joined))
            return RejectStop(LifecycleOperation::ConfirmJoined, WorkerState::Joined,
                LifecycleErrorCode::InvalidTransition, generation);

        state_ = WorkerState::Joined;
        return MakeStop(StopStatus::Joined);
    }

    [[nodiscard]] constexpr TransitionResult MarkFaulted(
        GenerationId generation,
        LifecycleOperation operation,
        std::uint32_t native_error = 0) noexcept
    {
        if (const LifecycleError error = ValidateGeneration(
                generation, operation, WorkerState::Faulted);
            error.HasError())
        {
            return MakeTransition(TransitionStatus::Rejected, error);
        }
        if (state_ == WorkerState::Faulted)
            return MakeTransition(TransitionStatus::NoChange, last_error_);
        if (!CanTransition(state_, WorkerState::Faulted))
            return RejectTransition(operation, WorkerState::Faulted,
                LifecycleErrorCode::InvalidTransition, generation, native_error);

        const WorkerState from = state_;
        state_ = WorkerState::Faulted;
        last_error_ = MakeError(LifecycleErrorCode::WorkerFaulted,
            operation, from, state_, generation, native_error);
        return MakeTransition(TransitionStatus::Applied, last_error_);
    }

    [[nodiscard]] constexpr StopResult MarkPoisoned(
        GenerationId generation,
        LifecycleOperation operation = LifecycleOperation::MarkPoisoned,
        LifecycleErrorCode reason = LifecycleErrorCode::StopTimedOut,
        std::uint32_t native_error = 0) noexcept
    {
        if (const LifecycleError error = ValidateGeneration(
                generation, operation, WorkerState::Poisoned);
            error.HasError())
        {
            return MakeStop(StopStatus::Rejected, error);
        }
        if (state_ == WorkerState::Poisoned)
            return MakeStop(StopStatus::Poisoned, last_error_);
        if (!CanTransition(state_, WorkerState::Poisoned))
            return RejectStop(operation, WorkerState::Poisoned,
                LifecycleErrorCode::InvalidTransition, generation, native_error);

        const WorkerState from = state_;
        state_ = WorkerState::Poisoned;
        last_error_ = MakeError(reason == LifecycleErrorCode::None
                ? LifecycleErrorCode::PrimitiveFailed : reason,
            operation, from, state_, generation, native_error);
        return MakeStop(reason == LifecycleErrorCode::StopTimedOut
                ? StopStatus::TimedOut : StopStatus::Poisoned,
            last_error_);
    }

private:
    [[nodiscard]] constexpr LifecycleError ValidateGeneration(
        GenerationId supplied,
        LifecycleOperation operation,
        WorkerState requested) const noexcept
    {
        if (!supplied.IsValid())
            return MakeError(LifecycleErrorCode::InvalidGeneration,
                operation, state_, requested, supplied, 0);
        if (supplied != generation_)
            return MakeError(LifecycleErrorCode::StaleGeneration,
                operation, state_, requested, supplied, 0);
        return {};
    }

    [[nodiscard]] constexpr LifecycleError MakeError(
        LifecycleErrorCode code,
        LifecycleOperation operation,
        WorkerState from,
        WorkerState requested,
        GenerationId supplied,
        std::uint32_t native_error) const noexcept
    {
        return LifecycleError{
            code,
            operation,
            from,
            requested,
            supplied,
            generation_,
            native_error,
        };
    }

    [[nodiscard]] constexpr StartResult MakeStart(
        StartStatus status, LifecycleError error = {}) const noexcept
    {
        return StartResult{ status, state_, generation_, error };
    }

    [[nodiscard]] constexpr StopResult MakeStop(
        StopStatus status, LifecycleError error = {}) const noexcept
    {
        return StopResult{ status, state_, generation_, error };
    }

    [[nodiscard]] constexpr TransitionResult MakeTransition(
        TransitionStatus status, LifecycleError error = {}) const noexcept
    {
        return TransitionResult{ status, state_, generation_, error };
    }

    [[nodiscard]] constexpr StartResult RejectStart(
        LifecycleOperation operation,
        WorkerState requested,
        LifecycleErrorCode code,
        GenerationId supplied,
        std::uint32_t native_error = 0) const noexcept
    {
        return MakeStart(StartStatus::Rejected,
            MakeError(code, operation, state_, requested, supplied, native_error));
    }

    [[nodiscard]] constexpr StopResult RejectStop(
        LifecycleOperation operation,
        WorkerState requested,
        LifecycleErrorCode code,
        GenerationId supplied,
        std::uint32_t native_error = 0) const noexcept
    {
        return MakeStop(StopStatus::Rejected,
            MakeError(code, operation, state_, requested, supplied, native_error));
    }

    [[nodiscard]] constexpr TransitionResult RejectTransition(
        LifecycleOperation operation,
        WorkerState requested,
        LifecycleErrorCode code,
        GenerationId supplied,
        std::uint32_t native_error = 0) const noexcept
    {
        return MakeTransition(TransitionStatus::Rejected,
            MakeError(code, operation, state_, requested, supplied, native_error));
    }

    WorkerState state_ = WorkerState::Stopped;
    GenerationId generation_{};
    LifecycleError last_error_{};
};

} // namespace halljoy::lifecycle
