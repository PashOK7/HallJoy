#include "../HallJoy/aula_win60he_protocol.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace
{
using namespace aula_win60he;

template <std::size_t Size>
void AssertPrefix(
    const Report& report,
    const std::array<std::uint8_t, Size>& expected)
{
    assert(std::equal(expected.begin(), expected.end(), report.begin()));
    assert(std::all_of(report.begin() + Size, report.end(),
        [](std::uint8_t value) { return value == 0u; }));
}

std::vector<std::uint8_t> MakeResponse(
    std::uint8_t requestCommand,
    const std::vector<std::uint8_t>& payload)
{
    const std::size_t reports = ResponseReportCount(payload.size());
    std::vector<std::uint8_t> stream(reports * kWireReportBytes, 0);
    stream[0] = kFrameHead;
    stream[1] = static_cast<std::uint8_t>(payload.size());
    stream[2] = ResponseCommand(requestCommand);
    stream[3] = ComputeChecksum(
        stream[1], stream[2], payload.empty() ? nullptr : payload.data());
    std::copy(payload.begin(), payload.end(), stream.begin() + 4u);
    return stream;
}

FrameView Parse(
    const std::vector<std::uint8_t>& stream,
    std::uint8_t requestCommand)
{
    FrameView frame{};
    assert(ParseResponseFrame(
        stream.data(), stream.size(), requestCommand, &frame));
    return frame;
}

KeyQuery FirstActiveQuery()
{
    return KeyQuery{{
        0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A,
    }};
}

std::vector<std::uint8_t> ActivePayload(
    const KeyQuery& query,
    std::uint8_t layout = kKeyLayoutFn0)
{
    std::vector<std::uint8_t> payload(kKeyFunctionPayloadBytes, 0);
    payload[0] = 0;
    for (std::size_t index = 0; index < query.size(); ++index)
    {
        const std::size_t offset = 1u + index * 4u;
        payload[offset] = query[index];
        payload[offset + 1u] = layout;
        payload[offset + 2u] = query[index];
        payload[offset + 3u] = 0;
    }
    return payload;
}

void TestExactRequestVectors()
{
    AssertPrefix(BuildSyncRequest(),
        std::array<std::uint8_t, 10>{{
            0x5C, 0x06, 0x01, 0x97, 0x01,
            0x02, 0x03, 0x04, 0xFF, 0xFF,
        }});
    AssertPrefix(BuildPrecisionStrokeRequest(),
        std::array<std::uint8_t, 7>{{
            0x5C, 0x03, 0x00, 0x93, 0x25, 0xFF, 0xFF,
        }});
    AssertPrefix(BuildDefaultKeyRequest(0, 1),
        std::array<std::uint8_t, 7>{{
            0x5C, 0x03, 0x2B, 0xC0, 0x00, 0x00, 0x01,
        }});
    AssertPrefix(BuildDefaultKeyRequest(2, 3),
        std::array<std::uint8_t, 7>{{
            0x5C, 0x03, 0x2B, 0xC2, 0x00, 0x02, 0x03,
        }});
    AssertPrefix(BuildDefaultKeyRequest(4, 5),
        std::array<std::uint8_t, 7>{{
            0x5C, 0x03, 0x2B, 0xC4, 0x00, 0x04, 0x05,
        }});
    AssertPrefix(BuildStatusRequest(),
        std::array<std::uint8_t, 8>{{
            0x5C, 0x04, 0x12, 0xA6, 0x03, 0x01, 0xFF, 0xFF,
        }});
    AssertPrefix(BuildTravelRequest(1),
        std::array<std::uint8_t, 8>{{
            0x5C, 0x04, 0x12, 0xA6, 0x02, 0x01, 0xFF, 0xFF,
        }});
    AssertPrefix(BuildTravelRequest(2),
        std::array<std::uint8_t, 8>{{
            0x5C, 0x04, 0x12, 0xA6, 0x02, 0x02, 0xFF, 0xFF,
        }});

    const auto query = FirstActiveQuery();
    const Report active = BuildKeyFunctionReadRequest(query, kKeyLayoutFn0);
    assert(active[0] == 0x5Cu);
    assert(active[1] == 0x39u);
    assert(active[2] == 0x23u);
    assert(active[3] == 0xEDu);
    assert(active[4] == 0u);
    for (std::size_t index = 0; index < query.size(); ++index)
    {
        const std::size_t offset = 5u + index * 4u;
        assert(active[offset] == query[index]);
        assert(active[offset + 1u] == 0u);
        assert(active[offset + 2u] == 0u);
        assert(active[offset + 3u] == 0u);
    }
}

void TestExactWindowsHidEnvelope()
{
    const Report request = BuildTravelRequest(1);
    HidWireReport wire{};
    std::size_t bytes = 123u;
    assert(EncodeHidReport(
        request, kWindowsHidReportBytes, &wire, &bytes));
    assert(bytes == kWindowsHidReportBytes);
    assert(wire[0] == 0u);
    assert(std::equal(request.begin(), request.end(), wire.begin() + 1u));

    Report decoded{};
    assert(DecodeHidReport(
        wire.data(), kWindowsHidReportBytes, &decoded));
    assert(decoded == request);

    bytes = 123u;
    assert(!EncodeHidReport(request, kWireReportBytes, &wire, &bytes));
    assert(bytes == 0u);
    assert(!DecodeHidReport(wire.data(), kWireReportBytes, &decoded));
    wire[0] = 1u;
    assert(!DecodeHidReport(
        wire.data(), kWindowsHidReportBytes, &decoded));

    std::array<std::uint8_t, kWindowsHidReportBytes * 2u> reports{};
    HidWireReport first{};
    HidWireReport second{};
    bytes = 0;
    assert(EncodeHidReport(BuildTravelRequest(1),
        kWindowsHidReportBytes, &first, &bytes));
    assert(EncodeHidReport(BuildTravelRequest(2),
        kWindowsHidReportBytes, &second, &bytes));
    std::copy(first.begin(), first.end(), reports.begin());
    std::copy(second.begin(), second.end(),
        reports.begin() + kWindowsHidReportBytes);
    ResponseStream flattened{};
    std::size_t flattenedBytes = 0;
    assert(FlattenHidReports(reports.data(), 2u,
        kWindowsHidReportBytes, &flattened, &flattenedBytes));
    assert(flattenedBytes == 128u);
}

void TestSyncPrecisionAndDefaultMap()
{
    std::vector<std::uint8_t> syncPayload(54u, 0);
    syncPayload[0] = 0;
    syncPayload[1] = 0x78;
    syncPayload[2] = 0x56;
    syncPayload[3] = 0x34;
    syncPayload[4] = 0x12;
    constexpr char serial[] = "AULA-UNIT-TEST";
    std::copy_n(serial, sizeof(serial) - 1u, syncPayload.begin() + 9u);
    constexpr char version[] = "App V1.1.6";
    constexpr char buildDate[] = "Feb  4 2026";
    std::copy_n(version, sizeof(version) - 1u, syncPayload.begin() + 26u);
    std::copy_n(buildDate, sizeof(buildDate) - 1u, syncPayload.begin() + 43u);
    SyncInfo sync{};
    assert(DecodeSyncInfo(Parse(MakeResponse(
        kCommandSync, syncPayload), kCommandSync), &sync));
    assert(IsExpectedAulaWin60HeMaxFirmware(sync));
    auto oversizedSyncPayload = syncPayload;
    oversizedSyncPayload.push_back(0);
    assert(!DecodeSyncInfo(Parse(MakeResponse(
        kCommandSync, oversizedSyncPayload), kCommandSync), &sync));
    assert(DecodeSyncInfo(Parse(MakeResponse(
        kCommandSync, syncPayload), kCommandSync), &sync));
    sync.appVersion[9] = '5';
    assert(!IsExpectedAulaWin60HeMaxFirmware(sync));
    assert(DecodeSyncInfo(Parse(MakeResponse(
        kCommandSync, syncPayload), kCommandSync), &sync));
    sync.appBuildDate[10] = '5';
    assert(!IsExpectedAulaWin60HeMaxFirmware(sync));

    const std::vector<std::uint8_t> precisionPayload{
        0, kOrderPrecisionStroke, 10, 10, 0, 0x48, 0x0D,
    };
    PrecisionStroke precision{};
    assert(DecodePrecisionStroke(Parse(MakeResponse(
        kCommandApi, precisionPayload), kCommandApi), &precision));
    assert(IsExpectedAulaWin60HeMaxPrecision(precision));

    KeyMap decoded{};
    const auto& expected = ExpectedAulaWin60HeMaxDefaultMap();
    for (std::uint8_t first = 0; first < kRows; first += 2u)
    {
        const std::uint8_t second = static_cast<std::uint8_t>(first + 1u);
        std::vector<std::uint8_t> payload(kDefaultKeyPayloadBytes, 0);
        payload[0] = 0;
        payload[1] = first;
        payload[23] = second;
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            payload[2u + column] = expected[first][column];
            payload[24u + column] = expected[second][column];
        }
        assert(DecodeDefaultKeyRows(Parse(MakeResponse(
            kCommandDefaultKeys, payload), kCommandDefaultKeys),
            first, second, &decoded));
    }
    assert(decoded == expected);
    assert(IsExpectedAulaWin60HeMaxDefaultMap(decoded));
    assert(CountPhysicalKeyPositions(decoded) == 61u);
    assert(CountMappedHids(decoded) == 60u);
    assert(decoded[5][12] == 0x01u);
    assert(!IsPublishableKeyboardUsage(decoded[5][12]));
}

