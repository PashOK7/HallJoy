#include "vigem_child_transport.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace halljoy::vigem_output;

namespace
{

enum class Call : std::uint32_t
{
    ClientAllocate = 1,
    ClientConnect,
    TargetAllocate,
    TargetAdd,
    ReportUpdate,
    NeutralUpdate,
    TargetRemove,
    TargetFree,
    ClientDisconnect,
    ClientFree,
};

struct FakeState
{
    bool failClientAllocate = false;
    VIGEM_ERROR connectError = VIGEM_ERROR_NONE;
    int failTargetAllocate = -1;
    int failTargetAdd = -1;
    int failReportUpdate = -1;
    int failNeutralUpdate = -1;
    int failTargetRemove = -1;
    std::uint32_t nextTarget = 0;
    bool clientLive = false;
    std::array<bool, kMaxPads> targetLive{};
    std::array<bool, kMaxPads> targetAdded{};
    std::array<unsigned, kMaxPads> reportUpdates{};
    std::array<unsigned, kMaxPads> neutralUpdates{};
    std::array<unsigned, kMaxPads> removeAttempts{};
    std::vector<std::uint32_t> calls;
};

FakeState* g_state = nullptr;

std::uint32_t Token(Call call, std::uint32_t pad = UINT32_MAX) noexcept
{
    return (static_cast<std::uint32_t>(call) << 8u) |
        (pad == UINT32_MAX ? 0xffu : pad);
}

PVIGEM_CLIENT FakeClientAllocate()
{
    assert(g_state);
    g_state->calls.push_back(Token(Call::ClientAllocate));
    if (g_state->failClientAllocate)
        return nullptr;
    g_state->clientLive = true;
    return reinterpret_cast<PVIGEM_CLIENT>(static_cast<std::uintptr_t>(0x1000u));
}

void FakeClientFree(PVIGEM_CLIENT client)
{
    assert(g_state && client && g_state->clientLive);
    g_state->calls.push_back(Token(Call::ClientFree));
    g_state->clientLive = false;
    for (bool& added : g_state->targetAdded)
        added = false; // process-owner teardown removes any failed-remove target
}

VIGEM_ERROR FakeClientConnect(PVIGEM_CLIENT client)
{
    assert(g_state && client && g_state->clientLive);
    g_state->calls.push_back(Token(Call::ClientConnect));
    return g_state->connectError;
}

void FakeClientDisconnect(PVIGEM_CLIENT client)
{
    assert(g_state && client && g_state->clientLive);
    g_state->calls.push_back(Token(Call::ClientDisconnect));
}

PVIGEM_TARGET FakeTargetAllocate()
{
    assert(g_state);
    const std::uint32_t index = g_state->nextTarget++;
    assert(index < kMaxPads);
    g_state->calls.push_back(Token(Call::TargetAllocate, index));
    if (static_cast<int>(index) == g_state->failTargetAllocate)
        return nullptr;
    g_state->targetLive[index] = true;
    return reinterpret_cast<PVIGEM_TARGET>(
        static_cast<std::uintptr_t>(0x2000u + index + 1u));
}

std::uint32_t TargetIndex(PVIGEM_TARGET target)
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(target);
    assert(value > 0x2000u && value <= 0x2000u + kMaxPads);
    return static_cast<std::uint32_t>(value - 0x2001u);
}

void FakeTargetFree(PVIGEM_TARGET target)
{
    assert(g_state && target);
    const std::uint32_t index = TargetIndex(target);
    assert(g_state->targetLive[index]);
    g_state->calls.push_back(Token(Call::TargetFree, index));
    g_state->targetLive[index] = false;
}

VIGEM_ERROR FakeTargetAdd(PVIGEM_CLIENT client, PVIGEM_TARGET target)
{
    assert(g_state && client && target);
    const std::uint32_t index = TargetIndex(target);
    g_state->calls.push_back(Token(Call::TargetAdd, index));
    if (static_cast<int>(index) == g_state->failTargetAdd)
        return VIGEM_ERROR_NO_FREE_SLOT;
    g_state->targetAdded[index] = true;
    return VIGEM_ERROR_NONE;
}

bool IsNeutral(const XUSB_REPORT& report) noexcept
{
    return report.wButtons == 0u && report.bLeftTrigger == 0u &&
        report.bRightTrigger == 0u && report.sThumbLX == 0 &&
        report.sThumbLY == 0 && report.sThumbRX == 0 && report.sThumbRY == 0;
}

VIGEM_ERROR FakeTargetUpdate(PVIGEM_CLIENT client, PVIGEM_TARGET target,
    XUSB_REPORT report)
{
    assert(g_state && client && target);
    const std::uint32_t index = TargetIndex(target);
    assert(g_state->targetAdded[index]);
    if (IsNeutral(report))
    {
        g_state->calls.push_back(Token(Call::NeutralUpdate, index));
        ++g_state->neutralUpdates[index];
        return static_cast<int>(index) == g_state->failNeutralUpdate
            ? VIGEM_ERROR_TIMED_OUT
            : VIGEM_ERROR_NONE;
    }
    g_state->calls.push_back(Token(Call::ReportUpdate, index));
    ++g_state->reportUpdates[index];
    return static_cast<int>(index) == g_state->failReportUpdate
        ? VIGEM_ERROR_BUS_INVALID_HANDLE
        : VIGEM_ERROR_NONE;
}

