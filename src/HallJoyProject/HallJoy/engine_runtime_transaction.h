#pragma once

#include <cstdint>

#include "runtime_command_state.h"

namespace halljoy::engine_runtime
{

enum class TransactionResult : std::uint8_t
{
    Completed,
    NoChange,
    Rejected,
    RetryableResumeFailure,
    Faulted,
};

// The concrete owner supplies these operations. They are deliberately sequenced
// here rather than in UI handlers, timers, or individual backend callbacks.
// Every false result means the resource was not confirmed released/started.
template <typename Operations>
[[nodiscard]] TransactionResult ExecutePause(
    runtime_command::Controller& controller,
    Operations& operations,
    std::uint32_t& nativeError) noexcept
{
    const auto request = controller.RequestPause();
    if (request == runtime_command::RequestStatus::NoChange)
        return TransactionResult::NoChange;
    if (request != runtime_command::RequestStatus::Accepted)
        return TransactionResult::Rejected;
    operations.StateChanged(controller.Snapshot());

    if (!operations.CloseAdmission(nativeError))
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }
    if (!controller.AdvancePause())
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }
    operations.StateChanged(controller.Snapshot());

    if (!operations.StopRecoverySupervisor(nativeError) ||
        !operations.PublishNeutral(nativeError) ||
        !operations.StopRealtime(nativeError) ||
        !operations.ReleaseUiInput(nativeError))
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }

    if (!controller.AdvancePause())
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }
    operations.StateChanged(controller.Snapshot());
    if (!operations.StopNativeProviders(nativeError) || !controller.AdvancePause())
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }
    operations.StateChanged(controller.Snapshot());
    if (!operations.ReleaseBackendLeases(nativeError) || !controller.AdvancePause())
    {
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    }
    operations.StateChanged(controller.Snapshot());
    return TransactionResult::Completed;
}

template <typename Operations>
[[nodiscard]] TransactionResult ExecuteResume(
    runtime_command::Controller& controller,
    Operations& operations,
    std::uint32_t& nativeError) noexcept
{
    const auto request = controller.RequestResume();
    if (request == runtime_command::RequestStatus::NoChange)
        return TransactionResult::NoChange;
    if (request != runtime_command::RequestStatus::Accepted)
        return TransactionResult::Rejected;
    operations.StateChanged(controller.Snapshot());

    const auto abortOrFault = [&]() noexcept {
        // Fresh enumeration and failed proof may already own handles, a native
        // routing claim, or a partial provider. Every failed Resume therefore
        // runs the same confirmed-release callback; only a failed release is a
        // retained-resource fault.
        if (operations.ReleaseFailedResume(nativeError))
        {
            (void)controller.AbortResume(nativeError);
            operations.StateChanged(controller.Snapshot());
            return TransactionResult::RetryableResumeFailure;
        }
        controller.Fault(nativeError);
        operations.StateChanged(controller.Snapshot());
        return TransactionResult::Faulted;
    };

    if (!controller.AdvanceResume() || !operations.EnumerateFresh(nativeError))
        return abortOrFault();
    operations.StateChanged(controller.Snapshot());
    if (!controller.AdvanceResume() || !operations.ProveCapabilities(nativeError))
        return abortOrFault();
    operations.StateChanged(controller.Snapshot());
    if (!controller.AdvanceResume() || !operations.StartFreshGeneration(nativeError))
        return abortOrFault();
    operations.StateChanged(controller.Snapshot());
    if (!operations.PublishNeutralGeneration(nativeError) ||
        !operations.RestoreUiInput(nativeError) ||
        !controller.AdvanceResume() ||
        !operations.OpenAdmission(nativeError) ||
        !controller.ConfirmAdmissionOpen())
        return abortOrFault();
    operations.StateChanged(controller.Snapshot());
    return TransactionResult::Completed;
}

} // namespace halljoy::engine_runtime
