#include "../HallJoy/aula_win60he_client.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
using namespace aula_win60he;

enum class Fault
{
    None,
    FlushFailure,
    WriteFailure,
    TimeoutFirstRead,
    DropLastReport,
    CorruptChecksum,
    WrongResponseCommand,
    WrongFirmware,
    WrongBuildDate,
    WrongPrecision,
    MutateDefaultMap,
    WrongActiveKey,
    WrongActiveLayout,
    InconsistentPadding,
    ActiveMapGenerationMismatch,
    OldStage8TravelHeader,
    ImplausibleTravel,
    BadStatusTrailer,
};

class FirmwareShapedTransport final : public ReportTransport
{
public:
    FirmwareShapedTransport()
        : defaultMap_(ExpectedAulaWin60HeMaxDefaultMap())
    {
        for (const auto& row : defaultMap_)
        {
            for (const std::uint8_t key : row)
            {
                if (key == 0)
                    continue;
                activeFunctions_[key] = key == 0x01u
                    ? 0xF001u
                    : static_cast<std::uint16_t>(key);
            }
        }
    }

    bool FlushInput() override
    {
        ++transactionCount_;
        ++flushCount_;
        currentFault_ = Fault::None;
        if (faultTransaction_ == transactionCount_)
        {
            currentFault_ = armedFault_;
            armedFault_ = Fault::None;
            faultTransaction_ = 0;
        }

        flushedReports_ += reads_.size();
        reads_.clear();
        readsThisTransaction_ = 0;
        return currentFault_ != Fault::FlushFailure;
    }

    bool WriteReport(const Report& report) override
    {
        ++writeCount_;
        writes_.push_back(report);
        if (!ValidRequest(report) || currentFault_ == Fault::WriteFailure)
            return false;

        const std::uint8_t command = report[2];
        const auto* payload = report.data() + 4u;
        std::vector<std::uint8_t> response;
        if (command == kCommandSync && report[1] == 6u)
        {
            response = SyncPayload(
                currentFault_ == Fault::WrongFirmware,
                currentFault_ == Fault::WrongBuildDate);
        }
        else if (command == kCommandApi && report[1] == 3u &&
            payload[0] == kOrderPrecisionStroke)
        {
            response = PrecisionPayload(currentFault_ == Fault::WrongPrecision);
        }
        else if (command == kCommandDefaultKeys && report[1] == 3u)
        {
            response = DefaultMapPayload(payload[1], payload[2],
                currentFault_ == Fault::MutateDefaultMap);
        }
        else if (command == kCommandKeyFunctions &&
            report[1] == kKeyFunctionPayloadBytes)
        {
            response = ActiveMapPayload(report);
        }
        else if (command == kCommandMatrix6x21 && report[1] == 4u &&
            payload[0] == kSelectorTravel &&
            (payload[1] == 1u || payload[1] == 2u))
        {
            response = TravelPayload(payload[1]);
        }
        else if (command == kCommandMatrix6x21 && report[1] == 4u &&
            payload[0] == kSelectorStatus)
        {
            response = StatusPayload();
        }
        else
        {
            return false;
        }

        QueueResponse(command, response);
        return true;
    }

    bool ReadReport(std::uint32_t timeoutMs, Report* out) override
    {
        if (out)
            out->fill(0);
        if (!out)
            return false;
        if (currentFault_ == Fault::TimeoutFirstRead &&
            readsThisTransaction_ == 0)
        {
            ++readsThisTransaction_;
            nowMs_ += timeoutMs;
            return false;
        }
        if (reads_.empty())
        {
            nowMs_ += timeoutMs;
            return false;
        }
        *out = reads_.front();
        reads_.pop_front();
        ++readsThisTransaction_;
        ++nowMs_;
        return true;
    }

    std::uint64_t NowMilliseconds() const noexcept override
    {
        return nowMs_;
    }

    void ArmFault(Fault fault, std::size_t transactionsFromNow = 1u)
    {
        assert(fault != Fault::None);
        assert(transactionsFromNow != 0u);
        armedFault_ = fault;
        faultTransaction_ = transactionCount_ + transactionsFromNow;
    }

    void Preload(const Report& report) { reads_.push_back(report); }

    void SetTravel(
        std::size_t row,
        std::size_t column,
        std::uint16_t value)
    {
        assert(row < kRows && column < kColumns);
        travel_[row][column] = value;
    }

    void SetTravelMatrix(const TravelMatrix& values) { travel_ = values; }