VIGEM_ERROR FakeTargetRemove(PVIGEM_CLIENT client, PVIGEM_TARGET target)
{
    assert(g_state && client && target);
    const std::uint32_t index = TargetIndex(target);
    assert(g_state->targetAdded[index]);
    g_state->calls.push_back(Token(Call::TargetRemove, index));
    ++g_state->removeAttempts[index];
    if (static_cast<int>(index) == g_state->failTargetRemove)
        return VIGEM_ERROR_REMOVAL_FAILED;
    g_state->targetAdded[index] = false;
    return VIGEM_ERROR_NONE;
}

const VigemApiV1 kFakeApi{
    &FakeClientAllocate,
    &FakeClientFree,
    &FakeClientConnect,
    &FakeClientDisconnect,
    &FakeTargetAllocate,
    &FakeTargetFree,
    &FakeTargetAdd,
    &FakeTargetRemove,
    &FakeTargetUpdate,
};

SnapshotPayloadV1 MakeSnapshot(std::uint32_t padCount,
    std::uint32_t validMask) noexcept
{
    SnapshotPayloadV1 snapshot{};
    snapshot.outputGeneration = 1u;
    snapshot.publicationSequence = 1u;
    snapshot.padCount = padCount;
    snapshot.validMask = validMask;
    for (std::uint32_t index = 0; index < padCount && index < kMaxPads; ++index)
    {
        snapshot.reports[index].buttons = static_cast<std::uint16_t>(1u << index);
        snapshot.reports[index].leftTrigger = static_cast<std::uint8_t>(10u + index);
        snapshot.reports[index].thumbLX = static_cast<std::int16_t>(100 + index);
    }
    return snapshot;
}

void AssertReleased(const FakeState& state)
{
    assert(!state.clientLive);
    for (std::uint32_t index = 0; index < kMaxPads; ++index)
    {
        assert(!state.targetLive[index]);
        assert(!state.targetAdded[index]);
    }
}

void TestSuccessfulLifecycle()
{
    FakeState state{};
    g_state = &state;
    VigemChildTransport transport(kFakeApi);
    const VigemTransportResult started = transport.Start(kMaxPads);
    assert(started.Succeeded());
    assert(transport.IsStarted());
    assert(transport.TargetCount() == kMaxPads);

    const VigemTransportResult applied = transport.Apply(
        MakeSnapshot(kMaxPads, (1u << kMaxPads) - 1u));
    assert(applied.Succeeded());
    const VigemTransportResult stopped = transport.Stop();
    assert(stopped.Succeeded());
    assert(stopped.neutralApplied);
    assert(stopped.targetsRemoved);
    for (std::uint32_t index = 0; index < kMaxPads; ++index)
    {
        assert(state.reportUpdates[index] == 1u);
        assert(state.neutralUpdates[index] == 2u);
        assert(state.removeAttempts[index] == 1u);
    }
    const auto firstNeutral = std::find(state.calls.begin(), state.calls.end(),
        Token(Call::NeutralUpdate, 0u));
    const auto firstRemove = std::find(state.calls.begin(), state.calls.end(),
        Token(Call::TargetRemove, 0u));
    assert(firstNeutral != state.calls.end() && firstRemove != state.calls.end());
    assert(firstNeutral < firstRemove);
    AssertReleased(state);
}

