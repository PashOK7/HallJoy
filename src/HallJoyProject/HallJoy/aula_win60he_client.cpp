#include "aula_win60he_client.h"

#include <algorithm>
#include <array>

namespace aula_win60he
{
namespace
{
void ClearFailure(Failure* failure) noexcept
{
    if (failure)
        *failure = Failure{};
}

std::uint32_t RemainingMilliseconds(
    std::uint64_t now,
    std::uint64_t deadline) noexcept
{
    if (now >= deadline)
        return 0;
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(deadline - now, 0xffffffffull));
}

bool FirstReportMatches(
    const Report& block,
    std::uint8_t requestCommand,
    std::size_t expectedReports,
    std::uint8_t selector,
    std::uint8_t index) noexcept
{
    if (block[0] != kFrameHead ||
        block[2] != ResponseCommand(requestCommand) ||
        ResponseReportCount(block[1]) != expectedReports)
        return false;

    switch (requestCommand)
    {
    case kCommandSync:
        return block[1] == kSyncPayloadBytes && block[4] == 0;
    case kCommandApi:
        return block[1] == 7u && block[4] == 0 && block[5] == selector;
    case kCommandDefaultKeys:
        return block[1] == 45u && block[4] == 0 &&
            block[5] == index &&
            block[27] == static_cast<std::uint8_t>(index + 1u);
    case kCommandKeyFunctions:
        return block[1] == kKeyFunctionPayloadBytes && block[4] == 0 &&
            block[6] == selector;
    case kCommandMatrix6x21:
        if (selector == kSelectorTravel)
            return block[1] == kTravelPayloadBytes &&
                block[4] == 0 && block[5] == kSelectorTravel;
        if (selector == kSelectorStatus)
            return block[1] == kStatusPayloadBytes &&
                block[4] == 0 && block[5] == kSelectorStatus;
        return false;
    default:
        return false;
    }
}

std::array<std::uint8_t, kExpectedPhysicalKeyPositions> CollectFactoryKeys(
    const KeyMap& map) noexcept
{
    std::array<std::uint8_t, kExpectedPhysicalKeyPositions> keys{};
    std::size_t index = 0;
    for (const auto& row : map)
    {
        for (const auto key : row)
        {
            if (key != 0 && index < keys.size())
                keys[index++] = key;
        }
    }
    return keys;
}
}

const char* FailureStageName(FailureStage stage) noexcept
{
    switch (stage)
    {
    case FailureStage::None: return "none";
    case FailureStage::SessionPoisoned: return "session-poisoned";
    case FailureStage::FlushInput: return "flush-input";
    case FailureStage::Write: return "write";
    case FailureStage::ReadFirst: return "read-first";
    case FailureStage::ReadContinuation: return "read-continuation";
    case FailureStage::ParseFrame: return "parse-frame";
    case FailureStage::DecodeSync: return "decode-sync";
    case FailureStage::UnexpectedFirmware: return "unexpected-firmware";
    case FailureStage::DecodePrecision: return "decode-precision";
    case FailureStage::UnexpectedPrecision: return "unexpected-precision";
    case FailureStage::DecodeDefaultMap: return "decode-default-map";
    case FailureStage::UnexpectedDefaultMap: return "unexpected-default-map";
    case FailureStage::DecodeActiveMap: return "decode-active-map";
    case FailureStage::UnstableActiveMap: return "unstable-active-map";
    case FailureStage::DecodeTravel: return "decode-travel";
    case FailureStage::DecodeStatus: return "decode-status";
    case FailureStage::ImplausibleTravel: return "implausible-travel";
    }
    return "unknown";
}

Client::Client(ReportTransport& transport, TraceSink trace) noexcept
    : transport_(transport), trace_(trace)
{
}

void Client::Trace(TraceKind kind, const char* label, const Report* report) const noexcept
{
    if (trace_.callback)
        trace_.callback(trace_.context, kind, label ? label : "", report);
}

void Client::Fail(
    Failure* failure,
    FailureStage stage,
    std::uint8_t command,
    std::uint8_t selector,
    std::uint8_t index,
    const char* traceLabel) noexcept
{
    poisoned_ = true;
    if (failure)
    {
        failure->stage = stage;
        failure->command = command;
        failure->selector = selector;
        failure->index = index;
    }
    Trace(TraceKind::Error, traceLabel, nullptr);
}

