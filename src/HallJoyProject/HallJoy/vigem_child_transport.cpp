#include "vigem_child_transport.h"

namespace halljoy::vigem_output
{
namespace
{

XUSB_REPORT ToNativeReport(const XusbReportV1& source) noexcept
{
    XUSB_REPORT report{};
    report.wButtons = source.buttons;
    report.bLeftTrigger = source.leftTrigger;
    report.bRightTrigger = source.rightTrigger;
    report.sThumbLX = source.thumbLX;
    report.sThumbLY = source.thumbLY;
    report.sThumbRX = source.thumbRX;
    report.sThumbRY = source.thumbRY;
    return report;
}

void RecordFirstFailure(VigemTransportResult& result, VIGEM_ERROR error,
    VigemTransportPhase phase, std::uint32_t padIndex) noexcept
{
    if (result.Succeeded() && !VIGEM_SUCCESS(error))
    {
        result.error = error;
        result.phase = phase;
        result.padIndex = padIndex;
    }
}

} // namespace

const VigemApiV1& RealVigemApi() noexcept
{
#if defined(HALLJOY_VIGEM_TRANSPORT_FAKE_ONLY)
    static const VigemApiV1 unavailable{};
    return unavailable;
#else
    static const VigemApiV1 api{
        &vigem_alloc,
        &vigem_free,
        &vigem_connect,
        &vigem_disconnect,
        &vigem_target_x360_alloc,
        &vigem_target_free,
        &vigem_target_add,
        &vigem_target_remove,
        &vigem_target_x360_update,
    };
    return api;
#endif
}

std::uint64_t EncodeVigemTransportCheckpoint(VigemTransportPhase phase,
    std::uint32_t padIndex) noexcept
{
    const std::uint64_t encodedPad = padIndex == UINT32_MAX
        ? 0u
        : static_cast<std::uint64_t>(padIndex) + 1u;
    return (static_cast<std::uint64_t>(phase) << 32u) | encodedPad;
}

VigemChildTransport::VigemChildTransport(const VigemApiV1& api) noexcept
    : api_(api)
{
}

VigemChildTransport::~VigemChildTransport() noexcept
{
    if (client_)
        (void)Cleanup(true);
}

bool VigemChildTransport::ApiComplete() const noexcept
{
    return api_.clientAllocate && api_.clientFree && api_.clientConnect &&
        api_.clientDisconnect && api_.targetAllocate && api_.targetFree &&
        api_.targetAdd && api_.targetRemove && api_.targetUpdate;
}

VigemTransportResult VigemChildTransport::Start(
    std::uint32_t padCount) noexcept
{
    VigemTransportResult result{};
    if (!ApiComplete() || client_ || started_ || padCount == 0u ||
        padCount > kMaxPads)
    {
        result.error = VIGEM_ERROR_INVALID_PARAMETER;
        result.phase = VigemTransportPhase::InvalidState;
        return result;
    }

    targetCount_ = padCount;
    client_ = api_.clientAllocate();
    if (!client_)
    {
        result.error = VIGEM_ERROR_BUS_NOT_FOUND;
        result.phase = VigemTransportPhase::ClientAllocate;
        targetCount_ = 0u;
        return result;
    }

    result.error = api_.clientConnect(client_);
    if (!result.Succeeded())
    {
        result.phase = VigemTransportPhase::ClientConnect;
        (void)Cleanup(true);
        return result;
    }
    connected_ = true;

    for (std::uint32_t index = 0; index < targetCount_; ++index)
    {
        TargetRecord& target = targets_[index];
        target.value = api_.targetAllocate();
        if (!target.value)
        {
            result.error = VIGEM_ERROR_INVALID_TARGET;
            result.phase = VigemTransportPhase::TargetAllocate;
            result.padIndex = index;
            (void)Cleanup(true);
            return result;
        }

        result.error = api_.targetAdd(client_, target.value);
        if (!result.Succeeded())
        {
            result.phase = VigemTransportPhase::TargetAdd;
            result.padIndex = index;
            (void)Cleanup(true);
            return result;
        }
        target.added = true;
    }

    const XUSB_REPORT neutral{};
    for (std::uint32_t index = 0; index < targetCount_; ++index)
    {
        result.error = api_.targetUpdate(
            client_, targets_[index].value, neutral);
        if (!result.Succeeded())
        {
            result.phase = VigemTransportPhase::InitialNeutral;
            result.padIndex = index;
            (void)Cleanup(true);
            return result;
        }
    }

    started_ = true;
    result.error = VIGEM_ERROR_NONE;
    result.phase = VigemTransportPhase::None;
    return result;
}

VigemTransportResult VigemChildTransport::Apply(
    const SnapshotPayloadV1& snapshot) noexcept
{
    VigemTransportResult result{};
    const std::uint32_t allowedMask = targetCount_ == 0u
        ? 0u
        : ((1u << targetCount_) - 1u);
    if (!started_ || !client_ || snapshot.padCount != targetCount_ ||
        (snapshot.validMask & ~allowedMask) != 0u)
    {
        result.error = VIGEM_ERROR_INVALID_PARAMETER;
        result.phase = VigemTransportPhase::InvalidState;
        return result;
    }

    for (std::uint32_t index = 0; index < targetCount_; ++index)
    {
        if ((snapshot.validMask & (1u << index)) == 0u)
            continue;
        const TargetRecord& target = targets_[index];
        if (!target.added || !target.value)
        {
            result.error = VIGEM_ERROR_TARGET_NOT_PLUGGED_IN;
            result.phase = VigemTransportPhase::InvalidState;
            result.padIndex = index;
            return result;
        }
        result.error = api_.targetUpdate(client_, target.value,
            ToNativeReport(snapshot.reports[index]));
        if (!result.Succeeded())
        {
            result.phase = VigemTransportPhase::ReportUpdate;
            result.padIndex = index;
            return result;
        }
    }
    return result;
}

VigemTransportResult VigemChildTransport::Cleanup(bool applyNeutral) noexcept
{
    VigemTransportResult result{};
    result.neutralApplied = true;
    result.targetsRemoved = true;

    if (client_ && applyNeutral)
    {
        const XUSB_REPORT neutral{};
        for (std::uint32_t index = 0; index < targetCount_; ++index)
        {
            const TargetRecord& target = targets_[index];
            if (!target.added || !target.value)
                continue;
            const VIGEM_ERROR error = api_.targetUpdate(
                client_, target.value, neutral);
            if (!VIGEM_SUCCESS(error))
            {
                result.neutralApplied = false;
                RecordFirstFailure(result, error,
                    VigemTransportPhase::NeutralUpdate, index);
            }
        }
    }

    if (client_)
    {
        for (std::uint32_t index = 0; index < targetCount_; ++index)
        {
            TargetRecord& target = targets_[index];
            if (!target.value)
                continue;
            if (target.added)
            {
                const VIGEM_ERROR error = api_.targetRemove(
                    client_, target.value);
                if (!VIGEM_SUCCESS(error))
                {
                    result.targetsRemoved = false;
                    RecordFirstFailure(result, error,
                        VigemTransportPhase::TargetRemove, index);
                }
            }
            api_.targetFree(target.value);
            target.value = nullptr;
            target.added = false;
        }
        if (connected_)
            api_.clientDisconnect(client_);
        api_.clientFree(client_);
    }

    client_ = nullptr;
    targetCount_ = 0u;
    connected_ = false;
    started_ = false;
    return result;
}

VigemTransportResult VigemChildTransport::Stop() noexcept
{
    if (!client_ || !started_)
    {
        VigemTransportResult result{};
        result.error = VIGEM_ERROR_INVALID_PARAMETER;
        result.phase = VigemTransportPhase::InvalidState;
        return result;
    }
    return Cleanup(true);
}

bool VigemChildTransport::IsStarted() const noexcept
{
    return started_;
}

std::uint32_t VigemChildTransport::TargetCount() const noexcept
{
    return targetCount_;
}

} // namespace halljoy::vigem_output
