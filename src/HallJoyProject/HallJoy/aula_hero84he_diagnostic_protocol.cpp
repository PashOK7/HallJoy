#include "aula_hero84he_diagnostic_protocol.h"

#include <algorithm>

namespace aula_hero84he_diagnostic
{
namespace
{
constexpr std::size_t kCommand = 1;
constexpr std::size_t kSubcommand = 2;
constexpr std::size_t kFrame = 3;
constexpr std::size_t kRequest = 4;
constexpr std::size_t kReserved = 5;
constexpr std::size_t kLength = 6;
constexpr std::size_t kData = 7;
constexpr std::size_t kChecksum = kReportBytes - 1;

bool UniquePositions(const std::uint16_t* positions, std::size_t count)
{
    if (!positions || !count || count > kMaxPositions) return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (positions[i] == 0 || positions[i] == 0xffff) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (positions[i] == positions[j]) return false;
    }
    return true;
}

void SetChecksum(Report* report)
{
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < kChecksum; ++i) sum += (*report)[i];
    (*report)[kChecksum] = static_cast<std::uint8_t>(0xffu - (sum & 0xffu));
}

bool Build(std::uint8_t command, std::uint8_t subcommand,
    const std::uint16_t* positions, std::size_t count, std::uint8_t length,
    Report* out)
{
    if (!out || (count && !UniquePositions(positions, count))) return false;
    if (kData + length > kChecksum) return false;
    Report report{};
    report[0] = kReportId;
    report[kCommand] = command;
    report[kSubcommand] = subcommand;
    report[kFrame] = 0;
    report[kRequest] = 1;
    report[kReserved] = 0;
    report[kLength] = length;
    for (std::size_t i = 0; i < count; ++i)
    {
        report[kData + i * 2] = static_cast<std::uint8_t>(positions[i] >> 8);
        report[kData + i * 2 + 1] = static_cast<std::uint8_t>(positions[i]);
    }
    SetChecksum(&report);
    *out = report;
    return true;
}

bool ResponseRecords(const Report& report, std::uint8_t command,
    std::uint8_t subcommand, std::size_t expectedCount)
{
    return IsExpectedResponseHeader(report, command, subcommand) &&
        report[kLength] == expectedCount * 6 &&
        kData + report[kLength] <= kChecksum;
}

bool CorrelatePositions(const Report& report, const std::uint16_t* expected,
    std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint16_t actual = static_cast<std::uint16_t>(
            report[kData + i * 6] << 8 | report[kData + i * 6 + 1]);
        if (actual != expected[i]) return false;
    }
    return true;
}
}

bool BuildIdentityRead(Report* out)
{
    // Official Eyt builder: 82 01 00 01 00 06, no variable data bytes.
    return Build(0x82, 0x01, nullptr, 0, 6, out);
}

bool BuildAssignmentRead(std::uint8_t layer, const std::uint16_t* positions,
    std::size_t count, Report* out)
{
    return UniquePositions(positions, count) &&
        Build(0x83, layer, positions, count,
            static_cast<std::uint8_t>(count * 2), out);
}

bool BuildDirectRead(const std::uint16_t* positions, std::size_t count,
    Report* out)
{
    return UniquePositions(positions, count) &&
        Build(0x94, 0x02, positions, count,
            static_cast<std::uint8_t>(count * 2), out);
}

bool HasValidChecksum(const Report& report)
{
    std::uint32_t sum = 0;
    for (const std::uint8_t value : report) sum += value;
    return (sum & 0xffu) == 0xffu;
}

bool IsExpectedResponseHeader(const Report& report, std::uint8_t command,
    std::uint8_t subcommand)
{
    return report[0] == kReportId && HasValidChecksum(report) &&
        report[kCommand] == command && report[kSubcommand] == subcommand &&
        report[kFrame] == 0 && report[kRequest] == 1 &&
        report[kReserved] == 0;
}

bool ParseIdentityResponse(const Report& report, std::array<std::uint8_t, 6>* uuid)
{
    if (!uuid || !ResponseRecords(report, 0x82, 0x01, 1)) return false;
    std::copy_n(report.begin() + kData, uuid->size(), uuid->begin());
    return true;
}

bool ParseAssignmentResponse(const Report& report, std::uint8_t layer,
    const std::uint16_t* expectedPositions, std::size_t expectedCount,
    std::array<Assignment, kMaxPositions>* assignments)
{
    if (!assignments || !UniquePositions(expectedPositions, expectedCount) ||
        !ResponseRecords(report, 0x83, layer, expectedCount) ||
        !CorrelatePositions(report, expectedPositions, expectedCount)) return false;
    assignments->fill({});
    for (std::size_t i = 0; i < expectedCount; ++i)
    {
        const std::size_t offset = kData + i * 6;
        (*assignments)[i].position = expectedPositions[i];
        (*assignments)[i].value = static_cast<std::uint32_t>(report[offset + 2]) << 24 |
            static_cast<std::uint32_t>(report[offset + 3]) << 16 |
            static_cast<std::uint32_t>(report[offset + 4]) << 8 |
            static_cast<std::uint32_t>(report[offset + 5]);
    }
    return true;
}

bool ParseDirectResponse(const Report& report,
    const std::uint16_t* expectedPositions, std::size_t expectedCount,
    std::array<DirectSample, kMaxPositions>* samples)
{
    if (!samples || !UniquePositions(expectedPositions, expectedCount) ||
        !ResponseRecords(report, 0x94, 0x02, expectedCount) ||
        !CorrelatePositions(report, expectedPositions, expectedCount)) return false;
    samples->fill({});
    for (std::size_t i = 0; i < expectedCount; ++i)
    {
        const std::size_t offset = kData + i * 6;
        const std::uint16_t current = static_cast<std::uint16_t>(
            report[offset + 2] << 8 | report[offset + 3]);
        (*samples)[i].position = expectedPositions[i];
        (*samples)[i].scannerLock = (current & 0x8000u) != 0;
        (*samples)[i].current = static_cast<std::uint16_t>(current & 0x7fffu);
        (*samples)[i].minimum = static_cast<std::uint16_t>(
            report[offset + 4] << 8 | report[offset + 5]);
    }
    return true;
}
}