bool Client::ReadResponse(
    const char* label,
    std::uint8_t requestCommand,
    std::size_t expectedReports,
    std::uint32_t timeoutMs,
    ResponseStream* outStream,
    std::size_t* outBytes,
    Failure* failure,
    std::uint8_t selector,
    std::uint8_t index)
{
    if (outStream) outStream->fill(0);
    if (outBytes) *outBytes = 0;
    if (!outStream || !outBytes || expectedReports == 0 ||
        expectedReports > kMaximumResponseReports)
    {
        Fail(failure, FailureStage::ParseFrame, requestCommand,
            selector, index, "error:invalid-read-contract");
        return false;
    }

    const std::uint64_t deadline = transport_.NowMilliseconds() + timeoutMs;
    Report block{};
    const std::uint32_t firstRemaining = RemainingMilliseconds(
        transport_.NowMilliseconds(), deadline);
    if (firstRemaining == 0 || !transport_.ReadReport(firstRemaining, &block))
    {
        Fail(failure, FailureStage::ReadFirst, requestCommand,
            selector, index, "error:read-first");
        return false;
    }
    Trace(TraceKind::Receive, label, &block);
    if (!FirstReportMatches(block, requestCommand, expectedReports, selector, index))
    {
        Fail(failure, FailureStage::ReadFirst, requestCommand,
            selector, index, "error:unexpected-first-report");
        return false;
    }
    std::copy(block.begin(), block.end(), outStream->begin());

    for (std::size_t report = 1; report < expectedReports; ++report)
    {
        const std::uint32_t remaining = RemainingMilliseconds(
            transport_.NowMilliseconds(), deadline);
        if (remaining == 0 || !transport_.ReadReport(remaining, &block))
        {
            Fail(failure, FailureStage::ReadContinuation, requestCommand,
                selector, static_cast<std::uint8_t>(report),
                "error:read-continuation");
            return false;
        }
        Trace(TraceKind::Receive, label, &block);
        std::copy(block.begin(), block.end(),
            outStream->begin() + report * kWireReportBytes);
    }

    *outBytes = expectedReports * kWireReportBytes;
    FrameView frame{};
    if (!ParseResponseFrame(
        outStream->data(), *outBytes, requestCommand, &frame) ||
        ResponseReportCount(frame.payloadBytes) != expectedReports)
    {
        Fail(failure, FailureStage::ParseFrame, requestCommand,
            selector, index, "error:response-frame");
        return false;
    }
    return true;
}

bool Client::Transact(
    const char* label,
    const Report& request,
    std::uint8_t requestCommand,
    std::size_t expectedReports,
    std::uint32_t timeoutMs,
    ResponseStream* outStream,
    std::size_t* outBytes,
    Failure* failure,
    std::uint8_t selector,
    std::uint8_t index)
{
    if (poisoned_)
    {
        if (failure)
        {
            failure->stage = FailureStage::SessionPoisoned;
            failure->command = requestCommand;
            failure->selector = selector;
            failure->index = index;
        }
        Trace(TraceKind::Error, "error:session-poisoned", nullptr);
        return false;
    }

    // Selector-2 replies contain neither a half number nor a transaction ID.
    // Every request therefore starts from a successfully flushed exclusive
    // collection queue; any failure permanently invalidates this Client/session.
    if (!transport_.FlushInput())
    {
        Fail(failure, FailureStage::FlushInput, requestCommand,
            selector, index, "error:flush-input");
        return false;
    }

    Trace(TraceKind::Transmit, label, &request);
    if (!transport_.WriteReport(request))
    {
        Fail(failure, FailureStage::Write, requestCommand,
            selector, index, "error:write");
        return false;
    }
    return ReadResponse(label, requestCommand, expectedReports, timeoutMs,
        outStream, outBytes, failure, selector, index);
}

