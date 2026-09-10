#pragma once

#include "vigem_output_process_client.h"

#include <cstdint>
#include <memory>

namespace halljoy::vigem_output
{

enum class OutputRuntimeState : std::uint32_t
{
    Stopped = 0,
    Starting,
    Disabled,
    Ready,
    Recovering,
    Stopping,
    Faulted,
};

struct OutputRuntimeStatus
{
    OutputRuntimeState state = OutputRuntimeState::Stopped;
    bool desiredEnabled = false;
    bool ready = false;
    bool restartSafe = true;
    std::uint32_t desiredPadCount = 0;
    std::uint32_t childPid = 0;
    std::uint32_t lastError = 0;
    OutputGenerationOutcome lastOutcome =
        OutputGenerationOutcome::SessionFailure;
    std::uint64_t activeGeneration = 0;
    std::uint64_t completedGeneration = 0;
    std::uint64_t restartCount = 0;
    std::uint64_t sessionRebuildCount = 0;
    std::uint64_t lastUnsafeGeneration = 0;
    std::uint32_t lastUnsafeFlags = 0;
    std::uint64_t appliedPublicationSequence = 0;
    std::uint64_t diagnosticCheckpoint = 0;
    std::uint64_t heartbeatTickMs = 0;
};

// Sole production owner for the HallJoy -> ViGEm process route. Start/Stop and
// Configure belong to the UI/lifecycle owner. TryPublish is the only realtime
// entry and never waits or takes a mutex.
class OutputRuntime final
{
public:
    OutputRuntime() noexcept;
    ~OutputRuntime() noexcept;

    OutputRuntime(const OutputRuntime&) = delete;
    OutputRuntime& operator=(const OutputRuntime&) = delete;
    OutputRuntime(OutputRuntime&&) = delete;
    OutputRuntime& operator=(OutputRuntime&&) = delete;

    [[nodiscard]] bool Start(bool enabled, std::uint32_t padCount,
        std::uint32_t& error) noexcept;
    [[nodiscard]] bool Stop(std::uint32_t& error) noexcept;
    void Configure(bool enabled, std::uint32_t padCount) noexcept;
    void RequestRestart() noexcept;

    [[nodiscard]] OutputPublishResult TryPublish(
        const XusbReportV1* reports,
        std::uint32_t padCount,
        std::uint32_t validMask,
        std::uint64_t timestampUs,
        std::uint64_t* publicationSequence = nullptr) noexcept;

    // Called after a complete backend calculation, including unchanged held
    // reports. This intentionally does not wake the child or allocate a slot.
    [[nodiscard]] bool PublishProducerProgress(std::uint64_t tickMs) noexcept;

    [[nodiscard]] OutputRuntimeStatus GetStatus() noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace halljoy::vigem_output
