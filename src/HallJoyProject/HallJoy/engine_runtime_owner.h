#pragma once

#include <cstdint>

#include "runtime_command_state.h"
#include "worker_lifecycle.h"

// The aggregate engine has exactly one normal command executor.  Its callback
// table is deliberately C-shaped: production callbacks must be noexcept and
// must provide their own bounded UI acknowledgement where UI work is needed.
namespace halljoy::engine_runtime
{

using Operation = bool (*)(void* context, std::uint32_t& nativeError) noexcept;

struct OperationsV1 final
{
    void* context = nullptr;
    Operation closeAdmission = nullptr;
    Operation stopRecoverySupervisor = nullptr;
    Operation publishNeutral = nullptr;
    Operation stopRealtime = nullptr;
    Operation releaseUiInput = nullptr;
    Operation stopNativeProviders = nullptr;
    Operation releaseBackendLeases = nullptr;
    Operation enumerateFresh = nullptr;
    Operation proveCapabilities = nullptr;
    Operation startFreshGeneration = nullptr;
    Operation publishNeutralGeneration = nullptr;
    Operation restoreUiInput = nullptr;
    Operation openAdmission = nullptr;
    Operation releaseFailedResume = nullptr;
    // Optional nonblocking notification under the owner lock: enqueue only;
    // never call back into the owner or wait for the UI.
    void (*stateChanged)(void* context) noexcept = nullptr;
};

enum class SubmitStatus : std::uint8_t
{
    Queued,
    NoChange,
    Rejected,
};

// Starts the command thread in Paused state.  Initial application startup is
// migrated through the same owner rather than adopting pre-existing leases.
bool EngineRuntimeOwner_Start(const OperationsV1& operations) noexcept;
SubmitStatus EngineRuntimeOwner_RequestPause() noexcept;
SubmitStatus EngineRuntimeOwner_RequestResume() noexcept;
runtime_command::SnapshotV1 EngineRuntimeOwner_Snapshot() noexcept;
halljoy::lifecycle::StopResult EngineRuntimeOwner_Stop() noexcept;
bool EngineRuntimeOwner_IsRunning() noexcept;

} // namespace halljoy::engine_runtime