bool Client::Probe(CapabilityProof* out, Failure* failure)
{
    if (out) *out = CapabilityProof{};
    ClearFailure(failure);
    if (!out || poisoned_)
    {
        if (poisoned_)
            Fail(failure, FailureStage::SessionPoisoned, 0, 0, 0,
                "error:probe-on-poisoned-session");
        return false;
    }

    CapabilityProof proof{};
    ResponseStream stream{};
    std::size_t bytes = 0;
    FrameView frame{};

    if (!Transact("sync", BuildSyncRequest(), kCommandSync, 1,
            kSingleResponseTimeoutMs, &stream, &bytes, failure, 0, 0) ||
        !ParseResponseFrame(stream.data(), bytes, kCommandSync, &frame) ||
        !DecodeSyncInfo(frame, &proof.sync))
    {
        if (!failure || failure->stage == FailureStage::None)
            Fail(failure, FailureStage::DecodeSync, kCommandSync,
                0, 0, "error:decode-sync");
        return false;
    }
    if (!IsExpectedAulaWin60HeMaxFirmware(proof.sync))
    {
        Fail(failure, FailureStage::UnexpectedFirmware, kCommandSync,
            0, 0, "error:unexpected-firmware");
        return false;
    }

    if (!Transact("precision", BuildPrecisionStrokeRequest(), kCommandApi, 1,
            kSingleResponseTimeoutMs, &stream, &bytes, failure,
            kOrderPrecisionStroke, 0) ||
        !ParseResponseFrame(stream.data(), bytes, kCommandApi, &frame) ||
        !DecodePrecisionStroke(frame, &proof.precision))
    {
        if (!failure || failure->stage == FailureStage::None)
            Fail(failure, FailureStage::DecodePrecision, kCommandApi,
                kOrderPrecisionStroke, 0, "error:decode-precision");
        return false;
    }
    if (!IsExpectedAulaWin60HeMaxPrecision(proof.precision))
    {
        Fail(failure, FailureStage::UnexpectedPrecision, kCommandApi,
            kOrderPrecisionStroke, 0, "error:unexpected-precision");
        return false;
    }

    for (std::uint8_t row = 0; row < kRows; row += 2u)
    {
        const auto secondRow = static_cast<std::uint8_t>(row + 1u);
        if (!Transact("default-map", BuildDefaultKeyRequest(row, secondRow),
                kCommandDefaultKeys, 1, kSingleResponseTimeoutMs,
                &stream, &bytes, failure, 0, row) ||
            !ParseResponseFrame(stream.data(), bytes,
                kCommandDefaultKeys, &frame) ||
            !DecodeDefaultKeyRows(frame, row, secondRow, &proof.defaultKeyMap))
        {
            if (!failure || failure->stage == FailureStage::None)
                Fail(failure, FailureStage::DecodeDefaultMap,
                    kCommandDefaultKeys, 0, row, "error:decode-default-map");
            return false;
        }
    }

    proof.physicalKeyPositions = CountPhysicalKeyPositions(proof.defaultKeyMap);
    proof.defaultMappedKeys = CountMappedHids(proof.defaultKeyMap);
    if (!IsExpectedAulaWin60HeMaxDefaultMap(proof.defaultKeyMap) ||
        proof.physicalKeyPositions != kExpectedPhysicalKeyPositions ||
        proof.defaultMappedKeys != kExpectedPublishableDefaultKeys)
    {
        Fail(failure, FailureStage::UnexpectedDefaultMap,
            kCommandDefaultKeys, 0, 0, "error:unexpected-default-map");
        return false;
    }

    ActiveMapSnapshot activeMap{};
    if (!ReadActiveMap(proof.defaultKeyMap, &activeMap, failure))
        return false;
    proof.activeFunctions = activeMap.functions;
    proof.keyMap = activeMap.keyMap;
    proof.mappedKeys = activeMap.mappedKeys;

    TravelMatrix initial{};
    if (!ReadTravelMatrix(proof, &initial, failure))
        return false;

    *out = proof;
    return true;
}