void TestStartFailures()
{
    {
        FakeState state{};
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        assert(transport.Start(0u).error == VIGEM_ERROR_INVALID_PARAMETER);
        assert(transport.Start(kMaxPads + 1u).error == VIGEM_ERROR_INVALID_PARAMETER);
        assert(state.calls.empty());
    }
    {
        FakeState state{};
        state.failClientAllocate = true;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        const auto result = transport.Start(1u);
        assert(result.error == VIGEM_ERROR_BUS_NOT_FOUND);
        assert(result.phase == VigemTransportPhase::ClientAllocate);
        AssertReleased(state);
    }
    {
        FakeState state{};
        state.connectError = VIGEM_ERROR_BUS_VERSION_MISMATCH;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        const auto result = transport.Start(1u);
        assert(result.error == VIGEM_ERROR_BUS_VERSION_MISMATCH);
        assert(result.phase == VigemTransportPhase::ClientConnect);
        AssertReleased(state);
    }

    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        state.failTargetAllocate = failed;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        const auto result = transport.Start(kMaxPads);
        assert(result.error == VIGEM_ERROR_INVALID_TARGET);
        assert(result.phase == VigemTransportPhase::TargetAllocate);
        assert(result.padIndex == static_cast<std::uint32_t>(failed));
        for (int index = 0; index < failed; ++index)
        {
            assert(state.neutralUpdates[static_cast<std::size_t>(index)] == 1u);
            assert(state.removeAttempts[static_cast<std::size_t>(index)] == 1u);
        }
        AssertReleased(state);
    }

    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        state.failTargetAdd = failed;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        const auto result = transport.Start(kMaxPads);
        assert(result.error == VIGEM_ERROR_NO_FREE_SLOT);
        assert(result.phase == VigemTransportPhase::TargetAdd);
        assert(result.padIndex == static_cast<std::uint32_t>(failed));
        for (int index = 0; index < failed; ++index)
        {
            assert(state.neutralUpdates[static_cast<std::size_t>(index)] == 1u);
            assert(state.removeAttempts[static_cast<std::size_t>(index)] == 1u);
        }
        AssertReleased(state);
    }

    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        state.failNeutralUpdate = failed;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        const auto result = transport.Start(kMaxPads);
        assert(result.error == VIGEM_ERROR_TIMED_OUT);
        assert(result.phase == VigemTransportPhase::InitialNeutral);
        assert(result.padIndex == static_cast<std::uint32_t>(failed));
        for (std::uint32_t index = 0; index < kMaxPads; ++index)
        {
            const unsigned expected = index <= static_cast<std::uint32_t>(failed)
                ? 2u : 1u;
            assert(state.neutralUpdates[index] == expected);
            assert(state.removeAttempts[index] == 1u);
        }
        AssertReleased(state);
    }
}

void TestUpdateAndExhaustiveStopFailures()
{
    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        state.failReportUpdate = failed;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        assert(transport.Start(kMaxPads).Succeeded());
        const auto applied = transport.Apply(
            MakeSnapshot(kMaxPads, (1u << kMaxPads) - 1u));
        assert(applied.error == VIGEM_ERROR_BUS_INVALID_HANDLE);
        assert(applied.phase == VigemTransportPhase::ReportUpdate);
        assert(applied.padIndex == static_cast<std::uint32_t>(failed));
        const auto stopped = transport.Stop();
        assert(stopped.Succeeded());
        for (std::uint32_t index = 0; index < kMaxPads; ++index)
        {
            assert(state.neutralUpdates[index] == 2u);
            assert(state.removeAttempts[index] == 1u);
        }
        AssertReleased(state);
    }

    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        assert(transport.Start(kMaxPads).Succeeded());
        state.failNeutralUpdate = failed;
        const auto stopped = transport.Stop();
        assert(stopped.error == VIGEM_ERROR_TIMED_OUT);
        assert(stopped.phase == VigemTransportPhase::NeutralUpdate);
        assert(stopped.padIndex == static_cast<std::uint32_t>(failed));
        assert(!stopped.neutralApplied);
        assert(stopped.targetsRemoved);
        for (std::uint32_t index = 0; index < kMaxPads; ++index)
        {
            assert(state.neutralUpdates[index] == 2u);
            assert(state.removeAttempts[index] == 1u);
        }
        AssertReleased(state);
    }

    for (int failed = 0; failed < static_cast<int>(kMaxPads); ++failed)
    {
        FakeState state{};
        state.failTargetRemove = failed;
        g_state = &state;
        VigemChildTransport transport(kFakeApi);
        assert(transport.Start(kMaxPads).Succeeded());
        const auto stopped = transport.Stop();
        assert(stopped.error == VIGEM_ERROR_REMOVAL_FAILED);
        assert(stopped.phase == VigemTransportPhase::TargetRemove);
        assert(stopped.padIndex == static_cast<std::uint32_t>(failed));
        assert(stopped.neutralApplied);
        assert(!stopped.targetsRemoved);
        for (std::uint32_t index = 0; index < kMaxPads; ++index)
        {
            assert(state.neutralUpdates[index] == 2u);
            assert(state.removeAttempts[index] == 1u);
        }
        AssertReleased(state);
    }
}

void TestInvalidSnapshotAndDestructorCleanup()
{
    FakeState state{};
    g_state = &state;
    {
        VigemChildTransport transport(kFakeApi);
        assert(transport.Start(2u).Succeeded());
        assert(transport.Apply(MakeSnapshot(1u, 1u)).error ==
            VIGEM_ERROR_INVALID_PARAMETER);
        assert(transport.Apply(MakeSnapshot(2u, 4u)).error ==
            VIGEM_ERROR_INVALID_PARAMETER);
    }
    assert(state.neutralUpdates[0] == 2u && state.neutralUpdates[1] == 2u);
    assert(state.removeAttempts[0] == 1u && state.removeAttempts[1] == 1u);
    AssertReleased(state);
}

} // namespace

int main()
{
    TestSuccessfulLifecycle();
    TestStartFailures();
    TestUpdateAndExhaustiveStopFailures();
    TestInvalidSnapshotAndDestructorCleanup();
    std::cout << "VIGEM_CHILD_TRANSPORT_TEST=PASS pads=4 "
        "partial_start_edges=8 initial_neutral_edges=4 update_edges=4 "
        "neutral_edges=4 remove_edges=4\n";
    return 0;
}