    void SetStatus(
        std::size_t row,
        std::size_t column,
        std::uint8_t value)
    {
        assert(row < kRows && column < kColumns);
        status_[row][column] = value;
    }

    void SetActiveFunction(
        std::uint8_t factoryKey,
        std::uint16_t function)
    {
        activeFunctions_[factoryKey] = function;
    }

    void SetReturnSwappedHalves(bool enabled)
    {
        returnSwappedHalves_ = enabled;
    }

    const TravelMatrix& Travel() const noexcept { return travel_; }
    std::size_t TransactionCount() const noexcept { return transactionCount_; }
    std::size_t FlushCount() const noexcept { return flushCount_; }
    std::size_t WriteCount() const noexcept { return writeCount_; }
    std::size_t FlushedReports() const noexcept { return flushedReports_; }

private:
    static bool ValidRequest(const Report& report) noexcept
    {
        if (report[0] != kFrameHead || report[1] > report.size() - 4u)
            return false;
        return report[3] == ComputeChecksum(
            report[1], report[2], report.data() + 4u);
    }

    static std::vector<std::uint8_t> SyncPayload(
        bool wrongFirmware,
        bool wrongBuildDate)
    {
        std::vector<std::uint8_t> payload(54u, 0);
        payload[0] = 0;
        payload[1] = 0x78;
        payload[2] = 0x56;
        payload[3] = 0x34;
        payload[4] = 0x12;
        payload[7] = 1;
        constexpr char serial[] = "AULA-WIN60-TEST";
        std::copy_n(serial, sizeof(serial) - 1u, payload.begin() + 9u);
        const std::string version = wrongFirmware
            ? "App V1.1.5"
            : "App V1.1.6";
        const std::string buildDate = wrongBuildDate
            ? "Feb  5 2026"
            : "Feb  4 2026";
        std::copy(version.begin(), version.end(), payload.begin() + 26u);
        std::copy(buildDate.begin(), buildDate.end(), payload.begin() + 43u);
        return payload;
    }

    static std::vector<std::uint8_t> PrecisionPayload(bool wrongPrecision)
    {
        const std::uint16_t maximum = wrongPrecision ? 3300u : 3400u;
        return {
            0x00,
            kOrderPrecisionStroke,
            static_cast<std::uint8_t>(kExpectedPrecisionUm),
            static_cast<std::uint8_t>(kExpectedMinimumTravelUm & 0xFFu),
            static_cast<std::uint8_t>(kExpectedMinimumTravelUm >> 8u),
            static_cast<std::uint8_t>(maximum & 0xFFu),
            static_cast<std::uint8_t>(maximum >> 8u),
        };
    }