bool Client::ReadActiveMap(
    const KeyMap& defaultKeyMap,
    ActiveMapSnapshot* out,
    Failure* failure)
{
    if (out) *out = ActiveMapSnapshot{};
    ClearFailure(failure);
    if (!out || poisoned_)
    {
        if (poisoned_)
            Fail(failure, FailureStage::SessionPoisoned,
                kCommandKeyFunctions, kKeyLayoutFn0, 0,
                "error:active-map-on-poisoned-session");
        return false;
    }

    // A complete Fn0 map requires five independent command/response pairs and
    // the firmware exposes neither a map generation nor an atomic full-map
    // command. Read two complete generations and publish only an exact match.
    // This rejects ordinary profile/remap transitions; the wire protocol still
    // cannot rule out a perfectly repeated ABA/oscillating torn pattern.
    ActiveMapSnapshot first{};
    ActiveMapSnapshot second{};
    if (!ReadActiveMapGeneration(defaultKeyMap, &first, failure) ||
        !ReadActiveMapGeneration(defaultKeyMap, &second, failure))
        return false;
    if (first.functions != second.functions ||
        first.keyMap != second.keyMap ||
        first.mappedKeys != second.mappedKeys)
    {
        Fail(failure, FailureStage::UnstableActiveMap,
            kCommandKeyFunctions, kKeyLayoutFn0, 0,
            "error:unstable-active-map");
        return false;
    }

    *out = second;
    return true;
}

bool Client::ReadActiveMapGeneration(
    const KeyMap& defaultKeyMap,
    ActiveMapSnapshot* out,
    Failure* failure)
{
    if (out) *out = ActiveMapSnapshot{};
    if (!out || poisoned_)
    {
        if (poisoned_)
            Fail(failure, FailureStage::SessionPoisoned,
                kCommandKeyFunctions, kKeyLayoutFn0, 0,
                "error:active-map-generation-on-poisoned-session");
        return false;
    }

    ActiveMapSnapshot snapshot{};
    const auto factoryKeys = CollectFactoryKeys(defaultKeyMap);
    if (factoryKeys.back() == 0)
    {
        Fail(failure, FailureStage::DecodeActiveMap,
            kCommandKeyFunctions, kKeyLayoutFn0, 0,
            "error:active-map-physical-key-count");
        return false;
    }

    for (std::size_t begin = 0; begin < factoryKeys.size();
        begin += kKeyFunctionRecordsPerFrame)
    {
        KeyQuery query{};
        const std::size_t remaining = factoryKeys.size() - begin;
        const std::size_t count = std::min(remaining, query.size());
        if (count == 0u)
        {
            Fail(failure, FailureStage::DecodeActiveMap,
                kCommandKeyFunctions, kKeyLayoutFn0, 0,
                "error:active-map-empty-batch");
            return false;
        }
        for (std::size_t index = 0; index < count; ++index)
            query[index] = factoryKeys[begin + index];
        const std::uint8_t paddingKey = query[count - 1u];
        for (std::size_t index = count; index < query.size(); ++index)
            query[index] = paddingKey;

        ResponseStream stream{};
        std::size_t bytes = 0;
        FrameView frame{};
        KeyFunctionBatch batch{};
        const auto batchIndex = static_cast<std::uint8_t>(
            begin / kKeyFunctionRecordsPerFrame);
        if (!Transact("active-fn0-map",
                BuildKeyFunctionReadRequest(query, kKeyLayoutFn0),
                kCommandKeyFunctions, 1, kSingleResponseTimeoutMs,
                &stream, &bytes, failure, kKeyLayoutFn0, batchIndex) ||
            !ParseResponseFrame(stream.data(), bytes,
                kCommandKeyFunctions, &frame) ||
            !DecodeKeyFunctionReadResponse(
                frame, query, kKeyLayoutFn0, &batch) ||
            !ApplyKeyFunctionBatch(
                defaultKeyMap, batch, &snapshot.functions))
        {
            if (!failure || failure->stage == FailureStage::None)
                Fail(failure, FailureStage::DecodeActiveMap,
                    kCommandKeyFunctions, kKeyLayoutFn0, batchIndex,
                    "error:decode-active-map");
            return false;
        }
    }

    BuildPublishableActiveKeyMap(
        defaultKeyMap, snapshot.functions, &snapshot.keyMap);
    snapshot.mappedKeys = CountMappedHids(snapshot.keyMap);
    *out = snapshot;
    return true;
}

