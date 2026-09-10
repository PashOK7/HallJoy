#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Pure, allocation-free packet contract for the isolated AULA HERO84 HE
// diagnostic.  A Report includes the HID report ID in byte zero; WebHID calls
// expose only bytes [1..63].  No configuration or calibration builders exist.
namespace aula_hero84he_diagnostic
{
constexpr std::uint16_t kVendorId = 0x372E;
constexpr std::uint16_t kProductId = 0x103E;
constexpr std::uint16_t kUsagePage = 0xFF60;
constexpr std::uint16_t kUsage = 0x0061;
constexpr std::uint8_t kReportId = 0x09;
constexpr std::size_t kReportBytes = 64;
constexpr std::size_t kPayloadBytes = kReportBytes - 1;
// Nine two-byte IDs and nine six-byte response records fit in the 63-byte
// vendor payload. The diagnostic still asks for only four movement positions;
// the shared parser also serves selected-key polling.
constexpr std::size_t kMaxPositions = 9;

using Report = std::array<std::uint8_t, kReportBytes>;

struct Assignment
{
    std::uint16_t position = 0;
    std::uint32_t value = 0;
};

struct DirectSample
{
    std::uint16_t position = 0;
    std::uint16_t current = 0; // scanner lock high bit is removed
    std::uint16_t minimum = 0; // logged only; never an input value
    bool scannerLock = false;
};

// Builders produce exactly one 64-byte kernel HID report.  `positions` must
// contain 1..kMaxPositions unique physical IDs in big-endian wire order.
bool BuildIdentityRead(Report* out);
bool BuildAssignmentRead(std::uint8_t layer, const std::uint16_t* positions,
    std::size_t count, Report* out);
// Firmware-derived read-only candidate; unlike 82/01 and 83 this was not
// emitted by the official web client.  Callers must label it as such.
bool BuildDirectRead(const std::uint16_t* positions, std::size_t count,
    Report* out);

bool HasValidChecksum(const Report& report);
bool IsExpectedResponseHeader(const Report& report, std::uint8_t command,
    std::uint8_t subcommand);
bool ParseIdentityResponse(const Report& report, std::array<std::uint8_t, 6>* uuid);
bool ParseAssignmentResponse(const Report& report, std::uint8_t layer,
    const std::uint16_t* expectedPositions, std::size_t expectedCount,
    std::array<Assignment, kMaxPositions>* assignments);
bool ParseDirectResponse(const Report& report,
    const std::uint16_t* expectedPositions, std::size_t expectedCount,
    std::array<DirectSample, kMaxPositions>* samples);
}