void TestActiveFunctionCorrelationAndPublication()
{
    const KeyQuery query = FirstActiveQuery();
    auto payload = ActivePayload(query);
    payload[3] = 0x52u; // Esc -> Up Arrow
    payload[4] = 0x00u;
    const auto activeStream = MakeResponse(kCommandKeyFunctions, payload);
    const auto frame = Parse(activeStream, kCommandKeyFunctions);
    KeyFunctionBatch batch{};
    assert(DecodeKeyFunctionReadResponse(
        frame, query, kKeyLayoutFn0, &batch));
    assert(batch[0].value == 0x0052u);

    KeyFunctionMap functions{};
    assert(ApplyKeyFunctionBatch(
        ExpectedAulaWin60HeMaxDefaultMap(), batch, &functions));
    KeyMap active{};
    BuildPublishableActiveKeyMap(
        ExpectedAulaWin60HeMaxDefaultMap(), functions, &active);
    assert(active[1][0] == 0x52u);

    auto wrongKey = payload;
    wrongKey[1u + 4u] ^= 1u;
    assert(!DecodeKeyFunctionReadResponse(Parse(MakeResponse(
        kCommandKeyFunctions, wrongKey), kCommandKeyFunctions),
        query, kKeyLayoutFn0, &batch));

    auto wrongLayout = payload;
    wrongLayout[1u + 4u + 1u] = 1u;
    assert(!DecodeKeyFunctionReadResponse(Parse(MakeResponse(
        kCommandKeyFunctions, wrongLayout), kCommandKeyFunctions),
        query, kKeyLayoutFn0, &batch));

    KeyQuery padded{};
    padded.fill(0x01u);
    auto paddedPayload = ActivePayload(padded);
    for (std::size_t index = 0; index < padded.size(); ++index)
    {
        const std::size_t offset = 1u + index * 4u;
        paddedPayload[offset + 2u] = 0x01u;
        paddedPayload[offset + 3u] = 0xF0u;
    }
    assert(DecodeKeyFunctionReadResponse(Parse(MakeResponse(
        kCommandKeyFunctions, paddedPayload), kCommandKeyFunctions),
        padded, kKeyLayoutFn0, &batch));
    paddedPayload[1u + 13u * 4u + 2u] ^= 1u;
    assert(!DecodeKeyFunctionReadResponse(Parse(MakeResponse(
        kCommandKeyFunctions, paddedPayload), kCommandKeyFunctions),
        padded, kKeyLayoutFn0, &batch));

    // Explicitly distinguish an ordinary remap from internal and disabled
    // functions. Zero is the value returned by active-map RAM itself; HallJoy
    // must not silently substitute the factory map.
    KeyQuery semanticQuery{};
    semanticQuery.fill(0x07u);
    semanticQuery[0] = 0x04u; // physical A
    semanticQuery[1] = 0x16u; // physical S
    semanticQuery[2] = 0x07u; // physical D
    auto semanticPayload = ActivePayload(semanticQuery);
    auto setValue = [&](std::size_t index, std::uint16_t value) {
        const std::size_t offset = 1u + index * 4u;
        semanticPayload[offset + 2u] = static_cast<std::uint8_t>(value & 0xFFu);
        semanticPayload[offset + 3u] = static_cast<std::uint8_t>(value >> 8u);
    };
    setValue(0u, 0x0052u); // A -> Up Arrow
    setValue(1u, 0xF001u); // internal function
    setValue(2u, 0x0000u); // disabled/unassigned
    for (std::size_t index = 3u; index < semanticQuery.size(); ++index)
        setValue(index, 0x0000u);
    assert(DecodeKeyFunctionReadResponse(Parse(MakeResponse(
        kCommandKeyFunctions, semanticPayload), kCommandKeyFunctions),
        semanticQuery, kKeyLayoutFn0, &batch));
    KeyFunctionMap semanticFunctions{};
    assert(ApplyKeyFunctionBatch(
        ExpectedAulaWin60HeMaxDefaultMap(), batch, &semanticFunctions));
    KeyMap semanticMap{};
    BuildPublishableActiveKeyMap(
        ExpectedAulaWin60HeMaxDefaultMap(), semanticFunctions, &semanticMap);
    assert(semanticMap[3][1] == 0x52u);
    assert(semanticMap[3][2] == 0u);
    assert(semanticMap[3][3] == 0u);

    assert(IsPublishableKeyFunction(0x0004u));
    assert(IsPublishableKeyFunction(0x00E7u));
    assert(!IsPublishableKeyFunction(0x0000u));
    assert(!IsPublishableKeyFunction(0xF001u));
    assert(!IsPublishableKeyFunction(0x1234u));
}