bool Client::ReadTravelMatrix(
    const CapabilityProof& proof,
    TravelMatrix* out,
    Failure* failure)
{
    if (out) out->fill({});
    ClearFailure(failure);
    if (!out || poisoned_)
    {
        if (poisoned_)
            Fail(failure, FailureStage::SessionPoisoned,
                kCommandMatrix6x21, kSelectorTravel, 0,
                "error:travel-on-poisoned-session");
        return false;
    }

    TravelHalf halves[2]{};
    for (std::uint8_t half = 1; half <= 2; ++half)
    {
        ResponseStream stream{};
        std::size_t bytes = 0;
        FrameView frame{};
        if (!Transact(half == 1 ? "travel-1" : "travel-2",
                BuildTravelRequest(half), kCommandMatrix6x21,
                kMaximumResponseReports, kMatrixResponseTimeoutMs,
                &stream, &bytes, failure, kSelectorTravel, half) ||
            !ParseResponseFrame(stream.data(), bytes,
                kCommandMatrix6x21, &frame) ||
            !DecodeTravelHalf(frame, &halves[half - 1u]))
        {
            if (!failure || failure->stage == FailureStage::None)
                Fail(failure, FailureStage::DecodeTravel,
                    kCommandMatrix6x21, kSelectorTravel, half,
                    "error:decode-travel");
            return false;
        }

        const std::uint8_t firstMatrixRow = half == 1 ? 0u : 3u;
        if (!TravelValuesPlausible(
            halves[half - 1u], proof.defaultKeyMap,
            firstMatrixRow, proof.precision))
        {
            Fail(failure, FailureStage::ImplausibleTravel,
                kCommandMatrix6x21, kSelectorTravel, half,
                "error:implausible-travel");
            return false;
        }
    }

    MergeTravelHalves(halves[0], halves[1], out);
    return true;
}

bool Client::QueryStatus(StatusMatrix* out, Failure* failure)
{
    if (out) out->fill({});
    ClearFailure(failure);
    if (!out || poisoned_)
    {
        if (poisoned_)
            Fail(failure, FailureStage::SessionPoisoned,
                kCommandMatrix6x21, kSelectorStatus, 0,
                "error:status-on-poisoned-session");
        return false;
    }

    ResponseStream stream{};
    std::size_t bytes = 0;
    FrameView frame{};
    if (!Transact("status", BuildStatusRequest(), kCommandMatrix6x21,
            kMaximumResponseReports, kMatrixResponseTimeoutMs,
            &stream, &bytes, failure, kSelectorStatus, 1) ||
        !ParseResponseFrame(stream.data(), bytes,
            kCommandMatrix6x21, &frame) ||
        !DecodeStatusMatrix(frame, out))
    {
        if (!failure || failure->stage == FailureStage::None)
            Fail(failure, FailureStage::DecodeStatus,
                kCommandMatrix6x21, kSelectorStatus, 1,
                "error:decode-status");
        return false;
    }
    return true;
}

bool TravelValuesPlausible(
    const TravelHalf& values,
    const KeyMap& physicalMap,
    std::uint8_t firstMatrixRow,
    const PrecisionStroke& precision) noexcept
{
    if (!IsExpectedAulaWin60HeMaxPrecision(precision) ||
        firstMatrixRow + kRowsPerTravelHalf > kRows)
        return false;

    const std::uint32_t toleratedMaximum =
        static_cast<std::uint32_t>(precision.maximumTravelUm) +
        std::max<std::uint32_t>(precision.precisionUm * 8u, 200u);
    for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            if (physicalMap[firstMatrixRow + row][column] == 0)
                continue;
            if (values[row][column] > toleratedMaximum)
                return false;
        }
    }
    return true;
}

void BuildHidMilliSnapshot(
    const KeyMap& activeMap,
    const TravelMatrix& travel,
    std::uint16_t maximumTravelUm,
    SnapshotResult* out) noexcept
{
    if (out) *out = SnapshotResult{};
    if (!out || maximumTravelUm == 0)
        return;

    for (std::size_t row = 0; row < kRows; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            const std::uint8_t hid = activeMap[row][column];
            if (!IsPublishableKeyboardUsage(hid))
                continue;
            const std::uint16_t milli = NormalizeTravelToMilli(
                travel[row][column], maximumTravelUm);
            out->milli[hid] = std::max(out->milli[hid], milli);
        }
    }

    for (const auto milli : out->milli)
        if (milli != 0)
            ++out->activeKeys;
}
}
