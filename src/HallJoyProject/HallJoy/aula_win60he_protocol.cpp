#include "aula_win60he_protocol.h"

#include <algorithm>
#include <cstring>

namespace aula_win60he
{
namespace
{
constexpr KeyMap kExpectedDefaultMap{{
    std::array<std::uint8_t, kColumns>{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    std::array<std::uint8_t, kColumns>{{ 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    std::array<std::uint8_t, kColumns>{{ 0x2B, 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    std::array<std::uint8_t, kColumns>{{ 0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33, 0x34, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    std::array<std::uint8_t, kColumns>{{ 0xE1, 0x00, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xE5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    std::array<std::uint8_t, kColumns>{{ 0xE0, 0xE3, 0xE2, 0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0xE6, 0x65, 0xE4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
}};

bool FindUniqueFactoryKey(
    const KeyMap& map,
    std::uint8_t key,
    std::size_t* outRow,
    std::size_t* outColumn) noexcept
{
    if (outRow) *outRow = 0;
    if (outColumn) *outColumn = 0;
    if (key == 0 || !outRow || !outColumn)
        return false;

    bool found = false;
    for (std::size_t row = 0; row < kRows; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            if (map[row][column] != key)
                continue;
            if (found)
                return false;
            found = true;
            *outRow = row;
            *outColumn = column;
        }
    }
    return found;
}
}

std::uint8_t ComputeChecksum(
    std::uint8_t payloadLength,
    std::uint8_t command,
    const std::uint8_t* payload) noexcept
{
    std::uint32_t checksum = 0x35u + kFrameHead + payloadLength + command;
    if (payloadLength > 0 && payload)
        checksum += payload[payloadLength - 1u];
    return static_cast<std::uint8_t>(checksum & 0xFFu);
}

Report BuildFrame(
    std::uint8_t command,
    const std::uint8_t* payload,
    std::size_t payloadLength) noexcept
{
    Report report{};
    if (payloadLength > report.size() - 4u || payloadLength > 0xFFu ||
        (payloadLength != 0 && !payload))
        return report;

    const auto length = static_cast<std::uint8_t>(payloadLength);
    report[0] = kFrameHead;
    report[1] = length;
    report[2] = command;
    report[3] = ComputeChecksum(length, command, payload);
    if (payloadLength)
        std::memcpy(report.data() + 4u, payload, payloadLength);
    return report;
}

Report BuildSyncRequest() noexcept
{
    constexpr std::array<std::uint8_t, 6> payload{{ 0x01, 0x02, 0x03, 0x04, 0xFF, 0xFF }};
    return BuildFrame(kCommandSync, payload.data(), payload.size());
}

Report BuildPrecisionStrokeRequest() noexcept
{
    constexpr std::array<std::uint8_t, 3> payload{{ kOrderPrecisionStroke, 0xFF, 0xFF }};
    return BuildFrame(kCommandApi, payload.data(), payload.size());
}

Report BuildDefaultKeyRequest(std::uint8_t firstRow, std::uint8_t secondRow) noexcept
{
    const std::array<std::uint8_t, 3> payload{{ kReadOperation, firstRow, secondRow }};
    return BuildFrame(kCommandDefaultKeys, payload.data(), payload.size());
}

Report BuildKeyFunctionReadRequest(
    const KeyQuery& factoryKeys,
    std::uint8_t layout) noexcept
{
    std::array<std::uint8_t, kKeyFunctionPayloadBytes> payload{};
    payload[0] = kReadOperation;
    for (std::size_t index = 0; index < factoryKeys.size(); ++index)
    {
        const std::size_t offset = 1u + index * 4u;
        payload[offset] = factoryKeys[index];
        payload[offset + 1u] = layout;
        payload[offset + 2u] = 0;
        payload[offset + 3u] = 0;
    }
    return BuildFrame(kCommandKeyFunctions, payload.data(), payload.size());
}

Report BuildTravelRequest(std::uint8_t half) noexcept
{
    const std::array<std::uint8_t, 4> payload{{ kSelectorTravel, half, 0xFF, 0xFF }};
    return BuildFrame(kCommandMatrix6x21, payload.data(), payload.size());
}

Report BuildStatusRequest() noexcept
{
    constexpr std::array<std::uint8_t, 4> payload{{ kSelectorStatus, 0x01, 0xFF, 0xFF }};
    return BuildFrame(kCommandMatrix6x21, payload.data(), payload.size());
}

bool EncodeHidReport(
    const Report& protocol,
    std::size_t reportBytes,
    HidWireReport* out,
    std::size_t* outBytes) noexcept
{
    if (out) out->fill(0);
    if (outBytes) *outBytes = 0;
    if (!out || !outBytes || reportBytes != kWindowsHidReportBytes)
        return false;

    std::copy(protocol.begin(), protocol.end(), out->begin() + 1u);
    *outBytes = reportBytes;
    return true;
}

bool DecodeHidReport(
    const std::uint8_t* wire,
    std::size_t reportBytes,
    Report* out) noexcept
{
    if (out) out->fill(0);
    if (!wire || !out || reportBytes != kWindowsHidReportBytes || wire[0] != 0)
        return false;

    std::copy_n(wire + 1u, kWireReportBytes, out->begin());
    return true;
}

bool FlattenHidReports(
    const std::uint8_t* reports,
    std::size_t reportCount,
    std::size_t reportBytes,
    ResponseStream* out,
    std::size_t* outBytes) noexcept
{
    if (out) out->fill(0);
    if (outBytes) *outBytes = 0;
    if (!reports || !out || !outBytes || reportCount == 0 ||
        reportCount > kMaximumResponseReports || reportBytes != kWindowsHidReportBytes)
        return false;

    for (std::size_t report = 0; report < reportCount; ++report)
    {
        Report block{};
        if (!DecodeHidReport(reports + report * reportBytes, reportBytes, &block))
            return false;
        std::copy(block.begin(), block.end(),
            out->begin() + static_cast<std::ptrdiff_t>(
                report * kWireReportBytes));
    }
    *outBytes = reportCount * kWireReportBytes;
    return true;
}

bool ParseResponseFrame(
    const std::uint8_t* stream,
    std::size_t streamBytes,
    std::uint8_t requestCommand,
    FrameView* out) noexcept
{
    if (out) *out = FrameView{};
    if (!stream || !out || streamBytes < 4u || stream[0] != kFrameHead)
        return false;

    const std::uint8_t payloadLength = stream[1];
    const std::uint8_t command = stream[2];
    const std::uint8_t checksum = stream[3];
    const std::size_t frameBytes = 4u + payloadLength;
    if (command != ResponseCommand(requestCommand) ||
        frameBytes > streamBytes || frameBytes > kMaximumResponseBytes)
        return false;

    const auto* payload = stream + 4u;
    if (checksum != ComputeChecksum(payloadLength, command, payload))
        return false;

    out->command = command;
    out->payload = payload;
    out->payloadBytes = payloadLength;
    return true;
}

std::size_t ResponseReportCount(std::size_t payloadBytes) noexcept
{
    const std::size_t frameBytes = 4u + payloadBytes;
    return (frameBytes + kWireReportBytes - 1u) / kWireReportBytes;
}

bool DecodeSyncInfo(const FrameView& frame, SyncInfo* out) noexcept
{
    if (out) *out = SyncInfo{};
    if (!out || frame.command != ResponseCommand(kCommandSync) || !frame.payload ||
        frame.payloadBytes != kSyncPayloadBytes || frame.payload[0] != 0)
        return false;

    const auto* p = frame.payload;
    std::copy_n(p, kSyncPayloadBytes, out->rawPayload.begin());
    out->boardId = static_cast<std::uint32_t>(p[1]) |
        (static_cast<std::uint32_t>(p[2]) << 8u) |
        (static_cast<std::uint32_t>(p[3]) << 16u) |
        (static_cast<std::uint32_t>(p[4]) << 24u);
    for (std::size_t i = 0; i < kSyncSerialBytes; ++i)
        out->serial[i] = static_cast<char>(p[kSyncSerialOffset + i]);
    for (std::size_t i = 0; i < kSyncAppVersionBytes; ++i)
        out->appVersion[i] =
            static_cast<char>(p[kSyncAppVersionOffset + i]);
    for (std::size_t i = 0; i < kSyncBuildLabelBytes; ++i)
        out->buildLabel[i] =
            static_cast<char>(p[kSyncBuildDescriptorOffset + i]);

    return out->appVersion[0] == 'A' && out->appVersion[1] == 'p' &&
        out->appVersion[2] == 'p' && out->appVersion[3] == ' ' &&
        out->appVersion[4] == 'V' && out->buildLabel[0] != '\0';
}

bool DecodePrecisionStroke(const FrameView& frame, PrecisionStroke* out) noexcept
{
    if (out) *out = PrecisionStroke{};
    if (!out || frame.command != ResponseCommand(kCommandApi) || !frame.payload ||
        frame.payloadBytes != 7u || frame.payload[0] != 0 ||
        frame.payload[1] != kOrderPrecisionStroke)
        return false;

    out->precisionUm = frame.payload[2];
    out->minimumTravelUm = static_cast<std::uint16_t>(frame.payload[3]) |
        (static_cast<std::uint16_t>(frame.payload[4]) << 8u);
    out->maximumTravelUm = static_cast<std::uint16_t>(frame.payload[5]) |
        (static_cast<std::uint16_t>(frame.payload[6]) << 8u);

    return out->precisionUm >= 1u && out->precisionUm <= 100u &&
        out->minimumTravelUm <= out->maximumTravelUm &&
        out->maximumTravelUm >= 500u && out->maximumTravelUm <= 10000u;
}

bool DecodeDefaultKeyRows(
    const FrameView& frame,
    std::uint8_t expectedFirstRow,
    std::uint8_t expectedSecondRow,
    KeyMap* inOutMap) noexcept
{
    if (!inOutMap || frame.command != ResponseCommand(kCommandDefaultKeys) ||
        !frame.payload || frame.payloadBytes != 45u || frame.payload[0] != 0 ||
        expectedFirstRow >= kRows || expectedSecondRow >= kRows ||
        expectedFirstRow == expectedSecondRow)
        return false;

    const auto* p = frame.payload;
    if (p[1] != expectedFirstRow || p[23] != expectedSecondRow)
        return false;

    for (std::size_t column = 0; column < kColumns; ++column)
    {
        (*inOutMap)[expectedFirstRow][column] = p[2u + column];
        (*inOutMap)[expectedSecondRow][column] = p[24u + column];
    }
    return true;
}

bool DecodeKeyFunctionReadResponse(
    const FrameView& frame,
    const KeyQuery& expectedFactoryKeys,
    std::uint8_t expectedLayout,
    KeyFunctionBatch* out) noexcept
{
    if (out) *out = KeyFunctionBatch{};
    if (!out || frame.command != ResponseCommand(kCommandKeyFunctions) ||
        !frame.payload || frame.payloadBytes != kKeyFunctionPayloadBytes ||
        frame.payload[0] != 0)
        return false;

    for (std::size_t index = 0; index < kKeyFunctionRecordsPerFrame; ++index)
    {
        const std::size_t offset = 1u + index * 4u;
        KeyFunctionRecord record{};
        record.key = frame.payload[offset];
        record.layout = frame.payload[offset + 1u];
        record.value = static_cast<std::uint16_t>(frame.payload[offset + 2u]) |
            (static_cast<std::uint16_t>(frame.payload[offset + 3u]) << 8u);
        if (record.key != expectedFactoryKeys[index] ||
            record.layout != expectedLayout || record.key == 0)
            return false;
        // The final short query is padded by repeating its last real factory
        // key. Every echo of that duplicate must return the same 16-bit
        // function; accepting last-write-wins would let an internally
        // contradictory response silently alter the active map.
        for (std::size_t previous = 0; previous < index; ++previous)
        {
            if (expectedFactoryKeys[previous] == record.key &&
                (*out)[previous].value != record.value)
                return false;
        }
        (*out)[index] = record;
    }
    return true;
}

bool DecodeTravelHalf(const FrameView& frame, TravelHalf* out) noexcept
{
    if (out) *out = TravelHalf{};
    if (!out || frame.command != ResponseCommand(kCommandMatrix6x21) ||
        !frame.payload || frame.payloadBytes != kTravelPayloadBytes ||
        frame.payload[0] != 0 || frame.payload[1] != kSelectorTravel)
        return false;

    const auto* values = frame.payload + 2u;
    for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            const std::size_t index = (row * kColumns + column) * 2u;
            (*out)[row][column] = static_cast<std::uint16_t>(values[index]) |
                (static_cast<std::uint16_t>(values[index + 1u]) << 8u);
        }
    }
    return true;
}

bool DecodeStatusMatrix(const FrameView& frame, StatusMatrix* out) noexcept
{
    if (out) *out = StatusMatrix{};
    if (!out || frame.command != ResponseCommand(kCommandMatrix6x21) ||
        !frame.payload || frame.payloadBytes != kStatusPayloadBytes ||
        frame.payload[0] != 0 || frame.payload[1] != kSelectorStatus ||
        frame.payload[kStatusPayloadBytes - 2u] != 0xFFu ||
        frame.payload[kStatusPayloadBytes - 1u] != 0xFFu)
        return false;

    const auto* values = frame.payload + 2u;
    for (std::size_t row = 0; row < kRows; ++row)
        for (std::size_t column = 0; column < kColumns; ++column)
            (*out)[row][column] = values[row * kColumns + column];
    return true;
}

void MergeTravelHalves(const TravelHalf& first, const TravelHalf& second, TravelMatrix* out) noexcept
{
    if (!out) return;
    out->fill({});
    for (std::size_t row = 0; row < kRowsPerTravelHalf; ++row)
    {
        (*out)[row] = first[row];
        (*out)[row + kRowsPerTravelHalf] = second[row];
    }
}

const KeyMap& ExpectedAulaWin60HeMaxDefaultMap() noexcept
{
    return kExpectedDefaultMap;
}

bool IsExpectedAulaWin60HeMaxDefaultMap(const KeyMap& map) noexcept
{
    return map == kExpectedDefaultMap;
}

bool IsExpectedAulaWin60HeMaxPrecision(const PrecisionStroke& precision) noexcept
{
    return precision.precisionUm == kExpectedPrecisionUm &&
        precision.minimumTravelUm == kExpectedMinimumTravelUm &&
        precision.maximumTravelUm == kExpectedMaximumTravelUm;
}

bool IsExpectedAulaWin60HeMaxFirmware(const SyncInfo& sync) noexcept
{
    constexpr std::array<std::uint8_t, 9> expectedPrefix{{
        0x00, 0x02, 0x19, 0x02, 0x0A, 0xC0, 0x01, 0x00, 0x10,
    }};
    constexpr std::array<std::uint8_t, kSyncDescriptorBytes> expectedApp{{
        0x41, 0x70, 0x70, 0x20, 0x56, 0x31, 0x2E, 0x31,
        0x2E, 0x36, 0x00, 0x00, 0x31, 0x36, 0x3A, 0x33,
    }};
    constexpr std::array<std::uint8_t, kSyncDescriptorBytes> expectedBuild{{
        0x46, 0x65, 0x62, 0x20, 0x20, 0x34, 0x20, 0x36,
        0x33, 0x32, 0x30, 0x38, 0xFC, 0x01, 0x03, 0x12,
    }};
    return std::equal(expectedPrefix.begin(), expectedPrefix.end(),
            sync.rawPayload.begin()) &&
        sync.rawPayload[kSyncAppLengthOffset] == kSyncDescriptorBytes &&
        std::equal(expectedApp.begin(), expectedApp.end(),
            sync.rawPayload.begin() + kSyncAppDescriptorOffset) &&
        sync.rawPayload[kSyncBuildLengthOffset] == kSyncDescriptorBytes &&
        std::equal(expectedBuild.begin(), expectedBuild.end(),
            sync.rawPayload.begin() + kSyncBuildDescriptorOffset) &&
        sync.rawPayload[kSyncTrailerOffset] == 0xFFu;
}

bool IsAula6x21FamilyFirmware(const SyncInfo& sync) noexcept
{
    // The product/board bytes and build descriptor may legitimately vary
    // between models. Keep the invariant framing produced by the proven
    // SparkPlayJoy 6x21 platform and require meaningful descriptor text.
    const auto printableAscii = [](const char* text, std::size_t bytes) noexcept {
        bool meaningful = false;
        for (std::size_t index = 0; index < bytes; ++index)
        {
            const auto value = static_cast<unsigned char>(text[index]);
            if (value == 0)
                continue;
            if (value < 0x20u || value > 0x7Eu)
                return false;
            meaningful = true;
        }
        return meaningful;
    };

    return sync.rawPayload[0] == 0u &&
        sync.boardId != 0u &&
        sync.rawPayload[5] == 0xC0u &&
        sync.rawPayload[6] == 0x01u &&
        sync.rawPayload[7] == 0x00u &&
        sync.rawPayload[kSyncSerialLengthOffset] == kSyncDescriptorBytes &&
        sync.rawPayload[kSyncAppLengthOffset] == kSyncDescriptorBytes &&
        sync.rawPayload[kSyncBuildLengthOffset] == kSyncDescriptorBytes &&
        sync.rawPayload[kSyncTrailerOffset] == 0xFFu &&
        sync.appVersion[0] == 'A' && sync.appVersion[1] == 'p' &&
        sync.appVersion[2] == 'p' && sync.appVersion[3] == ' ' &&
        sync.appVersion[4] == 'V' &&
        printableAscii(sync.appVersion.data(), kSyncAppVersionBytes) &&
        printableAscii(sync.buildLabel.data(), kSyncBuildLabelBytes);
}

bool IsAula6x21FamilyPrecision(const PrecisionStroke& precision) noexcept
{
    return precision.precisionUm >= 1u && precision.precisionUm <= 100u &&
        precision.minimumTravelUm <= precision.maximumTravelUm &&
        precision.maximumTravelUm >= 500u &&
        precision.maximumTravelUm <= 10000u;
}

bool IsAula6x21FamilyDefaultMap(const KeyMap& map) noexcept
{
    std::array<bool, 256> seen{};
    std::size_t physical = 0;
    std::size_t publishable = 0;
    for (const auto& row : map)
    {
        for (const auto key : row)
        {
            if (key == 0)
                continue;
            if (seen[key])
                return false;
            seen[key] = true;
            ++physical;
            if (IsPublishableKeyboardUsage(key))
                ++publishable;
        }
    }
    // A compatible keyboard must expose a useful keyboard map. The 6x21 wire
    // matrix is fixed, while the populated positions may vary by model.
    return physical >= 2u && physical <= kMatrixPositions && publishable >= 1u;
}

bool IsPublishableKeyboardUsage(std::uint8_t hidUsage) noexcept
{
    return hidUsage >= 0x04u && hidUsage <= 0xE7u;
}

bool IsPublishableKeyFunction(std::uint16_t function) noexcept
{
    return function <= 0x00FFu &&
        IsPublishableKeyboardUsage(static_cast<std::uint8_t>(function));
}

std::size_t CountMappedHids(const KeyMap& map) noexcept
{
    std::array<bool, 256> seen{};
    for (const auto& row : map)
        for (const auto hid : row)
            if (IsPublishableKeyboardUsage(hid))
                seen[hid] = true;
    return static_cast<std::size_t>(std::count(seen.begin(), seen.end(), true));
}

std::size_t CountPhysicalKeyPositions(const KeyMap& map) noexcept
{
    std::size_t count = 0;
    for (const auto& row : map)
        count += static_cast<std::size_t>(std::count_if(row.begin(), row.end(),
            [](std::uint8_t key) { return key != 0; }));
    return count;
}

bool ApplyKeyFunctionBatch(
    const KeyMap& defaultMap,
    const KeyFunctionBatch& batch,
    KeyFunctionMap* inOutFunctions) noexcept
{
    if (!inOutFunctions)
        return false;
    for (const auto& record : batch)
    {
        std::size_t row = 0;
        std::size_t column = 0;
        if (record.layout != kKeyLayoutFn0 ||
            !FindUniqueFactoryKey(defaultMap, record.key, &row, &column))
            return false;
        (*inOutFunctions)[row][column] = record.value;
    }
    return true;
}

void BuildPublishableActiveKeyMap(
    const KeyMap& defaultMap,
    const KeyFunctionMap& functions,
    KeyMap* out) noexcept
{
    if (!out)
        return;
    out->fill({});
    for (std::size_t row = 0; row < kRows; ++row)
    {
        for (std::size_t column = 0; column < kColumns; ++column)
        {
            if (defaultMap[row][column] == 0)
                continue;
            const auto function = functions[row][column];
            if (IsPublishableKeyFunction(function))
                (*out)[row][column] = static_cast<std::uint8_t>(function);
        }
    }
}

std::uint16_t NormalizeTravelToMilli(
    std::uint16_t travelUm,
    std::uint16_t maximumTravelUm) noexcept
{
    if (travelUm == 0 || maximumTravelUm == 0)
        return 0;
    if (travelUm >= maximumTravelUm)
        return 1000;
    const std::uint32_t scaled =
        (static_cast<std::uint32_t>(travelUm) * 1000u + maximumTravelUm / 2u) /
        maximumTravelUm;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(scaled, 1000u));
}
}
