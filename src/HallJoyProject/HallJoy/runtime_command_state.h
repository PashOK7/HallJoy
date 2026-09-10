#pragma once

#include <cstdint>

namespace halljoy::runtime_command
{

enum class State : std::uint8_t
{
    Active,
    PauseRequested,
    Neutralizing,
    StoppingProviders,
    ReleasingLeases,
    Paused,
    ResumeRequested,
    Enumerating,
    ProvingCapabilities,
    PublishingNeutralGeneration,
    PauseFaulted,
};

enum class RequestStatus : std::uint8_t
{
    Accepted,
    NoChange,
    Rejected,
};

struct SnapshotV1
{
    // Before the runtime owner has successfully acquired every provider and
    // published a fresh neutral generation, it owns no admitted device lease.
    State state = State::Paused;
    std::uint64_t commandGeneration = 0;
    std::uint32_t lastNativeError = 0;
    bool opensAdmitted = false;
};

// Allocation-free, single-owner state machine. The engine runtime owner owns
// mutation; UI only sends explicit requests and reads snapshots.
class Controller final
{
public:
    [[nodiscard]] constexpr SnapshotV1 Snapshot() const noexcept { return snapshot_; }

    [[nodiscard]] constexpr RequestStatus RequestPause() noexcept
    {
        if (snapshot_.state == State::Active)
        {
            ++snapshot_.commandGeneration;
            snapshot_.state = State::PauseRequested;
            snapshot_.opensAdmitted = false;
            snapshot_.lastNativeError = 0;
            return RequestStatus::Accepted;
        }
        switch (snapshot_.state)
        {
        case State::PauseRequested:
        case State::Neutralizing:
        case State::StoppingProviders:
        case State::ReleasingLeases:
        case State::Paused:
            return RequestStatus::NoChange;
        default:
            return RequestStatus::Rejected;
        }
    }

    [[nodiscard]] constexpr RequestStatus RequestResume() noexcept
    {
        if (snapshot_.state == State::Paused)
        {
            ++snapshot_.commandGeneration;
            snapshot_.state = State::ResumeRequested;
            snapshot_.opensAdmitted = false;
            snapshot_.lastNativeError = 0;
            return RequestStatus::Accepted;
        }
        switch (snapshot_.state)
        {
        case State::ResumeRequested:
        case State::Enumerating:
        case State::ProvingCapabilities:
        case State::PublishingNeutralGeneration:
        case State::Active:
            return RequestStatus::NoChange;
        default:
            return RequestStatus::Rejected;
        }
    }

    [[nodiscard]] constexpr bool AdvancePause() noexcept
    {
        switch (snapshot_.state)
        {
        case State::PauseRequested: snapshot_.state = State::Neutralizing; return true;
        case State::Neutralizing: snapshot_.state = State::StoppingProviders; return true;
        case State::StoppingProviders: snapshot_.state = State::ReleasingLeases; return true;
        case State::ReleasingLeases: snapshot_.state = State::Paused; return true;
        default: return false;
        }
    }

    [[nodiscard]] constexpr bool AdvanceResume() noexcept
    {
        switch (snapshot_.state)
        {
        case State::ResumeRequested: snapshot_.state = State::Enumerating; return true;
        case State::Enumerating: snapshot_.state = State::ProvingCapabilities; return true;
        case State::ProvingCapabilities: snapshot_.state = State::PublishingNeutralGeneration; return true;
        case State::PublishingNeutralGeneration:
            snapshot_.state = State::Active;
            return true;
        default: return false;
        }
    }

    // The owner calls this only after the concrete backend admission gate was
    // opened. Keeping this separate prevents a state transition from claiming
    // that callbacks may enter before the gate itself has succeeded.
    [[nodiscard]] constexpr bool ConfirmAdmissionOpen() noexcept
    {
        if (snapshot_.state != State::Active || snapshot_.opensAdmitted)
            return false;
        snapshot_.opensAdmitted = true;
        return true;
    }

    // A failed fresh enumeration/proof has released every resource acquired by
    // that attempt. It is safe to remain paused and let the user explicitly
    // retry; unlike Fault(), this never represents a retained live owner.
    [[nodiscard]] constexpr bool AbortResume(std::uint32_t nativeError) noexcept
    {
        switch (snapshot_.state)
        {
        case State::ResumeRequested:
        case State::Enumerating:
        case State::ProvingCapabilities:
        case State::PublishingNeutralGeneration:
            snapshot_.state = State::Paused;
            snapshot_.opensAdmitted = false;
            snapshot_.lastNativeError = nativeError;
            return true;
        default:
            return false;
        }
    }

    constexpr void Fault(std::uint32_t nativeError) noexcept
    {
        snapshot_.state = State::PauseFaulted;
        snapshot_.opensAdmitted = false;
        snapshot_.lastNativeError = nativeError;
    }

private:
    SnapshotV1 snapshot_{};
};

} // namespace halljoy::runtime_command
