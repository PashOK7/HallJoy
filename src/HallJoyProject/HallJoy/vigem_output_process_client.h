#pragma once

#include "process_generation_supervisor.h"
#include "vigem_output_channel.h"
#include "vigem_output_process_protocol.h"

#include <cstdint>
#include <atomic>
#include <memory>
#include <string>

namespace halljoy::vigem_output
{

enum class OutputGenerationOutcome : std::uint32_t
{
    CleanPlannedStop = 0,
    IncompletePlannedStop,
    ExitBeforeReady,
    UnexpectedExit,
    StartupTimeout,
    ProgressTimeout,
    ChildFault,
    ProtocolViolation,
    LaunchFailed,
    WaitFailed,
    ReapFailed,
    SessionFailure,
    Unsupported,
};

enum class OutputPublishResult : std::uint32_t
{
    Published = 0,
    Inactive,
    InvalidPayload,
    Contended,
    WakeFailed,
    SessionUnavailable,
};

struct OutputGenerationRequest
{
    std::wstring applicationPath;
    std::uint64_t generation = 0;
    std::uint32_t padCount = 0;
    OutputHostMode mode = OutputHostMode::Real;
    process::NativeHandle supervisorStopEvent = nullptr;
    process::SupervisionPolicy policy{};
};

struct OutputGenerationResult
{
    OutputGenerationOutcome outcome = OutputGenerationOutcome::SessionFailure;
    process::SupervisionResult process{};
    ChildTelemetrySnapshotV1 finalTelemetry{};
    bool finalTelemetryAvailable = false;
    bool generationDisabled = false;
    bool publicationQuiescent = false;
    bool neutralApplied = false;
    bool targetsRemoved = false;
    bool cleanStopConfirmed = false;
    bool restartSafe = false;
    std::uint32_t nativeError = 0;
};

// Process-lifetime IPC owner for the future ViGEm output service. RunOne is
// synchronous and belongs on one non-realtime owner thread. TryPublish is the
// only realtime-facing method and remains bounded through the R1.1 channel.
class OutputProcessSession final
{
public:
    OutputProcessSession() noexcept;
    ~OutputProcessSession() noexcept;

    OutputProcessSession(const OutputProcessSession&) = delete;
    OutputProcessSession& operator=(const OutputProcessSession&) = delete;
    OutputProcessSession(OutputProcessSession&&) = delete;
    OutputProcessSession& operator=(OutputProcessSession&&) = delete;

    bool Initialize(std::uint32_t& error) noexcept;
    bool Shutdown(std::uint32_t& error) noexcept;

    [[nodiscard]] OutputGenerationResult RunOne(
        const OutputGenerationRequest& request) noexcept;

    [[nodiscard]] OutputPublishResult TryPublish(
        const XusbReportV1* reports,
        std::uint32_t padCount,
        std::uint32_t validMask,
        std::uint64_t timestampUs,
        std::uint64_t* publicationSequence = nullptr) noexcept;

    [[nodiscard]] bool PublishProducerProgress(
        std::uint64_t tickMs) noexcept;

    [[nodiscard]] bool ReadTelemetry(
        ChildTelemetrySnapshotV1& snapshot) noexcept;

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] bool RestartSafe() const noexcept;
    [[nodiscard]] std::uint32_t OwnerPid() const noexcept;
    [[nodiscard]] std::uint64_t LaunchNonce() const noexcept;

private:
    void CloseResources() noexcept;

    std::unique_ptr<process::ProcessGenerationSupervisor> supervisor_;
    process::NativeHandle mapping_ = nullptr;
    process::NativeHandle wakeEvent_ = nullptr;
    process::NativeHandle childStopEvent_ = nullptr;
    process::NativeHandle ownerProcess_ = nullptr;
    SharedStateV1* shared_ = nullptr;
    std::uint32_t ownerPid_ = 0;
    std::uint64_t launchNonce_ = 0;
    bool initialized_ = false;
    std::atomic<bool> restartSafe_{ true };
};

} // namespace halljoy::vigem_output