    std::vector<std::uint8_t> DefaultMapPayload(
        std::uint8_t firstRow,
        std::uint8_t secondRow,
        bool mutate) const
    {
        std::vector<std::uint8_t> payload(kDefaultKeyPayloadBytes, 0);
        payload[0] = 0;
        payload[1] = firstRow;
        payload[23] = secondRow;
        if (firstRow >= kRows || secondRow >= kRows)
            return payload;
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            payload[2u + column] = defaultMap_[firstRow][column];
            payload[24u + column] = defaultMap_[secondRow][column];
        }
        if (mutate)
        {
            const std::size_t offset = firstRow == 0u ? 24u : 2u;
            payload[offset] ^= 0x01u;
        }
        return payload;
    }

    std::vector<std::uint8_t> ActiveMapPayload(const Report& request) const
    {
        std::vector<std::uint8_t> payload(kKeyFunctionPayloadBytes, 0);
        payload[0] = 0;
        for (std::size_t index = 0;
            index < kKeyFunctionRecordsPerFrame; ++index)
        {
            const std::size_t requestOffset = 5u + index * 4u;
            const std::size_t responseOffset = 1u + index * 4u;
            std::uint8_t key = request[requestOffset];
            std::uint8_t layout = request[requestOffset + 1u];
            std::uint16_t value = activeFunctions_[key];
            if (currentFault_ == Fault::WrongActiveKey && index == 1u)
                key ^= 0x01u;
            if (currentFault_ == Fault::WrongActiveLayout && index == 1u)
                layout = static_cast<std::uint8_t>(layout + 1u);
            if (currentFault_ == Fault::InconsistentPadding &&
                index + 1u == kKeyFunctionRecordsPerFrame &&
                request[requestOffset] == request[requestOffset - 4u])
            {
                value ^= 0x0001u;
            }
            if (currentFault_ == Fault::ActiveMapGenerationMismatch &&
                index == 0u)
            {
                // Return a structurally valid second-generation value.  Every
                // individual 0x23 response remains valid, but the complete map
                // no longer matches the first five-batch generation.
                value = value == 0x0052u ? 0x0029u : 0x0052u;
            }
            payload[responseOffset] = key;
            payload[responseOffset + 1u] = layout;
            payload[responseOffset + 2u] =
                static_cast<std::uint8_t>(value & 0xFFu);
            payload[responseOffset + 3u] =
                static_cast<std::uint8_t>(value >> 8u);
        }
        return payload;
    }

    std::vector<std::uint8_t> TravelPayload(
        std::uint8_t requestedHalf) const
    {
        std::vector<std::uint8_t> payload(kTravelPayloadBytes, 0);
        if (currentFault_ == Fault::OldStage8TravelHeader)
        {
            payload[0] = kSelectorTravel;
            payload[1] = requestedHalf;
        }
        else
        {
            payload[0] = 0;
            payload[1] = kSelectorTravel;
        }
        std::uint8_t sourceHalf = requestedHalf;
        if (returnSwappedHalves_)
            sourceHalf = requestedHalf == 1u ? 2u : 1u;
        const std::size_t firstRow = sourceHalf == 1u
            ? 0u
            : kRowsPerTravelHalf;
        for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
        {
            for (std::size_t column = 0; column < kColumns; ++column)
            {
                std::uint16_t value = travel_[firstRow + row][column];
                if (currentFault_ == Fault::ImplausibleTravel &&
                    row == 1u && column == 0u)
                {
                    value = 12000u;
                }
                const std::size_t offset =
                    2u + (row * kColumns + column) * 2u;
                payload[offset] = static_cast<std::uint8_t>(value & 0xFFu);
                payload[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
            }
        }
        return payload;
    }

    std::vector<std::uint8_t> StatusPayload() const
    {
        std::vector<std::uint8_t> payload(kStatusPayloadBytes, 0);
        payload[0] = 0;
        payload[1] = kSelectorStatus;
        for (std::size_t row = 0; row < kRows; ++row)
        {
            for (std::size_t column = 0; column < kColumns; ++column)
            {
                payload[2u + row * kColumns + column] =
                    status_[row][column];
            }
        }
        payload[kStatusPayloadBytes - 2u] = 0xFFu;
        payload[kStatusPayloadBytes - 1u] = 0xFFu;
        if (currentFault_ == Fault::BadStatusTrailer)
            payload[kStatusPayloadBytes - 1u] = 0;
        return payload;
    }

    void QueueResponse(
        std::uint8_t requestCommand,
        const std::vector<std::uint8_t>& payload)
    {
        const std::size_t reportCount = ResponseReportCount(payload.size());
        assert(reportCount >= 1u && reportCount <= kMaximumResponseReports);
        std::vector<std::uint8_t> stream(
            reportCount * kWireReportBytes, 0);
        stream[0] = kFrameHead;
        stream[1] = static_cast<std::uint8_t>(payload.size());
        stream[2] = ResponseCommand(requestCommand);
        stream[3] = ComputeChecksum(
            stream[1], stream[2], payload.data());
        std::copy(payload.begin(), payload.end(), stream.begin() + 4u);
        if (currentFault_ == Fault::CorruptChecksum)
            stream[3] ^= 0x01u;
        if (currentFault_ == Fault::WrongResponseCommand)
            stream[2] ^= 0x20u;

        std::vector<Report> reports(reportCount);
        for (std::size_t report = 0; report < reportCount; ++report)
        {
            std::copy_n(
                stream.data() + report * kWireReportBytes,
                kWireReportBytes,
                reports[report].begin());
        }
        if (currentFault_ == Fault::DropLastReport && !reports.empty())
            reports.pop_back();
        for (const auto& report : reports)
            reads_.push_back(report);
    }

    KeyMap defaultMap_{};
    std::array<std::uint16_t, 256> activeFunctions_{};
    TravelMatrix travel_{};
    StatusMatrix status_{};
    std::deque<Report> reads_;
    std::vector<Report> writes_;
    std::uint64_t nowMs_ = 1000u;
    std::size_t transactionCount_ = 0;
    std::size_t flushCount_ = 0;
    std::size_t writeCount_ = 0;
    std::size_t flushedReports_ = 0;
    std::size_t readsThisTransaction_ = 0;
    std::size_t faultTransaction_ = 0;
    Fault armedFault_ = Fault::None;
    Fault currentFault_ = Fault::None;
    bool returnSwappedHalves_ = false;
};

