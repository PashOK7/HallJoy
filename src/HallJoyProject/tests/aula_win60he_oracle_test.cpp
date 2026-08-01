#include "../HallJoy/aula_win60he_protocol.h"
#include "aula_win60he_oracle_fixtures.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
using namespace aula_win60he;
using namespace aula_win60he_oracle;

template <std::size_t Size>
void ExpectPrefix(
    const Report& actual,
    const std::array<std::uint8_t, Size>& expected)
{
    assert(std::equal(expected.begin(), expected.end(), actual.begin()));
    assert(std::all_of(actual.begin() + Size, actual.end(),
        [](std::uint8_t value) { return value == 0u; }));
}

template <std::size_t Size>
FrameView Parse(
    const std::array<std::uint8_t, Size>& bytes,
    std::uint8_t requestCommand)
{
    FrameView frame{};
    assert(ParseResponseFrame(
        bytes.data(), bytes.size(), requestCommand, &frame));
    return frame;
}

void TestImmutableRequestVectors()
{
    ExpectPrefix(BuildSyncRequest(), kRequestSync);
    ExpectPrefix(BuildPrecisionStrokeRequest(), kRequestPrecision);
    ExpectPrefix(BuildDefaultKeyRequest(0, 1), kRequestDefRows01);
    ExpectPrefix(BuildDefaultKeyRequest(2, 3), kRequestDefRows23);
    ExpectPrefix(BuildDefaultKeyRequest(4, 5), kRequestDefRows45);
    ExpectPrefix(BuildStatusRequest(), kRequestStatus);
    ExpectPrefix(BuildTravelRequest(1), kRequestTravelHalf1);
    ExpectPrefix(BuildTravelRequest(2), kRequestTravelHalf2);

    const std::array<const std::array<std::uint8_t, 61>*, 5> expected{{
        &kRequestActiveBatch0,
        &kRequestActiveBatch1,
        &kRequestActiveBatch2,
        &kRequestActiveBatch3,
        &kRequestActiveBatch4,
    }};
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const KeyQuery query = kActiveQueries[index];
        ExpectPrefix(
            BuildKeyFunctionReadRequest(query, kKeyLayoutFn0),
            *expected[index]);
    }
}

void TestFirmwareIdentityPrecisionAndDefaultMap()
{
    assert(std::strlen(kFirmwareSha256) == 64u);
    assert(std::strlen(kUpdaterSha256) == 64u);

    SyncInfo sync{};
    assert(DecodeSyncInfo(Parse(kResponseSync, kCommandSync), &sync));
    assert(IsExpectedAulaWin60HeMaxFirmware(sync));
    assert(std::strncmp(sync.appVersion.data(), "App V1.1.6", 10u) == 0);
    assert(std::strncmp(sync.appBuildDate.data(), "Feb  4 2026", 11u) == 0);

    PrecisionStroke precision{};
    assert(DecodePrecisionStroke(Parse(
        kResponsePrecision, kCommandApi), &precision));
    assert(precision.precisionUm == kPrecisionUm);
    assert(precision.minimumTravelUm == kMinimumTravelUm);
    assert(precision.maximumTravelUm == kMaximumTravelUm);
    assert(IsExpectedAulaWin60HeMaxPrecision(precision));

    KeyMap decoded{};
    assert(DecodeDefaultKeyRows(Parse(
        kResponseDefRows01, kCommandDefaultKeys), 0u, 1u, &decoded));
    assert(DecodeDefaultKeyRows(Parse(
        kResponseDefRows23, kCommandDefaultKeys), 2u, 3u, &decoded));
    assert(DecodeDefaultKeyRows(Parse(
        kResponseDefRows45, kCommandDefaultKeys), 4u, 5u, &decoded));
    assert(decoded == kFirmwareKeyMap);
    assert(decoded == ExpectedAulaWin60HeMaxDefaultMap());
    assert(CountPhysicalKeyPositions(decoded) == kFirmwarePhysicalPositions);
    assert(CountMappedHids(decoded) == kFirmwareMappedUsages);
    assert(decoded[5][12] == 0x01u);
    assert(!IsPublishableKeyboardUsage(decoded[5][12]));
}

