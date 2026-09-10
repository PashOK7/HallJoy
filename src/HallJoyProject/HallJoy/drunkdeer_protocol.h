#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace drunkdeer
{
constexpr std::size_t kReportBytes = 64;
constexpr std::size_t kReportCount = 3;
constexpr std::size_t kHeaderBytes = 5;
constexpr std::size_t kPayloadBytesPerReport = kReportBytes - kHeaderBytes;
constexpr std::size_t kPayloadBytes =
    kPayloadBytesPerReport * kReportCount;
constexpr std::size_t kRows = 6;
constexpr std::size_t kColumns = 21;
constexpr std::size_t kCellCount = kRows * kColumns;
constexpr std::uint8_t kReportId = 0x04;
constexpr std::uint8_t kTrackingRequestCommand = 0xb6;
constexpr std::uint8_t kTrackingResponseCommand = 0xb7;
constexpr std::uint8_t kNominalTravelMaximum = 40;

using Report = std::array<std::uint8_t, kReportBytes>;
using Reports = std::array<Report, kReportCount>;
using Payload = std::array<std::uint8_t, kPayloadBytes>;
using Matrix = std::array<std::uint8_t, kCellCount>;
using PositionToHid = std::array<std::uint16_t, kCellCount>;

struct TrackingFrame
{
    // Always normalized into firmware chunk order 0, 1, 2.
    Reports reports{};
    // Complete 59 * 3 transport payload. The final 51 bytes are retained for
    // diagnostics even though the official matrix currently consumes only 8
    // bytes from chunk 2.
    Payload payload{};
    Matrix matrix{};
};

enum class TrackingConsumeResult : std::uint8_t
{
    Ignored,
    Accepted,
    Restarted,
    OutOfOrder,
    Complete,
};

class TrackingFrameAssembler
{
public:
    TrackingConsumeResult Consume(const Report& report) noexcept;
    const Reports& OrderedReports() const noexcept { return reports_; }
    void Reset() noexcept;

private:
    Reports reports_{};
    std::size_t expectedChunk_ = 0;
};

Report BuildTrackingRequest(bool enabled) noexcept;
Report BuildMatrixRequest() noexcept;

// A tracking response is report 04/B7 with its chunk number in byte 4. Bytes
// 2 and 3 are retained as firmware metadata and are deliberately not assumed
// to be zero. This accepts only chunk numbers 0..2.
bool TrackingChunkIndex(const Report& report, std::size_t* out) noexcept;

// Decode the current official Antler stream format. Arrival order may vary,
// but every chunk must be present exactly once. The 6x21 matrix consists of
// 59 bytes from chunk 0, 59 from chunk 1, and 8 from chunk 2.
bool DecodeTrackingFrame(const Reports& arrivals, TrackingFrame* out) noexcept;

// Compatibility wrapper used by existing callers/tests.
bool DecodeMatrix(const Reports& reports, Matrix* out) noexcept;

// Existing Soup/UAP 6x21 table. It is intentionally exposed under this name:
// the DrunkDeer diagnostic compares physical raw cells with this unverified
// generic mapping, but never uses digital key events to change it.
PositionToHid GenericUapMap() noexcept;

// Static map for physical PID 352D:2382. Ordinary keys use USB HID keyboard
// usages; Fn/Menu retain Soup/UAP's stable extended codes 0x409/0x403.
PositionToHid G65AnsiMap() noexcept;
PositionToHid MapForProduct(std::uint16_t productId) noexcept;

std::uint16_t ToMilli(std::uint8_t travel) noexcept;
std::size_t MappedKeyCount(const PositionToHid& map) noexcept;
}