struct TraceCapture
{
    std::size_t transmit = 0;
    std::size_t receive = 0;
    std::size_t error = 0;

    static void Callback(
        void* context,
        TraceKind kind,
        const char*,
        const Report*) noexcept
    {
        auto* self = static_cast<TraceCapture*>(context);
        if (!self)
            return;
        if (kind == TraceKind::Transmit)
            ++self->transmit;
        else if (kind == TraceKind::Receive)
            ++self->receive;
        else
            ++self->error;
    }
};

void AssertPoisonedAfterFailure(
    Fault fault,
    std::size_t transaction,
    FailureStage expectedStage)
{
    FirmwareShapedTransport transport;
    transport.ArmFault(fault, transaction);
    Client client(transport);
    CapabilityProof proof{};
    Failure failure{};
    assert(!client.Probe(&proof, &failure));
    assert(failure.stage == expectedStage);
    assert(!client.IsUsable());

    const std::size_t writesBefore = transport.WriteCount();
    const std::size_t flushesBefore = transport.FlushCount();
    failure = Failure{};
    assert(!client.Probe(&proof, &failure));
    assert(failure.stage == FailureStage::SessionPoisoned);
    assert(transport.WriteCount() == writesBefore);
    assert(transport.FlushCount() == flushesBefore);
}

void TestCleanProbePollStatusAndRelease()
{
    FirmwareShapedTransport transport;
    transport.SetTravel(1u, 0u, 1700u); // physical Esc
    transport.SetTravel(2u, 2u, 850u);  // physical W
    transport.SetTravel(4u, 6u, 3400u); // physical Space
    transport.SetStatus(1u, 0u, 1u);

    TraceCapture trace{};
    Client client(transport, TraceSink{&TraceCapture::Callback, &trace});
    CapabilityProof proof{};
    Failure failure{};
    assert(client.Probe(&proof, &failure));
    assert(failure.stage == FailureStage::None);
    assert(client.IsUsable());
    assert(transport.TransactionCount() == kCapabilityTransactions);
    assert(transport.FlushCount() == kCapabilityTransactions);
    assert(transport.WriteCount() == kCapabilityTransactions);
    assert(proof.physicalKeyPositions == kExpectedPhysicalKeyPositions);
    assert(proof.defaultMappedKeys == kExpectedPublishableDefaultKeys);
    assert(proof.mappedKeys == kExpectedPublishableDefaultKeys);
    assert(proof.defaultKeyMap == ExpectedAulaWin60HeMaxDefaultMap());
    assert(proof.keyMap[5][12] == 0u);

    TravelMatrix travel{};
    assert(client.ReadTravelMatrix(proof, &travel, &failure));
    assert(travel == transport.Travel());
    SnapshotResult snapshot{};
    BuildHidMilliSnapshot(
        proof.keyMap, travel, proof.precision.maximumTravelUm, &snapshot);
    assert(snapshot.milli[0x29u] == 500u);
    assert(snapshot.milli[0x1Au] == 250u);
    assert(snapshot.milli[0x05u] == 1000u);
    assert(snapshot.activeKeys == 3u);

    StatusMatrix status{};
    assert(client.QueryStatus(&status, &failure));
    assert(status[1][0] == 1u);

    TravelMatrix released{};
    transport.SetTravelMatrix(released);
    assert(client.ReadTravelMatrix(proof, &travel, &failure));
    BuildHidMilliSnapshot(
        proof.keyMap, travel, proof.precision.maximumTravelUm, &snapshot);
    assert(snapshot.activeKeys == 0u);
    assert(std::all_of(snapshot.milli.begin(), snapshot.milli.end(),
        [](std::uint16_t value) { return value == 0u; }));
    assert(trace.transmit == transport.WriteCount());
    assert(trace.receive > trace.transmit);
    assert(trace.error == 0u);
}

void TestEveryTransactionFlushesAndStaleInputIsDiscarded()
{
    FirmwareShapedTransport transport;
    Report stale{};
    stale[0] = kFrameHead;
    stale[1] = 0;
    stale[2] = ResponseCommand(kCommandDefaultKeys);
    stale[3] = ComputeChecksum(0, stale[2], nullptr);
    transport.Preload(stale);

    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));
    assert(transport.FlushedReports() == 1u);
    assert(transport.FlushCount() == transport.WriteCount());
    assert(transport.FlushCount() == kCapabilityTransactions);
}

