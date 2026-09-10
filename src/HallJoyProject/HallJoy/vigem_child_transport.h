#pragma once

#include "vigem_output_shared.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <ViGEm/Client.h>

#include <array>
#include <cstdint>

namespace halljoy::vigem_output
{

enum class VigemTransportPhase : std::uint32_t
{
    None = 0,
    InvalidState,
    ClientAllocate,
    ClientConnect,
    TargetAllocate,
    TargetAdd,
    InitialNeutral,
    ReportUpdate,
    NeutralUpdate,
    TargetRemove,
};

struct VigemTransportResult
{
    VIGEM_ERROR error = VIGEM_ERROR_NONE;
    VigemTransportPhase phase = VigemTransportPhase::None;
    std::uint32_t padIndex = UINT32_MAX;
    bool neutralApplied = false;
    bool targetsRemoved = false;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return VIGEM_SUCCESS(error);
    }
};

// Immutable API dependency. Production uses `RealVigemApi`; deterministic
// failure tests provide a complete fake table and cannot be selected by host
// protocol or shared state.
struct VigemApiV1
{
    decltype(&vigem_alloc) clientAllocate = nullptr;
    decltype(&vigem_free) clientFree = nullptr;
    decltype(&vigem_connect) clientConnect = nullptr;
    decltype(&vigem_disconnect) clientDisconnect = nullptr;
    decltype(&vigem_target_x360_alloc) targetAllocate = nullptr;
    decltype(&vigem_target_free) targetFree = nullptr;
    decltype(&vigem_target_add) targetAdd = nullptr;
    decltype(&vigem_target_remove) targetRemove = nullptr;
    decltype(&vigem_target_x360_update) targetUpdate = nullptr;
};

[[nodiscard]] const VigemApiV1& RealVigemApi() noexcept;

[[nodiscard]] std::uint64_t EncodeVigemTransportCheckpoint(
    VigemTransportPhase phase,
    std::uint32_t padIndex) noexcept;

class VigemChildTransport final
{
public:
    explicit VigemChildTransport(const VigemApiV1& api) noexcept;
    ~VigemChildTransport() noexcept;

    VigemChildTransport(const VigemChildTransport&) = delete;
    VigemChildTransport& operator=(const VigemChildTransport&) = delete;
    VigemChildTransport(VigemChildTransport&&) = delete;
    VigemChildTransport& operator=(VigemChildTransport&&) = delete;

    [[nodiscard]] VigemTransportResult Start(std::uint32_t padCount) noexcept;
    [[nodiscard]] VigemTransportResult Apply(
        const SnapshotPayloadV1& snapshot) noexcept;
    [[nodiscard]] VigemTransportResult Stop() noexcept;

    [[nodiscard]] bool IsStarted() const noexcept;
    [[nodiscard]] std::uint32_t TargetCount() const noexcept;

private:
    struct TargetRecord
    {
        PVIGEM_TARGET value = nullptr;
        bool added = false;
    };

    [[nodiscard]] bool ApiComplete() const noexcept;
    [[nodiscard]] VigemTransportResult Cleanup(bool applyNeutral) noexcept;

    const VigemApiV1& api_;
    PVIGEM_CLIENT client_ = nullptr;
    std::array<TargetRecord, kMaxPads> targets_{};
    std::uint32_t targetCount_ = 0;
    bool connected_ = false;
    bool started_ = false;
};

} // namespace halljoy::vigem_output