void TestActiveFn0OracleAndPaddingCorrelation()
{
    const std::array<const std::array<std::uint8_t, 61>*, 5> responses{{
        &kResponseActiveBatch0,
        &kResponseActiveBatch1,
        &kResponseActiveBatch2,
        &kResponseActiveBatch3,
        &kResponseActiveBatch4,
    }};

    KeyFunctionMap functions{};
    std::size_t valueIndex = 0;
    for (std::size_t index = 0; index < responses.size(); ++index)
    {
        FrameView frame{};
        assert(ParseResponseFrame(
            responses[index]->data(), responses[index]->size(),
            kCommandKeyFunctions, &frame));
        KeyFunctionBatch batch{};
        const KeyQuery query = kActiveQueries[index];
        assert(DecodeKeyFunctionReadResponse(
            frame, query, kKeyLayoutFn0, &batch));
        assert(ApplyKeyFunctionBatch(
            kFirmwareKeyMap, batch, &functions));

        const std::size_t unique = index == 4u ? 5u : 14u;
        for (std::size_t record = 0; record < unique; ++record)
            assert(batch[record].value == kExpectedActiveValues[valueIndex++]);
    }
    assert(valueIndex == kExpectedActiveValues.size());

    KeyMap active{};
    BuildPublishableActiveKeyMap(kFirmwareKeyMap, functions, &active);
    assert(active[1][0] == 0x52u); // physical Esc -> active Up
    assert(active[3][1] == 0u);    // physical A disabled
    assert(active[5][12] == 0u);   // internal F001 never becomes HID 01
    assert(CountMappedHids(active) == kOracleActiveMappedUsages);

    // Mutate only the low byte of the last repeated padding record. The weak
    // firmware checksum depends on the final high byte, so framing still
    // parses, but the decoder must reject the contradictory duplicate echo.
    auto inconsistent = kResponseActiveBatch4;
    constexpr std::size_t lastValueLow = 4u + 1u + 13u * 4u + 2u;
    inconsistent[lastValueLow] ^= 0x01u;
    FrameView frame{};
    assert(ParseResponseFrame(inconsistent.data(), inconsistent.size(),
        kCommandKeyFunctions, &frame));
    KeyFunctionBatch batch{};
    assert(!DecodeKeyFunctionReadResponse(
        frame, kActiveQueries[4], kKeyLayoutFn0, &batch));
}

void TestTravelOracleHasNoHalfEcho()
{
    const FrameView frame = Parse(
        kResponseTravel, kCommandMatrix6x21);
    assert(frame.payloadBytes == kTravelPayloadBytes);
    assert(frame.payload[0] == 0u);
    assert(frame.payload[1] == kSelectorTravel);
    TravelHalf decoded{};
    assert(DecodeTravelHalf(frame, &decoded));
    for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            assert(decoded[row][column] ==
                kTravelValues[row * kColumns + column]);
        }
    }
    assert(ResponseReportCount(frame.payloadBytes) == 3u);
}

void TestStatusOracleIncludesTerminator()
{
    const FrameView frame = Parse(
        kResponseStatus, kCommandMatrix6x21);
    assert(frame.payloadBytes == kStatusPayloadBytes);
    assert(frame.payload[kStatusPayloadBytes - 2u] == 0xFFu);
    assert(frame.payload[kStatusPayloadBytes - 1u] == 0xFFu);
    StatusMatrix decoded{};
    assert(DecodeStatusMatrix(frame, &decoded));
    for (std::size_t row = 0; row < kRows; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            assert(decoded[row][column] ==
                kStatusOracleValues[row * kColumns + column]);
        }
    }
}
}

int main()
{
    TestImmutableRequestVectors();
    TestFirmwareIdentityPrecisionAndDefaultMap();
    TestActiveFn0OracleAndPaddingCorrelation();
    TestTravelOracleHasNoHalfEcho();
    TestStatusOracleIncludesTerminator();
    std::cout << "AULA_WIN60HE_ORACLE_TEST=PASS\n";
    return 0;
}