void TestTransportAndProofFailuresPoisonSession()
{
    struct Case
    {
        Fault fault;
        std::size_t transaction;
        FailureStage stage;
    };
    const std::array<Case, 16> cases{{
        {Fault::FlushFailure, 1u, FailureStage::FlushInput},
        {Fault::WriteFailure, 1u, FailureStage::Write},
        {Fault::TimeoutFirstRead, 1u, FailureStage::ReadFirst},
        {Fault::WrongResponseCommand, 1u, FailureStage::ReadFirst},
        {Fault::CorruptChecksum, 1u, FailureStage::ParseFrame},
        {Fault::WrongFirmware, 1u, FailureStage::UnexpectedFirmware},
        {Fault::WrongBuildDate, 1u, FailureStage::UnexpectedFirmware},
        {Fault::WrongPrecision, 2u, FailureStage::UnexpectedPrecision},
        {Fault::MutateDefaultMap, 3u, FailureStage::UnexpectedDefaultMap},
        {Fault::WrongActiveKey, 6u, FailureStage::DecodeActiveMap},
        {Fault::WrongActiveLayout, 6u, FailureStage::DecodeActiveMap},
        {Fault::InconsistentPadding, 10u, FailureStage::DecodeActiveMap},
        {Fault::ActiveMapGenerationMismatch, 11u, FailureStage::UnstableActiveMap},
        {Fault::OldStage8TravelHeader, 16u, FailureStage::ReadFirst},
        {Fault::ImplausibleTravel, 16u, FailureStage::ImplausibleTravel},
        {Fault::DropLastReport, 16u, FailureStage::ReadContinuation},
    }};
    for (const auto& test : cases)
        AssertPoisonedAfterFailure(test.fault, test.transaction, test.stage);
}

void TestTornActiveMapGenerationIsRejected()
{
    FirmwareShapedTransport transport;
    // Transactions 6..10 form generation one.  Transaction 11 is the first
    // batch of generation two and returns a valid remap for one physical key.
    // A one-generation implementation would silently publish whichever mix it
    // happened to collect; the hardened client must reject the mismatch.
    transport.ArmFault(Fault::ActiveMapGenerationMismatch, 11u);
    Client client(transport);
    CapabilityProof proof{};
    proof.mappedKeys = 999u;
    Failure failure{};
    assert(!client.Probe(&proof, &failure));
    assert(failure.stage == FailureStage::UnstableActiveMap);
    assert(!client.IsUsable());
    assert(proof.mappedKeys == 0u);
    assert(transport.TransactionCount() == 15u);
    assert(transport.FlushCount() == 15u);
    assert(transport.WriteCount() == 15u);
}

void TestStatusFailurePoisonsSession()
{
    FirmwareShapedTransport transport;
    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));
    transport.ArmFault(Fault::BadStatusTrailer);
    StatusMatrix status{};
    Failure failure{};
    assert(!client.QueryStatus(&status, &failure));
    assert(failure.stage == FailureStage::DecodeStatus);
    assert(!client.IsUsable());
}

void TestRuntimeActiveMapRefresh()
{
    FirmwareShapedTransport transport;
    transport.SetTravel(1u, 0u, 1700u);
    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));
    assert(proof.keyMap[1][0] == 0x29u);

    transport.SetActiveFunction(0x29u, 0x0052u);
    ActiveMapSnapshot refreshed{};
    Failure failure{};
    assert(client.ReadActiveMap(
        proof.defaultKeyMap, &refreshed, &failure));
    assert(refreshed.keyMap[1][0] == 0x52u);
    proof.activeFunctions = refreshed.functions;
    proof.keyMap = refreshed.keyMap;
    proof.mappedKeys = refreshed.mappedKeys;

    TravelMatrix travel{};
    assert(client.ReadTravelMatrix(proof, &travel, &failure));
    SnapshotResult snapshot{};
    BuildHidMilliSnapshot(
        proof.keyMap, travel, proof.precision.maximumTravelUm, &snapshot);
    assert(snapshot.milli[0x29u] == 0u);
    assert(snapshot.milli[0x52u] == 500u);

    transport.ArmFault(Fault::WrongActiveKey);
    assert(!client.ReadActiveMap(
        proof.defaultKeyMap, &refreshed, &failure));
    assert(failure.stage == FailureStage::DecodeActiveMap);
    assert(!client.IsUsable());
}