void TestRealTravelAndExactStatusShape()
{
    std::vector<std::uint8_t> travelPayload(kTravelPayloadBytes, 0);
    travelPayload[0] = 0;
    travelPayload[1] = kSelectorTravel;
    for (std::size_t index = 0; index < kTravelValuesPerHalf; ++index)
    {
        const std::uint16_t value = static_cast<std::uint16_t>(index * 53u);
        travelPayload[2u + index * 2u] =
            static_cast<std::uint8_t>(value & 0xFFu);
        travelPayload[3u + index * 2u] =
            static_cast<std::uint8_t>(value >> 8u);
    }
    TravelHalf travel{};
    assert(DecodeTravelHalf(Parse(MakeResponse(
        kCommandMatrix6x21, travelPayload), kCommandMatrix6x21), &travel));
    assert(travel[0][1] == 53u);
    assert(travel[2][20] == 62u * 53u);
    assert(ResponseReportCount(kTravelPayloadBytes) == 3u);

    auto stage8Payload = travelPayload;
    stage8Payload[0] = kSelectorTravel;
    stage8Payload[1] = 1u;
    assert(!DecodeTravelHalf(Parse(MakeResponse(
        kCommandMatrix6x21, stage8Payload), kCommandMatrix6x21), &travel));

    std::vector<std::uint8_t> statusPayload(kStatusPayloadBytes, 0);
    statusPayload[0] = 0;
    statusPayload[1] = kSelectorStatus;
    for (std::size_t index = 0; index < kRows * kColumns; ++index)
        statusPayload[2u + index] = static_cast<std::uint8_t>(index % 4u);
    statusPayload[kStatusPayloadBytes - 2u] = 0xFFu;
    statusPayload[kStatusPayloadBytes - 1u] = 0xFFu;
    StatusMatrix status{};
    assert(DecodeStatusMatrix(Parse(MakeResponse(
        kCommandMatrix6x21, statusPayload), kCommandMatrix6x21), &status));
    assert(status[5][20] == 1u);
    assert(ResponseReportCount(kStatusPayloadBytes) == 3u);
    statusPayload.back() = 0;
    assert(!DecodeStatusMatrix(Parse(MakeResponse(
        kCommandMatrix6x21, statusPayload), kCommandMatrix6x21), &status));
}