void TestDisabledAndInternalActiveFunctionsAreNotPublished()
{
    FirmwareShapedTransport transport;
    transport.SetActiveFunction(0x04u, 0x0052u); // physical A -> Up Arrow
    transport.SetActiveFunction(0x16u, 0xF001u); // physical S -> internal function
    transport.SetActiveFunction(0x07u, 0x0000u); // physical D -> disabled
    transport.SetTravel(3u, 1u, 1700u);
    transport.SetTravel(3u, 2u, 3400u);
    transport.SetTravel(3u, 3u, 2550u);

    Client client(transport);
    CapabilityProof proof{};
    Failure failure{};
    assert(client.Probe(&proof, &failure));
    assert(proof.keyMap[3][1] == 0x52u);
    assert(proof.keyMap[3][2] == 0u);
    assert(proof.keyMap[3][3] == 0u);

    TravelMatrix travel{};
    assert(client.ReadTravelMatrix(proof, &travel, &failure));
    SnapshotResult snapshot{};
    BuildHidMilliSnapshot(
        proof.keyMap, travel, proof.precision.maximumTravelUm, &snapshot);
    assert(snapshot.milli[0x52u] == 500u);
    assert(snapshot.milli[0x04u] == 0u);
    assert(snapshot.milli[0x16u] == 0u);
    assert(snapshot.milli[0x07u] == 0u);
    assert(snapshot.activeKeys == 1u);
}

void TestProtocolCannotAuthenticateHalfIdentity()
{
    FirmwareShapedTransport transport;
    for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            transport.SetTravel(row, column,
                static_cast<std::uint16_t>(100u + row * kColumns + column));
            transport.SetTravel(row + kRowsPerTravelHalf, column,
                static_cast<std::uint16_t>(1000u + row * kColumns + column));
        }
    }
    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));

    transport.SetReturnSwappedHalves(true);
    TravelMatrix decoded{};
    Failure failure{};
    assert(client.ReadTravelMatrix(proof, &decoded, &failure));
    assert(client.IsUsable());
    assert(decoded[0][0] == transport.Travel()[3][0]);
    assert(decoded[3][0] == transport.Travel()[0][0]);
    // The response contains no half echo or transaction ID. This accepted
    // counterexample is why exclusive ownership, flush-before-write and
    // destroying a session after any uncertainty are mandatory invariants.
}

void TestDuplicateActiveUsageUsesMaximumPhysicalTravel()
{
    FirmwareShapedTransport transport;
    transport.SetActiveFunction(0x29u, 0x0004u);
    transport.SetActiveFunction(0x04u, 0x0004u);
    transport.SetTravel(1u, 0u, 850u);
    transport.SetTravel(3u, 1u, 2550u);

    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));
    TravelMatrix travel{};
    assert(client.ReadTravelMatrix(proof, &travel));
    SnapshotResult snapshot{};
    BuildHidMilliSnapshot(
        proof.keyMap, travel, proof.precision.maximumTravelUm, &snapshot);
    assert(snapshot.milli[0x04u] == 750u);
    assert(snapshot.activeKeys == 1u);
}

void TestRandomFirmwareShapedMatricesRoundTrip()
{
    FirmwareShapedTransport transport;
    Client client(transport);
    CapabilityProof proof{};
    assert(client.Probe(&proof));
    std::mt19937 random(0xA61A2026u);
    std::uniform_int_distribution<unsigned int> value(0u, 3400u);
    for (std::size_t iteration = 0; iteration < 300u; ++iteration)
    {
        TravelMatrix expected{};
        for (auto& row : expected)
            for (auto& cell : row)
                cell = static_cast<std::uint16_t>(value(random));
        transport.SetTravelMatrix(expected);
        TravelMatrix decoded{};
        assert(client.ReadTravelMatrix(proof, &decoded));
        assert(decoded == expected);
    }
}
}

int main()
{
    TestCleanProbePollStatusAndRelease();
    TestEveryTransactionFlushesAndStaleInputIsDiscarded();
    TestTransportAndProofFailuresPoisonSession();
    TestTornActiveMapGenerationIsRejected();
    TestStatusFailurePoisonsSession();
    TestRuntimeActiveMapRefresh();
    TestDisabledAndInternalActiveFunctionsAreNotPublished();
    TestProtocolCannotAuthenticateHalfIdentity();
    TestDuplicateActiveUsageUsesMaximumPhysicalTravel();
    TestRandomFirmwareShapedMatricesRoundTrip();
    std::cout << "AULA_WIN60HE_END_TO_END_TEST=PASS\n";
    return 0;
}