void TestFrameRejectionNormalizationAndFuzzSmoke()
{
    const auto valid = MakeResponse(kCommandApi,
        {0, kOrderPrecisionStroke, 10, 10, 0, 0x48, 0x0D});
    FrameView frame{};
    assert(ParseResponseFrame(valid.data(), valid.size(), kCommandApi, &frame));

    auto broken = valid;
    broken[0] ^= 1u;
    assert(!ParseResponseFrame(
        broken.data(), broken.size(), kCommandApi, &frame));
    broken = valid;
    broken[2] ^= 1u;
    assert(!ParseResponseFrame(
        broken.data(), broken.size(), kCommandApi, &frame));
    broken = valid;
    broken[3] ^= 1u;
    assert(!ParseResponseFrame(
        broken.data(), broken.size(), kCommandApi, &frame));
    assert(!ParseResponseFrame(valid.data(), 4u, kCommandApi, &frame));

    assert(NormalizeTravelToMilli(0u, 3400u) == 0u);
    assert(NormalizeTravelToMilli(10u, 3400u) == 3u);
    assert(NormalizeTravelToMilli(850u, 3400u) == 250u);
    assert(NormalizeTravelToMilli(1700u, 3400u) == 500u);
    assert(NormalizeTravelToMilli(3400u, 3400u) == 1000u);
    assert(NormalizeTravelToMilli(4000u, 3400u) == 1000u);

    std::mt19937 random(0x5C9212u);
    std::uniform_int_distribution<unsigned int> byte(0u, 255u);
    std::uniform_int_distribution<std::size_t> length(0u, 192u);
    std::array<std::uint8_t, 192> input{};
    for (std::size_t iteration = 0; iteration < 50000u; ++iteration)
    {
        for (auto& value : input)
            value = static_cast<std::uint8_t>(byte(random));
        FrameView fuzz{};
        (void)ParseResponseFrame(
            input.data(), length(random), kCommandMatrix6x21, &fuzz);
    }
}
}

int main()
{
    TestExactRequestVectors();
    TestExactWindowsHidEnvelope();
    TestSyncPrecisionAndDefaultMap();
    TestActiveFunctionCorrelationAndPublication();
    TestRealTravelAndExactStatusShape();
    TestFrameRejectionNormalizationAndFuzzSmoke();
    std::cout << "AULA_WIN60HE_PROTOCOL_TEST=PASS\n";
    return 0;
}
